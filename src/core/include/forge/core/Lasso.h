// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <array>
#include <string>
#include <vector>

namespace forge {

class CommandStack;
class Document;

// Lasso cut and lasso mask (slice 99): a loop drawn round part of the view, reaching
// straight through the shape the way the view looks.
namespace lasso {

// A loop as the plate sees it: its points in order, on a plane square to the view, and
// the way the view looks (into the screen).
struct Loop {
    std::vector<Vec3> points;
    Vec3 along{0.0, 0.0, -1.0};
};

// The loop's own flat frame: `origin` on its plane, u and v across it, w out of the
// screen (towards the viewer). u, v, w turn the right way (like X, Y, Z).
struct Frame {
    Vec3 origin{0, 0, 0};
    Vec3 u{1, 0, 0}, v{0, 1, 0}, w{0, 0, 1};
};
Frame frameOf(const Loop& loop);
// The loop's points flat in its frame (u, v).
std::vector<std::array<double, 2>> flat(const Loop& loop, const Frame& frame);

// The loop with points that add little dropped (closer than `tolerance` to the line
// between their neighbours), and at most `most` points (the tolerance grows to fit).
Loop simplified(const Loop& loop, double tolerance, std::size_t most = 200);
// Whether its outline crosses itself.
bool crossesItself(const Loop& loop);

// Whether a point in the loop's frame (u, v) is inside the flat outline.
bool insideFlat(const std::vector<std::array<double, 2>>& outline, double x, double y);
// How far a point (u, v) is from the flat outline's nearest side.
double distanceFlat(const std::vector<std::array<double, 2>>& outline, double x, double y);

// The cutter: the loop's outline as a sketch, pushed through both ways far enough to go
// right through `through` (a pad, so it can be changed later like any sketch).
// Throws std::invalid_argument (in plain words) for a loop that's too small or crosses
// itself.
Body cutter(const Loop& loop, const Bounds& through, const std::string& name = "Lasso");

enum class Keep {
    Outside,  // what's inside the loop is cut away (a Subtract step)
    Inside,   // only what's inside the loop is kept (an Intersect step)
    Both,     // two shapes: "X (inside)" and "X (outside)", the second from a copy
};
const char* title(Keep keep);

// Cuts shape `id` (on the plate) through along the loop, as one undo step. Returns the
// shapes made. Throws std::invalid_argument (changing nothing) if the loop misses the
// shape or would leave nothing.
std::vector<ObjectId> cut(CommandStack& stack, Document& doc, ObjectId id, const Loop& loop, Keep keep);

}  // namespace lasso

}  // namespace forge
