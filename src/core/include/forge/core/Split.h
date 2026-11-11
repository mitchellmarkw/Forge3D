// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <string>
#include <vector>

namespace forge {

class CommandStack;
class Document;

// A cutting plane on the plate: a point on it and the direction it faces.
struct SplitPlane {
    Vec3 point{0.0, 0.0, 0.0};
    Vec3 normal{0.0, 0.0, 1.0};  // need not be unit length
};

enum class SplitKeep {
    Both,   // two shapes: the side the normal points to, and the other side
    Front,  // only the side the normal points to
    Back,   // only the other side
};

// "top" / "bottom", "right" / "left" or "back" / "front": the name of the side a
// direction points to, by its strongest axis (+Z top, +X right, +Y back).
std::string sideName(const Vec3& direction);

// The plane through `centre` facing the direction given by tilting straight up
// (+Z) by tiltX degrees around X, then tiltY degrees around Y, moved `offset` mm
// along that direction. tiltX 0, tiltY 90 faces +X; tiltX -90 faces +Y.
SplitPlane planeFromTilts(const Vec3& centre, double tiltX, double tiltY, double offset);
// The tilts and offset that rebuild `plane` with planeFromTilts(centre, ...).
void tiltsOfPlane(const SplitPlane& plane, const Vec3& centre, double& tiltX, double& tiltY, double& offset);

// Cuts top-level body `id` with `plane` (plate coordinates) as one undo step:
// the body moves inside a Split result ("Box 1 (top)"); for Both, a copy of the
// body goes inside a second one ("Box 1 (bottom)"). Returns the new results' ids.
// Throws (changing nothing) if the body is not on the plate or the plane misses it
// (one side would be empty).
std::vector<ObjectId> splitBody(CommandStack& stack, Document& doc, ObjectId id, const SplitPlane& plane,
                                SplitKeep keep);

// An existing Split result's plane, in plate coordinates.
SplitPlane splitPlaneOf(const Document& doc, ObjectId result);
// The Split result with a new plane (plate coordinates). Throws if the plane would
// leave nothing. The caller records the change (EditBodyCommand).
Body withSplitPlane(const Document& doc, ObjectId result, const SplitPlane& plane);

}  // namespace forge
