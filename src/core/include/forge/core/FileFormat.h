// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Pose.h"
#include "forge/core/Mannequin.h"
#include "forge/core/Reference.h"
#include "forge/core/Variables.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace forge {
class Document;
}

// The .f3d file: a zip archive holding document.json (and, later, sketches,
// sculpt layers and a preview model). A zip is easy to inspect and lets us add
// parts without breaking older files.
namespace forge::file {

// Version history (readers must keep accepting every older version):
//   1  slice 3a: box and cylinder, placement x/y/z + rotationZ
//   2  rotationX and rotationY added (rotation about all three axes)
//   3  cylinders store radiusX and radiusY (elliptical cylinders)
//   4  combine results (type "combine": operation, operands, pivot)
//   5  sphere, cone, torus, tube, wedge
//   6  oval sphere / cone / torus / tube (X / Y radii), pyramid
//   7  split results (operation "split", planePoint, planeNormal)
//   8  mirror results (operation "mirror", plane) and groups (operation "group")
//   9  imported meshes (type "mesh", width / depth / height, "data": zip entry name)
//  10  text (type "text": text, font, bold, italic) and emboss results (operation
//      "emboss", planePoint / planeNormal, "emboss": the words)
//  11  edge finishes (operation "edges"; "edges": kind, size, angle, picks)
//  12  steps switched off: "suppressed" / "rolledBack" on results (missing = false)
//  13  sketch pads (type "pad": length, reversed, midplane, "sketch": points, radii,
//      geos, rules)
//  14  revolves (type "revolve": axis, angle, sketch), holes (type "hole": kind, size,
//      diameters, sketch) and shells (operation "shell"; "shell": thickness, outward,
//      open faces)
//  15  follow steps (operation "follow": path sketch, where it starts, cross-section)
//  16  pattern steps (operation "pattern": kind, what, directions, counts, spacing, circle)
//  17  emboss pictures ("picture": logo or relief, settings, and the image's grey pixels,
//      deflated, in base64)
//  18  sculpt steps (operation "sculpt": detail and strokes: brush, size, strength,
//      mirrors, detail, dab points, Grab offset)
//  19  sculpt brushes Crease, Snake hook and the mask (paint, clear, invert)
//  20  simplify steps (operation "simplify": the share of triangles kept); Remesh in
//      sculpting (a stroke with the triangle size as its radius)
//  21  the Pattern sculpt brush: a step's patterns (built-in by name, or a picture) and
//      each stroke's pattern and repeat size
//  22  thread steps (operation "thread": the round face picked, size, pitch, fit, hand)
//  23  lofts (outlines stacked at heights) and sweeps (an outline along a drawn path
//      or a spiral)
//  24  the sketch rule "gap" (an offset copy's distance from its original); a pad's
//      "taper" (degrees its sides lean in); a sketch's "texts" (words whose letters
//      are outlines)
//  25  Smooth and Extrude Down steps
//  26  Quick Socket steps (operation "socket": a ball or peg socket at a spot)
//  27  Split by Mask steps (operation "maskSplit": the dabs painted, which piece)
//  28  reference images ("references": one per direction, its size, place, opacity,
//      shown and active; the picture file kept as it was in "references/<view>.<kind>")
//  29  Quick Hinge steps (operation "hinge": kind, which half, sizes, pin, fit, swing,
//      turn, and the number both halves of one hinge share)
//  30  saved poses ("poses": a name, the part held still, and each joint's angles by
//      the step that makes the joint)
//  31  clicks on Quick Hinge steps ("clicks", "clickDepth") and ridges in ball
//      sockets ("ridges", "ridgeHeight")
//  32  magnet pockets (a socket's kind "magnet", and "cover": the skin over it)
//      (0.65 adds "hardware": true on joint parts; older versions ignore it)
//  33  the figure guide ("mannequin": height, heads, place, turn, shown, and each
//      joint's angles by name)
//  34  Hollow steps (operation "hollow": wall thickness, drain hole size and spots)
//  35  Stamp steps (operation "stamp": kind, size, height, sunk, spin, spacing, spots)
//  36  Panel Lines steps (operation "panelLines": profile, width, depth, and lines of
//      points, each [x, y, z, normal x, y, z])
//  37  alignment keys on Split by Mask's cuts ("keyFit" in "mask")
//  38  Pose Figure steps (operation "posed": the skeleton's points and each joint's
//      angles by name, blend, feetFlat, onGround)
//  39  Paint steps (operation "paint": "colours" as "#RRGGBB", and dabs, each [x, y, z,
//      radius, colour number], -1 rubbing out)
//  40  Cut into Parts steps (operation "limbCut": the joint, the cut's point and normal,
//      the limb's reach as rods [ax, ay, az, bx, by, bz, radius], gap, and which piece)
//  41  Mould steps (operation "mould": top, parting, wall, keys, keySize, keyFit,
//      spoutSize, spout, ventSize, vents [x, y, z])
//  42  a mould's cut directions and second cut ("axis", "second", "axis2", "parting2",
//      "secondIn", "side2" in "mould")
//  43  Port steps (operation "port": kind peg / grip, shape, size, length, fit, end
//      flat / chamfer / ball, ball, turn, point, direction, hasExit, exit)
//  44  Pattern strokes turned ("turn") and pressed once as stamps ("once"); more built-in
//      patterns (chainmail, twill, knit, leather, cracks, wood, pores)
//  45  Linked mirrors (operation "linked", no parts; "link": source, point, normal)
//  46  Design variables ("variables": name, formula, note) and the fields that follow them
//      (a body's "formulas": field key -> formula)
//  47  Hinge pieces (a body's "jointPiece": step, index, withTab, pinSpan, madeAt, label)
//  48  Mask split cut styles (a mask split's "cut": flat / dome, and the socket's "fit")
//  49  Joints that follow their cut (a split's "cutJoint"; a body's "jointFor")
//  50  Sculpt strength (a sculpt step's "strength", 0..1.5, left out at 1)
//  51  Lassos: a mask lasso ({"loop", "along", "erase"} among a mask's dabs); the sculpt
//      Mask Lasso stroke (brush 14: its dabs the outline, "grab" the way it was seen)
//  52  Proportions (a posed step's "proportions": head, shoulders, torso, arms, hands, legs,
//      feet; left out as made)
//  53  Reshape steps (operation "reshape": the box, its handle counts and moves)
//  54  Figure styles: a posed step's "proportions" gain "limbs" and "body", and "blocky"
//      (left out when 0)
inline constexpr int kFormatVersion = 54;
// The highest sculpt::Brush number a file may hold (Pattern).
inline constexpr int kLastSculptBrush = 14;
inline constexpr const char* kExtension = ".f3d";
inline constexpr const char* kDocumentEntry = "document.json";

// Thrown for anything the user should be told about; what() is a plain-language
// message that can be shown in a dialog as it is.
class FileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Writes the document. The old file is replaced only once the new one is
// complete, so a failed save never destroys the previous version.
void save(const Document& doc, const std::filesystem::path& path, const std::string& createdBy);

// Reads a document's bodies. Use Document::replaceAll() to show them.
std::vector<Body> load(const std::filesystem::path& path);

// The poses saved in a file (none if it has none); a broken one is left out.
std::vector<SavedPose> loadPoses(const std::filesystem::path& path);
std::vector<SavedPose> posesFromJson(const std::string& json);

// The design variables kept in a file (none if it has none); a broken one is left out.
std::vector<Variable> loadVariables(const std::filesystem::path& path);
std::vector<Variable> variablesFromJson(const std::string& json);

// The figure guide kept in a file, if any (a broken one is left out).
std::optional<Mannequin> loadMannequin(const std::filesystem::path& path);
std::optional<Mannequin> mannequinFromJson(const std::string& json);

// The reference images kept in a file (none if it has none). A picture that can't be
// read is left out, never a reason to refuse the file. Use Document::setReferences().
std::vector<ReferenceImage> loadReferences(const std::filesystem::path& path);
// The same from the JSON part; `readEntry` returns a zip entry's bytes.
std::vector<ReferenceImage> referencesFromJson(const std::string& json,
                                               const std::function<std::string(const std::string&)>& readEntry);

// Saved results. Steps worked out by the mesh engine (Smooth, Extrude Down, Sculpt,
// merges of imported meshes...) can take a long while on big meshes, so save() also
// keeps each shape's finished triangles ("results/<id>.bin"), labelled with a
// fingerprint of everything that went into them. After opening a file, this hands the
// ones that still match (all of them, unless the file was edited by hand or saved by a
// Forge3D that works them out differently) to the document. Returns how many were used;
// a damaged or missing result is simply worked out again.
std::size_t useSavedResults(const Document& doc, const std::filesystem::path& path);
// The fingerprint of a shape and everything it's made from.
std::string resultKey(const Document& doc, ObjectId id);

// The JSON part on its own (for tests and tools).
std::string toJson(const Document& doc, const std::string& createdBy);
// Imported meshes keep their triangles in separate zip entries ("meshes/7.bin");
// `readEntry` returns an entry's bytes. Without it, a file with meshes is refused.
std::vector<Body> fromJson(const std::string& json,
                           const std::function<std::string(const std::string&)>& readEntry = {});

// The binary form of an imported mesh's triangles (the "meshes/<id>.bin" entries).
std::string meshToBytes(const MeshData& mesh);
std::shared_ptr<const MeshData> meshFromBytes(const std::string& bytes);

}  // namespace forge::file
