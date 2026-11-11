// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace forge {

using ObjectId = std::uint64_t;
inline constexpr ObjectId kInvalidId = 0;

// Where a body sits on the build plate. Millimetres and degrees.
// Applied as: rotate about the body's own Z axis, then translate.
struct Placement {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double rotationZ = 0.0;

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

struct CylinderSpec {
    double radius = 10.0;
    double height = 20.0;
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
