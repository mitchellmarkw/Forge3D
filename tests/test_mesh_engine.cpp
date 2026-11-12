// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 5c: imported meshes and everything made with them use the mesh engine.
#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Import.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"
#include "forge/core/Split.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <memory>
#include <numbers>
#include <random>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0, double y = 0, double z = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName(typeName(spec));
    b.placement.x = x;
    b.placement.y = y;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

// A finely triangulated ball of radius 20 (like an imported scan), added as a mesh body.
ObjectId addMeshBall(CommandStack& stack, Document& doc, double x = 0)
{
    Document scratch;
    CommandStack s2(scratch);
    const ObjectId ball = add(s2, scratch, SphereSpec{20});
    const ExportMesh m = exportMeshes(scratch, {ball}, exportQuality(true)).front();
    RawMesh raw;
    raw.name = "Scan";
    raw.positions = m.positions;
    raw.indices = m.indices;
    RepairReport report;
    Vec3 offset;
    const auto data = repairMesh(raw, report, offset);
    REQUIRE(data->closed);
    MeshSpec spec;
    spec.data = data;
    spec.width = data->size[0];
    spec.depth = data->size[1];
    spec.height = data->size[2];
    return add(stack, doc, spec, x);
}

Body combineBody(Document& doc, BooleanOp op, std::vector<ObjectId> parts)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName(toString(op));
    r.boolean = BooleanSpec{op, std::move(parts)};
    return r;
}

double volume(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

constexpr double pi = std::numbers::pi;

}  // namespace

TEST_CASE("Subtracting a box from an imported mesh uses the mesh engine", "[mesh-engine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = addMeshBall(stack, doc);
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 20}, 0, 0, 20);  // covers the top half
    CHECK(doc.usesMeshEngine(ball));
    CHECK_FALSE(doc.usesMeshEngine(box));
    const double ballVolume = volume(doc, ball);
    CHECK_THAT(ballVolume, WithinRel(4.0 / 3.0 * pi * 8000.0, 2e-3));

    Body cut = combineBody(doc, BooleanOp::Subtract, {ball, box});
    const Document::Preview preview = doc.previewResult(*cut.boolean);
    CHECK_FALSE(preview.empty);
    CHECK_THAT(preview.bounds.max[2], WithinAbs(20, 1e-3));
    stack.execute(std::make_unique<CombineCommand>(cut));
    CHECK(doc.usesMeshEngine(cut.id));
    CHECK_THAT(volume(doc, cut.id), WithinRel(ballVolume / 2.0, 2e-3));  // the bottom half is left
    const Bounds b = doc.worldBounds(cut.id);
    CHECK_THAT(b.max[2], WithinAbs(20, 1e-3));
    CHECK_THAT(b.min[2], WithinAbs(0, 1e-3));
    CHECK_FALSE(doc.mesh(cut.id)->empty());
    CHECK_THAT(doc.visibleCentre(cut.id)[2], WithinAbs(10, 1e-3));

    // Moving the box re-cuts.
    Placement p = doc.find(box)->placement;
    p.z = 30;
    stack.execute(std::make_unique<SetPlacementCommand>(box, p, false));
    CHECK_THAT(doc.worldBounds(cut.id).max[2], WithinAbs(30, 1e-3));

    // Export: watertight, same volume.
    const ExportMesh out = exportMeshes(doc, {cut.id}).front();
    CHECK(out.openEdges == 0);
    CHECK_THAT(out.volume(), WithinRel(volume(doc, cut.id), 1e-6));
}

TEST_CASE("Split, Mirror, Group, Merge and Intersect on meshes", "[mesh-engine]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = addMeshBall(stack, doc);
    const double whole = volume(doc, ball);

    SECTION("split")
    {
        const auto halves = splitBody(stack, doc, ball, SplitPlane{{0, 0, 20}, {0, 0, 1}}, SplitKeep::Both);
        REQUIRE(halves.size() == 2);
        CHECK_THAT(volume(doc, halves[0]) + volume(doc, halves[1]), WithinRel(whole, 1e-6));
        CHECK_THAT(doc.worldBounds(halves[0]).min[2], WithinAbs(20, 1e-3));
        CHECK_THROWS_AS(splitBody(stack, doc, halves[0], SplitPlane{{0, 0, 100}, {0, 0, 1}}, SplitKeep::Both),
                        std::invalid_argument);
    }
    SECTION("mirror and group")
    {
        const ObjectId m = mirrorBodies(stack, doc, {ball}, 2).front();
        CHECK_THAT(volume(doc, m), WithinRel(whole, 1e-9));
        CHECK_THAT(doc.worldBounds(m).min[2], WithinAbs(0, 1e-3));
        const ObjectId cyl = add(stack, doc, CylinderSpec{5, 10}, 60);
        const ObjectId g = groupBodies(stack, doc, {m, cyl});
        CHECK(doc.usesMeshEngine(g));
        CHECK_THAT(volume(doc, g), WithinRel(whole + pi * 25 * 10, 2e-3));
    }
    SECTION("merge and intersect with a cylinder through it")
    {
        const ObjectId cyl = add(stack, doc, CylinderSpec{5, 60}, 0, 0, -10);
        Body merge = combineBody(doc, BooleanOp::Merge, {ball, cyl});
        stack.execute(std::make_unique<CombineCommand>(merge));
        CHECK_THAT(doc.worldBounds(merge.id).max[2], WithinAbs(50, 1e-3));
        stack.undo();
        Body both = combineBody(doc, BooleanOp::Intersect, {ball, cyl});
        stack.execute(std::make_unique<CombineCommand>(both));
        CHECK(volume(doc, both.id) < pi * 25 * 40.5);
        CHECK(volume(doc, both.id) > pi * 25 * 38);
    }
    SECTION("resizing a mesh result")
    {
        const ObjectId box = add(stack, doc, BoxSpec{40, 40, 20}, 0, 0, 20);
        Body cut = combineBody(doc, BooleanOp::Subtract, {ball, box});
        stack.execute(std::make_unique<CombineCommand>(cut));
        const auto after = scaledTree(doc, cut.id, {2, 2, 2}, {0, 0, 0});
        std::vector<Body> before;
        for (const Body& x : after)
            before.push_back(*doc.find(x.id));
        stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Resize"));
        CHECK_THAT(doc.worldBounds(cut.id).max[2], WithinAbs(40, 1e-2));
    }
}

TEST_CASE("Mesh results save and load", "[mesh-engine]")
{
    std::random_device rd;
    const fs::path dir = fs::temp_directory_path() / ("forge3d-meshengine-" + std::to_string(rd()));
    fs::create_directories(dir);
    Document doc;
    CommandStack stack(doc);
    const ObjectId ball = addMeshBall(stack, doc);
    const ObjectId box = add(stack, doc, BoxSpec{40, 40, 20}, 0, 0, 20);
    Body cut = combineBody(doc, BooleanOp::Subtract, {ball, box});
    stack.execute(std::make_unique<CombineCommand>(cut));
    file::save(doc, dir / "m.f3d", "test");
    Document loaded;
    loaded.replaceAll(file::load(dir / "m.f3d"));
    CHECK_THAT(meshkernel::volume(loaded.meshSolid(cut.id)), WithinRel(volume(doc, cut.id), 1e-9));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("Open meshes are refused by the mesh engine", "[mesh-engine]")
{
    auto data = std::make_shared<MeshData>();
    data->positions = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    data->indices = {0, 2, 1, 0, 1, 3, 0, 3, 2};  // a tetrahedron missing one side
    data->size = {1, 1, 1};
    data->closed = false;
    CHECK_THROWS_AS(meshkernel::fromMeshData(*data, {1, 1, 1}), MeshEngineError);
}
