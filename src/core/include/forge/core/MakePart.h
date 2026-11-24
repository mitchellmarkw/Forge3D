// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <string>
#include <vector>

// Make Part from paint (slice 95): paint where armor, a glove or a belt goes on a shape,
// and a new separate part is made over that area, following the surface: a shell of a
// thickness, sitting on top of the body (with a fit gap) or set into it (the body cut
// back under it, so the outline stays and the part sits flush).
namespace forge {

class CommandStack;
class Document;

namespace makepart {

struct Options {
    bool setIn = false;      // set into the body (flush) rather than sitting on top
    double thickness = 1.5;  // mm
    double fit = 0.1;        // mm between the part and the body
};

// A closed shell over an open surface patch (its triangles facing out): between `inner`
// and `outer` along the surface's facing (negative: in under the surface). `shrink` first
// brings the patch's edge in along the surface by that much (for a fit at the sides).
// Throws (in plain words) if the patch is empty or its edge touches itself.
Mesh slab(const Mesh& patch, double inner, double outer, double shrink = 0.0);

// The painted area of shape `id` (on the plate), as its own surface in plate coordinates.
// `dabs` are in plate coordinates. Throws if nothing (or everything) is painted.
Mesh paintedSurface(const Document& doc, ObjectId id, const std::vector<MaskDab>& dabs);

struct Made {
    ObjectId part = kInvalidId;  // the new part
    ObjectId body = kInvalidId;  // the shape, as it is now (a new step if the part is set in)
};
// Makes the part (named "<shape> part"), as one undo step. Throws (in plain words) if it
// can't be made.
Made makePart(CommandStack& stack, Document& doc, ObjectId id, const std::vector<MaskDab>& dabs, const Options& o);

}  // namespace makepart

}  // namespace forge
