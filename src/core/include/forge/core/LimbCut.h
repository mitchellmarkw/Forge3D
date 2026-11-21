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
    double limbRadius = 0.0;  // how thick the limb is at the cut (from the ball to its surface, typically)
};
Cut plan(const Mesh& figure, const rig::Points& points, rig::Joint joint);
// Every cut, sharing the work of finding which bone each part of the surface is nearest.
std::vector<Cut> plan(const Mesh& figure, const rig::Points& points, const std::vector<rig::Joint>& joints);

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
struct Choice {
    rig::Joint joint = rig::Joint::LeftShoulder;
    JointKind kind = JointKind::Cut;
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
// Cuts the figure (a shape on the plate) at each chosen joint and puts the joints in, as
// one undo step. `points`: the skeleton, plate coordinates. Throws (changing nothing) if a
// cut leaves nothing.
Made cutIntoParts(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points,
                  std::vector<Choice> choices, std::uint32_t firstLink);

}  // namespace limbcut

// A Limb Cut step on `target` (the cut in plate coordinates), keeping the limb or the rest.
BooleanSpec limbCutSpec(const Document& doc, ObjectId target, const LimbCutFinish& inPlate);

}  // namespace forge
