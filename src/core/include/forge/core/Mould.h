// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <string>
#include <vector>

// Mould Maker (slice 69): a two-part box round a part, for casting resin copies of a
// printed master. The box is the part's box with a wall all round, the part taken out,
// parted level at a chosen height (by default where the part is widest). Registration
// keys in the box's corners line the halves up; a pour spout and air vents run straight
// up from the part to the top of the box.
namespace forge {

class CommandStack;
class Document;

namespace mould {

// Throws std::invalid_argument (in plain words) if the settings can't be made.
void check(const MouldFinish& f);
// "top half, 8 mm walls, spout Ø6, 2 air vents"
std::string describe(const MouldFinish& f);

// The area of the part's outline cut level at height `z` (mm², holes taken away).
double sectionArea(const Mesh& part, double z);
// The height where the part is widest (the biggest level outline), to part the mould at.
double widest(const Mesh& part);
// Where the spout meets the part: the middle of its highest spot.
Vec3 highest(const Mesh& part);
// Where air would be trapped when the mould is filled from the spout: the part's other
// high spots above the parting face (each the highest point round it), highest first,
// well apart from the spout and each other. At most `most`.
std::vector<Vec3> highSpots(const Mesh& part, double parting, const Vec3& spout, double ventSize, int most = 6);
// Settings for a mould round this part: parted where it's widest, the spout at its top,
// vents at its other high spots.
MouldFinish suggested(const Mesh& part);

// The box round the part.
Bounds box(const Bounds& part, double wall);
// The keys' middles (on the parting face, in the box's corners) and their radius there
// (0: the wall is too thin for keys).
std::vector<Vec3> keyPoints(const Bounds& part, const MouldFinish& f);
double keyRadius(const MouldFinish& f);

// For drawing: the box, the spout and vents, the keys (pegs).
Mesh boxMesh(const Bounds& b);
Mesh channels(const Bounds& part, const MouldFinish& f);
Mesh keys(const Bounds& part, const MouldFinish& f, bool holes);
// The parting face: a flat sheet across the box.
Mesh partingSheet(const Bounds& part, const MouldFinish& f);

// The half (f.top) of the mould round `part`.
MeshSolid apply(const MeshSolid& part, const MouldFinish& f);

}  // namespace mould

// The part's triangles: in a Mould step's own coordinates (for a Mould step), or on the
// plate (for a shape).
Mesh mouldPart(const Document& doc, ObjectId id);
// A Mould step's settings (its own coordinates; f.top says which half it is).
const MouldFinish& mouldOf(const Document& doc, ObjectId result);
// Makes a mould round top-level shape `id` as one undo step: two shapes, "<name> mould
// (top)" (made from a copy of the shape) and "<name> mould (bottom)", in place, closed
// (lifted together if need be, so the bottom half stands on the plate).
// `f` is in plate coordinates (f.top is ignored). Returns {top, bottom}. Throws, changing
// nothing, if it can't be made.
std::vector<ObjectId> makeMould(CommandStack& stack, Document& doc, ObjectId id, const MouldFinish& f);
// The step with new settings (its own coordinates; f.top is ignored), and its other half
// likewise if it's still on the plate with the same settings (so they keep fitting).
// The caller records them (EditBodiesCommand).
std::vector<Body> withMould(const Document& doc, ObjectId result, const MouldFinish& f);

}  // namespace forge
