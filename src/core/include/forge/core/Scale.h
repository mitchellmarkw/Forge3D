// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <stdexcept>
#include <vector>

namespace forge {

class Document;

// Thrown when a stretch can't be made by changing the parts' sizes: a part turned
// at an angle (not a multiple of 90 degrees) can only be scaled evenly, and a torus
// can't be stretched differently along its ring's X and Y.
class NotScalable : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// The primitive with each body axis multiplied by factors[0..2] (X, Y, Z of the
// body). Even factors work for every shape. Throws NotScalable for a torus with
// different X and Y factors.
PrimitiveSpec scaledSpec(const PrimitiveSpec& spec, const Vec3& factors);

// Scales body `id` (usually a combined shape) and everything inside it by
// `factors` along its own X, Y and Z, keeping `anchor` (a point in its own
// coordinates) where it is. Its own placement doesn't change: its parts' sizes and
// positions (and Split planes) do. Returns the new state of every body that
// changes; nothing is applied. Throws NotScalable as above (uneven factors and a
// part turned at an odd angle).
std::vector<Body> scaledTree(const Document& doc, ObjectId id, const Vec3& factors, const Vec3& anchor);

}  // namespace forge
