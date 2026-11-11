// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <array>
#include <optional>

namespace forge {

class Document;

// A ray in plate coordinates (millimetres). direction need not be normalised.
struct Ray {
    std::array<double, 3> origin{0.0, 0.0, 0.0};
    std::array<double, 3> direction{0.0, 0.0, -1.0};
};

struct PickHit {
    ObjectId id = kInvalidId;
    double distance = 0.0;  // along the normalised ray, in mm
    std::array<double, 3> point{0.0, 0.0, 0.0};
};

// The body whose surface the ray hits first, if any. Tests every triangle of
// every body (fine for hundreds of bodies; a BVH comes with sculpting).
std::optional<PickHit> pick(const Document& doc, const Ray& ray);

}  // namespace forge
