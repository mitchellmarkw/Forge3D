// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 75: "will it stand?"
#include "forge/core/Balance.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

struct Scene {
    Document doc;
    CommandStack stack{doc};
    ObjectId box(double w, double d, double h, double x, double y, double z)
    {
        Body b;
        b.id = doc.allocateId();
        b.spec = BoxSpec{w, d, h};
        b.name = "Box";
        b.placement.x = x;
        b.placement.y = y;
        b.placement.z = z;
        stack.execute(std::make_unique<AddBodyCommand>(b));
        return b.id;
    }
    std::vector<pose::PosedPart> parts() const
    {
        std::vector<pose::PosedPart> out;
        for (ObjectId id : doc.topLevelIds()) {
            pose::PosedPart p;
            p.id = id;
            p.solid = meshkernel::transformed(doc.meshSolid(id), doc.worldTransform(id));
            out.push_back(p);
        }
        return out;
    }
};

}  // namespace

TEST_CASE("Balance: a block stands, and how far it leans", "[balance]")
{
    Scene s;
    s.box(10, 10, 20, 0, 0, 0);
    const auto b = balance::of(s.parts());
    CHECK(b.stands());
    CHECK_THAT(b.weight, WithinRel(2000.0, 1e-6));
    CHECK_THAT(b.centre[2], WithinAbs(10.0, 1e-6));
    CHECK_THAT(b.margin, WithinAbs(5.0, 1e-6));
    CHECK_THAT(b.lean, WithinAbs(std::atan(0.5) * 180.0 / 3.14159265358979, 1e-6));
    REQUIRE(b.feet.size() == 1);
    CHECK_THAT(b.feet[0].area, WithinRel(100.0, 1e-6));
    CHECK(b.support.size() == 4);
}

TEST_CASE("Balance: heavy on one side, it tips that way", "[balance]")
{
    Scene s;
    s.box(6, 6, 2, 0, 0, 0);       // a small foot
    s.box(30, 6, 6, 15, 0, 2);     // a long, heavy arm out to +X, above it
    const auto b = balance::of(s.parts());
    CHECK_FALSE(b.stands());
    CHECK(b.margin < 0.0);
    CHECK(b.lean == 0.0);
    CHECK(b.toward[0] < -0.9);  // the foot's edge is back towards -X
    CHECK(b.feet.size() == 1);
}

TEST_CASE("Balance: a one-piece figure's two feet are two", "[balance]")
{
    Scene s;
    const ObjectId l = s.box(4, 6, 10, -5, 0, 0);
    const ObjectId r = s.box(4, 6, 10, 5, 0, 0);
    const ObjectId body = s.box(14, 6, 12, 0, 0, 10);
    Body merged;
    merged.id = s.doc.allocateId();
    merged.name = "Figure";
    BooleanSpec spec;
    spec.op = BooleanOp::Merge;
    spec.operands = {l, r, body};
    merged.boolean = spec;
    s.stack.execute(std::make_unique<CombineCommand>(merged));
    const auto parts = s.parts();
    const auto b = balance::of(parts);
    CHECK(b.stands());
    REQUIRE(b.feet.size() == 2);
    const auto spots = balance::pegSpots(parts, b);
    REQUIRE(spots.size() == 2);
    CHECK_THAT(std::abs(spots[0].point[0]), WithinAbs(5.0, 1e-4));
    CHECK_THAT(spots[0].point[0] + spots[1].point[0], WithinAbs(0.0, 1e-4));
}

TEST_CASE("Balance: posed, on two feet, with stand peg holes", "[balance]")
{
    Scene s;
    const ObjectId left = s.box(4, 6, 10, -5, 0, 0);
    const ObjectId right = s.box(4, 6, 10, 5, 0, 0);
    s.box(14, 6, 12, 0, 0, 10);  // the body on top
    auto parts = s.parts();
    auto b = balance::of(parts);
    CHECK(b.stands());
    CHECK(b.feet.size() == 2);
    CHECK_THAT(b.margin, WithinAbs(3.0, 1e-6));  // front to back is the narrow way
    auto spots = balance::pegSpots(parts, b);
    REQUIRE(spots.size() == 2);
    for (const auto& p : spots) {
        CHECK((p.part == left || p.part == right));
        CHECK_THAT(p.point[2], WithinAbs(0.0, 1e-6));
        CHECK_THAT(std::abs(p.point[0]), WithinAbs(5.0, 1e-6));
        CHECK_THAT(p.normal[2], WithinAbs(-1.0, 1e-9));
    }

    // One leg lifted (posed 3 mm up): it stands on the other only, and tips.
    for (auto& p : parts)
        if (p.id == left)
            p.moved = Transform::translation({0.0, 0.0, 3.0});
    b = balance::of(parts);
    CHECK(b.feet.size() == 1);
    CHECK_FALSE(b.stands());
    // Every part lifted together: the same balance, the ground with it; the holes still
    // in the soles as built.
    for (auto& p : parts)
        p.moved = Transform::translation({0.0, 0.0, 7.0});
    b = balance::of(parts);
    CHECK(b.stands());
    CHECK_THAT(b.ground, WithinAbs(7.0, 1e-6));
    spots = balance::pegSpots(parts, b);
    REQUIRE(spots.size() == 2);
    CHECK_THAT(spots[0].point[2], WithinAbs(0.0, 1e-6));
}
