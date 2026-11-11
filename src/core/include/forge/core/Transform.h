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
// The same for any body: a primitive's centre, or a combine result's stored pivot.
Vec3 pivotOf(const Body& body);

// Body coordinates -> plate coordinates:
//   move to (x, y, z), then rotate about the pivot.
// With all angles zero this is a plain move, so (x, y, z) is where the centre
// of the body's base sits.
Transform placementTransform(const Placement& placement, const PrimitiveSpec& spec);
Transform placementTransform(const Placement& placement, const Vec3& pivot);

// The placement that reproduces transform t for a body turning about `pivot`
// (the inverse of placementTransform).
Placement placementFromTransform(const Transform& t, const Vec3& pivot);

// The X, Y and Z angles (degrees) that rebuild the rotation part of t with
// rotationXYZ(). When Y is exactly +/-90 degrees the split between X and Z is
// ambiguous; Z is then reported as 0.
Vec3 eulerXYZ(const Transform& t);

// ---- Gizmo edits ----------------------------------------------------------------

// The placement after turning the body by `degrees` about a plate axis
// (0 = X, 1 = Y, 2 = Z) through the body's centre. The centre does not move.
Placement rotatedAboutPlateAxis(const Body& body, int axis, double degrees);

// The value a face handle changes: box width / depth / height for axis 0 / 1 / 2;
// cylinder radius X / radius Y / height.
double dimension(const PrimitiveSpec& spec, int axis);

struct ResizeOptions {
    // Scale every size by the same factor (keeps proportions).
    bool uniform = false;
    // Cylinders: a side handle changes both radii, so the cylinder stays round.
    // Off: it changes only that axis's radius (elliptical cylinder).
    bool keepRound = true;
    // No size goes below this (mm).
    double minValue = 0.1;
};

// The body after its face handle (axis 0..2 in the body's own frame, side +1 or -1)
// is dragged so that dimension(axis) becomes newValue.
// What stays still on the plate:
//   - top / bottom handle: the opposite end;
//   - box side handle: the opposite side, at base level;
//   - cylinder side handle: the centre line at the base (radii grow evenly).
// So an untilted body keeps its base on the plate, even with uniform scaling.
Body resizedFace(const Body& body, int axis, int side, double newValue, const ResizeOptions& options = {});

}  // namespace forge
