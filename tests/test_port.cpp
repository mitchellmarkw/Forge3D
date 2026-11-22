// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 70: hand and accessory ports.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Port.h"
#include "forge/core/Scale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kPi = std::numbers::pi;

struct Bench {
    Document doc;
    CommandStack stack{doc};
    ObjectId block = kInvalidId;
    Bench()
    {
        Body b;
        b.id = doc.allocateId();
        b.spec = BoxSpec{20, 20, 10};
        b.name = "Arm";
        stack.execute(std::make_unique<AddBodyCommand>(b));
        block = b.id;
    }
    ObjectId add(const emboss::SurfacePoint& where, const PortFinish& f)
    {
        Body r;
        r.id = doc.allocateId();
        r.name = "Arm";
        r.boolean = portSpec(doc, block, where, f);
        stack.execute(std::make_unique<CombineCommand>(r));
        return r.id;
    }
    double volume(ObjectId id) const { return meshkernel::volume(doc.meshSolid(id)); }
};

const emboss::SurfacePoint kTop{{0.0, 0.0, 10.0}, {0.0, 0.0, 1.0}};

}  // namespace

TEST_CASE("Port: pegs stand out of the surface", "[port]")
{
    Bench bench;
    const double before = bench.volume(bench.block);
    PortFinish f;
    f.size = 3.0;
    f.length = 4.0;

    SECTION("flat end")
    {
        f.end = PegEnd::Flat;
        const ObjectId id = bench.add(kTop, f);
        CHECK_THAT(bench.volume(id) - before, WithinRel(kPi * 1.5 * 1.5 * 4.0, 0.01));
        CHECK_THAT(bench.doc.worldBounds(id).max[2], WithinAbs(14.0, 1e-3));
        CHECK(history::describe(bench.doc, id) == "Round peg \xC3\x98" "3 \xC3\x97 4");
    }
    SECTION("chamfered end: a little less, as tall")
    {
        const ObjectId id = bench.add(kTop, f);
        const double added = bench.volume(id) - before;
        CHECK(added < kPi * 1.5 * 1.5 * 4.0);
        CHECK(added > kPi * 1.5 * 1.5 * 3.6);
        CHECK_THAT(bench.doc.worldBounds(id).max[2], WithinAbs(14.0, 1e-3));
    }
    SECTION("hexagon, across its flats")
    {
        f.end = PegEnd::Flat;
        f.shape = PegShape::Hexagon;
        const ObjectId id = bench.add(kTop, f);
        CHECK_THAT(bench.volume(id) - before, WithinRel(std::sqrt(3.0) / 2.0 * 9.0 * 4.0, 0.001));
    }
    SECTION("a ball on the end, for a ball socket")
    {
        f.end = PegEnd::Ball;
        f.size = 2.5;
        f.ball = 4.0;
        const ObjectId id = bench.add(kTop, f);
        CHECK_THAT(bench.doc.worldBounds(id).max[2], WithinAbs(16.0, 0.01));
        CHECK(history::describe(bench.doc, id) == "Round peg \xC3\x98" "2.5 \xC3\x97 4, ball \xC3\x98" "4");
        const SocketFinish s = port::socketFor(f, 0.1, true);
        CHECK(s.kind == SocketKind::Ball);
        CHECK(s.size == 4.0);
    }
    SECTION("the hand's socket matches the peg")
    {
        f.shape = PegShape::Square;
        f.turn = 15.0;
        const SocketFinish s = port::socketFor(f, 0.2, false);
        CHECK(s.kind == SocketKind::Peg);
        CHECK(s.shape == PegShape::Square);
        CHECK(s.size == 3.0);
        CHECK(s.depth == 4.0);
        CHECK(s.fit == 0.2);
        CHECK(s.turn == 15.0);
        // And back from a standard's joint.
        const PortFinish back = port::fromSocket(s, PortFinish{});
        CHECK(back.shape == PegShape::Square);
        CHECK(back.size == 3.0);
        CHECK(back.length == 4.0);
    }
    f.size = 0.1;
    CHECK_THROWS_AS(port::check(f), std::invalid_argument);
}

TEST_CASE("Port: grip holes go through", "[port]")
{
    Bench bench;
    const double before = bench.volume(bench.block);
    PortFinish f;
    f.kind = PortKind::Grip;
    f.size = 3.0;
    f.fit = 0.1;

    SECTION("straight in, through the part")
    {
        const ObjectId id = bench.add(kTop, f);
        CHECK_THAT(before - bench.volume(id), WithinRel(kPi * 1.6 * 1.6 * 10.0, 0.01));
        CHECK(history::describe(bench.doc, id) == "Grip hole \xC3\x98" "3.2 for a \xC3\x98" "3 handle");
    }
    SECTION("aimed at a way out")
    {
        f.hasExit = true;
        f.exit = {10.0, 0.0, 5.0};
        const ObjectId id = bench.add({{-10.0, 0.0, 5.0}, {-1.0, 0.0, 0.0}}, f);
        CHECK_THAT(before - bench.volume(id), WithinRel(kPi * 1.6 * 1.6 * 20.0, 0.01));
    }
}

TEST_CASE("Port: changed, scaled and kept in the file", "[port]")
{
    Bench bench;
    PortFinish f;
    const ObjectId id = bench.add(kTop, f);
    PortFinish longer = portOf(bench.doc, id);
    longer.length = 6.0;
    bench.stack.execute(std::make_unique<EditBodiesCommand>(std::vector<Body>{*bench.doc.find(id)},
                                                            std::vector<Body>{withPort(bench.doc, id, longer)},
                                                            "Change port"));
    CHECK_THAT(bench.doc.worldBounds(id).max[2], WithinAbs(16.0, 1e-3));

    // Scaled up with the joints kept: the peg keeps its size, its spot moves.
    CHECK(jointCount(bench.doc, {id}) == 1);
    ScaleOptions keep;
    keep.keepJoints = true;
    for (const Body& b : scaledTree(bench.doc, id, {2.0, 2.0, 2.0}, {0.0, 0.0, 0.0}, keep))
        if (b.id == id) {
            CHECK(b.boolean->port->length == 6.0);
            CHECK(b.boolean->port->point[2] == 20.0);
        }

    const std::string json = file::toJson(bench.doc, "test");
    Document back;
    back.replaceAll(file::fromJson(json));
    REQUIRE(back.find(id));
    CHECK(*back.find(id)->boolean->port == portOf(bench.doc, id));
}
