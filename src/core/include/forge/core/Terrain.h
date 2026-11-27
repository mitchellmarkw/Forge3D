// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/TerrainSpec.h"

#include <optional>
#include <string>

// Forge3D Scenery's terrain designer (slice 113): a base (rectangle or round, plain or
// bevelled edge) with rolling hills, craters, crags and paths, from real sizes at the
// model's scale. Craters, crags and paths are placed by a click or scattered at random.
namespace forge {

class CommandStack;
class Document;

namespace terrain {

TerrainSpec defaults();
const char* title(TerrainFeatureKind k);
// A new feature of kind k at (x, y), sized for the spec (a path: from there to (x2, y2)).
TerrainFeature feature(const TerrainSpec& s, TerrainFeatureKind k, double x, double y, double x2 = 0, double y2 = 0);
// Throws std::invalid_argument, in plain words, if it can't be made.
void check(const TerrainSpec& s);

// Inside the base's outline? (real mm from its middle)
bool inside(const TerrainSpec& s, double x, double y);
// The ground's height there (model mm above the base's underside), crags left out.
double heightAt(const TerrainSpec& s, double x, double y);
// Where a ray (model mm, from the terrain's own origin) first meets the ground: real mm.
std::optional<std::array<double, 2>> hitAt(const TerrainSpec& s, const std::array<double, 3>& origin,
                                           const std::array<double, 3>& direction);
// The feature at a spot (real mm), if any: the nearest whose area it's in.
std::optional<std::size_t> pickedFeature(const TerrainSpec& s, double x, double y);
// Replaces the scattered features with new ones: craters, crags and paths, at random.
void scatter(TerrainSpec& s, int craters, int crags, int paths, std::uint32_t seed);

// In model mm: the base's middle at X = Y = 0, its underside on Z = 0. `quick`: coarser.
Mesh make(const TerrainSpec& s, bool quick = false);
// A see-through column round a feature, to show it's picked (model mm).
Mesh marker(const TerrainSpec& s, const TerrainFeature& f);

Body body(const TerrainSpec& s, const Mesh& made, const std::string& name, double x, double y);
ObjectId add(CommandStack& stack, Document& doc, TerrainSpec s, const std::string& name, double x, double y);
void update(CommandStack& stack, Document& doc, ObjectId id, TerrainSpec s);

}  // namespace terrain
}  // namespace forge
