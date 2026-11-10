// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Mesh.h"

#include <algorithm>
#include <cmath>

namespace forge {

void Bounds::expand(float x, float y, float z)
{
    if (!valid) {
        min = {x, y, z};
        max = {x, y, z};
        valid = true;
        return;
    }
    min[0] = std::min(min[0], x);
    min[1] = std::min(min[1], y);
    min[2] = std::min(min[2], z);
    max[0] = std::max(max[0], x);
    max[1] = std::max(max[1], y);
    max[2] = std::max(max[2], z);
}

std::array<float, 3> Bounds::center() const
{
    return {(min[0] + max[0]) * 0.5f, (min[1] + max[1]) * 0.5f, (min[2] + max[2]) * 0.5f};
}

std::array<float, 3> Bounds::size() const
{
    return {max[0] - min[0], max[1] - min[1], max[2] - min[2]};
}

float Bounds::radius() const
{
    const auto s = size();
    return 0.5f * std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
}

Bounds Mesh::bounds() const
{
    Bounds b;
    for (std::size_t i = 0; i + 2 < positions.size(); i += 3)
        b.expand(positions[i], positions[i + 1], positions[i + 2]);
    return b;
}

}  // namespace forge
