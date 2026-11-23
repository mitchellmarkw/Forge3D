// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Sewing patterns (slice 79): a painted area of a figure (a jacket, a cape) cut into
// pieces and laid flat, at full size, to cut cloth from.
namespace forge {

class Document;

namespace sewing {

// Where the pieces are cut apart before they're laid flat.
enum class Split {
    FrontBack,  // side seams: a front and a back
    LeftRight,  // a seam down the middle, front and back: a left and a right
    None,       // as it is (a cape, a skirt panel, anything that already lies open)
};
inline constexpr int kSplitCount = 3;
const char* title(Split s);

using P2 = std::array<double, 2>;

struct Piece {
    std::string name;                     // "Front", "Back", "Left 2"...
    std::vector<P2> points;               // laid flat, mm (y up the figure)
    std::vector<std::uint32_t> indices;   // its triangles
    std::vector<std::vector<P2>> outline;  // its edges (the stitching lines), outer first, smoothed
    double stretch = 0.0;                 // most lengths are within this (0.02: 2%)
    double worst = 0.0;                   // the most any length changed
    double width = 0.0, height = 0.0;
    P2 grain{0, 1};                       // the way up the figure, on the piece (always up)
};

struct Pattern {
    std::vector<Piece> pieces;
    std::string problem;  // what didn't go well (a piece that goes all the way round...), or empty
};

// The triangles marked `chosen` (one per triangle) cut apart by `split` and laid flat.
// Positions in mm, the figure standing (+Z up, facing -Y). Throws std::invalid_argument if
// nothing is chosen.
Pattern make(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices,
             const std::vector<bool>& chosen, Split split);

// The colours painted on a shape (as Paint lists them).
std::vector<std::uint32_t> paintedColours(const Document& doc, ObjectId top);
// The pattern of the area of `top` painted `colour`, as it stands on the plate.
Pattern fromPaint(const Document& doc, ObjectId top, std::uint32_t colour, Split split);

}  // namespace sewing
}  // namespace forge
