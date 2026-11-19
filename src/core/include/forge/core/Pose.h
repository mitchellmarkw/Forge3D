// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Pose test: turn a figure's joints to see how far they go before parts hit, without
// changing the parts as printed. Joints come from the Quick Hinge and Quick Socket steps
// in the document; the poses are only for looking (and can be saved by name).
namespace forge {

class Document;

// A saved pose: the part held still and each joint's angles (degrees), by the step that
// makes the joint. Joints not listed are straight.
struct SavedPose {
    std::string name;
    ObjectId hold = kInvalidId;
    std::map<ObjectId, std::array<double, 3>> angles;
    bool operator==(const SavedPose&) const = default;
};

namespace pose {

enum class JointKind {
    Hinge,   // turns round its pin (Quick Hinge, single or double)
    Swivel,  // turns round the bore's axis (a swivel, a round peg)
    Ball,    // turns any way round the ball's middle: bend, bend sideways, twist
    Fixed,   // holds the parts together, doesn't turn (a peg with flats)
};

struct Joint {
    ObjectId step = kInvalidId;  // the Hinge (fork) or Socket step that makes it
    JointKind kind = JointKind::Hinge;
    Vec3 pivot{0, 0, 0};   // plate coordinates, as built
    Vec3 axis{0, 0, 1};    // the pin; a swivel's or ball's direction (out of the holder)
    Vec3 across{1, 0, 0};  // square to the axis: a ball's "bend" turns round this
    ObjectId holder = kInvalidId;  // the part with the fork or socket (on the plate)
    ObjectId other = kInvalidId;   // the part that turns in it, if one was found
    std::string name;              // "Hinge in Thigh", "Ball socket in Torso"
    int clicks = 0;                // clicks a turn (a ratchet): it only rests on a click
};
// Where a joint with clicks rests nearest `degrees` (unchanged without clicks).
double nearestClick(const Joint& joint, double degrees);

// Every joint in the document: the fork halves of Quick Hinges (each fork of a double
// hinge is a joint of its own, the link between them a part) and Quick Sockets. The
// other part is the tab's (by the hinge's link number) or else the biggest part, other
// than the holder, round the pivot (so not a pin through it).
std::vector<Joint> findJoints(const Document& doc);

// How the parts hang together: the part held still, and for every other part reached
// through joints, its parent and the joint to it. Parts that aren't joined but have a
// joint's pivot inside them (the pins) ride with that joint's parent part. Groups of
// parts not joined to the held one are held by their biggest part.
struct Rig {
    ObjectId hold = kInvalidId;
    std::map<ObjectId, ObjectId> parent;       // part -> the part it turns from
    std::map<ObjectId, std::size_t> joint;     // part -> its joint (index into the joints)
    std::map<ObjectId, ObjectId> rides;        // part -> the part it moves with (no joint)
    std::vector<ObjectId> order;               // parents before their children
};
// `sizes`: each part on the plate and how big it is (any measure: for choosing). The
// held part is the biggest jointed one unless `hold` is given.
Rig build(const std::vector<Joint>& joints, const std::map<ObjectId, double>& sizes,
          const std::map<ObjectId, Bounds>& bounds, ObjectId hold = kInvalidId);

// One joint's turn (about its pivot, in plate coordinates as built).
Transform turn(const Joint& joint, const std::array<double, 3>& angles);
// Where each moved part goes (applied on top of where it is on the plate); parts that
// don't move aren't listed.
std::map<ObjectId, Transform> transforms(const std::vector<Joint>& joints, const Rig& rig,
                                         const std::map<ObjectId, std::array<double, 3>>& angles);

// Rotation about an axis through a point (degrees, right-handed).
Transform rotationAbout(const Vec3& point, const Vec3& axis, double degrees);
// A turn as ball angles: bend (round `across`), bend sideways, twist (round `axis`).
std::array<double, 3> ballAngles(const Joint& joint, const Transform& rotation);

// Parts that hit each other. Each part as built (plate coordinates) and how it's moved;
// pairs that move together aren't checked.
struct PosedPart {
    ObjectId id = kInvalidId;
    MeshSolid solid;  // as built, plate coordinates
    Transform moved;  // the pose on top
};
struct Hit {
    ObjectId a = kInvalidId, b = kInvalidId;
    double volume = 0.0;  // mm^3 overlapping
    Mesh overlap;         // where (plate coordinates, posed)
};
std::vector<Hit> collisions(const std::vector<PosedPart>& parts, double smallest = 0.005);

// How far a hinge or swivel turns each way from straight before parts hit (degrees,
// to 1 degree, or to the last click it rests on; up to 180). Other joints keep their angles. Parts not moved by the joint
// aren't moved. `parts` as built (their `moved` is ignored).
std::array<double, 2> range(const std::vector<Joint>& joints, const Rig& rig,
                            std::map<ObjectId, std::array<double, 3>> angles, std::size_t joint,
                            const std::vector<PosedPart>& parts, double smallest = 0.005);

}  // namespace pose
}  // namespace forge
