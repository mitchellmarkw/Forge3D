// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"

#include <TopoDS_Shape.hxx>

namespace forge {

struct TessellationSettings {
    double linearDeflection = 0.05;   // max distance from the true surface, in mm
    double angularDeflection = 0.35;  // max angle between neighbouring facets, in radians
    bool relative = false;            // if true, linearDeflection is a fraction of each edge's size
};

// Turns an exact B-rep shape into triangles for display and mesh export.
// Normals are smooth inside each face and sharp across face boundaries,
// so curved faces look round and box edges stay crisp.
Mesh tessellate(const TopoDS_Shape& shape, const TessellationSettings& settings = {});

}  // namespace forge
