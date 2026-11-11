// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Picking.h"

#include "forge/core/Document.h"
#include "forge/core/Transform.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace forge {

namespace {

struct V3 {
    double x, y, z;
};
V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Möller–Trumbore; returns the ray parameter t, or a negative value for no hit.
// Both faces count, so open or inverted meshes can still be picked.
double intersect(V3 o, V3 d, V3 a, V3 b, V3 c)
{
    constexpr double kEps = 1e-12;
    const V3 e1 = sub(b, a);
    const V3 e2 = sub(c, a);
    const V3 p = cross(d, e2);
    const double det = dot(e1, p);
    if (std::abs(det) < kEps)
        return -1.0;
    const double inv = 1.0 / det;
    const V3 s = sub(o, a);
    const double u = dot(s, p) * inv;
    if (u < 0.0 || u > 1.0)
        return -1.0;
    const V3 q = cross(s, e1);
    const double v = dot(d, q) * inv;
    if (v < 0.0 || u + v > 1.0)
        return -1.0;
    return dot(e2, q) * inv;
}

}  // namespace

std::optional<PickHit> pick(const Document& doc, const Ray& ray)
{
    const double len = std::sqrt(ray.direction[0] * ray.direction[0] + ray.direction[1] * ray.direction[1] +
                                 ray.direction[2] * ray.direction[2]);
    if (!(len > 0.0))
        return std::nullopt;
    const V3 worldDir{ray.direction[0] / len, ray.direction[1] / len, ray.direction[2] / len};

    std::optional<PickHit> best;
    for (const ObjectId id : doc.topLevelIds()) {  // parts inside a result are not on the plate
        // Move the ray into the body's own coordinates instead of moving every vertex.
        // Rigid transforms keep lengths, so t along the local ray = distance in mm.
        const Transform toLocal = doc.worldTransform(id).inverse();
        const Vec3 lo = toLocal.apply(ray.origin);
        const Vec3 ld = toLocal.applyDirection({worldDir.x, worldDir.y, worldDir.z});
        const V3 o{lo[0], lo[1], lo[2]};
        const V3 d{ld[0], ld[1], ld[2]};

        const auto mesh = doc.mesh(id);
        const auto& P = mesh->positions;
        const auto vertex = [&P](std::uint32_t i) {
            return V3{P[3 * i], P[3 * i + 1], P[3 * i + 2]};
        };
        for (std::size_t t = 0; t + 2 < mesh->indices.size(); t += 3) {
            const double hit = intersect(o, d, vertex(mesh->indices[t]), vertex(mesh->indices[t + 1]),
                                         vertex(mesh->indices[t + 2]));
            if (hit > 1e-9 && (!best || hit < best->distance)) {
                best = PickHit{id, hit,
                               {ray.origin[0] + worldDir.x * hit, ray.origin[1] + worldDir.y * hit,
                                ray.origin[2] + worldDir.z * hit}};
            }
        }
    }
    return best;
}

}  // namespace forge
