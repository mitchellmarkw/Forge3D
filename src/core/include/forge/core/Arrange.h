// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <optional>
#include <utility>
#include <vector>

namespace forge {

class CommandStack;
class Document;

// A shape and everything inside it, as captured for the clipboard: parts before
// the shapes that hold them, the shape itself last.
using BodyTree = std::vector<Body>;

// The shape `id` and everything inside it, as they are now.
BodyTree captureTree(const Document& doc, ObjectId id);

// Copies of `tree` with new ids and names ("Box 1 copy", "Box 1 copy 2"...), ready
// to insert. The root (last) is moved by `offset` on the plate.
BodyTree cloneTree(Document& doc, const BodyTree& tree, const Vec3& offset);

// Adds copies of these trees as one undo step ("Duplicate" / "Paste"); returns the
// new shapes on the plate.
std::vector<ObjectId> addCopies(CommandStack& stack, Document& doc, const std::vector<BodyTree>& trees,
                                const Vec3& offset, const std::string& label);

// How far to move copies of these shapes so they sit just beside them along +X
// (their combined width plus a gap).
Vec3 besideOffset(const Document& doc, const std::vector<ObjectId>& ids, double gap = 5.0);
// How far along +X to move something whose left edge is at `minX` so it lands
// clear of everything on the plate (a gap to the right of the rightmost shape).
Vec3 clearOfPlate(const Document& doc, double minX, double gap = 5.0);
// The left edge (smallest X on the plate) of these shapes.
double leftEdge(const Document& doc, const std::vector<ObjectId>& ids);

// Mirrors each shape on the plate in place, across the plate axis `axis` (0 = X
// flips left / right, 1 = Y, 2 = Z flips upside down) through its own middle, as
// one undo step. Each becomes a Mirror result ("Box 1 (mirrored)"); Separate
// undoes the mirror. Returns the new results.
std::vector<ObjectId> mirrorBodies(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids, int axis);

// Linked mirrors (slice 74). A copy of shape `id` (on the plate) mirrored across the
// plate axis `axis` (0 = X, left / right; 1 = Y, front / back) at `at` mm along it, that
// keeps up with the shape as it changes (see MirrorLink). Named for the other side
// ("Left arm" → "Right arm", else "<name> (mirrored)"). One undo step; returns it.
ObjectId addLinkedMirror(CommandStack& stack, Document& doc, ObjectId id, int axis, double at);
// "Left arm" ↔ "Right arm" (and left/right, L/R words), else "<name> (mirrored)".
std::string otherSideName(const std::string& name);
// The linked mirror with its mirror moved (`axis`, `at`); the caller records it.
Body withLinkPlane(const Document& doc, ObjectId linked, int axis, double at);
// Makes a linked mirror (on the plate) a shape of its own as it is now, no longer
// following the other: one undo step. Returns the new shape.
ObjectId unlinkMirror(CommandStack& stack, Document& doc, ObjectId linked);

// Groups shapes on the plate so they move, turn and resize as one ("Group 1"),
// without merging them. Separate ungroups. Needs two or more shapes.
ObjectId groupBodies(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids);

// Where a ray from the plate first meets shape `id` (on the plate), and which way its
// surface faces there, both in plate coordinates. Nothing if it misses.
std::optional<std::pair<Vec3, Vec3>> faceUnder(const Document& doc, ObjectId id, const Vec3& origin,
                                               const Vec3& direction);
// The placement that turns shape `id` (on the plate) about its middle, the least
// needed, so the surface facing `normal` (plate coordinates) faces straight down, and
// then rests it on the plate ("Lay on Face"). Where it sits across the plate stays.
Placement laidOnFace(const Document& doc, ObjectId id, const Vec3& normal);

}  // namespace forge
