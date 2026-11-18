// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 44: how fast sculpting is on big meshes (hidden: run with "[.bench]").
#include "forge/core/Primitives.h"
#include "forge/core/Sculpt.h"
#include "forge/core/Tessellator.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <iostream>

using namespace forge;

namespace {
double ms(std::chrono::steady_clock::time_point from)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
}
}  // namespace

TEST_CASE("Sculpt benchmark: a stroke on a big mesh", "[.bench]")
{
    SphereSpec ball;
    ball.radiusX = ball.radiusY = ball.radiusZ = 40.0;
    const Mesh coarse = sculpt::welded(tessellate(primitives::buildShape(ball), TessellationSettings{}), 1e-4);
    for (const std::size_t target : {std::size_t(1'000'000), std::size_t(1'500'000)}) {
        for (const bool detail : {false, true}) {
            for (const double radius : {3.0, 10.0}) {
                auto t0 = std::chrono::steady_clock::now();
                sculpt::Sculptor s(sculpt::refined(coarse, sculpt::edgeForTriangles(coarse, target), 4'000'000));
                const double build = ms(t0);
                if (detail)
                    s.setDetail(sculpt::edgeForTriangles(coarse, target) * 0.5);
                const std::size_t before = s.mesh().triangleCount();
                double hitMs = 0, dabMs = 0, worst = 0;
                std::size_t span = 0, grew = 0, runs = 0;
                s.takeChanges();
                std::uint64_t topo = s.topologyVersion();
                const int dabs = 120;
                for (int i = 0; i < dabs; ++i) {
                    // Across the front of the ball.
                    const double a = -0.8 + 1.6 * i / (dabs - 1);
                    const Vec3 origin{100 * std::sin(a), -100 * std::cos(a), 40.0 + 10 * std::sin(3 * a)};
                    const Vec3 dir{-std::sin(a), std::cos(a), 0.0};
                    t0 = std::chrono::steady_clock::now();
                    const auto hit = s.hit(origin, dir);
                    hitMs += ms(t0);
                    if (!hit)
                        continue;
                    sculpt::Dab dab;
                    dab.centre = hit->point;
                    dab.radius = radius;
                    dab.strength = 0.5;
                    t0 = std::chrono::steady_clock::now();
                    const auto [first, last] = s.apply(dab, hit->triangle);
                    const double one = ms(t0);
                    dabMs += one;
                    worst = std::max(worst, one);
                    (void)first;
                    (void)last;
                    const auto changes = s.takeChanges();
                    for (const auto& [a, b] : changes.points)
                        span += b - a;
                    runs += changes.points.size();
                    if (s.topologyVersion() != topo) {
                        ++grew;
                        topo = s.topologyVersion();
                    }
                }
                std::cerr << target / 1000 << "k triangles (built " << before / 1000 << "k in " << int(build)
                          << " ms), radius " << radius << (detail ? ", detail on" : ", detail off") << ": hit "
                          << hitMs / dabs << " ms, dab " << dabMs / dabs << " ms (worst " << worst
                          << "), points re-sent per dab " << span / dabs << " in " << runs / dabs << " runs, of "
                          << s.mesh().vertexCount()
                          << ", triangles added on " << grew << " dabs, now " << s.mesh().triangleCount() / 1000
                          << "k\n";
            }
        }
    }
}
