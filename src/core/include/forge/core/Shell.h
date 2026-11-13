// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <TopoDS_Shape.hxx>

#include <vector>

namespace forge {

class Document;

// Shell: a solid hollowed out to walls of a set thickness, with some faces left open
// (OpenCASCADE BRepOffsetAPI). Faces are picked by clicking them and stored by place
// (FaceRef), so they are found again when the shape is rebuilt or resized.
namespace shell {

// A reference to face `face` (as edges::FaceFinder numbers them) of `shape`, clicked
// at `at` (on or near the face).
FaceRef refFor(const TopoDS_Shape& shape, int face, const Vec3& at);
// Notes where each reference sits in `part`'s box (to find it after resizing).
void locate(const TopoDS_Shape& part, std::vector<FaceRef>& refs);
// The faces of `shape` the references mean (FaceFinder numbering; -1: not found).
std::vector<int> match(const TopoDS_Shape& shape, const std::vector<FaceRef>& refs);
// The hollowed solid.
TopoDS_Shape apply(const TopoDS_Shape& shape, const ShellFinish& finish);

}  // namespace shell

// A Shell step on `target` (picks in plate coordinates), set up like edgesSpec.
BooleanSpec shellSpec(const Document& doc, ObjectId target, const ShellFinish& inPlate);
// The open faces of a Shell result, as faces of pickableShape(doc, result).
std::vector<int> pickedFaces(const Document& doc, ObjectId result);
// Its finish with the picks in plate coordinates (to edit).
ShellFinish shellInPlate(const Document& doc, ObjectId result);
// The result with a changed finish (picks in plate coordinates).
Body withShell(const Document& doc, ObjectId result, const ShellFinish& inPlate);

}  // namespace forge
