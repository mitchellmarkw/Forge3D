// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/Transform.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

void requireSameRotation(const Transform& a, const Transform& b)
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            CHECK_THAT(a.m[r][c], WithinAbs(b.m[r][c], 1e-9));
}

void requireNear(const Vec3& a, const Vec3& b, double tol = 1e-9)
{
    for (int i = 0; i < 3; ++i)
        CHECK_THAT(a[i], WithinAbs(b[i], tol));
}

Body makeBody(PrimitiveSpec spec, Placement p)
{
    Body b;
    b.id = 1;
    b.name = "B";
    b.spec = spec;
    b.placement = p;
    return b;
}

Placement tilted(double x, double y, double z, double rx, double ry, double rz)
{
    Placement p;
    p.x = x;
    p.y = y;
    p.z = z;
    p.rotationX = rx;
    p.rotationY = ry;
    p.rotationZ = rz;
    return p;
}

// Where a body point ends up on the plate.
Vec3 world(const Body& b, const Vec3& local)
{
    return placementTransform(b.placement, b.spec).apply(local);
}

}  // namespace

TEST_CASE("eulerXYZ recovers the angles that built a rotation", "[gizmo][transform]")
{
    const auto [x, y, z] = GENERATE(table<double, double, double>({
        {0, 0, 0}, {30, 0, 0}, {0, 45, 0}, {0, 0, 120}, {10, 20, 30}, {-75, 60, 170}, {135, -30, -90},
    }));
    const Vec3 e = eulerXYZ(Transform::rotationXYZ(x, y, z));
    requireSameRotation(Transform::rotationXYZ(e[0], e[1], e[2]), Transform::rotationXYZ(x, y, z));
}

TEST_CASE("eulerXYZ copes with a 90 degree tilt (gimbal lock)", "[gizmo][transform]")
{
    for (double y : {90.0, -90.0}) {
        const Transform r = Transform::rotationXYZ(25, y, 40);
        const Vec3 e = eulerXYZ(r);
        CHECK_THAT(std::abs(e[1]), WithinAbs(90.0, 1e-6));
        requireSameRotation(Transform::rotationXYZ(e[0], e[1], e[2]), r);
    }
}

TEST_CASE("turning about a plate axis keeps the centre and adds the turn", "[gizmo]")
{
    const Body b = makeBody(BoxSpec{40, 20, 10}, tilted(15, -5, 2, 0, 0, 30));
    const Vec3 centreBefore = world(b, pivotOf(b.spec));

    Body turned = b;
    turned.placement = rotatedAboutPlateAxis(b, 2, 45);
    CHECK_THAT(turned.placement.rotationZ, WithinAbs(75.0, 1e-9));
    requireNear(world(turned, pivotOf(turned.spec)), centreBefore);
}

TEST_CASE("turning about X after a Z turn uses the plate's X axis", "[gizmo]")
{
    // A 40 mm bar along X, turned 90 degrees on the plate so it lies along Y.
    const Body b = makeBody(BoxSpec{40, 4, 4}, tilted(0, 0, 0, 0, 0, 90));
    Body turned = b;
    turned.placement = rotatedAboutPlateAxis(b, 0, 90);  // plate X: lifts the bar upright
    // The bar's +X end (local (20, 0, 2)) now points straight up from the centre (0, 0, 2).
    requireNear(world(turned, {20, 0, 2}), {0, 0, 22}, 1e-6);
}

TEST_CASE("dragging a box face keeps the opposite face still", "[gizmo]")
{
    const Placement p = GENERATE(tilted(0, 0, 0, 0, 0, 0), tilted(10, 20, 5, 0, 0, 37),
                                 tilted(-30, 4, 12, 25, -40, 110));
    const Body b = makeBody(BoxSpec{40, 30, 20}, p);

    SECTION("+X face grows the width; -X face stays")
    {
        const Body r = resizedFace(b, 0, +1, 55);
        CHECK(std::get<BoxSpec>(r.spec).width == 55.0);
        requireNear(world(r, {-27.5, 0, 10}), world(b, {-20, 0, 10}), 1e-9);
        // The dragged face moved out by exactly the 15 mm the box grew.
        const Vec3 moved = world(r, {27.5, 0, 10});
        const Vec3 was = world(b, {20, 0, 10});
        const double dx = moved[0] - was[0], dy = moved[1] - was[1], dz = moved[2] - was[2];
        CHECK_THAT(std::sqrt(dx * dx + dy * dy + dz * dz), WithinAbs(15.0, 1e-9));
    }
    SECTION("-Y face grows the depth; +Y face stays")
    {
        const Body r = resizedFace(b, 1, -1, 50);
        requireNear(world(r, {0, 25, 10}), world(b, {0, 15, 10}), 1e-9);
    }
    SECTION("top face grows the height; bottom stays")
    {
        const Body r = resizedFace(b, 2, +1, 32);
        requireNear(world(r, {0, 0, 0}), world(b, {0, 0, 0}), 1e-9);
    }
    SECTION("bottom face grows the height; top stays")
    {
        const Body r = resizedFace(b, 2, -1, 32);
        requireNear(world(r, {0, 0, 32}), world(b, {0, 0, 20}), 1e-9);
    }
}

TEST_CASE("cylinder handles change radius around the centre line, or the height", "[gizmo]")
{
    const Body b = makeBody(CylinderSpec{10, 30}, tilted(5, 5, 0, 30, 0, 45));
    const Body wider = resizedFace(b, 0, +1, 14);
    CHECK(std::get<CylinderSpec>(wider.spec).radiusX == 14.0);
    CHECK(std::get<CylinderSpec>(wider.spec).radiusY == 14.0);  // stays round by default
    requireNear(world(wider, {0, 0, 0}), world(b, {0, 0, 0}), 1e-9);    // base centre fixed
    requireNear(world(wider, {0, 0, 30}), world(b, {0, 0, 30}), 1e-9);  // top centre fixed

    const Body taller = resizedFace(b, 2, +1, 45);
    requireNear(world(taller, {0, 0, 0}), world(b, {0, 0, 0}), 1e-9);
}

TEST_CASE("sizes never go below the minimum", "[gizmo]")
{
    const Body b = makeBody(BoxSpec{10, 10, 10}, {});
    CHECK(std::get<BoxSpec>(resizedFace(b, 0, 1, -50).spec).width == 0.1);
    ResizeOptions atLeastOne;
    atLeastOne.minValue = 1.0;
    CHECK(dimension(resizedFace(b, 2, -1, 0.0, atLeastOne).spec, 2) == 1.0);
}

TEST_CASE("an edit command records a whole drag as one undo step", "[gizmo][commands]")
{
    Document doc;
    CommandStack stack(doc);
    Body b = makeBody(BoxSpec{20, 20, 20}, {});
    b.id = doc.allocateId();
    stack.execute(std::make_unique<AddBodyCommand>(b));

    // The drag previews directly on the document...
    const Body before = *doc.find(b.id);
    Body after = resizedFace(before, 0, 1, 35);
    doc.setSpec(b.id, after.spec);
    doc.setPlacement(b.id, after.placement);
    // ...then records one step.
    stack.execute(std::make_unique<EditBodyCommand>(before, after, "Resize"));
    CHECK(stack.count() == 2);
    CHECK(stack.undoLabel() == "Resize");

    stack.undo();
    CHECK(*doc.find(b.id) == before);
    stack.redo();
    CHECK(*doc.find(b.id) == after);
}

TEST_CASE("an edit command with a bad size changes nothing", "[gizmo][commands]")
{
    Document doc;
    CommandStack stack(doc);
    Body b = makeBody(BoxSpec{20, 20, 20}, {});
    b.id = doc.allocateId();
    stack.execute(std::make_unique<AddBodyCommand>(b));
    Body bad = b;
    bad.spec = BoxSpec{-1, 20, 20};
    bad.placement.x = 50;
    CHECK_THROWS(stack.execute(std::make_unique<EditBodyCommand>(b, bad, "Resize")));
    CHECK(*doc.find(b.id) == b);
    CHECK(stack.count() == 1);
}

TEST_CASE("uniform scaling keeps proportions and the base on the plate", "[gizmo][uniform]")
{
    const Body b = makeBody(BoxSpec{40, 20, 10}, tilted(5, 5, 0, 0, 0, 0));
    ResizeOptions uniform;
    uniform.uniform = true;

    SECTION("from a side handle")
    {
        const Body r = resizedFace(b, 0, +1, 60, uniform);  // width 40 -> 60: x1.5
        CHECK(std::get<BoxSpec>(r.spec) == BoxSpec{60, 30, 15});
        CHECK_THAT(r.placement.z, WithinAbs(0.0, 1e-9));                      // still on the plate
        requireNear(world(r, {-30, 0, 0}), world(b, {-20, 0, 0}), 1e-9);      // opposite side, base level
    }
    SECTION("from the top handle")
    {
        const Body r = resizedFace(b, 2, +1, 5, uniform);  // height 10 -> 5: x0.5
        CHECK(std::get<BoxSpec>(r.spec) == BoxSpec{20, 10, 5});
        requireNear(world(r, {0, 0, 0}), world(b, {0, 0, 0}), 1e-9);
    }
    SECTION("cylinders scale every size")
    {
        const Body c = makeBody(CylinderSpec{10, 6, 30}, {});
        const Body r = resizedFace(c, 1, +1, 9, uniform);  // radius Y 6 -> 9: x1.5
        CHECK(std::get<CylinderSpec>(r.spec) == CylinderSpec{15, 9, 45});
        CHECK_THAT(r.placement.z, WithinAbs(0.0, 1e-9));
    }
    SECTION("never shrinks any size below the minimum")
    {
        const Body r = resizedFace(b, 0, +1, 0.5, uniform);  // would make height 0.125
        CHECK(dimension(r.spec, 2) >= uniform.minValue - 1e-12);
        CHECK_THAT(dimension(r.spec, 0) / dimension(r.spec, 2), WithinAbs(4.0, 1e-9));  // proportions kept
    }
}

TEST_CASE("turning off keep-round makes an elliptical cylinder", "[gizmo][ellipse]")
{
    const Body b = makeBody(CylinderSpec{10, 30}, tilted(3, 4, 0, 0, 0, 20));
    ResizeOptions oneAxis;
    oneAxis.keepRound = false;
    const Body r = resizedFace(b, 1, -1, 4, oneAxis);
    const auto& cyl = std::get<CylinderSpec>(r.spec);
    CHECK(cyl.radiusX == 10.0);
    CHECK(cyl.radiusY == 4.0);
    CHECK_FALSE(cyl.isRound());
    requireNear(world(r, {0, 0, 0}), world(b, {0, 0, 0}), 1e-9);  // centre line stays
}
