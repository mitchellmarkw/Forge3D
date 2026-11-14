// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Sculpt.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace forge::sculpt {

namespace {

struct V3 {
    double x, y, z;
};
V3 at(const std::vector<float>& p, std::uint32_t i) { return {p[3 * i], p[3 * i + 1], p[3 * i + 2]}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double len(V3 a) { return std::sqrt(dot(a, a)); }
V3 toV(const Vec3& v) { return {v[0], v[1], v[2]}; }

// Vertex normals: the area-weighted sum of the face normals round each point.
void allNormals(Mesh& m)
{
    m.normals.assign(m.positions.size(), 0.f);
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const std::uint32_t a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
        const V3 n = cross(at(m.positions, b) - at(m.positions, a), at(m.positions, c) - at(m.positions, a));
        for (std::uint32_t v : {a, b, c}) {
            m.normals[3 * v] += float(n.x);
            m.normals[3 * v + 1] += float(n.y);
            m.normals[3 * v + 2] += float(n.z);
        }
    }
    for (std::size_t v = 0; v + 2 < m.normals.size(); v += 3) {
        const double l = std::sqrt(double(m.normals[v]) * m.normals[v] + double(m.normals[v + 1]) * m.normals[v + 1] +
                                   double(m.normals[v + 2]) * m.normals[v + 2]);
        if (l > 0)
            for (int c = 0; c < 3; ++c)
                m.normals[v + c] = float(m.normals[v + c] / l);
    }
}

// Smooth falloff: 1 in the middle, 0 at the edge.
double falloff(double d, double radius)
{
    const double t = std::clamp(1.0 - d / radius, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

std::uint64_t edgeKey(std::uint32_t a, std::uint32_t b)
{
    return a < b ? (std::uint64_t(a) << 32 | b) : (std::uint64_t(b) << 32 | a);
}

}  // namespace

Mesh welded(const Mesh& mesh, double tolerance)
{
    if (!(tolerance > 0.0))
        throw std::invalid_argument("welded: tolerance must be above zero");
    struct KeyHash {
        std::size_t operator()(const std::array<std::int64_t, 3>& k) const
        {
            return std::size_t(k[0] * 73856093 ^ k[1] * 19349663 ^ k[2] * 83492791);
        }
    };
    std::unordered_map<std::array<std::int64_t, 3>, std::uint32_t, KeyHash> seen;
    Mesh out;
    std::vector<std::uint32_t> remap(mesh.vertexCount());
    for (std::size_t v = 0; v < mesh.vertexCount(); ++v) {
        const std::array<std::int64_t, 3> key{std::llround(mesh.positions[3 * v] / tolerance),
                                              std::llround(mesh.positions[3 * v + 1] / tolerance),
                                              std::llround(mesh.positions[3 * v + 2] / tolerance)};
        const auto [it, added] = seen.try_emplace(key, std::uint32_t(out.positions.size() / 3));
        if (added)
            out.positions.insert(out.positions.end(), mesh.positions.begin() + 3 * v, mesh.positions.begin() + 3 * v + 3);
        remap[v] = it->second;
    }
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t a = remap[mesh.indices[t]], b = remap[mesh.indices[t + 1]], c = remap[mesh.indices[t + 2]];
        if (a == b || b == c || a == c)
            continue;  // collapsed to nothing
        out.indices.insert(out.indices.end(), {a, b, c});
    }
    allNormals(out);
    return out;
}

double edgeForTriangles(const Mesh& mesh, std::size_t triangles)
{
    double area = 0.0;
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
        area += len(cross(at(mesh.positions, mesh.indices[t + 1]) - at(mesh.positions, mesh.indices[t]),
                          at(mesh.positions, mesh.indices[t + 2]) - at(mesh.positions, mesh.indices[t]))) /
                2.0;
    // An equilateral triangle of side e covers e^2 * sqrt(3) / 4.
    return std::sqrt(4.0 * area / (std::sqrt(3.0) * double(std::max<std::size_t>(triangles, 1))));
}

Mesh refined(const Mesh& mesh, double maxEdge, std::size_t maxTriangles)
{
    if (!(maxEdge > 0.0))
        throw std::invalid_argument("refined: maxEdge must be above zero");
    Mesh m = mesh;
    const double limit2 = maxEdge * maxEdge;
    for (int pass = 0; pass < 40; ++pass) {
        // Every long edge gets a middle point, shared by both triangles on it.
        std::unordered_map<std::uint64_t, std::uint32_t> middle;
        const auto mid = [&](std::uint32_t a, std::uint32_t b) -> std::int64_t {
            const V3 pa = at(m.positions, a), pb = at(m.positions, b);
            const V3 d = pb - pa;
            if (dot(d, d) <= limit2)
                return -1;
            const auto [it, added] = middle.try_emplace(edgeKey(a, b), std::uint32_t(m.positions.size() / 3));
            if (added) {
                const V3 c = (pa + pb) * 0.5;
                m.positions.insert(m.positions.end(), {float(c.x), float(c.y), float(c.z)});
            }
            return it->second;
        };
        std::vector<std::uint32_t> next;
        next.reserve(m.indices.size() * 2);
        bool any = false;
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            const std::uint32_t v[3] = {m.indices[t], m.indices[t + 1], m.indices[t + 2]};
            const std::int64_t e[3] = {mid(v[0], v[1]), mid(v[1], v[2]), mid(v[2], v[0])};
            const int splits = (e[0] >= 0) + (e[1] >= 0) + (e[2] >= 0);
            if (splits == 0) {
                next.insert(next.end(), {v[0], v[1], v[2]});
                continue;
            }
            any = true;
            if (splits == 3) {
                const auto a = std::uint32_t(e[0]), b = std::uint32_t(e[1]), c = std::uint32_t(e[2]);
                next.insert(next.end(), {v[0], a, c, a, v[1], b, c, b, v[2], a, b, c});
                continue;
            }
            // Turn so the first split edge is v0-v1 (edge 0).
            int r = 0;
            while (e[r] < 0)
                ++r;
            if (splits == 2 && e[(r + 2) % 3] >= 0 && r == 0)
                r = 2;  // edges 2 and 0 split: start from edge 2 so the pair is (r, r+1)
            const std::uint32_t a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3];
            const auto m0 = std::uint32_t(e[r]);
            if (splits == 1) {
                next.insert(next.end(), {a, m0, c, m0, b, c});
            } else {
                const auto m1 = std::uint32_t(e[(r + 1) % 3]);  // on b-c
                next.insert(next.end(), {m0, b, m1, a, m0, m1, a, m1, c});
            }
        }
        m.indices = std::move(next);
        if (!any || m.indices.size() / 3 >= maxTriangles)
            break;
    }
    allNormals(m);
    return m;
}

Sculptor::Sculptor(Mesh mesh) : m_mesh(std::move(mesh))
{
    const std::size_t n = m_mesh.vertexCount();
    if (m_mesh.normals.size() != m_mesh.positions.size())
        allNormals(m_mesh);
    // Triangles round each point.
    m_triangleStart.assign(n + 1, 0);
    for (std::uint32_t v : m_mesh.indices)
        ++m_triangleStart[v + 1];
    for (std::size_t v = 0; v < n; ++v)
        m_triangleStart[v + 1] += m_triangleStart[v];
    m_triangles.resize(m_mesh.indices.size());
    {
        std::vector<std::uint32_t> fill(m_triangleStart.begin(), m_triangleStart.end() - 1);
        for (std::size_t i = 0; i < m_mesh.indices.size(); ++i)
            m_triangles[fill[m_mesh.indices[i]]++] = std::uint32_t(i / 3);
    }
    // Neighbours: the other corners of those triangles, once each.
    m_neighbourStart.assign(n + 1, 0);
    std::vector<std::uint32_t> list;
    for (std::size_t v = 0; v < n; ++v) {
        list.clear();
        for (std::uint32_t k = m_triangleStart[v]; k < m_triangleStart[v + 1]; ++k)
            for (int c = 0; c < 3; ++c) {
                const std::uint32_t w = m_mesh.indices[3 * m_triangles[k] + c];
                if (w != v)
                    list.push_back(w);
            }
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
        m_neighbours.insert(m_neighbours.end(), list.begin(), list.end());
        m_neighbourStart[v + 1] = std::uint32_t(m_neighbours.size());
    }
    m_mark.assign(n, 0);
}

std::optional<Hit> Sculptor::hit(const Vec3& origin, const Vec3& direction) const
{
    const V3 o = toV(origin), d = toV(direction);
    double best = 1e300;
    std::optional<Hit> out;
    const auto& p = m_mesh.positions;
    for (std::size_t t = 0; t + 2 < m_mesh.indices.size(); t += 3) {
        const V3 a = at(p, m_mesh.indices[t]), b = at(p, m_mesh.indices[t + 1]), c = at(p, m_mesh.indices[t + 2]);
        // Möller–Trumbore.
        const V3 e1 = b - a, e2 = c - a, h = cross(d, e2);
        const double det = dot(e1, h);
        if (std::abs(det) < 1e-14)
            continue;
        const double f = 1.0 / det;
        const V3 s = o - a;
        const double u = f * dot(s, h);
        if (u < 0.0 || u > 1.0)
            continue;
        const V3 q = cross(s, e1);
        const double w = f * dot(d, q);
        if (w < 0.0 || u + w > 1.0)
            continue;
        const double dist = f * dot(e2, q);
        if (dist > 1e-9 && dist < best) {
            best = dist;
            const V3 n = cross(e1, e2);
            const double l = len(n);
            const V3 pt = o + d * dist;
            out = Hit{{pt.x, pt.y, pt.z}, {n.x / l, n.y / l, n.z / l}, std::uint32_t(t / 3)};
        }
    }
    return out;
}

std::vector<std::uint32_t> Sculptor::pointsWithin(const Vec3& centre, double radius, std::uint32_t nearTriangle)
{
    // A walk out from the triangle under the brush, through neighbours, keeping to the
    // ball: only the surface the brush touches, not a part that just passes near.
    ++m_walk;
    if (m_walk == 0) {
        std::fill(m_mark.begin(), m_mark.end(), 0);
        m_walk = 1;
    }
    const V3 c = toV(centre);
    const double r2 = radius * radius;
    std::vector<std::uint32_t> out, todo;
    for (int k = 0; k < 3 && 3 * std::size_t(nearTriangle) + 2 < m_mesh.indices.size(); ++k) {
        const std::uint32_t v = m_mesh.indices[3 * std::size_t(nearTriangle) + k];
        if (m_mark[v] != m_walk) {
            m_mark[v] = m_walk;
            todo.push_back(v);
        }
    }
    while (!todo.empty()) {
        const std::uint32_t v = todo.back();
        todo.pop_back();
        const V3 d = at(m_mesh.positions, v) - c;
        if (dot(d, d) > r2)
            continue;
        out.push_back(v);
        for (std::uint32_t k = m_neighbourStart[v]; k < m_neighbourStart[v + 1]; ++k) {
            const std::uint32_t w = m_neighbours[k];
            if (m_mark[w] != m_walk) {
                m_mark[w] = m_walk;
                todo.push_back(w);
            }
        }
    }
    return out;
}

void Sculptor::normalsAround(const std::vector<std::uint32_t>& points)
{
    auto& p = m_mesh.positions;
    auto& nrm = m_mesh.normals;
    for (std::uint32_t v : points) {
        V3 sum{0, 0, 0};
        for (std::uint32_t k = m_triangleStart[v]; k < m_triangleStart[v + 1]; ++k) {
            const std::size_t t = 3 * std::size_t(m_triangles[k]);
            const std::uint32_t a = m_mesh.indices[t], b = m_mesh.indices[t + 1], c = m_mesh.indices[t + 2];
            sum = sum + cross(at(p, b) - at(p, a), at(p, c) - at(p, a));
        }
        const double l = len(sum);
        if (l > 0)
            sum = sum * (1.0 / l);
        nrm[3 * v] = float(sum.x);
        nrm[3 * v + 1] = float(sum.y);
        nrm[3 * v + 2] = float(sum.z);
    }
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::apply(const Dab& dab, std::uint32_t nearTriangle)
{
    if (!(dab.radius > 0.0) || 3 * std::size_t(nearTriangle) + 2 >= m_mesh.indices.size())
        return {0, 0};
    const std::vector<std::uint32_t> touched = pointsWithin(dab.centre, dab.radius, nearTriangle);
    if (touched.empty())
        return {0, 0};
    const V3 c = toV(dab.centre);
    auto& p = m_mesh.positions;
    const double strength = std::clamp(dab.strength, 0.0, 1.0);
    std::vector<V3> moved(touched.size());
    if (dab.brush == Brush::Draw) {
        // Out along the surface's average facing under the brush.
        V3 n{0, 0, 0};
        for (std::uint32_t v : touched)
            n = n + V3{m_mesh.normals[3 * v], m_mesh.normals[3 * v + 1], m_mesh.normals[3 * v + 2]} *
                        falloff(len(at(p, v) - c), dab.radius);
        const double l = len(n);
        if (!(l > 0))
            return {0, 0};
        n = n * ((dab.invert ? -1.0 : 1.0) / l);
        const double most = 0.08 * dab.radius * strength;  // one dab, at the middle
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const V3 q = at(p, touched[i]);
            moved[i] = q + n * (most * falloff(len(q - c), dab.radius));
        }
    } else {
        // Each point part of the way to the middle of its neighbours, a few times over
        // (one step only evens out the finest ripples on a dense mesh).
        std::vector<V3> now(touched.size());
        for (std::size_t i = 0; i < touched.size(); ++i)
            moved[i] = at(p, touched[i]);
        for (int round = 0; round < 4; ++round) {
            for (std::size_t i = 0; i < touched.size(); ++i) {
                const std::uint32_t v = touched[i];
                const V3 q = moved[i];
                V3 avg{0, 0, 0};
                const std::uint32_t count = m_neighbourStart[v + 1] - m_neighbourStart[v];
                for (std::uint32_t k = m_neighbourStart[v]; k < m_neighbourStart[v + 1]; ++k)
                    avg = avg + at(p, m_neighbours[k]);
                if (count == 0) {
                    now[i] = q;
                    continue;
                }
                avg = avg * (1.0 / count);
                now[i] = q + (avg - q) * (strength * falloff(len(q - c), dab.radius));
            }
            moved.swap(now);
            for (std::size_t i = 0; i < touched.size(); ++i) {
                const std::uint32_t v = touched[i];
                p[3 * v] = float(moved[i].x);
                p[3 * v + 1] = float(moved[i].y);
                p[3 * v + 2] = float(moved[i].z);
            }
        }
    }
    std::uint32_t lo = UINT32_MAX, hi = 0;
    for (std::size_t i = 0; i < touched.size(); ++i) {
        const std::uint32_t v = touched[i];
        p[3 * v] = float(moved[i].x);
        p[3 * v + 1] = float(moved[i].y);
        p[3 * v + 2] = float(moved[i].z);
    }
    // Normals: the moved points and their neighbours (whose triangles moved too).
    std::vector<std::uint32_t> around = touched;
    for (std::uint32_t v : touched)
        for (std::uint32_t k = m_neighbourStart[v]; k < m_neighbourStart[v + 1]; ++k)
            around.push_back(m_neighbours[k]);
    std::sort(around.begin(), around.end());
    around.erase(std::unique(around.begin(), around.end()), around.end());
    normalsAround(around);
    for (std::uint32_t v : around) {
        lo = std::min(lo, v);
        hi = std::max(hi, v + 1);
    }
    return {lo, hi};
}

void Sculptor::setPositions(const std::vector<float>& positions)
{
    if (positions.size() != m_mesh.positions.size())
        throw std::invalid_argument("setPositions: a different number of points");
    m_mesh.positions = positions;
    allNormals(m_mesh);
}

}  // namespace forge::sculpt
