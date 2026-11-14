// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Sculpt.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <functional>
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

// ---- The grid of triangles, for finding the surface under the mouse quickly ----

namespace {
constexpr std::int64_t kGridBias = 1 << 20;
std::uint64_t cellKey(std::int64_t i, std::int64_t j, std::int64_t k)
{
    return std::uint64_t(i + kGridBias) << 42 | std::uint64_t(j + kGridBias) << 21 | std::uint64_t(k + kGridBias);
}
}  // namespace

std::array<std::int32_t, 6> Sculptor::cellsOf(std::uint32_t t) const
{
    const auto& p = m_mesh.positions;
    std::array<double, 3> lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
    for (int k = 0; k < 3; ++k) {
        const std::uint32_t v = m_mesh.indices[3 * std::size_t(t) + k];
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::min(lo[c], double(p[3 * v + c]));
            hi[c] = std::max(hi[c], double(p[3 * v + c]));
        }
    }
    std::array<std::int32_t, 6> r{};
    for (int c = 0; c < 3; ++c) {
        r[c] = std::int32_t(std::floor(lo[c] / m_cell));
        r[3 + c] = std::int32_t(std::floor(hi[c] / m_cell));
    }
    return r;
}

void Sculptor::gridInsert(std::uint32_t t)
{
    const auto r = cellsOf(t);
    if (m_triCells.size() <= t)
        m_triCells.resize(std::size_t(t) + 1);
    m_triCells[t] = r;
    for (std::int32_t i = r[0]; i <= r[3]; ++i)
        for (std::int32_t j = r[1]; j <= r[4]; ++j)
            for (std::int32_t k = r[2]; k <= r[5]; ++k)
                m_cells[cellKey(i, j, k)].push_back(t);
    for (int c = 0; c < 3; ++c) {
        m_cellLo[c] = std::min(m_cellLo[c], r[c]);
        m_cellHi[c] = std::max(m_cellHi[c], r[3 + c]);
    }
}

void Sculptor::gridRemove(std::uint32_t t)
{
    const auto& r = m_triCells[t];
    for (std::int32_t i = r[0]; i <= r[3]; ++i)
        for (std::int32_t j = r[1]; j <= r[4]; ++j)
            for (std::int32_t k = r[2]; k <= r[5]; ++k) {
                const auto it = m_cells.find(cellKey(i, j, k));
                if (it == m_cells.end())
                    continue;
                auto& list = it->second;
                const auto at = std::find(list.begin(), list.end(), t);
                if (at != list.end()) {
                    *at = list.back();
                    list.pop_back();
                }
            }
}

void Sculptor::gridUpdate(const std::vector<std::uint32_t>& points)
{
    // The triangles round moved points: only those whose cells changed move in the grid.
    ++m_walk;
    if (m_walk == 0) {
        std::fill(m_mark.begin(), m_mark.end(), 0);
        m_walk = 1;
    }
    for (std::uint32_t v : points)
        for (std::uint32_t t : m_vt[v]) {
            if (m_triMark.size() <= t)
                m_triMark.resize(std::size_t(t) + 1, 0);
            if (m_triMark[t] == m_walk)
                continue;
            m_triMark[t] = m_walk;
            if (cellsOf(t) != m_triCells[t]) {
                gridRemove(t);
                gridInsert(t);
            }
        }
}

void Sculptor::rebuild()
{
    const std::size_t n = m_mesh.vertexCount();
    if (m_mesh.normals.size() != m_mesh.positions.size())
        allNormals(m_mesh);
    m_vt.assign(n, {});
    for (std::size_t i = 0; i < m_mesh.indices.size(); ++i)
        m_vt[m_mesh.indices[i]].push_back(std::uint32_t(i / 3));
    m_mark.assign(n, 0);
    m_triMark.assign(m_mesh.triangleCount(), 0);
    m_walk = 0;
    // Cells about twice the average edge.
    double edges = 0.0;
    std::size_t count = 0;
    for (std::size_t t = 0; t + 2 < m_mesh.indices.size() && count < 30000; t += 3, ++count)
        edges += len(at(m_mesh.positions, m_mesh.indices[t + 1]) - at(m_mesh.positions, m_mesh.indices[t]));
    m_cell = std::max(count ? 2.0 * edges / double(count) : 1.0, 1e-6);
    m_cells.clear();
    m_triCells.clear();
    m_cellLo = {INT32_MAX, INT32_MAX, INT32_MAX};
    m_cellHi = {INT32_MIN, INT32_MIN, INT32_MIN};
    for (std::uint32_t t = 0; t < m_mesh.triangleCount(); ++t)
        gridInsert(t);
    ++m_topology;
}

Sculptor::Sculptor(Mesh mesh) : m_mesh(std::move(mesh))
{
    rebuild();
}

void Sculptor::setMesh(Mesh mesh)
{
    m_mesh = std::move(mesh);
    m_grab.clear();
    rebuild();
}

void Sculptor::setDetail(double finest, std::size_t maxTriangles)
{
    m_detail = finest;
    m_maxTriangles = maxTriangles;
}

std::optional<Hit> Sculptor::hit(const Vec3& origin, const Vec3& direction) const
{
    const V3 o = toV(origin);
    V3 d = toV(direction);
    const double dl = len(d);
    if (!(dl > 0) || m_cells.empty())
        return std::nullopt;
    d = d * (1.0 / dl);
    // Into the box the grid covers.
    double t0 = 0.0, t1 = 1e300;
    const double oc[3] = {o.x, o.y, o.z}, dc[3] = {d.x, d.y, d.z};
    for (int c = 0; c < 3; ++c) {
        const double lo = m_cellLo[c] * m_cell, hi = (double(m_cellHi[c]) + 1.0) * m_cell;
        if (std::abs(dc[c]) < 1e-15) {
            if (oc[c] < lo || oc[c] > hi)
                return std::nullopt;
            continue;
        }
        double a = (lo - oc[c]) / dc[c], b = (hi - oc[c]) / dc[c];
        if (a > b)
            std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
    }
    if (t0 > t1)
        return std::nullopt;
    // Cell by cell along the ray (Amanatides & Woo), testing the triangles in each.
    const double start = t0 + 1e-9;
    std::int64_t cell[3], step[3];
    double next[3], delta[3];
    for (int c = 0; c < 3; ++c) {
        const double x = oc[c] + dc[c] * start;
        cell[c] = std::clamp<std::int64_t>(std::int64_t(std::floor(x / m_cell)), m_cellLo[c], m_cellHi[c]);
        if (dc[c] > 0) {
            step[c] = 1;
            next[c] = ((double(cell[c]) + 1.0) * m_cell - oc[c]) / dc[c];
            delta[c] = m_cell / dc[c];
        } else if (dc[c] < 0) {
            step[c] = -1;
            next[c] = (double(cell[c]) * m_cell - oc[c]) / dc[c];
            delta[c] = -m_cell / dc[c];
        } else {
            step[c] = 0;
            next[c] = 1e300;
            delta[c] = 1e300;
        }
    }
    double best = 1e300;
    std::optional<Hit> out;
    const auto& p = m_mesh.positions;
    for (int guard = 0; guard < 1'000'000; ++guard) {
        const auto found = m_cells.find(cellKey(cell[0], cell[1], cell[2]));
        if (found != m_cells.end())
            for (std::uint32_t t : found->second) {
                const V3 a = at(p, m_mesh.indices[3 * std::size_t(t)]), b = at(p, m_mesh.indices[3 * std::size_t(t) + 1]),
                         c = at(p, m_mesh.indices[3 * std::size_t(t) + 2]);
                const V3 e1 = b - a, e2 = c - a, h = cross(d, e2);
                const double det = dot(e1, h);
                if (std::abs(det) < 1e-14)
                    continue;
                const double f = 1.0 / det;
                const V3 sv = o - a;
                const double u = f * dot(sv, h);
                if (u < 0.0 || u > 1.0)
                    continue;
                const V3 q = cross(sv, e1);
                const double w = f * dot(d, q);
                if (w < 0.0 || u + w > 1.0)
                    continue;
                const double dist = f * dot(e2, q);
                if (dist > 1e-9 && dist < best) {
                    best = dist;
                    const V3 n = cross(e1, e2);
                    const double l = len(n);
                    const V3 pt = o + d * dist;
                    out = Hit{{pt.x, pt.y, pt.z}, {n.x / l, n.y / l, n.z / l}, t};
                }
            }
        // On to the next cell, unless what's found is nearer than it.
        int axis = 0;
        if (next[1] < next[axis])
            axis = 1;
        if (next[2] < next[axis])
            axis = 2;
        if (best <= next[axis] || next[axis] > t1)
            break;
        cell[axis] += step[axis];
        next[axis] += delta[axis];
        if (cell[axis] < m_cellLo[axis] || cell[axis] > m_cellHi[axis])
            break;
    }
    return out;
}

std::vector<std::uint32_t> Sculptor::pointsWithin(const Vec3& centre, double radius, std::uint32_t nearTriangle)
{
    std::vector<std::uint32_t> seeds;
    for (int k = 0; k < 3 && 3 * std::size_t(nearTriangle) + 2 < m_mesh.indices.size(); ++k)
        seeds.push_back(m_mesh.indices[3 * std::size_t(nearTriangle) + k]);
    return pointsFrom(centre, radius, seeds);
}

std::vector<std::uint32_t> Sculptor::pointsFrom(const Vec3& centre, double radius,
                                                const std::vector<std::uint32_t>& seeds)
{
    // A walk out from the points under the brush, through neighbours, keeping to the
    // ball: only the surface the brush touches, not a part that just passes near.
    ++m_walk;
    if (m_walk == 0) {
        std::fill(m_mark.begin(), m_mark.end(), 0);
        std::fill(m_triMark.begin(), m_triMark.end(), 0);
        m_walk = 1;
    }
    const V3 c = toV(centre);
    const double r2 = radius * radius;
    std::vector<std::uint32_t> out, todo;
    for (std::uint32_t v : seeds)
        if (v < m_mark.size() && m_mark[v] != m_walk) {
            m_mark[v] = m_walk;
            todo.push_back(v);
        }
    while (!todo.empty()) {
        const std::uint32_t v = todo.back();
        todo.pop_back();
        const V3 d = at(m_mesh.positions, v) - c;
        if (dot(d, d) > r2)
            continue;
        out.push_back(v);
        for (std::uint32_t t : m_vt[v])
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t w = m_mesh.indices[3 * std::size_t(t) + k];
                if (m_mark[w] != m_walk) {
                    m_mark[w] = m_walk;
                    todo.push_back(w);
                }
            }
    }
    return out;
}

void Sculptor::normalsOf(const std::vector<std::uint32_t>& points)
{
    auto& p = m_mesh.positions;
    auto& nrm = m_mesh.normals;
    for (std::uint32_t v : points) {
        V3 sum{0, 0, 0};
        for (std::uint32_t t3 : m_vt[v]) {
            const std::size_t t = 3 * std::size_t(t3);
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

std::pair<std::uint32_t, std::uint32_t> Sculptor::normalsAround(const std::vector<std::uint32_t>& moved)
{
    // The moved points and their neighbours (whose triangles moved too).
    std::vector<std::uint32_t> around = moved;
    for (std::uint32_t v : moved)
        for (std::uint32_t t : m_vt[v])
            for (int k = 0; k < 3; ++k)
                around.push_back(m_mesh.indices[3 * std::size_t(t) + k]);
    std::sort(around.begin(), around.end());
    around.erase(std::unique(around.begin(), around.end()), around.end());
    normalsOf(around);
    gridUpdate(moved);
    if (around.empty())
        return {0, 0};
    return {around.front(), around.back() + 1};
}

std::uint32_t Sculptor::nearestPoint(const Vec3& point) const
{
    const V3 c = toV(point);
    double best = 1e300;
    std::uint32_t found = 0;
    for (std::uint32_t v = 0; v < m_mesh.vertexCount(); ++v) {
        const V3 d = at(m_mesh.positions, v) - c;
        const double d2 = dot(d, d);
        if (d2 < best) {
            best = d2;
            found = v;
        }
    }
    return found;
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::apply(const Dab& dab, std::uint32_t nearTriangle)
{
    if (3 * std::size_t(nearTriangle) + 2 >= m_mesh.indices.size())
        return {0, 0};
    std::vector<std::uint32_t> seeds;
    for (int k = 0; k < 3; ++k)
        seeds.push_back(m_mesh.indices[3 * std::size_t(nearTriangle) + k]);
    return applyFrom(dab, seeds);
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::apply(const Dab& dab, std::uint32_t nearTriangle,
                                                        const Symmetry& symmetry)
{
    auto range = apply(dab, nearTriangle);
    if (!symmetry.any())
        return range;
    const auto images = symmetry.images(dab.centre);
    for (std::size_t i = 1; i < images.size(); ++i) {
        Dab mirrored = dab;
        mirrored.centre = images[i];
        const auto r = applyFrom(mirrored, {nearestPoint(images[i])});
        if (r.first < r.second) {
            range.first = range.first < range.second ? std::min(range.first, r.first) : r.first;
            range.second = std::max(range.second, r.second);
        }
    }
    return range;
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::applyFrom(const Dab& dab, const std::vector<std::uint32_t>& seeds)
{
    if (!(dab.radius > 0.0) || dab.brush == Brush::Grab)
        return {0, 0};
    std::vector<std::uint32_t> touched = pointsFrom(dab.centre, dab.radius, seeds);
    if (touched.empty())
        return {0, 0};
    // Detail where the brush is: first split long edges under it, so the shape it makes
    // has the triangles to show it.
    std::uint32_t grownFrom = UINT32_MAX;
    if (m_detail > 0.0 && m_mesh.triangleCount() < m_maxTriangles) {
        const std::size_t before = m_mesh.vertexCount();
        if (splitAround(touched, std::max(m_detail, dab.radius * 0.08))) {
            grownFrom = std::uint32_t(before);
            touched = pointsFrom(dab.centre, dab.radius, touched);
            if (touched.empty())
                return {0, 0};
        }
    }
    const V3 c = toV(dab.centre);
    auto& p = m_mesh.positions;
    const double strength = std::clamp(dab.strength, 0.0, 1.0);
    const double sign = dab.invert ? -1.0 : 1.0;
    const double most = 0.08 * dab.radius * strength;  // how far one dab moves the middle
    const auto normalOf = [&](std::uint32_t v) {
        return V3{m_mesh.normals[3 * v], m_mesh.normals[3 * v + 1], m_mesh.normals[3 * v + 2]};
    };
    // The area's average facing and middle, weighted by the falloff.
    V3 areaNormal{0, 0, 0}, areaMiddle{0, 0, 0};
    double weights = 0.0;
    for (std::uint32_t v : touched) {
        const double w = falloff(len(at(p, v) - c), dab.radius);
        areaNormal = areaNormal + normalOf(v) * w;
        areaMiddle = areaMiddle + at(p, v) * w;
        weights += w;
    }
    const double nl = len(areaNormal);
    if (!(nl > 0) || !(weights > 0))
        return {0, 0};
    areaNormal = areaNormal * (1.0 / nl);
    areaMiddle = areaMiddle * (1.0 / weights);

    // The middle of a point's neighbours (each counted once per triangle it shares, which
    // weights them evenly on a closed surface).
    const auto neighbourMiddle = [&](std::uint32_t v, const std::function<V3(std::uint32_t)>& pos) {
        V3 sum{0, 0, 0};
        int count = 0;
        for (std::uint32_t t : m_vt[v])
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t w = m_mesh.indices[3 * std::size_t(t) + k];
                if (w != v) {
                    sum = sum + pos(w);
                    ++count;
                }
            }
        return count ? sum * (1.0 / count) : pos(v);
    };

    std::vector<V3> moved(touched.size());
    switch (dab.brush) {
    case Brush::Draw:
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const V3 q = at(p, touched[i]);
            moved[i] = q + areaNormal * (sign * most * falloff(len(q - c), dab.radius));
        }
        break;
    case Brush::Inflate:
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const V3 q = at(p, touched[i]);
            moved[i] = q + normalOf(touched[i]) * (sign * most * falloff(len(q - c), dab.radius));
        }
        break;
    case Brush::Flatten:
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const V3 q = at(p, touched[i]);
            const double above = dot(q - areaMiddle, areaNormal);
            moved[i] = q - areaNormal * (above * 0.5 * strength * falloff(len(q - c), dab.radius));
        }
        break;
    case Brush::Clay: {
        // A plane a little above (below, inverted) the area: points under it rise to it.
        const V3 plane = areaMiddle + areaNormal * (sign * most);
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const V3 q = at(p, touched[i]);
            const double under = dot(plane - q, areaNormal) * sign;
            moved[i] = under > 0 ? q + areaNormal * (sign * under * strength * falloff(len(q - c), dab.radius)) : q;
        }
        break;
    }
    case Brush::Pinch:
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const V3 q = at(p, touched[i]);
            V3 toMiddle = c - q;
            toMiddle = toMiddle - areaNormal * dot(toMiddle, areaNormal);  // along the surface
            moved[i] = q + toMiddle * (sign * 0.15 * strength * falloff(len(q - c), dab.radius));
        }
        break;
    case Brush::Smooth: {
        // Each point part of the way to the middle of its neighbours, a few times over
        // (one step only evens out the finest ripples on a dense mesh).
        const auto current = [&](std::uint32_t w) { return at(p, w); };
        for (int round = 0; round < 4; ++round) {
            for (std::size_t i = 0; i < touched.size(); ++i) {
                const std::uint32_t v = touched[i];
                const V3 q = at(p, v);
                moved[i] = q + (neighbourMiddle(v, current) - q) * (strength * falloff(len(q - c), dab.radius));
            }
            for (std::size_t i = 0; i < touched.size(); ++i) {
                const std::uint32_t v = touched[i];
                p[3 * v] = float(moved[i].x);
                p[3 * v + 1] = float(moved[i].y);
                p[3 * v + 2] = float(moved[i].z);
            }
        }
        break;
    }
    case Brush::Grab:
        return {0, 0};
    }
    for (std::size_t i = 0; i < touched.size(); ++i) {
        const std::uint32_t v = touched[i];
        p[3 * v] = float(moved[i].x);
        p[3 * v + 1] = float(moved[i].y);
        p[3 * v + 2] = float(moved[i].z);
    }
    auto range = normalsAround(touched);
    if (grownFrom != UINT32_MAX) {
        range.first = std::min(range.first, grownFrom);
        range.second = std::uint32_t(m_mesh.vertexCount());
    }
    return range;
}

bool Sculptor::splitAround(const std::vector<std::uint32_t>& points, double maxEdge)
{
    const double limit2 = maxEdge * maxEdge;
    auto& pos = m_mesh.positions;
    auto& idx = m_mesh.indices;
    bool any = false;
    std::vector<std::uint32_t> region = points;
    for (int pass = 0; pass < 3 && m_mesh.triangleCount() < m_maxTriangles; ++pass) {
        // Long edges of the triangles round the region's points: each gets a middle point.
        std::unordered_map<std::uint64_t, std::uint32_t> middle;
        std::vector<std::uint32_t> candidates;
        for (std::uint32_t v : region)
            for (std::uint32_t t : m_vt[v])
                candidates.push_back(t);
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        for (std::uint32_t t : candidates)
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = idx[3 * std::size_t(t) + k], b = idx[3 * std::size_t(t) + (k + 1) % 3];
                const V3 d = at(pos, b) - at(pos, a);
                if (dot(d, d) <= limit2 || middle.count(edgeKey(a, b)))
                    continue;
                const auto m = std::uint32_t(pos.size() / 3);
                const V3 c = (at(pos, a) + at(pos, b)) * 0.5;
                pos.insert(pos.end(), {float(c.x), float(c.y), float(c.z)});
                const V3 n = V3{m_mesh.normals[3 * a], m_mesh.normals[3 * a + 1], m_mesh.normals[3 * a + 2]} +
                             V3{m_mesh.normals[3 * b], m_mesh.normals[3 * b + 1], m_mesh.normals[3 * b + 2]};
                const double nl = len(n);
                m_mesh.normals.insert(m_mesh.normals.end(),
                                      {float(nl > 0 ? n.x / nl : 0), float(nl > 0 ? n.y / nl : 0),
                                       float(nl > 0 ? n.z / nl : 1)});
                m_vt.emplace_back();
                m_mark.push_back(0);
                middle.emplace(edgeKey(a, b), m);
            }
        if (middle.empty())
            break;
        any = true;
        // Every triangle on a split edge (some just outside the region) is divided, so
        // the surface stays closed.
        std::vector<std::uint32_t> affected;
        for (const auto& [key, m] : middle) {
            const auto a = std::uint32_t(key >> 32), b = std::uint32_t(key & 0xffffffffu);
            for (std::uint32_t t : m_vt[a]) {
                const std::uint32_t* v = &idx[3 * std::size_t(t)];
                if (v[0] == b || v[1] == b || v[2] == b)
                    affected.push_back(t);
            }
        }
        std::sort(affected.begin(), affected.end());
        affected.erase(std::unique(affected.begin(), affected.end()), affected.end());
        std::vector<std::uint32_t> next;  // the corners of the new triangles
        for (std::uint32_t t : affected) {
            const std::uint32_t v[3] = {idx[3 * std::size_t(t)], idx[3 * std::size_t(t) + 1], idx[3 * std::size_t(t) + 2]};
            std::int64_t e[3];
            for (int k = 0; k < 3; ++k) {
                const auto f = middle.find(edgeKey(v[k], v[(k + 1) % 3]));
                e[k] = f == middle.end() ? -1 : std::int64_t(f->second);
            }
            const int splits = (e[0] >= 0) + (e[1] >= 0) + (e[2] >= 0);
            if (splits == 0)
                continue;
            std::vector<std::uint32_t> kids;
            if (splits == 3) {
                const auto a = std::uint32_t(e[0]), b = std::uint32_t(e[1]), c = std::uint32_t(e[2]);
                kids = {v[0], a, c, a, v[1], b, c, b, v[2], a, b, c};
            } else {
                int r = 0;
                while (e[r] < 0)
                    ++r;
                if (splits == 2 && e[(r + 2) % 3] >= 0 && r == 0)
                    r = 2;
                const std::uint32_t a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3];
                const auto m0 = std::uint32_t(e[r]);
                if (splits == 1)
                    kids = {a, m0, c, m0, b, c};
                else
                    kids = {m0, b, std::uint32_t(e[(r + 1) % 3]), a, m0, std::uint32_t(e[(r + 1) % 3]), a,
                            std::uint32_t(e[(r + 1) % 3]), c};
            }
            // Out of the lists and the grid as it was...
            gridRemove(t);
            for (std::uint32_t corner : v) {
                auto& list = m_vt[corner];
                const auto it = std::find(list.begin(), list.end(), t);
                if (it != list.end()) {
                    *it = list.back();
                    list.pop_back();
                }
            }
            // ...and back as its pieces: the first in its place, the rest at the end.
            for (std::size_t k = 0; k < kids.size(); k += 3) {
                const std::uint32_t id = k == 0 ? t : std::uint32_t(idx.size() / 3);
                if (k == 0)
                    std::copy(kids.begin(), kids.begin() + 3, idx.begin() + 3 * std::size_t(t));
                else
                    idx.insert(idx.end(), kids.begin() + std::ptrdiff_t(k), kids.begin() + std::ptrdiff_t(k + 3));
                for (int j = 0; j < 3; ++j) {
                    m_vt[kids[k + j]].push_back(id);
                    next.push_back(kids[k + j]);
                }
                gridInsert(id);
            }
        }
        if (m_triMark.size() < m_mesh.triangleCount())
            m_triMark.resize(m_mesh.triangleCount(), 0);
        std::sort(next.begin(), next.end());
        next.erase(std::unique(next.begin(), next.end()), next.end());
        normalsOf(next);
        region = std::move(next);
    }
    if (any)
        ++m_topology;
    return any;
}

void Sculptor::beginGrab(const Vec3& centre, double radius, std::uint32_t nearTriangle, const Symmetry& symmetry)
{
    m_grab.clear();
    m_grabSymmetry = symmetry;
    if (!(radius > 0.0))
        return;
    const auto images = symmetry.images(centre);
    std::vector<char> taken(m_mesh.vertexCount(), 0);
    for (std::size_t i = 0; i < images.size(); ++i) {
        const V3 c = toV(images[i]);
        const auto points = i == 0 ? pointsWithin(images[i], radius, nearTriangle)
                                   : pointsFrom(images[i], radius, {nearestPoint(images[i])});
        for (std::uint32_t v : points) {
            if (taken[v])
                continue;  // where the images overlap, the first one takes it
            taken[v] = 1;
            const auto& q = m_mesh.positions;
            m_grab.push_back({v, float(falloff(len(at(q, v) - c), radius)),
                              {q[3 * v], q[3 * v + 1], q[3 * v + 2]}, std::uint8_t(i)});
        }
    }
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::moveGrab(const Vec3& offset)
{
    if (m_grab.empty())
        return {0, 0};
    std::vector<Vec3> offsets;
    for (std::size_t i = 0; i < 8; ++i)
        offsets.push_back(m_grabSymmetry.mirrored(offset, i));
    std::vector<std::uint32_t> points;
    points.reserve(m_grab.size());
    for (const Grabbed& g : m_grab) {
        const Vec3& o = offsets[g.image];
        for (int k = 0; k < 3; ++k)
            m_mesh.positions[3 * g.point + k] = float(g.from[k] + o[k] * g.weight);
        points.push_back(g.point);
    }
    return normalsAround(points);
}

void Sculptor::endGrab()
{
    m_grab.clear();
}

std::vector<Vec3> Symmetry::images(const Vec3& p) const
{
    std::vector<Vec3> out{p};
    for (int axis = 0; axis < 3; ++axis) {
        const bool on = axis == 0 ? x : axis == 1 ? y : z;
        if (!on)
            continue;
        const std::size_t n = out.size();
        for (std::size_t i = 0; i < n; ++i) {
            Vec3 q = out[i];
            q[axis] = 2.0 * centre[axis] - q[axis];
            out.push_back(q);
        }
    }
    return out;
}

Vec3 Symmetry::mirrored(const Vec3& v, std::size_t image) const
{
    // images() doubles the list for each axis that's on, in X, Y, Z order: bit k of the
    // image number says whether the k-th axis that's on flipped it.
    Vec3 out = v;
    int bit = 0;
    for (int axis = 0; axis < 3; ++axis) {
        const bool on = axis == 0 ? x : axis == 1 ? y : z;
        if (!on)
            continue;
        if (image & (std::size_t(1) << bit))
            out[axis] = -out[axis];
        ++bit;
    }
    return out;
}

void Sculptor::setPositions(const std::vector<float>& positions)
{
    if (positions.size() != m_mesh.positions.size())
        throw std::invalid_argument("setPositions: a different number of points");
    m_mesh.positions = positions;
    allNormals(m_mesh);
    rebuild();
}

}  // namespace forge::sculpt
