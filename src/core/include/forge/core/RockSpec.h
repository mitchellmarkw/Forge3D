// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>

// A rock's description (Forge3D Scenery's rock designer, slice 112): kept on the shape it
// made, so it can be changed and made again. Sizes are real-world millimetres; `scale`
// turns them into the model's (1:scale).
namespace forge {

// Boulder: rounded and worn. Crag: broken, flat-faced and sharp-edged. Slab: a flat
// stone. Pile: a heap of smaller rocks (scree, rubble).
enum class RockKind { Boulder, Crag, Slab, Pile };

struct RockSpec {
    double scale = 35.0;
    RockKind kind = RockKind::Boulder;
    // Its size before it's sunk: along X, along Y, up.
    double length = 1500.0, width = 1100.0, height = 900.0;
    double roughness = 0.5;  // 0 smooth, 1 very rough
    bool layers = false;     // bedding lines across it (sedimentary rock)
    double layerThickness = 150.0;
    double sunk = 0.2;       // how much of its height is in the ground (its base cut flat)
    int count = 8;           // a pile's rocks
    std::uint32_t seed = 1;  // which rock of this kind (a new one each)
    double smallest = 0.4;   // model mm: the finest detail the printer makes
    std::array<double, 3> offset{0.0, 0.0, 0.0};  // as BuildingSpec::offset
    bool operator==(const RockSpec&) const = default;
};

}  // namespace forge
