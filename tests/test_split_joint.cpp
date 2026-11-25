// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 88: a joint at a split.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/SplitJoint.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <optional>
#include <utility>

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
    CHECK(stack.undoLabel() == "Split Arm with a ball joint");
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


// Slice 94: the joint follows its cut.
namespace {

// The highest point of what's merged onto the bottom piece: the ball's top.
double topOf(const Document& doc, ObjectId id)
{
    return doc.worldBounds(id).max[2];
}

int countJointFor(const Document& doc, std::uint32_t link)
{
    int n = 0;
    for (const Body& b : doc.bodies())
        n += b.jointFor == link ? 1 : 0;
    return n;
}

}  // namespace

TEST_CASE("Split joint: moving the cut moves the joint", "[splitjoint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId arm = add(stack, doc, CylinderSpec{6, 40});
    const auto b = doc.worldBounds(arm);
    const double cx = (b.min[0] + b.max[0]) / 2, cy = (b.min[1] + b.max[1]) / 2, mid = (b.min[2] + b.max[2]) / 2;
    splitjoint::Options o;
    o.kind = splitjoint::Kind::Ball;
    const std::uint32_t link = 4242;
    const splitjoint::Made made = splitjoint::splitWithJoint(stack, doc, arm, {{cx, cy, mid}, {0, 0, 1}}, o, link);
    const std::vector<ObjectId> cut = splitjoint::cutPieces(doc, link);
    REQUIRE(cut.size() == 2);
    CHECK(doc.find(cut[0])->boolean->cutJoint->first);
    CHECK_FALSE(doc.find(cut[1])->boolean->cutJoint->first);
    const int tagged = countJointFor(doc, link);
    CHECK(tagged >= 3);  // the socket step, the merge step and the ball
    const double ballTop = topOf(doc, made.pieces[1]);
    CHECK(ballTop > mid + 1.0);

    // Saved and read back: still knows its joint.
    {
        const auto path = std::filesystem::temp_directory_path() / "cutjoint.f3d";
        file::save(doc, path, "test");
        Document reread;
        reread.replaceAll(file::load(path));
        CHECK(reread.find(cut[0])->boolean->cutJoint == doc.find(cut[0])->boolean->cutJoint);
        CHECK(countJointFor(reread, link) == tagged);
    }

    // The top piece's cut moved up 6 mm: the bottom's goes with it, and the ball too.
    const std::size_t steps = stack.index();
    splitjoint::moveCut(stack, doc, cut[0], {{cx, cy, mid + 6.0}, {0, 0, 1}});
    CHECK(stack.index() == steps + 1);  // one undo step
    CHECK(stack.undoLabel() == "Move cut");
    CHECK(countJointFor(doc, link) == tagged);  // the old joint gone, a new one in its place
    const std::vector<ObjectId> now = splitjoint::cutPieces(doc, link);
    REQUIRE(now.size() == 2);
    const ObjectId topObject = history::objectOf(doc, now[0]), bottomObject = history::objectOf(doc, now[1]);
    CHECK(doc.isTopLevel(topObject));
    CHECK(doc.isTopLevel(bottomObject));
    CHECK_THAT(doc.worldBounds(topObject).min[2], WithinAbs(mid + 6.0, 1e-3));
    CHECK_THAT(topOf(doc, bottomObject), WithinAbs(ballTop + 6.0, 0.05));
    CHECK(doc.worldBounds(bottomObject).min[2] < b.min[2] + 1e-3);

    // One undo puts it all back.
    stack.undo();
    CHECK_THAT(topOf(doc, made.pieces[1]), WithinAbs(ballTop, 1e-6));
    CHECK_THAT(doc.worldBounds(made.pieces[0]).min[2], WithinAbs(mid, 1e-3));

    // A joint taken out by hand isn't put back when the cut moves.
    CHECK(splitjoint::removeJoint(stack, doc, link));
    CHECK(countJointFor(doc, link) == 0);
    CHECK(doc.isTopLevel(cut[0]));
    CHECK(doc.isTopLevel(cut[1]));
    splitjoint::moveCut(stack, doc, cut[0], {{cx, cy, mid + 3.0}, {0, 0, 1}});
    CHECK(countJointFor(doc, link) == 0);
    CHECK(doc.isTopLevel(cut[0]));
}

TEST_CASE("Split joint: changing the mask moves the joint", "[splitjoint]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId arm = add(stack, doc, CylinderSpec{6, 40});
    const auto paint = [](double from) {
        std::vector<MaskDab> dabs;
        for (double z = from; z <= 40.0; z += 1.0)
            dabs.push_back({{0, 0, z}, 9.0, false});
        return dabs;
    };
    splitjoint::Options o;
    o.kind = splitjoint::Kind::Magnets;
    const std::uint32_t link = 77;
    splitjoint::splitByMaskWithJoint(stack, doc, arm, paint(30.0), MaskCut::Flat, o, link);
    std::vector<ObjectId> cut = splitjoint::cutPieces(doc, link);
    REQUIRE(cut.size() == 2);
    const double before = doc.worldBounds(history::objectOf(doc, cut[0])).min[2];
    // The paint's edge 8 mm lower: both pieces cut there, magnets in again.
    std::vector<MaskDab> lower = paint(22.0);
    splitjoint::changeMask(stack, doc, cut[0], lower, std::nullopt, std::pair{MaskCut::Flat, 0.15});
    CHECK(stack.undoLabel() == "Change mask");
    cut = splitjoint::cutPieces(doc, link);
    REQUIRE(cut.size() == 2);
    const ObjectId hand = history::objectOf(doc, cut[0]), rest = history::objectOf(doc, cut[1]);
    CHECK(doc.isTopLevel(hand));
    CHECK(doc.isTopLevel(rest));
    CHECK(doc.worldBounds(hand).min[2] < before - 6.0);
    CHECK(doc.worldBounds(rest).max[2] < before - 6.0);  // the rest changed too
    CHECK(countJointFor(doc, link) == 2);                // a magnet pocket in each
    stack.undo();
    CHECK_THAT(doc.worldBounds(history::objectOf(doc, cut[0])).min[2], WithinAbs(before, 1e-3));
}
