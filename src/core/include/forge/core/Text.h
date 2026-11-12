// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <TopoDS_Shape.hxx>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Letters from the fonts installed on the computer (through OpenCASCADE and FreeType).
namespace forge::text {

// The font families installed, sorted, without repeats. The first call looks through
// the system's fonts, which can take a second or two; later calls are instant.
// warmUp() does that first call (safe from another thread at startup).
std::vector<std::string> availableFonts();
void warmUp();
// The font used when none is named, or the named one isn't installed: Arial where
// there is one, else the first sans font found.
std::string defaultFont();
// The font that will actually be used for `name` (the default if it isn't installed).
std::string resolvedFont(const std::string& name);

// The words as flat faces in the XY plane, read from +Z: the left end of the first
// line's baseline at the origin, lines going down (-Y). emSize: the font size in mm.
// Throws std::invalid_argument for text with nothing to draw (empty or spaces only).
TopoDS_Shape letterFaces(const std::string& text, const std::string& font, bool bold, bool italic, double emSize);

// How tall a capital letter is for each mm of font size (about 0.7).
double capitalRatio(const std::string& font, bool bold, bool italic);

// The solid: letter faces stretched to width x depth (centred on X / Y), pushed up
// `height` from Z = 0.
TopoDS_Shape makeText(const TextSpec& spec);

// The same spec with its width and depth set so capitals are letterHeight tall and
// the letters keep their natural proportions.
TextSpec sizedForLetters(TextSpec spec, double letterHeight);
// The spec after changing the words, font or style: the letters keep the size and
// stretch they had, so the block grows or shrinks with the text.
TextSpec withText(const TextSpec& old, const std::string& text, const std::string& font, bool bold, bool italic);
// Capital-letter height of a text shape as it is (stretch included), mm.
double letterHeightOf(const TextSpec& spec);

// The letters as flat triangles, for embossing: (x, y) points and triangles
// anticlockwise from +Z, one closed outline group per letter piece, centred on the
// origin, capitals letterHeight tall, turned angleDegrees anticlockwise. No triangle
// edge is longer than maxEdge, so the letters can bend with a surface.
struct FlatPiece {
    std::vector<std::array<double, 2>> points;
    std::vector<std::uint32_t> triangles;
};
std::vector<FlatPiece> flatLetters(const EmbossText& words, double maxEdge);

}  // namespace forge::text
