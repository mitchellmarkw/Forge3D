// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The mesh engine: solids made of triangles (imported meshes, and anything combined
// with them), using the Manifold library. Much faster than the B-rep engine on
// meshes with many triangles. Manifold stays hidden behind this header.

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <memory>
#include <stdexcept>
#include <vector>

namespace forge {

struct MeshSolidImpl;
// A watertight triangle solid. Immutable and cheap to share.
using MeshSolid = std::shared_ptr<const MeshSolidImpl>;

class MeshEngineError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

namespace meshkernel {

// From an imported mesh, stretched by scale. Throws MeshEngineError if it isn't a
// clean closed surface.
MeshSolid fromMeshData(const MeshData& data, const Vec3& scale);
// From welded triangles (e.g. a tessellated B-rep shape).
MeshSolid fromTriangles(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices);

MeshSolid transformed(const MeshSolid& solid, const Transform& t);
MeshSolid merged(const std::vector<MeshSolid>& solids);
MeshSolid subtracted(const MeshSolid& from, const std::vector<MeshSolid>& tools);
MeshSolid intersected(const std::vector<MeshSolid>& solids);
// Keeps the side `normal` points to.
MeshSolid trimmed(const MeshSolid& solid, const Vec3& point, const Vec3& normal);
MeshSolid mirrored(const MeshSolid& solid, const Vec3& point, const Vec3& normal);
// Side by side, not merged (a group).
MeshSolid together(const std::vector<MeshSolid>& solids);

bool isEmpty(const MeshSolid& solid);
double volume(const MeshSolid& solid);
std::size_t triangleCount(const MeshSolid& solid);
// Bounds after moving by t.
Bounds bounds(const MeshSolid& solid, const Transform& t = Transform::identity());
// Flat-shaded triangles for drawing.
Mesh displayMesh(const MeshSolid& solid);
// Shared-point triangles (for export).
void triangles(const MeshSolid& solid, std::vector<float>& positions, std::vector<std::uint32_t>& indices);

}  // namespace meshkernel
}  // namespace forge
