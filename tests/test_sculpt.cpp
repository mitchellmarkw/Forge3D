// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 17: the sculpting prototype.
#include "forge/core/Primitives.h"
#include "forge/core/Sculpt.h"
#include "forge/core/Tessellator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <cmath>
#include <map>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

double volume(const Mesh& m)
{
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &m.positions[3 * m.indices[t]];
        const float* b = &m.positions[3 * m.indices[t + 1]];
        const float* c = &m.positions[3 * m.indices[t + 2]];
        v += (a[0] * (double(b[1]) * c[2] - double(b[2]) * c[1]) - a[1] * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
              a[2] * (double(b[0]) * c[1] - double(b[1]) * c[0])) /
             6.0;
    }
    return v;
}

// Every edge used by exactly two triangles, once each way.
bool closed(const Mesh& m)
{
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
        for (int k = 0; k < 3; ++k)
            ++edges[{m.indices[t + k], m.indices[t + (k + 1) % 3]}];
    for (const auto& [e, n] : edges)
        if (n != 1 || edges.count({e.second, e.first}) != 1)
            return false;
    return true;
}

Mesh box(double size)
{
    return sculpt::welded(tessellate(primitives::buildShape(BoxSpec{size, size, size})), 1e-4);
}

}  // namespace

TEST_CASE("Sculpt: welding and refining keep the shape closed", "[sculpt]")
{
    const Mesh b = box(20);
    CHECK(b.vertexCount() == 8);
    CHECK(b.triangleCount() == 12);
    CHECK(closed(b));
    CHECK_THAT(volume(b), WithinRel(8000.0, 1e-6));

    const Mesh fine = sculpt::refined(b, 1.0, 10'000'000);
    CHECK(closed(fine));
    CHECK_THAT(volume(fine), WithinRel(8000.0, 1e-5));
    for (std::size_t t = 0; t + 2 < fine.indices.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const float* p = &fine.positions[3 * fine.indices[t + k]];
            const float* q = &fine.positions[3 * fine.indices[t + (k + 1) % 3]];
            CHECK(std::hypot(p[0] - q[0], p[1] - q[1], p[2] - q[2]) <= 1.0 + 1e-6);
        }
    CHECK(fine.triangleCount() > 4000);
    // Stops at the triangle limit.
    CHECK(sculpt::refined(b, 0.01, 50'000).triangleCount() < 400'000);
    // About the asked-for number from edgeForTriangles.
    const Mesh sized = sculpt::refined(b, sculpt::edgeForTriangles(b, 20'000), 10'000'000);
    CHECK(sized.triangleCount() > 10'000);
    CHECK(sized.triangleCount() < 80'000);
}

TEST_CASE("Sculpt: Draw pushes out, inverted in, Smooth evens out", "[sculpt]")
{
    sculpt::Sculptor s(sculpt::refined(box(20), 0.5, 10'000'000));
    const double before = volume(s.mesh());
    // The box is centred in X and Y, from 0 to 20 in Z: aim at the middle of its top.
    const auto hit = s.hit({0, 0, 50}, {0, 0, -1});
    REQUIRE(hit);
    CHECK_THAT(hit->point[2], WithinAbs(20.0, 1e-4));
    CHECK_THAT(hit->normal[2], WithinAbs(1.0, 1e-6));

    sculpt::Dab dab;
    dab.centre = hit->point;
    dab.radius = 4.0;
    dab.strength = 1.0;
    const auto range = s.apply(dab, hit->triangle);
    CHECK(range.first < range.second);
    const double raised = volume(s.mesh());
    CHECK(raised > before + 1.0);
    CHECK(closed(s.mesh()));
    const auto top = s.hit({0, 0, 50}, {0, 0, -1});
    REQUIRE(top);
    CHECK_THAT(top->point[2], WithinAbs(20.0 + 0.08 * 4.0, 0.02));  // the most one dab moves
    // Away from the brush, nothing moved.
    const auto far = s.hit({8, 8, 50}, {0, 0, -1});
    REQUIRE(far);
    CHECK_THAT(far->point[2], WithinAbs(20.0, 1e-5));

    // Smoothing the bump takes some of it back down.
    dab.brush = sculpt::Brush::Smooth;
    for (int i = 0; i < 10; ++i)
        s.apply(dab, s.hit({0, 0, 50}, {0, 0, -1})->triangle);
    CHECK(s.hit({0, 0, 50}, {0, 0, -1})->point[2] < top->point[2] - 0.05);

    // Inverted Draw pushes in.
    dab.brush = sculpt::Brush::Draw;
    dab.invert = true;
    const double smoothed = volume(s.mesh());
    s.apply(dab, s.hit({0, 0, 50}, {0, 0, -1})->triangle);
    CHECK(volume(s.mesh()) < smoothed - 1.0);

    // Undo puts the points back.
    std::vector<float> start = sculpt::refined(box(20), 0.5, 10'000'000).positions;
    s.setPositions(start);
    CHECK_THAT(volume(s.mesh()), WithinRel(before, 1e-9));
}

TEST_CASE("Sculpt: a dab on a dense mesh is quick", "[sculpt][slow]")
{
    // About 400 000 triangles, the size the prototype offers at High detail.
    const Mesh b = box(40);
    sculpt::Sculptor s(sculpt::refined(b, sculpt::edgeForTriangles(b, 400'000), 2'000'000));
    INFO("triangles " << s.mesh().triangleCount());
    const auto t0 = std::chrono::steady_clock::now();
    const auto hit = s.hit({0, 0, 100}, {0, 0, -1});
    const double hitMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    REQUIRE(hit);
    sculpt::Dab dab;
    dab.centre = hit->point;
    dab.radius = 5.0;
    const auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i)
        s.apply(dab, hit->triangle);
    const double dabMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count() / 20;
    WARN("triangles " << s.mesh().triangleCount() << ": finding the surface " << hitMs << " ms, a dab " << dabMs
                      << " ms");
    CHECK(dabMs < 40.0);  // a big brush: tens of thousands of points
    CHECK(hitMs < 2.0);   // through the grid
}

TEST_CASE("Sculpt: the other brushes", "[sculpt]")
{
    const Mesh start = sculpt::refined(box(20), 0.5, 10'000'000);
    const auto topAt = [](const sculpt::Sculptor& s, double x, double y) {
        return s.hit({x, y, 50}, {0, 0, -1})->point[2];
    };
    sculpt::Dab dab;
    dab.centre = {0, 0, 20};
    dab.radius = 4.0;
    dab.strength = 1.0;

    SECTION("Inflate swells out, inverted shrinks")
    {
        sculpt::Sculptor s(start);
        dab.brush = sculpt::Brush::Inflate;
        s.apply(dab, s.hit({0, 0, 50}, {0, 0, -1})->triangle);
        CHECK(volume(s.mesh()) > 8000.0 + 1.0);
        CHECK(closed(s.mesh()));
        sculpt::Sculptor t(start);
        dab.invert = true;
        t.apply(dab, t.hit({0, 0, 50}, {0, 0, -1})->triangle);
        CHECK(volume(t.mesh()) < 8000.0 - 1.0);
    }
    SECTION("Flatten takes a bump back down")
    {
        sculpt::Sculptor s(start);
        dab.radius = 2.0;
        s.apply(dab, s.hit({0, 0, 50}, {0, 0, -1})->triangle);  // a small bump
        const double bump = topAt(s, 0, 0);
        dab.radius = 6.0;
        dab.brush = sculpt::Brush::Flatten;
        for (int i = 0; i < 5; ++i)
            s.apply(dab, s.hit({0, 0, 50}, {0, 0, -1})->triangle);
        CHECK(topAt(s, 0, 0) < bump - 0.05);
    }
    SECTION("Clay builds a flat layer")
    {
        sculpt::Sculptor s(start);
        dab.brush = sculpt::Brush::Clay;
        for (int i = 0; i < 3; ++i)
            s.apply(dab, s.hit({0, 0, 50}, {0, 0, -1})->triangle);
        CHECK(topAt(s, 0, 0) > 20.1);
        // Flat-ish on top: the middle and a little way off about the same height.
        CHECK_THAT(topAt(s, 0.8, 0), WithinAbs(topAt(s, 0, 0), 0.05));
    }
    SECTION("Pinch draws points in towards the middle")
    {
        sculpt::Sculptor s(start);
        dab.brush = sculpt::Brush::Pinch;
        const std::uint32_t v = s.nearestPoint({2, 0, 20});
        const double before = std::abs(s.mesh().positions[3 * v]);
        s.apply(dab, s.hit({0, 0, 50}, {0, 0, -1})->triangle);
        CHECK(std::abs(s.mesh().positions[3 * v]) < before - 0.05);
        CHECK_THAT(s.mesh().positions[3 * v + 2], WithinAbs(20.0, 1e-4));  // along the surface
    }
    SECTION("Grab pulls the area with the mouse")
    {
        sculpt::Sculptor s(start);
        const auto hit = s.hit({0, 0, 50}, {0, 0, -1});
        s.beginGrab(hit->point, 4.0, hit->triangle, {});
        s.moveGrab({0, 0, 3});
        CHECK_THAT(topAt(s, 0, 0), WithinAbs(23.0, 0.05));
        s.moveGrab({0, 0, 1});  // from where it started, not added up
        CHECK_THAT(topAt(s, 0, 0), WithinAbs(21.0, 0.05));
        s.endGrab();
        CHECK(closed(s.mesh()));
    }
}

TEST_CASE("Sculpt: symmetry does the same on the other side", "[sculpt]")
{
    sculpt::Sculptor s(sculpt::refined(box(20), 0.5, 10'000'000));
    sculpt::Symmetry mirror;
    mirror.x = true;
    mirror.centre = {0, 0, 10};
    CHECK(mirror.images({3, 1, 20}).size() == 2);
    CHECK(mirror.images({3, 1, 20})[1] == Vec3{-3, 1, 20});
    mirror.y = true;
    CHECK(mirror.images({3, 1, 20}).size() == 4);
    CHECK(mirror.mirrored({1, 1, 1}, 3) == Vec3{-1, -1, 1});
    mirror.y = false;

    sculpt::Dab dab;
    dab.centre = {5, 2, 20};
    dab.radius = 2.0;
    dab.strength = 1.0;
    const auto hit = s.hit({5, 2, 50}, {0, 0, -1});
    s.apply(dab, hit->triangle, mirror);
    const double here = s.hit({5, 2, 50}, {0, 0, -1})->point[2];
    const double there = s.hit({-5, 2, 50}, {0, 0, -1})->point[2];
    CHECK(here > 20.1);
    CHECK_THAT(there, WithinAbs(here, 1e-4));
    CHECK_THAT(s.hit({5, -6, 50}, {0, 0, -1})->point[2], WithinAbs(20.0, 1e-5));  // not mirrored in Y

    // Grab, mirrored: pulling one side out to +X pulls the other out to -X.
    sculpt::Sculptor g(sculpt::refined(box(20), 0.5, 10'000'000));
    const auto side = g.hit({50, 0, 10}, {-1, 0, 0});
    REQUIRE(side);
    g.beginGrab(side->point, 3.0, side->triangle, mirror);
    g.moveGrab({2, 0, 0});
    CHECK_THAT(g.hit({50, 0, 10}, {-1, 0, 0})->point[0], WithinAbs(12.0, 0.05));
    CHECK_THAT(g.hit({-50, 0, 10}, {1, 0, 0})->point[0], WithinAbs(-12.0, 0.05));
}

TEST_CASE("Sculpt: detail is added where the brush goes", "[sculpt]")
{
    const Mesh coarse = sculpt::refined(box(20), 2.0, 10'000'000);
    sculpt::Sculptor s(coarse);
    const std::size_t before = s.mesh().triangleCount();
    const auto version = s.topologyVersion();
    s.setDetail(0.25);
    sculpt::Dab dab;
    dab.radius = 3.0;
    dab.strength = 1.0;
    const auto hit = s.hit({0, 0, 50}, {0, 0, -1});
    REQUIRE(hit);
    dab.centre = hit->point;
    const auto range = s.apply(dab, hit->triangle);
    CHECK(s.mesh().triangleCount() > before + 100);
    CHECK(s.topologyVersion() > version);
    CHECK(range.second == s.mesh().vertexCount());  // the new points are in the range
    CHECK(closed(s.mesh()));
    // Fine under the brush, untouched far away: every edge near the middle is short.
    for (std::size_t t = 0; t + 2 < s.mesh().indices.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const float* p = &s.mesh().positions[3 * s.mesh().indices[t + k]];
            const float* q = &s.mesh().positions[3 * s.mesh().indices[t + (k + 1) % 3]];
            if (std::hypot(p[0], p[1]) < 1.5 && std::hypot(q[0], q[1]) < 1.5 && p[2] > 19 && q[2] > 19)
                CHECK(std::hypot(p[0] - q[0], p[1] - q[1], p[2] - q[2]) <= 0.25 * 1.01 + 0.08 * 3.0);
        }
    const auto far = s.hit({8, 8, 50}, {0, 0, -1});
    REQUIRE(far);
    CHECK_THAT(far->point[2], WithinAbs(20.0, 1e-5));
    // The grid keeps up: the raised middle is found where it now is.
    CHECK(s.hit({0, 0, 50}, {0, 0, -1})->point[2] > 20.1);
    // Whole-mesh undo.
    s.setMesh(coarse);
    CHECK(s.mesh().triangleCount() == before);
    CHECK_THAT(volume(s.mesh()), WithinRel(8000.0, 1e-6));

    // With the grid, a hit agrees with testing every triangle.
    sculpt::Sculptor g(sculpt::refined(box(20), 0.7, 10'000'000));
    for (const Vec3& from : {Vec3{3, -50, 7}, Vec3{-50, 2.5, 13}, Vec3{40, 30, 35}}) {
        const Vec3 dir{-from[0], -from[1], 10 - from[2]};
        const auto h = g.hit(from, dir);
        REQUIRE(h);
        // On the box's surface: one coordinate at +-10 (or Z at 0 / 20).
        const bool onFace = std::abs(std::abs(h->point[0]) - 10) < 1e-4 || std::abs(std::abs(h->point[1]) - 10) < 1e-4 ||
                            std::abs(h->point[2]) < 1e-4 || std::abs(h->point[2] - 20) < 1e-4;
        CHECK(onFace);
    }
    CHECK_FALSE(g.hit({50, 50, 50}, {1, 0, 0}));  // pointing away
}
