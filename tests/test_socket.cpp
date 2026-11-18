// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 46: Quick Socket.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"
#include "forge/core/Socket.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double pi = std::numbers::pi;

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, Placement at = {})
{
    Body b;
    b.id = doc.allocateId();
    b.spec = std::move(spec);
    b.name = doc.uniqueName("Shape");
    b.placement = at;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId addSocket(CommandStack& stack, Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                   const SocketFinish& f)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName("Socketed");
    r.boolean = socketSpec(doc, target, where, f);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

double volumeOf(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

// The shape as it sits on the plate.
MeshSolid placed(const Document& doc, ObjectId id)
{
    return meshkernel::transformed(doc.meshSolid(id), doc.worldTransform(id));
}

// How much of shape b is inside shape a.
double overlap(const Document& doc, ObjectId a, ObjectId b)
{
    return meshkernel::volume(meshkernel::intersected({placed(doc, a), placed(doc, b)}));
}

}  // namespace

TEST_CASE("Socket: how a ball sits", "[socket]")
{
    SocketFinish f;
    f.size = 3.0;
    f.fit = 0.1;
    f.grip = 10.0;
    const auto snap = socket::ballSeat(f);
    CHECK_THAT(snap.radius, WithinAbs(1.6, 1e-12));
    CHECK_THAT(snap.opening, WithinAbs(2.7, 1e-12));  // narrower than the ball: it snaps in
    CHECK_THAT(snap.centreDepth, WithinAbs(std::sqrt(1.6 * 1.6 - 1.35 * 1.35), 1e-12));
    f.snap = false;
    const auto open = socket::ballSeat(f);
    CHECK(open.opening >= 3.0);         // the ball drops in
    CHECK_THAT(open.centreDepth, WithinAbs(1.5, 1e-12));  // its top level with the surface
    CHECK(socket::describe(f) == "Ball socket \xC3\x98" "3 (open)");

    f.grip = 60.0;
    f.snap = true;
    CHECK_THROWS_AS(socket::check(f), std::invalid_argument);
    f.grip = 10.0;
    f.size = 0.0;
    CHECK_THROWS_AS(socket::check(f), std::invalid_argument);
}

TEST_CASE("Socket: a snap-in ball socket takes its ball without touching", "[socket]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId block = add(stack, doc, BoxSpec{20, 20, 10});  // top at z = 10
    const double before = volumeOf(doc, block);
    SocketFinish f;
    f.size = 3.0;
    f.fit = 0.1;
    f.grip = 10.0;
    const ObjectId s = addSocket(stack, doc, block, {{2.0, 3.0, 10.0}, {0.0, 0.0, 1.0}}, f);

    // What's gone is the ball's hollow below the surface.
    const auto seat = socket::ballSeat(f);
    const double cap = seat.radius - seat.centreDepth;  // the part above the surface
    const double hollow = 4.0 / 3.0 * pi * std::pow(seat.radius, 3) - pi * cap * cap * (3 * seat.radius - cap) / 3.0;
    CHECK_THAT(before - volumeOf(doc, s), WithinRel(hollow, 0.01));
    CHECK(history::describe(doc, s) == "Ball socket \xC3\x98" "3 (snap-in)");

    // The matching ball, seated: inside the block but not cutting into it anywhere.
    Placement at;
    at.x = 2.0;
    at.y = 3.0;
    at.z = 10.0 - seat.centreDepth;
    const ObjectId ball = add(stack, doc, socket::matchingPart(f), at);
    CHECK_THAT(volumeOf(doc, ball), WithinRel(4.0 / 3.0 * pi * 1.5 * 1.5 * 1.5, 0.01));
    CHECK(overlap(doc, s, ball) < 0.001);
    // Without the fit, the hollow would be exactly the ball: with it, there's room.
}

TEST_CASE("Socket: a hexagon peg socket takes its peg, flats lined up", "[socket]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId block = add(stack, doc, BoxSpec{20, 20, 10});
    const double before = volumeOf(doc, block);
    SocketFinish f;
    f.kind = SocketKind::Peg;
    f.shape = PegShape::Hexagon;
    f.size = 3.0;
    f.depth = 4.0;
    f.fit = 0.1;
    const ObjectId s = addSocket(stack, doc, block, {{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}}, f);
    const double across = 3.2;  // the flats, with room either side
    const double hexagon = std::sqrt(3.0) / 2.0 * across * across;
    CHECK_THAT(before - volumeOf(doc, s), WithinRel(hexagon * 4.1, 0.01));  // and room at the bottom

    // The matching peg (a hexagon padded up), pushed in: no overlap.
    Placement at;
    at.z = 6.0;
    const ObjectId peg = add(stack, doc, socket::matchingPart(f), at);
    CHECK_THAT(volumeOf(doc, peg), WithinRel(std::sqrt(3.0) / 2.0 * 9.0 * 4.0, 0.01));
    CHECK(overlap(doc, s, peg) < 0.001);
    CHECK(socket::matchingName(f) == "Hexagon peg 3 \xC3\x97 4");

    // Turned 30°, a corner now points where a flat was: the peg no longer fits.
    f.turn = 30.0;
    const ObjectId other = add(stack, doc, BoxSpec{20, 20, 10});
    const ObjectId turned = addSocket(stack, doc, other, {{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}}, f);
    CHECK(overlap(doc, turned, peg) > 0.1);
}

TEST_CASE("Socket: round and square pegs; relief cuts", "[socket]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId block = add(stack, doc, BoxSpec{20, 20, 10});
    const double before = volumeOf(doc, block);
    SocketFinish f;
    f.kind = SocketKind::Peg;
    f.size = 2.0;
    f.depth = 5.0;
    f.fit = 0.2;
    const ObjectId round = addSocket(stack, doc, block, {{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}}, f);
    CHECK_THAT(before - volumeOf(doc, round), WithinRel(pi * 1.2 * 1.2 * 5.2, 0.01));
    CHECK(std::holds_alternative<CylinderSpec>(socket::matchingPart(f)));

    f.shape = PegShape::Square;
    const ObjectId square = addSocket(stack, doc, add(stack, doc, BoxSpec{20, 20, 10}), {{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}}, f);
    CHECK_THAT(before - volumeOf(doc, square), WithinRel(2.4 * 2.4 * 5.2, 0.01));
    CHECK(std::holds_alternative<BoxSpec>(socket::matchingPart(f)));

    // Four relief cuts, 0.4 wide, to half way down by default: more taken away.
    f.reliefCuts = 4;
    f.reliefWidth = 0.4;
    CHECK_THAT(socket::reliefDepth(f), WithinAbs(2.5, 1e-12));
    const ObjectId relieved = addSocket(stack, doc, add(stack, doc, BoxSpec{20, 20, 10}), {{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}}, f);
    CHECK(volumeOf(doc, relieved) < volumeOf(doc, square) - 1.0);
    CHECK(history::describe(doc, relieved) == "Square peg socket 2 \xC3\x97 5, 4 relief cuts");
}

TEST_CASE("Socket: in a moved and turned shape, on its side; kept in the file", "[socket]")
{
    Document doc;
    CommandStack stack(doc);
    Placement at;
    at.x = 40.0;
    at.y = -10.0;
    at.rotationZ = 30.0;
    const ObjectId block = add(stack, doc, BoxSpec{20, 20, 10}, at);
    const double before = volumeOf(doc, block);
    // The middle of the block's side facing +X (turned with it), in plate coordinates.
    const Transform t = doc.worldTransform(block);
    const Vec3 point = t.apply({10.0, 0.0, 5.0});
    const Vec3 normal = t.applyDirection({1.0, 0.0, 0.0});
    SocketFinish f;
    f.size = 4.0;
    f.fit = 0.15;
    f.resin = false;
    f.snap = false;
    f.reliefCuts = 3;
    const ObjectId s = addSocket(stack, doc, block, {point, normal}, f);
    // Open: a half ball below a cylinder to its middle, all of it inside the block.
    const double r = 2.15;
    const double hollow = 2.0 / 3.0 * pi * r * r * r + pi * r * r * 2.0;
    CHECK(before - volumeOf(doc, s) > hollow * 0.99);
    // Where it is, as it was given.
    const auto spot = socketSpot(doc, s);
    for (int k = 0; k < 3; ++k) {
        CHECK_THAT(spot.point[std::size_t(k)], WithinAbs(point[std::size_t(k)], 1e-9));
        CHECK_THAT(spot.normal[std::size_t(k)], WithinAbs(normal[std::size_t(k)], 1e-9));
    }

    // Saved and read back: the same.
    const auto bodies = file::fromJson(file::toJson(doc, "test"));
    Document reread;
    reread.replaceAll(bodies);
    REQUIRE(reread.find(s));
    CHECK(reread.find(s)->boolean->socket == doc.find(s)->boolean->socket);
    CHECK(reread.find(s)->boolean->op == BooleanOp::Socket);
    CHECK_THAT(volumeOf(reread, s), WithinRel(volumeOf(doc, s), 1e-6));

    // Changed: moved to the top, made a peg socket.
    SocketFinish g = f;
    g.kind = SocketKind::Peg;
    const Body changed = withSocket(doc, s, {t.apply({0.0, 0.0, 10.0}), {0.0, 0.0, 1.0}}, g);
    CHECK(changed.boolean->socket->kind == SocketKind::Peg);
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(s), changed, "Change socket"));
    CHECK_THAT(socketSpot(doc, s).normal[2], WithinAbs(1.0, 1e-9));
}

TEST_CASE("Socket: scaled evenly, it grows; the fit stays", "[socket]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId block = add(stack, doc, BoxSpec{20, 20, 10});
    SocketFinish f;
    f.size = 3.0;
    f.fit = 0.1;
    const ObjectId s = addSocket(stack, doc, block, {{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}}, f);
    const auto tree = scaledTree(doc, s, {2.0, 2.0, 2.0}, {0.0, 0.0, 0.0});
    const Body& top = tree.back();
    REQUIRE(top.boolean->socket);
    CHECK_THAT(top.boolean->socket->size, WithinAbs(6.0, 1e-12));
    CHECK_THAT(top.boolean->socket->fit, WithinAbs(0.1, 1e-12));
    CHECK_THAT(top.boolean->planePoint[2], WithinAbs(20.0, 1e-9));
}

TEST_CASE("Socket: in an imported mesh, at a clicked spot", "[socket]")
{
    Document doc;
    CommandStack stack(doc);
    auto data = std::make_shared<MeshData>();
    data->positions = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1};
    data->indices = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                     1, 2, 6, 1, 6, 5, 2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7};
    data->size = {1, 1, 1};
    MeshSpec m;
    m.data = data;
    m.width = 20;
    m.depth = 20;
    m.height = 10;
    const ObjectId part = add(stack, doc, m);
    const double before = volumeOf(doc, part);
    REQUIRE_THAT(before, WithinRel(4000.0, 1e-6));
    // Clicked from above, over the middle of the top.
    const Bounds box = doc.worldBounds(part);
    const auto c = box.center();
    const auto spot = surfaceUnder(doc, part, {c[0], c[1], 100.0}, {0.0, 0.0, -1.0}, 1.0);
    REQUIRE(spot);
    CHECK_THAT(spot->normal[2], WithinAbs(1.0, 1e-6));
    SocketFinish f;
    f.size = 3.0;
    f.fit = 0.1;
    const ObjectId s = addSocket(stack, doc, part, *spot, f);
    const auto seat = socket::ballSeat(f);
    const double cap = seat.radius - seat.centreDepth;
    const double hollow = 4.0 / 3.0 * pi * std::pow(seat.radius, 3) - pi * cap * cap * (3 * seat.radius - cap) / 3.0;
    CHECK_THAT(before - volumeOf(doc, s), WithinRel(hollow, 0.01));
}
