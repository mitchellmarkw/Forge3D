// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <string>
#include <vector>

// Mould Maker (slices 69, 69.1): a box round a part, for casting resin copies of a
// printed master. The box is the part's box with a wall all round, the part taken out,
// cut by a flat face (level, or upright either way), by default where the part is widest,
// and by a second face through one or both halves for a three- or four-part mould.
// Registration keys line the pieces up; a pour spout and air vents run straight up from
// the part to the top of the box. Undercuts (where a piece would lock on the part as it's
// pulled off) are found, and the split with the fewest.
namespace forge {

class CommandStack;
class Document;

namespace mould {

// Throws std::invalid_argument (in plain words) if the settings can't be made.
void check(const MouldFinish& f);
// "top piece, 8 mm walls, spout Ø6, 2 air vents, keyed"
std::string describe(const MouldFinish& f);

// "top" / "bottom", "right" / "left", "back" / "front": a side of a cut square to `axis`.
std::string sideName(int axis, bool positive);
// The piece's name: "top", or "top, left" for a piece of a second cut.
std::string pieceName(const MouldFinish& f);
// Every piece of the mould these settings make (each a copy with `top` and `side2` set):
// two, three or four.
std::vector<MouldFinish> pieces(const MouldFinish& f);
// Whether two pieces belong to the same mould (the same settings but which piece).
bool sameMould(const MouldFinish& a, const MouldFinish& b);

// The area of the part's outline cut square to `axis` at `level` along it (mm², holes
// taken away).
double sectionArea(const Mesh& part, double level, int axis = 2);
// Where along `axis` the part is widest (the biggest outline), to part the mould at.
double widest(const Mesh& part, int axis = 2);
// Where the spout meets the part: the middle of its highest spot.
Vec3 highest(const Mesh& part);
// Where air would be trapped when the mould is filled from the spout: the part's other
// high spots higher than `above` (each the highest point round it), highest first,
// well apart from the spout and each other. At most `most`.
std::vector<Vec3> highSpots(const Mesh& part, double above, const Vec3& spout, double ventSize, int most = 6);
// Settings for a level two-part mould round this part: parted where it's widest, the
// spout at its top, vents at its other high spots.
MouldFinish suggested(const Mesh& part);

// Undercuts: the part's triangles (by number) that a piece would catch on as it's pulled
// straight off (away from the cut it's on, or either of its two cuts), and their area.
struct Undercuts {
    std::vector<std::uint32_t> triangles;
    double area = 0.0;
    double total = 0.0;  // the part's whole surface, mm²
};
Undercuts undercuts(const Mesh& part, const MouldFinish& f);
// Just those triangles, to draw.
Mesh undercutMesh(const Mesh& part, const Undercuts& u);
// The single cut (direction and place) with the least undercut, trying each direction at
// where the part is widest and at levels across it; the widest of those about as good.
// Returns `f` with `axis` and `parting` set (and the second cut turned another way if it
// was the same way).
MouldFinish bestSplit(const Mesh& part, const MouldFinish& f);

// The box round the part.
Bounds box(const Bounds& part, double wall);
// A key's radius (0: the wall is too thin for keys).
double keyRadius(const MouldFinish& f);
// The keys: where each one's middle is on its cut, which way it points (the cut's axis),
// which cut (1 or 2), and which side of the other cut it's on (0: none).
struct Key {
    Vec3 point{0, 0, 0};
    int axis = 2;
    int cut = 1;
    int side = 0;  // cut 1's keys: side of cut 2 (if cut there); cut 2's keys: side of cut 1
};
std::vector<Key> keyList(const Bounds& part, const MouldFinish& f);

// For drawing: the box, the spout and vents, the keys (pegs), the cuts as thin sheets.
Mesh boxMesh(const Bounds& b);
Mesh channels(const Bounds& part, const MouldFinish& f);
Mesh keys(const Bounds& part, const MouldFinish& f);
Mesh partingSheets(const Bounds& part, const MouldFinish& f);

// The piece (f.top, f.side2) of the mould round `part`.
MeshSolid apply(const MeshSolid& part, const MouldFinish& f);

}  // namespace mould

// The part's triangles: in a Mould step's own coordinates (for a Mould step), or on the
// plate (for a shape).
Mesh mouldPart(const Document& doc, ObjectId id);
// A Mould step's settings (its own coordinates; top / side2 say which piece it is).
const MouldFinish& mouldOf(const Document& doc, ObjectId result);
// Makes a mould round top-level shape `id` as one undo step: two to four shapes,
// "<name> mould (top)" and so on, the last made from the shape and the others from
// copies of it, in place, closed (lifted together if need be, so the lowest stands on the
// plate). `f` is in plate coordinates (top / side2 are ignored). Returns the pieces, in
// the order mould::pieces gives. Throws, changing nothing, if it can't be made.
std::vector<ObjectId> makeMould(CommandStack& stack, Document& doc, ObjectId id, const MouldFinish& f);
// The step with new settings (its own coordinates; top / side2 are kept), and the other
// pieces of its mould likewise if they're still on the plate with the same settings (so
// they keep fitting). The caller records them (EditBodiesCommand). The number of pieces
// and the cuts' directions can't be changed: throws if they would be.
std::vector<Body> withMould(const Document& doc, ObjectId result, const MouldFinish& f);

}  // namespace forge
