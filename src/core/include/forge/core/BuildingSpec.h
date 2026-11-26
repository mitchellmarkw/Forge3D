// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <vector>

// A building's description (Forge3D Scenery's building designer, slice 111): kept on the
// shape it made, so it can be changed and made again. Sizes are real-world millimetres;
// `scale` turns them into the model's (1:scale).
namespace forge {

enum class WallMaterial { Brick, Stone, Logs, Planks };
enum class RoofKind { None, Flat, Gable, Hip };
enum class OpeningKind { Door, Window };

// A door or window in a wall. Walls, seen from outside: 0 the front (-Y), 1 the right
// (+X), 2 the back (+Y), 3 the left (-X). `along`: its middle's distance from the wall's
// left end (as seen from outside); `bottom`: its sill's height (a door's: 0).
struct BuildingOpening {
    OpeningKind kind = OpeningKind::Window;
    int wall = 0;
    double along = 0.0;
    double bottom = 900.0;
    double width = 900.0;
    double height = 1200.0;
    bool operator==(const BuildingOpening&) const = default;
};

struct BuildingSpec {
    double scale = 35.0;  // 1:scale
    // Outside sizes: along X, along Y, the walls' height.
    double width = 6000.0, depth = 4000.0, height = 2800.0;
    WallMaterial material = WallMaterial::Brick;
    // The wall's units: a brick's or block's length, height and depth; a log's diameter
    // is unitHeight; a plank's showing width is unitHeight.
    double unitLength = 194.0, unitHeight = 57.0, unitDepth = 92.0;
    bool mortar = true;
    double joint = 10.0;          // the mortar between units
    double wallThickness = 230.0;  // logs: their diameter, whatever this says
    RoofKind roof = RoofKind::Gable;
    double pitch = 35.0;     // degrees
    double overhang = 300.0;  // past the walls, all round
    bool roofSeparate = true;  // the roof its own shape (it lifts off)
    std::vector<BuildingOpening> openings;
    std::uint32_t seed = 1;  // stone blocks' lengths
    double smallest = 0.4;   // model mm: the finest detail the printer makes
    // Where the shape's own origin is in the building's coordinates (set when it's made;
    // keeps a remade building where it was).
    std::array<double, 3> offset{0.0, 0.0, 0.0};
    bool operator==(const BuildingSpec&) const = default;
};

}  // namespace forge
