// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>

// A tree's description (Forge3D Scenery's tree designer, slice 114): kept on the shape it
// made, so it can be changed and made again. Sizes are real-world millimetres; `scale`
// turns them into the model's (1:scale).
namespace forge {

// Broadleaf: spreading branches, a rounded crown (an oak). Conifer: a straight trunk and
// tiers of drooping branches (a pine or fir). Dead: bare, gnarled branches. Palm: a tall
// curved trunk with fronds at the top.
enum class TreeKind { Broadleaf, Conifer, Dead, Palm };

struct TreeSpec {
    double scale = 35.0;
    TreeKind kind = TreeKind::Broadleaf;
    double height = 9000.0;         // to the top of the crown
    double crownWidth = 7000.0;     // across the crown (a palm's: its fronds)
    double trunkDiameter = 600.0;   // at the bottom, above the roots
    double branches = 0.5;          // 0 few, 1 many
    bool leaves = true;             // foliage clumps (never on a dead tree)
    double foliage = 0.5;           // how full the clumps are: 0 sparse, 1 full
    double lean = 0.0;              // degrees from upright
    std::uint32_t seed = 1;         // which tree of this kind
    double smallest = 0.4;          // model mm: the finest detail the printer makes
    std::array<double, 3> offset{0.0, 0.0, 0.0};  // as BuildingSpec::offset
    bool operator==(const TreeSpec&) const = default;
};

}  // namespace forge
