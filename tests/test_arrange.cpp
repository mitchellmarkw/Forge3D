// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 4c: Duplicate, copy / paste, Mirror, Group.
#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Scale.h"

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

TEST_CASE("Duplicate puts copies beside the originals", "[arrange]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 10, 10});
    const ObjectId b = add(stack, doc, CylinderSpec{5, 10}, 30);  // x 25..35: together -10..35
    const Vec3 offset = besideOffset(doc, {a, b});
    CHECK_THAT(offset[0], WithinAbs(45 + 5, 1e-3));
    const auto made = addCopies(stack, doc, {captureTree(doc, a), captureTree(doc, b)}, offset, "Duplicate");
    REQUIRE(made.size() == 2);
    CHECK(doc.find(made[0])->name == "Box 1 copy");
    CHECK_THAT(doc.worldBounds(made[0]).min[0], WithinAbs(40, 1e-3));
    CHECK(stack.undoLabel() == "Duplicate");
    stack.undo();
    CHECK(doc.bodyCount() == 2);
}

TEST_CASE("Copies of a combined shape copy its parts", "[arrange]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, CylinderSpec{5, 30});
    const ObjectId g = groupBodies(stack, doc, {a, b});
    const BodyTree tree = captureTree(doc, g);
    REQUIRE(tree.size() == 3);
    CHECK(tree.back().id == g);

    // Paste after the original was deleted: the clipboard still has it.
    stack.execute(std::make_unique<RemoveBodyCommand>(g));
    stack.execute(std::make_unique<RemoveBodyCommand>(a));
    stack.execute(std::make_unique<RemoveBodyCommand>(b));
    CHECK(doc.empty());
    const auto pasted = addCopies(stack, doc, {tree}, {0, 0, 0}, "Paste");
    CHECK(doc.bodyCount() == 3);
    const Body* copy = doc.find(pasted.front());
    CHECK(copy->name == "Group 1 copy");
    CHECK(doc.find(copy->boolean->operands[0])->name == "Box 1 copy");
    CHECK(doc.isTopLevel(pasted.front()));
    CHECK_FALSE(doc.isTopLevel(copy->boolean->operands[1]));
}

TEST_CASE("Mirror flips a shape in place", "[arrange]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId w = add(stack, doc, WedgeSpec{20, 10, 10}, 50);  // tall end at -X
    const Bounds before = doc.worldBounds(w);
    const auto made = mirrorBodies(stack, doc, {w}, 0);
    REQUIRE(made.size() == 1);
    const ObjectId m = made.front();
    CHECK(doc.find(m)->name == "Wedge 1 (mirrored)");
    const Bounds after = doc.worldBounds(m);
    for (int i = 0; i < 3; ++i) {
        CHECK_THAT(after.min[i], WithinAbs(before.min[i], 1e-4));
        CHECK_THAT(after.max[i], WithinAbs(before.max[i], 1e-4));
    }
    CHECK(BRepCheck_Analyzer(doc.shape(m)).IsValid());
    CHECK_THAT(volumeOf(doc.shape(m)), WithinRel(1000.0, 1e-9));  // inside out would be negative
    // The tall end is now at +X, so most of the volume is right of the middle.
    const TopoDS_Shape s = doc.shape(m);
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    CHECK(props.CentreOfMass().X() > 50.0);  // was left of the middle, now right

    // Upside down, then Separate takes the mirror away.
    stack.undo();
    const ObjectId flipped = mirrorBodies(stack, doc, {w}, 2).front();
    CHECK_THAT(doc.worldBounds(flipped).min[2], WithinAbs(0, 1e-4));
    stack.execute(std::make_unique<SeparateCommand>(flipped));
    CHECK(doc.topLevelIds() == std::vector<ObjectId>{w});
}

TEST_CASE("Mirrored and grouped shapes combine, scale and save", "[arrange]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{10, 10, 10});
    const ObjectId b = add(stack, doc, BoxSpec{10, 10, 10}, 30);
    const ObjectId g = groupBodies(stack, doc, {a, b});
    CHECK(doc.find(g)->name == "Group 1");
    CHECK_THAT(volumeOf(doc.shape(g)), WithinRel(2000.0, 1e-9));
    CHECK_THROWS_AS(groupBodies(stack, doc, {g}), std::invalid_argument);

    // Stretch the group: both boxes get taller, still apart.
    std::vector<Body> before;
    const auto after = scaledTree(doc, g, {1, 1, 2}, {15, 0, 0});
    for (const Body& x : after)
        before.push_back(*doc.find(x.id));
    stack.execute(std::make_unique<EditBodiesCommand>(before, after, "Resize"));
    CHECK_THAT(doc.worldBounds(g).max[2], WithinAbs(20, 1e-4));

    const ObjectId m = mirrorBodies(stack, doc, {g}, 1).front();
    const auto bodies = file::fromJson(file::toJson(doc, "test"));
    Document loaded;
    loaded.replaceAll(bodies);
    CHECK(loaded.find(m)->boolean->op == BooleanOp::Mirror);
    CHECK_THAT(volumeOf(loaded.shape(m)), WithinRel(volumeOf(doc.shape(m)), 1e-9));
}

TEST_CASE("Copies land clear of everything on the plate", "[arrange]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 10, 10});      // x -10..10
    add(stack, doc, BoxSpec{10, 10, 10}, 40);                      // x 35..45
    CHECK_THAT(leftEdge(doc, {a}), WithinAbs(-10, 1e-4));
    const Vec3 offset = clearOfPlate(doc, leftEdge(doc, {a}));
    CHECK_THAT(offset[0], WithinAbs(45 + 5 + 10, 1e-4));
    const auto made = addCopies(stack, doc, {captureTree(doc, a)}, offset, "Duplicate");
    CHECK_THAT(doc.worldBounds(made.front()).min[0], WithinAbs(50, 1e-4));
}
