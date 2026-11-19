// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <optional>
#include <string>

namespace forge {

class CommandStack;
class Document;

// Join Parts: one part moved so a feature on it meets a feature on another (a peg into
// its socket, a ball into its cup, a face flat against a face). Plate coordinates.
namespace join {

struct Feature {
    enum class Kind {
        Flat,        // a flat face: its middle, facing out
        Peg,         // a round peg: its tip, pointing out along it
        Hole,        // a round hole: its bottom, pointing out of the opening
        Ball,        // a ball: its middle, pointing out of the part it's on
        BallSocket,  // a round cup a ball sits in: where the ball's middle goes, pointing out of it
        Socket,      // a Quick Socket: where a ball's middle or a peg's tip goes, pointing out of it
        Spot,        // a spot on a surface: where it was clicked, facing out
    };
    Kind kind = Kind::Spot;
    Vec3 point{0, 0, 0};
    Vec3 direction{0, 0, 1};  // unit, out of the part
    double size = 0.0;        // a peg's, hole's or ball's diameter (0: none)
    double gap = 0.0;         // the gap it suggests when joined (a peg socket: its fit)
    std::string text;         // "Peg Ø3", "Flat face", "Ball socket Ø3 (snap-in)"...
};

// What's under a click on the shape `id` (on the plate). Quick Sockets in its history
// are found first; then, on shapes made in Forge3D, the face's kind; on meshes, a ball
// if the surface round the spot is one, else the spot. Nothing if the ray misses.
std::optional<Feature> featureAt(const Document& doc, ObjectId id, const Vec3& origin, const Vec3& direction);

// How the moving feature sits against the fixed one once joined: `along` moves it back
// out along the fixed feature's direction (a gap), `acrossX` / `acrossY` slide it across
// (in a frame laid as Emboss lays words), `turn` spins it round the shared direction
// (degrees), and `flip` turns the moving feature's direction round first.
struct Offsets {
    double along = 0.0;
    double acrossX = 0.0;
    double acrossY = 0.0;
    double turn = 0.0;
    bool flip = false;
};

// The move (a turn, then a shift) that takes the moving feature onto the fixed one:
// points together, directions opposite (facing each other).
Transform mating(const Feature& moving, const Feature& fixed, const Offsets& offsets);

// Where the shape `moving` (on the plate) goes for that.
Placement joinedPlacement(const Document& doc, ObjectId moving, const Feature& movingFeature,
                          const Feature& fixed, const Offsets& offsets);

// Moves `moving` to `placed` as one undo step ("Join A to B"). With `together`, the two
// are then grouped so they move as one; joining onto a group adds to it (the group is
// made again with the part in it, keeping its name). Returns the group, or `moving`.
ObjectId apply(CommandStack& stack, Document& doc, ObjectId moving, ObjectId fixed, const Placement& placed,
               bool together);

}  // namespace join
}  // namespace forge
