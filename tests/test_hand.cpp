// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 71: the hand generator.
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Hand.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Port.h"
#include "forge/core/Socket.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstdlib>
#include <fstream>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

SocketFinish wristPeg()
{
    SocketFinish s;
    s.kind = SocketKind::Peg;
    s.size = 3.0;
    s.depth = 4.0;
    s.fit = 0.1;
    return s;
}

}  // namespace

TEST_CASE("Hand: every pose, either hand, made closed at its size", "[hand]")
{
    for (int p = 0; p < kHandPoseCount; ++p)
        for (bool left : {false, true}) {
            HandSettings s;
            s.pose = HandPose(p);
            s.left = left;
            s.length = 16.0;
            const hand::Made m = hand::make(s);
            const MeshSolid solid = meshkernel::fromTriangles(m.mesh.positions, m.mesh.indices);
            INFO(hand::nameOf(s));
            CHECK(meshkernel::volume(solid) > 50.0);
            const Bounds b = m.mesh.bounds();
            CHECK_THAT(b.min[2], WithinAbs(0.0, 0.05));  // standing on its wrist
            CHECK(b.max[2] > 8.0);
            CHECK(b.max[2] < 17.5);
            // The thumb's on the side towards the body's middle: +X for a right hand.
            const double middleX = (b.min[0] + b.max[0]) / 2.0;
            if (s.pose == HandPose::Open)
                CHECK((left ? middleX < 0.0 : middleX > 0.0));
        }
    HandSettings s;
    CHECK(hand::nameOf(s) == "Right hand (fist)");
    s.length = 2.0;
    CHECK_THROWS_AS(hand::check(s), std::invalid_argument);
    CHECK_THAT(hand::lengthAtScale(12.0), WithinRel(15.75, 1e-9));
}

TEST_CASE("Hand: a fist is shorter than an open hand; pointing has one finger out", "[hand]")
{
    HandSettings s;
    s.length = 20.0;
    s.pose = HandPose::Open;
    const double open = hand::make(s).mesh.bounds().max[2];
    s.pose = HandPose::Fist;
    const double fist = hand::make(s).mesh.bounds().max[2];
    s.pose = HandPose::Point;
    const double point = hand::make(s).mesh.bounds().max[2];
    CHECK(open > 19.0);
    CHECK(fist < 0.8 * open);
    CHECK(point > 0.9 * open);
}

TEST_CASE("Hand: gripping wraps round a handle, with a hole through", "[hand]")
{
    Document doc;
    CommandStack stack(doc);
    HandSettings s;
    s.pose = HandPose::Grip;
    s.length = 16.0;
    s.handle = 3.0;
    const auto made = hand::addHands(stack, doc, s, wristPeg(), false, 10.0, 0.0);
    REQUIRE(made.size() == 1);
    const Body* top = doc.find(made[0]);
    REQUIRE(top);
    CHECK(top->name == "Right hand (gripping)");
    REQUIRE(top->boolean);
    CHECK(top->boolean->op == BooleanOp::Port);
    const ObjectId socketed = top->boolean->operands.front();
    CHECK(doc.find(socketed)->boolean->op == BooleanOp::Socket);
    const double withSocket = meshkernel::volume(doc.meshSolid(socketed));
    const ObjectId plain = doc.find(socketed)->boolean->operands.front();
    // The socket takes out the peg's room (3.2 across, 4.1 deep).
    CHECK_THAT(meshkernel::volume(doc.meshSolid(plain)) - withSocket, WithinRel(3.14159 * 1.6 * 1.6 * 4.1, 0.08));
    const double withHole = meshkernel::volume(doc.meshSolid(made[0]));
    // The hole takes away a good part of a rod the handle's size, a hand's width long
    // (the fingers wrap round it, so most of its length is already open).
    CHECK(withHole < withSocket);

    const hand::Made m = hand::make(s, hand::socketRadius(wristPeg()), hand::socketDepth(wristPeg()));
    REQUIRE(m.grip);
    CHECK_THAT(m.gripMiddle[0], WithinAbs(0.0, 1e-9));

    // One undo step.
    const std::size_t count = doc.bodies().size();
    stack.undo();
    CHECK(doc.bodies().size() < count);
    CHECK(doc.find(made[0]) == nullptr);
}

TEST_CASE("Hand: a pair, the wrist socket with room round it", "[hand]")
{
    Document doc;
    CommandStack stack(doc);
    HandSettings s;
    s.pose = HandPose::Relaxed;
    s.length = 12.0;
    SocketFinish ball;
    ball.kind = SocketKind::Ball;
    ball.size = 4.0;
    ball.fit = 0.1;
    const auto made = hand::addHands(stack, doc, s, ball, true, 0.0, 0.0);
    REQUIRE(made.size() == 2);
    CHECK(doc.find(made[0])->name == "Right hand (relaxed)");
    CHECK(doc.find(made[1])->name == "Left hand (relaxed)");
    CHECK(doc.worldBounds(made[0]).max[0] < doc.worldBounds(made[1]).min[0] + 1.0);
    // The wrist grew to hold the ball socket with a wall round it.
    const hand::Made m = hand::make(s, hand::socketRadius(ball), hand::socketDepth(ball));
    CHECK(m.wristHalfThickness >= hand::socketRadius(ball) + 0.8 - 1e-9);
    for (ObjectId id : made)
        CHECK(meshkernel::volume(doc.meshSolid(id)) > 0.0);

    if (const char* out = std::getenv("FORGE_HAND_DUMP")) {
        Document all;
        CommandStack st(all);
        int i = 0;
        for (int p = 0; p < kHandPoseCount; ++p) {
            HandSettings h;
            h.pose = HandPose(p);
            h.length = 20.0;
            hand::addHands(st, all, h, wristPeg(), true, (i++) * 45.0 - 90.0, 0.0);
        }
        file::save(all, out, "hands");
        for (int p = 0; p < kHandPoseCount; ++p) {
            Document one;
            CommandStack os(one);
            HandSettings h;
            h.pose = HandPose(p);
            h.length = 90.0;
            h.handle = 15.0;
            hand::addHands(os, one, h, std::nullopt, false, 0.0, 0.0);
            file::save(one, std::string(out) + "." + std::to_string(p) + ".f3d", "hands");
        }
    }
}
