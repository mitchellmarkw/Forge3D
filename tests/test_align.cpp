// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 3c2: combined shapes turn about their visible centre; Align Shapes.
#include "forge/core/Align.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using namespace forge;
using Catch::Matchers::WithinAbs;

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

// Pivot deliberately left at the middle of all the parts (the 0.5.0 rule), so the
// tests show rotation ignores it and uses the visible centre.
ObjectId combine(CommandStack& stack, Document& doc, BooleanOp op, std::vector<ObjectId> parts)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName(toString(op));
    BooleanSpec spec;
    spec.op = op;
    spec.operands = std::move(parts);
    const auto c = doc.worldBounds().center();
    spec.pivot = {c[0], c[1], c[2]};
    r.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

void requireCentre(const Bounds& b, double x, double y, double z, double tol = 1e-3)
{
    REQUIRE(b.valid);
    const auto c = b.center();
    CHECK_THAT(c[0], WithinAbs(x, tol));
    CHECK_THAT(c[1], WithinAbs(y, tol));
    CHECK_THAT(c[2], WithinAbs(z, tol));
}

void turn(CommandStack& stack, Document& doc, ObjectId id, int axis, double degrees)
{
    const Body* b = doc.find(id);
    const Placement p = rotatedAboutPlateAxis(*b, axis, degrees, doc.visibleCentre(id));
    stack.execute(std::make_unique<SetPlacementCommand>(id, p, false));
}

}  // namespace

TEST_CASE("Visible centre ignores the cut-away cylinder of a Subtract", "[align]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId cube = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId cyl = add(stack, doc, CylinderSpec{5, 40}, 0, 0, -10);  // z -10 .. 30
    const ObjectId r = combine(stack, doc, BooleanOp::Subtract, {cube, cyl});

    const Vec3 vc = doc.visibleCentre(r);
    CHECK_THAT(vc[2], WithinAbs(10.0, 1e-4));  // the cube's middle, not the cylinder's
    requireCentre(doc.worldBounds(r), 0, 0, 10);

    for (int axis = 0; axis < 3; ++axis) {
        turn(stack, doc, r, axis, 90);
        requireCentre(doc.worldBounds(r), 0, 0, 10);
    }
    const auto size = doc.worldBounds(r).size();
    CHECK_THAT(size[0], WithinAbs(20, 1e-3));
    CHECK_THAT(size[2], WithinAbs(20, 1e-3));
}

TEST_CASE("A Merge turns about the middle of the combined height", "[align]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId block = add(stack, doc, BoxSpec{20, 40, 20});
    const ObjectId peg = add(stack, doc, CylinderSpec{5, 15}, 0, 0, 20);  // total height 35
    const ObjectId r = combine(stack, doc, BooleanOp::Merge, {block, peg});

    CHECK_THAT(doc.visibleCentre(r)[2], WithinAbs(17.5, 1e-4));
    turn(stack, doc, r, 1, 90);
    requireCentre(doc.worldBounds(r), 0, 0, 17.5);
    turn(stack, doc, r, 0, 90);
    requireCentre(doc.worldBounds(r), 0, 0, 17.5);
}

TEST_CASE("An Intersect turns about the shared part only", "[align]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});
    const ObjectId b = add(stack, doc, BoxSpec{20, 20, 20}, 10, 0, 0);  // overlap x 0..10
    const ObjectId r = combine(stack, doc, BooleanOp::Intersect, {a, b});

    requireCentre(doc.worldBounds(r), 5, 0, 10);
    turn(stack, doc, r, 2, 45);
    requireCentre(doc.worldBounds(r), 5, 0, 10);
}

TEST_CASE("Typed angles keep the visible centre in place", "[align]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId cube = add(stack, doc, BoxSpec{20, 20, 20}, 30, 0, 0);
    const ObjectId cyl = add(stack, doc, CylinderSpec{5, 40}, 30, 0, -10);
    const ObjectId r = combine(stack, doc, BooleanOp::Subtract, {cube, cyl});

    const Body* body = doc.find(r);
    const Placement p = withRotation(*body, {90, 0, 30}, doc.visibleCentre(r));
    CHECK(p.rotationX == 90);
    CHECK(p.rotationZ == 30);
    stack.execute(std::make_unique<SetPlacementCommand>(r, p, false));
    requireCentre(doc.worldBounds(r), 30, 0, 10);

    // Back to zero returns it exactly where it started.
    const Placement back = withRotation(*doc.find(r), {0, 0, 0}, doc.visibleCentre(r));
    CHECK_THAT(back.x, WithinAbs(0, 1e-9));
    CHECK_THAT(back.y, WithinAbs(0, 1e-9));
    CHECK_THAT(back.z, WithinAbs(0, 1e-9));
}

TEST_CASE("Primitives still turn about their own middle", "[align]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{10, 20, 30}, 5, 6, 7);
    const Body* b = doc.find(box);
    const Placement viaCentre = rotatedAboutPlateAxis(*b, 0, 30, doc.visibleCentre(box));
    const Placement old = rotatedAboutPlateAxis(*b, 0, 30);
    CHECK_THAT(viaCentre.x, WithinAbs(old.x, 1e-9));
    CHECK_THAT(viaCentre.y, WithinAbs(old.y, 1e-9));
    CHECK_THAT(viaCentre.z, WithinAbs(old.z, 1e-9));
    CHECK_THAT(viaCentre.rotationX, WithinAbs(old.rotationX, 1e-9));
}

TEST_CASE("World bounds are tight for tilted round shapes", "[align]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId cyl = add(stack, doc, CylinderSpec{5, 20});
    Placement p;
    p.rotationZ = 45;  // a box around the circle's corners would say 7.07
    stack.execute(std::make_unique<SetPlacementCommand>(cyl, p, false));
    const Bounds b = doc.worldBounds(cyl);
    CHECK_THAT(b.max[0], WithinAbs(5, 1e-3));
    CHECK_THAT(b.min[1], WithinAbs(-5, 1e-3));

    // Lying on its side, Sit on plate puts its lowest point exactly on the plate.
    p.rotationX = 90;
    p.z = 3;
    stack.execute(std::make_unique<SetPlacementCommand>(cyl, p, false));
    stack.execute(std::make_unique<SetPlacementCommand>(cyl, sitOnPlate(doc, cyl), false));
    CHECK_THAT(doc.worldBounds(cyl).min[2], WithinAbs(0, 1e-3));
}

TEST_CASE("Facing axis", "[align]")
{
    Bounds a, b;
    a.expand(-10, -10, 0);
    a.expand(10, 10, 20);
    b.expand(40, -5, 5);
    b.expand(50, 5, 15);
    CHECK(facingAxis(a, b) == 0);
    Bounds same = a;
    CHECK(facingAxis(a, same) == 2);  // same centre: Z
    Bounds above;
    above.expand(-2, -2, 30);
    above.expand(2, 2, 40);
    CHECK(facingAxis(a, above) == 2);
}

TEST_CASE("Align Shapes moves shape 2 against shape 1", "[align]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId a = add(stack, doc, BoxSpec{20, 20, 20});         // x -10..10, z 0..20
    const ObjectId b = add(stack, doc, BoxSpec{10, 10, 10}, 50, 3, 6);  // right of a, centre z 11

    const auto centreAfter = [&](AlignMode mode) {
        const Body original = *doc.find(b);
        const Placement p = alignedPlacement(doc, a, b, mode);
        CHECK(p.rotationX == original.placement.rotationX);  // position only
        stack.execute(std::make_unique<SetPlacementCommand>(b, p, false));
        const auto c = doc.worldBounds(b).center();
        stack.undo();
        return c;
    };
    const auto expectCentre = [](std::array<float, 3> c, double x, double y, double z) {
        CHECK_THAT(c[0], WithinAbs(x, 1e-3));
        CHECK_THAT(c[1], WithinAbs(y, 1e-3));
        CHECK_THAT(c[2], WithinAbs(z, 1e-3));
    };

    expectCentre(centreAfter(AlignMode::MeetAtFace), 15, 3, 11);     // touches a's right side
    expectCentre(centreAfter(AlignMode::FaceCentreX), 10, 3, 11);    // halfway into it
    expectCentre(centreAfter(AlignMode::FaceCentreY), 50, 10, 11);   // y: on a's +Y face
    expectCentre(centreAfter(AlignMode::FaceCentreZ), 50, 3, 20);    // z: on a's top face
    expectCentre(centreAfter(AlignMode::FaceCentreXYZ), 10, 0, 10);  // middle of a's right face
    expectCentre(centreAfter(AlignMode::CentreX), 0, 3, 11);
    expectCentre(centreAfter(AlignMode::CentreY), 50, 0, 11);
    expectCentre(centreAfter(AlignMode::CentreZ), 50, 3, 10);
    expectCentre(centreAfter(AlignMode::CentreXYZ), 0, 0, 10);

    SECTION("from the other side")
    {
        stack.execute(std::make_unique<SetPlacementCommand>(b, Placement{-50, 0, 4}, false));
        expectCentre(centreAfter(AlignMode::MeetAtFace), -15, 0, 9);
    }
    SECTION("a cylinder above meets the top face")
    {
        const ObjectId peg = add(stack, doc, CylinderSpec{5, 15}, 1, 2, 60);
        const Placement p = alignedPlacement(doc, a, peg, AlignMode::MeetAtFace);
        CHECK_THAT(p.z, WithinAbs(20, 1e-3));
        CHECK(p.x == 1);
        CHECK(p.y == 2);
    }
    SECTION("parts cannot be aligned")
    {
        const ObjectId r = combine(stack, doc, BooleanOp::Merge, {a, b});
        const ObjectId c = add(stack, doc, BoxSpec{5, 5, 5}, 80, 0, 0);
        CHECK_THROWS_AS(alignedPlacement(doc, a, c, AlignMode::CentreX), std::invalid_argument);
        CHECK_NOTHROW(alignedPlacement(doc, r, c, AlignMode::CentreX));
    }
}
