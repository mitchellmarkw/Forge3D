// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 112: Forge3D Scenery's rock designer.
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Rock.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
double volumeOf(const Mesh& m)
{
    return meshkernel::volume(meshkernel::fromTriangles(m.positions, m.indices));
}
}  // namespace

TEST_CASE("Rocks: every kind, to scale, sunk with a flat base", "[rock]")
{
    for (RockKind kind : {RockKind::Boulder, RockKind::Crag, RockKind::Slab, RockKind::Pile}) {
        INFO(rock::title(kind));
        RockSpec s = rock::defaults(kind);
        s.scale = 35;
        const auto start = std::chrono::steady_clock::now();
        const Mesh m = rock::make(s);
        WARN(rock::title(kind) << ": " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
                               << " s, " << m.triangleCount() << " triangles");
        const Bounds b = m.bounds();
        const double k = 1.0 / s.scale;
        CHECK_THAT(double(b.min[2]), WithinAbs(0.0, 0.05));  // on the ground
        CHECK_THAT(double(b.center()[0]), WithinAbs(0.0, 0.05));
        // About its size (lumps and cuts change it a little); sunk: lower.
        CHECK(b.size()[0] > 0.6 * s.length * k);
        CHECK(b.size()[0] < 1.3 * s.length * k);
        CHECK(b.size()[2] < (1.0 - s.sunk) * s.height * k * 1.3);
        CHECK(b.size()[2] > 0.4 * (1.0 - s.sunk) * s.height * k);
        CHECK(volumeOf(m) > 0);
        // A flat base: plenty of the surface down on Z = 0.
        std::size_t onGround = 0;
        for (std::size_t i = 2; i < m.positions.size(); i += 3)
            onGround += std::abs(m.positions[i]) < 1e-3;
        CHECK(onGround > 10);
    }
}

TEST_CASE("Rocks: each seed a different rock, the same seed the same one", "[rock]")
{
    RockSpec s = rock::defaults(RockKind::Crag);
    const double a = volumeOf(rock::make(s));
    CHECK_THAT(volumeOf(rock::make(s)), WithinRel(a, 1e-9));
    s.seed = 2;
    CHECK(std::abs(volumeOf(rock::make(s)) - a) > 0.01 * a);
    // Smooth vs rough: the rough one's surface is bigger for about the same size.
    RockSpec smooth = rock::defaults(RockKind::Boulder);
    smooth.roughness = 0;
    RockSpec rough = smooth;
    rough.roughness = 1;
    const auto area = [](const Mesh& m) {
        double sum = 0;
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            const float* p = &m.positions[3 * m.indices[t]];
            const float* q = &m.positions[3 * m.indices[t + 1]];
            const float* r = &m.positions[3 * m.indices[t + 2]];
            const double e1[3] = {q[0] - p[0], q[1] - p[1], q[2] - p[2]}, e2[3] = {r[0] - p[0], r[1] - p[1], r[2] - p[2]};
            sum += 0.5 * std::sqrt(std::pow(e1[1] * e2[2] - e1[2] * e2[1], 2) + std::pow(e1[2] * e2[0] - e1[0] * e2[2], 2) +
                                   std::pow(e1[0] * e2[1] - e1[1] * e2[0], 2));
        }
        return sum;
    };
    const Mesh ms = rock::make(smooth), mr = rock::make(rough);
    CHECK(area(mr) / std::cbrt(volumeOf(mr) * volumeOf(mr)) > area(ms) / std::cbrt(volumeOf(ms) * volumeOf(ms)));
    // Layers: grooves, less volume.
    RockSpec layered = rock::defaults(RockKind::Crag);
    const double plain = volumeOf(rock::make(layered));
    layered.layers = true;
    CHECK(volumeOf(rock::make(layered)) < plain);
    // Quick: coarser.
    CHECK(rock::make(layered, true).triangleCount() < rock::make(layered).triangleCount());
    // Checked.
    RockSpec tiny = rock::defaults(RockKind::Boulder);
    tiny.length = tiny.width = tiny.height = 20;
    tiny.scale = 100;
    CHECK_THROWS_AS(rock::make(tiny), std::invalid_argument);
}

TEST_CASE("Rocks: added, changed and saved", "[rock]")
{
    Document doc;
    CommandStack stack(doc);
    const RockSpec s = rock::defaults(RockKind::Boulder);
    const ObjectId id = rock::add(stack, doc, s, "Boulder", 30.0, 15.0);
    REQUIRE(doc.find(id));
    CHECK(doc.find(id)->rock);
    CHECK_THAT(double(doc.worldBounds(id).center()[0]), WithinAbs(30.0, 0.1));
    CHECK_THAT(double(doc.worldBounds(id).min[2]), WithinAbs(0.0, 0.05));
    RockSpec t = s;
    t.seed = 9;
    t.length = 2500;
    rock::update(stack, doc, id, t);
    CHECK(doc.find(id)->rock->seed == 9);
    CHECK_THAT(double(doc.worldBounds(id).center()[0]), WithinAbs(30.0, 0.1));
    stack.undo();
    CHECK(doc.find(id)->rock->seed == 1);
    const auto path = std::filesystem::temp_directory_path() / "forge_rock_test.f3d";
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    CHECK(other.find(id)->rock == doc.find(id)->rock);
}
