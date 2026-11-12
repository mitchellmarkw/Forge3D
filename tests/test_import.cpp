// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 5b: import STL / OBJ / 3MF with repair.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Import.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <miniz.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

struct TempDir {
    fs::path path;
    TempDir()
    {
        std::random_device rd;
        path = fs::temp_directory_path() / ("forge3d-import-" + std::to_string(rd()));
        fs::create_directories(path);
    }
    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

double meshVolume(const MeshData& d)
{
    double v = 0;
    for (std::size_t t = 0; t + 2 < d.indices.size(); t += 3) {
        const float* a = &d.positions[d.indices[t] * 3];
        const float* b = &d.positions[d.indices[t + 1] * 3];
        const float* c = &d.positions[d.indices[t + 2] * 3];
        v += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0;
    }
    return v;
}

// A 10 mm cube from 0 to 10, as 12 triangles facing out, every corner shared.
RawMesh cube()
{
    RawMesh m;
    m.name = "cube";
    m.positions = {0, 0, 0, 10, 0, 0, 10, 10, 0, 0, 10, 0, 0, 0, 10, 10, 0, 10, 10, 10, 10, 0, 10, 10};
    m.indices = {0, 2, 1, 0, 3, 2,  4, 5, 6, 4, 6, 7,  0, 1, 5, 0, 5, 4,
                 1, 2, 6, 1, 6, 5,  2, 3, 7, 2, 7, 6,  3, 0, 4, 3, 4, 7};
    return m;
}

ObjectId addBox(CommandStack& stack, Document& doc)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = BoxSpec{20, 20, 20};
    b.name = "Box 1";
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

}  // namespace

TEST_CASE("A clean cube needs no repair", "[import]")
{
    RepairReport r;
    Vec3 offset;
    const auto data = repairMesh(cube(), r, offset);
    CHECK_FALSE(r.changedAnything());
    CHECK(r.summary() == "No repairs needed.");
    CHECK(data->closed);
    CHECK_THAT(meshVolume(*data), WithinRel(1000.0, 1e-9));
    CHECK(data->size == std::array<float, 3>{10, 10, 10});
    CHECK_THAT(offset[0], WithinAbs(5, 1e-9));
    CHECK_THAT(offset[2], WithinAbs(0, 1e-9));
}

TEST_CASE("Repair fixes the usual problems", "[import]")
{
    RepairReport r;
    Vec3 offset;
    SECTION("a missing triangle (a hole)")
    {
        RawMesh m = cube();
        m.indices.resize(m.indices.size() - 3);
        const auto data = repairMesh(m, r, offset);
        CHECK(r.holesFilled == 1);
        CHECK(data->closed);
        CHECK_THAT(meshVolume(*data), WithinRel(1000.0, 1e-6));
    }
    SECTION("a missing face (two triangles: a square hole)")
    {
        RawMesh m = cube();
        m.indices.resize(m.indices.size() - 6);
        const auto data = repairMesh(m, r, offset);
        CHECK(r.holesFilled == 1);
        CHECK(data->closed);
        CHECK_THAT(meshVolume(*data), WithinRel(1000.0, 1e-6));
    }
    SECTION("one triangle facing the wrong way")
    {
        RawMesh m = cube();
        std::swap(m.indices[4], m.indices[5]);
        const auto data = repairMesh(m, r, offset);
        CHECK(r.trianglesFlipped == 1);
        CHECK_THAT(meshVolume(*data), WithinRel(1000.0, 1e-6));
    }
    SECTION("the whole thing inside out")
    {
        RawMesh m = cube();
        for (std::size_t t = 0; t < m.indices.size(); t += 3)
            std::swap(m.indices[t + 1], m.indices[t + 2]);
        const auto data = repairMesh(m, r, offset);
        CHECK(r.partsTurnedOut == 1);
        CHECK_THAT(meshVolume(*data), WithinRel(1000.0, 1e-6));
    }
    SECTION("repeated and empty triangles, duplicate points")
    {
        RawMesh m = cube();
        m.indices.insert(m.indices.end(), {0, 2, 1});  // repeated
        m.indices.insert(m.indices.end(), {0, 1, 1});  // empty
        m.positions.insert(m.positions.end(), {10, 10, 10});  // point 8 = point 6
        REQUIRE(m.indices[29] == 6);
        m.indices[29] = 8;  // the back face uses the copy
        const auto data = repairMesh(m, r, offset);
        CHECK(r.duplicatesRemoved == 1);
        CHECK(r.degenerateRemoved == 1);
        CHECK(r.pointsMerged == 1);
        CHECK(data->closed);
        CHECK(data->indices.size() == 36);
        CHECK(r.holesFilled == 0);
        CHECK(r.summary().find("1 repeated triangle removed") != std::string::npos);
    }
}

TEST_CASE("Text STL, OBJ with quads and groups, 3MF with components and units", "[import]")
{
    const std::string stl = R"(solid t
facet normal 0 0 -1
 outer loop
  vertex 0 0 0
  vertex 0 10 0
  vertex 10 0 0
 endloop
endfacet
facet normal 0 -1 0
 outer loop
  vertex 0 0 0
  vertex 10 0 0
  vertex 0 0 10
 endloop
endfacet
facet normal -1 0 0
 outer loop
  vertex 0 0 0
  vertex 0 0 10
  vertex 0 10 0
 endloop
endfacet
facet normal 1 1 1
 outer loop
  vertex 10 0 0
  vertex 0 10 0
  vertex 0 0 10
 endloop
endfacet
endsolid t
)";
    const auto tet = readStl(stl, "tet");
    REQUIRE(tet.size() == 1);
    RepairReport r;
    Vec3 off;
    const auto data = repairMesh(tet[0], r, off);
    CHECK(data->closed);
    CHECK_THAT(meshVolume(*data), WithinRel(1000.0 / 6.0, 1e-6));

    const std::string obj = R"(# two cubes' worth of points, quads
v 0 0 0
v 10 0 0
v 10 10 0
v 0 10 0
v 0 0 10
v 10 0 10
v 10 10 10
v 0 10 10
o First
f 1 4 3 2
f 5 6 7 8
f 1 2 6 5
f 2/1/1 3/2/2 7 6
f 3 4 8 7
f -4 -8 -5 -1
g Second
f 1 2 3
)";
    const auto parts = readObj(obj, "file");
    REQUIRE(parts.size() == 2);
    CHECK(parts[0].name == "First");
    CHECK(parts[0].indices.size() == 36);
    CHECK(repairMesh(parts[0], r, off)->closed);
    CHECK(parts[1].name == "Second");

    // 3MF: one object made of two components (the cube, and the cube moved 2 units up
    // in X), in centimetres, placed by a build item moved 5 cm along Y.
    std::ostringstream model;
    model << R"(<?xml version="1.0"?><model unit="centimeter" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02"><resources>)"
          << R"(<object id="1" type="model"><mesh><vertices>)";
    const RawMesh c = cube();
    for (std::size_t i = 0; i < c.positions.size(); i += 3)
        model << "<vertex x=\"" << c.positions[i] / 10 << "\" y=\"" << c.positions[i + 1] / 10 << "\" z=\""
              << c.positions[i + 2] / 10 << "\"/>";
    model << "</vertices><triangles>";
    for (std::size_t i = 0; i < c.indices.size(); i += 3)
        model << "<triangle v1=\"" << c.indices[i] << "\" v2=\"" << c.indices[i + 1] << "\" v3=\"" << c.indices[i + 2] << "\"/>";
    model << R"(</triangles></mesh></object><object id="2" name="Pair &amp; more"><components>)"
          << R"(<component objectid="1"/><component objectid="1" transform="1 0 0 0 1 0 0 0 1 2 0 0"/>)"
          << R"(</components></object></resources><build><item objectid="2" transform="1 0 0 0 1 0 0 0 1 0 5 0"/></build></model>)";
    mz_zip_archive zip{};
    REQUIRE(mz_zip_writer_init_heap(&zip, 0, 0));
    const std::string m = model.str();
    REQUIRE(mz_zip_writer_add_mem(&zip, "3D/3dmodel.model", m.data(), m.size(), static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION)));
    void* buf = nullptr;
    size_t size = 0;
    REQUIRE(mz_zip_writer_finalize_heap_archive(&zip, &buf, &size));
    const std::string bytes(static_cast<const char*>(buf), size);
    mz_zip_writer_end(&zip);
    const auto mf = read3mf(bytes, "file");
    REQUIRE(mf.size() == 1);
    CHECK(mf[0].name == "Pair & more");
    const auto pair = repairMesh(mf[0], r, off);
    CHECK(pair->closed);
    CHECK_THAT(pair->size[0], WithinAbs(30, 1e-4));  // two 10 mm cubes, 20 mm apart
    CHECK_THAT(off[1], WithinAbs(55, 1e-4));          // 5 cm along Y, plus half a cube
    CHECK_THAT(meshVolume(*pair), WithinRel(2000.0, 1e-6));

    CHECK_THROWS_AS(readStl("not an stl", "x"), ImportError);
    CHECK_THROWS_AS(readObj("v 1 2 3\n", "x"), ImportError);
    CHECK_THROWS_AS(read3mf("not a zip", "x"), ImportError);
}

TEST_CASE("Export, import again, combine, save and load", "[import]")
{
    TempDir dir;
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc);
    const fs::path stl = dir.path / "box.stl";
    writeMeshes(exportMeshes(doc, {box}), stl, ExportFormat::Stl);
    const fs::path mf = dir.path / "box.3mf";
    writeMeshes(exportMeshes(doc, {box}), mf, ExportFormat::ThreeMf);
    const fs::path obj = dir.path / "box.obj";
    writeMeshes(exportMeshes(doc, {box}), obj, ExportFormat::Obj);

    for (const fs::path& p : {stl, mf, obj}) {
        INFO(p.filename().string());
        const auto parts = readMeshFile(p);
        REQUIRE(parts.size() == 1);
        RepairReport r;
        Vec3 off;
        const auto data = repairMesh(parts[0], r, off);
        CHECK(data->closed);
        CHECK_FALSE(r.changedAnything());
        CHECK_THAT(meshVolume(*data), WithinRel(8000.0, 1e-6));
    }

    // Import into the document: placed to the right of the box, on the plate.
    const ImportResult result = importFile(stack, doc, stl);
    REQUIRE(result.bodies.size() == 1);
    const ObjectId mesh = result.bodies[0];
    CHECK(doc.find(mesh)->name == "box");
    CHECK(stack.undoLabel() == "Import box.stl");
    const Bounds b = doc.worldBounds(mesh);
    CHECK_THAT(b.min[0], WithinAbs(15, 1e-3));
    CHECK_THAT(b.min[2], WithinAbs(0, 1e-3));

    // Resize it, then subtract the original box overlapping half of it.
    Body before = *doc.find(mesh);
    Body after = before;
    std::get<MeshSpec>(after.spec).height = 40;
    stack.execute(std::make_unique<EditBodyCommand>(before, after, "Resize"));
    CHECK_THAT(doc.worldBounds(mesh).max[2], WithinAbs(40, 1e-3));
    Placement p = doc.find(box)->placement;
    p.x = 20;  // box now covers x 10..30: half of the mesh (15..35)... 15..30
    stack.execute(std::make_unique<SetPlacementCommand>(box, p, false));
    Body cut;
    cut.id = doc.allocateId();
    cut.name = "Subtract 1";
    cut.boolean = BooleanSpec{BooleanOp::Subtract, {mesh, box}};
    stack.execute(std::make_unique<CombineCommand>(cut));
    CHECK(BRepCheck_Analyzer(doc.shape(cut.id)).IsValid());
    CHECK_THAT(volumeOf(doc.shape(cut.id)), WithinRel(20.0 * 20 * 40 - 15.0 * 20 * 20, 1e-6));

    // Save and load keep the triangles.
    const fs::path f3d = dir.path / "model.f3d";
    file::save(doc, f3d, "test");
    const auto bodies = file::load(f3d);
    Document loaded;
    loaded.replaceAll(bodies);
    const auto& spec = std::get<MeshSpec>(loaded.find(mesh)->spec);
    const auto& orig = std::get<MeshSpec>(doc.find(mesh)->spec);
    CHECK(spec.height == 40);
    CHECK(spec.data->positions == orig.data->positions);
    CHECK(spec.data->indices == orig.data->indices);
    CHECK_THAT(volumeOf(loaded.shape(cut.id)), WithinRel(volumeOf(doc.shape(cut.id)), 1e-9));
    CHECK_THROWS(file::fromJson(file::toJson(doc, "test")));  // the triangles live outside the JSON
}
