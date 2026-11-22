// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 17: the sculpting prototype.
#include "forge/core/Primitives.h"
#include "forge/core/Sculpt.h"
#include "forge/core/Tessellator.h"

#include <fstream>
#include <cstdlib>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
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
    const std::vector<float> start = s.mesh().positions;  // in the sculptor's own order
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

TEST_CASE("Sculpt: Crease, Snake hook and the mask", "[sculpt][mask]")
{
    const Mesh start = sculpt::refined(box(20), 0.5, 10'000'000);
    const auto topAt = [](const sculpt::Sculptor& s, double x, double y) {
        return s.hit({x, y, 50}, {0, 0, -1})->point[2];
    };
    const auto under = [](const sculpt::Sculptor& s, double x, double y) {
        return s.hit({x, y, 50}, {0, 0, -1})->triangle;
    };
    sculpt::Dab dab;
    dab.radius = 4.0;
    dab.strength = 1.0;

    SECTION("Crease cuts a narrow groove; inverted, a ridge")
    {
        sculpt::Sculptor s(start);
        dab.brush = sculpt::Brush::Crease;
        for (double x = -5; x <= 5; x += 0.8) {
            dab.centre = {x, 0, 20};
            s.apply(dab, under(s, x, 0));
        }
        CHECK(topAt(s, 0, 0) < 20.0 - 0.2);
        CHECK(topAt(s, 0, 3.0) > topAt(s, 0, 0) + 0.15);  // narrow: shallower just off it
        CHECK(closed(s.mesh()));
        sculpt::Sculptor t(start);
        dab.invert = true;
        dab.centre = {0, 0, 20};
        t.apply(dab, under(t, 0, 0));
        CHECK(topAt(t, 0, 0) > 20.0 + 0.1);
    }

    SECTION("Snake hook pulls the surface out after the mouse, adding detail")
    {
        sculpt::Sculptor s(start);
        s.setDetail(0.3);
        const std::size_t before = s.mesh().triangleCount();
        s.beginSnake({0, 0, 20}, 3.0, 1.0, under(s, 0, 0), {});
        for (double z = 21; z <= 32; z += 1.0)
            s.moveSnake({0.0, 0.0, z});
        s.endSnake();
        CHECK(s.mesh().bounds().max[2] > 31.0);  // a horn about 12 long
        CHECK_THAT(topAt(s, 8, 8), WithinAbs(20.0, 1e-4));  // the rest untouched
        CHECK(s.mesh().triangleCount() > before);
        CHECK(closed(s.mesh()));

        // Mirrored across X: two horns.
        sculpt::Sculptor m(start);
        sculpt::Symmetry sym;
        sym.x = true;
        m.beginSnake({5, 0, 20}, 3.0, 1.0, under(m, 5, 0), sym);
        for (double z = 21; z <= 30; z += 1.0)
            m.moveSnake({5.0, 0.0, z});
        m.endSnake();
        CHECK(topAt(m, 5, 0) > 28.0);
        CHECK(topAt(m, -5, 0) > 28.0);
        CHECK(closed(m.mesh()));
    }

    SECTION("Masked areas don't move; inverted, the rest doesn't; cleared, all do")
    {
        sculpt::Sculptor s(start);
        dab.brush = sculpt::Brush::Mask;
        dab.radius = 4.0;
        for (double x = -9; x <= 0; x += 1.0)
            for (double y = -9; y <= 9; y += 1.0) {
                dab.centre = {x, y, 20};
                s.apply(dab, under(s, x, y));
            }
        REQUIRE(s.mesh().shade.size() == s.mesh().vertexCount());
        const auto drawAcross = [&](sculpt::Sculptor& t) {
            sculpt::Dab d;
            d.brush = sculpt::Brush::Draw;
            d.radius = 3.0;
            d.strength = 1.0;
            for (double x = -6; x <= 6; x += 0.6) {
                d.centre = {x, 0, 20};
                t.apply(d, t.triangleAt(d.centre));
            }
        };
        drawAcross(s);
        CHECK_THAT(topAt(s, -6, 0), WithinAbs(20.0, 1e-4));  // masked
        CHECK(topAt(s, 6, 0) > 20.2);                                  // free
        // Detail added under a mask keeps it masked.
        sculpt::Sculptor d(start);
        d.setDetail(0.1);
        sculpt::Dab paint = dab;
        paint.centre = {-6, 0, 20};
        d.apply(paint, under(d, -6, 0));
        drawAcross(d);
        CHECK(d.mesh().shade.size() == d.mesh().vertexCount());
        CHECK_THAT(topAt(d, -6, 0), WithinAbs(20.0, 1e-3));

        const double right = topAt(s, 6, 0);
        s.invertMask();
        drawAcross(s);
        CHECK(topAt(s, -6, 0) > 20.2);                       // now free
        CHECK_THAT(topAt(s, 6, 0), WithinAbs(right, 1e-4));  // now masked: as it was
        s.clearMask();
        CHECK(s.mesh().shade.empty());
        CHECK(closed(s.mesh()));
    }

    SECTION("Recorded mask, crease and snake strokes play back")
    {
        SculptFinish finish;
        SculptStroke mask;
        mask.brush = int(sculpt::Brush::Mask);
        mask.radius = 5;
        mask.strength = 1;
        mask.dabs = {{-6.f, 0.f, 20.f}};
        SculptStroke invert;
        invert.brush = int(sculpt::Brush::MaskInvert);
        SculptStroke clear;
        clear.brush = int(sculpt::Brush::MaskClear);
        SculptStroke crease;
        crease.brush = int(sculpt::Brush::Crease);
        crease.radius = 3;
        crease.strength = 1;
        crease.dabs = {{-6.f, 0.f, 20.f}, {6.f, 0.f, 20.f}};
        SculptStroke snake;
        snake.brush = int(sculpt::Brush::SnakeHook);
        snake.radius = 3;
        snake.strength = 1;
        snake.finest = 0.3;
        snake.dabs = {{0.f, 6.f, 20.f}, {0.f, 6.f, 22.f}, {0.f, 6.f, 24.f}};
        finish.strokes = {mask, invert, crease, clear, snake};
        const Mesh out = sculpt::replayed(start, finish);
        sculpt::Sculptor s(out);
        CHECK(topAt(s, -6, 0) < 20.0 - 0.1);                           // inside the inverted mask's hole
        CHECK_THAT(topAt(s, 6, 0), WithinAbs(20.0, 1e-4));     // masked after inverting
        CHECK(topAt(s, 0, 6) > 23.0);                                  // the horn
        CHECK(out.shade.empty());
        CHECK(closed(out));
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

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Scale.h"

namespace {
// A stroke of Draw across the top of a box 20 high, as the app records it.
SculptStroke drawAcrossTop(int brush = 0)
{
    SculptStroke st;
    st.brush = brush;
    st.radius = 3.0;
    st.strength = 1.0;
    st.finest = 0.3;
    for (double x = -4.0; x <= 4.0; x += 0.6)
        st.dabs.push_back({float(x), 0.f, 20.f});
    return st;
}
}  // namespace

TEST_CASE("Sculpt steps: strokes play back the same, and follow a changed shape", "[sculpt][sculptstep]")
{
    Document doc;
    CommandStack stack(doc);
    Body box;
    box.id = doc.allocateId();
    box.name = "Box";
    box.spec = BoxSpec{20, 20, 20};
    stack.execute(std::make_unique<AddBodyCommand>(box));

    SculptFinish finish;
    finish.detail = 0;
    finish.strokes.push_back(drawAcrossTop());
    SculptStroke mirrored = drawAcrossTop(int(sculpt::Brush::Inflate));
    mirrored.dabs = {{6.f, 6.f, 20.f}};
    mirrored.mirrorX = true;
    finish.strokes.push_back(mirrored);

    // Played live (as the app does it) and replayed: the same surface.
    const Mesh start = doc.sculptStart(box.id, finish.detail);
    sculpt::Sculptor live(start);
    for (const SculptStroke& st : finish.strokes)
        sculpt::replay(live, st, start);
    const Mesh again = sculpt::replayed(start, finish);
    CHECK(again.indices == live.mesh().indices);
    CHECK(again.positions == live.mesh().positions);
    CHECK(again.triangleCount() > start.triangleCount());  // detail was added
    CHECK(closed(again));

    // As a step.
    Body step;
    step.id = doc.allocateId();
    step.name = "Sculpt";
    BooleanSpec spec;
    spec.op = BooleanOp::Sculpt;
    spec.operands = {box.id};
    spec.sculpt = finish;
    step.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(step));
    const double sculpted = meshkernel::volume(doc.meshSolid(step.id));
    CHECK(sculpted > 8000.0 + 5.0);
    CHECK(doc.worldBounds(step.id).max[2] > 20.1);
    CHECK(history::describe(doc, step.id).find("2 strokes") != std::string::npos);

    // Make the box longer: the ridge is played again on it, where it was.
    Body longer = *doc.find(box.id);
    longer.spec = BoxSpec{40, 20, 20};
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box.id), longer, "Resize"));
    const double after = meshkernel::volume(doc.meshSolid(step.id));
    CHECK(after > 16000.0 + 5.0);
    CHECK(doc.worldBounds(step.id).max[2] > 20.1);
    CHECK_THAT(doc.worldBounds(step.id).size()[0], WithinAbs(40.0, 0.5));

    // Saved and opened: the same strokes, the same shape.
    const auto loaded = file::fromJson(file::toJson(doc, "test"));
    Document other;
    other.replaceAll(loaded);
    CHECK(*other.find(step.id) == *doc.find(step.id));
    CHECK_THAT(meshkernel::volume(other.meshSolid(step.id)), WithinRel(after, 1e-9));

    // Resized evenly, the strokes and brushes grow with it.
    const auto scaled = scaledTree(doc, step.id, {2, 2, 2}, {0, 0, 0});
    for (const Body& b : scaled)
        if (b.id == step.id) {
            CHECK_THAT(b.boolean->sculpt->strokes[0].radius, WithinRel(6.0, 1e-9));
            CHECK_THAT(b.boolean->sculpt->strokes[0].dabs[0][0], WithinRel(-8.0, 1e-6));
        }
}
TEST_CASE("Sculpt steps: the strokes stay on the part when it moves or turns", "[sculpt][sculptstep]")
{
    Document doc;
    CommandStack stack(doc);
    Body box;
    box.id = doc.allocateId();
    box.name = "Box";
    box.spec = BoxSpec{20, 20, 20};
    stack.execute(std::make_unique<AddBodyCommand>(box));
    // An ear near a top corner, mirrored across X: one at each top corner on that side.
    SculptFinish finish;
    finish.detail = 0;
    SculptStroke ear;
    ear.brush = int(sculpt::Brush::Draw);
    ear.radius = 3.0;
    ear.strength = 1.0;
    ear.mirrorX = true;
    ear.dabs = {{7.f, 7.f, 20.f}};
    finish.strokes.push_back(ear);
    Body step;
    step.id = doc.allocateId();
    step.name = "Sculpt";
    BooleanSpec spec;
    spec.op = BooleanOp::Sculpt;
    spec.operands = {box.id};
    spec.sculpt = finish;
    step.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(step));

    // Where the ears are on the plate: the points well above the box's top, the box's
    // top found from where it is now.
    const auto ears = [&] {
        const Mesh& m = *doc.mesh(step.id);
        const Transform t = doc.worldTransform(step.id);
        Bounds b;
        for (std::size_t i = 0; i < m.positions.size(); i += 3) {
            const Vec3 w = t.apply(Vec3{m.positions[i], m.positions[i + 1], m.positions[i + 2]});
            if (w[2] > doc.worldBounds(box.id).max[2] + 0.12)
                b.expand(float(w[0]), float(w[1]), float(w[2]));
        }
        return b;
    };
    const Bounds before = ears();
    REQUIRE(before.valid);
    CHECK_THAT(before.min[0], WithinAbs(-8.4, 0.6));  // both corners
    CHECK_THAT(before.max[0], WithinAbs(8.4, 0.6));

    // Move the box inside the step: the ears go with it.
    Body moved = *doc.find(box.id);
    moved.placement.x += 15.0;
    moved.placement.y -= 5.0;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box.id), moved, "Move"));
    const Bounds after = ears();
    REQUIRE(after.valid);
    CHECK_THAT(after.min[0], WithinAbs(before.min[0] + 15.0, 0.05));
    CHECK_THAT(after.max[0], WithinAbs(before.max[0] + 15.0, 0.05));
    CHECK_THAT(after.max[1], WithinAbs(before.max[1] - 5.0, 0.05));

    // Turn it a quarter round Z: the ears are now at the ends across Y.
    Body turned = *doc.find(box.id);
    turned.placement.rotationZ = 90.0;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box.id), turned, "Turn"));
    const Bounds round = ears();
    const auto c = doc.worldBounds(box.id).center();
    CHECK_THAT(round.max[1] - round.min[1], WithinAbs(before.max[0] - before.min[0], 0.1));
    CHECK_THAT((round.max[1] + round.min[1]) / 2, WithinAbs(c[1], 0.1));

    // Resized evenly as a whole (box turned inside): the ears stay at the corners (a
    // little wider: the bigger brush raises more above the line measured from).
    stack.execute(std::make_unique<EditBodiesCommand>(
        [&] {
            std::vector<Body> was;
            for (const Body& b : scaledTree(doc, step.id, {2, 2, 2}, {0, 0, 0}))
                was.push_back(*doc.find(b.id));
            return was;
        }(),
        scaledTree(doc, step.id, {2, 2, 2}, {0, 0, 0}), "Scale"));
    const Bounds big = ears();
    const auto c2 = doc.worldBounds(box.id).center();
    CHECK_THAT(big.max[1] - big.min[1], WithinAbs(2 * (round.max[1] - round.min[1]), 2.0));
    CHECK_THAT((big.max[1] + big.min[1]) / 2, WithinAbs(c2[1], 0.2));
}

TEST_CASE("Sculpt steps: strokes stay on a part that grows or shrinks", "[sculpt][sculptstep]")
{
    Document doc;
    CommandStack stack(doc);
    Body ball;
    ball.id = doc.allocateId();
    ball.name = "Ball";
    ball.spec = SphereSpec{10};
    stack.execute(std::make_unique<AddBodyCommand>(ball));
    const double top = doc.worldBounds(ball.id).max[2];
    // A bump on top, a horn out of the side, and a remesh between them.
    SculptFinish finish;
    finish.detail = 0;
    SculptStroke bump;
    bump.brush = int(sculpt::Brush::Draw);
    bump.radius = 3;
    bump.strength = 1;
    for (int i = 0; i < 4; ++i)
        bump.dabs.push_back({0.f, 0.f, float(top)});
    SculptStroke even;
    even.brush = int(sculpt::Brush::Remesh);
    even.radius = 0.8;
    SculptStroke horn;
    horn.brush = int(sculpt::Brush::SnakeHook);
    horn.radius = 3;
    horn.strength = 1;
    horn.finest = 0.5;
    const auto mid = doc.worldBounds(ball.id).center();
    for (int i = 0; i <= 8; ++i)
        horn.dabs.push_back({float(mid[0] + 10 + i), float(mid[1]), float(mid[2])});
    finish.strokes = {bump, even, horn};
    Body step;
    step.id = doc.allocateId();
    step.name = "Sculpt";
    BooleanSpec spec;
    spec.op = BooleanOp::Sculpt;
    spec.operands = {ball.id};
    spec.sculpt = finish;
    step.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(step));
    const auto check = [&](double radius) {
        const Bounds ballBox = doc.worldBounds(ball.id);
        const Bounds b = doc.worldBounds(step.id);
        INFO("radius " << radius);
        CHECK(b.max[2] > ballBox.max[2] + 0.4);   // the bump, on the new top
        CHECK(b.max[0] > ballBox.max[0] + 6.0);   // the horn, out of the new side
    };
    check(10);
    for (double r : {14.0, 7.0}) {
        Body resized = *doc.find(ball.id);
        resized.spec = SphereSpec{r};
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(ball.id), resized, "Resize"));
        check(r);
    }
}

TEST_CASE("Sculpt: the Pattern brush", "[sculpt][pattern]")
{
    SECTION("The built-in patterns repeat seamlessly")
    {
        for (const std::string& name : sculpt::builtInPatterns()) {
            SculptPattern pat;
            pat.name = name;
            const auto image = sculpt::patternImage(pat);
            REQUIRE(image);
            INFO(name);
            CHECK(image->width >= 128);
            CHECK(image->grey.size() == std::size_t(image->width) * image->height);
            // Across the seam it changes no more than from one column to the next inside.
            double seam = 0.0, inside = 0.0;
            for (int y = 0; y < image->height; ++y) {
                const auto g = [&](int x) { return double(image->grey[std::size_t(y) * image->width + x]); };
                seam += std::abs(g(image->width - 1) - g(0));
                inside += std::abs(g(image->width / 2) - g(image->width / 2 - 1));
            }
            CHECK(seam < inside * 3.0 + 4.0 * image->height);
            const auto min = *std::min_element(image->grey.begin(), image->grey.end());
            const auto max = *std::max_element(image->grey.begin(), image->grey.end());
            CHECK(max - min > 150);  // a real pattern, not flat
        }
        SculptPattern unknown;
        unknown.name = "nonsense";
        CHECK_THROWS(sculpt::patternImage(unknown));
    }

    SECTION("Pressed in up to its depth; going over again doesn't press deeper; Ctrl engraves")
    {
        const Mesh start = sculpt::refined(box(20), 0.25, 10'000'000);
        SculptPattern pat;
        pat.name = "bricks";
        const double tile = 4.0, depth = 0.6;
        sculpt::Sculptor s(start);
        s.beginPattern(sculpt::patternImage(pat), tile, depth);
        sculpt::Dab dab;
        dab.brush = sculpt::Brush::Pattern;
        dab.radius = 4.0;
        dab.strength = 1.0;
        for (int pass = 0; pass < 3; ++pass)
            for (double x = -5; x <= 5; x += 0.8) {
                dab.centre = {x, 0, 20};
                s.apply(dab, s.triangleAt(dab.centre));
            }
        s.endPattern();
        double top = 0, high = 0;
        int raised = 0, low = 0;
        for (std::size_t v = 0; v < s.mesh().vertexCount(); ++v) {
            const float z = s.mesh().positions[3 * v + 2];
            if (std::abs(s.mesh().positions[3 * v]) < 3 && std::abs(s.mesh().positions[3 * v + 1]) < 1.5 &&
                z > 19.9) {
                top = std::max(top, double(z));
                (z > 20.0 + depth * 0.7 ? raised : low)++;
            }
            high = std::max(high, double(z));
        }
        CHECK(high <= 20.0 + depth + 1e-4);  // never more than its depth, however often
        CHECK(top > 20.0 + depth * 0.9);
        CHECK(raised > 20);  // the bricks
        CHECK(low > 5);      // the mortar between
        CHECK(closed(s.mesh()));

        sculpt::Sculptor e(start);
        e.beginPattern(sculpt::patternImage(pat), tile, -depth);
        dab.centre = {0, 0, 20};
        e.apply(dab, e.triangleAt(dab.centre));
        e.endPattern();
        CHECK(e.mesh().bounds().max[2] <= 20.0 + 1e-4);
        CHECK(e.mesh().bounds().min[2] >= -1e-4);
        double deepest = 20.0;
        for (std::size_t v = 0; v < e.mesh().vertexCount(); ++v)
            if (std::abs(e.mesh().positions[3 * v]) < 1 && std::abs(e.mesh().positions[3 * v + 1]) < 1)
                deepest = std::min(deepest, double(e.mesh().positions[3 * v + 2]));
        CHECK(deepest < 20.0 - depth * 0.8);
    }

    SECTION("A stamp: the picture pressed once, centred, turned")
    {
        const Mesh start = sculpt::refined(box(20), 0.25, 10'000'000);
        // A white square in the middle of a black picture.
        auto square = std::make_shared<PictureData>();
        square->width = square->height = 64;
        square->grey.assign(64 * 64, 0);
        for (int y = 16; y < 48; ++y)
            for (int x = 16; x < 48; ++x)
                square->grey[std::size_t(y) * 64 + std::size_t(x)] = 255;
        const auto raisedAt = [](const Mesh& m, double x, double y) {
            double z = -1e9;
            for (std::size_t v = 0; v < m.vertexCount(); ++v)
                if (std::abs(m.positions[3 * v] - x) < 0.3 && std::abs(m.positions[3 * v + 1] - y) < 0.3)
                    z = std::max(z, double(m.positions[3 * v + 2]));
            return z - 20.0;
        };
        for (double turn : {0.0, 45.0}) {
            sculpt::Sculptor s(start);
            s.setDetail(0.1);
            s.beginPattern(square, 8.0, 0.5, turn, true);
            sculpt::Dab dab;
            dab.brush = sculpt::Brush::Pattern;
            dab.radius = sculpt::stampRadius(*square, 8.0);
            dab.strength = 1.0;
            dab.centre = {0, 0, 20};
            s.apply(dab, s.triangleAt(dab.centre));
            s.endPattern();
            INFO("turned " << turn);
            CHECK(raisedAt(s.mesh(), 0.0, 0.0) > 0.45);   // the square
            CHECK(raisedAt(s.mesh(), 6.0, 0.0) < 0.05);   // outside the stamp: not repeated
            CHECK(raisedAt(s.mesh(), 3.2, 0.0) < 0.05);   // outside the square, on its black
            // Turned 45°, the square's corner reaches out along X.
            CHECK((turn == 0.0 ? raisedAt(s.mesh(), 2.5, 0.0) < 0.05 : raisedAt(s.mesh(), 2.5, 0.0) > 0.4));
            CHECK(closed(s.mesh()));
        }
        CHECK_THAT(sculpt::stampRadius(*square, 8.0), WithinRel(std::sqrt(32.0) * 1.02, 1e-9));
        if (const char* out = std::getenv("FORGE_PATTERN_DUMP"))
            for (const std::string& name : sculpt::builtInPatterns()) {
                SculptPattern pat;
                pat.name = name;
                const auto im = sculpt::patternImage(pat);
                std::ofstream f(std::string(out) + name + ".pgm", std::ios::binary);
                f << "P5 " << im->width * 2 << " " << im->height * 2 << " 255\n";
                for (int y = 0; y < im->height * 2; ++y)
                    for (int x = 0; x < im->width * 2; ++x)
                        f.put(char(im->grey[std::size_t(y % im->height) * im->width + std::size_t(x % im->width)]));
            }
    }

    SECTION("Kept as strokes: played back, saved with the step (a picture of one's own too)")
    {
        Document doc;
        CommandStack stack(doc);
        Body b;
        b.id = doc.allocateId();
        b.name = "Box";
        b.spec = BoxSpec{20, 20, 20};
        stack.execute(std::make_unique<AddBodyCommand>(b));
        SculptFinish finish;
        finish.detail = 0;
        SculptPattern scales;
        scales.name = "scales";
        SculptPattern own;
        own.name = "dots.png";
        own.builtIn = false;
        auto pic = std::make_shared<PictureData>();
        pic->width = 8;
        pic->height = 8;
        pic->grey.assign(64, 0);
        pic->grey[27] = pic->grey[28] = pic->grey[35] = pic->grey[36] = 255;  // a dot in the middle
        own.image = pic;
        finish.patterns = {scales, own};
        for (int k = 0; k < 2; ++k) {
            SculptStroke st;
            st.brush = int(sculpt::Brush::Pattern);
            st.radius = 3;
            st.strength = 1;
            st.finest = 0.2;
            st.pattern = k;
            st.tile = 3;
            st.mirrorX = k == 1;
            for (double x = -2; x <= 2; x += 0.7)
                st.dabs.push_back({float(x + 6 * k - 3), float(4 * k - 2), 20.f});
            finish.strokes.push_back(st);
        }
        {
            // A stamp, turned (kept in the file too).
            SculptStroke st;
            st.brush = int(sculpt::Brush::Pattern);
            st.strength = 1;
            st.finest = 0.2;
            st.pattern = 1;
            st.tile = 4;
            st.turn = 30;
            st.once = true;
            st.radius = sculpt::stampRadius(*pic, 4);
            st.dabs.push_back({-4.f, -5.f, 20.f});
            finish.strokes.push_back(st);
        }
        Body step;
        step.id = doc.allocateId();
        step.name = "Sculpt";
        BooleanSpec spec;
        spec.op = BooleanOp::Sculpt;
        spec.operands = {b.id};
        spec.sculpt = finish;
        step.boolean = spec;
        stack.execute(std::make_unique<CombineCommand>(step));
        CHECK(doc.worldBounds(step.id).max[2] > 20.5);

        const Mesh first = sculpt::replayed(doc.sculptStart(b.id, 0), finish);
        const Mesh second = sculpt::replayed(doc.sculptStart(b.id, 0), finish);
        CHECK(first.positions == second.positions);

        const auto loaded = file::fromJson(file::toJson(doc, "test"));
        Document other;
        other.replaceAll(loaded);
        CHECK(*other.find(step.id) == *doc.find(step.id));
        CHECK_THAT(meshkernel::volume(other.meshSolid(step.id)),
                   WithinRel(meshkernel::volume(doc.meshSolid(step.id)), 1e-9));

        // Resized evenly: the pattern grows with it.
        for (const Body& x : scaledTree(doc, step.id, {2, 2, 2}, {0, 0, 0}))
            if (x.id == step.id)
                CHECK_THAT(x.boolean->sculpt->strokes[0].tile, WithinRel(6.0, 1e-9));
    }
}

TEST_CASE("Sculpt: only what a dab changed is reported, in short runs", "[sculpt]")
{
    sculpt::Sculptor s(sculpt::refined(box(20), 0.25, 10'000'000));
    const auto first = s.takeChanges();
    CHECK(first.everything);  // a new sculptor: everything to send
    CHECK(s.takeChanges().points.empty());
    s.setDetail(0.1);

    const auto inside = [](std::uint32_t i, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& runs) {
        for (const auto& [a, b] : runs)
            if (i >= a && i < b)
                return true;
        return false;
    };
    std::size_t sent = 0;
    for (int k = 0; k < 6; ++k) {
        const Mesh before = s.mesh();
        sculpt::Dab dab;
        dab.centre = s.onSurface({-4.0 + 1.5 * k, 2.0, 10.0});
        dab.radius = 2.0;
        dab.strength = 0.6;
        s.apply(dab, s.triangleAt(dab.centre));
        const auto changes = s.takeChanges();
        REQUIRE_FALSE(changes.everything);
        const Mesh& after = s.mesh();
        std::size_t moved = 0;
        for (std::uint32_t i = 0; i < after.vertexCount(); ++i) {
            const bool isNew = i >= before.vertexCount();
            bool differs = isNew;
            for (int c = 0; c < 3 && !differs; ++c)
                differs = after.positions[3 * i + c] != before.positions[3 * i + c] ||
                          after.normals[3 * i + c] != before.normals[3 * i + c];
            if (differs) {
                ++moved;
                CHECK(inside(i, changes.points));  // every changed point is sent
            }
        }
        for (std::uint32_t t = 0; t < after.triangleCount(); ++t) {
            const bool differs = t >= before.triangleCount() ||
                                 !std::equal(after.indices.begin() + 3 * t, after.indices.begin() + 3 * t + 3,
                                             before.indices.begin() + 3 * t);
            if (differs)
                CHECK(inside(t, changes.triangles));
        }
        CHECK(moved > 0);
        for (const auto& [a, b] : changes.points)
            sent += b - a;
    }
    s.settle();
    // Points near each other are numbered near each other: a small brush sends a small
    // part of the mesh, not a stretch from one end of it to the other.
    CHECK(double(sent) / 6.0 < 0.25 * double(s.mesh().vertexCount()));
}
