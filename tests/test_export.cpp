// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 5a: export to STL / 3MF / OBJ.
#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Export.h"
#include "forge/core/Split.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <miniz.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

double volumeOf(const TopoDS_Shape& s)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName(typeName(spec));
    b.placement.x = x;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

std::string readAll(const fs::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

struct TempDir {
    fs::path path;
    TempDir()
    {
        std::random_device rd;
        path = fs::temp_directory_path() / ("forge3d-export-" + std::to_string(rd()));
        fs::create_directories(path);
    }
    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::size_t countOf(const std::string& text, const std::string& what)
{
    std::size_t n = 0;
    for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1))
        ++n;
    return n;
}

}  // namespace

TEST_CASE("Every primitive exports watertight, the right way out", "[export]")
{
    Document doc;
    CommandStack stack(doc);
    std::vector<ObjectId> ids;
    for (std::size_t kind = 0; kind < kPrimitiveKindCount; ++kind)
        ids.push_back(add(stack, doc, defaultSpec(kind), 40.0 * double(kind)));
    ids.push_back(add(stack, doc, SphereSpec{10, 6, 4}, 400));  // ellipsoid: B-spline surface
    const auto meshes = exportMeshes(doc, ids);
    REQUIRE(meshes.size() == ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        INFO(meshes[i].name);
        CHECK(meshes[i].openEdges == 0);
        CHECK_THAT(meshes[i].volume(), WithinRel(volumeOf(doc.shape(ids[i])), 5e-3));
    }
}

TEST_CASE("Combined, split, mirrored and grouped shapes export watertight", "[export]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, CylinderSpec{6, 30});
    Body cut;
    cut.id = doc.allocateId();
    cut.name = "Subtract 1";
    cut.boolean = BooleanSpec{BooleanOp::Subtract, {a, b}};
    stack.execute(std::make_unique<CombineCommand>(cut));
    const ObjectId half = splitBody(stack, doc, cut.id, SplitPlane{{0, 0, 8}, {0.3, 0, 1}}, SplitKeep::Front).front();
    const ObjectId mirrored = mirrorBodies(stack, doc, {half}, 0).front();
    const ObjectId w = add(stack, doc, WedgeSpec{}, 40);
    const ObjectId w2 = add(stack, doc, BoxSpec{}, 80);
    const ObjectId group = groupBodies(stack, doc, {w, w2});

    const auto meshes = exportMeshes(doc, {mirrored, group});
    for (const ExportMesh& m : meshes) {
        INFO(m.name);
        CHECK(m.openEdges == 0);
        CHECK(m.volume() > 0.0);
    }
    CHECK_THAT(meshes[0].volume(), WithinRel(volumeOf(doc.shape(mirrored)), 5e-3));
    CHECK_THAT(meshes[1].volume(), WithinRel(20.0 * 20 * 20 * 1.5, 1e-6));  // wedge + box, flat faces: exact
}

TEST_CASE("Export positions are on the plate", "[export]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{10, 10, 10}, 100);
    const auto m = exportMeshes(doc, {box}).front();
    float minX = 1e9f, maxZ = -1e9f;
    for (std::size_t i = 0; i < m.positions.size(); i += 3) {
        minX = std::min(minX, m.positions[i]);
        maxZ = std::max(maxZ, m.positions[i + 2]);
    }
    CHECK_THAT(minX, WithinAbs(95, 1e-4));
    CHECK_THAT(maxZ, WithinAbs(10, 1e-4));
    CHECK(m.positions.size() / 3 == 8);  // a box has 8 corners once welded
    CHECK(m.triangleCount() == 12);
}

TEST_CASE("Files in all three formats", "[export]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{10, 10, 10});
    const ObjectId b = add(stack, doc, BoxSpec{10, 10, 10}, 30);
    doc.rename(b, "Lid & <top>");
    const auto meshes = exportMeshes(doc, {a, b});
    TempDir dir;

    const fs::path stl = dir.path / "parts.stl";
    writeMeshes(meshes, stl, formatForPath(stl));
    const std::string s = readAll(stl);
    REQUIRE(s.size() == 84 + 50 * 24);
    std::uint32_t count = 0;
    std::memcpy(&count, s.data() + 80, 4);
    CHECK(count == 24);
    CHECK_FALSE(fs::exists(dir.path / "parts.stl.saving"));

    const fs::path obj = dir.path / "PARTS.OBJ";
    writeMeshes(meshes, obj, formatForPath(obj));
    const std::string o = readAll(obj);
    CHECK(countOf(o, "\nv ") == 16);
    CHECK(countOf(o, "\nf ") == 24);
    CHECK(countOf(o, "\no ") == 2);
    CHECK(o.find("f 9 ") != std::string::npos);  // the second object's points are numbered after the first's

    const fs::path mf = dir.path / "parts.3mf";
    writeMeshes(meshes, mf, formatForPath(mf));
    const std::string zipBytes = readAll(mf);
    mz_zip_archive zip{};
    REQUIRE(mz_zip_reader_init_mem(&zip, zipBytes.data(), zipBytes.size(), 0));
    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, "3D/3dmodel.model", &size, 0);
    REQUIRE(data);
    const std::string model(static_cast<const char*>(data), size);
    mz_free(data);
    CHECK(mz_zip_reader_locate_file(&zip, "[Content_Types].xml", nullptr, 0) >= 0);
    CHECK(mz_zip_reader_locate_file(&zip, "_rels/.rels", nullptr, 0) >= 0);
    mz_zip_reader_end(&zip);
    CHECK(countOf(model, "<object ") == 2);
    CHECK(countOf(model, "<vertex ") == 16);
    CHECK(countOf(model, "<triangle ") == 24);
    CHECK(model.find("unit=\"millimeter\"") != std::string::npos);
    CHECK(model.find("Lid &amp; &lt;top&gt;") != std::string::npos);

    CHECK_THROWS_AS(formatForPath(dir.path / "parts.step"), ExportError);
    CHECK_THROWS_AS(writeMeshes({}, stl, ExportFormat::Stl), ExportError);
}
