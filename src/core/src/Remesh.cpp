// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Remesh.h"

#include "forge/core/Sculpt.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace forge::remesh {

namespace {

struct V3 {
    double x = 0, y = 0, z = 0;
};
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double len2(V3 a) { return dot(a, a); }
double len(V3 a) { return std::sqrt(dot(a, a)); }

using Tri = std::array<std::uint32_t, 3>;

// Which way round: if t has u then v (in its turning order), w is its third corner.
bool turns(const Tri& t, std::uint32_t u, std::uint32_t v, std::uint32_t& w)
{
    for (int k = 0; k < 3; ++k)
        if (t[k] == u && t[(k + 1) % 3] == v) {
            w = t[(k + 2) % 3];
            return true;
        }
    return false;
}

// A triangle mesh being changed: triangles can die, points can die, both can be added.
struct Work {
    std::vector<V3> p;
    std::vector<float> shade;  // empty: none
    std::vector<Tri> tri;
    std::vector<char> deadTri;
    std::vector<std::vector<std::uint32_t>> vt;  // the live triangles round each point
    std::vector<char> deadV;
    std::vector<char> open;  // on an open edge (worked out once: the changes here keep it)
    std::size_t alive = 0;

    explicit Work(const Mesh& m)
    {
        const std::size_t n = m.vertexCount();
        p.resize(n);
        for (std::size_t v = 0; v < n; ++v)
            p[v] = {m.positions[3 * v], m.positions[3 * v + 1], m.positions[3 * v + 2]};
        if (m.shade.size() == n)
            shade = m.shade;
        vt.assign(n, {});
        deadV.assign(n, 0);
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            const Tri x{m.indices[t], m.indices[t + 1], m.indices[t + 2]};
            if (x[0] == x[1] || x[1] == x[2] || x[0] == x[2])
                continue;
            const auto id = std::uint32_t(tri.size());
            tri.push_back(x);
            for (std::uint32_t v : x)
                vt[v].push_back(id);
        }
        deadTri.assign(tri.size(), 0);
        alive = tri.size();
        open.assign(n, 0);
        for (std::uint32_t v = 0; v < n; ++v)
            open[v] = findOpenEdge(v) ? 1 : 0;
    }

    V3 faceNormal(const Tri& t) const { return cross(p[t[1]] - p[t[0]], p[t[2]] - p[t[0]]); }

    // The live triangles with edge u-v (two inside a closed surface).
    int edgeTris(std::uint32_t u, std::uint32_t v, std::uint32_t out[2]) const
    {
        int n = 0;
        for (std::uint32_t t : vt[u]) {
            const Tri& x = tri[t];
            if (x[0] == v || x[1] == v || x[2] == v) {
                if (n < 2)
                    out[n] = t;
                ++n;
            }
        }
        return n;
    }

    bool onOpenEdge(std::uint32_t v) const { return open[v] != 0; }

    bool findOpenEdge(std::uint32_t v) const
    {
        for (std::uint32_t t : vt[v])
            for (std::uint32_t w : tri[t]) {
                if (w == v)
                    continue;
                std::uint32_t two[2];
                if (edgeTris(v, w, two) != 2)
                    return true;
            }
        return false;
    }

    void neighbours(std::uint32_t v, std::vector<std::uint32_t>& out) const
    {
        out.clear();
        for (std::uint32_t t : vt[v])
            for (std::uint32_t w : tri[t])
                if (w != v)
                    out.push_back(w);
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }

    void drop(std::uint32_t v, std::uint32_t t)
    {
        auto& list = vt[v];
        const auto it = std::find(list.begin(), list.end(), t);
        if (it != list.end()) {
            *it = list.back();
            list.pop_back();
        }
    }

    std::uint32_t addPoint(V3 q, float s)
    {
        p.push_back(q);
        if (!shade.empty())
            shade.push_back(s);
        vt.emplace_back();
        deadV.push_back(0);
        open.push_back(0);  // only edges inside are split
        return std::uint32_t(p.size() - 1);
    }

    std::uint32_t addTri(const Tri& x)
    {
        const auto id = std::uint32_t(tri.size());
        tri.push_back(x);
        deadTri.push_back(0);
        for (std::uint32_t v : x)
            vt[v].push_back(id);
        ++alive;
        return id;
    }

    // Edge u-v split at its middle (both its triangles, inside the surface).
    bool split(std::uint32_t u, std::uint32_t v)
    {
        std::uint32_t two[2];
        if (edgeTris(u, v, two) != 2)
            return false;
        std::uint32_t t0 = two[0], t1 = two[1], c = 0, d = 0;
        if (!turns(tri[t0], u, v, c))
            std::swap(t0, t1);
        if (!turns(tri[t0], u, v, c) || !turns(tri[t1], v, u, d))
            return false;
        const std::uint32_t m =
            addPoint((p[u] + p[v]) * 0.5, shade.empty() ? 0.0f : 0.5f * (shade[u] + shade[v]));
        // t0 (u v c) → (u m c) + (m v c); t1 (v u d) → (v m d) + (m u d).
        tri[t0] = {u, m, c};
        drop(v, t0);
        vt[m].push_back(t0);
        addTri({m, v, c});
        tri[t1] = {v, m, d};
        drop(u, t1);
        vt[m].push_back(t1);
        addTri({m, u, d});
        return true;
    }

    // Can edge u-v shrink to one point at `to` (u kept, v gone)? Only if the surface stays
    // closed and unfolded and, when maxEdge2 > 0, no edge round it gets longer than that.
    bool collapsible(std::uint32_t u, std::uint32_t v, V3 to, double maxEdge2,
                     std::vector<std::uint32_t>& scratchA, std::vector<std::uint32_t>& scratchB) const
    {
        std::uint32_t two[2];
        if (edgeTris(u, v, two) != 2)
            return false;
        if (onOpenEdge(u) || onOpenEdge(v))
            return false;
        std::uint32_t c = 0, d = 0;
        const Tri& x0 = tri[two[0]];
        const Tri& x1 = tri[two[1]];
        for (std::uint32_t w : x0)
            if (w != u && w != v)
                c = w;
        for (std::uint32_t w : x1)
            if (w != u && w != v)
                d = w;
        if (c == d || vt[c].size() <= 3 || vt[d].size() <= 3)
            return false;
        // Shared neighbours only the two across the edge, or the surface would pinch.
        neighbours(u, scratchA);
        neighbours(v, scratchB);
        int shared = 0;
        for (std::size_t i = 0, j = 0; i < scratchA.size() && j < scratchB.size();) {
            if (scratchA[i] < scratchB[j])
                ++i;
            else if (scratchB[j] < scratchA[i])
                ++j;
            else {
                if (scratchA[i] != c && scratchA[i] != d)
                    return false;
                ++shared;
                ++i;
                ++j;
            }
        }
        if (shared != 2)
            return false;
        // No triangle round them may turn over (or get much longer edges).
        for (std::uint32_t end : {u, v})
            for (std::uint32_t t : vt[end]) {
                if (t == two[0] || t == two[1])
                    continue;
                const Tri& x = tri[t];
                const V3 before = faceNormal(x);
                V3 q[3];
                for (int k = 0; k < 3; ++k)
                    q[k] = (x[k] == u || x[k] == v) ? to : p[x[k]];
                const V3 after = cross(q[1] - q[0], q[2] - q[0]);
                const double lb = len(before), la = len(after);
                if (!(la > 1e-30) || dot(before, after) <= 0.2 * lb * la)
                    return false;
                if (maxEdge2 > 0.0)
                    for (int k = 0; k < 3; ++k)
                        if (len2(q[(k + 1) % 3] - q[k]) > maxEdge2)
                            return false;
            }
        return true;
    }

    void collapse(std::uint32_t u, std::uint32_t v, V3 to)
    {
        std::uint32_t two[2];
        const int n = edgeTris(u, v, two);
        for (int i = 0; i < std::min(n, 2); ++i) {
            const std::uint32_t t = two[i];
            deadTri[t] = 1;
            for (std::uint32_t w : tri[t])
                drop(w, t);
            --alive;
        }
        for (std::uint32_t t : vt[v]) {
            for (std::uint32_t& w : tri[t])
                if (w == v)
                    w = u;
            vt[u].push_back(t);
        }
        vt[v].clear();
        deadV[v] = 1;
        p[u] = to;
        if (!shade.empty())
            shade[u] = 0.5f * (shade[u] + shade[v]);
    }

    // Edge u-v swapped for the other diagonal of its two triangles, if that evens out
    // how many neighbours the four corners have (about six each is best).
    bool flipIfBetter(std::uint32_t u, std::uint32_t v)
    {
        std::uint32_t two[2];
        if (edgeTris(u, v, two) != 2)
            return false;
        std::uint32_t t0 = two[0], t1 = two[1], c = 0, d = 0;
        if (!turns(tri[t0], u, v, c))
            std::swap(t0, t1);
        if (!turns(tri[t0], u, v, c) || !turns(tri[t1], v, u, d) || c == d)
            return false;
        if (vt[u].size() <= 3 || vt[v].size() <= 3)
            return false;
        std::uint32_t none[2];
        if (edgeTris(c, d, none) != 0)
            return false;  // already joined
        const auto off = [](std::size_t valence) { return (double(valence) - 6.0) * (double(valence) - 6.0); };
        const double before = off(vt[u].size()) + off(vt[v].size()) + off(vt[c].size()) + off(vt[d].size());
        const double after =
            off(vt[u].size() - 1) + off(vt[v].size() - 1) + off(vt[c].size() + 1) + off(vt[d].size() + 1);
        if (!(after < before))
            return false;
        // The new triangles must face the way the old ones did.
        const V3 n = faceNormal(tri[t0]) + faceNormal(tri[t1]);
        const Tri a{c, u, d}, b{d, v, c};
        const V3 na = faceNormal(a), nb = faceNormal(b);
        if (dot(na, n) <= 0.0 || dot(nb, n) <= 0.0 || !(len(na) > 1e-30) || !(len(nb) > 1e-30))
            return false;
        // And not fold sharply against each other (a flip across a crease).
        if (dot(na, nb) <= 0.5 * len(na) * len(nb))
            return false;
        tri[t0] = a;
        tri[t1] = b;
        drop(u, t1);
        drop(v, t0);
        vt[c].push_back(t1);
        vt[d].push_back(t0);
        return true;
    }

    // Every edge once (each is used u→v by one triangle and v→u by the other).
    std::vector<std::pair<std::uint32_t, std::uint32_t>> edges() const
    {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
        out.reserve(alive * 3 / 2 + 1);
        for (std::size_t t = 0; t < tri.size(); ++t) {
            if (deadTri[t])
                continue;
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = tri[t][k], b = tri[t][(k + 1) % 3];
                if (a < b)
                    out.emplace_back(a, b);
            }
        }
        return out;
    }

    V3 pointNormal(std::uint32_t v) const
    {
        V3 n;
        for (std::uint32_t t : vt[v])
            n = n + faceNormal(tri[t]);
        const double l = len(n);
        return l > 0 ? n * (1.0 / l) : V3{0, 0, 1};
    }

    Mesh out() const
    {
        Mesh m;
        std::vector<std::uint32_t> remap(p.size(), UINT32_MAX);
        for (std::size_t t = 0; t < tri.size(); ++t) {
            if (deadTri[t])
                continue;
            for (std::uint32_t v : tri[t]) {
                if (remap[v] == UINT32_MAX) {
                    remap[v] = std::uint32_t(m.positions.size() / 3);
                    m.positions.insert(m.positions.end(), {float(p[v].x), float(p[v].y), float(p[v].z)});
                    if (!shade.empty())
                        m.shade.push_back(shade[v]);
                }
                m.indices.push_back(remap[v]);
            }
        }
        // Normals: the area-weighted sum round each point.
        m.normals.assign(m.positions.size(), 0.0f);
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            const auto P = [&](std::uint32_t i) {
                return V3{m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]};
            };
            const V3 n = cross(P(m.indices[t + 1]) - P(m.indices[t]), P(m.indices[t + 2]) - P(m.indices[t]));
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t v = m.indices[t + k];
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
        return m;
    }
};

// The nearest point on the original surface, through a grid of its triangles.
class Nearest {
public:
    explicit Nearest(const Mesh& m) : m_mesh(m)
    {
        double edges = 0.0;
        std::size_t count = 0;
        for (std::size_t t = 0; t + 2 < m.indices.size() && count < 30000; t += 3, ++count)
            edges += len(P(m.indices[t + 1]) - P(m.indices[t]));
        m_cell = std::max(count ? 2.0 * edges / double(count) : 1.0, 1e-6);
        for (std::uint32_t t = 0; t < m.triangleCount(); ++t) {
            V3 lo = P(m.indices[3 * t]), hi = lo;
            for (int k = 1; k < 3; ++k) {
                const V3 q = P(m.indices[3 * t + k]);
                lo = {std::min(lo.x, q.x), std::min(lo.y, q.y), std::min(lo.z, q.z)};
                hi = {std::max(hi.x, q.x), std::max(hi.y, q.y), std::max(hi.z, q.z)};
            }
            for (std::int64_t i = cellOf(lo.x); i <= cellOf(hi.x); ++i)
                for (std::int64_t j = cellOf(lo.y); j <= cellOf(hi.y); ++j)
                    for (std::int64_t k = cellOf(lo.z); k <= cellOf(hi.z); ++k)
                        m_cells[key(i, j, k)].push_back(t);
        }
    }

    struct Found {
        V3 point;
        std::uint32_t triangle = 0;
        double w[3] = {1, 0, 0};
    };

    Found nearest(V3 q) const
    {
        const std::int64_t ci = cellOf(q.x), cj = cellOf(q.y), ck = cellOf(q.z);
        Found best;
        double bestD = 1e300;
        for (std::int64_t ring = 0; ring <= 6; ++ring) {
            for (std::int64_t i = ci - ring; i <= ci + ring; ++i)
                for (std::int64_t j = cj - ring; j <= cj + ring; ++j)
                    for (std::int64_t k = ck - ring; k <= ck + ring; ++k) {
                        if (std::max({std::abs(i - ci), std::abs(j - cj), std::abs(k - ck)}) != ring)
                            continue;  // only the shell of this ring
                        const auto cell = m_cells.find(key(i, j, k));
                        if (cell == m_cells.end())
                            continue;
                        for (std::uint32_t t : cell->second)
                            consider(q, t, best, bestD);
                    }
            // Nothing in a further ring can be nearer than this ring's inner edge.
            if (bestD < 1e299 && std::sqrt(bestD) <= double(ring) * m_cell)
                break;
        }
        if (bestD > 1e299)  // far from everything: look at every triangle
            for (std::uint32_t t = 0; t < m_mesh.triangleCount(); ++t)
                consider(q, t, best, bestD);
        return best;
    }

private:
    V3 P(std::uint32_t i) const
    {
        return {m_mesh.positions[3 * i], m_mesh.positions[3 * i + 1], m_mesh.positions[3 * i + 2]};
    }
    std::int64_t cellOf(double x) const { return std::int64_t(std::floor(x / m_cell)); }
    static std::uint64_t key(std::int64_t i, std::int64_t j, std::int64_t k)
    {
        return (std::uint64_t(i & 0x1fffff) << 42) | (std::uint64_t(j & 0x1fffff) << 21) | std::uint64_t(k & 0x1fffff);
    }

    void consider(V3 q, std::uint32_t t, Found& best, double& bestD) const
    {
        const V3 a = P(m_mesh.indices[3 * t]), b = P(m_mesh.indices[3 * t + 1]), c = P(m_mesh.indices[3 * t + 2]);
        double w[3];
        const V3 x = closestOnTriangle(q, a, b, c, w);
        const double d = len2(x - q);
        if (d < bestD) {
            bestD = d;
            best.point = x;
            best.triangle = t;
            std::copy(w, w + 3, best.w);
        }
    }

    // The nearest point of triangle abc to q (Ericson, Real-Time Collision Detection 5.1.5).
    static V3 closestOnTriangle(V3 q, V3 a, V3 b, V3 c, double w[3])
    {
        const V3 ab = b - a, ac = c - a, aq = q - a;
        const double d1 = dot(ab, aq), d2 = dot(ac, aq);
        if (d1 <= 0 && d2 <= 0) {
            w[0] = 1, w[1] = 0, w[2] = 0;
            return a;
        }
        const V3 bq = q - b;
        const double d3 = dot(ab, bq), d4 = dot(ac, bq);
        if (d3 >= 0 && d4 <= d3) {
            w[0] = 0, w[1] = 1, w[2] = 0;
            return b;
        }
        const double vc = d1 * d4 - d3 * d2;
        if (vc <= 0 && d1 >= 0 && d3 <= 0) {
            const double s = d1 / (d1 - d3);
            w[0] = 1 - s, w[1] = s, w[2] = 0;
            return a + ab * s;
        }
        const V3 cq = q - c;
        const double d5 = dot(ab, cq), d6 = dot(ac, cq);
        if (d6 >= 0 && d5 <= d6) {
            w[0] = 0, w[1] = 0, w[2] = 1;
            return c;
        }
        const double vb = d5 * d2 - d1 * d6;
        if (vb <= 0 && d2 >= 0 && d6 <= 0) {
            const double s = d2 / (d2 - d6);
            w[0] = 1 - s, w[1] = 0, w[2] = s;
            return a + ac * s;
        }
        const double va = d3 * d6 - d5 * d4;
        if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
            const double s = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            w[0] = 0, w[1] = 1 - s, w[2] = s;
            return b + (c - b) * s;
        }
        const double den = 1.0 / (va + vb + vc);
        const double v = vb * den, u = vc * den;
        w[0] = 1 - v - u, w[1] = v, w[2] = u;
        return a + ab * v + ac * u;
    }

    const Mesh& m_mesh;
    double m_cell = 1.0;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_cells;
};

}  // namespace

Mesh remeshed(const Mesh& mesh, double edge, int rounds, std::size_t maxTriangles)
{
    if (!(edge > 0.0))
        throw std::invalid_argument("remeshed: the edge length must be above zero");
    if (mesh.indices.empty())
        return mesh;
    // Not finer than maxTriangles allows (an even mesh of edge e covers e^2 sqrt(3)/4 each).
    edge = std::max(edge, sculpt::edgeForTriangles(mesh, maxTriangles / 2));
    const Nearest surface(mesh);
    const bool hasShade = mesh.shade.size() == mesh.vertexCount();
    Work w(mesh);
    const double high = 4.0 / 3.0 * edge, low = 4.0 / 5.0 * edge;
    const double high2 = high * high, low2 = low * low;
    std::vector<std::uint32_t> sa, sb;
    for (int round = 0; round < rounds; ++round) {
        // Long edges split, again until none are.
        for (int pass = 0; pass < 8 && w.alive < maxTriangles; ++pass) {
            bool any = false;
            for (const auto& [a, b] : w.edges()) {
                if (w.alive >= maxTriangles)
                    break;
                if (w.deadV[a] || w.deadV[b] || len2(w.p[b] - w.p[a]) <= high2)
                    continue;
                any |= w.split(a, b);
            }
            if (!any)
                break;
        }
        // Short edges collapsed to their middles (where that makes no edge too long).
        for (const auto& [a, b] : w.edges()) {
            if (w.deadV[a] || w.deadV[b] || len2(w.p[b] - w.p[a]) >= low2)
                continue;
            std::uint32_t two[2];
            if (w.edgeTris(a, b, two) != 2)
                continue;  // gone already
            const V3 mid = (w.p[a] + w.p[b]) * 0.5;
            if (w.collapsible(a, b, mid, high2, sa, sb))
                w.collapse(a, b, mid);
        }
        // Six neighbours each, as near as can be.
        for (const auto& [a, b] : w.edges())
            if (!w.deadV[a] && !w.deadV[b])
                w.flipIfBetter(a, b);
        // Each point slid along the surface towards the middle of its neighbours, then
        // put back on the original surface.
        std::vector<V3> next(w.p.size());
        std::vector<std::uint32_t> nb;
        for (std::uint32_t v = 0; v < w.p.size(); ++v) {
            next[v] = w.p[v];
            if (w.deadV[v] || w.vt[v].empty() || w.onOpenEdge(v))
                continue;
            w.neighbours(v, nb);
            V3 middle;
            for (std::uint32_t u : nb)
                middle = middle + w.p[u];
            middle = middle * (1.0 / double(nb.size()));
            const V3 n = w.pointNormal(v);
            const V3 move = middle - w.p[v];
            next[v] = w.p[v] + (move - n * dot(move, n));
        }
        for (std::uint32_t v = 0; v < w.p.size(); ++v) {
            if (w.deadV[v] || w.vt[v].empty())
                continue;
            const auto found = surface.nearest(next[v]);
            w.p[v] = found.point;
            if (hasShade) {
                const std::uint32_t* corner = &mesh.indices[3 * std::size_t(found.triangle)];
                w.shade[v] = float(found.w[0] * mesh.shade[corner[0]] + found.w[1] * mesh.shade[corner[1]] +
                                   found.w[2] * mesh.shade[corner[2]]);
            }
        }
    }
    return w.out();
}

namespace {

// A quadric: the sum of squared distances to planes, as the 10 numbers of a symmetric
// 4x4 matrix.
struct Quadric {
    double a[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // xx xy xz xw yy yz yw zz zw ww
    void addPlane(V3 n, double d, double weight)
    {
        const double v[4] = {n.x, n.y, n.z, d};
        int k = 0;
        for (int i = 0; i < 4; ++i)
            for (int j = i; j < 4; ++j)
                a[k++] += weight * v[i] * v[j];
    }
    Quadric& operator+=(const Quadric& o)
    {
        for (int i = 0; i < 10; ++i)
            a[i] += o.a[i];
        return *this;
    }
    double error(V3 p) const
    {
        const double x = p.x, y = p.y, z = p.z;
        return a[0] * x * x + 2 * a[1] * x * y + 2 * a[2] * x * z + 2 * a[3] * x + a[4] * y * y + 2 * a[5] * y * z +
               2 * a[6] * y + a[7] * z * z + 2 * a[8] * z + a[9];
    }
    // Where the error is least, if that is well defined.
    bool best(V3& out) const
    {
        const double m[3][3] = {{a[0], a[1], a[2]}, {a[1], a[4], a[5]}, {a[2], a[5], a[7]}};
        const double r[3] = {-a[3], -a[6], -a[8]};
        const double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                           m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                           m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        const double scale = std::abs(m[0][0]) + std::abs(m[1][1]) + std::abs(m[2][2]);
        if (!(std::abs(det) > 1e-9 * scale * scale * scale))
            return false;
        const auto solve = [&](int col) {
            double t[3][3];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    t[i][j] = j == col ? r[i] : m[i][j];
            return (t[0][0] * (t[1][1] * t[2][2] - t[1][2] * t[2][1]) - t[0][1] * (t[1][0] * t[2][2] - t[1][2] * t[2][0]) +
                    t[0][2] * (t[1][0] * t[2][1] - t[1][1] * t[2][0])) /
                   det;
        };
        out = {solve(0), solve(1), solve(2)};
        return true;
    }
};

struct Candidate {
    double cost;
    std::uint32_t a, b;
    std::uint32_t stampA, stampB;
    V3 to;
    bool operator>(const Candidate& o) const
    {
        if (cost != o.cost)
            return cost > o.cost;
        if (a != o.a)
            return a > o.a;
        return b > o.b;
    }
};

}  // namespace

Mesh simplified(const Mesh& mesh, std::size_t triangles)
{
    if (mesh.indices.empty())
        return mesh;
    const Bounds b = mesh.bounds();
    Mesh shared = sculpt::welded(mesh, std::max(1e-9, double(b.radius()) * 1e-6));
    shared.shade.clear();
    Work w(shared);
    triangles = std::max<std::size_t>(triangles, 4);
    if (w.alive <= triangles)
        return w.out();

    std::vector<Quadric> q(w.p.size());
    for (const Tri& t : w.tri) {
        const V3 n = w.faceNormal(t);
        const double l = len(n);
        if (!(l > 0))
            continue;
        const V3 u = n * (1.0 / l);
        const double d = -dot(u, w.p[t[0]]);
        for (std::uint32_t v : t)
            q[v].addPlane(u, d, l * 0.5);
    }
    std::vector<std::uint32_t> stamp(w.p.size(), 0);
    const std::vector<char>& open = w.open;

    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> heap;
    const auto consider = [&](std::uint32_t a, std::uint32_t b2) {
        if (open[a] || open[b2])
            return;  // open edges keep their points
        Quadric sum = q[a];
        sum += q[b2];
        const V3 mid = (w.p[a] + w.p[b2]) * 0.5;
        V3 to = mid;
        double cost = sum.error(mid);
        V3 opt;
        const double edge2 = len2(w.p[b2] - w.p[a]);
        if (sum.best(opt) && len2(opt - mid) <= 4.0 * edge2) {
            to = opt;
            cost = sum.error(opt);
        } else {
            for (const V3& c : {w.p[a], w.p[b2]}) {
                const double e = sum.error(c);
                if (e < cost) {
                    cost = e;
                    to = c;
                }
            }
        }
        heap.push({std::max(cost, 0.0), std::min(a, b2), std::max(a, b2), stamp[std::min(a, b2)],
                   stamp[std::max(a, b2)], to});
    };
    for (const auto& [a, b2] : w.edges())
        consider(a, b2);

    std::vector<std::uint32_t> sa, sb, nb;
    while (w.alive > triangles && !heap.empty()) {
        const Candidate c = heap.top();
        heap.pop();
        if (w.deadV[c.a] || w.deadV[c.b] || stamp[c.a] != c.stampA || stamp[c.b] != c.stampB)
            continue;  // out of date
        if (!w.collapsible(c.a, c.b, c.to, 0.0, sa, sb))
            continue;
        w.collapse(c.a, c.b, c.to);
        q[c.a] += q[c.b];
        ++stamp[c.a];
        w.neighbours(c.a, nb);
        for (std::uint32_t n : nb)
            consider(c.a, n);
    }
    return w.out();
}

}  // namespace forge::remesh
