// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Sketch.h"

#include <array>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <map>
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

// A sketch pushed up into a solid: its closed outlines (holes inside outlines stay
// holes) rise `length` from the sketch plane (local XY), or sink below it when
// reversed, or go half each way (midplane).
struct PadSpec {
    sketch::Sketch sketch;
    double length = 10.0;
    bool reversed = false;
    bool midplane = false;
    // Draft: degrees the sides lean in as they go away from the sketch (0: straight up;
    // negative: they lean out). Holes in the outline widen as the outside narrows.
    double taper = 0.0;
    bool operator==(const PadSpec&) const = default;
};

// A sketch spun around one of its own lines (`axis`: a helper line of the sketch) by
// `angle` degrees, anticlockwise looking down the line from its first point (the other
// way when reversed; half each way for midplane).
struct RevolveSpec {
    sketch::Sketch sketch;
    int axis = -1;  // index of the line in sketch.geos
    double angle = 360.0;
    bool reversed = false;
    bool midplane = false;
    bool operator==(const RevolveSpec&) const = default;
};

// A solid blended between outlines stacked up (+Z) from the sketch plane: the first
// at height 0, each next higher (a square base to a round top, a vase).
struct LoftSection {
    sketch::Sketch sketch;  // one closed outline
    double height = 0.0;    // above the sketch plane
    bool operator==(const LoftSection&) const = default;
};
struct LoftSpec {
    std::vector<LoftSection> sections;  // at least two, bottom first, each higher
    bool ruled = false;                 // straight sides between them (else a smooth blend)
    bool operator==(const LoftSpec&) const = default;
};

// A cross-section swept along a path. Along a drawn path: the outline drawn in the
// sketch plane, the path drawn standing up from it (path sketch X along the sketch's X,
// its Y up out of the plane, +Z), starting at the sketch's origin. Along a spiral (a
// spring, a coil): the outline drawn as seen from the side, going round the sketch's Y
// line (like Revolve) and rising `pitch` along it each turn.
struct SweepSpec {
    sketch::Sketch profile;
    bool spiral = false;
    sketch::Sketch path;   // a drawn path: lines and arcs joined end to end
    double pitch = 5.0;    // a spiral: how far it rises each turn
    double turns = 3.0;
    bool leftHand = false;
    bool operator==(const SweepSpec&) const = default;
};

// Holes drilled down (-Z) from the sketch plane at every loose point of the sketch
// (points not used by any line or circle). The tool body of a Hole step (a Subtract).
enum class HoleKind { Plain, Counterbored, Countersunk };
struct HoleSpec {
    sketch::Sketch sketch;  // where the holes go
    HoleKind kind = HoleKind::Plain;
    std::string size;        // "M3", "M4", ... or empty for sizes typed by hand
    bool tapped = false;     // sized for cutting a thread (tap drill), else for a bolt to pass
    double diameter = 3.4;
    double depth = 10.0;
    double headDiameter = 6.5;       // counterbore
    double headDepth = 3.4;          // counterbore
    double countersinkDiameter = 6.7;
    double countersinkAngle = 90.0;  // degrees, the cone's full angle
    bool operator==(const HoleSpec&) const = default;
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
                 PadSpec, RevolveSpec, HoleSpec, LoftSpec, SweepSpec, MeshSpec>;

// The kinds of primitive the Add Shape menu offers (every kind but those made from a
// sketch (pad, revolve, hole, loft, sweep) and an imported mesh, the last six), and a
// new one of each.
inline constexpr std::size_t kPrimitiveKindCount = std::variant_size_v<PrimitiveSpec> - 6;
// Made from a sketch: a pad, revolve or hole.
bool isSketched(const PrimitiveSpec& spec);
const sketch::Sketch* sketchOf(const PrimitiveSpec& spec);
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

enum class BooleanOp {
    Merge, Subtract, Intersect, Split, Mirror, Group, Emboss, Edges, Shell, Follow, Pattern, Sculpt, Simplify, Thread,
    Smooth,       // the whole surface smoothed (mesh engine)
    ExtrudeDown,  // the underside stretched straight down to a plane (planePoint / planeNormal: up)
    Socket,       // a ball or peg socket cut in at a spot (planePoint, facing out: planeNormal)
    Hinge,        // one half of a hinge or swivel at a spot (planePoint, facing out: planeNormal)
    MaskSplit,    // the part cut along the edge of a painted mask: the masked piece or the rest
    Hollow,       // hollowed for resin printing, with drain holes (mesh engine; see `hollow`)
    Stamp,        // small details stamped on: rivets, bolts, vents... raised or sunk (see `stamp`;
                  // a second operand, if any, is the shape used as the stamp)
    PanelLines,   // thin grooves cut along lines drawn on the surface (see `panelLines`)
    Posed,        // a figure bent at its joints by a simple skeleton inside it (see `posed`)
    Paint,        // colours brushed onto the surface (see `paint`; the shape stays as it was)
    LimbCut,      // a figure cut at a joint: the limb, or the rest (see `limbCut`)
    Mould,        // one piece of a mould box round the part (see `mould`)
    Port,         // a wrist or accessory peg added, or a grip hole cut through (see `port`)
    Linked,       // another shape mirrored, kept up to date with it (see `link`; no parts)
};

// What happens to the picked edges of a shape.
enum class EdgeFinishKind {
    Rounding,  // outside (convex) edges turned into a curve of `size` radius
    Fillet,    // inside (concave) corners filled with a curve of `size` radius
    Chamfer,   // the corner cut off flat, `size` back along both faces
    Bevel,     // a flat slope `size` wide on the picked face, at `angle` to that face
};

// One picked edge, found again by place: a point on it, and a point on the face it
// was picked from (Bevel measures from that face). Result coordinates.
struct EdgeRef {
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> facePoint{0.0, 0.0, 0.0};
    // Where the edge sits within the part's box (0..1 on each axis), and which way the
    // face it was picked from points: these find it again after the part is resized.
    bool located = false;
    std::array<double, 3> relative{0.0, 0.0, 0.0};
    std::array<double, 3> faceNormal{0.0, 0.0, 0.0};
    bool operator==(const EdgeRef&) const = default;
};

struct EdgeFinish {
    EdgeFinishKind kind = EdgeFinishKind::Rounding;
    double size = 1.0;    // radius (Rounding / Fillet), distance (Chamfer), width on the face (Bevel)
    double angle = 45.0;  // Bevel: degrees between the slope and the picked face
    std::vector<EdgeRef> edges;
    bool operator==(const EdgeFinish&) const = default;
};

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

// A picture's pixels as shades of grey, 0 black to 255 white, row by row from the top
// (transparent parts already turned white). Shared, never changed once made.
struct PictureData {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> grey;  // width * height
    bool operator==(const PictureData&) const = default;
};

// A picture raised from (or engraved into) a shape's surface, following it like words.
//   Logo:  the dark parts (darker than `threshold`; the light ones if `invert`) traced
//          to a clean outline at one height, `depth`.
//   Photo (relief): every shade a height, black `depth` and white nothing (the other
//          way round if `invert`).
struct EmbossPicture {
    std::string name;  // the file it came from, for showing
    std::shared_ptr<const PictureData> image;
    bool relief = false;    // true: photo relief; false: logo
    int threshold = 128;    // logo: shades darker than this make the shape (1..254)
    bool invert = false;    // light parts make the shape / stand highest
    double width = 30.0;    // across the surface, mm (the height follows the picture's shape)
    double depth = 1.0;     // how far it stands out / goes in, mm (the most, for a relief)
    bool raised = true;     // false: engraved
    double angle = 0.0;     // turn on the surface, degrees (anticlockwise)
    bool operator==(const EmbossPicture& o) const
    {
        const bool sameImage = image == o.image || (image && o.image && *image == *o.image);
        return sameImage && name == o.name && relief == o.relief && threshold == o.threshold &&
               invert == o.invert && width == o.width && depth == o.depth && raised == o.raised &&
               angle == o.angle;
    }
};

// One brush stroke of a Sculpt step, kept so it can be played again on a changed shape.
// Points are in the sculpted part's own coordinates (they move and turn with it).
struct SculptStroke {
    int brush = 0;  // sculpt::Brush
    bool invert = false;
    double radius = 1.0;
    double strength = 0.5;  // 0..1
    bool mirrorX = false;
    bool mirrorY = false;
    bool mirrorZ = false;
    double finest = 0.0;  // detail added under the brush (0: none)
    std::vector<std::array<float, 3>> dabs;  // where each dab went (Grab: where it started)
    std::array<float, 3> grab{0.f, 0.f, 0.f};  // Grab: how far it was pulled
    int pattern = -1;    // Pattern: which of the step's patterns (SculptFinish::patterns)
    double tile = 0.0;   // Pattern: how wide one repeat of it is, mm (a stamp: how wide it is)
    double turn = 0.0;   // Pattern: degrees the picture is turned round the surface's facing
    bool once = false;   // Pattern: pressed once, as a stamp centred on the first dab (not repeating)
    bool operator==(const SculptStroke&) const = default;
};

// A pattern the Pattern brush presses in: one of the built-in ones (by name), or a
// picture of the user's (light parts stand highest; it repeats side by side).
struct SculptPattern {
    std::string name;  // built-in: see sculpt::builtInPatterns(); else the file
    bool builtIn = true;
    std::shared_ptr<const PictureData> image;  // a picture of the user's (not built-in)
    bool operator==(const SculptPattern& o) const
    {
        const bool sameImage = image == o.image || (image && o.image && *image == *o.image);
        return sameImage && name == o.name && builtIn == o.builtIn;
    }
};

// A Sculpt step: the starting detail and the strokes, in order. The shape under it is
// divided finely (detail 0 Low, 1 Medium, 2 High) and the strokes played on it.
struct SculptFinish {
    int detail = 1;
    std::vector<SculptStroke> strokes;
    std::vector<SculptPattern> patterns;  // the Pattern brush's, by number from the strokes
    bool operator==(const SculptFinish&) const = default;
};

// A Simplify step: the part's triangles thinned to this share of them (0.01..1).
struct SimplifyFinish {
    double keep = 0.5;
    bool operator==(const SimplifyFinish&) const = default;
};

// A Smooth step: how strongly the whole surface is smoothed (1 gentle .. 10 strong).
struct SmoothFinish {
    int strength = 3;
    bool operator==(const SmoothFinish&) const = default;
};

// One picked face, found again by place (like EdgeRef): a point on it, which way it
// faces there, and (once located) where that point sits in the part's box.
struct FaceRef {
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> normal{0.0, 0.0, 1.0};
    bool located = false;
    std::array<double, 3> relative{0.0, 0.0, 0.0};
    bool operator==(const FaceRef&) const = default;
};

// A solid hollowed out: walls `thickness` thick (inside the old surface, or outside it
// when outward), with the picked faces left open.
struct ShellFinish {
    double thickness = 2.0;
    bool outward = false;
    std::vector<FaceRef> open;
    bool operator==(const ShellFinish&) const = default;
};

// A Thread step: a screw thread cut into a round face, a peg (outside thread) or a hole
// (inside thread). Sizes in mm; `fit` loosens it for printing (the peg's thread made
// that much smaller, the hole's that much bigger, all round).
struct ThreadFinish {
    FaceRef face;          // the round face, picked like Shell's open faces
    double size = 6.0;     // the thread's nominal (outside) diameter, M6: 6
    double pitch = 1.0;    // from one turn to the next
    double fit = 0.2;
    bool leftHand = false;
    bool operator==(const ThreadFinish&) const = default;
};

// A Quick Socket: a hole cut into the surface at a spot (the step's planePoint, facing
// out along planeNormal), sized to take a ball or a peg once printed. Sizes in mm.
// Magnet: a round pocket the magnet sits in (size: its diameter, depth: its thickness).
enum class SocketKind { Ball, Peg, Magnet };
enum class PegShape { Round, Square, Hexagon, Octagon };
struct SocketFinish {
    SocketKind kind = SocketKind::Ball;
    double size = 3.0;    // the ball's diameter; the peg's diameter (round) or across its flats
    double depth = 4.0;   // Peg: how far it goes in
    double fit = 0.1;     // room all round (and at the bottom of a peg socket) for printing
    bool resin = true;    // the printer the fit was chosen for (Resin 0.1, FDM 0.2 to start)
    bool snap = true;     // Ball: the opening narrower than the ball, so it snaps in and stays
    double grip = 10.0;   // Ball, snap: how much narrower, as a percentage of the ball
    PegShape shape = PegShape::Round;
    double turn = 0.0;    // degrees round the socket's axis (a peg's flats, the relief cuts)
    // Strain relief: slots from the opening down through the socket's side, so it can open
    // as the ball or peg goes in. 0 = none.
    int reliefCuts = 0;
    double reliefWidth = 0.4;
    double reliefDepth = 0.0;  // how far down from the opening; 0: to the ball's middle (pegs: half way)
    // Ball: ridges left inside the cup, from the opening down, that press on the ball for
    // a stiffer hold. 0: none.
    int ridges = 0;
    double ridgeHeight = 0.2;  // how far they stand in (more than the fit, to grip)
    // Magnet: a skin left over it (0: an open pocket, the magnet flush with the surface;
    // more: buried, dropped in during a pause in the print).
    double cover = 0.0;
    bool operator==(const SocketFinish&) const = default;
};

// A Quick Hinge: one half of a hinge or swivel joint at a spot (the step's planePoint,
// on the fork's surface, facing out of the fork along planeNormal). Sizes in mm.
//  - Single hinge: a fork (a slot with a pin hole across it) and a tab with a rounded
//    end that turns in it on a pin.
//  - Double hinge: a fork in each part and a link between them, with two pins.
//  - Swivel: a bore with a groove, and a peg with a lip that snaps into the groove.
enum class HingeKind { Single, Double, Swivel };
// What this step makes of its part: the fork (Swivel: the bore) cut in, or the part's
// end made into the tab (Swivel: the peg).
enum class HingeHalf { Fork, Tab };
// The pin: printed with the parts, a length of 1.75 mm filament, or a wire or rod.
enum class PinKind { Printed, Filament, Rod };
// Which way a hinge can bend from straight: the slot is open deeper on that side.
enum class HingeSwing { Both, Forward, Back };
// Which piece of a hinge a shape is (slice 82): the Hinge step it goes with, its place
// in hinge::pieces, how that was made (with a tab; the printed pin's span), where it was
// put (if it's still there it moves with the hinge; if it's been moved it stays), and
// the name it was given (a name still starting with it gets the new sizes).
struct JointPiece {
    ObjectId step = 0;
    int index = 0;
    bool withTab = false;
    std::array<double, 2> pinSpan{0.0, 0.0};
    std::array<double, 3> madeAt{0.0, 0.0, 0.0};
    std::string label;
    bool operator==(const JointPiece&) const = default;
};

struct HingeFinish {
    HingeKind kind = HingeKind::Single;
    HingeHalf half = HingeHalf::Fork;
    double width = 3.0;    // the tab's thickness, along the pin; Swivel: the peg's diameter
    double barrel = 5.0;   // the tab's rounded end, across; Swivel: how far the peg goes in
    PinKind pin = PinKind::Printed;
    double pinSize = 2.0;  // the pin's diameter (filament: 1.75)
    double fit = 0.1;      // room all round, for printing
    bool resin = true;     // the printer the fit was chosen for
    bool stiff = true;     // the tab grips the pin, to hold a pose (else it turns freely)
    HingeSwing swing = HingeSwing::Both;
    double turn = 0.0;     // degrees round the spot: which way the pin lies
    double reach = 0.0;    // how far the slot and the pin hole go out each side; 0: auto
    double gap = 1.0;      // Double: room between the two parts' faces
    double lip = 0.4;      // Swivel: how far the lip stands out
    // Clicks (a ratchet): ridges on the tab's faces (Swivel: along the peg) that snap into
    // grooves in the fork's arms (the bore), so the joint holds a pose. 0: none.
    int clicks = 0;          // per turn
    double clickDepth = 0.4;  // how far the ridges stand out (more than the fit, to click)
    std::uint32_t link = 0;  // both halves of one hinge share this number (0: none)
    bool operator==(const HingeFinish&) const = default;
};

// Split by Mask: a mask painted on a part with a round brush, as dabs in order (a later
// dab wins where they overlap: a point is masked if the last dab covering it painted).
// The part is cut along the mask's edge and each piece closed with a smooth cap.
struct MaskDab {
    std::array<double, 3> centre{0.0, 0.0, 0.0};  // the step's coordinates
    double radius = 2.0;
    bool erase = false;
    bool operator==(const MaskDab&) const = default;
};
// A Hollow step: the part hollowed out to walls `thickness` thick (any shape, sculpts
// and imported meshes too), with round drain holes through the wall so resin can run
// out. Hole spots are in the step's coordinates: a point on the outside and the way out.
struct DrainHole {
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> normal{0.0, 0.0, -1.0};
    bool operator==(const DrainHole&) const = default;
};
struct HollowFinish {
    double thickness = 2.0;
    double holeSize = 3.0;  // the drain holes' diameter
    std::vector<DrainHole> holes;
    bool operator==(const HollowFinish&) const = default;
};

// A Stamp step: small details stamped onto (raised) or into (sunk) the surface, each at a
// spot on it. Spots are in the step's coordinates: a point on the surface, the way out,
// and a direction along the surface (the stamp's own X: a row's direction).
enum class StampKind { Rivet, HexBolt, Screw, Stud, Vent, Scale, Stitch, Custom };
struct StampSpot {
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> normal{0.0, 0.0, 1.0};
    std::array<double, 3> along{1.0, 0.0, 0.0};
    bool operator==(const StampSpot&) const = default;
};
struct StampFinish {
    StampKind kind = StampKind::Rivet;  // Custom: the step's second operand is the stamp
    double size = 3.0;     // across, mm (the widest way)
    double height = 1.0;   // how far it stands out (or goes in), mm
    bool sunk = false;     // cut in instead of standing out
    double spin = 0.0;     // degrees about the way out, from `along`
    double spacing = 5.0;  // between stamps in a row, mm (middle to middle)
    std::vector<StampSpot> spots;
    bool operator==(const StampFinish&) const = default;
};

// Panel lines: thin grooves cut along lines drawn on the surface (armour plates,
// vehicle panels). Each line is a run of points on the surface (close together, so it
// follows the surface), each with the way out; in the step's coordinates.
enum class GrooveProfile { V, Square, Round };
struct PanelPoint {
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> normal{0.0, 0.0, 1.0};
    bool operator==(const PanelPoint&) const = default;
};
struct PanelLine {
    std::vector<PanelPoint> points;
    bool closed = false;  // back round to the first point
    bool operator==(const PanelLine&) const = default;
};
struct PanelLineFinish {
    GrooveProfile profile = GrooveProfile::V;
    double width = 0.6;  // across the top, mm
    double depth = 0.5;  // mm
    std::vector<PanelLine> lines;
    bool operator==(const PanelLineFinish&) const = default;
};

// How Split by Mask's cut surface is made (slice 87).
enum class MaskCut {
    Curved,  // a smooth surface spanning the mask's edge (as the mask runs round the shape)
    Flat,    // a flat cut through where the mask's edge runs
    Dome,    // flat, with a dome on the masked piece and a socket for it in the rest
};

struct MaskSplitFinish {
    std::vector<MaskDab> dabs;
    bool masked = true;  // this piece is the masked part (false: the rest)
    MaskCut cut = MaskCut::Curved;
    double fit = 0.15;   // Dome: the socket this much bigger all round
    // Alignment keys on each cut, so the pieces only go back together one way: a
    // three-sided peg on the masked piece, a hole `keyFit` bigger all round in the rest.
    bool keys = false;
    double keyFit = 0.1;
    bool operator==(const MaskSplitFinish&) const = default;
};

// Follow: a groove cut along (or a ridge raised along) a path drawn on the surface.
// The path is a sketch drawn at a spot on the surface (`origin`, facing `normal`, its X
// along `xAxis`) and laid over the surface from there: on a flat face it stays as
// drawn, on a curved one it bends round with the surface. The cross-section is a
// ready-made shape `width` across and `depth` deep (high), or one drawn in `custom`
// (X across the path, Y up from the surface).
enum class FollowProfile { Round, Square, V, Custom };
struct FollowFinish {
    sketch::Sketch path;
    std::array<double, 3> origin{0.0, 0.0, 0.0};
    std::array<double, 3> xAxis{1.0, 0.0, 0.0};
    std::array<double, 3> normal{0.0, 0.0, 1.0};
    bool located = false;  // relative: where the origin sits in the part's box
    std::array<double, 3> relative{0.0, 0.0, 0.0};
    bool ridge = false;    // raised instead of cut
    FollowProfile profile = FollowProfile::Square;
    double width = 2.0;
    double depth = 1.0;
    sketch::Sketch custom;
    bool operator==(const FollowFinish&) const = default;
};

// Pattern: copies in a row, a grid or round a circle, of the whole part or of the
// feature its last step added (a hole set, pad, pocket, groove...), linked: change the
// original and the copies follow. Directions and the circle's centre are in the step's
// own coordinates.
enum class PatternKind { Row, Grid, Circle };
struct PatternFinish {
    PatternKind kind = PatternKind::Row;
    bool feature = false;  // repeat the part's last step (its added / cut shape), not all of it
    std::array<double, 3> dir1{1.0, 0.0, 0.0};
    std::array<double, 3> dir2{0.0, 1.0, 0.0};  // grid
    int count1 = 3;
    int count2 = 2;
    double spacing1 = 10.0;
    double spacing2 = 10.0;
    std::array<double, 3> centre{0.0, 0.0, 0.0};  // circle
    std::array<double, 3> axis{0.0, 0.0, 1.0};
    int count = 6;
    double angle = 360.0;  // spread over this (360: evenly all the way round)
    bool operator==(const PatternFinish&) const = default;
};

// "Merge", "Subtract", "Intersect", "Split", "Mirror", "Group"
std::string toString(BooleanOp op);

// A body made by combining other bodies (its parts, or "operands").
// A Posed step: one figure (a sculpt, or an imported mesh) bent at its joints, the
// surface following (see rig::). `points` are the skeleton's points where they sit in
// the figure as it was made (the step's coordinates, rig::Point order; the figure stands
// up +Z and faces -Y); `angles` the pose (rig::Joint order, degrees; the figure guide's
// angles, 0 standing straight).
inline constexpr int kRigPointCount = 19;
inline constexpr int kRigJointCount = 14;
struct PosedFinish {
    std::array<std::array<double, 3>, kRigPointCount> points{};
    std::array<std::array<double, 2>, kRigJointCount> angles{};
    double blend = 4.0;     // mm: how far a bend spreads either side of a joint
    bool feetFlat = true;   // the feet keep their angle to the ground (ankle angles unused)
    bool onGround = true;   // moved up or down so it stands where it stood
    bool operator==(const PosedFinish&) const = default;
};

// A Paint step: colours brushed onto the surface with a round brush, as dabs in order (a
// later dab wins where they overlap; an `erase` dab gives the shape its own colour back).
// Each triangle takes the colour of the last dab over its middle. Dabs are in the step's
// coordinates; colours 0xRRGGBB.
struct PaintDab {
    std::array<double, 3> centre{0.0, 0.0, 0.0};
    double radius = 2.0;
    std::uint32_t colour = 0;
    bool erase = false;
    bool operator==(const PaintDab&) const = default;
};
struct PaintFinish {
    std::vector<PaintDab> dabs;
    bool operator==(const PaintFinish&) const = default;
};

// A Limb Cut step (Cut into Parts): a figure cut at a joint, square to the limb. The cut
// is the plane through `point` facing `normal` (into the limb); the limb is what of the
// part lies beyond it inside `reach` (rounded rods along the limb's bones, as fat as the
// limb). `gap` opens the cut up: each side is cut back half of it. `joint` is the
// rig::Joint cut at. In the step's coordinates.
struct LimbRod {
    std::array<double, 3> a{0.0, 0.0, 0.0}, b{0.0, 0.0, 0.0};
    double radius = 1.0;
    bool operator==(const LimbRod&) const = default;
};
struct LimbCutFinish {
    int joint = 0;
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> normal{0.0, 0.0, 1.0};
    std::vector<LimbRod> reach;
    double gap = 0.0;
    bool limb = true;  // this piece is the limb (false: the rest)
    bool operator==(const LimbCutFinish&) const = default;
};

// A linked mirror (slice 74): a shape that is another shape (`source`) mirrored in a
// plane, kept up to date as that shape changes: steps added on top of the source (a
// Sculpt, a socket...) show too, as long as the source is the first part of each. It has
// no parts of its own; on the plate it starts where the mirror puts it (plate
// coordinates are its own) and can be moved from there.
struct MirrorLink {
    ObjectId source = kInvalidId;
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> normal{1.0, 0.0, 0.0};
    bool operator==(const MirrorLink&) const = default;
};

// A Port step (hand and accessory ports): a peg added to the part at a spot (a wrist peg
// for swappable hands, a handle on an accessory), standing out along `direction`, or a
// round grip hole cut through it for a handle, from the spot in along -`direction` (or
// towards `exit`). Sizes in mm; in the step's coordinates.
enum class PortKind { Peg, Grip };
// A peg's far end: flat, with its edge taken off (easier to push in), or a ball (for a
// ball socket: a ball-jointed wrist).
enum class PegEnd { Flat, Chamfer, Ball };
struct PortFinish {
    PortKind kind = PortKind::Peg;
    PegShape shape = PegShape::Round;  // a peg's (a grip hole is round)
    double size = 3.0;    // the peg's diameter (round) or across its flats; the handle's diameter
    double length = 4.0;  // Peg: how far it stands out (Ball: to the ball's middle)
    double fit = 0.1;     // Grip: room all round
    PegEnd end = PegEnd::Chamfer;
    double ball = 4.0;    // Peg, Ball end: the ball's diameter
    double turn = 0.0;    // degrees round the peg's axis (its flats)
    std::array<double, 3> point{0.0, 0.0, 0.0};
    std::array<double, 3> direction{0.0, 0.0, 1.0};  // out of the surface
    bool hasExit = false;  // Grip: aimed at a way out (else straight in, through)
    std::array<double, 3> exit{0.0, 0.0, 0.0};
    bool operator==(const PortFinish&) const = default;
};

// A Mould step: one piece of a mould box round the part, for casting copies of it. The
// box is the part's box with `wall` all round, the part taken out, parted by a flat face
// square to `axis` (2: level, top and bottom halves; 0: upright, left and right; 1:
// upright, front and back) at `parting` along it. A second cut (three or four pieces)
// square to `axis2` at `parting2` goes through both halves (`secondIn` 0) or only the
// half on one side of the first (+1: the `top` side, -1: the other). Registration keys
// near the corners of each cut: pegs on the piece on the low side, holes (bigger by
// `keyFit`) on the high side. A pour spout from `spout` (where it meets the part, near its
// top) and air vents from each of `vents` run straight up out of the top. In the step's
// coordinates.
struct MouldFinish {
    bool top = false;       // this piece is on the side `axis` points to (top, right or back)
    int axis = 2;
    double parting = 0.0;   // where the first cut is, along `axis`
    bool second = false;    // a second cut
    int axis2 = 0;
    double parting2 = 0.0;
    int secondIn = 0;
    int side2 = 0;          // this piece's side of the second cut (+1: the way axis2 points; 0: not cut)
    double wall = 8.0;      // mm round the part
    bool keys = true;
    double keySize = 6.0;   // a key's diameter at the parting face
    double keyFit = 0.2;    // added to a key hole's radius
    double spoutSize = 6.0;
    std::array<double, 3> spout{0.0, 0.0, 0.0};
    double ventSize = 2.0;
    std::vector<std::array<double, 3>> vents;
    bool operator==(const MouldFinish&) const = default;
};

//   Merge:     everything in any part.
//   Subtract:  the first part, minus all the others.
//   Intersect: only what every part shares.
//   Split:     one part, cut by a plane; only the side the plane's normal points
//              to is kept. (Keeping both halves makes two Split results, one on a
//              copy of the part.)
//   Mirror:    one part, reflected in the plane (planePoint / planeNormal).
//   Group:     the parts kept as they are, side by side, moving as one (no
//              merging: they may overlap or be far apart).
//   Edges:     one solid part with some edges rounded, filleted, chamfered or
//              bevelled; see `edges`.
//   Shell:     one solid part hollowed out; see `shell`.
//   Follow:    one solid part with a groove or ridge along a path; see `follow`.
//   Pattern:   one part repeated (or its last feature repeated on it); see `pattern`.
//   Emboss:    one part with words (or a picture) raised from or engraved into its
//              surface, centred on planePoint (on the surface) and facing out along
//              planeNormal; see `emboss` / `picture`.
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
    // Emboss: the words, or (instead) a picture.
    std::optional<EmbossText> emboss{};
    std::optional<EmbossPicture> picture{};
    // Sculpt: the strokes.
    std::optional<SculptFinish> sculpt{};
    // Simplify: how many of the triangles to keep.
    std::optional<SimplifyFinish> simplify{};
    // Smooth: how strongly.
    std::optional<SmoothFinish> smooth{};
    // Thread: where and what size.
    std::optional<ThreadFinish> thread{};
    // Socket: what it takes (the spot is planePoint / planeNormal).
    std::optional<SocketFinish> socket{};
    // Hinge: which half and its settings (the spot is planePoint / planeNormal).
    std::optional<HingeFinish> hinge{};
    // Split by Mask: the painted mask, and which piece this is.
    std::optional<MaskSplitFinish> maskSplit{};
    // Hollow: wall thickness and drain holes.
    std::optional<HollowFinish> hollow{};
    // Stamp: what, how big, and where.
    std::optional<StampFinish> stamp{};
    // Panel lines: the groove and the lines.
    std::optional<PanelLineFinish> panelLines{};
    // Posed: the skeleton and the pose.
    std::optional<PosedFinish> posed{};
    // Paint: the colours brushed on.
    std::optional<PaintFinish> paint{};
    // Limb Cut: where, and which piece.
    std::optional<LimbCutFinish> limbCut{};
    // Mould: which half, and the box's settings.
    std::optional<MouldFinish> mould{};
    // Port: the peg or grip hole.
    std::optional<PortFinish> port{};
    // Linked: the shape mirrored, and the mirror.
    std::optional<MirrorLink> link{};
    // Edges: what to do to which edges.
    std::optional<EdgeFinish> edges{};
    // Shell: how thick, and which faces are left open.
    std::optional<ShellFinish> shell{};
    // Follow: the path, its cross-section, groove or ridge.
    std::optional<FollowFinish> follow{};
    // Pattern: what's repeated, and where the copies go.
    std::optional<PatternFinish> pattern{};
    // Subtract: straight tools (box, cylinder, tube, text) are stretched along their own
    // height to go right through the first part, whatever its size ("through all").
    bool throughAll = false;

    bool operator==(const BooleanSpec&) const = default;
};

// A solid object in the document: either a primitive (spec) or a combine result
// (boolean). For a result, spec is unused.
// Shapes without a colour of their own show in these, in turn (by id).
inline constexpr std::array<std::uint32_t, 6> kDefaultColours{
    0x478CD9,  // blue
    0xED8C33,  // orange
    0x59B366,  // green
    0x9E73CC,  // purple
    0x33ADB3,  // teal
    0xE06B85,  // rose
};

struct Body {
    ObjectId id = kInvalidId;
    std::string name;
    PrimitiveSpec spec;
    Placement placement;
    std::optional<BooleanSpec> boolean;
    // A combine step can be switched off: by hand (suppressed), or because the rollback
    // bar is below it (rolledBack). Off, it passes its shape through: its first part
    // (one-part steps, Subtract), or its parts side by side (Merge, Intersect, Group).
    bool suppressed = false;
    bool rolledBack = false;
    // A colour chosen for this shape, 0xRRGGBB. None: a combined shape takes its first
    // part's colour; anything else one from the default palette (see Document::colourOf).
    std::optional<std::uint32_t> colour;
    // A joint part (a ball, a peg, a hinge pin or tab, a split peg or its hole, a
    // stand-in magnet): Rescale with "keep joint sizes" moves it with the design but
    // leaves its size alone.
    bool hardware = false;
    // Sizes and positions that follow the design variables (Variables.h): the field's key
    // ("width", "at.x"...) and its formula ("PegDia + 0.4").
    std::map<std::string, std::string> formulas;
    // A piece Quick Hinge made to go with a Hinge step (a tab, link, peg or pin): remade
    // when the hinge's sizes change (slice 82).
    std::optional<JointPiece> jointPiece{};

    bool isBoolean() const { return boolean.has_value(); }
    bool isOff() const { return boolean.has_value() && (suppressed || rolledBack); }
    bool operator==(const Body&) const = default;
};

// "Box", "Cylinder", "Merge", "Subtract", "Intersect"
std::string typeName(const Body& body);

}  // namespace forge
