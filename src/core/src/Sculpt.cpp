// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Sculpt.h"

#include "forge/core/FileFormat.h"
#include "forge/core/Remesh.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <functional>
#include <thread>
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
            // Left where it's filed until settle(); found meanwhile through m_stale.
            if (m_isStale.size() <= t)
                m_isStale.resize(std::size_t(t) + 1, 0);
            if (!m_isStale[t]) {
                m_isStale[t] = 1;
                m_stale.push_back(t);
            }
        }
    // Testing them all on every hit would start to tell: past a point the oldest are
    // filed again, a share on each dab (all at once was a hitch in a big stroke).
    if (m_stale.size() > 100'000)
        settle(m_stale.size() / 4);
}

void Sculptor::settle(std::size_t most)
{
    const std::size_t n = std::min(most, m_stale.size());
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t t = m_stale[i];
        m_isStale[t] = 0;
        if (std::size_t(t) < m_triCells.size() && cellsOf(t) != m_triCells[t]) {
            gridRemove(t);
            gridInsert(t);
        }
    }
    m_stale.erase(m_stale.begin(), m_stale.begin() + std::ptrdiff_t(n));
}

void Sculptor::rebuild()
{
    m_everything = true;
    m_stale.clear();
    m_isStale.clear();
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

namespace {

// Runs work(begin, end) over [0, n) in pieces on several threads when n is big enough to
// be worth it (a big brush on a dense mesh), else on this one.
void parallelFor(std::size_t n, const std::function<void(std::size_t, std::size_t)>& work)
{
    const unsigned cores = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    if (n < 4096 || cores == 1) {
        work(0, n);
        return;
    }
    const std::size_t pieces = std::min<std::size_t>(cores, n / 2048);
    std::vector<std::thread> threads;
    threads.reserve(pieces - 1);
    const std::size_t step = (n + pieces - 1) / pieces;
    for (std::size_t i = 1; i < pieces; ++i) {
        const std::size_t from = i * step, to = std::min(n, from + step);
        if (from < to)
            threads.emplace_back(work, from, to);
    }
    work(0, std::min(n, step));
    for (std::thread& t : threads)
        t.join();
}

// Spreads the low 21 bits of v out to every third bit.
std::uint64_t spread3(std::uint64_t v)
{
    v &= 0x1FFFFF;
    v = (v | v << 32) & 0x1F00000000FFFFULL;
    v = (v | v << 16) & 0x1F0000FF0000FFULL;
    v = (v | v << 8) & 0x100F00F00F00F00FULL;
    v = (v | v << 4) & 0x10C30C30C30C30C3ULL;
    v = (v | v << 2) & 0x1249249249249249ULL;
    return v;
}

}  // namespace

void spatialOrder(Mesh& mesh)
{
    const std::size_t n = mesh.vertexCount();
    if (n < 2)
        return;
    // Points that are near each other get numbers near each other (a Z-order curve), and
    // so do triangles: a brush then works on memory that's close together, and the
    // points it changes go to the graphics card in a few short runs, not one spanning
    // the whole mesh.
    const Bounds box = mesh.bounds();
    const double sx = box.max[0] - box.min[0], sy = box.max[1] - box.min[1], sz = box.max[2] - box.min[2];
    const double scale = 2097151.0 / std::max({sx, sy, sz, 1e-9});
    std::vector<std::pair<std::uint64_t, std::uint32_t>> keyed(n);
    for (std::size_t v = 0; v < n; ++v) {
        const auto q = [&](int k) {
            return std::uint64_t(std::clamp((mesh.positions[3 * v + k] - box.min[std::size_t(k)]) * scale, 0.0, 2097151.0));
        };
        keyed[v] = {spread3(q(0)) | spread3(q(1)) << 1 | spread3(q(2)) << 2, std::uint32_t(v)};
    }
    std::sort(keyed.begin(), keyed.end());
    std::vector<std::uint32_t> newIndex(n);
    for (std::size_t i = 0; i < n; ++i)
        newIndex[keyed[i].second] = std::uint32_t(i);
    const auto reorder = [&](std::vector<float>& values, std::size_t width) {
        if (values.size() != n * width)
            return;
        std::vector<float> out(values.size());
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t k = 0; k < width; ++k)
                out[i * width + k] = values[std::size_t(keyed[i].second) * width + k];
        values.swap(out);
    };
    reorder(mesh.positions, 3);
    reorder(mesh.normals, 3);
    reorder(mesh.shade, 1);
    // Triangles by their first (lowest) point, keeping each one's corners in order.
    const std::size_t tris = mesh.indices.size() / 3;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> byFirst(tris);
    for (std::size_t t = 0; t < tris; ++t) {
        for (int k = 0; k < 3; ++k)
            mesh.indices[3 * t + k] = newIndex[mesh.indices[3 * t + k]];
        byFirst[t] = {std::min({mesh.indices[3 * t], mesh.indices[3 * t + 1], mesh.indices[3 * t + 2]}),
                      std::uint32_t(t)};
    }
    std::sort(byFirst.begin(), byFirst.end());
    std::vector<std::uint32_t> indices(mesh.indices.size());
    for (std::size_t i = 0; i < tris; ++i)
        for (int k = 0; k < 3; ++k)
            indices[3 * i + k] = mesh.indices[3 * std::size_t(byFirst[i].second) + k];
    mesh.indices.swap(indices);
}

Sculptor::Sculptor(Mesh mesh) : m_mesh(std::move(mesh))
{
    spatialOrder(m_mesh);
    m_rest = m_mesh.positions;
    rebuild();
}

void Sculptor::setMesh(Mesh mesh)
{
    m_mesh = std::move(mesh);
    m_grab.clear();
    spatialOrder(m_mesh);
    m_rest = m_mesh.positions;  // a new surface (a remesh keeps what was sculpted before it)
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
    double best = 1e300;
    std::optional<Hit> out;
    const auto& p = m_mesh.positions;
    const auto test = [&](std::uint32_t t) {
        const V3 a = at(p, m_mesh.indices[3 * std::size_t(t)]), b = at(p, m_mesh.indices[3 * std::size_t(t) + 1]),
                 c = at(p, m_mesh.indices[3 * std::size_t(t) + 2]);
        const V3 e1 = b - a, e2 = c - a, h = cross(d, e2);
        const double det = dot(e1, h);
        if (std::abs(det) < 1e-14)
            return;
        const double f = 1.0 / det;
        const V3 sv = o - a;
        const double u = f * dot(sv, h);
        if (u < 0.0 || u > 1.0)
            return;
        const V3 q = cross(sv, e1);
        const double w = f * dot(d, q);
        if (w < 0.0 || u + w > 1.0)
            return;
        const double dist = f * dot(e2, q);
        if (dist > 1e-9 && dist < best) {
            best = dist;
            const V3 n = cross(e1, e2);
            const double l = len(n);
            const V3 pt = o + d * dist;
            out = Hit{{pt.x, pt.y, pt.z}, {n.x / l, n.y / l, n.z / l}, t};
        }
    };
    // The triangles moved since they were filed in the grid: each tested directly.
    for (std::uint32_t t : m_stale)
        test(t);
    // Into the box the grid covers.
    double t0 = 0.0, t1 = 1e300;
    const double oc[3] = {o.x, o.y, o.z}, dc[3] = {d.x, d.y, d.z};
    for (int c = 0; c < 3; ++c) {
        const double lo = m_cellLo[c] * m_cell, hi = (double(m_cellHi[c]) + 1.0) * m_cell;
        if (std::abs(dc[c]) < 1e-15) {
            if (oc[c] < lo || oc[c] > hi)
                return out;
            continue;
        }
        double a = (lo - oc[c]) / dc[c], b = (hi - oc[c]) / dc[c];
        if (a > b)
            std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
    }
    if (t0 > t1)
        return out;
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
    for (int guard = 0; guard < 1'000'000; ++guard) {
        const auto found = m_cells.find(cellKey(cell[0], cell[1], cell[2]));
        if (found != m_cells.end())
            for (std::uint32_t t : found->second)
                test(t);
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
    parallelFor(points.size(), [&](std::size_t from, std::size_t to) {
    for (std::size_t i = from; i < to; ++i) {
        const std::uint32_t v = points[i];
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
    });
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::normalsAround(const std::vector<std::uint32_t>& moved)
{
    // The moved points and their neighbours (whose triangles moved too), each once:
    // marked as they're found (sorting them all took longer than the brush itself).
    ++m_walk;
    if (m_walk == 0) {
        std::fill(m_mark.begin(), m_mark.end(), 0);
        std::fill(m_triMark.begin(), m_triMark.end(), 0);
        m_walk = 1;
    }
    std::vector<std::uint32_t> around;
    around.reserve(moved.size() * 2);
    std::uint32_t lo = UINT32_MAX, hi = 0;
    const auto add = [&](std::uint32_t v) {
        if (m_mark[v] == m_walk)
            return;
        m_mark[v] = m_walk;
        around.push_back(v);
        lo = std::min(lo, v);
        hi = std::max(hi, v + 1);
    };
    for (std::uint32_t v : moved) {
        add(v);
        for (std::uint32_t t : m_vt[v])
            for (int k = 0; k < 3; ++k)
                add(m_mesh.indices[3 * std::size_t(t) + k]);
    }
    normalsOf(around);
    gridUpdate(moved);
    noteChanged(around);
    if (around.empty())
        return {0, 0};
    return {lo, hi};
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
    m_dabSymmetry = symmetry;
    m_dabImage = 0;
    auto range = apply(dab, nearTriangle);
    if (!symmetry.any()) {
        m_dabSymmetry = {};
        return range;
    }
    const auto images = symmetry.images(dab.centre);
    for (std::size_t i = 1; i < images.size(); ++i) {
        Dab mirrored = dab;
        mirrored.centre = images[i];
        m_dabImage = i;
        const auto r = applyFrom(mirrored, {nearestPoint(images[i])});
        if (r.first < r.second) {
            range.first = range.first < range.second ? std::min(range.first, r.first) : r.first;
            range.second = std::max(range.second, r.second);
        }
    }
    m_dabSymmetry = {};
    m_dabImage = 0;
    return range;
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::applyFrom(const Dab& dab, const std::vector<std::uint32_t>& seeds)
{
    if (!(dab.radius > 0.0) || dab.brush == Brush::Grab || dab.brush == Brush::SnakeHook ||
        dab.brush == Brush::MaskClear || dab.brush == Brush::MaskInvert || dab.brush == Brush::Remesh)
        return {0, 0};
    std::vector<std::uint32_t> touched = pointsFrom(dab.centre, dab.radius, seeds);
    if (touched.empty())
        return {0, 0};
    if (dab.brush == Brush::Mask) {
        // Paints the mask only: no point moves, no detail is added.
        if (m_mesh.shade.size() != m_mesh.vertexCount())
            m_mesh.shade.assign(m_mesh.vertexCount(), 0.0f);
        const V3 c = toV(dab.centre);
        const double amount = (dab.invert ? -1.0 : 1.0) * std::clamp(dab.strength, 0.0, 1.0);
        std::uint32_t lo = UINT32_MAX, hi = 0;
        for (std::uint32_t v : touched) {
            const double w = falloff(len(at(m_mesh.positions, v) - c), dab.radius);
            m_mesh.shade[v] = float(std::clamp(m_mesh.shade[v] + amount * std::min(1.0, 2.0 * w), 0.0, 1.0));
            lo = std::min(lo, v);
            hi = std::max(hi, v + 1);
        }
        noteChanged(touched);
        return {lo, hi};
    }
    // Detail where the brush is: first split long edges under it, so the shape it makes
    // has the triangles to show it.
    std::uint32_t grownFrom = UINT32_MAX;
    if (m_detail > 0.0 && m_mesh.triangleCount() < m_maxTriangles) {
        const std::size_t before = m_mesh.vertexCount();
        // A pattern needs triangles a good deal smaller than one repeat of it.
        const double limit = dab.brush == Brush::Pattern && m_pattern
                                 ? std::max(m_detail, std::min(dab.radius * 0.08,
                                                               m_pattern->tile / (m_pattern->once ? 64.0 : 24.0)))
                                 : std::max(m_detail, dab.radius * 0.08);
        if (splitAround(touched, limit)) {
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
        parallelFor(touched.size(), [&](std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                const V3 q = at(p, touched[i]);
                moved[i] = q + areaNormal * (sign * most * falloff(len(q - c), dab.radius));
            }
        });
        break;
    case Brush::Inflate:
        parallelFor(touched.size(), [&](std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                const V3 q = at(p, touched[i]);
                moved[i] = q + normalOf(touched[i]) * (sign * most * falloff(len(q - c), dab.radius));
            }
        });
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
                moved[i] = q + (neighbourMiddle(v, current) - q) *
                                   (strength * falloff(len(q - c), dab.radius) * (1.0 - double(maskAt(v))));
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
    case Brush::Crease:
        // In along the area's facing with a sharp middle, and drawn together towards it:
        // a narrow groove (a ridge, inverted).
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const V3 q = at(p, touched[i]);
            const double f = falloff(len(q - c), dab.radius);
            V3 toMiddle = c - q;
            toMiddle = toMiddle - areaNormal * dot(toMiddle, areaNormal);
            moved[i] = q - areaNormal * (sign * most * f * f) + toMiddle * (0.2 * strength * f);
        }
        break;
    case Brush::Pattern: {
        if (!m_pattern || !m_pattern->image || m_pattern->image->width <= 0 || m_pattern->image->height <= 0)
            return {0, 0};
        PatternLayer& layer = *m_pattern;
        if (!layer.framed) {
            // The pattern lies flat across the first dab's facing, the same way round each
            // time (so strokes that meet line up).
            const V3 n = areaNormal;
            const V3 ref = std::abs(n.z) < 0.9 ? V3{0, 0, 1} : V3{1, 0, 0};
            V3 u0 = cross(n, ref);
            u0 = u0 * (1.0 / len(u0));
            const V3 v0 = cross(n, u0);
            // Turned round the facing.
            const double a = layer.turn * 3.14159265358979 / 180.0;
            const V3 u = u0 * std::cos(a) + v0 * std::sin(a);
            const V3 v = v0 * std::cos(a) - u0 * std::sin(a);
            layer.u = {u.x, u.y, u.z};
            layer.v = {v.x, v.y, v.z};
            // A stamp's middle: the first dab's, seen from its first image.
            const Vec3 middle = m_dabSymmetry.centre;
            const Vec3 back = m_dabSymmetry.mirrored({c.x - middle[0], c.y - middle[1], c.z - middle[2]}, m_dabImage);
            layer.origin = {back[0] + middle[0], back[1] + middle[1], back[2] + middle[2]};
            layer.framed = true;
        }
        const PictureData& image = *layer.image;
        const double tileW = layer.tile, tileH = layer.tile * double(image.height) / double(image.width);
        const auto height = [&](double s, double t) {
            // Bilinear, wrapping round: the picture repeats side by side.
            const double x = (s - std::floor(s)) * image.width - 0.5, y = (t - std::floor(t)) * image.height - 0.5;
            const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
            const double fx = x - x0, fy = y - y0;
            const auto px = [&](int xi, int yi) {
                xi = ((xi % image.width) + image.width) % image.width;
                yi = ((yi % image.height) + image.height) % image.height;
                return image.grey[std::size_t(yi) * image.width + xi] / 255.0;
            };
            return (px(x0, y0) * (1 - fx) + px(x0 + 1, y0) * fx) * (1 - fy) +
                   (px(x0, y0 + 1) * (1 - fx) + px(x0 + 1, y0 + 1) * fx) * fy;
        };
        for (std::size_t i = 0; i < touched.size(); ++i) {
            const std::uint32_t v = touched[i];
            moved[i] = at(p, v);
            auto [it, added] = layer.base.try_emplace(v);
            PatternLayer::Base& base = it->second;
            if (added) {
                base.at = {moved[i].x, moved[i].y, moved[i].z};
                base.facing = {m_mesh.normals[3 * v], m_mesh.normals[3 * v + 1], m_mesh.normals[3 * v + 2]};
            }
            const V3 from{base.at[0], base.at[1], base.at[2]};
            // A stamp's picture makes its own edges: all of it, wherever the brush reaches,
            // on the surface facing the way it's pressed (not round the back of a thin part).
            const V3 facing{base.facing[0], base.facing[1], base.facing[2]};
            const float w = float((layer.once ? (len(from - c) <= dab.radius && dot(facing, areaNormal) > 0.1 ? 1.0 : 0.0)
                                              : falloff(len(from - c), dab.radius)) *
                                  (1.0 - double(maskAt(v))));
            if (!(w > base.done))
                continue;  // had as much already: going over again doesn't press deeper
            base.done = w;
            // Where this point is in the pattern: seen from the first image of the dab
            // (mirror images show the pattern mirrored).
            const Vec3 middle = m_dabSymmetry.centre;
            const Vec3 back = m_dabSymmetry.mirrored({from.x - middle[0], from.y - middle[1], from.z - middle[2]},
                                                     m_dabImage);
            const V3 q{back[0] + middle[0], back[1] + middle[1], back[2] + middle[2]};
            double h = 0.0;
            if (layer.once) {
                // Once, centred: nothing outside the picture.
                const V3 o{layer.origin[0], layer.origin[1], layer.origin[2]};
                const double s = dot(q - o, V3{layer.u[0], layer.u[1], layer.u[2]}) / tileW + 0.5;
                const double t = 0.5 - dot(q - o, V3{layer.v[0], layer.v[1], layer.v[2]}) / tileH;
                if (s >= 0.0 && s <= 1.0 && t >= 0.0 && t <= 1.0)
                    h = height(std::min(s, 0.9999), std::min(t, 0.9999));
            } else {
                h = height(dot(q, V3{layer.u[0], layer.u[1], layer.u[2]}) / tileW,
                           dot(q, V3{layer.v[0], layer.v[1], layer.v[2]}) / tileH);
            }
            moved[i] = from + V3{base.facing[0], base.facing[1], base.facing[2]} * (layer.depth * h * w);
        }
        break;
    }
    case Brush::Grab:
    case Brush::SnakeHook:
    case Brush::Mask:
    case Brush::MaskClear:
    case Brush::MaskInvert:
    case Brush::Remesh:
        return {0, 0};
    }
    if (!m_mesh.shade.empty() && dab.brush != Brush::Smooth && dab.brush != Brush::Pattern)
        for (std::size_t i = 0; i < touched.size(); ++i) {
            // Masked points move less (not at all where fully masked).
            const V3 q = at(p, touched[i]);
            moved[i] = q + (moved[i] - q) * (1.0 - double(m_mesh.shade[touched[i]]));
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
                if (m_rest.size() + 3 == pos.size()) {
                    const V3 r = (at(m_rest, a) + at(m_rest, b)) * 0.5;
                    m_rest.insert(m_rest.end(), {float(r.x), float(r.y), float(r.z)});
                }
                const V3 n = V3{m_mesh.normals[3 * a], m_mesh.normals[3 * a + 1], m_mesh.normals[3 * a + 2]} +
                             V3{m_mesh.normals[3 * b], m_mesh.normals[3 * b + 1], m_mesh.normals[3 * b + 2]};
                const double nl = len(n);
                m_mesh.normals.insert(m_mesh.normals.end(),
                                      {float(nl > 0 ? n.x / nl : 0), float(nl > 0 ? n.y / nl : 0),
                                       float(nl > 0 ? n.z / nl : 1)});
                if (!m_mesh.shade.empty())
                    m_mesh.shade.push_back(0.5f * (m_mesh.shade[a] + m_mesh.shade[b]));
                if (m_pattern) {
                    // Under a pattern: the new point's place before it, between its ends'.
                    const auto fa = m_pattern->base.find(a), fb = m_pattern->base.find(b);
                    if (fa != m_pattern->base.end() && fb != m_pattern->base.end()) {
                        PatternLayer::Base mid;
                        V3 f{0, 0, 0};
                        for (int k = 0; k < 3; ++k) {
                            mid.at[k] = 0.5 * (fa->second.at[k] + fb->second.at[k]);
                            mid.facing[k] = fa->second.facing[k] + fb->second.facing[k];
                        }
                        f = {mid.facing[0], mid.facing[1], mid.facing[2]};
                        const double fl = len(f);
                        if (fl > 0)
                            for (int k = 0; k < 3; ++k)
                                mid.facing[k] /= fl;
                        mid.done = std::min(fa->second.done, fb->second.done);
                        m_pattern->base.emplace(m, mid);
                    }
                }
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
                noteTriangle(id);
            }
        }
        if (m_triMark.size() < m_mesh.triangleCount())
            m_triMark.resize(m_mesh.triangleCount(), 0);
        std::sort(next.begin(), next.end());
        next.erase(std::unique(next.begin(), next.end()), next.end());
        normalsOf(next);
        noteChanged(next);
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
            m_grab.push_back({v, float(falloff(len(at(q, v) - c), radius) * (1.0f - maskAt(v))),
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

void Sculptor::beginSnake(const Vec3& centre, double radius, double strength, std::uint32_t nearTriangle,
                          const Symmetry& symmetry)
{
    m_snake.clear();
    m_snakeSymmetry = symmetry;
    m_snakeRadius = radius;
    // How closely the tip follows the mouse: all the way at full strength.
    m_snakeFollow = 0.5 + 0.5 * std::clamp(strength, 0.0, 1.0);
    if (!(radius > 0.0) || 3 * std::size_t(nearTriangle) + 2 >= m_mesh.indices.size())
        return;
    const auto images = symmetry.images(centre);
    for (std::size_t i = 0; i < images.size(); ++i) {
        SnakeImage image;
        image.tip = images[i];
        if (i == 0)
            for (int k = 0; k < 3; ++k)
                image.seeds.push_back(m_mesh.indices[3 * std::size_t(nearTriangle) + k]);
        else
            image.seeds = {nearestPoint(images[i])};
        m_snake.push_back(std::move(image));
    }
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::moveSnake(const Vec3& to)
{
    if (m_snake.empty())
        return {0, 0};
    const Vec3 delta{to[0] - m_snake[0].tip[0], to[1] - m_snake[0].tip[1], to[2] - m_snake[0].tip[2]};
    const std::size_t before = m_mesh.vertexCount();
    bool grew = false;
    std::vector<std::uint32_t> movedAll;
    std::vector<char> taken;
    for (std::size_t i = 0; i < m_snake.size(); ++i) {
        SnakeImage& image = m_snake[i];
        std::vector<std::uint32_t> touched = pointsFrom(image.tip, m_snakeRadius, image.seeds);
        if (touched.empty())
            continue;
        // Stretched edges get split, so the pulled-out part stays smooth.
        if (m_detail > 0.0 && m_mesh.triangleCount() < m_maxTriangles &&
            splitAround(touched, std::max(m_detail, m_snakeRadius * 0.08))) {
            grew = true;
            touched = pointsFrom(image.tip, m_snakeRadius, touched);
        }
        if (taken.size() < m_mesh.vertexCount())
            taken.resize(m_mesh.vertexCount(), 0);
        const Vec3 d = m_snakeSymmetry.mirrored(delta, i);
        const V3 tip = toV(image.tip);
        std::vector<std::uint32_t> mine;
        for (std::uint32_t v : touched) {
            if (taken[v])
                continue;  // where the images overlap, the first one takes it
            taken[v] = 1;
            const double w = falloff(len(at(m_mesh.positions, v) - tip), m_snakeRadius) * m_snakeFollow *
                             (1.0 - double(maskAt(v)));
            for (int k = 0; k < 3; ++k)
                m_mesh.positions[3 * std::size_t(v) + k] += float(d[k] * w);
            mine.push_back(v);
        }
        for (int k = 0; k < 3; ++k)
            image.tip[k] += d[k] * m_snakeFollow;
        image.seeds = touched;
        movedAll.insert(movedAll.end(), mine.begin(), mine.end());
    }
    auto range = normalsAround(movedAll);
    if (grew) {
        range.first = std::min(range.first, std::uint32_t(before));
        range.second = std::uint32_t(m_mesh.vertexCount());
    }
    return range;
}

void Sculptor::endSnake()
{
    m_snake.clear();
}

static_assert(int(Brush::Pattern) == file::kLastSculptBrush, "files must be able to hold every brush");

void Sculptor::beginPattern(std::shared_ptr<const PictureData> image, double tile, double depth, double turn,
                            bool once)
{
    m_pattern.emplace();
    m_pattern->image = std::move(image);
    m_pattern->tile = std::max(tile, 1e-3);
    m_pattern->depth = depth;
    m_pattern->turn = turn;
    m_pattern->once = once;
}

void Sculptor::endPattern()
{
    m_pattern.reset();
}

void Sculptor::remesh(double edge)
{
    m_grab.clear();
    m_snake.clear();
    setMesh(remesh::remeshed(m_mesh, edge));
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::clearMask()
{
    if (m_mesh.shade.empty())
        return {0, 0};
    m_mesh.shade.clear();
    m_everything = true;
    return {0, std::uint32_t(m_mesh.vertexCount())};
}

std::pair<std::uint32_t, std::uint32_t> Sculptor::invertMask()
{
    if (m_mesh.shade.size() != m_mesh.vertexCount())
        m_mesh.shade.assign(m_mesh.vertexCount(), 0.0f);
    for (float& m : m_mesh.shade)
        m = 1.0f - m;
    m_everything = true;
    return {0, std::uint32_t(m_mesh.vertexCount())};
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

std::uint32_t Sculptor::triangleAt(const Vec3& point) const
{
    // The triangle nearest the point among those in its cell and the cells round it.
    const std::int64_t ci = std::int64_t(std::floor(point[0] / m_cell)), cj = std::int64_t(std::floor(point[1] / m_cell)),
                       ck = std::int64_t(std::floor(point[2] / m_cell));
    const V3 p = toV(point);
    double best = 1e300;
    std::int64_t found = -1;
    for (std::int64_t i = ci - 1; i <= ci + 1; ++i)
        for (std::int64_t j = cj - 1; j <= cj + 1; ++j)
            for (std::int64_t k = ck - 1; k <= ck + 1; ++k) {
                const auto cell = m_cells.find(cellKey(i, j, k));
                if (cell == m_cells.end())
                    continue;
                for (std::uint32_t t : cell->second) {
                    const std::size_t b = 3 * std::size_t(t);
                    const V3 c = (at(m_mesh.positions, m_mesh.indices[b]) + at(m_mesh.positions, m_mesh.indices[b + 1]) +
                                  at(m_mesh.positions, m_mesh.indices[b + 2])) *
                                 (1.0 / 3.0);
                    const V3 d = c - p;
                    const double d2 = dot(d, d);
                    if (d2 < best) {
                        best = d2;
                        found = t;
                    }
                }
            }
    for (std::uint32_t t : m_stale) {  // moved, and maybe not yet filed where they are now
        const std::size_t b = 3 * std::size_t(t);
        const V3 c = (at(m_mesh.positions, m_mesh.indices[b]) + at(m_mesh.positions, m_mesh.indices[b + 1]) +
                      at(m_mesh.positions, m_mesh.indices[b + 2])) *
                     (1.0 / 3.0);
        const V3 d = c - p;
        const double d2 = dot(d, d);
        if (d2 < best) {
            best = d2;
            found = t;
        }
    }
    if (found >= 0)
        return std::uint32_t(found);
    // Far from everything: the triangle round the nearest point.
    const std::uint32_t v = nearestPoint(point);
    return m_vt[v].empty() ? 0 : m_vt[v].front();
}

namespace {
// The nearest point of triangle abc to q (Ericson, Real-Time Collision Detection 5.1.5).
V3 closestOnTriangle(V3 q, V3 a, V3 b, V3 c)
{
    const V3 ab = b - a, ac = c - a, aq = q - a;
    const double d1 = dot(ab, aq), d2 = dot(ac, aq);
    if (d1 <= 0 && d2 <= 0)
        return a;
    const V3 bq = q - b;
    const double d3 = dot(ab, bq), d4 = dot(ac, bq);
    if (d3 >= 0 && d4 <= d3)
        return b;
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0)
        return a + ab * (d1 / (d1 - d3));
    const V3 cq = q - c;
    const double d5 = dot(ab, cq), d6 = dot(ac, cq);
    if (d6 >= 0 && d5 <= d6)
        return c;
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0)
        return a + ac * (d2 / (d2 - d6));
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const double den = 1.0 / (va + vb + vc);
    return a + ab * (vb * den) + ac * (vc * den);
}
}  // namespace

Vec3 Sculptor::onSurface(const Vec3& point) const
{
    if (m_mesh.indices.empty())
        return point;
    const V3 q = toV(point);
    // The triangles round the corners of the one triangleAt finds (it goes by middles,
    // so the nearest may be a neighbour).
    const std::uint32_t near = triangleAt(point);
    V3 best = q;
    double bestD = 1e300;
    for (int k = 0; k < 3; ++k)
        for (std::uint32_t t : m_vt[m_mesh.indices[3 * std::size_t(near) + k]]) {
            const std::size_t b = 3 * std::size_t(t);
            const V3 x = closestOnTriangle(q, at(m_mesh.positions, m_mesh.indices[b]),
                                           at(m_mesh.positions, m_mesh.indices[b + 1]),
                                           at(m_mesh.positions, m_mesh.indices[b + 2]));
            const double d = dot(x - q, x - q);
            if (d < bestD) {
                bestD = d;
                best = x;
            }
        }
    return {best.x, best.y, best.z};
}

std::size_t trianglesFor(int detail)
{
    return detail <= 0 ? 40'000 : detail >= 2 ? 500'000 : 150'000;
}

Mesh startingMesh(const Mesh& surface, int detail)
{
    const Bounds b = surface.bounds();
    const Mesh shared = welded(surface, std::max(1e-6, double(b.radius()) * 1e-6));
    const std::size_t want = trianglesFor(detail);
    // Splitting halves edges, so the count lands within about 2x of the aim either way.
    return refined(shared, edgeForTriangles(shared, want) * 1.4, want * 3);
}

Symmetry symmetryFor(const SculptStroke& stroke, const Mesh& start)
{
    Symmetry s;
    s.x = stroke.mirrorX;
    s.y = stroke.mirrorY;
    s.z = stroke.mirrorZ;
    const auto c = start.bounds().center();
    s.centre = {c[0], c[1], c[2]};
    return s;
}

void replay(Sculptor& sculptor, const SculptStroke& stroke, const Mesh& start,
            const std::vector<SculptPattern>& patterns)
{
    const Symmetry symmetry = symmetryFor(stroke, start);
    sculptor.setDetail(stroke.finest);
    if (stroke.brush == int(Brush::MaskClear)) {
        sculptor.clearMask();
        return;
    }
    if (stroke.brush == int(Brush::Remesh)) {
        sculptor.remesh(stroke.radius);
        return;
    }
    if (stroke.brush == int(Brush::MaskInvert)) {
        sculptor.invertMask();
        return;
    }
    if (stroke.brush == int(Brush::SnakeHook)) {
        if (stroke.dabs.empty())
            return;
        // From the surface, and the tips moved by as much as the start was.
        const Vec3 raw{stroke.dabs.front()[0], stroke.dabs.front()[1], stroke.dabs.front()[2]};
        const Vec3 from = sculptor.onSurface(raw);
        const Vec3 shift{from[0] - raw[0], from[1] - raw[1], from[2] - raw[2]};
        sculptor.beginSnake(from, stroke.radius, stroke.strength, sculptor.triangleAt(from), symmetry);
        for (std::size_t i = 1; i < stroke.dabs.size(); ++i)
            sculptor.moveSnake({stroke.dabs[i][0] + shift[0], stroke.dabs[i][1] + shift[1],
                                stroke.dabs[i][2] + shift[2]});
        sculptor.endSnake();
        return;
    }
    if (stroke.brush == int(Brush::Grab)) {
        if (stroke.dabs.empty())
            return;
        const Vec3 from = sculptor.onSurface({stroke.dabs.front()[0], stroke.dabs.front()[1], stroke.dabs.front()[2]});
        sculptor.beginGrab(from, stroke.radius, sculptor.triangleAt(from), symmetry);
        sculptor.moveGrab({stroke.grab[0], stroke.grab[1], stroke.grab[2]});
        sculptor.endGrab();
        return;
    }
    const bool pattern = stroke.brush == int(Brush::Pattern);
    if (pattern) {
        if (stroke.pattern < 0 || stroke.pattern >= int(patterns.size()))
            return;  // its pattern is missing: nothing to press
        sculptor.beginPattern(patternImage(patterns[std::size_t(stroke.pattern)]), stroke.tile,
                              patternDepth(stroke), stroke.turn, stroke.once);
    }
    Dab dab;
    dab.radius = stroke.radius;
    dab.strength = stroke.strength;
    dab.brush = pattern ? Brush::Pattern : Brush(std::clamp(stroke.brush, 0, int(Brush::Mask)));
    dab.invert = stroke.invert;
    for (const auto& c : stroke.dabs) {
        // On the surface as it is now (the shape under the step may have grown or shrunk).
        dab.centre = sculptor.onSurface({c[0], c[1], c[2]});
        sculptor.apply(dab, sculptor.triangleAt(dab.centre), symmetry);
    }
    if (pattern)
        sculptor.endPattern();
}

Replay replayedFully(const Mesh& start, const SculptFinish& finish)
{
    Sculptor sculptor(start);
    for (const SculptStroke& stroke : finish.strokes)
        replay(sculptor, stroke, start, finish.patterns);
    return {sculptor.mesh(), sculptor.restPositions()};
}

Mesh atStrength(const Mesh& full, const std::vector<float>& rest, double strength)
{
    if (std::abs(strength - 1.0) < 1e-9 || rest.size() != full.positions.size())
        return full;
    Mesh m = full;
    const float s = float(strength);
    for (std::size_t i = 0; i < m.positions.size(); ++i)
        m.positions[i] = rest[i] + s * (full.positions[i] - rest[i]);
    allNormals(m);
    return m;
}

Mesh replayed(const Mesh& start, const SculptFinish& finish)
{
    Replay r = replayedFully(start, finish);
    return atStrength(r.mesh, r.rest, finish.strength);
}

void Sculptor::setPositions(const std::vector<float>& positions)
{
    if (positions.size() != m_mesh.positions.size())
        throw std::invalid_argument("setPositions: a different number of points");
    m_mesh.positions = positions;
    allNormals(m_mesh);
    rebuild();
}

namespace {
constexpr int kBlockShift = 8;  // blocks of 256
}

void Sculptor::noteChanged(const std::vector<std::uint32_t>& points)
{
    if (m_everything)
        return;
    for (std::uint32_t v : points) {
        const std::uint32_t b = v >> kBlockShift;
        if (b >= m_pointBlockDirty.size())
            m_pointBlockDirty.resize(std::size_t(b) + 1, 0);
        if (!m_pointBlockDirty[b]) {
            m_pointBlockDirty[b] = 1;
            m_pointBlocks.push_back(b);
        }
    }
}

void Sculptor::noteTriangle(std::uint32_t triangle)
{
    if (m_everything)
        return;
    const std::uint32_t b = triangle >> kBlockShift;
    if (b >= m_triBlockDirty.size())
        m_triBlockDirty.resize(std::size_t(b) + 1, 0);
    if (!m_triBlockDirty[b]) {
        m_triBlockDirty[b] = 1;
        m_triBlocks.push_back(b);
    }
}

Sculptor::Changes Sculptor::takeChanges()
{
    Changes out;
    out.everything = m_everything;
    const auto runs = [](std::vector<std::uint32_t>& blocks, std::vector<char>& dirty, std::size_t count,
                         std::vector<std::pair<std::uint32_t, std::uint32_t>>& into) {
        std::sort(blocks.begin(), blocks.end());
        for (std::size_t i = 0; i < blocks.size();) {
            std::size_t j = i + 1;
            while (j < blocks.size() && blocks[j] == blocks[j - 1] + 1)
                ++j;
            const std::uint32_t first = blocks[i] << kBlockShift;
            const auto last = std::uint32_t(std::min<std::size_t>(std::size_t(blocks[j - 1] + 1) << kBlockShift, count));
            if (first < last)
                into.push_back({first, last});
            i = j;
        }
        for (std::uint32_t b : blocks)
            dirty[b] = 0;
        blocks.clear();
    };
    if (!m_everything) {
        runs(m_pointBlocks, m_pointBlockDirty, m_mesh.vertexCount(), out.points);
        runs(m_triBlocks, m_triBlockDirty, m_mesh.triangleCount(), out.triangles);
    } else {
        std::fill(m_pointBlockDirty.begin(), m_pointBlockDirty.end(), 0);
        std::fill(m_triBlockDirty.begin(), m_triBlockDirty.end(), 0);
        m_pointBlocks.clear();
        m_triBlocks.clear();
    }
    m_everything = false;
    return out;
}

}  // namespace forge::sculpt
