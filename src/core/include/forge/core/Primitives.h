// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <TopoDS_Shape.hxx>

namespace forge::primitives {

// All primitives sit on the build plate: centred on X and Y, bottom face at Z = 0.
// Dimensions are in millimetres and must be positive; invalid input throws
// std::invalid_argument.

TopoDS_Shape makeBox(double width, double depth, double height);
TopoDS_Shape makeCylinder(double radius, double height);
// Elliptical when the radii differ (exact elliptic surface); round when equal.
TopoDS_Shape makeCylinder(double radiusX, double radiusY, double height);

// Builds whichever primitive the spec describes.
TopoDS_Shape buildShape(const PrimitiveSpec& spec);

}  // namespace forge::primitives
