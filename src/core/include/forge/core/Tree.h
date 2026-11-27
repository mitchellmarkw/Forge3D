// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/TreeSpec.h"

#include <string>

// Forge3D Scenery's tree designer (slice 114): broadleaf, conifer, dead and palm trees from
// real sizes at the model's scale, each one different (its seed), with lumpy foliage
// clumps (printable) and flared roots cut flat to stand on.
namespace forge {

class CommandStack;
class Document;

namespace tree {

TreeSpec defaults(TreeKind k, const TreeSpec& from = {});
const char* title(TreeKind k);
// Throws std::invalid_argument, in plain words, if it can't be made.
void check(const TreeSpec& s);

// The thinnest branch made (model mm across): thinner ones are thickened to it, or left off.
double thinnest(const TreeSpec& s);

// In model mm: the trunk's foot at X = Y = 0, standing on Z = 0. `quick`: the parts
// overlapping, not joined (for a preview), and simpler.
Mesh make(const TreeSpec& s, bool quick = false);

Body body(const TreeSpec& s, const Mesh& made, const std::string& name, double x, double y);
ObjectId add(CommandStack& stack, Document& doc, TreeSpec s, const std::string& name, double x, double y);
void update(CommandStack& stack, Document& doc, ObjectId id, TreeSpec s);

}  // namespace tree
}  // namespace forge
