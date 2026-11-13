// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <TopoDS_Shape.hxx>

#include <vector>

namespace forge {

class Document;

// Follow: a groove cut (or a ridge raised) along a path drawn on a shape's surface.
// The path is drawn flat at a spot on the surface and laid over it from there, walking
// step by step and keeping to the surface (on a flat face it stays exactly as drawn);
// a cross-section is then swept along it (OpenCASCADE BRepOffsetAPI_MakePipeShell),
// kept upright to the surface, and cut from or added to the shape.
namespace follow {

// The path's sketch frame: origin on the surface, X along xAxis, Z out of the surface.
Transform frameOf(const FollowFinish& f);
// The path laid over `part` (one polyline per connected run of lines and arcs), in
// part coordinates: to show it, and to sweep along.
std::vector<std::vector<Vec3>> laidPath(const TopoDS_Shape& part, const FollowFinish& f);
// The swept cross-section (what's cut away or added), in part coordinates.
TopoDS_Shape swept(const TopoDS_Shape& part, const FollowFinish& f);
// The part with the groove cut / ridge added.
TopoDS_Shape apply(const TopoDS_Shape& part, const FollowFinish& f);

}  // namespace follow

// A Follow step on `target` (the finish in plate coordinates), like shellSpec.
BooleanSpec followSpec(const Document& doc, ObjectId target, const FollowFinish& inPlate);
// Its finish in plate coordinates (to edit).
FollowFinish followInPlate(const Document& doc, ObjectId result);
// The result with a changed finish (plate coordinates).
Body withFollow(const Document& doc, ObjectId result, const FollowFinish& inPlate);
// The path laid over the shape a Follow step works on, in plate coordinates (to show).
std::vector<std::vector<Vec3>> followPathInPlate(const Document& doc, ObjectId target, const FollowFinish& inPlate,
                                                  bool editing);

}  // namespace forge
