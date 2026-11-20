// SPDX-License-Identifier: GPL-3.0-or-later
// Timings on a heavy mesh (about a million triangles, like a detailed sculpt). Hidden:
// run with  forge_tests "[.heavy]"
#include "forge/core/Hollow.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/PanelLines.h"
#include "forge/core/PrintCheck.h"
#include "forge/core/Stamp.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <map>
#include <numbers>

using namespace forge;

namespace {

// A bumpy ball of about 4^levels × 20 triangles, radius 40 mm, sitting on the plate.
Mesh heavyBall(int levels)
{
    const double t = (1 + std::sqrt(5.0)) / 2;
    std::vector<Vec3> v{{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                        {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    std::vector<std::array<std::uint32_t, 3>> f{{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                                                {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                                {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                                                {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};
    const auto norm = [](Vec3 p) {
        const double l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        return Vec3{p[0] / l, p[1] / l, p[2] / l};
    };
    for (auto& p : v)
        p = norm(p);
    for (int level = 0; level < levels; ++level) {
        std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> mid;
        const auto half = [&](std::uint32_t a, std::uint32_t b) {
            const auto key = std::minmax(a, b);
            const auto it = mid.find(key);
            if (it != mid.end())
                return it->second;
            v.push_back(norm({(v[a][0] + v[b][0]) / 2, (v[a][1] + v[b][1]) / 2, (v[a][2] + v[b][2]) / 2}));
            return mid[key] = std::uint32_t(v.size() - 1);
        };
        std::vector<std::array<std::uint32_t, 3>> next;
        for (const auto& [a, b, c] : f) {
            const auto ab = half(a, b), bc = half(b, c), ca = half(c, a);
            next.insert(next.end(), {{a, ab, ca}, {b, bc, ab}, {c, ca, bc}, {ab, bc, ca}});
        }
        f = std::move(next);
    }
    Mesh m;
    for (const auto& p : v) {
        const double bump = 1.0 + 0.02 * std::sin(13 * p[0]) * std::sin(11 * p[1]) * std::sin(7 * p[2]);
        m.positions.push_back(float(40 * bump * p[0]));
        m.positions.push_back(float(40 * bump * p[1]));
        m.positions.push_back(float(40 * bump * p[2] + 41));
    }
    for (const auto& tri : f)
        m.indices.insert(m.indices.end(), tri.begin(), tri.end());
    return m;
}

double since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

TEST_CASE("Heavy mesh timings", "[.heavy]")
{
    const Mesh ball = heavyBall(8);  // about 1.3 million triangles
    const MeshSolid solid = meshkernel::fromTriangles(ball.positions, ball.indices);
    WARN("triangles: " << ball.triangleCount());

    auto start = std::chrono::steady_clock::now();
    const auto thick = printcheck::faceThickness(ball);
    WARN("Print Check, measuring: " << since(start) << " s");
    start = std::chrono::steady_clock::now();
    Mesh shown;
    printcheck::classify(ball, thick, Transform::identity(), printcheck::Settings{}, &shown);
    WARN("Print Check, colouring: " << since(start) << " s");

    start = std::chrono::steady_clock::now();
    HollowFinish h;
    h.holes = {{{0, 0, 1}, {0, 0, -1}}};
    const MeshSolid hollowed = hollow::apply(solid, h);
    WARN("Hollow: " << since(start) << " s, " << meshkernel::triangleCount(hollowed) << " triangles");

    start = std::chrono::steady_clock::now();
    StampFinish st;
    for (int k = 0; k < 24; ++k) {
        const double a = 2 * std::numbers::pi * k / 24;
        st.spots.push_back({{40 * std::cos(a), 40 * std::sin(a), 41}, {std::cos(a), std::sin(a), 0}, {0, 0, 1}});
    }
    meshkernel::triangleCount(stamp::apply(solid, st));
    WARN("Stamp (24 rivets): " << since(start) << " s");

    start = std::chrono::steady_clock::now();
    PanelLineFinish pl;
    PanelLine line;
    for (int k = 0; k <= 300; ++k) {
        const double a = std::numbers::pi * k / 300;
        line.points.push_back({{40 * std::cos(a), 40 * std::sin(a), 41}, {std::cos(a), std::sin(a), 0}});
    }
    pl.lines = {line};
    meshkernel::triangleCount(panel::apply(solid, pl));
    WARN("Panel Lines (300 points): " << since(start) << " s");

    start = std::chrono::steady_clock::now();
    meshkernel::displayMesh(solid);
    WARN("Making the shown mesh: " << since(start) << " s");
}
