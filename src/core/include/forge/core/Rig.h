// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mannequin.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

// Pose Figure: simple bones put inside one figure (a sculpt, or an imported mesh) so it
// can be bent at its joints, the surface following smoothly. The skeleton is the figure
// guide's (neck, waist, shoulders, elbows, hips, knees) with wrists and ankles added.
//
// The skeleton's points are fitted to the figure as it was made (any pose: arms down,
// A or T pose), in the step's coordinates, where the figure stands up +Z and faces -Y.
// The angles are the figure guide's: 0 is standing straight, arms and legs down, so the
// figure guide's ready poses work on any figure. Each joint's angles from how the figure
// was made are worked out from the points (restAngles); a joint left at those doesn't
// move.
//
// Each point of the surface follows the bones nearest it: fully the nearest one, and
// a share of any other within `blend` of that (the bend spreads that far either side of
// a joint). Blended as dual quaternions, so bent elbows and knees keep their thickness.
namespace forge::rig {

enum class Point {
    Waist, Neck, Head, LeftShoulder, RightShoulder, LeftElbow, RightElbow, LeftWrist, RightWrist, LeftHand,
    RightHand, LeftHip, RightHip, LeftKnee, RightKnee, LeftAnkle, RightAnkle, LeftToe, RightToe
};
static_assert(kRigPointCount == 19);

// The figure guide's joints first, in its order (so its poses copy straight across).
enum class Joint {
    Neck, Waist, LeftShoulder, RightShoulder, LeftElbow, RightElbow, LeftHip, RightHip, LeftKnee, RightKnee,
    LeftWrist, RightWrist, LeftAnkle, RightAnkle
};
static_assert(kRigJointCount == 14);

// "waist", "leftShoulder"... (the file's names) and "Waist", "Left shoulder"... to show.
const char* key(Point p);
const char* title(Point p);
// The same point on the other side (itself for the middle ones).
Point mirrored(Point p);

const char* key(Joint j);
const char* title(Joint j);
// How many angles a joint has, what each is called, and the range it may take (degrees).
int angleCount(Joint j);
const char* angleTitle(Joint j, int which);
std::array<double, 2> angleRange(Joint j, int which);
// The point a joint turns about.
Point pivot(Joint j);

using Points = std::array<Vec3, kRigPointCount>;
using Angles = std::array<std::array<double, 2>, kRigJointCount>;

// Throws std::invalid_argument (in plain words) if it can't be posed: points missing,
// on top of each other or not numbers, an angle not a number, blend out of range.
void check(const PosedFinish& f);
// "posed: 6 joints bent, feet flat"
std::string describe(const PosedFinish& f);

// The skeleton fitted to a figure standing up +Z, facing -Y (`mesh` in the step's
// coordinates): torso from the usual proportions for its height, arms and legs found
// from the surface (out to the sides or down). The angles are set so it doesn't move
// (restAngles), blend from its size.
PosedFinish fit(const Mesh& mesh);
// The angles the figure was made in, from its points.
Angles restAngles(const Points& points);
// A figure guide pose (its 10 joints; wrists and ankles straight).
Angles fromMannequin(const std::array<std::array<double, 2>, kMJointCount>& pose);
// How much the figure's height is.
double height(const Points& points);

// Each bone's move from the figure as made to the pose (indexed by bone; the first is
// the hips, which stay put), and where the skeleton's points go.
struct Moves {
    std::vector<Transform> bones;
    Points points{};
};
Moves moves(const PosedFinish& f);

// The figure posed. Surface weights are worked out once for a figure and its points, so
// trying angle after angle is quick.
class Skin {
public:
    // `rest`: the figure as made, step coordinates. Weights from f.points and f.blend.
    Skin(const Mesh& rest, const PosedFinish& f);
    ~Skin();
    Skin(Skin&&) noexcept;
    Skin& operator=(Skin&&) noexcept;
    // Whether these settings use the same weights (same points and blend).
    bool sameWeights(const PosedFinish& f) const;
    // The posed surface (positions and normals; the same triangles), and where the
    // skeleton's points went. Kept on the ground (f.onGround): moved up or down so its
    // lowest point is as low as the figure's was.
    struct Posed {
        Mesh mesh;
        Points points{};
    };
    Posed posed(const PosedFinish& f) const;
    // The rest surface's lowest point, up the step's Z.
    double lowest() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};

// The figure posed, as a solid (the Posed step).
MeshSolid apply(const MeshSolid& solid, const PosedFinish& f);

// The skeleton to draw: a ball on each point (`highlight` bigger), sticks between them.
Mesh skeletonMesh(const Points& points, double ballRadius, int highlight = -1);

}  // namespace forge::rig

namespace forge {
class Document;
// A Posed step on `target` (settings in the step's coordinates: those of whatever holds
// the target, see rigFrame).
BooleanSpec posedSpec(const Document& doc, ObjectId target, const PosedFinish& f);
// The step's coordinates -> plate, for a new step on `target`.
Transform rigFrame(const Document& doc, ObjectId target);
// The step with changed settings.
Body withPosed(const Document& doc, ObjectId result, const PosedFinish& f);
}  // namespace forge
