// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <string>
#include <vector>

// Hollow: a part hollowed out for resin printing, walls of a set thickness, with drain
// holes through the wall so the resin inside can run out (and plugs to close them).
// Works on any shape, sculpts and imported meshes too: the inside is found on a grid of
// small cubes (about a third of the wall thickness), so the walls are that thickness to
// within about one of them.
namespace forge {

class Document;

namespace hollow {

// Throws std::invalid_argument (in plain words) if the settings can't be made.
void check(const HollowFinish& f);
// "2 mm walls, 2 drain holes Ø3"
std::string describe(const HollowFinish& f);

// The size of the grid's cubes for a part this big (mm).
double cellFor(const Bounds& box, double thickness);

// The part with the inside taken out (no holes). If it's nowhere thicker than two walls
// it comes back as it was.
MeshSolid hollowed(const MeshSolid& solid, double thickness);
// What a drain hole cuts: a round rod from just outside the surface in through the wall.
Mesh holeCutter(const DrainHole& hole, double size, double thickness);
// Hollowed, with the holes cut.
MeshSolid apply(const MeshSolid& solid, const HollowFinish& f);

// Where to put `count` drain holes on their own: low on the part (as it sits on the
// plate, `plate` being its triangles there) and apart from each other, facing out.
// The first is the lowest point; the others the next lowest, at least four hole sizes
// from the ones before.
std::vector<DrainHole> lowSpots(const Mesh& plate, double holeSize, int count = 2);

// A plug to close a hole after printing: a short peg the hole's size less the fit all
// round, as long as the wall is thick.
PrimitiveSpec plug(const HollowFinish& f, double fit);

}  // namespace hollow

// A Hollow step on `target` (hole spots given in plate coordinates).
BooleanSpec hollowSpec(const Document& doc, ObjectId target, const HollowFinish& inPlate);
// Its settings with the hole spots in plate coordinates (to change).
HollowFinish hollowInPlate(const Document& doc, ObjectId result);
// The step with changed settings (hole spots in plate coordinates).
Body withHollow(const Document& doc, ObjectId result, const HollowFinish& inPlate);

}  // namespace forge
