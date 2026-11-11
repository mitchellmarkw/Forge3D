// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace forge {

using ObjectId = std::uint64_t;
inline constexpr ObjectId kInvalidId = 0;

// Where a body sits on the build plate. Millimetres and degrees.
// (x, y, z) is where the centre of the body's base goes; the body is then
// turned about its own centre: first about X, then Y, then Z (fixed axes).
// See placementTransform() in Transform.h.
//
// rotationZ comes before rotationX/Y only for historical reasons: existing
// code writes Placement{x, y, z, rotationZ}. Prefer named fields for new code.
struct Placement {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double rotationZ = 0.0;
    double rotationX = 0.0;
    double rotationY = 0.0;

    bool operator==(const Placement&) const = default;
};

// ---- Primitive parameters ---------------------------------------------------
// Each primitive is fully described by its parameters; geometry is always rebuilt
// from them (the parametric principle). Dimensions in millimetres.

struct BoxSpec {
    double width = 20.0;   // X
    double depth = 20.0;   // Y
    double height = 20.0;  // Z
    bool operator==(const BoxSpec&) const = default;
};

// A cylinder standing on the plate. radiusX == radiusY gives a round cylinder;
// different radii give an elliptical one (an ellipse extruded straight up).
struct CylinderSpec {
    double radiusX = 10.0;
    double radiusY = 10.0;
    double height = 20.0;

    CylinderSpec() = default;
    // Round: CylinderSpec{radius, height}
    CylinderSpec(double radius, double h) : radiusX(radius), radiusY(radius), height(h) {}
    // Elliptical: CylinderSpec{radiusX, radiusY, height}
    CylinderSpec(double rx, double ry, double h) : radiusX(rx), radiusY(ry), height(h) {}

    bool isRound() const { return radiusX == radiusY; }
    bool operator==(const CylinderSpec&) const = default;
};

using PrimitiveSpec = std::variant<BoxSpec, CylinderSpec>;

// "Box", "Cylinder": used for default names and command labels.
std::string typeName(const PrimitiveSpec& spec);

// A solid object in the document.
struct Body {
    ObjectId id = kInvalidId;
    std::string name;
    PrimitiveSpec spec;
    Placement placement;

    bool operator==(const Body&) const = default;
};

}  // namespace forge
