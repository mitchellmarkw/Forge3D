// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 111: Forge3D Scenery's building designer.
#include "forge/core/Building.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"

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

Bounds boundsOf(const Mesh& m)
{
    return m.bounds();
}

}  // namespace

TEST_CASE("Buildings: brick walls to scale, mortar recessed", "[building]")
{
    BuildingSpec s = building::defaults(WallMaterial::Brick);
    s.roof = RoofKind::None;
    s.scale = 35;
    const auto start = std::chrono::steady_clock::now();
    const building::Made made = building::make(s);
    WARN("brick building: " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()
                            << " s, " << made.walls.triangleCount() << " triangles");
    const Bounds b = boundsOf(made.walls);
    // 6 x 4 x 2.8 m at 1:35.
    CHECK_THAT(double(b.size()[0]), WithinAbs(6000.0 / 35, 0.01));
    CHECK_THAT(double(b.size()[1]), WithinAbs(4000.0 / 35, 0.01));
    CHECK_THAT(double(b.size()[2]), WithinAbs(2800.0 / 35, 0.01));
    CHECK_THAT(double(b.min[2]), WithinAbs(0.0, 1e-6));
    // Mortar recessed: less than solid walls, more than the shells inside the recess.
    const double T = 230.0 / 35, W = 6000.0 / 35, D = 4000.0 / 35, H = 80.0;
    const double solid = (W * D - (W - 2 * T) * (D - 2 * T)) * H;
    const double v = volumeOf(made.walls);
    CHECK(v < solid);
    CHECK(v > 0.9 * solid);
    // The joint, 10 mm, is 0.29 mm at 1:35: widened to the printer's 0.4.
    REQUIRE_FALSE(made.notes.empty());
    CHECK(made.notes[0].find("widened") != std::string::npos);
    // A quick preview: plain walls.
    const building::Made quick = building::make(s, true);
    CHECK_THAT(volumeOf(quick.walls), WithinRel(solid, 0.01));
    CHECK(quick.walls.triangleCount() < made.walls.triangleCount() / 10);
}

TEST_CASE("Buildings: every material, every roof", "[building]")
{
    for (WallMaterial m : {WallMaterial::Brick, WallMaterial::Stone, WallMaterial::Logs, WallMaterial::Planks})
        for (RoofKind r : {RoofKind::None, RoofKind::Flat, RoofKind::Gable, RoofKind::Hip}) {
            INFO(building::title(m) << ", " << building::title(r));
            BuildingSpec s = building::defaults(m);
            s.roof = r;
            s.scale = 48;
            const auto start = std::chrono::steady_clock::now();
            const building::Made made = building::make(s);
            const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (r == RoofKind::Gable)
                WARN(building::title(m) << ": " << took << " s");
            CHECK(volumeOf(made.walls) > 0);
            const Bounds b = boundsOf(made.walls);
            CHECK(b.size()[0] >= 6000.0 / 48 - 0.01);
            if (r == RoofKind::None) {
                CHECK(made.roof.indices.empty());
            } else {
                REQUIRE_FALSE(made.roof.indices.empty());
                const Bounds rb = boundsOf(made.roof);
                CHECK(volumeOf(made.roof) > 0);
                // On the walls' top, past them by the overhang.
                CHECK_THAT(double(rb.min[2]), WithinAbs(double(b.max[2]), 0.6));
                CHECK_THAT(double(rb.size()[0]), WithinAbs((6000.0 + 600.0) / 48, 0.05));
                if (r == RoofKind::Gable || r == RoofKind::Hip)
                    CHECK(rb.size()[2] > 1500.0 / 48);  // 35 degrees over half the depth
            }
        }
    // Roof joined to the walls: one shape.
    BuildingSpec s = building::defaults(WallMaterial::Planks);
    s.roofSeparate = false;
    const building::Made one = building::make(s);
    CHECK(one.roof.indices.empty());
    CHECK(boundsOf(one.walls).size()[2] > 2800.0 / 35 + 10);
}

TEST_CASE("Buildings: doors and windows, placed by a click", "[building]")
{
    BuildingSpec s = building::defaults(WallMaterial::Brick);
    s.roof = RoofKind::None;
    const double k = 1.0 / s.scale;
    // A ray straight at the front wall, 2 m from its left end, 1.5 m up.
    const auto spot = building::spotAt(s, {-3000 * k + 2000 * k, -100.0, 1500 * k}, {0, 1, 0});
    REQUIRE(spot);
    CHECK(spot->wall == 0);
    CHECK_THAT(spot->along, WithinAbs(2000.0, 1.0));
    CHECK_THAT(spot->up, WithinAbs(1500.0, 1.0));
    // From inside, or missing: nothing.
    CHECK_FALSE(building::spotAt(s, {0, 0, 1}, {0, -1, 0}));
    CHECK_FALSE(building::spotAt(s, {0, -100, 500}, {0, 1, 0}));  // over the walls
    // The right wall from outside.
    const auto right = building::spotAt(s, {200, 0, 40}, {-1, 0, 0});
    REQUIRE(right);
    CHECK(right->wall == 1);
    CHECK_THAT(right->along, WithinAbs(2000.0, 1.0));

    const BuildingOpening window = building::openingAt(s, OpeningKind::Window, *spot);
    CHECK(window.wall == 0);
    CHECK_THAT(window.bottom, WithinAbs(1500.0 - 600.0, 1e-6));  // its middle at the click
    const BuildingOpening door = building::openingAt(s, OpeningKind::Door, {0, 200.0, 900.0});
    CHECK(door.bottom == 0.0);
    CHECK(door.along > 200.0);  // kept clear of the corner

    const double plain = volumeOf(building::make(s).walls);
    s.openings = {window, door};
    const building::Made made = building::make(s);
    CHECK(volumeOf(made.walls) < plain);  // the holes outweigh the frames
    // The frames stand out of the front.
    CHECK(boundsOf(made.walls).min[1] < -4000.0 / 2 * k - 0.5);
}

TEST_CASE("Buildings: added, changed and saved", "[building]")
{
    Document doc;
    CommandStack stack(doc);
    BuildingSpec s = building::defaults(WallMaterial::Stone);
    s.scale = 87;
    const ObjectId walls = building::add(stack, doc, s, "Building", 20.0, -10.0);
    REQUIRE(doc.find(walls));
    CHECK(doc.find(walls)->building);
    ObjectId roof = kInvalidId;
    for (ObjectId id : doc.topLevelIds())
        if (doc.find(id)->roofOf == walls)
            roof = id;
    REQUIRE(roof != kInvalidId);
    CHECK(doc.find(roof)->name == "Building roof");
    const Bounds b = doc.worldBounds(walls);
    CHECK_THAT(double(b.center()[0]), WithinAbs(20.0, 0.1));
    CHECK_THAT(double(b.center()[1]), WithinAbs(-10.0, 0.5));
    CHECK(stack.count() == 1);

    // Changed: bigger, the roof joined in; where it was.
    BuildingSpec t = *doc.find(walls)->building;
    t.width = 9000;
    t.roofSeparate = false;
    building::update(stack, doc, walls, t);
    CHECK(doc.find(roof) == nullptr);
    CHECK_THAT(double(doc.worldBounds(walls).size()[0]), WithinAbs((9000.0 + 600.0) / 87, 0.1));
    CHECK_THAT(double(doc.worldBounds(walls).center()[0]), WithinAbs(20.0, 0.1));
    stack.undo();
    REQUIRE(doc.find(roof));
    CHECK(doc.find(walls)->building->width == 6000.0);

    // Saved and opened: the description comes too.
    const auto path = std::filesystem::temp_directory_path() / "forge_building_test.f3d";
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    REQUIRE(other.find(walls));
    CHECK(other.find(walls)->building == doc.find(walls)->building);
    CHECK(other.find(roof)->roofOf == walls);

    BuildingSpec bad = s;
    bad.wallThickness = 3000;
    CHECK_THROWS_AS(building::make(bad), std::invalid_argument);
}
TEST_CASE("Buildings: doors and windows kept apart", "[building]")
{
    BuildingSpec s = building::defaults(WallMaterial::Brick);
    BuildingOpening door;
    door.kind = OpeningKind::Door, door.wall = 0, door.along = 2000, door.width = 900, door.height = 2000, door.bottom = 0;
    s.openings = {door};
    BuildingOpening window;
    window.wall = 0, window.along = 2500, window.bottom = 900, window.width = 900, window.height = 1200;
    CHECK(building::clashes(s, window));
    window.along = 4000;
    CHECK_FALSE(building::clashes(s, window));
    window.wall = 1;
    window.along = 2000;
    CHECK_FALSE(building::clashes(s, window));  // another wall
    CHECK_FALSE(building::clashes(s, door, 0));  // itself
}
