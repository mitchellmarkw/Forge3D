// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 23: remeshing and simplifying.
#include "forge/core/Primitives.h"
#include "forge/core/Remesh.h"
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

double volumeOf(const Mesh& m)
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

bool closedMesh(const Mesh& m)
{
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
        for (int k = 0; k < 3; ++k)
            ++edges[{m.indices[t + k], m.indices[t + (k + 1) % 3]}];
    for (const auto& [e, n] : edges)
        if (n != 1 || !edges.count({e.second, e.first}))
            return false;
    return true;
}

// Edge lengths: the shortest, the longest and the average.
struct Spread {
    double lo = 1e300, hi = 0, mean = 0;
};
Spread spread(const Mesh& m)
{
    Spread s;
    std::size_t n = 0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const float* a = &m.positions[3 * m.indices[t + k]];
            const float* b = &m.positions[3 * m.indices[t + (k + 1) % 3]];
            const double l = std::hypot(double(a[0]) - b[0], double(a[1]) - b[1], double(a[2]) - b[2]);
            s.lo = std::min(s.lo, l);
            s.hi = std::max(s.hi, l);
            s.mean += l;
            ++n;
        }
    s.mean /= double(std::max<std::size_t>(n, 1));
    return s;
}

Mesh sphere(double r)
{
    return sculpt::welded(tessellate(primitives::buildShape(SphereSpec{r})), 1e-5);
}

}  // namespace

TEST_CASE("Remesh: even triangles of the size asked, on the same surface", "[remesh]")
{
    // A sphere with its top pulled up: long thin triangles up the spike.
    sculpt::Sculptor s(sculpt::refined(sphere(10), 1.0, 1'000'000));
    const auto top = s.hit({0, 0, 50}, {0, 0, -1});
    REQUIRE(top);
    s.beginGrab(top->point, 4.0, top->triangle, {});
    s.moveGrab({0, 0, 12});
    s.endGrab();
    const Mesh stretched = s.mesh();
    REQUIRE(spread(stretched).hi > 3.0);

    const auto t0 = std::chrono::steady_clock::now();
    const Mesh even = remesh::remeshed(stretched, 0.8);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    INFO(ms << " ms, " << even.triangleCount() << " triangles");
    CHECK(closedMesh(even));
    const Spread e = spread(even);
    CHECK(e.hi < 0.8 * 4.0 / 3.0 * 1.6);  // nothing long left
    CHECK_THAT(e.mean, WithinRel(0.8, 0.25));
    CHECK_THAT(volumeOf(even), WithinRel(volumeOf(stretched), 0.02));  // the same shape
    CHECK(even.bounds().max[2] > stretched.bounds().max[2] - 1.0);  // the spike is still there
    CHECK(ms < 10000.0);

    // Coarser: fewer triangles, still closed and the same size.
    const Mesh coarse = remesh::remeshed(stretched, 2.0);
    CHECK(coarse.triangleCount() < even.triangleCount() / 3);
    CHECK(closedMesh(coarse));
    CHECK_THAT(volumeOf(coarse), WithinRel(volumeOf(stretched), 0.05));

    // The same every time.
    const Mesh again = remesh::remeshed(stretched, 0.8);
    CHECK(again.positions == even.positions);
    CHECK(again.indices == even.indices);
}

TEST_CASE("Remesh: a mask comes along", "[remesh]")
{
    Mesh m = sculpt::refined(sphere(10), 1.0, 1'000'000);
    m.shade.assign(m.vertexCount(), 0.0f);
    for (std::size_t v = 0; v < m.vertexCount(); ++v)
        if (m.positions[3 * v + 2] > 0)
            m.shade[v] = 1.0f;  // the top half masked
    const Mesh even = remesh::remeshed(m, 0.7);
    REQUIRE(even.shade.size() == even.vertexCount());
    int right = 0, wrong = 0;
    for (std::size_t v = 0; v < even.vertexCount(); ++v) {
        const float z = even.positions[3 * v + 2];
        if (std::abs(z) < 1.5)
            continue;  // the blurred line between
        ((z > 0) == (even.shade[v] > 0.5f) ? right : wrong)++;
    }
    CHECK(wrong == 0);
    CHECK(right > 100);
}

TEST_CASE("Simplify: fewer triangles, the same shape, still closed", "[remesh][simplify]")
{
    const Mesh dense = sculpt::refined(sphere(10), 0.25, 4'000'000);
    REQUIRE(dense.triangleCount() > 50'000);
    const auto t0 = std::chrono::steady_clock::now();
    const Mesh tenth = remesh::simplified(dense, dense.triangleCount() / 10);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    INFO(ms << " ms for " << dense.triangleCount() << " triangles");
    CHECK(tenth.triangleCount() <= dense.triangleCount() / 10);
    CHECK(tenth.triangleCount() > dense.triangleCount() / 12);
    CHECK(closedMesh(tenth));
    CHECK_THAT(volumeOf(tenth), WithinRel(volumeOf(dense), 0.01));
    CHECK(ms < 10000.0);

    // Very few: still a closed, round-ish shape.
    const Mesh few = remesh::simplified(dense, 200);
    CHECK(few.triangleCount() <= 200);
    CHECK(closedMesh(few));
    CHECK_THAT(volumeOf(few), WithinRel(volumeOf(dense), 0.1));

    // A box's flat sides need very few triangles: they go first and corners stay sharp.
    const Mesh box = sculpt::refined(sculpt::welded(tessellate(primitives::buildShape(BoxSpec{20, 20, 20})), 1e-5),
                                     0.5, 4'000'000);
    const Mesh plain = remesh::simplified(box, 12);
    CHECK(plain.triangleCount() == 12);
    CHECK(closedMesh(plain));
    CHECK_THAT(volumeOf(plain), WithinRel(8000.0, 1e-3));

    // A tessellated solid as it comes (corners not shared yet) is fine too.
    const Mesh raw = tessellate(primitives::buildShape(SphereSpec{10}));
    const Mesh fromRaw = remesh::simplified(raw, raw.triangleCount() / 2);
    CHECK(closedMesh(fromRaw));
}

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"

TEST_CASE("Simplify steps, and Remesh kept as a sculpt stroke", "[remesh][simplifystep]")
{
    Document doc;
    CommandStack stack(doc);
    Body ball;
    ball.id = doc.allocateId();
    ball.name = "Ball";
    ball.spec = SphereSpec{10};
    stack.execute(std::make_unique<AddBodyCommand>(ball));

    // Sculpted (High detail, a remesh in the middle), then simplified to a tenth.
    SculptFinish finish;
    finish.detail = 0;
    SculptStroke draw;
    draw.brush = int(sculpt::Brush::Draw);
    draw.radius = 3;
    draw.strength = 1;
    draw.dabs = {{0.f, 0.f, 10.f}};
    SculptStroke even;
    even.brush = int(sculpt::Brush::Remesh);
    even.radius = 0.6;
    SculptStroke crease;
    crease.brush = int(sculpt::Brush::Crease);
    crease.radius = 2;
    crease.strength = 1;
    crease.dabs = {{0.f, 0.f, 10.f}};
    finish.strokes = {draw, even, crease};
    Body sculpted;
    sculpted.id = doc.allocateId();
    sculpted.name = "Sculpted";
    BooleanSpec ss;
    ss.op = BooleanOp::Sculpt;
    ss.operands = {ball.id};
    ss.sculpt = finish;
    sculpted.boolean = ss;
    stack.execute(std::make_unique<CombineCommand>(sculpted));
    const auto full = doc.mesh(sculpted.id)->triangleCount();
    const double fullVolume = meshkernel::volume(doc.meshSolid(sculpted.id));

    Body simple;
    simple.id = doc.allocateId();
    simple.name = "Simplified";
    BooleanSpec sp;
    sp.op = BooleanOp::Simplify;
    sp.operands = {sculpted.id};
    sp.simplify = SimplifyFinish{0.1};
    simple.boolean = sp;
    stack.execute(std::make_unique<CombineCommand>(simple));
    const auto fewer = doc.mesh(simple.id)->triangleCount();
    CHECK(fewer <= full / 10 + 2);
    CHECK(fewer > full / 13);
    CHECK_THAT(meshkernel::volume(doc.meshSolid(simple.id)), WithinRel(fullVolume, 0.02));
    CHECK(history::describe(doc, simple.id).find("10% of the triangles") != std::string::npos);

    // Keep more: more triangles.
    Body more = *doc.find(simple.id);
    more.boolean->simplify->keep = 0.5;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(simple.id), more, "Simplify"));
    CHECK(doc.mesh(simple.id)->triangleCount() > fewer * 4);

    // Saved and opened: every brush and the Simplify step come back as they were.
    const auto loaded = file::fromJson(file::toJson(doc, "test"));
    Document other;
    other.replaceAll(loaded);
    CHECK(*other.find(sculpted.id) == *doc.find(sculpted.id));
    CHECK(*other.find(simple.id) == *doc.find(simple.id));
    CHECK(other.mesh(simple.id)->triangleCount() == doc.mesh(simple.id)->triangleCount());

    // Undo takes it all back.
    stack.undo();
    stack.undo();
    CHECK(doc.find(simple.id) == nullptr);
}

TEST_CASE("Every sculpt brush survives saving", "[remesh][simplifystep]")
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
    for (int brush = 0; brush <= int(sculpt::Brush::Remesh); ++brush) {
        SculptStroke st;
        st.brush = brush;
        st.radius = brush == int(sculpt::Brush::Remesh) ? 1.5 : 2.0;
        st.strength = 0.5;
        if (brush != int(sculpt::Brush::MaskClear) && brush != int(sculpt::Brush::MaskInvert) &&
            brush != int(sculpt::Brush::Remesh))
            st.dabs = {{float(brush - 6), 0.f, 20.f}, {float(brush - 6), 0.f, 21.f}};
        if (brush == int(sculpt::Brush::Grab))
            st.grab = {0.f, 0.f, 1.f};
        finish.strokes.push_back(st);
    }
    Body step;
    step.id = doc.allocateId();
    step.name = "Sculpted";
    BooleanSpec ss;
    ss.op = BooleanOp::Sculpt;
    ss.operands = {box.id};
    ss.sculpt = finish;
    step.boolean = ss;
    stack.execute(std::make_unique<CombineCommand>(step));
    const auto loaded = file::fromJson(file::toJson(doc, "test"));
    Document other;
    other.replaceAll(loaded);
    REQUIRE(other.find(step.id));
    CHECK(*other.find(step.id) == *doc.find(step.id));
    CHECK_THAT(meshkernel::volume(other.meshSolid(step.id)), WithinRel(meshkernel::volume(doc.meshSolid(step.id)), 1e-9));
}
