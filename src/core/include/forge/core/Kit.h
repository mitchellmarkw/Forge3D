// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <string>
#include <vector>

// Kit layout: small parts (hands, accessories) laid out in rows on a frame like a model
// kit's sprue, each joined to the nearest bar by a thin gate, with its number on a tag
// beside it. The frame is one shape; the parts can be joined to it into one.
namespace forge {

class CommandStack;
class Document;

namespace kit {

struct Settings {
    double gap = 4.0;      // between a part and the bars, and between parts in a row, mm
    double bar = 2.0;      // the frame's bars: square, this wide and tall
    double gate = 0.8;     // the gates' thickness (round)
    bool numbers = true;   // a numbered tag by each part (and the parts renamed "1 Hand"...)
    double numberHeight = 3.0;
    bool join = true;      // the parts and frame merged into one shape
    // The tree's name, raised on a tag under the frame (and the kit named after it).
    std::string name;
    bool includeName = true;
};

// Throws std::invalid_argument (in plain words) if the settings can't make a kit.
void check(const Settings& s);

// Where each part goes and the frame round them, worked out without changing anything.
struct Slot {
    ObjectId id = kInvalidId;
    int number = 0;
    double dx = 0.0, dy = 0.0;  // how far it moves on the plate
};
struct Plan {
    std::vector<Slot> placed;  // in number order (row by row, left to right)
    Mesh frame;               // bars, gates, tags and numbers, plate coordinates
    Bounds area;              // the whole kit's footprint
};
Plan plan(const Document& doc, const std::vector<ObjectId>& ids, const Settings& s);

// Lays the shapes `ids` (on the plate) out as a kit, as one undo step ("Make kit"):
// moves (and numbers) them, adds the frame, and merges them all into "Kit" if asked.
// Returns the frame, or the merged kit.
ObjectId make(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids, const Settings& s);

}  // namespace kit
}  // namespace forge
