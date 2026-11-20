// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <array>
#include <optional>
#include <string>

// The figure guide: a simple posable mannequin standing on the plate, built to a height
// in heads, to model a figure to (a 3D reference image). Kept in the .f3d file beside
// the reference images; never part of the model, so never printed or exported.
namespace forge {

enum class MJoint { Neck, Waist, LeftShoulder, RightShoulder, LeftElbow, RightElbow, LeftHip, RightHip, LeftKnee,
                    RightKnee };
inline constexpr int kMJointCount = 10;

struct Mannequin {
    double height = 152.4;  // mm, feet to the top of the head (a 6-inch figure)
    double heads = 7.5;     // how many head-heights tall
    double x = 0.0, y = 0.0;  // where it stands on the plate (between its feet)
    double turn = 0.0;        // degrees about Z; 0 faces the front (towards -Y)
    bool shown = true;
    // Degrees, per joint (MJoint order). Shoulders and hips: forward, out to the side.
    // Elbows and knees: bend ([1] unused). Neck: nod forward, turn. Waist: bend
    // forward, twist.
    std::array<std::array<double, 2>, kMJointCount> pose{};
    bool operator==(const Mannequin&) const = default;
};

namespace mannequin {

// "neck", "leftShoulder"... (the file's names) and back again.
const char* key(MJoint joint);
std::optional<MJoint> fromKey(const std::string& key);
// "Neck", "Left shoulder"... for showing.
const char* title(MJoint joint);
// How many angles a joint has (1: elbows and knees) and what each is called.
int angleCount(MJoint joint);
const char* angleTitle(MJoint joint, int which);
// The range each angle may take, degrees.
std::array<double, 2> angleRange(MJoint joint, int which);

// Throws std::invalid_argument (in plain words) if it can't be drawn: a height or heads
// out of range, an angle out of its range or not a number.
void validate(const Mannequin& m);

enum class PosePreset { Standing, APose, TPose, Walking, Action };
inline constexpr int kPosePresetCount = 5;
const char* title(PosePreset preset);
std::array<std::array<double, 2>, kMJointCount> preset(PosePreset preset);

// Where things are, plate mm, as posed (for checking, and for the labels).
struct Landmarks {
    Vec3 top, chin, leftShoulder, rightShoulder, leftElbow, rightElbow, leftWrist, rightWrist, leftHip, rightHip,
        leftKnee, rightKnee, leftAnkle, rightAnkle;
};
Landmarks landmarks(const Mannequin& m);

// The mannequin as simple rounded shapes, plate coordinates.
Mesh mesh(const Mannequin& m);

}  // namespace mannequin
}  // namespace forge
