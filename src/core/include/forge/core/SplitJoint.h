// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Split.h"
#include "forge/core/Transform.h"

#include <cstdint>
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
