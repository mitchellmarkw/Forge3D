// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <vector>

// Rotate for Best Print (slice 89): which way up to print a part, for the quickest print,
// the cleanest surface, or the strongest part, worked out from its shape.
namespace forge {

class Document;

namespace orient {

enum class Goal {
    Fastest,    // the fewest layers and the least support
    Quality,    // the least surface touched by supports, and the least stair-stepping
    Strongest,  // layers along the part's length (it's weakest across them), on a steady base
};
const char* title(Goal goal);

// How a part prints lying a given way: `down` (the part's own coordinates) is the way
// that faces the plate.
struct Facts {
    Vec3 down{0, 0, -1};
    double height = 0.0;         // mm: how tall it stands (layers)
    double overhangArea = 0.0;   // mm² of surface leaning out past the angle (supports touch it)
    double supportVolume = 0.0;  // mm³, roughly, under those faces down to the plate
    double baseArea = 0.0;       // mm² lying flat on the plate
    double staircase = 0.0;      // mm² of gently sloping surface, which shows the layers most
    double alongLength = 0.0;    // 0..1: how much the part's length stands upright (0 lying flat)
    double score = 0.0;          // lower is better, for the goal
};

struct Settings {
    double overhangAngle = 45.0;  // degrees from upright: faces leaning out further need supports
};

// The facts for `mesh` printed with `down` toward the plate, scored for `goal`.
Facts facts(const Mesh& mesh, const Vec3& down, Goal goal, const Settings& s = {});

// The best way down among many tried (its flat faces, the six sides and the turns between,
// and directions all round), and how it compares with `current` (the way down now).
struct Choice {
    Facts best;
    Facts now;
};
Choice best(const Mesh& mesh, Goal goal, const Vec3& current = {0, 0, -1}, const Settings& s = {});

// The placement that prints shape `id` (on the plate) best for `goal`: turned about its
// middle, the least needed, and resting on the plate. `choice` (if not null) gets the
// facts, in plate coordinates.
Placement bestPlacement(const Document& doc, ObjectId id, Goal goal, Choice* choice = nullptr, const Settings& s = {});

}  // namespace orient

}  // namespace forge
