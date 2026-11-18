// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <array>
#include <string>
#include <vector>

// Laying pieces out for printing (slice 43): each turned onto its flattest side, then
// packed onto as many build plates as it takes. Plate 1 is the one at the middle of
// the view; plates 2, 3... sit beside it along +X.
namespace forge {

class CommandStack;
class Document;

// The gap between neighbouring plates in the view, mm.
inline constexpr double kPlateGap = 20.0;

// The middle of plate `index` (0 = plate 1) along X.
double plateCentreX(int index, double plateWidth);
// The plate a shape's box lies wholly on (0 = plate 1, at most `most` - 1), or -1.
int plateHolding(const Bounds& box, const std::array<double, 3>& plate, int most = 50);

struct LayoutOptions {
    std::array<double, 3> plate{220.0, 220.0, 250.0};  // the printer's build volume, mm
    double gap = 5.0;      // between pieces
    bool turnOver = true;  // turn each piece onto its flattest side first
    // Sides to favour (either way along these directions, plate coordinates): the cuts
    // of a split, so pegs stand straight up and holes go straight down (round holes
    // print round that way; lying down they come out a little oval).
    std::vector<Vec3> favour;
};

struct LayoutResult {
    int plates = 0;                      // plates used
    std::vector<int> plateOf;            // per shape, in the order given
    std::vector<ObjectId> tooBig;        // shapes no plate can take (left where they are)
};

// The side to put down: of the shape's large flat areas (and the way it sits now), the
// one with the most of the shape touching the plate, as a direction in plate
// coordinates (which way that side faces now). The way it sits now is kept unless
// another side touches clearly more (half as much again).
Vec3 flattestSide(const Document& doc, ObjectId id, const std::array<double, 3>& plate,
                  const std::vector<Vec3>& favour = {});

// Turns (optionally) and moves the shapes on the plate so each lies on its flattest
// side, packed in rows onto plate 1, then plates 2, 3... as needed, as one undo step.
// Shapes too big for any plate are left where they are (and listed).
LayoutResult layOut(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids,
                    const LayoutOptions& options);

}  // namespace forge
