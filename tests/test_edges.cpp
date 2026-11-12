// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 5e: rounding, fillets, chamfers and bevels.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Import.h"
#include "forge/core/Scale.h"

#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <numbers>
#include <random>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

constexpr double pi = std::numbers::pi;

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

double volumeOf(const TopoDS_Shape& s)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

// The edge nearest a point.
int edgeNear(const edges::ShapeEdges& found, Vec3 p)
{
    int best = -1;
    double bestD = 1e300;
    for (std::size_t i = 0; i < found.edges.size(); ++i) {
        const Vec3& m = found.edges[i].mid;
        const double d = std::hypot(m[0] - p[0], m[1] - p[1], m[2] - p[2]);
        if (d < bestD) {
            bestD = d;
            best = int(i);
        }
    }
    return best;
}

// Finish the edges nearest these points of a shape on the plate, as the tool would.
ObjectId finish(CommandStack& stack, Document& doc, ObjectId target, EdgeFinish f, std::vector<Vec3> near,
                std::vector<int> fromFace = {})
{
    const TopoDS_Shape shape = pickableShape(doc, target);
    const auto found = edges::analyse(shape);
    for (std::size_t i = 0; i < near.size(); ++i) {
        const int e = edgeNear(found, near[i]);
        REQUIRE(e >= 0);
        f.edges.push_back(edges::refFor(shape, found, e, i < fromFace.size() ? fromFace[i] : -1));
    }
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Edges");
    r.boolean = edgesSpec(doc, target, f);
    REQUIRE_FALSE(doc.previewMesh(*r.boolean)->empty());
    const ObjectId id = r.id;
    stack.execute(std::make_unique<CombineCommand>(std::move(r)));
    return id;
}

}  // namespace

TEST_CASE("Edges of shapes are found and sorted by how they bend", "[edges]")
{
    SECTION("a box: 12 outside edges, 4 round each face")
    {
        const auto found = edges::analyse(BRepPrimAPI_MakeBox(20, 20, 10).Shape());
        REQUIRE(found.edges.size() == 12);
        for (const auto& e : found.edges)
            CHECK(e.bend == edges::Bend::Convex);
        REQUIRE(found.faceEdges.size() == 6);
        for (const auto& f : found.faceEdges)
            CHECK(f.size() == 4);
    }
    SECTION("a peg on a plate has an inside corner")
    {
        Document doc;
        CommandStack stack(doc);
        const ObjectId plate = add(stack, doc, BoxSpec{40, 40, 5});
        const ObjectId peg = add(stack, doc, CylinderSpec{5, 20}, 0, 0, 5);
        Body merge;
        merge.id = doc.allocateId();
        merge.name = "Merge";
        merge.boolean = BooleanSpec{BooleanOp::Merge, {plate, peg}};
        stack.execute(std::make_unique<CombineCommand>(merge));
        const auto found = edges::analyse(pickableShape(doc, merge.id));
        const int corner = edgeNear(found, {5, 0, 5});
        REQUIRE(corner >= 0);
        CHECK(found.edges[std::size_t(corner)].bend == edges::Bend::Concave);
        CHECK_FALSE(edges::suits(EdgeFinishKind::Rounding, edges::Bend::Concave));
        CHECK(edges::suits(EdgeFinishKind::Fillet, edges::Bend::Concave));
        CHECK(edges::suits(EdgeFinishKind::Chamfer, edges::Bend::Concave));
    }
    SECTION("a cylinder: two rims, no seam")
    {
        Document doc;
        CommandStack stack(doc);
        const ObjectId cyl = add(stack, doc, CylinderSpec{10, 20});
        const auto found = edges::analyse(pickableShape(doc, cyl));
        CHECK(found.edges.size() == 2);
    }
    SECTION("clicking finds the face and its rim")
    {
        Document doc;
        CommandStack stack(doc);
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10});
        const TopoDS_Shape shape = pickableShape(doc, box);
        const auto hit = edges::faceUnder(shape, {3, 2, 50}, {0, 0, -1});
        REQUIRE(hit);
        CHECK_THAT(hit->point[2], WithinAbs(10.0, 1e-6));
        const auto found = edges::analyse(shape);
        for (int e : found.faceEdges[std::size_t(hit->face)])
            CHECK_THAT(found.edges[std::size_t(e)].mid[2], WithinAbs(10.0, 1e-6));
        CHECK_FALSE(edges::faceUnder(shape, {100, 0, 50}, {0, 0, -1}));
    }
}

TEST_CASE("Rounding, fillets, chamfers and bevels", "[edges]")
{
    Document doc;
    CommandStack stack(doc);

    SECTION("rounding the top rim of a box")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10});
        EdgeFinish f;
        f.kind = EdgeFinishKind::Rounding;
        f.size = 2.0;
        const ObjectId r = finish(stack, doc, box, f, {{0, -10, 10}, {10, 0, 10}, {0, 10, 10}, {-10, 0, 10}});
        const double removed = 4000.0 - volumeOf(doc.shape(r));
        // Four edges lose (r² - πr²/4) per mm, less at the corners where they meet.
        const double perMm = 4.0 - pi;
        CHECK(removed > perMm * 4 * 16);
        CHECK(removed < perMm * 4 * 20);
        const Bounds b = doc.worldBounds(r);
        CHECK_THAT(b.max[2], WithinAbs(10.0, 1e-4));
        CHECK_THAT(b.size()[0], WithinAbs(20.0, 1e-4));
        CHECK(exportMeshes(doc, {r}).front().openEdges == 0);
    }
    SECTION("a fillet in the corner round a peg")
    {
        const ObjectId plate = add(stack, doc, BoxSpec{40, 40, 5});
        const ObjectId peg = add(stack, doc, CylinderSpec{5, 20}, 0, 0, 5);
        Body merge;
        merge.id = doc.allocateId();
        merge.name = "Merge";
        merge.boolean = BooleanSpec{BooleanOp::Merge, {plate, peg}};
        stack.execute(std::make_unique<CombineCommand>(merge));
        const double before = volumeOf(doc.shape(merge.id));
        EdgeFinish f;
        f.kind = EdgeFinishKind::Fillet;
        f.size = 2.0;
        const ObjectId r = finish(stack, doc, merge.id, f, {{5, 0, 5}});
        const double added = volumeOf(doc.shape(r)) - before;
        // A ring of (r² - πr²/4) cross-section round a 5 mm peg: about 2π(5 + 0.45r) long.
        const double expected = (4.0 - pi) * 2 * pi * (5 + 0.45 * 2);
        CHECK_THAT(added, WithinRel(expected, 0.1));
    }
    SECTION("a chamfer on one edge")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10});
        EdgeFinish f;
        f.kind = EdgeFinishKind::Chamfer;
        f.size = 2.0;
        const ObjectId r = finish(stack, doc, box, f, {{0, -10, 10}});
        CHECK_THAT(4000.0 - volumeOf(doc.shape(r)), WithinRel(20 * 2 * 2 / 2.0, 1e-6));
    }
    SECTION("a bevel measured from the top face")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10});
        const TopoDS_Shape shape = pickableShape(doc, box);
        const auto top = edges::faceUnder(shape, {0, 0, 50}, {0, 0, -1});
        REQUIRE(top);
        EdgeFinish f;
        f.kind = EdgeFinishKind::Bevel;
        f.size = 6.0;    // 6 mm back across the top
        f.angle = 30.0;  // sloping 30° down from it: 6 tan 30 = 3.46 mm down the front
        const ObjectId r = finish(stack, doc, box, f, {{0, -10, 10}}, {top->face});
        const double removed = 4000.0 - volumeOf(doc.shape(r));
        CHECK_THAT(removed, WithinRel(20 * 6 * 6 * std::tan(pi / 6) / 2.0, 1e-3));
        // From the front face instead: 6 down the front, 6 tan 30 back across the top.
        const auto front = edges::faceUnder(shape, {0, -50, 5}, {0, 1, 0});
        REQUIRE(front);
        stack.undo();
        const ObjectId r2 = finish(stack, doc, box, f, {{0, -10, 10}}, {front->face});
        CHECK_THAT(4000.0 - volumeOf(doc.shape(r2)), WithinRel(removed, 1e-3));
        const Bounds b = doc.worldBounds(r2);
        CHECK_THAT(b.max[2], WithinAbs(10.0, 1e-4));
    }
    SECTION("too big a radius is refused with a message")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10});
        const TopoDS_Shape shape = pickableShape(doc, box);
        const auto found = edges::analyse(shape);
        EdgeFinish f;
        f.size = 30.0;
        f.edges.push_back(edges::refFor(shape, found, edgeNear(found, {0, -10, 10}), -1));
        CHECK_THROWS_AS(doc.previewMesh(edgesSpec(doc, box, f)), GeometryError);
    }
    SECTION("imported meshes are refused")
    {
        auto data = std::make_shared<MeshData>();
        data->positions = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
        data->indices = {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
        data->size = {1, 1, 1};
        data->closed = true;
        MeshSpec m;
        m.data = data;
        m.width = m.depth = m.height = 10;
        const ObjectId id = add(stack, doc, m);
        CHECK_THROWS_AS(pickableShape(doc, id), std::invalid_argument);
        CHECK_THROWS_AS(edgesSpec(doc, id, EdgeFinish{}), std::invalid_argument);
    }
}

TEST_CASE("Edge finishes stay put when things change", "[edges]")
{
    std::random_device rd;
    const fs::path dir = fs::temp_directory_path() / ("forge3d-edges-" + std::to_string(rd()));
    fs::create_directories(dir);
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 10}, 30, 0, 0);
    EdgeFinish f;
    f.kind = EdgeFinishKind::Rounding;
    f.size = 2.0;
    const ObjectId r = finish(stack, doc, box, f, {{30, -10, 10}, {40, 0, 10}});
    const double v = volumeOf(doc.shape(r));

    SECTION("moving the result")
    {
        Placement p = doc.find(r)->placement;
        p.x += 25;
        stack.execute(std::make_unique<SetPlacementCommand>(r, p, false));
        CHECK_THAT(volumeOf(doc.shape(r)), WithinRel(v, 1e-9));
        CHECK_THAT(doc.worldBounds(r).min[0], WithinAbs(45.0, 1e-4));
        // Picks read back in plate coordinates follow it.
        CHECK_THAT(edgesInPlate(doc, r).edges.front().point[0], WithinAbs(55.0, 1e-6));
    }
    SECTION("the box under it grows taller")
    {
        BoxSpec taller{20, 20, 14};
        stack.execute(std::make_unique<SetSpecCommand>(box, PrimitiveSpec{taller}));
        const double now = volumeOf(doc.shape(r));
        CHECK_THAT(now, WithinRel(v + 20 * 20 * 4, 1e-6));  // the same two edges, still rounded
    }
    SECTION("resizing evenly")
    {
        const auto after = scaledTree(doc, r, {2, 2, 2}, {30, 0, 0});
        std::vector<Body> before;
        for (const Body& x : after)
            before.push_back(*doc.find(x.id));
        stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Resize"));
        CHECK_THAT(doc.find(r)->boolean->edges->size, WithinRel(4.0, 1e-9));
        CHECK_THAT(volumeOf(doc.shape(r)), WithinRel(v * 8, 1e-6));
    }
    SECTION("changing the finish")
    {
        EdgeFinish g = edgesInPlate(doc, r);
        g.kind = EdgeFinishKind::Chamfer;
        g.size = 1.0;
        const Body before = *doc.find(r);
        stack.execute(std::make_unique<EditBodyCommand>(before, withEdges(doc, r, g), "Change edges"));
        CHECK_THAT(4000.0 - volumeOf(doc.shape(r)), WithinRel(2 * 20 * 0.5, 0.05));
    }
    SECTION("save and load")
    {
        file::save(doc, dir / "e.f3d", "test");
        Document loaded;
        loaded.replaceAll(file::load(dir / "e.f3d"));
        CHECK(*loaded.find(r) == *doc.find(r));
        CHECK_THAT(volumeOf(loaded.shape(r)), WithinRel(v, 1e-9));
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
}
