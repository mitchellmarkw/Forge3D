// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Rig.h"

#include <vector>

// Figure styles (slice 105): a figure turned into a familiar toy look in one go, as a
// Pose Figure step's proportions and blockiness (so it can still be posed and changed
// after), and for a bobblehead, its head cut off to sit on a spring.
namespace forge {

class CommandStack;
class Document;

namespace figurestyle {

// Bobblehead: a big head on a smaller body. Vinyl collectible: a big head on a short,
// chunky body with stubby limbs. Caricature: what's already big made bigger, what's
// small smaller, and a bigger head. (Slice 105's Blocky was dropped in 106.)
enum class Style { Bobblehead, Vinyl, Caricature };
inline constexpr int kStyleCount = 3;
const char* title(Style s);  // "Bobblehead", "Vinyl collectible"...
const char* blurb(Style s);  // a line on what it does

// How strong a style is: 1 the style itself, 0 the figure as made.
inline constexpr double kLeastStrength = 0.0, kMostStrength = 2.0;

struct Look {
    rig::Proportions proportions = rig::asMade();
};
// The style's proportions and blockiness for a figure with this skeleton (as made), at
// this strength. Each part's share goes as (its share in the style) ^ strength, kept in
// the proportions' range.
Look look(Style s, const rig::Points& asMade, double strength = 1.0);

// A bobblehead's spring: how wide it is outside, its wire, and the room left round it.
struct Spring {
    double diameter = 5.0;
    double wire = 0.5;
    double fit = 0.2;
};
inline constexpr double kLeastSpring = 2.0, kMostSpring = 20.0;
// The post on the neck (the spring's inside, less a little) and the socket in the head
// (the spring's outside and the fit all round): their diameters and how far each goes.
struct SpringSizes {
    double post = 0.0, postHeight = 0.0;
    double socket = 0.0, socketDepth = 0.0;
};
SpringSizes sizesFor(const Spring& s);
// Throws std::invalid_argument (in plain words) if the spring can't be used.
void check(const Spring& s);

struct Bobble {
    ObjectId body = kInvalidId;
    ObjectId head = kInvalidId;
};

// Where the head comes off: a flat cut at the neck. Found where the neck's narrowest
// between the neck's ball and the shoulders (a big head widens the neck just under it),
// then moved by hand: `raise` mm up the neck (down if less than 0), tipped `tilt` degrees
// forward (the face down; less than 0 back). `radius`: the neck's there, typically.
struct NeckCut {
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 0, 1};  // up into the head
    double radius = 0.0;
    double shift = 0.0;  // along the neck from where Cut into Parts would cut it
    double tilt = 0.0;
};
inline constexpr double kMostRaise = 30.0, kMostTilt = 45.0;
// `surface`, `points`: the figure and its skeleton, in the same coordinates.
NeckCut neckCut(const Mesh& surface, const rig::Points& points, double raise = 0.0, double tilt = 0.0);
// A flat disc showing the cut (`radius` across), for the preview.
Mesh cutDisc(const NeckCut& cut, double radius);

// Cuts the figure (a shape on the plate; `points` its skeleton in plate coordinates) at
// the neck, puts a post for the spring on the neck and a socket for it in the head, as one
// undo step. Throws (changing nothing) if the neck's too thin for the spring.
Bobble separateHead(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points,
                    const Spring& spring, double raise = 0.0, double tilt = 0.0);
// The same, the head being what's masked (dabs in plate coordinates): cut through the
// flat that best fits the mask's edge, which must go once round the neck.
Bobble separateHeadByMask(CommandStack& stack, Document& doc, ObjectId figure, const std::vector<MaskDab>& dabs,
                          const Spring& spring);

}  // namespace figurestyle
}  // namespace forge
