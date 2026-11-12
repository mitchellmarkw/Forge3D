// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

using namespace forge;
namespace fs = std::filesystem;
using Catch::Matchers::ContainsSubstring;

namespace {

// A temporary folder removed at the end of each test.
struct TempDir {
    fs::path path;
    TempDir()
    {
        std::random_device rd;
        path = fs::temp_directory_path() / ("forge3d-test-" + std::to_string(rd()));
        fs::create_directories(path);
    }
    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void fill(Document& doc)
{
    Body box;
    box.id = doc.allocateId();
    box.name = "Lid";
    box.spec = BoxSpec{40.5, 30.25, 2.0};
    box.placement = Placement{-12.5, 7.0, 0.0, 45.0};
    box.placement.rotationX = 90.0;
    box.placement.rotationY = -30.0;
    doc.insertBody(box);

    Body cyl;
    cyl.id = doc.allocateId();
    cyl.name = "Peg";
    cyl.spec = CylinderSpec{3.0, 5.5, 12.0};  // elliptical
    cyl.placement = Placement{30.0, 0.0, 2.0, 0.0};
    doc.insertBody(cyl);
}

void writeText(const fs::path& p, const std::string& text)
{
    std::ofstream(p, std::ios::binary) << text;
}

}  // namespace

TEST_CASE("save then load gives back the same bodies", "[file]")
{
    TempDir dir;
    Document doc;
    fill(doc);
    const fs::path path = dir.path / "model.f3d";

    file::save(doc, path, "Forge3D test");
    REQUIRE(fs::exists(path));
    CHECK_FALSE(fs::exists(dir.path / "model.f3d.saving"));  // temp file cleaned up

    const auto bodies = file::load(path);
    REQUIRE(bodies.size() == 2);
    CHECK(bodies[0] == doc.bodies()[0]);
    CHECK(bodies[1] == doc.bodies()[1]);
}

TEST_CASE("saving over an existing file replaces it", "[file]")
{
    TempDir dir;
    const fs::path path = dir.path / "model.f3d";
    Document first;
    fill(first);
    file::save(first, path, "test");

    Document second;
    file::save(second, path, "test");
    CHECK(file::load(path).empty());
}

TEST_CASE("paths with spaces and non-ASCII characters work", "[file]")
{
    TempDir dir;
    Document doc;
    fill(doc);
    const fs::path path = dir.path / fs::path(u8"my parts – größe.f3d");
    file::save(doc, path, "test");
    CHECK(file::load(path).size() == 2);
}

TEST_CASE("a loaded document restores ids so new ids do not clash", "[file]")
{
    TempDir dir;
    Document doc;
    fill(doc);
    const fs::path path = dir.path / "m.f3d";
    file::save(doc, path, "test");

    Document reopened;
    reopened.replaceAll(file::load(path));
    CHECK(reopened.bodyCount() == 2);
    CHECK(reopened.allocateId() == 3);
}

TEST_CASE("the JSON names its format and version", "[file]")
{
    Document doc;
    fill(doc);
    const std::string json = file::toJson(doc, "Forge3D 9.9");
    CHECK_THAT(json, ContainsSubstring("\"format\": \"forge3d-document\""));
    CHECK_THAT(json, ContainsSubstring("\"formatVersion\": 8"));
    CHECK_THAT(json, ContainsSubstring("\"radiusY\""));
    CHECK_THAT(json, ContainsSubstring("\"rotationX\""));
    CHECK_THAT(json, ContainsSubstring("\"createdBy\": \"Forge3D 9.9\""));
    CHECK_THAT(json, ContainsSubstring("\"type\": \"cylinder\""));
}

TEST_CASE("bad files give clear errors", "[file]")
{
    TempDir dir;

    SECTION("missing file")
    {
        CHECK_THROWS_WITH(file::load(dir.path / "nope.f3d"), ContainsSubstring("Could not open"));
    }
    SECTION("not a zip")
    {
        writeText(dir.path / "text.f3d", "hello");
        CHECK_THROWS_WITH(file::load(dir.path / "text.f3d"), ContainsSubstring("not a Forge3D file"));
    }
    SECTION("newer format version")
    {
        CHECK_THROWS_WITH(file::fromJson(R"({"format":"forge3d-document","formatVersion":99,"bodies":[]})"),
                          ContainsSubstring("newer version"));
    }
    SECTION("unknown shape type")
    {
        CHECK_THROWS_WITH(
            file::fromJson(R"({"format":"forge3d-document","formatVersion":1,"bodies":[
                {"id":1,"name":"x","type":"teapot","parameters":{},
                 "placement":{"x":0,"y":0,"z":0,"rotationZ":0}}]})"),
            ContainsSubstring("teapot"));
    }
    SECTION("version 3 cylinder without radiusY")
    {
        CHECK_THROWS_WITH(
            file::fromJson(R"({"format":"forge3d-document","formatVersion":3,"bodies":[
                {"id":1,"name":"x","type":"cylinder","parameters":{"radius":5,"height":1},
                 "placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}}]})"),
            ContainsSubstring("radiusX"));
    }
    SECTION("missing parameter")
    {
        CHECK_THROWS_WITH(
            file::fromJson(R"({"format":"forge3d-document","formatVersion":1,"bodies":[
                {"id":1,"name":"x","type":"box","parameters":{"width":1,"depth":1},
                 "placement":{"x":0,"y":0,"z":0,"rotationZ":0}}]})"),
            ContainsSubstring("height"));
    }
    SECTION("broken JSON")
    {
        CHECK_THROWS_AS(file::fromJson("{not json"), file::FileError);
    }
    SECTION("wrong kind of JSON")
    {
        CHECK_THROWS_AS(file::fromJson(R"({"hello":"world"})"), file::FileError);
    }
}

TEST_CASE("replaceAll rejects bad bodies and leaves the document alone", "[file][document]")
{
    Document doc;
    fill(doc);

    std::vector<Body> bad(2);
    bad[0].id = 7;
    bad[0].spec = BoxSpec{1, 1, 1};
    bad[1].id = 7;  // duplicate
    bad[1].spec = BoxSpec{1, 1, 1};
    CHECK_THROWS(doc.replaceAll(bad));
    CHECK(doc.bodyCount() == 2);

    bad[1].id = 8;
    bad[1].spec = BoxSpec{-1, 1, 1};  // invalid size
    CHECK_THROWS(doc.replaceAll(bad));
    CHECK(doc.bodyCount() == 2);
    CHECK(doc.bodies()[0].name == "Lid");
}

TEST_CASE("version 1 files still open, with no tilt", "[file]")
{
    const auto bodies = file::fromJson(R"({"format":"forge3d-document","formatVersion":1,"units":"mm","bodies":[
        {"id":3,"name":"Old box","type":"box","parameters":{"width":10,"depth":20,"height":30},
         "placement":{"x":1,"y":2,"z":3,"rotationZ":45}}]})");
    REQUIRE(bodies.size() == 1);
    CHECK(bodies[0].placement.rotationZ == 45.0);
    CHECK(bodies[0].placement.rotationX == 0.0);
    CHECK(bodies[0].placement.rotationY == 0.0);
}

TEST_CASE("version 2 files must have all three angles", "[file]")
{
    CHECK_THROWS_WITH(file::fromJson(R"({"format":"forge3d-document","formatVersion":2,"bodies":[
        {"id":1,"name":"x","type":"box","parameters":{"width":1,"depth":1,"height":1},
         "placement":{"x":0,"y":0,"z":0,"rotationZ":0}}]})"),
                      ContainsSubstring("rotationX"));
}

TEST_CASE("version 2 cylinders open as round cylinders", "[file]")
{
    const auto bodies = file::fromJson(R"({"format":"forge3d-document","formatVersion":2,"units":"mm","bodies":[
        {"id":1,"name":"Peg","type":"cylinder","parameters":{"radius":4,"height":9},
         "placement":{"x":0,"y":0,"z":0,"rotationX":0,"rotationY":0,"rotationZ":0}}]})");
    REQUIRE(bodies.size() == 1);
    const auto& cyl = std::get<CylinderSpec>(bodies[0].spec);
    CHECK(cyl == CylinderSpec{4, 9});
    CHECK(cyl.isRound());
}
