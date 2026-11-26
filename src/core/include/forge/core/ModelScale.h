// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>
#include <vector>

// Model scales (Forge3D Scenery, slice 110): how many real millimetres one model
// millimetre stands for. A real brick 194 mm long is 194 / 35 = 5.5 mm at 1:35.
namespace forge::modelscale {

struct Scale {
    double ratio = 35.0;  // 1:ratio
    double toModel(double realMm) const { return realMm / ratio; }
    double toReal(double modelMm) const { return modelMm * ratio; }
    bool operator==(const Scale&) const = default;
};

// The usual ones, as hobbyists know them ("1:35", "1:87 (HO)").
struct Named {
    Scale scale;
    std::string name;
};
const std::vector<Named>& common();

// "1:35", "1/35", "35", "1:87.1" (and spaces): the scale; none if it isn't one, or is
// outside 1:1 to 1:1000.
std::optional<Scale> parse(const std::string& text);
// "1:35", "1:87.1"
std::string format(const Scale& s);

inline constexpr double kLargest = 1.0, kSmallest = 1000.0;

}  // namespace forge::modelscale
