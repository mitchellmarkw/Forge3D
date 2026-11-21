// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 67: joint standards for a figure line.
#include "forge/core/JointStandard.h"

#include <catch2/catch_test_macros.hpp>

using namespace forge;
using namespace forge::standards;

namespace {

Standard fiveInch()
{
    SocketFinish ball;
    ball.kind = SocketKind::Ball;
    ball.size = 6.0;
    ball.grip = 12.0;
    ball.ridges = 4;
    ball.fit = 0.15;   // the printer's: not kept
    ball.turn = 30.0;  // the spot's: not kept
    SocketFinish peg;
    peg.kind = SocketKind::Peg;
    peg.size = 3.0;
    peg.depth = 5.0;
    HingeFinish elbow;
    elbow.kind = HingeKind::Double;
    elbow.width = 3.5;
    elbow.barrel = 6.0;
    elbow.pinSize = 2.2;
    elbow.clicks = 8;
    elbow.half = HingeHalf::Tab;
    elbow.link = 7;
    Standard s;
    s.name = "5-inch line";
    s.notes = "Compatible with the Mk2 bucks";
    s.joints = {jointFrom("Shoulder ball", ball), jointFrom("Wrist peg", peg), jointFrom("Elbow", elbow)};
    return s;
}

}  // namespace

TEST_CASE("Joint standards: what's kept, and what's applied", "[standards]")
{
    const Standard s = fiveInch();
    REQUIRE_NOTHROW(check(s));
    const SocketFinish& kept = *s.joints[0].socket;
    CHECK(kept.size == 6.0);
    CHECK(kept.grip == 12.0);
    CHECK(kept.ridges == 4);
    CHECK(kept.fit == SocketFinish{}.fit);    // the printer's fit isn't part of it
    CHECK(kept.turn == 0.0);
    CHECK(s.joints[2].hinge->half == HingeHalf::Fork);
    CHECK(s.joints[2].hinge->link == 0);
    CHECK(describe(s.joints[0]).find("6") != std::string::npos);

    // Applied to a socket being made: the sizes come over, the fit and turn stay.
    SocketFinish now;
    now.kind = SocketKind::Peg;
    now.size = 2.0;
    now.fit = 0.2;
    now.resin = false;
    now.turn = 45.0;
    const SocketFinish after = applied(*s.joints[0].socket, now);
    CHECK(after.kind == SocketKind::Ball);
    CHECK(after.size == 6.0);
    CHECK(after.fit == 0.2);
    CHECK_FALSE(after.resin);
    CHECK(after.turn == 45.0);
    HingeFinish tab;
    tab.half = HingeHalf::Tab;
    tab.link = 3;
    const HingeFinish h = applied(*s.joints[2].hinge, tab);
    CHECK(h.kind == HingeKind::Double);
    CHECK(h.clicks == 8);
    CHECK(h.half == HingeHalf::Tab);
    CHECK(h.link == 3);

    // The tolerance test sized to the standard.
    const tolerance::Settings t = toleranceSizes(s, tolerance::Settings{});
    CHECK(t.ballSize == 6.0);
    CHECK(t.grip == 12.0);
    CHECK(t.pegSize == 3.0);
    CHECK(t.pegDepth == 5.0);
    CHECK(t.pinSize == 2.2);
}

TEST_CASE("Joint standards: kept as text, and checked", "[standards]")
{
    Standard other = fiveInch();
    other.name = "Mini";
    other.joints.pop_back();
    const std::vector<Standard> library{fiveInch(), other};
    const auto back = fromJson(toJson(library));
    CHECK(back == library);

    CHECK_THROWS_AS(fromJson("not json"), std::invalid_argument);
    CHECK_THROWS_AS(fromJson(R"({"format":"something-else"})"), std::invalid_argument);
    CHECK_THROWS_AS(fromJson(R"({"format":"forge3d-joint-standards","version":99,"standards":[]})"),
                    std::invalid_argument);

    Standard bad = fiveInch();
    bad.joints[1].name = "Shoulder ball";
    CHECK_THROWS_AS(check(bad), std::invalid_argument);
    bad = fiveInch();
    bad.joints[0].socket->size = -1;
    CHECK_THROWS_AS(check(bad), std::invalid_argument);
    bad = fiveInch();
    bad.name.clear();
    CHECK_THROWS_AS(check(bad), std::invalid_argument);

    CHECK(uniqueName(library, "Mini") == "Mini 2");
    CHECK(uniqueName(library, "6-inch line") == "6-inch line");
}
