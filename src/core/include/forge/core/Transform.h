// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <array>

namespace forge {

using Vec3 = std::array<double, 3>;

// A rigid transform (rotation + translation): p' = R p + t.
// Row-major 3x4: m[row][0..2] is the rotation row, m[row][3] the translation.
struct Transform {
    std::array<std::array<double, 4>, 3> m{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};

    static Transform identity() { return {}; }
    static Transform translation(const Vec3& t);
    // Degrees, about the fixed X, Y and Z axes, applied X first, then Y, then Z
    // (R = Rz * Ry * Rx; the same convention as FreeCAD's roll / pitch / yaw).
    static Transform rotationXYZ(double degX, double degY, double degZ);

    Vec3 apply(const Vec3& p) const;
    Vec3 applyDirection(const Vec3& d) const;  // rotation only
    Transform inverse() const;                 // valid because R is a pure rotation
    Transform operator*(const Transform& rhs) const;  // (this * rhs)(p) = this(rhs(p))
};

// The point a body turns around, in its own coordinates: the centre of its
// bounding box (every primitive is centred on X/Y with its base at Z = 0).
Vec3 pivotOf(const PrimitiveSpec& spec);

// Body coordinates -> plate coordinates:
//   move to (x, y, z), then rotate about the pivot.
// With all angles zero this is a plain move, so (x, y, z) is where the centre
// of the body's base sits.
Transform placementTransform(const Placement& placement, const PrimitiveSpec& spec);

}  // namespace forge
