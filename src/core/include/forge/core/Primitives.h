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

TopoDS_Shape makeSphere(double radius);
// An ellipsoid when the radii differ; Z is up.
TopoDS_Shape makeSphere(double radiusX, double radiusY, double radiusZ);
// Round ends. Either radius may be 0 (a point), not both; equal radii give a cylinder.
TopoDS_Shape makeCone(double radiusBottom, double radiusTop, double height);
// Oval ends: each end's radii are both above 0, or both 0 (a point).
TopoDS_Shape makeCone(double bottomX, double bottomY, double topX, double topY, double height);
// Lying flat, resting on the plate.
TopoDS_Shape makeTorus(double majorRadius, double minorRadius);
// Oval ring (ringX / ringY) and oval tube (tube across, tubeZ up).
TopoDS_Shape makeTorus(double ringX, double ringY, double tube, double tubeZ);
TopoDS_Shape makeTube(double outerRadius, double innerRadius, double height);
TopoDS_Shape makeTube(double outerX, double outerY, double innerX, double innerY, double height);
// Tall end at -X, sloping down to the plate at +X.
TopoDS_Shape makeWedge(double width, double depth, double height);
// Rectangular base and top, both centred; a top size of 0 makes a point or ridge.
TopoDS_Shape makePyramid(double baseWidth, double baseDepth, double topWidth, double topDepth, double height);

// Builds whichever primitive the spec describes.
TopoDS_Shape buildShape(const PrimitiveSpec& spec);

}  // namespace forge::primitives
