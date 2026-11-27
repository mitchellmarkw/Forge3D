// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 113: Forge3D Scenery's terrain designer.
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Terrain.h"

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

TEST_CASE("Terrain: to scale, a flat underside, rectangle or round, plain or bevelled", "[terrain]")
{
    TerrainSpec s = terrain::defaults();
    const double k = 1.0 / s.scale;
    const auto start = std::chrono::steady_clock::now();
    const Mesh m = terrain::make(s);
    INFO("made in " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s");
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(20));
    const Bounds b = m.bounds();
    CHECK_THAT(double(b.max[0] - b.min[0]), WithinRel(s.width * k, 0.03));
    CHECK_THAT(double(b.max[1] - b.min[1]), WithinRel(s.depth * k, 0.03));
    CHECK_THAT(double(b.min[2]), WithinAbs(0.0, 0.01));
    CHECK(double(b.max[2]) > s.baseThickness + 0.3 * s.hillHeight * k);
    CHECK(double(b.max[2]) < s.baseThickness + 1.2 * s.hillHeight * k + 2);
    // Level edges: at the base's height there.
    CHECK_THAT(terrain::heightAt(s, s.width / 2 - 1, 0), WithinAbs(s.baseThickness, 0.3));
    CHECK(terrain::heightAt(s, 0, 0) > 0.0);
    const double rect = volumeOf(m);
    // Round: an oval inside the rectangle, less of it.
    TerrainSpec round = s;
    round.base = TerrainBase::Round;
    CHECK(terrain::inside(round, 0, 0));
    CHECK_FALSE(terrain::inside(round, s.width / 2 - 10, s.depth / 2 - 10));
    CHECK(terrain::inside(s, s.width / 2 - 10, s.depth / 2 - 10));
    const double oval = volumeOf(terrain::make(round));
    CHECK(oval < 0.85 * rect);
    CHECK(oval > 0.65 * rect);
    // Bevelled: a little less.
    TerrainSpec bev = s;
    bev.edge = TerrainEdge::Bevelled;
    CHECK(volumeOf(terrain::make(bev)) < rect);
    // Flat: no hills, the base's slab and a little roughness.
    TerrainSpec flat = s;
    flat.hillHeight = 0;
    flat.roughness = 0;
    const Bounds fb = terrain::make(flat).bounds();
    CHECK_THAT(double(fb.max[2]), WithinAbs(s.baseThickness, 0.2));
    // Quick: coarser.
    CHECK(terrain::make(s, true).triangleCount() < m.triangleCount());
    // Checked.
    TerrainSpec tiny = s;
    tiny.width = tiny.depth = 200;
    CHECK_THROWS_AS(terrain::make(tiny), std::invalid_argument);
}

TEST_CASE("Terrain: craters, crags and paths, placed or scattered", "[terrain]")
{
    TerrainSpec s = terrain::defaults();
    s.hillHeight = 0;
    s.roughness = 0;
    s.baseThickness = 25;  // deep enough for the crater (they stop short of the underside)
    const double k = 1.0 / s.scale;
    const double flat = terrain::heightAt(s, 0, 0);
    // A crater: a dip with a raised rim.
    s.features.push_back(terrain::feature(s, TerrainFeatureKind::Crater, 0, 0));
    const TerrainFeature crater = s.features.back();
    CHECK(terrain::heightAt(s, 0, 0) < flat - 0.5 * crater.depth * k);
    CHECK(terrain::heightAt(s, crater.size / 2, 0) > flat);
    REQUIRE(terrain::pickedFeature(s, 100, 100));
    CHECK(*terrain::pickedFeature(s, 100, 100) == 0);
    CHECK_FALSE(terrain::pickedFeature(s, crater.size * 2, 0));
    // A path: sunk along it.
    s.features.push_back(terrain::feature(s, TerrainFeatureKind::Path, -3000, 2000, 3000, 2000));
    CHECK_THAT(terrain::heightAt(s, 0, 2000), WithinAbs(flat - s.features.back().depth * k, 0.05));
    CHECK(*terrain::pickedFeature(s, 1000, 2100) == 1);
    // A crag: up through the ground.
    const double before = volumeOf(terrain::make(s));
    s.features.push_back(terrain::feature(s, TerrainFeatureKind::Crag, 2500, -1500));
    const Mesh withCrag = terrain::make(s);
    CHECK(volumeOf(withCrag) > before);
    CHECK(double(withCrag.bounds().max[2]) > flat + 0.4 * s.features.back().depth * k);
    // The view's ray finds the ground.
    const auto hit = terrain::hitAt(s, {-30.0 * k * 35, 0.0, 200.0}, {0.0, 0.0, -1.0});
    REQUIRE(hit);
    CHECK_THAT((*hit)[0], WithinAbs(-30.0 * 35, 1.0));
    CHECK_FALSE(terrain::hitAt(s, {1000.0, 0.0, 200.0}, {0.0, 0.0, -1.0}));
    // Scatter: replaces only scattered ones; another seed, other places.
    terrain::scatter(s, 3, 2, 1, 5);
    CHECK(s.features.size() == 9);
    const auto first = s.features;
    terrain::scatter(s, 3, 2, 1, 6);
    CHECK(s.features.size() == 9);
    CHECK(s.features != first);
    CHECK(s.features[0] == first[0]);  // the placed ones stay
    terrain::scatter(s, 0, 0, 0, 7);
    CHECK(s.features.size() == 3);
    terrain::scatter(s, 6, 3, 2, 8);
    for (const TerrainFeature& f : s.features)
        if (f.kind != TerrainFeatureKind::Path)
            CHECK(terrain::inside(s, f.x, f.y));
    CHECK_NOTHROW(terrain::make(s, true));
    CHECK(terrain::marker(s, s.features[0]).triangleCount() > 10);
    CHECK(terrain::marker(s, s.features[1]).triangleCount() == 12);
}

TEST_CASE("Terrain: added, changed and saved", "[terrain]")
{
    Document doc;
    CommandStack stack(doc);
    TerrainSpec s = terrain::defaults();
    s.features.push_back(terrain::feature(s, TerrainFeatureKind::Crater, 1000, 500));
    const ObjectId id = terrain::add(stack, doc, s, "Terrain", 40.0, -10.0);
    REQUIRE(doc.find(id));
    CHECK(doc.find(id)->terrain);
    CHECK_THAT(double(doc.worldBounds(id).center()[0]), WithinAbs(40.0, 0.5));
    CHECK_THAT(double(doc.worldBounds(id).min[2]), WithinAbs(0.0, 0.05));
    TerrainSpec t = *doc.find(id)->terrain;
    t.seed = 4;
    t.features.push_back(terrain::feature(t, TerrainFeatureKind::Path, -3000, 0, 3000, 0));
    terrain::update(stack, doc, id, t);
    CHECK(doc.find(id)->terrain->seed == 4);
    CHECK(doc.find(id)->terrain->features.size() == 2);
    CHECK_THAT(double(doc.worldBounds(id).center()[0]), WithinAbs(40.0, 0.5));
    stack.undo();
    CHECK(doc.find(id)->terrain->features.size() == 1);
    stack.redo();
    const auto path = std::filesystem::temp_directory_path() / "forge_terrain_test.f3d";
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    CHECK(other.find(id)->terrain == doc.find(id)->terrain);
}
