// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Rig.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <string>
#include <vector>

// Cut into Parts: a figure sculpted in one piece cut at its joints (the Pose Figure
// skeleton's), each cut square to the limb, so the pieces can be jointed (a ball and socket,
// a hinge) and printed apart.
namespace forge {

class CommandStack;
class Document;

namespace limbcut {

// The joints a figure can be cut at, from the middle out: neck, waist, shoulders, hips,
// elbows, knees, wrists, ankles.
std::vector<rig::Joint> cuttable();

// A cut worked out on a figure (the points and mesh in the same coordinates).
struct Cut {
    LimbCutFinish finish;  // the plane, and the limb's reach (finish.limb, finish.gap unset)
    double limbRadius = 0.0;  // how thick the limb is at the cut (from its bone to its surface, typically)
};
// A cut turned by hand from the way it would go (degrees): `forward` tips it towards the
// front (round the line square to the cut and the front), `side` turns it round the line
// from front to back (at the left shoulder, more tips it down, square to an arm hanging
// down). The right side's joints turn the other way, so the same numbers make a mirrored
// pair.
struct Tilt {
    double forward = 0.0;
    double side = 0.0;
    bool operator==(const Tilt&) const = default;
};
// The cut's direction turned by a tilt (the figure faces -Y).
Vec3 tilted(const Vec3& normal, const Tilt& tilt);

Cut plan(const Mesh& figure, const rig::Points& points, rig::Joint joint, const Tilt& tilt = {});
// Every cut, sharing the work of finding which bone each part of the surface is nearest.
// `tilts`: one per joint, or none.
std::vector<Cut> plan(const Mesh& figure, const rig::Points& points, const std::vector<rig::Joint>& joints,
                      const std::vector<Tilt>& tilts = {});

// Throws std::invalid_argument (in plain words) if it can't be used.
void check(const LimbCutFinish& f);
// "the left arm, cut at the left shoulder" / "the rest, cut at the left shoulder"
std::string describe(const LimbCutFinish& f);
// The cut moved by a rigid transform.
LimbCutFinish moved(const LimbCutFinish& f, const Transform& t);
// What's kept: the limb, or the rest.
MeshSolid apply(const MeshSolid& part, const LimbCutFinish& f);
// Where the limb's and the rest's cut faces are (the plane moved back half the gap each way).
Vec3 limbFace(const LimbCutFinish& f);
Vec3 restFace(const LimbCutFinish& f);

// A rounded rod (closed surface), for the limb's reach.
Mesh rod(const Vec3& a, const Vec3& b, double radius, int around = 20);

// What goes in a cut: nothing (just cut), a ball joint (the ball on the piece nearer the
// middle, the socket in the limb), or a hinge (single, double or swivel: the fork in the
// piece nearer the middle, the tab made from the limb's end).
enum class JointKind { Cut, Ball, Hinge };
// Toy joint recipes (slice 96): two joints in a row with a small block between them, cut
// from the piece nearer the middle just inside the cut. `kind` (and its ball or hinge)
// is the joint between the block and the limb; the block's own joint is set by the recipe:
//   Butterfly (shoulders): a hinge with an upright pin, so the arm can swing forward
//     (crossed arms), then the ball.
//   DropHip (hips): a hinge with its pin left to right, so the leg's ball can drop for
//     high kicks, then the ball.
//   AnkleRocker (ankles): a hinge left to right (the foot up and down), then a hinge
//     front to back (it rocks side to side).
//   AbCrunch (waist): a swivel (turning at the waist), then a hinge left to right (it
//     bends forward).
enum class Recipe { None, Butterfly, DropHip, AnkleRocker, AbCrunch };
// Whether a recipe is for this joint.
bool recipeFits(Recipe recipe, rig::Joint joint);
const char* title(Recipe recipe);
struct Choice {
    rig::Joint joint = rig::Joint::LeftShoulder;
    JointKind kind = JointKind::Cut;
    Recipe recipe = Recipe::None;
    Tilt tilt;           // the cut turned by hand
    SocketFinish ball;   // JointKind::Ball (the fit and printer included)
    HingeFinish hinge;   // JointKind::Hinge
};
// Sizes for a joint in a limb this thick (to its nearest surface at the cut).
SocketFinish ballFor(double limbRadius);
HingeFinish hingeFor(double limbRadius, HingeKind kind);
// How far apart the pieces are cut: room for a ball's neck to tilt; none for a single
// hinge or swivel (the tab is made from the limb's end); a double hinge's gap.
double gapFor(const Choice& c);
// The ball on its neck, for a socket opening at `spot` (facing out of the limb), its neck
// reaching `into` the other piece at `restFace`: a closed surface.
Mesh ballOnNeck(const SocketFinish& f, const Vec3& spot, const Vec3& normalOutOfLimb, const Vec3& restFace);

struct Made {
    std::vector<ObjectId> pieces;  // one per piece, named for its bones
    std::vector<ObjectId> extras;  // hinge pins and links, to print with them
};
// Print in place (slice 103): every joint built captive with `clearance` all round, so the
// figure prints already put together on a filament printer and moves once it's off the
// plate. Balls are held in sockets narrower than them; hinge pins are part of the fork (a
// double hinge's link stays a piece of its own, held on both pins); swivels keep their lip
// in its groove. No clicks, ridges or relief cuts (they'd print joined).
struct InPlace {
    bool on = false;
    double clearance = 0.35;  // mm, all round
};
inline constexpr double kLeastClearance = 0.2, kMostClearance = 1.0;
// A joint's settings as they're built in place (what join() uses).
SocketFinish inPlaceBall(SocketFinish ball, double clearance);
HingeFinish inPlaceHinge(HingeFinish hinge, double clearance);
// Cuts the figure (a shape on the plate) at each chosen joint and puts the joints in, as
// one undo step. `points`: the skeleton, plate coordinates. Throws (changing nothing) if a
// cut leaves nothing.
Made cutIntoParts(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points,
                  std::vector<Choice> choices, std::uint32_t firstLink, const InPlace& inPlace = {});

}  // namespace limbcut

// A Limb Cut step on `target` (the cut in plate coordinates), keeping the limb or the rest.
BooleanSpec limbCutSpec(const Document& doc, ObjectId target, const LimbCutFinish& inPlate);

}  // namespace forge
