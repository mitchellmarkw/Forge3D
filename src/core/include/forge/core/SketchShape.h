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

// The revolve's solid: the sketch's outlines spun around its axis line.
TopoDS_Shape revolveShape(const RevolveSpec& revolve);
// The cutters of a Hole step: one per loose point of its sketch, going down (-Z) from
// a little above the sketch plane.
TopoDS_Shape holeShape(const HoleSpec& hole);

// Standard metric sizes for holes (ISO): clearance (bolt passes, medium fit), tap
// drill, and counterbore / countersink for socket-head / flat-head screws.
struct HoleSize {
    const char* name;
    double clearance;
    double tapDrill;
    double headDiameter;
    double headDepth;
    double countersinkDiameter;
};
const std::vector<HoleSize>& holeSizes();
// The hole with a standard size's diameters (size empty: left as it is).
HoleSpec sizedAs(HoleSpec hole, const std::string& size, bool tapped);

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
// Where the sketch plane `frame` (its Z = 0) cuts through `shape`, in sketch
// coordinates: the shape's cross-section, to see and snap to on a middle plane.
std::vector<std::vector<Point>> sectionOutlines(const TopoDS_Shape& shape, const Transform& frame);
// The plane through `centre` facing the front (0: X right, Z up, looking from -Y),
// the side (1: Y right, Z up, looking from +X) or the top (2: X right, Y up).
Transform middlePlane(int which, const Vec3& centre);

}  // namespace forge::sketch
