// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <TopoDS_Shape.hxx>

namespace forge::sketch {

// The sketch's closed outlines as flat faces in its plane (local XY): an outline inside
// another is a hole, one inside that is solid again, and so on. Construction lines are
// left out. Throws GeometryError if an outline isn't closed or nothing is closed.
TopoDS_Shape faces(const Sketch& s);

// The pad's solid.
TopoDS_Shape padShape(const PadSpec& pad);

}  // namespace forge::sketch
