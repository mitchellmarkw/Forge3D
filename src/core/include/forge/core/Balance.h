// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Pose.h"
#include "forge/core/Transform.h"

#include <array>
#include <vector>

// "Will it stand?" (slice 75): a figure's centre of weight against where its feet touch
// the ground, in the pose it's in, and peg holes for a stand in its soles.
namespace forge::balance {

// Where one part touches the ground.
struct Foot {
    ObjectId part = kInvalidId;
    Vec3 centre{0, 0, 0};                         // the middle of where it touches (posed)
    std::vector<std::array<double, 2>> outline;   // round where it touches (posed, seen from above)
    double area = 0.0;                            // of that outline, mm²
};

struct Balance {
    Vec3 centre{0, 0, 0};  // the centre of weight (posed), every part solid and as heavy
    double ground = 0.0;   // the height of the lowest point: the ground it stands on
    double weight = 0.0;   // the parts' volume, mm³
    std::vector<std::array<double, 2>> support;  // round everything touching the ground (seen from above)
    // How far the centre's foot (straight down) is inside the support's edge (mm;
    // negative: outside, it tips over), the way to the nearest edge, and how far it can
    // lean that way before tipping (degrees; 0 if it tips).
    double margin = 0.0;
    std::array<double, 2> toward{0.0, 0.0};
    double lean = 0.0;
    std::vector<Foot> feet;
    bool stands() const { return margin > 0.0; }
};

// The balance of these parts as posed (each part's solid as built in plate coordinates,
// `moved` on top). Anything within `touching` mm of the lowest point counts as on the
// ground. Throws std::invalid_argument if there's nothing solid.
Balance of(const std::vector<pose::PosedPart>& parts, double touching = 0.4);

// The volume and centre of a closed triangle surface.
std::pair<double, Vec3> volumeAndCentre(const Mesh& mesh);

// Where a stand peg hole goes in each foot: the middle of where it touches, back in the
// part's own (as built) coordinates, facing down out of the sole there.
struct PegSpot {
    ObjectId part = kInvalidId;
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 0, -1};
};
std::vector<PegSpot> pegSpots(const std::vector<pose::PosedPart>& parts, const Balance& b);

}  // namespace forge::balance
