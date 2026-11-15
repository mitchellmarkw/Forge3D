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
