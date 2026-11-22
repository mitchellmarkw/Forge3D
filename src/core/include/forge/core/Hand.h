// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <optional>
#include <vector>

// The hand generator (slice 71): fists, gripping, open, relaxed and pointing hands at a
// chosen size, left or right, with the socket for the wrist peg cut into the wrist and,
// gripping, a hole through the fist for a handle. The hand stands on its wrist (the wrist
// face flat on the plate, the fingers up), the palm facing the front.
namespace forge {

class CommandStack;
class Document;

enum class HandPose { Fist, Grip, Open, Relaxed, Point };
inline constexpr int kHandPoseCount = 5;

struct HandSettings {
    HandPose pose = HandPose::Fist;
    bool left = false;
    double length = 15.75;    // mm, wrist to the middle fingertip (a person's 189 mm at 1:12)
    double fullness = 1.2;    // palm thickness and finger girth, 1: lifelike, more: chunkier (toy-like)
    double handle = 3.0;      // Grip: the handle's diameter
    double fit = 0.1;         // Grip: room all round the handle
    double gripAngle = 10.0;  // Grip: degrees the handle tips down towards the little finger
};

namespace hand {

// "Fist", "Gripping"...
const char* title(HandPose pose);
// Throws std::invalid_argument (in plain words) if it can't be made.
void check(const HandSettings& s);

// A person's hand, wrist to fingertip (mm), and the hand length at a scale (1:12 → 15.75).
inline constexpr double kLifeSize = 189.0;
double lengthAtScale(double scale);

struct Made {
    Mesh mesh;  // closed; the wrist face's middle at the origin, on Z = 0, fingers up
    // Grip: the handle's line through the fist (its middle and which way it goes).
    bool grip = false;
    Vec3 gripMiddle{0, 0, 0};
    Vec3 gripAlong{1, 0, 0};
    // The wrist's half width (X) and half thickness (Y) and how far up it's straight, so
    // the socket has room.
    double wristHalfWidth = 0, wristHalfThickness = 0;
};
// The hand, made round a wrist socket of radius `socketRadius` and depth `socketDepth`
// (0: none) with `wall` mm round it.
Made make(const HandSettings& s, double socketRadius = 0.0, double socketDepth = 0.0, double wall = 0.8);

// The socket's room: how wide (radius, with its fit) and deep it is.
double socketRadius(const SocketFinish& f);
double socketDepth(const SocketFinish& f);

// Adds the hand (or a pair: both, the right one at x + gap) standing at (x, y) on the
// plate as one undo step: the hand, the wrist socket (if any) as a Socket step, and a
// Grip hole through a gripping hand as a Port step. Returns the top-level hands.
std::vector<ObjectId> addHands(CommandStack& stack, Document& doc, const HandSettings& s,
                               const std::optional<SocketFinish>& wrist, bool pair, double x, double y);
// The name a hand gets: "Right hand (fist)".
std::string nameOf(const HandSettings& s);

}  // namespace hand
}  // namespace forge
