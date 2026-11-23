// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Balance.h"
#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <optional>
#include <vector>

// Display bases (slice 76): a base for a figure in the pose it's in, sized round where
// it touches the ground, with pegs standing up into the stand peg holes in its feet (or
// holes for pegs on its feet), each right under its foot.
namespace forge {

class CommandStack;
class Document;

namespace displaybase {

enum class Shape { Round, Oval, Square, Hexagon };
inline constexpr int kShapeCount = 4;

struct Settings {
    Shape shape = Shape::Round;
    double margin = 8.0;     // mm beyond the feet (and the centre of weight) all round
    double thickness = 4.0;
    double bevel = 1.0;      // the top edge taken off
    bool pegs = true;        // pegs standing up (false: holes going down, for pegs on the feet)
    double pegSize = 3.0;    // diameter
    double pegHeight = 3.5;  // how far a peg stands up (or a hole goes down)
    double fit = 0.1;        // holes: room all round a foot's peg
};

// Throws std::invalid_argument (in plain words) if it can't be made.
void check(const Settings& s);
// "Round", "Oval"...
const char* title(Shape s);

// The base for this balance (as posed): its top at the ground, under the feet, with a
// peg (or a hole) at each of `spots` (posed: straight under each foot). Closed.
Mesh make(const Settings& s, const balance::Balance& b, const std::vector<Vec3>& spots);

}  // namespace displaybase

// The posed middles of the feet that have room for a peg (as balance::pegSpots, but where
// they are as posed).
std::vector<Vec3> footSpots(const balance::Balance& b);

// Stand peg holes (Socket steps, `peg`) straight up into the feet at `spots`, where there
// isn't a hole there already; two on the same part go one into the other's step. Call
// inside a macro. Returns how many were made.
std::size_t addFootHoles(CommandStack& stack, Document& doc, const std::vector<balance::PegSpot>& spots,
                         const SocketFinish& peg);

// A base made before (named "Display base...").
bool isDisplayBase(const Document& doc, ObjectId id);

// Adds the base ("Display base") as one undo step, and (`holesInFeet`) the stand peg
// holes in the feet that don't have one there yet, as Socket steps (`peg`). `parts`: the
// parts as posed (any base made before is left out). Returns the base.
ObjectId addDisplayBase(CommandStack& stack, Document& doc, const std::vector<pose::PosedPart>& parts,
                        const displaybase::Settings& s, const std::optional<SocketFinish>& holesInFeet);

}  // namespace forge
