// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/MeshKernel.h"

#include <manifold/manifold.h>

#include <cmath>
#include <string>

namespace forge {

struct MeshSolidImpl {
    manifold::Manifold m;
};

namespace meshkernel {

namespace {

MeshSolid wrap(manifold::Manifold m)
{
    return std::make_shared<const MeshSolidImpl>(MeshSolidImpl{std::move(m)});
}

const manifold::Manifold& unwrap(const MeshSolid& s)
{
    if (!s)
        throw MeshEngineError("missing mesh solid");
    return s->m;
}

std::string describe(manifold::Manifold::Error e)
{
    using E = manifold::Manifold::Error;
    switch (e) {
    case E::NotManifold:
        return "its surface isn't closed and clean (some edges are open or shared by more than two triangles)";
    case E::NonFiniteVertex: return "it has points with invalid coordinates";
    case E::VertexOutOfBounds: return "a triangle refers to a point that doesn't exist";
    case E::ResultTooLarge: return "the result would be too large";
    case E::Cancelled: return "it was cancelled";
    default: return "the mesh engine reported error " + std::to_string(static_cast<int>(e));
    }
}

MeshSolid checked(manifold::Manifold m, const char* what)
{
    const auto status = m.Status();
    if (status != manifold::Manifold::Error::NoError)
        throw MeshEngineError(std::string(what) + " didn't work: " + describe(status) + ".");
    return wrap(std::move(m));
}

manifold::mat3x4 toMat(const Transform& t)
{
    return manifold::mat3x4({t.m[0][0], t.m[1][0], t.m[2][0]}, {t.m[0][1], t.m[1][1], t.m[2][1]},
                            {t.m[0][2], t.m[1][2], t.m[2][2]}, {t.m[0][3], t.m[1][3], t.m[2][3]});
}

std::vector<manifold::Manifold> all(const std::vector<MeshSolid>& solids)
{
    std::vector<manifold::Manifold> out;
    out.reserve(solids.size());
    for (const MeshSolid& s : solids)
        out.push_back(unwrap(s));
    return out;
}

}  // namespace

MeshSolid fromTriangles(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices)
{
    manifold::MeshGL gl;
    gl.numProp = 3;
    gl.vertProperties = positions;
    gl.triVerts = indices;
    return checked(manifold::Manifold(gl), "Making a solid from the triangles");
}

MeshSolid fromMeshData(const MeshData& data, const Vec3& scale)
{
    manifold::MeshGL gl;
    gl.numProp = 3;
    gl.vertProperties.resize(data.positions.size());
    for (std::size_t i = 0; i < data.positions.size(); i += 3) {
        gl.vertProperties[i] = float(data.positions[i] * scale[0]);
        gl.vertProperties[i + 1] = float(data.positions[i + 1] * scale[1]);
        gl.vertProperties[i + 2] = float(data.positions[i + 2] * scale[2]);
    }
    gl.triVerts = data.indices;
    return checked(manifold::Manifold(gl), "Using the imported mesh");
}

MeshSolid transformed(const MeshSolid& solid, const Transform& t)
{
    return wrap(unwrap(solid).Transform(toMat(t)));
}

MeshSolid merged(const std::vector<MeshSolid>& solids)
{
    return checked(manifold::Manifold::BatchBoolean(all(solids), manifold::OpType::Add), "Merge");
}

MeshSolid subtracted(const MeshSolid& from, const std::vector<MeshSolid>& tools)
{
    std::vector<manifold::Manifold> list{unwrap(from)};
    for (const MeshSolid& t : tools)
        list.push_back(unwrap(t));
    return checked(manifold::Manifold::BatchBoolean(list, manifold::OpType::Subtract), "Subtract");
}

MeshSolid intersected(const std::vector<MeshSolid>& solids)
{
    return checked(manifold::Manifold::BatchBoolean(all(solids), manifold::OpType::Intersect), "Intersect");
}

MeshSolid trimmed(const MeshSolid& solid, const Vec3& point, const Vec3& normal)
{
    const double len = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (!(len > 0))
        throw MeshEngineError("Split needs a plane direction");
    const manifold::vec3 n{normal[0] / len, normal[1] / len, normal[2] / len};
    const double offset = n.x * point[0] + n.y * point[1] + n.z * point[2];
    return checked(unwrap(solid).TrimByPlane(n, offset), "Split");
}

MeshSolid mirrored(const MeshSolid& solid, const Vec3& point, const Vec3& normal)
{
    const manifold::vec3 p{point[0], point[1], point[2]};
    return wrap(unwrap(solid).Translate(-p).Mirror({normal[0], normal[1], normal[2]}).Translate(p));
}

MeshSolid together(const std::vector<MeshSolid>& solids)
{
    // Kept as separate pieces: overlapping members are not merged.
    std::vector<float> positions;
    std::vector<std::uint32_t> indices;
    for (const MeshSolid& s : solids) {
        const manifold::MeshGL gl = unwrap(s).GetMeshGL();
        const auto base = static_cast<std::uint32_t>(positions.size() / 3);
        for (std::size_t i = 0; i < gl.vertProperties.size(); i += gl.numProp)
            positions.insert(positions.end(), {gl.vertProperties[i], gl.vertProperties[i + 1], gl.vertProperties[i + 2]});
        for (std::uint32_t v : gl.triVerts)
            indices.push_back(base + v);
    }
    if (indices.empty())
        return wrap(manifold::Manifold());
    return fromTriangles(positions, indices);
}

bool isEmpty(const MeshSolid& solid)
{
    return unwrap(solid).IsEmpty();
}

double volume(const MeshSolid& solid)
{
    return unwrap(solid).Volume();
}

std::size_t triangleCount(const MeshSolid& solid)
{
    return unwrap(solid).NumTri();
}

Bounds bounds(const MeshSolid& solid, const Transform& t)
{
    Bounds out;
    const manifold::Manifold& m = unwrap(solid);
    if (m.IsEmpty())
        return out;
    const bool identity = t.m == Transform::identity().m;
    if (identity) {
        const manifold::Box box = m.BoundingBox();
        out.expand(float(box.min.x), float(box.min.y), float(box.min.z));
        out.expand(float(box.max.x), float(box.max.y), float(box.max.z));
        return out;
    }
    // Moved: the true box comes from the points themselves.
    const manifold::MeshGL gl = m.GetMeshGL();
    for (std::size_t i = 0; i < gl.vertProperties.size(); i += gl.numProp) {
        const Vec3 p = t.apply({gl.vertProperties[i], gl.vertProperties[i + 1], gl.vertProperties[i + 2]});
        out.expand(float(p[0]), float(p[1]), float(p[2]));
    }
    return out;
}

Mesh displayMesh(const MeshSolid& solid)
{
    const manifold::MeshGL gl = unwrap(solid).GetMeshGL();
    Mesh mesh;
    mesh.positions.reserve(gl.triVerts.size() * 3);
    mesh.normals.reserve(gl.triVerts.size() * 3);
    mesh.indices.reserve(gl.triVerts.size());
    for (std::size_t t = 0; t + 2 < gl.triVerts.size(); t += 3) {
        float p[3][3];
        for (int k = 0; k < 3; ++k)
            for (int c = 0; c < 3; ++c)
                p[k][c] = gl.vertProperties[gl.triVerts[t + k] * gl.numProp + c];
        const float ux = p[1][0] - p[0][0], uy = p[1][1] - p[0][1], uz = p[1][2] - p[0][2];
        const float wx = p[2][0] - p[0][0], wy = p[2][1] - p[0][1], wz = p[2][2] - p[0][2];
        float n[3] = {uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx};
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 0.0f)
            for (float& c : n)
                c /= len;
        for (int k = 0; k < 3; ++k) {
            mesh.indices.push_back(static_cast<std::uint32_t>(mesh.positions.size() / 3));
            mesh.positions.insert(mesh.positions.end(), {p[k][0], p[k][1], p[k][2]});
            mesh.normals.insert(mesh.normals.end(), {n[0], n[1], n[2]});
        }
    }
    return mesh;
}

void triangles(const MeshSolid& solid, std::vector<float>& positions, std::vector<std::uint32_t>& indices)
{
    const manifold::MeshGL gl = unwrap(solid).GetMeshGL();
    positions.clear();
    positions.reserve(gl.vertProperties.size() / gl.numProp * 3);
    for (std::size_t i = 0; i < gl.vertProperties.size(); i += gl.numProp)
        positions.insert(positions.end(), {gl.vertProperties[i], gl.vertProperties[i + 1], gl.vertProperties[i + 2]});
    indices = gl.triVerts;
}

}  // namespace meshkernel
}  // namespace forge
