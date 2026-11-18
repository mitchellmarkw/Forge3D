// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 40: Smooth and Extrude Down.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, double z = 0.0)
{
    Body b;
    b.id = doc.allocateId();
    b.name = doc.uniqueName("Shape");
    b.spec = std::move(spec);
    b.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

ObjectId step(CommandStack& stack, Document& doc, BooleanSpec spec)
{
    Body r;
    r.id = doc.allocateId();
    r.name = doc.uniqueName(toString(spec.op));
    r.boolean = std::move(spec);
    stack.execute(std::make_unique<CombineCommand>(r));
    return r.id;
}

double volumeOf(const Document& doc, ObjectId id)
{
    return meshkernel::volume(doc.meshSolid(id));
}

}  // namespace

TEST_CASE("Smooth: corners rounded, size kept, still closed", "[meshsteps]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    BooleanSpec s;
    s.op = BooleanOp::Smooth;
    s.operands = {box};
    s.smooth = SmoothFinish{3};
    const ObjectId smooth = step(stack, doc, s);
    const auto mesh = doc.mesh(smooth);
    CHECK(mesh->triangleCount() > 1000);  // evened out first
    // Corners pulled in: no point as far out as the box's corners (17.3 from the centre).
    double far = 0.0;
    for (std::size_t i = 0; i + 2 < mesh->positions.size(); i += 3) {
        const double x = mesh->positions[i], y = mesh->positions[i + 1], z = mesh->positions[i + 2] - 10.0;
        far = std::max(far, std::sqrt(x * x + y * y + z * z));
    }
    CHECK(far < 16.5);
    // ...and not shrunk: the volume is kept.
    const double v = volumeOf(doc, smooth);
    CHECK_THAT(v, WithinRel(8000.0, 0.01));
    const auto farthest = [&doc](ObjectId id) {
        const auto m = doc.mesh(id);
        double out = 0.0;
        for (std::size_t i = 0; i + 2 < m->positions.size(); i += 3) {
            const double x = m->positions[i], y = m->positions[i + 1], z = m->positions[i + 2] - 10.0;
            out = std::max(out, std::sqrt(x * x + y * y + z * z));
        }
        return out;
    };

    // Stronger: rounder.
    Body after = *doc.find(smooth);
    after.boolean->smooth->strength = 10;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(smooth), after, "Smooth"));
    CHECK(farthest(smooth) < far - 1.0);
    stack.undo();
    CHECK_THAT(volumeOf(doc, smooth), WithinRel(v, 1e-9));

    // Saved and read back.
    Document again;
    again.replaceAll(file::fromJson(file::toJson(doc, "test")));
    CHECK(again.find(smooth)->boolean->smooth->strength == 3);
    CHECK_THAT(volumeOf(again, smooth), WithinRel(v, 1e-6));

    CHECK_THROWS(step(stack, doc, [&] {
        BooleanSpec bad = s;
        bad.operands = {smooth};
        bad.smooth->strength = 11;
        return bad;
    }()));
}

TEST_CASE("Extrude Down: the underside stretched down to the plate", "[meshsteps]")
{
    Document doc;
    CommandStack stack(doc);
    BooleanSpec e;
    e.op = BooleanOp::ExtrudeDown;
    e.planePoint = {0, 0, 0};
    e.planeNormal = {0, 0, 1};

    SECTION("a box held up in the air becomes a taller box")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20}, 10.0);
        e.operands = {box};
        const ObjectId down = step(stack, doc, e);
        const Bounds b = doc.worldBounds(down);
        CHECK_THAT(b.min[2], WithinAbs(0.0, 1e-3));
        CHECK_THAT(b.max[2], WithinAbs(30.0, 1e-3));
        CHECK_THAT(volumeOf(doc, down), WithinRel(20.0 * 20 * 30, 1e-4));
    }
    SECTION("a ball over the plate gets a column under it")
    {
        SphereSpec ball;  // radius 10
        const ObjectId sphere = add(stack, doc, ball, 5.0);  // bottom 5 mm up
        e.operands = {sphere};
        const ObjectId down = step(stack, doc, e);
        CHECK_THAT(doc.worldBounds(down).min[2], WithinAbs(0.0, 1e-3));
        // The ball, plus a column 5 mm tall, plus the space under the lower half.
        const double ballVolume = 4.0 / 3.0 * M_PI * 1000.0;
        const double expected = ballVolume / 2 + M_PI * 100.0 * 15.0;  // top half + cylinder r10, 0..15
        CHECK_THAT(volumeOf(doc, down), WithinRel(expected, 0.03));
        // Saved and read back.
        Document again;
        again.replaceAll(file::fromJson(file::toJson(doc, "test")));
        CHECK_THAT(volumeOf(again, down), WithinRel(volumeOf(doc, down), 1e-6));
    }
    SECTION("a T fills in under both arms, and a ring fills in under itself")
    {
        const ObjectId stem = add(stack, doc, BoxSpec{10, 10, 30});
        const ObjectId bar = add(stack, doc, BoxSpec{40, 10, 5}, 30.0);
        BooleanSpec m;
        m.op = BooleanOp::Merge;
        m.operands = {stem, bar};
        const ObjectId t = step(stack, doc, m);
        e.operands = {t};
        const ObjectId down = step(stack, doc, e);
        CHECK_THAT(volumeOf(doc, down), WithinRel(40.0 * 10 * 35, 1e-4));

        TorusSpec ring;  // lying flat, lifted 3 mm: the hole stays open, the ring gets a skirt
        const ObjectId torus = add(stack, doc, ring, 3.0);
        e.operands = {torus};
        const ObjectId skirt = step(stack, doc, e);
        CHECK_THAT(doc.worldBounds(skirt).min[2], WithinAbs(0.0, 1e-3));
        // The ring's top half, and below its middle a solid annulus (radii 8 and 16) 7 mm tall.
        const double expected = M_PI * M_PI * 12.0 * 16.0 + M_PI * (16.0 * 16.0 - 8.0 * 8.0) * 7.0;
        CHECK_THAT(volumeOf(doc, skirt), WithinRel(expected, 0.03));
    }
    SECTION("a shape sitting on the plate is unchanged")
    {
        const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
        e.operands = {box};
        const ObjectId down = step(stack, doc, e);
        CHECK_THAT(volumeOf(doc, down), WithinRel(8000.0, 1e-6));
    }
}



#include "forge/core/MeshSteps.h"
#include <algorithm>
#include <array>
#include <map>
TEST_CASE("Smooth: an even result, with no lumps", "[meshsteps]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{20, 20, 20});
    Mesh surface;
    meshkernel::triangles(doc.meshSolid(box), surface.positions, surface.indices);
    const Mesh round = meshsteps::smoothed(surface, 10);
    // Strongly smoothed, a cube becomes a rounded, pebble-like shape. Lumps show up as
    // sharp bends between neighbouring triangles.
    const std::size_t n = round.positions.size() / 3;
    std::vector<double> r(n);
    for (std::size_t v = 0; v < n; ++v) {
        const double x = round.positions[v * 3], y = round.positions[v * 3 + 1], z = round.positions[v * 3 + 2] - 10.0;
        r[v] = std::sqrt(x * x + y * y + z * z);
    }
    const auto normal = [&round](std::size_t t) {
        const float* a = &round.positions[round.indices[t * 3] * 3];
        const float* b = &round.positions[round.indices[t * 3 + 1] * 3];
        const float* c = &round.positions[round.indices[t * 3 + 2] * 3];
        const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        std::array<double, 3> nn{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                                 e1[0] * e2[1] - e1[1] * e2[0]};
        const double l = std::sqrt(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
        for (double& x : nn)
            x /= std::max(l, 1e-300);
        return nn;
    };
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::size_t> edgeOf;
    double sharpest = 0.0;
    for (std::size_t t = 0; t < round.indices.size() / 3; ++t)
        for (int k = 0; k < 3; ++k) {
            std::uint32_t a = round.indices[t * 3 + k], b = round.indices[t * 3 + (k + 1) % 3];
            const auto e = std::minmax(a, b);
            const auto [it, added] = edgeOf.try_emplace({e.first, e.second}, t);
            if (added)
                continue;
            const auto n1 = normal(it->second), n2 = normal(t);
            const double d = std::clamp(n1[0] * n2[0] + n1[1] * n2[1] + n1[2] * n2[2], -1.0, 1.0);
            sharpest = std::max(sharpest, std::acos(d) * 180.0 / M_PI);
        }
    CHECK(sharpest < 20.0);  // degrees between neighbouring triangles
    const auto [lo, hi] = std::minmax_element(r.begin(), r.end());
    CHECK(*hi - *lo < 3.5);  // rounded right off: corners (17.3) and faces (10) much closer
}

#include <filesystem>
#include <random>
TEST_CASE("Slow results are saved with the file and used when it's opened", "[meshsteps][file]")
{
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / ("forge3d-results-" + std::to_string(std::random_device{}()) + ".f3d");
    Document doc;
    CommandStack stack(doc);
    const ObjectId sphere = add(stack, doc, SphereSpec{}, 5.0);
    BooleanSpec e;
    e.op = BooleanOp::ExtrudeDown;
    e.operands = {sphere};
    const ObjectId down = step(stack, doc, e);
    BooleanSpec s;
    s.op = BooleanOp::Smooth;
    s.operands = {down};
    s.smooth = SmoothFinish{4};
    const ObjectId smooth = step(stack, doc, s);
    const double volume = volumeOf(doc, smooth);
    file::save(doc, path, "test");

    SECTION("used as saved")
    {
        Document again;
        again.replaceAll(file::load(path));
        CHECK(file::useSavedResults(again, path) == 1);
        REQUIRE(again.cachedSolid(smooth).has_value());
        CHECK_THAT(meshkernel::volume(*again.cachedSolid(smooth)), WithinRel(volume, 1e-5));
        CHECK_FALSE(again.cachedSolid(down).has_value());  // the steps inside: worked out if needed
        // Renamed, recoloured or moved: still the same shape.
        CommandStack s2(again);
        s2.execute(std::make_unique<RenameBodyCommand>(smooth, "Pebble"));
        Placement moved = again.find(smooth)->placement;
        moved.x += 30;
        s2.execute(std::make_unique<SetPlacementCommand>(smooth, moved));
        CHECK(file::resultKey(again, smooth) == file::resultKey(doc, smooth));
    }
    SECTION("not used once something it's made from is different")
    {
        Document again;
        auto bodies = file::load(path);
        for (Body& b : bodies)
            if (b.id == smooth)
                b.boolean->smooth->strength = 5;
        again.replaceAll(bodies);
        CHECK(file::useSavedResults(again, path) == 0);
        CHECK_FALSE(again.cachedSolid(smooth).has_value());
    }
    fs::remove(path);
}
