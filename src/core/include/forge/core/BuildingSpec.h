// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <vector>

// A building's description (Forge3D Scenery's building designer, slice 111): kept on the
// shape it made, so it can be changed and made again. Sizes are real-world millimetres;
// `scale` turns them into the model's (1:scale).
namespace forge {

// RoughStone: roughly squared, rock-faced stones in courses of varying height. Fieldstone:
// rounded stones of every shape set in wide mortar (a "giraffe" stone house). (Slice 116.)
enum class WallMaterial { Brick, Stone, Logs, Planks, RoughStone, Fieldstone };
enum class RoofKind { None, Flat, Gable, Hip };
enum class OpeningKind { Door, Window };
// The footprint (slice 115). L: a wing along the front and one up the left side. T: a bar
// along the back and a stem down the middle to the front. U: a bar along the back and an
// arm down each side, open to the front. Courtyard: wings all round an open middle.
enum class BuildingShape { Rectangle, L, T, U, Courtyard };

// A door or window in a wall. The walls are the footprint's outline's sides, counter-
// clockwise seen from above, from the front-left corner: a rectangle's 0 the front (-Y),
// 1 the right (+X), 2 the back (+Y), 3 the left (-X); then a courtyard's inner walls. `along`: its middle's distance from the wall's
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
    BuildingShape shape = BuildingShape::Rectangle;
    // The wings' sizes (not a rectangle's): `wingDepth` front to back (the L's front wing,
    // the T's and U's back bar, the courtyard's front and back wings); `wingWidth` across
    // (the L's side wing, the T's stem, the U's arms, the courtyard's sides).
    double wingDepth = 3000.0, wingWidth = 3000.0;
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
