// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Rig.h"

#include <string>
#include <vector>

// Brick figure (slice 107): a figure's own head on a new, simple toy body of blocks: legs,
// hips and torso, arms with round hands (a grip hole through each for the accessories'
// handles), and a post the head sits on. Made in one piece, or as parts that go together
// on pegs (the arms and legs turn on them, the head on its post).
namespace forge {

class CommandStack;
class Document;

namespace brickfig {

enum class Build { Parts, OnePiece };

struct Settings {
    double height = 60.0;    // mm, the whole figure printed, head included
    double headSize = 1.0;   // the head's size against the usual (the top 30% of the height)
    Build build = Build::Parts;
    double fit = 0.15;       // mm: room round each peg in its hole
    double grip = 3.0;       // mm: the hands' grip holes (0: none); Accessories' handles are 3 mm
    bool operator==(const Settings&) const = default;
};
inline constexpr double kLeastHeight = 20.0, kMostHeight = 400.0;
inline constexpr double kLeastHeadSize = 0.6, kMostHeadSize = 1.6;

// Throws std::invalid_argument (in plain words) if it can't be made.
void check(const Settings& s);

// The body's parts, standing put together, the figure's feet on Z = 0 and centred on
// X = Y = 0, facing -Y (closed surfaces). One piece: just "Body". Parts: "Torso" (hips,
// neck post and pegs), "Left arm", "Right arm", "Left leg", "Right leg".
struct Part {
    std::string name;
    Mesh mesh;
};
std::vector<Part> body(const Settings& s);
// Where the head sits on the body: the middle of the torso's top, and its post.
struct Seat {
    double z = 0.0;           // the torso's top
    double postRadius = 0.0;
    double postHeight = 0.0;
};
Seat seatFor(const Settings& s);

struct Made {
    std::vector<ObjectId> parts;  // the head first
};
// Takes the head off the figure (a shape on the plate; `points` its skeleton in plate
// coordinates) where its neck is narrowest, sizes it for the body, a socket in it for the
// post, and adds it with the body beside the figure (which stays as it is), as one undo
// step. Throws (changing nothing) if it can't be made.
Made make(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points, const Settings& s);

}  // namespace brickfig
}  // namespace forge
