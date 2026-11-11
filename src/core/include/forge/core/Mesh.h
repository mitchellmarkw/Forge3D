// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace forge {

// Axis-aligned bounding box in model units (millimetres).
struct Bounds {
    std::array<float, 3> min{0.f, 0.f, 0.f};
    std::array<float, 3> max{0.f, 0.f, 0.f};
    bool valid = false;

    void expand(float x, float y, float z);
    std::array<float, 3> center() const;
    std::array<float, 3> size() const;
    float radius() const;  // half the diagonal
};

// An indexed triangle mesh ready for the GPU.
// positions and normals hold 3 floats per vertex; indices hold 3 per triangle.
struct Mesh {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<std::uint32_t> indices;

    std::size_t vertexCount() const { return positions.size() / 3; }
    std::size_t triangleCount() const { return indices.size() / 3; }
    bool empty() const { return indices.empty(); }

    Bounds bounds() const;
};

}  // namespace forge
