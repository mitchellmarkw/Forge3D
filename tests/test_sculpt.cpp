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
    CHECK(dabMs < 15.0);
    CHECK(hitMs < 60.0);
}
