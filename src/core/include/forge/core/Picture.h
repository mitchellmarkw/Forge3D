// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Text.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

// Pictures for Emboss: logos traced to outlines, photos turned into reliefs.
namespace forge::picture {

// The longest side a picture is kept at (pixels); bigger ones are shrunk to it.
inline constexpr int kMaxSide = 1000;

// A picture's grey pixels from straight RGBA bytes (row by row from the top),
// transparent parts made white, shrunk to at most kMaxSide on its longest side.
std::shared_ptr<const PictureData> fromRgba(int width, int height, const std::vector<std::uint8_t>& rgba);

// How tall the picture is on the surface, mm (its width is picture.width).
double heightOf(const EmbossPicture& picture);

// A logo's outlines: closed loops (first point not repeated) round the parts darker
// than `threshold` (lighter, if `invert`), in pixels: x to the right, y down, the
// middle of the top-left pixel at (0.5, 0.5). Smoothed (the outline runs between
// pixels at the threshold's shade) and simplified to within `tolerance` pixels.
// Specks smaller than `minArea` square pixels are left out.
std::vector<std::vector<std::array<double, 2>>> outlines(const PictureData& image, int threshold, bool invert,
                                                         double tolerance = 0.35, double minArea = 4.0);

// The picture as flat pieces for embossing (see text::FlatPiece): centred on the
// origin, `width` across, turned `angle`, no edge longer than maxEdge. A logo is its
// traced outlines; a relief is one sheet whose points carry heights (0..1).
// Throws GeometryError if there is nothing to emboss (or far too much detail).
std::vector<text::FlatPiece> flatPieces(const EmbossPicture& picture, double maxEdge);

}  // namespace forge::picture
