// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Sketch.h"

#include <optional>
#include <vector>

// Sketch editing tools: round a corner, trim a piece off, offset an outline, mirror.
// Each changes the sketch directly and adds the rules that keep the result in shape;
// solve afterwards. When it can't be done, each throws std::invalid_argument with a
// message for the user and leaves the sketch as it was.
namespace forge::sketch {

// Rounds the corner at `point`, where two lines meet, with an arc of `radius` that
// touches both (the lines are shortened to it). The corner point stays, joined to the
// arc's ends by helper lines, so rules and dimensions on it still hold; a length on
// either line becomes the distance from its far end to the corner.
void roundCorner(Sketch& s, int point, double radius);

// The piece of curve `geo` around `at` (sketch coordinates) between the nearest places
// where other curves cross or touch it (or its ends): what trim would take away.
struct TrimPiece {
    int geo = -1;
    std::vector<Point> path;  // along the piece, for showing it
};
std::optional<TrimPiece> trimPiece(const Sketch& s, int geo, Point at);

// Takes that piece away. A curve with nothing crossing it goes altogether; a piece in
// the middle splits it in two (kept in line, or on the same circle). New ends stay on
// the curve that cut them.
void trim(Sketch& s, int geo, Point at);

// Copies the outlines that `geos` are part of (whole chains of joined lines and arcs,
// and circles) `distance` away: outward from a closed outline (negative: inward), to
// the left of an open one (going from its first point). Lines stay parallel to the
// originals and arcs / circles share their centres, and Gap dimensions hold the
// distance (one for each run of smoothly joined pieces). Returns the new curves.
std::vector<int> offset(Sketch& s, const std::vector<int>& geos, double distance);

// Copies `geos` and the loose `points` mirrored about line `axis`. Each copied point
// is held mirror-image to its original (Symmetric rules); points on the axis are
// shared, and loose ends all but on it are put on it. Returns the new curves.
std::vector<int> mirror(Sketch& s, const std::vector<int>& geos, const std::vector<int>& points, int axis);

// Loose ends of `geos` that are on `line`, or all but (within 3% of their size), are
// put on it with a rule keeping them there. Mirror does this itself.
void endsOntoLine(Sketch& s, const std::vector<int>& geos, int line);

// A line leaning a little (up to 10 degrees) from upright or level is made so, with a
// Vertical / Horizontal rule; the ends of `geos` on it go with it. Returns whether it
// did anything. Solve afterwards (other rules may not allow it).
bool straighten(Sketch& s, const std::vector<int>& geos, int line);

}  // namespace forge::sketch
