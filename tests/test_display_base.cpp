// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 76: display bases
#include "forge/core/Balance.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/DisplayBase.h"
#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <stdexcept>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

struct Scene {
    Document doc;
    CommandStack stack{doc};
    ObjectId box(double w, double d, double h, double x, double y, double z)
    {
        Body b;
        b.id = doc.allocateId();
        b.spec = BoxSpec{w, d, h};
        b.name = "Leg";
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

Bounds boundsOf(const Mesh& m)
{
    Bounds b;
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
        b.expand(m.positions[i], m.positions[i + 1], m.positions[i + 2]);
    return b;
}

double volumeOf(const Mesh& m)
{
    return meshkernel::volume(meshkernel::fromTriangles(m.positions, m.indices));
}

}  // namespace

TEST_CASE("Display base: round, under two feet, with a peg under each", "[displaybase]")
{
    Scene s;
    s.box(6, 6, 20, -10, 0, 0);
    s.box(6, 6, 20, 10, 0, 0);
    const auto b = balance::of(s.parts());
    const auto spots = footSpots(b);
    REQUIRE(spots.size() == 2);
    displaybase::Settings set;
    set.bevel = 0.0;
    const Mesh m = displaybase::make(set, b, spots);
    REQUIRE_FALSE(m.indices.empty());
    const Bounds bb = boundsOf(m);
    CHECK_THAT(bb.max[2], WithinAbs(set.pegHeight, 1e-3));    // the pegs' tops
    CHECK_THAT(bb.min[2], WithinAbs(-set.thickness, 1e-3));   // its top at the ground
    // Reaches past the feet's far corners by the margin.
    const double r = std::hypot(13.0, 3.0) + set.margin;
    CHECK_THAT(bb.max[0], WithinAbs(r, 0.05));
    CHECK_THAT(bb.min[0], WithinAbs(-r, 0.05));
    const double plate = 3.14159265 * r * r * set.thickness;
    const double pegs = 2 * 3.14159265 * 1.5 * 1.5 * set.pegHeight;
    CHECK_THAT(volumeOf(m), WithinAbs(plate + pegs, (plate + pegs) * 0.01));
}

TEST_CASE("Display base: holes instead of pegs, and the other shapes", "[displaybase]")
{
    Scene s;
    s.box(6, 6, 20, -10, 0, 0);
    s.box(6, 6, 20, 10, 0, 0);
    const auto b = balance::of(s.parts());
    displaybase::Settings set;
    set.shape = displaybase::Shape::Square;
    set.bevel = 0.0;
    const Mesh plain = displaybase::make(set, b, {});
    const Bounds pb = boundsOf(plain);
    CHECK_THAT(pb.max[0], WithinAbs(13.0 + set.margin, 1e-3));
    CHECK_THAT(pb.max[1], WithinAbs(3.0 + set.margin, 1e-3));
    set.pegs = false;
    set.pegHeight = 3.0;
    const Mesh holed = displaybase::make(set, b, footSpots(b));
    CHECK_THAT(boundsOf(holed).max[2], WithinAbs(0.0, 1e-3));
    const double hole = 3.14159265 * 1.6 * 1.6 * 3.0;
    CHECK_THAT(volumeOf(plain) - volumeOf(holed), WithinAbs(2 * hole, 2 * hole * 0.02));

    set.pegs = true;
    for (auto shape : {displaybase::Shape::Oval, displaybase::Shape::Hexagon}) {
        set.shape = shape;
        set.bevel = 1.0;
        const Mesh m = displaybase::make(set, b, footSpots(b));
        CHECK(volumeOf(m) > 0.0);
        const Bounds bb = boundsOf(m);
        CHECK(bb.max[0] >= 13.0 + set.margin - 1e-3);
    }
}

TEST_CASE("Display base: bad settings are refused", "[displaybase]")
{
    displaybase::Settings set;
    set.thickness = 0.5;
    CHECK_THROWS_AS(displaybase::check(set), std::invalid_argument);
    set = {};
    set.bevel = set.thickness;
    CHECK_THROWS_AS(displaybase::check(set), std::invalid_argument);
    set = {};
    set.pegs = false;
    set.pegHeight = set.thickness;
    CHECK_THROWS_AS(displaybase::check(set), std::invalid_argument);
}

TEST_CASE("Display base: added in one undo step, with holes in the feet", "[displaybase]")
{
    Scene s;
    const ObjectId left = s.box(6, 6, 20, -10, 0, 0);
    s.box(6, 6, 20, 10, 0, 0);
    const auto before = s.doc.topLevelIds().size();
    SocketFinish hole;
    hole.kind = SocketKind::Peg;
    hole.size = 3.0;
    hole.depth = 4.0;
    const ObjectId base = addDisplayBase(s.stack, s.doc, s.parts(), {}, hole);
    REQUIRE(s.doc.find(base));
    CHECK(s.doc.find(base)->name == "Display base");
    CHECK(s.doc.topLevelIds().size() == before + 1);
    // Each foot now has a hole (its top-level is a Socket step).
    int sockets = 0;
    for (ObjectId id : s.doc.topLevelIds()) {
        const Body* b = s.doc.find(id);
        if (b->isBoolean() && b->boolean->op == BooleanOp::Socket)
            ++sockets;
    }
    CHECK(sockets == 2);
    (void)left;
    // Doing it again doesn't make the holes twice.
    const ObjectId again = addDisplayBase(s.stack, s.doc, s.parts(), {}, hole);
    CHECK(again != base);
    sockets = 0;
    for (ObjectId id : s.doc.topLevelIds()) {
        const Body* b = s.doc.find(id);
        if (b->isBoolean() && b->boolean->op == BooleanOp::Socket)
            ++sockets;
    }
    CHECK(sockets == 2);
    s.stack.undo();
    s.stack.undo();
    CHECK(s.doc.topLevelIds().size() == before);
}

TEST_CASE("Display base: a one-piece figure gets a hole in each foot", "[displaybase]")
{
    Scene s;
    s.box(6, 6, 20, -10, 0, 0);
    s.box(6, 6, 20, 10, 0, 0);
    s.box(30, 6, 6, 0, 0, 20);
    // Merge them into one piece.
    Body merged;
    merged.id = s.doc.allocateId();
    merged.name = "Figure";
    BooleanSpec spec;
    spec.op = BooleanOp::Merge;
    spec.operands = s.doc.topLevelIds();
    merged.boolean = spec;
    s.stack.execute(std::make_unique<CombineCommand>(merged));
    REQUIRE(s.doc.topLevelIds().size() == 1);
    const auto parts = s.parts();
    const auto b = balance::of(parts);
    const auto spots = balance::pegSpots(parts, b);
    REQUIRE(spots.size() == 2);
    SocketFinish peg;
    peg.kind = SocketKind::Peg;
    s.stack.beginMacro("holes");
    CHECK(addFootHoles(s.stack, s.doc, spots, peg) == 2);
    s.stack.endMacro();
    REQUIRE(s.doc.topLevelIds().size() == 1);
    const double before = meshkernel::volume(s.doc.meshSolid(merged.id));
    const double after = meshkernel::volume(s.doc.meshSolid(s.doc.topLevelIds().front()));
    CHECK(before - after > 2 * 3.14159 * 1.6 * 1.6 * 4.0 * 0.9);
    // Again: both are there already.
    s.stack.beginMacro("again");
    CHECK(addFootHoles(s.stack, s.doc, balance::pegSpots(s.parts(), balance::of(s.parts())), peg) == 0);
    s.stack.endMacro();
}
