// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <vector>

// A terrain piece's description (Forge3D Scenery's terrain designer, slice 113): kept on
// the shape it made, so it can be changed and made again. Ground sizes are real-world
// millimetres (`scale` turns them into the model's); the base's are the model's own.
namespace forge {

enum class TerrainBase { Rectangle, Round };  // Round: a circle or an oval
enum class TerrainEdge { Plain, Bevelled };
enum class TerrainFeatureKind { Crater, Crag, Path };

// Something on the ground, at (x, y) from the base's middle (real mm). A path runs from
// there to (x2, y2).
struct TerrainFeature {
    TerrainFeatureKind kind = TerrainFeatureKind::Crater;
    double x = 0.0, y = 0.0;
    double x2 = 0.0, y2 = 0.0;
    double size = 2000.0;   // a crater's or crag's across; a path's width
    double depth = 600.0;   // a crater's depth; a crag's height; how far a path is sunk
    bool scattered = false;  // put there by Scatter (Scatter again replaces it)
    bool operator==(const TerrainFeature&) const = default;
};

struct TerrainSpec {
    double scale = 35.0;
    TerrainBase base = TerrainBase::Rectangle;
    double width = 8000.0, depth = 6000.0;  // along X, along Y
    double baseThickness = 3.0;  // model mm: the flat slab under the ground
    TerrainEdge edge = TerrainEdge::Plain;
    double bevel = 3.0;          // model mm: how far a bevelled edge slopes in
    // The ground: rolling hills up to `hillHeight`, about `hillSize` across, roughened.
    double hillHeight = 1200.0;
    double hillSize = 6000.0;
    double roughness = 0.4;
    bool levelEdges = true;  // the ground meets the edge at the base's height (pieces line up)
    std::vector<TerrainFeature> features;
    std::uint32_t seed = 1;
    double smallest = 0.4;  // model mm
    std::array<double, 3> offset{0.0, 0.0, 0.0};  // as BuildingSpec::offset
    bool operator==(const TerrainSpec&) const = default;
};

}  // namespace forge
