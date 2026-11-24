// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Split.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <string>
#include <vector>

// A joint at a split (slice 88): when a shape is split (by a plane or by a mask) into two
// pieces, a joint put in where they meet, the way Cut into Parts does at a figure's joints.
namespace forge {

class CommandStack;
class Document;

namespace splitjoint {

enum class Kind {
    None,
    Ball,         // a snap-in ball socket in one piece, the ball on a neck on the other
    Hinge,        // a hinge: the fork in one piece, the other's end made into the tab, a pin
    DoubleHinge,  // two forks and a link between them
    Swivel,       // a peg turning in a bore
    Magnets,      // a magnet pocket in each face
};
const char* title(Kind kind);

struct Options {
    Kind kind = Kind::None;
    double size = 0.0;  // the ball's, peg's or magnet's diameter (a hinge: the tab's thickness); 0: to fit the cut
    double fit = 0.1;   // room all round for printing (resin 0.1, filament 0.2)
    bool operator==(const Options&) const = default;
};

// Where the joint goes: on the cut, `normal` facing out of the first piece (the one with
// the socket, bore or tab) into the second; `room` to the cut's nearest edge from there,
// `reach` to its farthest.
struct Spot {
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 0, 1};
    double room = 0.0;
    double reach = 0.0;
};

// The spot on a plane's cut through `id` (plate coordinates) farthest in from its edges,
// the joint going from the side the plane faces into the other side. Throws if the plane
// misses the shape.
Spot planeSpot(const Document& doc, ObjectId id, const SplitPlane& plane);
// The spot on a mask's cut (one edge round the shape), the joint going out of the masked
// part into the rest. Throws (in plain words) if the mask's edge goes round more than once.
Spot maskSpot(const Document& doc, ObjectId id, const std::vector<MaskDab>& dabs, MaskCut cut);

// How far apart the pieces' faces are left: room for a ball's neck to tilt, a double
// hinge's gap; nothing for the others.
double gapFor(const Options& o, double room);
// The ball joint's or hinge's sizes for a cut this far in from its edge (o.size used if set).
SocketFinish ballFor(const Options& o, double room);
HingeFinish hingeFor(const Options& o, double room, HingeKind kind);
SocketFinish magnetFor(const Options& o, double room);

struct Made {
    std::vector<ObjectId> pieces;  // the first piece and the second, as they are now
    std::vector<ObjectId> extras;  // hinge pins, links and pegs to print with them
};

// Puts the joint between `first` and `second` (pieces on the plate) at `spot`, the second
// piece's face `gap` along the normal from the first's. Throws (in plain words) if there
// isn't room. Run inside a macro.
Made addJoint(CommandStack& stack, Document& doc, ObjectId first, ObjectId second, const Spot& spot, double gap,
              const Options& o, std::uint32_t link);

// The joint's settings kept on a cut.
Options optionsOf(const CutJoint& c);

// Puts the joint in between a cut's two pieces (on the plate), from the cut as it is now:
// for a plane, the second piece's cut is brought to meet the first's (back by the gap).
// Run inside a macro.
Made joinCut(CommandStack& stack, Document& doc, ObjectId first, ObjectId second, const Options& o,
             std::uint32_t link);
// The two pieces of the cut with this joint (first, then second), or none.
std::vector<ObjectId> cutPieces(const Document& doc, std::uint32_t link);
// Takes out what the joint made (its steps, the ball, the gap, pins and links that are
// still loose). False if there was nothing to take out. Run inside a macro.
bool removeJoint(CommandStack& stack, Document& doc, std::uint32_t link);
// Moves a split's cut (plate coordinates, as withSplitPlane); with a joint, the other
// piece's cut goes with it and the joint is put in again at the new cut. One undo step.
void moveCut(CommandStack& stack, Document& doc, ObjectId splitStep, const SplitPlane& plane);
// Changes a mask split's mask (as withMask); with a joint, it's put in again at the new
// cut. One undo step. Throws if the mask doesn't cut the shape in two.
void changeMask(CommandStack& stack, Document& doc, ObjectId maskStep, const std::vector<MaskDab>& dabs,
                std::optional<double> keyFit, std::optional<std::pair<MaskCut, double>> cut);

// Split by a plane with a joint, as one undo step: the side the plane faces gets the
// socket (or fork, bore), the other side the ball (or tab, peg), cut back by the gap.
Made splitWithJoint(CommandStack& stack, Document& doc, ObjectId id, const SplitPlane& plane, const Options& o,
                    std::uint32_t link);
// Split by a mask with a joint, as one undo step: the masked part gets the socket, the rest
// the ball. The rest's face is cut back by the gap round the joint.
Made splitByMaskWithJoint(CommandStack& stack, Document& doc, ObjectId id, const std::vector<MaskDab>& dabs,
                          MaskCut cut, const Options& o, std::uint32_t link);

}  // namespace splitjoint

}  // namespace forge
