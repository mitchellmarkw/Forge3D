// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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

// A ball resting on the plate.
struct SphereSpec {
    double radius = 10.0;
    bool operator==(const SphereSpec&) const = default;
};

// A cone (or a cut-off cone) standing on its base. Either radius may be 0 for a
// point (radiusTop 0: point up; radiusBottom 0: point down). Equal radii make a
// plain cylinder.
struct ConeSpec {
    double radiusBottom = 10.0;
    double radiusTop = 0.0;
    double height = 20.0;
    bool operator==(const ConeSpec&) const = default;
};

// A ring (doughnut) lying flat on the plate. majorRadius: centre of the ring to
// the middle of the tube; minorRadius: the tube's own radius (half its thickness).
// minorRadius must be smaller than majorRadius, so there is a hole.
struct TorusSpec {
    double majorRadius = 12.0;
    double minorRadius = 4.0;
    bool operator==(const TorusSpec&) const = default;
};

// A pipe standing on the plate: a cylinder with a round hole along its axis.
struct TubeSpec {
    double outerRadius = 10.0;
    double innerRadius = 7.0;
    double height = 20.0;
    bool operator==(const TubeSpec&) const = default;
};

// A ramp: full height at the -X side, sloping down to the plate at +X.
// Its side view (along Y) is a right triangle.
struct WedgeSpec {
    double width = 20.0;   // X
    double depth = 20.0;   // Y
    double height = 20.0;  // Z
    bool operator==(const WedgeSpec&) const = default;
};

using PrimitiveSpec = std::variant<BoxSpec, CylinderSpec, SphereSpec, ConeSpec, TorusSpec, TubeSpec, WedgeSpec>;

// How many kinds of primitive there are, and a new one of each (Add Shape menu).
inline constexpr std::size_t kPrimitiveKindCount = std::variant_size_v<PrimitiveSpec>;
PrimitiveSpec defaultSpec(std::size_t kind);  // kind = variant index

// "Box", "Cylinder", "Sphere", ...: used for default names and command labels.
std::string typeName(const PrimitiveSpec& spec);
// "box", "cylinder", ...: the type in .f3d files.
std::string fileTypeName(const PrimitiveSpec& spec);

// ---- Generic access to a primitive's sizes ----------------------------------------
// Every primitive is a short list of lengths. The Properties panel, the file format
// and uniform scaling all work through this list.

struct SpecField {
    const char* key;    // name in .f3d files, e.g. "radiusX"
    const char* label;  // e.g. "Radius X"
    const char* tip;    // tooltip
    bool allowZero;     // e.g. a cone's top radius may be 0 (a point)
};

const std::vector<SpecField>& specFields(const PrimitiveSpec& spec);
double specValue(const PrimitiveSpec& spec, std::size_t field);
void setSpecValue(PrimitiveSpec& spec, std::size_t field, double value);
// Every length times factor (keeps the shape's proportions).
PrimitiveSpec scaled(const PrimitiveSpec& spec, double factor);
// True for shapes that are round seen from above (cylinder, sphere, cone, torus, tube).
bool isRoundFromAbove(const PrimitiveSpec& spec);

// ---- Combine results ------------------------------------------------------------

enum class BooleanOp { Merge, Subtract, Intersect };

// "Merge", "Subtract", "Intersect"
std::string toString(BooleanOp op);

// A body made by combining other bodies (its parts, or "operands").
//   Merge:     everything in any part.
//   Subtract:  the first part, minus all the others.
//   Intersect: only what every part shares.
// The parts stay in the document, hidden inside the result and still editable;
// their placements are relative to the result. A part belongs to at most one result.
struct BooleanSpec {
    BooleanOp op = BooleanOp::Merge;
    std::vector<ObjectId> operands;
    // The point the result turns around, in its own coordinates. Fixed when the
    // result is made, so editing a part never makes the result jump.
    std::array<double, 3> pivot{0.0, 0.0, 0.0};

    bool operator==(const BooleanSpec&) const = default;
};

// A solid object in the document: either a primitive (spec) or a combine result
// (boolean). For a result, spec is unused.
struct Body {
    ObjectId id = kInvalidId;
    std::string name;
    PrimitiveSpec spec;
    Placement placement;
    std::optional<BooleanSpec> boolean;

    bool isBoolean() const { return boolean.has_value(); }
    bool operator==(const Body&) const = default;
};

// "Box", "Cylinder", "Merge", "Subtract", "Intersect"
std::string typeName(const Body& body);

}  // namespace forge
