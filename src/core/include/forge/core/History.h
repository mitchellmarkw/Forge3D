// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <string>
#include <vector>

namespace forge {

class Document;

// An object's history, read from its tree of results: the main line follows each
// step's first part down to the shape it all started from.
namespace history {

// The main line of the object whose top is `top`, oldest first: the starting shape,
// then each step built on it, ending with `top` itself.
std::vector<ObjectId> steps(const Document& doc, ObjectId top);

// The top-level object a body belongs to.
ObjectId objectOf(const Document& doc, ObjectId id);

// How many of the object's steps are shown: the rollback bar sits after this many
// (all of them when nothing is rolled back). Always at least 1 (the starting shape).
std::size_t barAt(const Document& doc, ObjectId top);
// The bodies to change so the bar sits after `shown` steps (for EditBodiesCommand):
// steps after it are rolled back, the rest rolled forward.
std::vector<Body> rolledTo(const Document& doc, ObjectId top, std::size_t shown);
// The step new steps go on while the object is rolled back: the last one shown.
ObjectId tip(const Document& doc, ObjectId top);

// A short description of what a body is or does: "20 × 20 × 10 mm",
// "4 edges rounded R2", "\"Hello\" raised", "minus Cylinder 1".
std::string describe(const Document& doc, ObjectId id);

}  // namespace history
}  // namespace forge
