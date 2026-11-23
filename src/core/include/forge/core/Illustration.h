// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/PartsList.h"

#include <cstdint>
#include <vector>

// Instruction sheets (slice 78): a picture of the parts pulled apart (an exploded view),
// drawn flat-shaded and outlined like a kit's instructions, with a numbered balloon
// pointing at each part, the numbers matching the Parts List.
namespace forge {

class Document;

namespace illustrate {

enum class View { Front, ThreeQuarter, Left, Right, Back, Top };
inline constexpr int kViewCount = 6;
const char* title(View v);

struct Item {
    Mesh mesh;                 // world coordinates (positions and indices)
    std::uint32_t colour = 0x8a9bb0;
    int number = 0;            // its balloon (0: none)
};

// A balloon: the number, the point on the part it points at, and where it sits.
struct Label {
    int number = 0;
    double ax = 0, ay = 0;  // the point (pixels)
    double bx = 0, by = 0;  // the balloon's middle (pixels)
};

struct Picture {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels;  // 0xAARRGGBB, row by row; background transparent
    std::vector<Label> labels;
    double balloon = 0.0;  // the balloons' radius, pixels
};

// Draws `items` seen from `view`, fitted into width × height pixels with room round
// them for the balloons. Throws std::invalid_argument for a size out of range.
Picture render(const std::vector<Item>& items, View view, int width, int height);

// The parts of the Parts List rows (those with a number), each as it is in the world,
// moved out from the middle of them all by `spread` (0: as they are; 1: each one's
// distance from the middle doubled).
std::vector<Item> exploded(const Document& doc, const std::vector<parts::Row>& rows, double spread);

}  // namespace illustrate
}  // namespace forge
