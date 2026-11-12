// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <memory>
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

// A ball resting on the plate. Different radii make an ellipsoid (an egg or a
// squashed ball); radiusZ is the up direction.
struct SphereSpec {
    double radiusX = 10.0;
    double radiusY = 10.0;
    double radiusZ = 10.0;

    SphereSpec() = default;
    explicit SphereSpec(double r) : radiusX(r), radiusY(r), radiusZ(r) {}
    SphereSpec(double rx, double ry, double rz) : radiusX(rx), radiusY(ry), radiusZ(rz) {}
    bool operator==(const SphereSpec&) const = default;
};

// A cone (or a cut-off cone) standing on its base. Each end is an ellipse with
// radii X and Y (equal: a circle). An end with both radii 0 is a point; one radius
// 0 and the other not is not allowed. Equal round ends make a plain cylinder.
struct ConeSpec {
    double bottomRadiusX = 10.0;
    double bottomRadiusY = 10.0;
    double topRadiusX = 0.0;
    double topRadiusY = 0.0;
    double height = 20.0;

    ConeSpec() = default;
    // Round: ConeSpec{bottom, top, height}
    ConeSpec(double bottom, double top, double h)
        : bottomRadiusX(bottom), bottomRadiusY(bottom), topRadiusX(top), topRadiusY(top), height(h) {}
    ConeSpec(double bx, double by, double tx, double ty, double h)
        : bottomRadiusX(bx), bottomRadiusY(by), topRadiusX(tx), topRadiusY(ty), height(h) {}
    bool operator==(const ConeSpec&) const = default;
};

// A ring (doughnut) lying flat on the plate. The ring's centre line is an ellipse
// with radii ringRadiusX / ringRadiusY (equal: a circle). The tube's cross-section
// is an ellipse too: tubeRadius across the ring (in the plate's plane) and
// tubeRadiusZ up. The tube must leave a hole: tubeRadius < (smaller ring radius)^2
// / (larger ring radius), which for a round ring is just tubeRadius < ringRadius.
struct TorusSpec {
    double ringRadiusX = 12.0;
    double ringRadiusY = 12.0;
    double tubeRadius = 4.0;
    double tubeRadiusZ = 4.0;

    TorusSpec() = default;
    // Round: TorusSpec{ring, tube}
    TorusSpec(double ring, double tube) : ringRadiusX(ring), ringRadiusY(ring), tubeRadius(tube), tubeRadiusZ(tube) {}
    TorusSpec(double rx, double ry, double t, double tz) : ringRadiusX(rx), ringRadiusY(ry), tubeRadius(t), tubeRadiusZ(tz) {}
    bool operator==(const TorusSpec&) const = default;
};

// A pipe standing on the plate: outside and hole are ellipses (equal radii:
// circles). The hole must be inside: innerRadiusX < outerRadiusX and the same for Y.
struct TubeSpec {
    double outerRadiusX = 10.0;
    double outerRadiusY = 10.0;
    double innerRadiusX = 7.0;
    double innerRadiusY = 7.0;
    double height = 20.0;

    TubeSpec() = default;
    // Round: TubeSpec{outer, inner, height}
    TubeSpec(double outer, double inner, double h)
        : outerRadiusX(outer), outerRadiusY(outer), innerRadiusX(inner), innerRadiusY(inner), height(h) {}
    TubeSpec(double ox, double oy, double ix, double iy, double h)
        : outerRadiusX(ox), outerRadiusY(oy), innerRadiusX(ix), innerRadiusY(iy), height(h) {}
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

// A pyramid on a rectangular base, centred. The top is a rectangle topWidth x
// topDepth, centred over the base: both 0 make a point, one 0 makes a ridge (like
// a roof), both above 0 a flat top.
struct PyramidSpec {
    double baseWidth = 20.0;  // X
    double baseDepth = 20.0;  // Y
    double topWidth = 0.0;
    double topDepth = 0.0;
    double height = 20.0;
    bool operator==(const PyramidSpec&) const = default;
};

// 3D letters lying flat on the plate, read from above (+Y is the top of the letters).
// The laid-out text is stretched to width x depth (its block on the plate) and is
// height thick. Changing the words or font keeps the letter size (see text::withText).
struct TextSpec {
    std::string text = "Text";
    std::string font;  // family name; empty = the default font
    bool bold = false;
    bool italic = false;
    bool vertical = false;  // letters stacked top to bottom (each upright)
    double width = 30.0;   // X: the whole line(s)
    double depth = 10.0;   // Y: from the lowest to the highest letter
    double height = 3.0;   // Z: thickness
    bool operator==(const TextSpec&) const = default;
};

// Triangles read from a file (STL / 3MF / OBJ), after repair. Shared between copies
// and never changed once made. Points are centred on X and Y with the lowest at Z = 0,
// like every primitive.
struct MeshData {
    std::vector<float> positions;       // 3 per point, millimetres
    std::vector<std::uint32_t> indices;  // 3 per triangle, anticlockwise seen from outside
    std::array<float, 3> size{0.f, 0.f, 0.f};  // its natural width, depth, height
    bool closed = true;                 // watertight (every edge shared by two triangles)
};

// An imported shape: the triangles stretched to width x depth x height.
struct MeshSpec {
    std::shared_ptr<const MeshData> data;
    double width = 1.0;
    double depth = 1.0;
    double height = 1.0;
    bool operator==(const MeshSpec& o) const
    {
        return data == o.data && width == o.width && depth == o.depth && height == o.height;
    }
};

using PrimitiveSpec =
    std::variant<BoxSpec, CylinderSpec, SphereSpec, ConeSpec, TorusSpec, TubeSpec, WedgeSpec, PyramidSpec, TextSpec,
                 MeshSpec>;

// The kinds of primitive the Add Shape menu offers (every kind but the imported
// mesh, which is last), and a new one of each.
inline constexpr std::size_t kPrimitiveKindCount = std::variant_size_v<PrimitiveSpec> - 1;
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
    // Fields with the same non-zero group are the X / Y (/ Z) radii of one round
    // part. While "Round" is ticked they change together; the first field of the
    // group leads, the others follow it.
    int roundGroup = 0;
};

const std::vector<SpecField>& specFields(const PrimitiveSpec& spec);
double specValue(const PrimitiveSpec& spec, std::size_t field);
void setSpecValue(PrimitiveSpec& spec, std::size_t field, double value);
// Every length times factor (keeps the shape's proportions).
PrimitiveSpec scaled(const PrimitiveSpec& spec, double factor);
// True for shapes that are round seen from above (cylinder, sphere, cone, torus, tube),
// even when oval.
bool isRoundFromAbove(const PrimitiveSpec& spec);
// Whether the shape has radii that "Round" can link.
bool hasRoundGroups(const PrimitiveSpec& spec);
// Every round group has equal values (a circle, not an oval). True for shapes with
// no round groups.
bool isRound(const PrimitiveSpec& spec);
// The same shape made round: every field in a round group takes its group leader's
// value. Can give an invalid spec (e.g. a hole no longer inside); callers validate.
PrimitiveSpec madeRound(const PrimitiveSpec& spec);

// ---- Combine results ------------------------------------------------------------

enum class BooleanOp { Merge, Subtract, Intersect, Split, Mirror, Group, Emboss };

// Words raised from (or engraved into) a shape's surface. Each letter follows the
// surface: every point of it stands `depth` out from (or sinks `depth` into) the
// surface under it.
struct EmbossText {
    std::string text = "Text";
    std::string font;  // empty = the default font
    bool bold = true;
    bool italic = false;
    bool vertical = false;      // letters stacked top to bottom (each upright)
    double letterHeight = 8.0;  // height of a capital letter, mm
    double depth = 1.0;         // how far the letters stand out / go in, mm
    bool raised = true;         // false: engraved
    double angle = 0.0;         // turn of the words on the surface, degrees (anticlockwise)
    bool operator==(const EmbossText&) const = default;
};

// "Merge", "Subtract", "Intersect", "Split", "Mirror", "Group"
std::string toString(BooleanOp op);

// A body made by combining other bodies (its parts, or "operands").
//   Merge:     everything in any part.
//   Subtract:  the first part, minus all the others.
//   Intersect: only what every part shares.
//   Split:     one part, cut by a plane; only the side the plane's normal points
//              to is kept. (Keeping both halves makes two Split results, one on a
//              copy of the part.)
//   Mirror:    one part, reflected in the plane (planePoint / planeNormal).
//   Group:     the parts kept as they are, side by side, moving as one (no
//              merging: they may overlap or be far apart).
//   Emboss:    one part with words raised from or engraved into its surface,
//              centred on planePoint (on the surface) and facing out along
//              planeNormal; see `emboss`.
// The parts stay in the document, hidden inside the result and still editable;
// their placements are relative to the result. A part belongs to at most one result.
struct BooleanSpec {
    BooleanOp op = BooleanOp::Merge;
    std::vector<ObjectId> operands;
    // The point the result turns around, in its own coordinates. Fixed when the
    // result is made, so editing a part never makes the result jump.
    std::array<double, 3> pivot{0.0, 0.0, 0.0};
    // Split: the cutting plane in the result's own coordinates, as a point on it and
    // a unit normal pointing into the half that is kept. Mirror: the mirror plane.
    std::array<double, 3> planePoint{0.0, 0.0, 0.0};
    std::array<double, 3> planeNormal{0.0, 0.0, 1.0};
    // Emboss: the words.
    std::optional<EmbossText> emboss{};

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
