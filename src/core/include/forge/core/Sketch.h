// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <utility>
#include <vector>

// 2D sketches: points, lines, circles and arcs on a plane, held in shape by dimensions
// and geometric rules (constraints), solved numerically. A Pad (PadSpec) pushes the
// closed outlines of a sketch up into a solid.
namespace forge::sketch {

struct Point {
    double x = 0.0;
    double y = 0.0;
    bool operator==(const Point&) const = default;
};

enum class GeoKind {
    Line,    // a = start point, b = end point
    Circle,  // a = centre point, r = radius (index into Sketch::radii)
    Arc,     // a = centre, b = start, c = end: anticlockwise from start to end
};

struct Geo {
    GeoKind kind = GeoKind::Line;
    int a = -1, b = -1, c = -1;
    int r = -1;
    bool construction = false;  // a helper line: kept by the rules, not part of the outline
    bool operator==(const Geo&) const = default;
};

// What each rule refers to (points and geometry by index):
enum class Rule {
    Horizontal,     // a: line (or a, b: two points level with each other)
    Vertical,       // a: line (or a, b: two points above each other)
    Parallel,       // a, b: lines
    Perpendicular,  // a, b: lines
    Equal,          // a, b: lines (same length) or circles / arcs (same radius)
    Tangent,        // a, b: a line and a circle / arc, or two circles / arcs (value: 1 outside, -1 inside)
    PointOn,        // a: point, b: line / circle / arc it lies on
    Coincident,     // a, b: points in the same place
    Symmetric,      // a, b: points mirrored about line c
    Midpoint,       // c: point half way between points a and b
    Fixed,          // a: point locked at (value, value2)
    Distance,       // a, b: points `value` apart; or a: line `value` long (b = -1)
    DistanceX,      // a, b: points `value` apart across (signed, b - a)
    DistanceY,      // a, b: points `value` apart up (signed, b - a)
    Radius,         // a: circle / arc of radius `value`
    Angle,          // a, b: lines at `value` degrees (a to b, anticlockwise); b = -1: a from across
};

struct Constraint {
    Rule rule = Rule::Coincident;
    int a = -1, b = -1, c = -1;
    double value = 0.0;
    double value2 = 0.0;
    bool operator==(const Constraint&) const = default;
};

// Point 0 is the sketch's origin: always there, never moves.
struct Sketch {
    std::vector<Point> points{Point{}};
    std::vector<double> radii;
    std::vector<Geo> geos;
    std::vector<Constraint> constraints;
    bool operator==(const Sketch&) const = default;
};

bool isDimension(Rule rule);  // has a value the user types (distance, radius, angle)

// ---- Solving ----

struct Solved {
    bool ok = true;            // every rule holds
    int freedom = 0;           // how many ways it can still move (0 = fully held)
    std::vector<int> failing;  // rules that couldn't be met (they conflict)
};

// Moves points and radii so every rule holds, changing as little as possible. While
// dragging, `pull` gives points to pull towards spots (as far as the rules allow).
Solved solve(Sketch& sketch, const std::vector<std::pair<int, Point>>& pull = {});

// How many ways it can still move, without moving anything.
int freedom(const Sketch& sketch);

// The radius of a circle or arc.
double radiusOf(const Sketch& sketch, const Geo& g);

// ---- Editing helpers ----

// Adds a point and returns its index.
int addPoint(Sketch& s, Point p);
// Joins point `from` into point `into`: everything using `from` uses `into`, and `from`
// is removed. Returns the index `into` has afterwards.
int mergePoints(Sketch& s, int into, int from);
// Removes geometry (and rules that refer to it, and points nothing uses any more).
void removeGeos(Sketch& s, std::vector<int> geos);
// Removes rules.
void removeConstraints(Sketch& s, std::vector<int> rules);

// Stretches the sketch (its points, radii and dimensions). Returns false (and changes
// nothing) if it can't be stretched like that: circles, arcs, lengths along slopes
// and angles only scale evenly.
bool scale(Sketch& s, double sx, double sy);

// ---- Outlines ----

// Ends of lines and arcs not joined to anything (the outline isn't closed there).
std::vector<int> looseEnds(const Sketch& s);

// A short description of a rule, e.g. "Horizontal", "Length 20 mm", "R5".
std::string describe(const Sketch& s, const Constraint& c);

}  // namespace forge::sketch
