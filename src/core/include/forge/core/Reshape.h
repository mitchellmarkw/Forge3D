// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <string>

// Reshape (slice 102): a box round a shape with a few handles; moving a handle bends the
// shape smoothly with it (a Bezier lattice), like pulling on a cage round it.
namespace forge {

class Document;

namespace reshape {

inline constexpr int kLeastHandles = 2, kMostHandles = 4;

// Throws std::invalid_argument (in plain words) if the box can't be used.
void check(const ReshapeFinish& f);
// "3 × 3 × 3 handles, 4 moved"
std::string describe(const ReshapeFinish& f);
// A box a little bigger than `b` with `handles` along each side, none moved.
ReshapeFinish around(const Bounds& b, int handles = 3);
// The handles laid out again `handles` along each side, keeping the box (the moves are
// lost: they belong to the old handles).
ReshapeFinish withHandles(const ReshapeFinish& f, int handles);
int handleCount(const ReshapeFinish& f);
int handleIndex(const ReshapeFinish& f, int i, int j, int k);
// Where a handle starts (on the box's grid), and where it is (moved).
Vec3 rest(const ReshapeFinish& f, int index);
Vec3 at(const ReshapeFinish& f, int index);
// The handle across the box's middle in X (left / right): itself on the middle line.
int mirrored(const ReshapeFinish& f, int index);
// Where a point goes.
Vec3 moved(const ReshapeFinish& f, const Vec3& p);
// The surface made fine enough to bend smoothly (no edge longer than about a 60th of the
// box), each point then moved.
Mesh reshaped(const Mesh& surface, const ReshapeFinish& f);
// Only the move, of a surface already fine enough (as the preview while dragging).
Mesh movedMesh(const Mesh& fine, const ReshapeFinish& f);
// A surface made fine enough for the box.
Mesh finer(const Mesh& surface, const ReshapeFinish& f);

}  // namespace reshape

// The Reshape step for `target` with a box round it, in its step's coordinates (the
// parent's, the plate's for a shape on it).
BooleanSpec reshapeSpec(const Document& doc, ObjectId target, const ReshapeFinish& f);

}  // namespace forge
