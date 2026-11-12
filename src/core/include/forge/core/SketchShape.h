// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <optional>
#include <vector>

#include <TopoDS_Shape.hxx>

namespace forge::sketch {

// The sketch's closed outlines as flat faces in its plane (local XY): an outline inside
// another is a hole, one inside that is solid again, and so on. Construction lines are
// left out. Throws GeometryError if an outline isn't closed or nothing is closed.
TopoDS_Shape faces(const Sketch& s);

// The pad's solid.
TopoDS_Shape padShape(const PadSpec& pad);

// A flat face to sketch on: where the sketch sits (sketch -> shape coordinates: its
// origin at the middle of the face, Z pointing out of the solid, X along the plate's X
// on faces that look up or down, else level), and the face's outlines in sketch
// coordinates (to show and snap to).
struct FacePlane {
    Transform frame;
    std::vector<std::vector<Point>> outlines;
};
// Face `face` (as edges::FaceFinder numbers them) of `shape`; nothing if it isn't flat.
std::optional<FacePlane> planeOfFace(const TopoDS_Shape& shape, int face);
// The outlines of the flat faces of `shape` lying in the sketch plane `frame` (its
// Z = 0), in sketch coordinates: the face a sketch was drawn on, found again.
std::vector<std::vector<Point>> outlinesOnPlane(const TopoDS_Shape& shape, const Transform& frame);

}  // namespace forge::sketch
