// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <optional>
#include <string>
#include <vector>

// Surface stamps: small details stamped onto a surface (rivets, bolts, screws, studs,
// vents, scales, stitches, or a custom shape), standing out or cut in, one at a time or
// in rows. Each stamp has a stem going the other way (into the surface when raised, out
// of it when sunk), so it meets a curved surface without a gap.
namespace forge {

class Document;

namespace stamp {

inline constexpr int kKindCount = 8;
const char* title(StampKind kind);          // "Rivet", "Hex bolt"...
const char* key(StampKind kind);            // the file's name: "rivet", "hexBolt"...
std::optional<StampKind> fromKey(const std::string& key);
// The usual height for a size (a rivet's dome is about a third of its width...).
double usualHeight(StampKind kind, double size);

// Throws std::invalid_argument (in plain words) if it can't be stamped.
void check(const StampFinish& f);
// "12 rivets Ø3, raised"
std::string describe(const StampFinish& f, const std::string& customName = {});

// One stamp, standing on the origin facing +Z (sunk: going down into -Z), X along.
// `custom`: the shape to use for StampKind::Custom (any size: it's fitted to size
// and height, its middle on the origin and its bottom on it).
MeshSolid shape(const StampFinish& f, const MeshSolid* custom = nullptr);
// Where a stamp at `spot` goes: the origin on the point, +Z out, X along (turned by spin).
Transform frame(const StampSpot& spot, double spin);
// All the stamps, placed (step coordinates).
MeshSolid placed(const StampFinish& f, const MeshSolid* custom = nullptr);
// The part with the stamps added or cut.
MeshSolid apply(const MeshSolid& part, const StampFinish& f, const MeshSolid* custom = nullptr);

// Points every `spacing` along the straight line from a to b (both ends, evenly spread
// so the last lands on b; at least a).
std::vector<Vec3> rowPoints(const Vec3& a, const Vec3& b, double spacing);

}  // namespace stamp

// A Stamp step on `target` (spots in plate coordinates); `custom`: the shape used as
// the stamp (on the plate, taken into the step), if the kind is Custom.
BooleanSpec stampSpec(const Document& doc, ObjectId target, const StampFinish& inPlate,
                      ObjectId custom = kInvalidId);
StampFinish stampInPlate(const Document& doc, ObjectId result);
Body withStamp(const Document& doc, ObjectId result, const StampFinish& inPlate);

}  // namespace forge
