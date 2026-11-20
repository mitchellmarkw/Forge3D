// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <optional>
#include <string>
#include <vector>

// Panel lines: thin grooves (a V, square or round section) cut along lines drawn on a
// surface, for armour plates, vehicle panels and scale models. Works on any shape. The
// groove is made a short straight piece at a time between the line's points, each
// piece upright to the surface there, the pieces overlapping at the joins.
namespace forge {

class Document;

namespace panel {

const char* title(GrooveProfile p);  // "V", "Square", "Round"
const char* key(GrooveProfile p);    // the file's name: "v", "square", "round"
std::optional<GrooveProfile> fromKey(const std::string& key);

// Throws std::invalid_argument (in plain words) if it can't be cut.
void check(const PanelLineFinish& f);
// "3 panel lines, V 0.6 × 0.5 mm"
std::string describe(const PanelLineFinish& f);

// How far apart to put a line's points as it's drawn, for a groove this wide (mm).
double pointSpacing(double width);
// What's cut away along one line, in pieces (the line's coordinates).
std::vector<MeshSolid> cutters(const PanelLine& line, const PanelLineFinish& f);
// All of it, for showing.
Mesh shown(const PanelLineFinish& f);
// The part with the grooves cut.
MeshSolid apply(const MeshSolid& part, const PanelLineFinish& f);

}  // namespace panel

// A Panel Lines step on `target` (lines in plate coordinates), its finish back in plate
// coordinates, and the step with a changed finish.
BooleanSpec panelLineSpec(const Document& doc, ObjectId target, const PanelLineFinish& inPlate);
PanelLineFinish panelLinesInPlate(const Document& doc, ObjectId result);
Body withPanelLines(const Document& doc, ObjectId result, const PanelLineFinish& inPlate);

}  // namespace forge
