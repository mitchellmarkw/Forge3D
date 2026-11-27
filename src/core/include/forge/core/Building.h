// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/BuildingSpec.h"
#include "forge/core/Mesh.h"

#include <optional>
#include <string>
#include <vector>

// Forge3D Scenery's building designer (slice 111): walls of brick, stone blocks, logs or
// planks, from real sizes at the model's scale, with doors and windows, and a roof.
namespace forge {

class CommandStack;
class Document;

namespace building {

// A material's usual sizes (real mm): units, joint, wall thickness.
BuildingSpec defaults(WallMaterial m, const BuildingSpec& from = {});
const char* title(WallMaterial m);  // "Brick", "Stone blocks"...
const char* title(RoofKind r);
const char* title(BuildingShape s);

// The footprint (real mm, the building's middle at 0, 0): its outline counter-clockwise from
// the front-left corner, then a courtyard's inner outline (clockwise). Its sides are the walls.
std::vector<std::vector<std::array<double, 2>>> outlines(const BuildingSpec& s);
int wallCount(const BuildingSpec& s);
// A name for a wall ("front", "right"..., or "inner front"...).
std::string wallName(const BuildingSpec& s, int wall);
// The wings (real mm: x0, y0, x1, y1), rectangles that make up the footprint: each gets its
// own roof.
std::vector<std::array<double, 4>> wings(const BuildingSpec& s);
// A shape's usual wing sizes for this width and depth.
BuildingSpec withShape(const BuildingSpec& s, BuildingShape shape);
// The wing sizes kept to what can be built (each wing at least 2.5 walls thick, an open
// middle at least 500 mm across); openings on walls it hasn't got dropped.
BuildingSpec kept(BuildingSpec s);

// The footprint's corners that can be dragged (real mm): the four outside corners (width
// and depth) first, then the inner ones (the wings' sizes).
std::vector<std::array<double, 2>> corners(const BuildingSpec& s);
// The spec with corner `i` dragged to (x, y), real mm (kept buildable).
BuildingSpec movedCorner(const BuildingSpec& s, std::size_t i, double x, double y);
// A see-through post at a corner, up past the roof (model mm): to grab and drag.
Mesh cornerMarker(const BuildingSpec& s, std::size_t i);
// Which corner's post a ray (model mm) passes, if any, and how high (model mm).
std::optional<std::size_t> pickedCorner(const BuildingSpec& s, const std::array<double, 3>& origin,
                                        const std::array<double, 3>& direction, double* height = nullptr);

// Throws std::invalid_argument, in plain words, if it can't be made.
void check(const BuildingSpec& s);

// What's made, in model mm: the building centred on X = Y = 0, standing on Z = 0, its
// front to -Y. `notes`: what was changed to print (mortar widened...).
struct Made {
    Mesh walls;
    Mesh roof;  // empty: no roof, or it's in the walls
    std::vector<std::string> notes;
};
// `quick`: plain walls (no bricks, logs or planks), for a preview while dragging.
Made make(const BuildingSpec& s, bool quick = false);

// A wall's outside face, in model mm: its left end at the bottom, the way along it and
// out of it, its length and height.
struct Face {
    std::array<double, 3> start, along, out;
    double length = 0.0, height = 0.0;
};
Face face(const BuildingSpec& s, int wall);

// Where a ray (model mm, the building's coordinates) first meets a wall's outside face:
// the wall, and the point's distance along it and up it (real mm).
struct Spot {
    int wall = 0;
    double along = 0.0, up = 0.0;
};
std::optional<Spot> spotAt(const BuildingSpec& s, const std::array<double, 3>& origin,
                           const std::array<double, 3>& direction);
// An opening of this kind, its middle at the spot (a door standing on the floor), kept
// inside its wall.
BuildingOpening openingAt(const BuildingSpec& s, OpeningKind kind, const Spot& spot);
// Kept inside its wall, clear of the corners.
BuildingOpening keptIn(const BuildingSpec& s, BuildingOpening o);

// A thin plate just out from the wall where the opening is (model mm): to show which one
// is picked.
Mesh openingMarker(const BuildingSpec& s, const BuildingOpening& o);
// Whether an opening would overlap another (their frames included), leaving out the one
// at `skip` (itself, when it's being moved).
bool clashes(const BuildingSpec& s, const BuildingOpening& o, std::optional<std::size_t> skip = std::nullopt);
// Which opening a spot is on, if any (its index).
std::optional<std::size_t> pickedOpening(const BuildingSpec& s, const Spot& spot);

// The shapes to add (the walls first, then the roof if it's its own), placed so the
// building's middle is at (x, y) on the plate.
std::vector<Body> bodies(const BuildingSpec& s, const Made& made, const std::string& name, double x, double y);

// Adds a new building at (x, y) on the plate: one undo step. Returns the walls' id.
ObjectId add(CommandStack& stack, Document& doc, BuildingSpec s, const std::string& name, double x, double y);
// Makes the building `walls` again from `s` (its roof added, changed or removed to
// match), where it is: one undo step.
void update(CommandStack& stack, Document& doc, ObjectId walls, BuildingSpec s);

}  // namespace building
}  // namespace forge
