// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 4a: Split by plane.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Split.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

double volumeOf(const TopoDS_Shape& s)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

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

}  // namespace

TEST_CASE("Split a box flat, keeping both halves", "[split]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    const auto made = splitBody(stack, doc, box, SplitPlane{{0, 0, 5}, {0, 0, 1}}, SplitKeep::Both);
    REQUIRE(made.size() == 2);
    CHECK(doc.topLevelIds() == made);
    CHECK(doc.find(made[0])->name == "Box 1 (top)");
    CHECK(doc.find(made[1])->name == "Box 1 (bottom)");
    CHECK_THAT(volumeOf(doc.shape(made[0])), WithinRel(20.0 * 20 * 15, 1e-9));
    CHECK_THAT(volumeOf(doc.shape(made[1])), WithinRel(20.0 * 20 * 5, 1e-9));
    CHECK_THAT(doc.worldBounds(made[0]).min[2], WithinAbs(5, 1e-4));
    CHECK_THAT(doc.worldBounds(made[1]).max[2], WithinAbs(5, 1e-4));
    CHECK(BRepCheck_Analyzer(doc.shape(made[0])).IsValid());
    CHECK(stack.undoLabel() == "Split Box 1");

    // Each half turns about its own middle.
    CHECK_THAT(doc.visibleCentre(made[0])[2], WithinAbs(12.5, 1e-4));

    stack.undo();  // one step brings the whole box back
    CHECK(doc.bodyCount() == 1);
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{box});
    stack.redo();
    CHECK(doc.topLevelIds().size() == 2);
}

TEST_CASE("Keep one side only", "[split]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId cyl = add(stack, doc, CylinderSpec{10, 20});
    const auto front = splitBody(stack, doc, cyl, SplitPlane{{3, 0, 0}, {1, 0, 0}}, SplitKeep::Front);
    REQUIRE(front.size() == 1);
    CHECK(doc.bodyCount() == 2);  // the cylinder is inside the result
    CHECK(doc.find(front[0])->name == "Cylinder 1 (right)");
    CHECK_THAT(doc.worldBounds(front[0]).min[0], WithinAbs(3, 1e-4));
    stack.undo();

    const auto back = splitBody(stack, doc, cyl, SplitPlane{{3, 0, 0}, {1, 0, 0}}, SplitKeep::Back);
    CHECK(doc.find(back[0])->name == "Cylinder 1 (left)");
    CHECK_THAT(doc.worldBounds(back[0]).max[0], WithinAbs(3, 1e-4));
}

TEST_CASE("A plane that misses the shape is refused", "[split]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    CHECK_THROWS_AS(splitBody(stack, doc, box, SplitPlane{{0, 0, 30}, {0, 0, 1}}, SplitKeep::Both),
                    std::invalid_argument);
    CHECK(doc.bodyCount() == 1);
    CHECK(stack.undoLabel() == "Add Box");  // nothing was recorded
}

TEST_CASE("Separate gives back the whole shape", "[split]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    const auto made = splitBody(stack, doc, box, SplitPlane{{0, 0, 10}, {0, 0, 1}}, SplitKeep::Front);
    stack.execute(std::make_unique<SeparateCommand>(made[0]));
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{box});
    CHECK_THAT(doc.worldBounds(box).max[2], WithinAbs(20, 1e-4));
}

TEST_CASE("Splitting a combined shape copies all its parts", "[split]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, CylinderSpec{5, 30});
    Body merge;
    merge.id = doc.allocateId();
    merge.name = "Merge 1";
    merge.boolean = BooleanSpec{BooleanOp::Merge, {a, b}};
    stack.execute(std::make_unique<CombineCommand>(merge));

    const auto made = splitBody(stack, doc, merge.id, SplitPlane{{0, 0, 10}, {0, 0, 1}}, SplitKeep::Both);
    CHECK(doc.bodyCount() == 3 + 3 + 2);  // original tree, its copy, two halves
    const Body* bottom = doc.find(made[1]);
    const Body* copy = doc.find(bottom->boolean->operands.front());
    CHECK(copy->name == "Merge 1 copy");
    REQUIRE(copy->boolean);
    CHECK(doc.find(copy->boolean->operands[0])->name == "Box 1 copy");
    CHECK(doc.find(copy->boolean->operands[1])->name == "Cylinder 1 copy");
    // Top: box above 10 plus the peg above it; bottom: box below 10 plus the peg below it.
    CHECK_THAT(doc.worldBounds(made[0]).max[2], WithinAbs(30, 1e-4));
    CHECK_THAT(doc.worldBounds(made[1]).max[2], WithinAbs(10, 1e-4));
}

TEST_CASE("Tilts and offsets", "[split]")
{
    const Vec3 c{1, 2, 3};
    const SplitPlane x = planeFromTilts(c, 0, 90, 4);
    CHECK_THAT(x.normal[0], WithinAbs(1, 1e-12));
    CHECK_THAT(x.point[0], WithinAbs(5, 1e-12));
    const SplitPlane y = planeFromTilts(c, -90, 0, 0);
    CHECK_THAT(y.normal[1], WithinAbs(1, 1e-12));
    CHECK(sideName(x.normal) == "right");
    CHECK(sideName(y.normal) == "back");
    CHECK(sideName({0, 0, -1}) == "bottom");

    for (const auto& [tx, ty, off] : {std::tuple{20.0, 35.0, 3.5}, {-60.0, 10.0, -2.0}, {0.0, 0.0, 0.0}}) {
        double rx = 0, ry = 0, ro = 0;
        tiltsOfPlane(planeFromTilts(c, tx, ty, off), c, rx, ry, ro);
        CHECK_THAT(rx, WithinAbs(tx, 1e-6));
        CHECK_THAT(ry, WithinAbs(ty, 1e-6));
        CHECK_THAT(ro, WithinAbs(off, 1e-6));
    }
}

TEST_CASE("Moving the cut of a moved, turned half", "[split]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    const auto made = splitBody(stack, doc, box, SplitPlane{{0, 0, 10}, {0, 0, 1}}, SplitKeep::Front);
    const ObjectId top = made[0];

    // Move the half 50 mm along X; its plane moves with it.
    Placement p = doc.find(top)->placement;
    p.x += 50;
    stack.execute(std::make_unique<SetPlacementCommand>(top, p, false));
    const SplitPlane now = splitPlaneOf(doc, top);
    CHECK_THAT(now.point[0], WithinAbs(50, 1e-9));
    CHECK_THAT(now.point[2], WithinAbs(10, 1e-9));

    // Lower the cut to Z = 4: the half grows downward, in place.
    const Body before = *doc.find(top);
    const Body after = withSplitPlane(doc, top, SplitPlane{{50, 0, 4}, {0, 0, 1}});
    stack.execute(std::make_unique<EditBodyCommand>(before, after, "Move cut"));
    const Bounds b = doc.worldBounds(top);
    CHECK_THAT(b.min[2], WithinAbs(4, 1e-4));
    CHECK_THAT(b.min[0], WithinAbs(40, 1e-4));
    CHECK_THROWS_AS(withSplitPlane(doc, top, SplitPlane{{50, 0, 40}, {0, 0, 1}}), std::invalid_argument);
}

TEST_CASE("Split results survive save and load", "[split][file]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    splitBody(stack, doc, box, SplitPlane{{0, 0, 7}, {0.2, 0, 1}}, SplitKeep::Both);
    const auto bodies = file::fromJson(file::toJson(doc, "test"));
    REQUIRE(bodies.size() == doc.bodyCount());
    for (std::size_t i = 0; i < bodies.size(); ++i)
        CHECK(bodies[i] == doc.bodies()[i]);
    Document loaded;
    loaded.replaceAll(bodies);
    for (ObjectId id : doc.topLevelIds())
        CHECK_THAT(volumeOf(loaded.shape(id)), WithinRel(volumeOf(doc.shape(id)), 1e-9));
}
