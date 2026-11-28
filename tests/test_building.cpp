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
    CHECK(doc.find(walls)->building->width == 9000.0);  // its description too
    CHECK_FALSE(doc.find(walls)->building->roofSeparate);
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

TEST_CASE("Buildings: L, T, U and courtyard shapes", "[building]")
{
    BuildingSpec base = building::defaults(WallMaterial::Brick);
    base.width = 10000;
    base.depth = 8000;
    base.roof = RoofKind::Flat;
    base.overhang = 0;
    const double k = 1.0 / base.scale;
    const auto plainVolume = [&](BuildingSpec s) {
        s.roof = RoofKind::None;
        return meshkernel::volume(meshkernel::fromTriangles(building::make(s, true).walls.positions,
                                                            building::make(s, true).walls.indices));
    };
    struct Case {
        BuildingShape shape;
        int walls;
        std::size_t wings, corners;
    };
    for (const Case c : {Case{BuildingShape::L, 6, 2, 5}, Case{BuildingShape::T, 8, 2, 6}, Case{BuildingShape::U, 8, 3, 6},
                         Case{BuildingShape::Courtyard, 8, 4, 8}}) {
        INFO(building::title(c.shape));
        const BuildingSpec s = building::withShape(base, c.shape);
        CHECK(building::wallCount(s) == c.walls);
        CHECK(building::wings(s).size() == c.wings);
        CHECK(building::corners(s).size() == c.corners);
        // The footprint's area: the walls' length times their thickness, about.
        double length = 0;
        for (int w = 0; w < c.walls; ++w)
            length += building::face(s, w).length;
        const double t = s.wallThickness * k, h = s.height * k;
        CHECK_THAT(plainVolume(s), WithinRel(length * t * h, 0.12));
        const building::Made made = building::make(s);
        REQUIRE_FALSE(made.walls.indices.empty());
        REQUIRE_FALSE(made.roof.indices.empty());
        const Bounds b = made.walls.bounds();
        CHECK_THAT(double(b.max[0] - b.min[0]), WithinAbs(s.width * k, 0.3));
        CHECK_THAT(double(b.max[1] - b.min[1]), WithinAbs(s.depth * k, 0.3));
        // Every wall takes a window.
        BuildingSpec windows = s;
        for (int w = 0; w < c.walls; ++w) {
            const building::Face f = building::face(s, w);
            if (f.length * s.scale > 2000)
                windows.openings.push_back(building::openingAt(s, OpeningKind::Window, {w, f.length * s.scale / 2, 1500}));
        }
        CHECK(windows.openings.size() >= 4);
        CHECK_NOTHROW(building::make(windows, true));
        // Gable and hip roofs over the wings, joined.
        for (RoofKind r : {RoofKind::Gable, RoofKind::Hip}) {
            BuildingSpec roofed = s;
            roofed.roof = r;
            roofed.overhang = 300;
            CHECK_FALSE(building::make(roofed, true).roof.indices.empty());
        }
        // Logs and planks follow the shape too.
        for (WallMaterial m : {WallMaterial::Logs, WallMaterial::Planks, WallMaterial::Stone}) {
            BuildingSpec other = building::withShape(building::defaults(m, base), c.shape);
            CHECK_NOTHROW(building::make(other));
        }
    }
    // Courtyard: the middle's open (a ray straight down there meets no wall).
    const BuildingSpec court = building::withShape(base, BuildingShape::Courtyard);
    CHECK_FALSE(building::spotAt(court, {0, 0, 50}, {0, 0, -1}));
    CHECK(building::wallName(court, 4).rfind("inner", 0) == 0);
    // Dragging corners: the outside one widens it; the L's inside one sets its wings.
    BuildingSpec l = building::withShape(base, BuildingShape::L);
    BuildingSpec wider = building::movedCorner(l, 1, 6000, -4000);
    CHECK(wider.width == 12000);
    CHECK(wider.wingWidth == l.wingWidth);
    BuildingSpec wings = building::movedCorner(l, 4, -5000 + 2500, -4000 + 3500);
    CHECK(wings.wingWidth == 2500);
    CHECK(wings.wingDepth == 3500);
    // Kept buildable: a wing can't be thinner than 2.5 walls.
    BuildingSpec thin = building::movedCorner(l, 4, -5000 + 100, -4000 + 100);
    CHECK(thin.wingWidth >= 2.5 * thin.wallThickness);
    BuildingSpec bad = l;
    bad.wingWidth = 50;
    CHECK_THROWS_AS(building::make(bad), std::invalid_argument);
    // A post at each corner, found by a ray down it.
    const auto corner = building::corners(l)[4];
    double z = 0;
    const auto picked = building::pickedCorner(l, {corner[0] * k + 0.1, corner[1] * k, 200}, {0, 0, -1}, &z);
    REQUIRE(picked);
    CHECK(*picked == 4);
    CHECK_FALSE(building::cornerMarker(l, 4).indices.empty());
    // Saved and read back.
    const auto path = std::filesystem::temp_directory_path() / "forge_building_shapes.f3d";
    Document doc;
    CommandStack stack(doc);
    BuildingSpec u = building::withShape(base, BuildingShape::U);
    u.openings.push_back(building::openingAt(u, OpeningKind::Door, {6, 2000, 0}));
    const ObjectId id = building::add(stack, doc, u, "U", 0, 0);
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    CHECK(other.find(id)->building == doc.find(id)->building);
    CHECK(other.find(id)->building->shape == BuildingShape::U);
}

TEST_CASE("Buildings: rough-hewn stone and fieldstone", "[building]")
{
    for (WallMaterial m : {WallMaterial::RoughStone, WallMaterial::Fieldstone}) {
        INFO(building::title(m));
        BuildingSpec s = building::defaults(m);
        s.roof = RoofKind::None;
        const double k = 1.0 / s.scale;
        const auto start = std::chrono::steady_clock::now();
        const building::Made made = building::make(s);
        const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        INFO("made in " << took << " s, " << made.walls.triangleCount() << " triangles");
        CHECK(took < 30.0);
        const MeshSolid walls = meshkernel::fromTriangles(made.walls.positions, made.walls.indices);
        CHECK(meshkernel::volume(walls) > 0);
        // The stones stand out of the face, a little.
        const Bounds b = made.walls.bounds();
        CHECK(double(b.max[0] - b.min[0]) > s.width * k);
        CHECK(double(b.max[0] - b.min[0]) < s.width * k + 2 * 0.3 * s.wallThickness * k);
        CHECK_THAT(double(b.min[2]), WithinAbs(0.0, 0.01));
        // Another seed, other stones; the quick one plain.
        BuildingSpec other = s;
        other.seed = 9;
        CHECK(std::abs(meshkernel::volume(meshkernel::fromTriangles(building::make(other).walls.positions,
                                                                    building::make(other).walls.indices)) -
                       meshkernel::volume(walls)) > 1e-3);
        CHECK(building::make(s, true).walls.triangleCount() < made.walls.triangleCount() / 10);
        // With a door, an L, and dry-laid.
        BuildingSpec more = building::withShape(s, BuildingShape::L);
        more.openings.push_back(building::openingAt(more, OpeningKind::Door, {0, 2000, 0}));
        more.mortar = false;
        CHECK_NOTHROW(building::make(more));
        // Too small to show: plain, with a note.
        BuildingSpec tiny = s;
        tiny.scale = 220;
        const building::Made plain = building::make(tiny);
        CHECK_FALSE(plain.notes.empty());
    }
}

TEST_CASE("Buildings: battlements and towers", "[building]")
{
    BuildingSpec s = building::defaults(WallMaterial::Stone);
    s.width = 10000;
    s.depth = 8000;
    s.roof = RoofKind::Flat;
    const double k = 1.0 / s.scale;
    const auto volumeOf = [](const Mesh& m) { return meshkernel::volume(meshkernel::fromTriangles(m.positions, m.indices)); };
    const double plain = volumeOf(building::make(s, true).walls);
    // Battlements: the walls up past the roof by the parapet, notched.
    BuildingSpec b = s;
    b.battlements = true;
    const building::Made made = building::make(b, true);
    CHECK_THAT(double(made.walls.bounds().max[2]), WithinAbs((s.height + s.parapetHeight) * k, 0.05));
    const double up = volumeOf(made.walls);
    CHECK(up > plain);
    CHECK(up < plain * (s.height + s.parapetHeight) / s.height);  // less: the crenels
    // The roof inside the parapet, at the walls' old top.
    REQUIRE_FALSE(made.roof.indices.empty());
    const Bounds rb = made.roof.bounds();
    CHECK(double(rb.max[0] - rb.min[0]) < s.width * k - 1.9 * s.wallThickness * k);
    CHECK_THAT(double(rb.min[2]), WithinAbs(s.height * k, 0.05));
    // Towers: snapped to a corner, or onto a wall's middle line.
    const BuildingTower corner = building::tower(b, TowerShape::Round, 4800, 3900);
    CHECK(corner.x == 5000);
    CHECK(corner.y == 4000);
    const BuildingTower onWall = building::tower(b, TowerShape::Square, 0, -3700);
    CHECK_THAT(onWall.y, WithinAbs(-4000 + s.wallThickness / 2, 1e-6));
    CHECK(onWall.height > s.height + s.parapetHeight);
    const auto alone = building::snapTower(b, 3000, 20000, 0);
    CHECK(alone[0] == 20000);
    b.towers = {corner, onWall};
    b.towers[1].top = TowerTop::Roof;
    for (bool quick : {true, false}) {
        INFO("quick " << quick);
        const building::Made withTowers = building::make(b, quick);
        CHECK(double(withTowers.walls.bounds().max[2]) >= (corner.height + s.parapetHeight) * k - 0.1);
        CHECK(double(withTowers.roof.bounds().max[2]) > onWall.height * k + 0.5 * onWall.size * k);  // its pyramid
        CHECK(volumeOf(withTowers.walls) > up);
    }
    // Every material, a round and a square tower.
    for (WallMaterial m : {WallMaterial::Brick, WallMaterial::Logs, WallMaterial::Planks, WallMaterial::RoughStone,
                           WallMaterial::Fieldstone}) {
        BuildingSpec other = building::defaults(m, b);
        other.towers = {building::tower(other, TowerShape::Round, -5000, -4000),
                        building::tower(other, TowerShape::Square, 5000, -4000)};
        CHECK_NOTHROW(building::make(other));
    }
    // Picked by a ray down through it.
    double z = 0;
    const auto picked = building::pickedTower(b, {corner.x * k, corner.y * k, 400}, {0, 0, -1}, &z);
    REQUIRE(picked);
    CHECK(*picked == 0);
    CHECK_FALSE(building::towerMarker(b, 0).indices.empty());
    // Checked; saved and read back.
    BuildingSpec bad = b;
    bad.towers[0].size = 100;
    CHECK_THROWS_AS(building::make(bad), std::invalid_argument);
    Document doc;
    CommandStack stack(doc);
    const ObjectId id = building::add(stack, doc, b, "Keep", 0, 0);
    const auto path = std::filesystem::temp_directory_path() / "forge_building_towers.f3d";
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    CHECK(other.find(id)->building == doc.find(id)->building);
    CHECK(other.find(id)->building->towers.size() == 2);
}

TEST_CASE("Buildings: towers in their own material", "[building]")
{
    BuildingSpec s = building::defaults(WallMaterial::Brick);
    s.width = 10000;
    s.depth = 8000;
    s.roof = RoofKind::Flat;
    s.battlements = true;
    const auto volumeOf = [](const Mesh& m) { return meshkernel::volume(meshkernel::fromTriangles(m.positions, m.indices)); };
    for (TowerShape shape : {TowerShape::Round, TowerShape::Square}) {
        INFO(building::title(shape));
        BuildingSpec b = s;
        b.towers = {building::tower(b, shape, 5000, 4000)};
        const double asBuilding = volumeOf(building::make(b).walls);
        // Each other material: made, and different.
        for (WallMaterial m : {WallMaterial::Stone, WallMaterial::RoughStone, WallMaterial::Fieldstone, WallMaterial::Logs,
                               WallMaterial::Planks}) {
            INFO(building::title(m));
            BuildingSpec other = b;
            other.towers[0].material = m;
            const auto start = std::chrono::steady_clock::now();
            const double v = volumeOf(building::make(other).walls);
            CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(30));
            CHECK(std::abs(v - asBuilding) > 1e-3);
        }
        // The building's own material named: the same as none.
        BuildingSpec same = b;
        same.towers[0].material = WallMaterial::Brick;
        CHECK_THAT(volumeOf(building::make(same).walls), WithinRel(asBuilding, 1e-9));
    }
    // A fieldstone building's round tower is fieldstone too (not blocks): a rough face round it.
    BuildingSpec field = building::defaults(WallMaterial::Fieldstone, s);
    field.towers = {building::tower(field, TowerShape::Round, 5000, 4000)};
    CHECK_NOTHROW(building::make(field));
    // Saved and read back.
    BuildingSpec b = s;
    b.towers = {building::tower(b, TowerShape::Round, 5000, 4000), building::tower(b, TowerShape::Square, -5000, 4000)};
    b.towers[0].material = WallMaterial::Fieldstone;
    Document doc;
    CommandStack stack(doc);
    const ObjectId id = building::add(stack, doc, b, "Keep", 0, 0);
    const auto path = std::filesystem::temp_directory_path() / "forge_building_tower_material.f3d";
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    CHECK(other.find(id)->building == doc.find(id)->building);
    CHECK(other.find(id)->building->towers[0].material == WallMaterial::Fieldstone);
    CHECK_FALSE(other.find(id)->building->towers[1].material);
}
