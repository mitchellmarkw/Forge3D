// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 88: a joint at a split.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/SplitJoint.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double x = 0, double y = 0, double z = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = std::move(spec);
    b.name = doc.uniqueName("Arm");
    b.placement.x = x;
    b.placement.y = y;
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

double volumeOf(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

}  // namespace

TEST_CASE("Split joint: the spot is the middle of the cut", "[splitjoint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 10, 30});
    const auto b = doc.worldBounds(box);
    const Vec3 mid{(b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, 15.0};
    const splitjoint::Spot s = splitjoint::planeSpot(doc, box, {mid, {0, 0, 1}});
    CHECK_THAT(s.room, WithinAbs(5.0, 0.05));
    CHECK_THAT(s.point[1], WithinAbs(mid[1], 0.05));
    CHECK_THAT(s.point[2], WithinAbs(15.0, 1e-6));
    CHECK(std::abs(s.point[0] - mid[0]) < 5.0 + 0.05);
    CHECK(s.normal[2] < -0.99);  // out of the top piece, into the bottom one
    CHECK(s.reach > 9.0);
    CHECK_THROWS_AS(splitjoint::planeSpot(doc, box, {{0, 0, 100}, {0, 0, 1}}), std::invalid_argument);
}

TEST_CASE("Split joint: a ball joint at a plane split", "[splitjoint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId arm = add(stack, doc, CylinderSpec{6, 40});
    const double whole = volumeOf(doc, arm);
    const auto b = doc.worldBounds(arm);
    const Vec3 mid{(b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, (b.min[2] + b.max[2]) / 2};
    splitjoint::Options o;
    o.kind = splitjoint::Kind::Ball;
    const double gap = splitjoint::gapFor(o, 6.0);
    CHECK(gap > 0.3);
    const splitjoint::Made made = splitjoint::splitWithJoint(stack, doc, arm, {mid, {0, 0, 1}}, o, 7);
    REQUIRE(made.pieces.size() == 2);
    CHECK(stack.undoLabel() == "Split Arm 1 with a ball joint");
    const ObjectId top = made.pieces[0], bottom = made.pieces[1];
    CHECK(doc.isTopLevel(top));
    CHECK(doc.isTopLevel(bottom));
    // The ball stands up out of the bottom piece into the top one; the bottom's face is cut back.
    CHECK(doc.worldBounds(bottom).max[2] > mid[2] + 1.0);
    CHECK(doc.worldBounds(top).min[2] > mid[2] - 1e-3);
    CHECK_THAT(doc.worldBounds(bottom).min[2], WithinAbs(b.min[2], 1e-3));  // the bottom still the bottom
    const double vt = volumeOf(doc, top), vb = volumeOf(doc, bottom);
    CHECK(vt < whole / 2 - 1.0);  // the socket hollowed out of the top
    const SocketFinish f = splitjoint::ballFor(o, 6.0);
    CHECK_THAT(f.size, WithinAbs(6.0, 0.01));
    const double ball = std::numbers::pi / 6.0 * f.size * f.size * f.size;
    CHECK(vb > whole / 2 - std::numbers::pi * 36.0 * gap + 0.5 * ball);  // cut back, the ball added

    // Saved and read back the same.
    const auto path = std::filesystem::temp_directory_path() / "splitjoint.f3d";
    file::save(doc, path, "test");
    Document reread;
    reread.replaceAll(file::load(path));
    CHECK_THAT(volumeOf(reread, top), WithinRel(vt, 1e-6));
    CHECK_THAT(volumeOf(reread, bottom), WithinRel(vb, 1e-6));

    // One undo puts it back.
    stack.undo();
    CHECK(doc.find(arm));
    CHECK(doc.isTopLevel(arm));
    CHECK(!doc.find(top));
}

TEST_CASE("Split joint: hinges, swivel and magnets at a plane split", "[splitjoint]")
{
    for (const auto kind : {splitjoint::Kind::Hinge, splitjoint::Kind::DoubleHinge, splitjoint::Kind::Swivel,
                            splitjoint::Kind::Magnets}) {
        Document doc;
        CommandStack stack(doc);
        const ObjectId arm = add(stack, doc, CylinderSpec{6, 40});
        const double whole = volumeOf(doc, arm);
        const auto b = doc.worldBounds(arm);
        const Vec3 mid{(b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, (b.min[2] + b.max[2]) / 2};
        splitjoint::Options o;
        o.kind = kind;
        INFO(splitjoint::title(kind));
        const splitjoint::Made made = splitjoint::splitWithJoint(stack, doc, arm, {mid, {0, 0, 1}}, o, 9);
        REQUIRE(made.pieces.size() == 2);
        const double v = volumeOf(doc, made.pieces[0]) + volumeOf(doc, made.pieces[1]);
        CHECK(v > 0.5 * whole);
        CHECK(v < whole);
        if (kind == splitjoint::Kind::Magnets)
            CHECK(made.extras.empty());
        if (kind == splitjoint::Kind::Hinge || kind == splitjoint::Kind::DoubleHinge)
            CHECK(!made.extras.empty());  // the pin (and the link)
    }
}

TEST_CASE("Split joint: too big a joint for the cut is refused", "[splitjoint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId arm = add(stack, doc, CylinderSpec{3, 20});
    const auto b = doc.worldBounds(arm);
    const Vec3 mid{(b.min[0] + b.max[0]) / 2, (b.min[1] + b.max[1]) / 2, (b.min[2] + b.max[2]) / 2};
    splitjoint::Options o;
    o.kind = splitjoint::Kind::Ball;
    o.size = 8.0;
    CHECK_THROWS_AS(splitjoint::splitWithJoint(stack, doc, arm, {mid, {0, 0, 1}}, o, 3), std::invalid_argument);
    CHECK(doc.find(arm));
    CHECK(doc.isTopLevel(arm));
    CHECK((!stack.canUndo() || stack.undoLabel() != "Split Arm 1 with a ball joint"));
}

TEST_CASE("Split joint: a ball joint at a mask split", "[splitjoint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId arm = add(stack, doc, CylinderSpec{6, 40});
    std::vector<MaskDab> dabs;
    for (int i = 0; i <= 10; ++i)
        dabs.push_back({{0, 0, 40.0 - i}, 9.0, false});
    const double whole = volumeOf(doc, arm);
    const splitjoint::Spot s = splitjoint::maskSpot(doc, arm, dabs, MaskCut::Flat);
    CHECK(s.normal[2] < -0.9);  // out of the hand end, down into the rest
    CHECK(s.room > 4.0);
    splitjoint::Options o;
    o.kind = splitjoint::Kind::Ball;
    const splitjoint::Made made = splitjoint::splitByMaskWithJoint(stack, doc, arm, dabs, MaskCut::Flat, o, 5);
    REQUIRE(made.pieces.size() == 2);
    const ObjectId hand = made.pieces[0], rest = made.pieces[1];
    // The ball reaches up from the rest into the hand.
    CHECK(doc.worldBounds(rest).max[2] > s.point[2] + 1.0);
    const double vh = volumeOf(doc, hand), vr = volumeOf(doc, rest);
    CHECK(vh + vr < whole);
    CHECK(vh > 50.0);
    CHECK_THROWS_AS(splitjoint::splitByMaskWithJoint(stack, doc, hand, dabs, MaskCut::Dome, o, 5),
                    std::invalid_argument);
    stack.undo();
    CHECK(doc.isTopLevel(arm));
}

