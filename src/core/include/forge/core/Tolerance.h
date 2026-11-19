// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <string>
#include <vector>

// Tolerance test print: small bars with holes, cups and pin holes at stepped fits, each
// labelled, and the pegs, ball and pins to try in them. Printed once, the fit that
// works best becomes the starting fit for Quick Socket and Quick Hinge.
namespace forge::tolerance {

enum class Kind { Peg, Ball, Pin };

struct Settings {
    bool pegs = true, balls = true, pins = true;
    double pegSize = 3.0, pegDepth = 4.0;  // a round peg, and how far it goes in
    double ballSize = 6.0;                 // a snap-in ball (Quick Socket's grip)
    double grip = 10.0;
    double pinSize = 2.0;                  // a hinge pin, through a hole
    std::vector<double> fits{0.10, 0.15, 0.20, 0.25, 0.30};
};

// The fits from `from` to `to` (both included) every `step`, rounded to 0.01 mm.
// Throws std::invalid_argument if there'd be none or more than 12.
std::vector<double> steps(double from, double to, double step);

// Throws std::invalid_argument (in plain words) if the settings can't make a test.
void check(const Settings& s);

// The shapes, laid out round the plate's middle, each a closed surface standing on the
// plate: a bar per kind ("Peg fit test .10–.30"...) and the pieces to try in them.
struct Piece {
    std::string name;
    Mesh mesh;
};
std::vector<Piece> pieces(const Settings& s);

// A fit as its label reads: ".15" for 0.15 mm, "1.05" past a millimetre.
std::string label(double fit);
// Raised seven-segment digits (and points) for `text`, `height` tall, standing `raise`
// mm up from z = 0, left end at x = 0, middle at y = 0. Just digits, '.' and '-'.
Mesh digits(const std::string& text, double height, double raise);

}  // namespace forge::tolerance
