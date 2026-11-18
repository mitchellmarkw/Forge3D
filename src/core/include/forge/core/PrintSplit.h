// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/core/Split.h"

#include <array>
#include <string>
#include <vector>

// Splitting a part so it can be printed in pieces: cuts with alignment pegs (or loose
// pins) so the pieces line up when glued, and a "smart" split that cuts a part that's
// too big for the printer into pieces that fit.
namespace forge {

class CommandStack;
class Document;

struct KeyOptions {
    bool keys = true;          // pegs (or pins) at all
    bool loosePins = false;    // holes in both pieces and separate pins, else pegs on one piece
    double diameter = 5.0;     // mm
    double length = 6.0;       // how far a peg stands out (a pin: its whole length)
    double taper = 2.0;        // degrees the peg's sides lean in towards its tip
    double clearance = 0.2;    // added to the hole's radius
};

// Where the pegs go on a cut: points on the plane (plate coordinates), and the
// diameter used there (smaller than asked where the cut is too small for it).
struct KeySpot {
    Vec3 point;
    double diameter = 0.0;
};

// The outlines where `plane` cuts body `id` (plate coordinates, each closed).
std::vector<std::vector<Vec3>> sectionLoops(const Document& doc, ObjectId id, const SplitPlane& plane);
// The area of that cut (holes in it taken away), mm².
double sectionArea(const Document& doc, ObjectId id, const SplitPlane& plane);

// Pegs for a cut through `id`: for each separate piece of the cut, one, two or three
// spots well inside it (clear of its edges and of holes), with room behind them for the
// hole. Empty when the cut is too small or thin for any.
// `keepClear`: other cuts still to be made, which pegs and holes keep away from (so a
// later cut doesn't slice through them).
std::vector<KeySpot> keySpots(const Document& doc, ObjectId id, const SplitPlane& plane, const KeyOptions& options,
                              const std::vector<SplitPlane>& keepClear = {});

struct KeyedSplit {
    std::vector<ObjectId> pieces;  // the side the plane faces, then the other side
    std::vector<ObjectId> pins;    // loose pins (separate shapes beside the model)
    int keys = 0;                  // pegs or pins made
};

// Cuts top-level body `id` in two with `plane`, as one undo step, and keys the cut:
// pegs stand out of the back piece (the side away from the plane's direction; with a
// flat cut, the lower one) into tapered holes in the front piece. With loose pins both
// pieces get holes and the pins are added beside the model. Throws, changing nothing,
// as splitBody does.
KeyedSplit splitWithKeys(CommandStack& stack, Document& doc, ObjectId id, const SplitPlane& plane,
                         const KeyOptions& options, const std::vector<SplitPlane>& keepClear = {});

// The smart split: where the cuts would go to make `id` fit a printer that can print
// `volume` (X, Y, Z mm). Cuts are level or upright, as few as will do, each put where
// it cuts through least. Empty when it already fits.
struct SmartPlan {
    int pieces = 1;
    std::array<int, 3> cutsAcross{0, 0, 0};  // cuts across X, Y and Z
};
SmartPlan planSmartSplit(const Document& doc, ObjectId id, const std::array<double, 3>& volume,
                         const KeyOptions& options);

// ---- Planned cuts (Split to Fit, slice 42) ----------------------------------------
// Split to Fit plans its cuts first, as whole planes through the model (each cuts every
// piece it passes through), so they can be shown and dragged before anything is cut.

// One planned cut: across `axis` (0 X, 1 Y, 2 Z) at `position` (plate mm, where it
// crosses the middle of the model), tilted `tilt` degrees around `tiltAxis`.
struct FitCut {
    int axis = 0;
    double position = 0.0;
    int tiltAxis = 1;
    double tilt = 0.0;
    bool operator==(const FitCut&) const = default;
};

struct FitPlan {
    std::vector<FitCut> cuts;
    int pieces = 1;
    bool turned = false;  // pieces lie turned a quarter on the plate (X and Y swapped)
};

// The plane of a cut, for a model whose box is `box`.
SplitPlane planeOf(const FitCut& cut, const Bounds& box);
// The cuts that make `id` fit `volume`: as few as will do, each where it cuts through
// least. Level and upright cuts only, unless `angled`: then each may lean up to 30°
// where that cuts through less. Empty cuts when it already fits.
FitPlan planFitCuts(const Document& doc, ObjectId id, const std::array<double, 3>& volume, const KeyOptions& options,
                    bool angled);
// What's wrong with a set of cuts (after the user moved them): pieces that would be
// too long for the printer, cuts that cross inside the model or miss it. Worked out
// from the model's box, so it errs on the safe side. Empty when they'll do.
std::vector<std::string> fitProblems(const Document& doc, ObjectId id, const FitPlan& plan,
                                     const std::array<double, 3>& volume, const KeyOptions& options);
// Makes the cuts (keyed), as one undo step: each cut splits every piece it passes
// through. Returns the pieces and pins. Throws, changing nothing, if a cut fails.
KeyedSplit splitAlong(CommandStack& stack, Document& doc, ObjectId id, const std::vector<FitCut>& cuts,
                      const KeyOptions& options);

// Does it: cuts `id` (keyed) until every piece fits `volume`, as one undo step.
// Returns the pieces (and pins). Throws, changing nothing, if a piece can't be made
// to fit (a printer smaller than a peg, say).
KeyedSplit smartSplit(CommandStack& stack, Document& doc, ObjectId id, const std::array<double, 3>& volume,
                      const KeyOptions& options);

}  // namespace forge
