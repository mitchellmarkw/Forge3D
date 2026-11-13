// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <optional>
#include <string>
#include <vector>

namespace forge {

class Document;

namespace pattern {

// Where each copy goes (the first, the original, is the identity).
std::vector<Transform> placements(const PatternFinish& f);
// How many copies (the original included).
int copies(const PatternFinish& f);
// The feature `step` adds or cuts, if it can be repeated: a Merge / Subtract of two
// parts (a pad, pocket, revolve, groove, hole, or any shape added or cut), or a Follow.
// Its name for the panel ("Hole 1"), else nothing.
std::optional<std::string> featureName(const Document& doc, ObjectId step);

}  // namespace pattern

// Reasonable settings to start from for patterning `target` (in plate coordinates).
PatternFinish startingPattern(const Document& doc, ObjectId target, bool feature);
// A Pattern step on `target`, the finish in plate coordinates.
BooleanSpec patternSpec(const Document& doc, ObjectId target, const PatternFinish& inPlate);
// Its finish in plate coordinates (to edit), and the result changed.
PatternFinish patternInPlate(const Document& doc, ObjectId result);
Body withPattern(const Document& doc, ObjectId result, const PatternFinish& inPlate);

}  // namespace forge
