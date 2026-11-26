// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Rig.h"

#include <string>
#include <vector>

// Brick figure (slice 107, its body reworked in 108): a figure's own head on a new toy body
// in the build of the generic brick-compatible figures: a torso narrower at the shoulders,
// hips with an axle the square legs hang from, arms angled out and bent forward with
// C-shaped hands (their hole takes the accessories' handles), and a post the head sits on.
// Made in one piece, or as parts that go together (the arms and legs turn on pegs, the
// head on its post).
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
    // Where the head comes off (figurestyle::neckCut): moved up the neck, tipped forward.
    double raise = 0.0;
    double tilt = 0.0;
    double headForward = 0.0;  // mm the head sits forward of the post's middle (less: back)
    // Or the head is what's masked (slice 109; plate coordinates): cut flat through the
    // mask's edge. Empty: the neck's cut above.
    std::vector<MaskDab> headMask;
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
// `coarseness`: how much bigger than usual the surfaces' steps are (a quick preview: 3).
std::vector<Part> body(const Settings& s, double coarseness = 1.0);
// Where the head sits on the body: the middle of the torso's top, and its post.
struct Seat {
    double z = 0.0;           // the torso's top
    double postRadius = 0.0;
    double postHeight = 0.0;
};
Seat seatFor(const Settings& s);

// Every part as it will be made (the head first; one piece: one part), where it will be on
// the plate, beside the figure. `preview`: quickly, coarser, the head without its socket.
// Throws std::invalid_argument (in plain words) if it can't be made.
std::vector<Part> build(const Document& doc, ObjectId figure, const rig::Points& points, const Settings& s,
                        bool preview = false);
// The preview's parts as one surface (for showing).
Mesh preview(const Document& doc, ObjectId figure, const rig::Points& points, const Settings& s);

struct Made {
    std::vector<ObjectId> parts;  // the head first
};
// Takes the head off the figure (a shape on the plate; `points` its skeleton in plate
// coordinates) at the neck's cut, stands it up, sizes it for the body and sets the middle
// of its neck on the post (moved forward by headForward), a socket in it for the post,
// and adds it with the body beside the figure (which stays as it is), as one undo step.
// Throws (changing nothing) if it can't be made.
Made make(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points, const Settings& s);

}  // namespace brickfig
}  // namespace forge
