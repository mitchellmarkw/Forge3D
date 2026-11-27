// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/RockSpec.h"

#include <string>

// Forge3D Scenery's rock designer (slice 112): boulders, crags, slabs and piles, from real
// sizes at the model's scale, each one different (its seed), sunk into the ground with a
// flat base to print on.
namespace forge {

class CommandStack;
class Document;

namespace rock {

RockSpec defaults(RockKind k, const RockSpec& from = {});
const char* title(RockKind k);
// Throws std::invalid_argument, in plain words, if it can't be made.
void check(const RockSpec& s);

// In model mm: centred on X = Y = 0, its base on Z = 0. `quick`: coarser, for a preview.
Mesh make(const RockSpec& s, bool quick = false);

Body body(const RockSpec& s, const Mesh& made, const std::string& name, double x, double y);
// Adds a new rock at (x, y) on the plate, one undo step; its id.
ObjectId add(CommandStack& stack, Document& doc, RockSpec s, const std::string& name, double x, double y);
// Makes the rock `id` again from `s`, where it is: one undo step.
void update(CommandStack& stack, Document& doc, ObjectId id, RockSpec s);

}  // namespace rock
}  // namespace forge
