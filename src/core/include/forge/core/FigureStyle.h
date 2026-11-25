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

// Bobblehead: a big head on a smaller body. Vinyl collectible: a huge head on a short,
// chunky body with stubby limbs. Caricature: what's already big made bigger, what's
// small smaller, and a bigger head. Blocky: squared off, like a toy built from blocks.
enum class Style { Bobblehead, Vinyl, Caricature, Blocky };
inline constexpr int kStyleCount = 4;
const char* title(Style s);  // "Bobblehead", "Vinyl collectible"...
const char* blurb(Style s);  // a line on what it does

// How strong a style is: 1 the style itself, 0 the figure as made.
inline constexpr double kLeastStrength = 0.0, kMostStrength = 1.5;

struct Look {
    rig::Proportions proportions = rig::asMade();
    double blocky = 0.0;
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
// Cuts the figure (a shape on the plate; `points` its skeleton in plate coordinates) at
// the neck, puts a post for the spring on the neck and a socket for it in the head, as one
// undo step. Throws (changing nothing) if the neck's too thin for the spring.
Bobble separateHead(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points,
                    const Spring& spring);

}  // namespace figurestyle
}  // namespace forge
