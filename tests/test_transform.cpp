// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Document.h"
#include "forge/core/Picking.h"
#include "forge/core/Transform.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {
void requireNear(const Vec3& a, const Vec3& b, double tol = 1e-9)
{
    for (int i = 0; i < 3; ++i)
        CHECK_THAT(a[i], WithinAbs(b[i], tol));
}

ObjectId addBox(Document& doc, BoxSpec spec, Placement p)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = doc.uniqueName("Box");
    b.placement = p;
    doc.insertBody(b);
    return b.id;
}
}  // namespace

TEST_CASE("single-axis rotations follow the right-hand rule", "[transform]")
{
    requireNear(Transform::rotationXYZ(90, 0, 0).apply({0, 1, 0}), {0, 0, 1});
    requireNear(Transform::rotationXYZ(0, 90, 0).apply({0, 0, 1}), {1, 0, 0});
    requireNear(Transform::rotationXYZ(0, 0, 90).apply({1, 0, 0}), {0, 1, 0});
}

TEST_CASE("rotations apply X first, then Y, then Z", "[transform]")
{
    // X 90 takes +Y to +Z; then Y 90 takes +Z to +X; then Z 90 takes +X to +Y.
    requireNear(Transform::rotationXYZ(90, 90, 90).apply({0, 1, 0}), {0, 1, 0});
    // Z alone would take +Y to -X; the combined result differs, proving the order matters.
    requireNear(Transform::rotationXYZ(0, 0, 90).apply({0, 1, 0}), {-1, 0, 0});
}

TEST_CASE("inverse undoes the transform", "[transform]")
{
    const Transform t = Transform::translation({5, -3, 12}) * Transform::rotationXYZ(30, -45, 110);
    const Vec3 p{7.5, 2.0, -4.0};
    requireNear(t.inverse().apply(t.apply(p)), p);
    requireNear((t.inverse() * t).apply({1, 2, 3}), {1, 2, 3});
}

TEST_CASE("with no rotation the placement is a plain move", "[transform]")
{
    Placement p;
    p.x = 10;
    p.y = 20;
    p.z = 3;
    requireNear(placementTransform(p, BoxSpec{40, 30, 20}).apply({0, 0, 0}), {10, 20, 3});
}

TEST_CASE("bodies turn about their own centre", "[transform]")
{
    Placement p;
    p.x = 100;
    p.rotationX = 90;
    const Transform t = placementTransform(p, BoxSpec{40, 30, 20});
    // The centre of a 20 mm tall box is at height 10 and stays there.
    requireNear(t.apply({0, 0, 10}), {100, 0, 10});
}

TEST_CASE("tilting a box changes its world bounds", "[transform][document]")
{
    Document doc;
    Placement p;
    p.rotationX = 90;  // stand the 40 x 30 x 20 box on its side: depth and height swap
    const ObjectId id = addBox(doc, BoxSpec{40, 30, 20}, p);
    const Bounds wb = doc.worldBounds(id);
    CHECK_THAT(wb.max[0] - wb.min[0], WithinAbs(40, 1e-3));
    CHECK_THAT(wb.max[1] - wb.min[1], WithinAbs(20, 1e-3));
    CHECK_THAT(wb.max[2] - wb.min[2], WithinAbs(30, 1e-3));
    CHECK_THAT(wb.min[2], WithinAbs(-5, 1e-3));  // centre stays at 10, so 15 below it
}

TEST_CASE("sitOnPlate moves a tilted body so it rests on Z = 0", "[transform][document]")
{
    Document doc;
    Placement p;
    p.rotationX = 90;
    p.z = 7;
    const ObjectId id = addBox(doc, BoxSpec{40, 30, 20}, p);
    doc.setPlacement(id, sitOnPlate(doc, id));
    CHECK_THAT(doc.worldBounds(id).min[2], WithinAbs(0, 1e-3));
    CHECK(doc.find(id)->placement.rotationX == 90.0);  // only the height changed
}

TEST_CASE("picking follows tilted bodies", "[transform][picking]")
{
    Document doc;
    Placement p;
    p.rotationY = 90;  // a 60 mm long bar along X, tipped up to stand along Z
    const ObjectId id = addBox(doc, BoxSpec{60, 10, 10}, p);
    // Standing up, the bar reaches z = 5 + 30 = 35 (its centre stays at z = 5).
    const auto hit = pick(doc, Ray{{0, 0, 500}, {0, 0, -1}});
    REQUIRE(hit);
    CHECK(hit->id == id);
    CHECK_THAT(hit->point[2], WithinAbs(35.0, 1e-6));
    // Where the bar used to lie, there is nothing now.
    CHECK_FALSE(pick(doc, Ray{{25, 0, 500}, {0, 0, -1}}));
}
