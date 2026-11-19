// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 52: tolerance test print.
#include "forge/core/MeshKernel.h"
#include "forge/core/Tolerance.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

MeshSolid solidOf(const Mesh& m) { return meshkernel::fromTriangles(m.positions, m.indices); }

}  // namespace

TEST_CASE("Tolerance: the fits to try, and their labels", "[tolerance]")
{
    const auto f = tolerance::steps(0.10, 0.30, 0.05);
    REQUIRE(f.size() == 5);
    CHECK_THAT(f[0], WithinAbs(0.10, 1e-12));
    CHECK_THAT(f[4], WithinAbs(0.30, 1e-12));
    CHECK(tolerance::steps(0.05, 0.05, 0.05).size() == 1);
    CHECK_THROWS(tolerance::steps(0.3, 0.1, 0.05));
    CHECK_THROWS(tolerance::steps(0.0, 1.0, 0.05));  // 21 fits: too many
    CHECK(tolerance::label(0.15) == ".15");
    CHECK(tolerance::label(1.05) == "1.05");
    // Raised digits, standing on z = 0.
    const Mesh d = tolerance::digits(".15", 3.0, 0.6);
    const Bounds b = meshkernel::bounds(solidOf(d));
    CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-5));
    CHECK_THAT(b.max[2], WithinAbs(0.6, 1e-5));
    CHECK_THAT(b.size()[1], WithinAbs(3.0, 1e-5));
}

TEST_CASE("Tolerance: a bar per kind and the pieces to try in them", "[tolerance]")
{
    tolerance::Settings s;
    const auto pieces = tolerance::pieces(s);
    // Three bars, two pegs, one ball, two pins.
    REQUIRE(pieces.size() == 8);
    CHECK(pieces[0].name == "Peg fit test .10\xE2\x80\x93.30");
    CHECK(pieces[1].name == "Ball fit test .10\xE2\x80\x93.30");
    CHECK(pieces[2].name == "Pin fit test .10\xE2\x80\x93.30");
    CHECK(pieces[3].name == "Test peg \xC3\x98" "3.00");
    CHECK(pieces[5].name == "Test ball \xC3\x98" "6.00");
    for (const auto& p : pieces) {
        const MeshSolid m = solidOf(p.mesh);
        INFO(p.name);
        CHECK(meshkernel::volume(m) > 1.0);
        CHECK_THAT(meshkernel::bounds(m).min[2], WithinAbs(0.0, 1e-4));  // standing on the plate
    }
    // The bars and the pieces don't overlap on the plate.
    for (std::size_t i = 0; i < pieces.size(); ++i)
        for (std::size_t k = i + 1; k < pieces.size(); ++k)
            CHECK(meshkernel::volume(meshkernel::intersected({solidOf(pieces[i].mesh), solidOf(pieces[k].mesh)})) < 1e-6);

    tolerance::Settings one;
    one.balls = one.pins = false;
    one.fits = {0.2};
    CHECK(tolerance::pieces(one).size() == 3);
    one.pegs = false;
    CHECK_THROWS(tolerance::pieces(one));
}

TEST_CASE("Tolerance: a test peg fits each hole with the room it's labelled with", "[tolerance]")
{
    tolerance::Settings s;
    s.balls = s.pins = false;
    const auto pieces = tolerance::pieces(s);
    const MeshSolid bar = solidOf(pieces[0].mesh);
    const Mesh& peg = pieces[1].mesh;
    const Bounds pb = meshkernel::bounds(solidOf(peg));
    const Bounds bb = meshkernel::bounds(bar);
    // The holes are along the back of the bar, evenly spaced: find each by the peg
    // dropped straight down into it.
    const double length = bb.size()[0];
    const double pitch = (length - 4.0) / double(s.fits.size());
    const double holeY = bb.max[1] - 2.0 - (s.pegSize + 2.0 * 0.30) / 2.0;
    for (std::size_t i = 0; i < s.fits.size(); ++i) {
        const double x = bb.min[0] + 2.0 + pitch * (double(i) + 0.5);
        const Vec3 to{x - (pb.min[0] + pb.max[0]) / 2.0, holeY - (pb.min[1] + pb.max[1]) / 2.0,
                      bb.max[2] - s.pegDepth};
        const MeshSolid in = meshkernel::transformed(solidOf(peg), Transform::translation(to));
        INFO("fit " << s.fits[i]);
        CHECK(meshkernel::volume(meshkernel::intersected({bar, in})) < 1e-4);
        // A peg a fit wider than it would hit.
        const double grow = (s.pegSize + 2.0 * s.fits[i] + 0.1) / s.pegSize;
        Transform wider;
        wider.m[0][0] = wider.m[1][1] = grow;
        const Vec3 c{x, holeY, 0};
        const MeshSolid fat = meshkernel::transformed(
            meshkernel::transformed(in, Transform::translation({-c[0], -c[1], 0})), wider);
        const MeshSolid back = meshkernel::transformed(fat, Transform::translation(c));
        CHECK(meshkernel::volume(meshkernel::intersected({bar, back})) > 1e-3);
    }
}
