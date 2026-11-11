// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <string>

namespace forge {

class Document;

// Ways to line shape 2 up against shape 1 (Align Shapes menu). Shape 1 stays put.
enum class AlignMode {
    MeetAtFace,       // slide along the facing axis until the facing sides touch
    FaceCentreXYZ,    // centre onto the middle of shape 1's facing face
    FaceCentreX,      // along X only: centre onto shape 1's facing X face (halfway in)
    FaceCentreY,
    FaceCentreZ,
    CentreXYZ,        // centre onto shape 1's centre
    CentreX,          // along X only
    CentreY,
    CentreZ,
};

std::string toString(AlignMode mode);  // menu text, e.g. "Align Face Center X"

// The axis (0 = X, 1 = Y, 2 = Z) along which two boxes sit most clearly side by
// side: the largest gap between centres relative to their combined half sizes.
// Ties and coinciding centres prefer Z, then Y.
int facingAxis(const Bounds& fixed, const Bounds& moving);

// The placement that moves body `moving` (a shape on the plate) into line with
// body `fixed`, using their exact world bounding boxes. Only the position
// changes. Throws std::invalid_argument for a body that is a part of another
// shape, or std::out_of_range for a missing body.
Placement alignedPlacement(const Document& doc, ObjectId fixed, ObjectId moving, AlignMode mode);

}  // namespace forge
