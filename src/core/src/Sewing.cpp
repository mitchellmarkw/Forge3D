// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Sewing.h"

#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Paint.h"
#include "forge/core/Remesh.h"
#include "forge/core/Sculpt.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <unordered_map>

namespace forge::sewing {

namespace {

using V3 = std::array<double, 3>;

V3 sub(const V3& a, const V3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
double dot(const V3& a, const V3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V3 cross(const V3& a, const V3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double len(const V3& a)
{
    return std::sqrt(dot(a, a));
}

std::uint64_t edgeKey(std::uint32_t a, std::uint32_t b)
{
    if (a > b)
        std::swap(a, b);
    return (std::uint64_t(a) << 32) | b;
}

// Solves A x = b for a symmetric positive definite A (given as a product), with a Jacobi
// preconditioner, starting from x.
void conjugateGradient(const std::function<void(const std::vector<double>&, std::vector<double>&)>& times,
                       const std::vector<double>& diagonal, const std::vector<double>& b, std::vector<double>& x,
                       int maxIterations, double tolerance)
{
    const std::size_t n = b.size();
    std::vector<double> r(n), z(n), p(n), q(n);
    times(x, q);
    for (std::size_t i = 0; i < n; ++i)
        r[i] = b[i] - q[i];
    const auto precondition = [&] {
        for (std::size_t i = 0; i < n; ++i)
            z[i] = diagonal[i] > 1e-300 ? r[i] / diagonal[i] : r[i];
    };
    precondition();
    p = z;
    double rz = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);
    const double bNorm = std::sqrt(std::inner_product(b.begin(), b.end(), b.begin(), 0.0)) + 1e-300;
    for (int it = 0; it < maxIterations; ++it) {
        const double rNorm = std::sqrt(std::inner_product(r.begin(), r.end(), r.begin(), 0.0));
        if (rNorm / bNorm < tolerance)
            break;
        times(p, q);
        const double pq = std::inner_product(p.begin(), p.end(), q.begin(), 0.0);
        if (!(std::abs(pq) > 1e-300))
            break;
        const double alpha = rz / pq;
        for (std::size_t i = 0; i < n; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * q[i];
        }
        precondition();
        const double rzNext = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);
        const double beta = rzNext / rz;
        rz = rzNext;
        for (std::size_t i = 0; i < n; ++i)
            p[i] = z[i] + beta * p[i];
    }
}

constexpr std::size_t kMostTriangles = 4000;  // in a piece, to lay it flat

struct Part {
    std::vector<V3> points;
    std::vector<std::array<std::uint32_t, 3>> tris;
    std::vector<int> source;  // each triangle's cloth (painted seams), while it's being made
    int side = 0;
    double area = 0.0;
    V3 centre{0, 0, 0};
};

// A triangle in its own plane: its corners' 2D coordinates.
std::array<P2, 3> local(const V3& a, const V3& b, const V3& c)
{
    const V3 ab = sub(b, a), ac = sub(c, a);
    const double l = len(ab);
    const V3 e1{ab[0] / l, ab[1] / l, ab[2] / l};
    V3 n = cross(ab, ac);
    const double nl = len(n);
    n = {n[0] / nl, n[1] / nl, n[2] / nl};
    const V3 e2 = cross(n, e1);
    return {P2{0, 0}, P2{l, 0}, P2{dot(ac, e1), dot(ac, e2)}};
}

double signedArea(const P2& a, const P2& b, const P2& c)
{
    return 0.5 * ((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]));
}

// The edge loops of a piece (as point indices), each in order.
std::vector<std::vector<std::uint32_t>> loops(const std::vector<std::array<std::uint32_t, 3>>& tris)
{
    std::unordered_map<std::uint64_t, int> uses;
    for (const auto& t : tris)
        for (int k = 0; k < 3; ++k)
            ++uses[edgeKey(t[std::size_t(k)], t[std::size_t((k + 1) % 3)])];
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> next;
    for (const auto& t : tris)
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t a = t[std::size_t(k)], b = t[std::size_t((k + 1) % 3)];
            if (uses[edgeKey(a, b)] == 1)
                next[a].push_back(b);
        }
    std::vector<std::vector<std::uint32_t>> out;
    std::unordered_map<std::uint32_t, std::size_t> used;
    for (auto& [start, list] : next) {
        while (used[start] < list.size()) {
            std::vector<std::uint32_t> loop{start};
            std::uint32_t at = list[used[start]++];
            for (std::size_t guard = 0; at != start && guard < tris.size() * 3; ++guard) {
                loop.push_back(at);
                auto& outs = next[at];
                if (used[at] >= outs.size())
                    break;
                at = outs[used[at]++];
            }
            if (loop.size() >= 3)
                out.push_back(std::move(loop));
        }
    }
    return out;
}

// A closed line with the points that hardly matter dropped (within `tolerance` of the
// line without them).
std::vector<P2> simplifiedLine(const std::vector<P2>& line, double tolerance)
{
    if (line.size() < 8)
        return line;
    std::vector<bool> keep(line.size() + 1, false);
    const auto at = [&](std::size_t i) { return line[i % line.size()]; };
    const std::function<void(std::size_t, std::size_t)> step = [&](std::size_t a, std::size_t b) {
        if (b <= a + 1)
            return;
        const P2 p = at(a), q = at(b);
        const double dx = q[0] - p[0], dy = q[1] - p[1], l = std::hypot(dx, dy);
        double far = -1;
        std::size_t which = a;
        for (std::size_t i = a + 1; i < b; ++i) {
            const P2 r = at(i);
            const double d = l > 1e-12 ? std::abs((r[0] - p[0]) * dy - (r[1] - p[1]) * dx) / l
                                       : std::hypot(r[0] - p[0], r[1] - p[1]);
            if (d > far) {
                far = d;
                which = i;
            }
        }
        if (far > tolerance) {
            keep[which] = true;
            step(a, which);
            step(which, b);
        }
    };
    // From the first point round to the point farthest from it, and back.
    std::size_t half = 0;
    double far = -1;
    for (std::size_t i = 1; i < line.size(); ++i)
        if (const double d = std::hypot(line[i][0] - line[0][0], line[i][1] - line[0][1]); d > far) {
            far = d;
            half = i;
        }
    keep[0] = keep[half] = true;
    step(0, half);
    step(half, line.size());
    std::vector<P2> out;
    for (std::size_t i = 0; i < line.size(); ++i)
        if (keep[i])
            out.push_back(line[i]);
    return out;
}

// A sparse symmetric matrix by rows (the diagonal apart).
struct Sparse {
    std::vector<std::size_t> start;
    std::vector<std::uint32_t> col;
    std::vector<double> value;
    std::vector<double> diagonal;
};

Sparse build(std::size_t n, const std::vector<std::array<std::uint32_t, 3>>& tris,
             const std::vector<std::array<double, 3>>& weight)
{
    // Each edge's weight (from the corners across it), then rows.
    std::unordered_map<std::uint64_t, double> edge;
    for (std::size_t t = 0; t < tris.size(); ++t)
        for (int k = 0; k < 3; ++k)
            edge[edgeKey(tris[t][std::size_t((k + 1) % 3)], tris[t][std::size_t((k + 2) % 3)])] +=
                weight[t][std::size_t(k)];
    Sparse m;
    m.diagonal.assign(n, 0.0);
    std::vector<std::size_t> count(n + 1, 0);
    for (const auto& [key, w] : edge) {
        ++count[std::size_t(key >> 32)];
        ++count[std::size_t(key & 0xFFFFFFFFu)];
    }
    m.start.assign(n + 1, 0);
    for (std::size_t i = 0; i < n; ++i)
        m.start[i + 1] = m.start[i] + count[i];
    m.col.resize(m.start[n]);
    m.value.resize(m.start[n]);
    std::vector<std::size_t> fill(m.start.begin(), m.start.end() - 1);
    for (const auto& [key, w] : edge) {
        const auto a = std::uint32_t(key >> 32), b = std::uint32_t(key & 0xFFFFFFFFu);
        m.col[fill[a]] = b;
        m.value[fill[a]++] = w;
        m.col[fill[b]] = a;
        m.value[fill[b]++] = w;
        m.diagonal[a] += w;
        m.diagonal[b] += w;
    }
    return m;
}

// Lays one piece flat: a fold-free start (its edge round a circle, the inside where its
// neighbours pull it), then as-rigid-as-possible passes, so lengths stay as they are
// (cloth doesn't stretch much). Empty if it can't be.
std::vector<P2> flatten(const Part& part, std::string& problem)
{
    const std::size_t n = part.points.size();
    const auto edgeLoops = loops(part.tris);
    if (edgeLoops.empty()) {
        problem = "it's closed all the way round (choose a split that cuts it open)";
        return {};
    }
    // The longest edge loop goes round the circle.
    std::size_t outer = 0;
    double outerLength = 0;
    for (std::size_t l = 0; l < edgeLoops.size(); ++l) {
        double length = 0;
        for (std::size_t i = 0; i < edgeLoops[l].size(); ++i)
            length += len(sub(part.points[edgeLoops[l][i]], part.points[edgeLoops[l][(i + 1) % edgeLoops[l].size()]]));
        if (length > outerLength) {
            outerLength = length;
            outer = l;
        }
    }
    if (outerLength < 1e-9) {
        problem = "it's too small";
        return {};
    }

    // Each triangle in its own plane, and its area.
    std::vector<std::array<P2, 3>> flat(part.tris.size());
    std::vector<double> area(part.tris.size());
    for (std::size_t t = 0; t < part.tris.size(); ++t) {
        const auto& tr = part.tris[t];
        const double a = 0.5 * len(cross(sub(part.points[tr[1]], part.points[tr[0]]),
                                         sub(part.points[tr[2]], part.points[tr[0]])));
        area[t] = a;
        if (a > 1e-12)
            flat[t] = local(part.points[tr[0]], part.points[tr[1]], part.points[tr[2]]);
    }

    // ---- The start ----
    // Seen straight on (along its average facing): no folds for most pieces (a front, a
    // back, a sleeve's half).
    std::vector<P2> uv(n, P2{0, 0});
    bool folded = true;
    {
        V3 facing{0, 0, 0};
        for (const auto& t : part.tris) {
            const V3 c = cross(sub(part.points[t[1]], part.points[t[0]]), sub(part.points[t[2]], part.points[t[0]]));
            for (int k = 0; k < 3; ++k)
                facing[std::size_t(k)] += c[std::size_t(k)];
        }
        const double fl = len(facing);
        if (fl > 1e-12) {
            const V3 f{facing[0] / fl, facing[1] / fl, facing[2] / fl};
            const V3 helper = std::abs(f[2]) < 0.9 ? V3{0, 0, 1} : V3{1, 0, 0};
            V3 e1 = cross(helper, f);
            const double l1 = len(e1);
            e1 = {e1[0] / l1, e1[1] / l1, e1[2] / l1};
            const V3 e2 = cross(f, e1);
            for (std::size_t i = 0; i < n; ++i)
                uv[i] = {dot(part.points[i], e1), dot(part.points[i], e2)};
            std::size_t flipped = 0, counted = 0;
            for (std::size_t t = 0; t < part.tris.size(); ++t)
                if (area[t] > 1e-12) {
                    ++counted;
                    flipped += signedArea(uv[part.tris[t][0]], uv[part.tris[t][1]], uv[part.tris[t][2]]) <= 0;
                }
            folded = flipped > counted / 10;
        }
    }
    // Folded seen that way: Tutte's embedding (the edge round a circle, the inside where
    // its neighbours pull it), which never folds.
    std::vector<bool> fixed(n, false);
    if (folded) {
        const auto& loop = edgeLoops[outer];
        const double radius = outerLength / (2 * std::numbers::pi);
        double along = 0;
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const double a = 2 * std::numbers::pi * along / outerLength;
            uv[loop[i]] = {radius * std::cos(a), radius * std::sin(a)};
            fixed[loop[i]] = true;
            along += len(sub(part.points[loop[i]], part.points[loop[(i + 1) % loop.size()]]));
        }
        const Sparse uniform =
            build(n, part.tris, std::vector<std::array<double, 3>>(part.tris.size(), {1.0, 1.0, 1.0}));
        std::vector<double> bu(n, 0.0), bv(n, 0.0), xu(n, 0.0), xv(n, 0.0), pre(n, 1.0);
        for (std::size_t i = 0; i < n; ++i) {
            if (fixed[i]) {
                bu[i] = xu[i] = uv[i][0];
                bv[i] = xv[i] = uv[i][1];
                continue;
            }
            pre[i] = uniform.diagonal[i];
            for (std::size_t e = uniform.start[i]; e < uniform.start[i + 1]; ++e)
                if (fixed[uniform.col[e]]) {
                    bu[i] += uniform.value[e] * uv[uniform.col[e]][0];
                    bv[i] += uniform.value[e] * uv[uniform.col[e]][1];
                }
        }
        const auto times = [&](const std::vector<double>& in, std::vector<double>& out) {
            for (std::size_t i = 0; i < n; ++i) {
                if (fixed[i]) {
                    out[i] = in[i];
                    continue;
                }
                double v = uniform.diagonal[i] * in[i];
                for (std::size_t e = uniform.start[i]; e < uniform.start[i + 1]; ++e)
                    if (!fixed[uniform.col[e]])
                        v -= uniform.value[e] * in[uniform.col[e]];
                out[i] = v;
            }
        };
        conjugateGradient(times, pre, bu, xu, 20000, 1e-10);
        conjugateGradient(times, pre, bv, xv, 20000, 1e-10);
        for (std::size_t i = 0; i < n; ++i)
            uv[i] = {xu[i], xv[i]};
    }
    // Seen from outside (the cloth's right side up): flip if it came out mirrored.
    double signedSum = 0;
    for (const auto& t : part.tris)
        signedSum += signedArea(uv[t[0]], uv[t[1]], uv[t[2]]);
    if (signedSum < 0)
        for (P2& p : uv)
            p[0] = -p[0];

    // ---- As rigid as possible ----
    // Cotangent weights (kept positive), one per corner, for the edge across from it.
    std::vector<std::array<double, 3>> cot(part.tris.size(), {0, 0, 0});
    for (std::size_t t = 0; t < part.tris.size(); ++t) {
        if (area[t] <= 1e-12)
            continue;
        const auto& p = flat[t];
        for (int k = 0; k < 3; ++k) {
            const P2& a = p[std::size_t(k)];
            const P2& b = p[std::size_t((k + 1) % 3)];
            const P2& c = p[std::size_t((k + 2) % 3)];
            const double ux = b[0] - a[0], uy = b[1] - a[1], vx = c[0] - a[0], vy = c[1] - a[1];
            const double cr = std::abs(ux * vy - uy * vx);
            cot[t][std::size_t(k)] = std::clamp((ux * vx + uy * vy) / std::max(cr, 1e-12), 0.01, 50.0);
        }
    }
    const Sparse lap = build(n, part.tris, cot);
    const std::uint32_t anchor = edgeLoops[outer].front();  // held still
    std::vector<double> bu(n), bv(n), xu(n), xv(n);
    const auto lapTimes = [&](const std::vector<double>& in, std::vector<double>& out) {
        for (std::size_t i = 0; i < n; ++i) {
            if (i == anchor) {
                out[i] = in[i];
                continue;
            }
            double v = lap.diagonal[i] * in[i];
            for (std::size_t e = lap.start[i]; e < lap.start[i + 1]; ++e)
                if (lap.col[e] != anchor)
                    v -= lap.value[e] * in[lap.col[e]];
            out[i] = v;
        }
    };
    std::vector<double> lapPre = lap.diagonal;
    lapPre[anchor] = 1.0;
    for (int pass = 0; pass < 150; ++pass) {
        std::fill(bu.begin(), bu.end(), 0.0);
        std::fill(bv.begin(), bv.end(), 0.0);
        for (std::size_t t = 0; t < part.tris.size(); ++t) {
            if (area[t] <= 1e-12)
                continue;
            const auto& tr = part.tris[t];
            const auto& p = flat[t];
            // The nearest turn (no stretch) to how it's laid now.
            double s00 = 0, s01 = 0, s10 = 0, s11 = 0;
            for (int k = 0; k < 3; ++k) {
                const int a = (k + 1) % 3, b = (k + 2) % 3;
                const double w = cot[t][std::size_t(k)];
                const double ux = uv[tr[std::size_t(a)]][0] - uv[tr[std::size_t(b)]][0];
                const double uy = uv[tr[std::size_t(a)]][1] - uv[tr[std::size_t(b)]][1];
                const double xx = p[std::size_t(a)][0] - p[std::size_t(b)][0];
                const double xy = p[std::size_t(a)][1] - p[std::size_t(b)][1];
                s00 += w * ux * xx;
                s01 += w * ux * xy;
                s10 += w * uy * xx;
                s11 += w * uy * xy;
            }
            const double angle = std::atan2(s10 - s01, s00 + s11);
            const double c = std::cos(angle), s = std::sin(angle);
            for (int k = 0; k < 3; ++k) {
                const int a = (k + 1) % 3, b = (k + 2) % 3;
                const double w = cot[t][std::size_t(k)];
                const double xx = p[std::size_t(a)][0] - p[std::size_t(b)][0];
                const double xy = p[std::size_t(a)][1] - p[std::size_t(b)][1];
                const double rx = c * xx - s * xy, ry = s * xx + c * xy;
                bu[tr[std::size_t(a)]] += w * rx;
                bv[tr[std::size_t(a)]] += w * ry;
                bu[tr[std::size_t(b)]] -= w * rx;
                bv[tr[std::size_t(b)]] -= w * ry;
            }
        }
        // The anchor stays; its pull moves to the others' right-hand side.
        for (std::size_t i = 0; i < n; ++i) {
            xu[i] = uv[i][0];
            xv[i] = uv[i][1];
        }
        for (std::size_t e = lap.start[anchor]; e < lap.start[anchor + 1]; ++e) {
            bu[lap.col[e]] += lap.value[e] * uv[anchor][0];
            bv[lap.col[e]] += lap.value[e] * uv[anchor][1];
        }
        bu[anchor] = uv[anchor][0];
        bv[anchor] = uv[anchor][1];
        conjugateGradient(lapTimes, lapPre, bu, xu, 3000, 1e-7);
        conjugateGradient(lapTimes, lapPre, bv, xv, 3000, 1e-7);
        double moved = 0, scale = 0;
        for (std::size_t i = 0; i < n; ++i) {
            moved = std::max(moved, std::hypot(xu[i] - uv[i][0], xv[i] - uv[i][1]));
            uv[i] = {xu[i], xv[i]};
            scale = std::max(scale, std::hypot(uv[i][0], uv[i][1]));
        }
        if (pass > 4 && moved < scale * 1e-4)
            break;  // settled
    }
    return uv;
}

}  // namespace

const char* title(Split s)
{
    switch (s) {
    case Split::LeftRight: return "Left and right (a seam down the middle)";
    case Split::None: return "One piece (as it is)";
    default: return "Front and back (side seams)";
    }
}

Pattern make(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices,
             const std::vector<bool>& chosen, Split split, const std::vector<bool>& seams)
{
    // Painted seam lines are part of the cloth (shared out below), not holes in it.
    const auto onSeam = [&](std::size_t t) { return t < seams.size() && seams[t] && !(t < chosen.size() && chosen[t]); };
    // Little gaps in the paint (between dabs) count as painted: unpainted patches, each
    // much smaller than what's painted, would be holes in the cloth.
    std::vector<bool> marked = chosen;
    marked.resize(indices.size() / 3, false);
    for (std::size_t t = 0; t < marked.size(); ++t)
        if (onSeam(t))
            marked[t] = true;
    {
        const std::size_t count = indices.size() / 3;
        std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> byEdge;
        for (std::size_t t = 0; t < count; ++t)
            for (int k = 0; k < 3; ++k)
                byEdge[edgeKey(indices[t * 3 + std::size_t(k)], indices[t * 3 + std::size_t((k + 1) % 3)])].push_back(
                    std::uint32_t(t));
        const auto triArea = [&](std::size_t t) {
            V3 p[3];
            for (int k = 0; k < 3; ++k) {
                const std::size_t v = indices[t * 3 + std::size_t(k)];
                p[k] = {positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]};
            }
            return 0.5 * len(cross(sub(p[1], p[0]), sub(p[2], p[0])));
        };
        double paintedArea = 0;
        for (std::size_t t = 0; t < count; ++t)
            if (marked[t])
                paintedArea += triArea(t);
        std::vector<int> seen(count, 0);
        for (std::size_t start = 0; start < count; ++start) {
            if (marked[start] || seen[start])
                continue;
            std::vector<std::uint32_t> patch{std::uint32_t(start)}, stack{std::uint32_t(start)};
            seen[start] = 1;
            double patchArea = 0;
            bool open = false;  // reaches the mesh's own edge
            while (!stack.empty()) {
                const std::uint32_t t = stack.back();
                stack.pop_back();
                patchArea += triArea(t);
                for (int k = 0; k < 3; ++k) {
                    const auto& with = byEdge[edgeKey(indices[t * 3 + std::size_t(k)], indices[t * 3 + std::size_t((k + 1) % 3)])];
                    if (with.size() < 2)
                        open = true;
                    for (std::uint32_t o : with)
                        if (!marked[o] && !seen[o]) {
                            seen[o] = 1;
                            stack.push_back(o);
                            patch.push_back(o);
                        }
                }
            }
            if (!open && patchArea < paintedArea * 0.05)
                for (std::uint32_t t : patch)
                    marked[t] = true;
        }
    }

    // The chosen triangles, their points shared.
    std::vector<V3> pts;
    std::unordered_map<std::uint32_t, std::uint32_t> remap;
    std::vector<std::array<std::uint32_t, 3>> tris;
    std::vector<bool> seamTri;  // per kept triangle: on a painted seam line
    for (std::size_t t = 0; t * 3 + 2 < indices.size(); ++t) {
        if (!marked[t])
            continue;
        std::array<std::uint32_t, 3> tri{};
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t v = indices[t * 3 + std::size_t(k)];
            auto [it, fresh] = remap.try_emplace(v, std::uint32_t(pts.size()));
            if (fresh)
                pts.push_back({positions[std::size_t(v) * 3], positions[std::size_t(v) * 3 + 1],
                               positions[std::size_t(v) * 3 + 2]});
            tri[std::size_t(k)] = it->second;
        }
        if (tri[0] != tri[1] && tri[1] != tri[2] && tri[0] != tri[2]) {
            tris.push_back(tri);
            seamTri.push_back(onSeam(t));
        }
    }
    if (std::none_of(seamTri.begin(), seamTri.end(), [](bool b) { return !b; }))
        throw std::invalid_argument("Nothing is painted that colour.");

    // Painted seams: each seam line's triangles go to the cloth they're nearest (the one
    // each was reached from, out from the cloth a ring at a time), so the pieces meet down
    // the line's middle and no cloth goes missing. Two triangles side by side join only if
    // the cloth they came from touches: across a line's middle it doesn't, so it's cut.
    std::vector<int> source(tris.size());
    std::iota(source.begin(), source.end(), 0);
    const std::vector<std::array<std::uint32_t, 3>> cloth = tris;  // as kept, for `source`
    if (std::any_of(seamTri.begin(), seamTri.end(), [](bool b) { return b; })) {
        std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> byEdge;
        for (std::size_t t = 0; t < tris.size(); ++t)
            for (int k = 0; k < 3; ++k)
                byEdge[edgeKey(tris[t][std::size_t(k)], tris[t][std::size_t((k + 1) % 3)])].push_back(std::uint32_t(t));
        std::vector<std::uint32_t> front;
        for (std::size_t t = 0; t < tris.size(); ++t)
            if (seamTri[t])
                source[t] = -1;
            else
                front.push_back(std::uint32_t(t));
        while (!front.empty()) {
            std::vector<std::uint32_t> ring;
            for (std::uint32_t t : front)
                for (int k = 0; k < 3; ++k)
                    for (std::uint32_t o : byEdge[edgeKey(tris[t][std::size_t(k)], tris[t][std::size_t((k + 1) % 3)])])
                        if (seamTri[o] && source[o] < 0) {
                            source[o] = source[t];
                            ring.push_back(o);
                        }
            front = std::move(ring);
        }
        // Seam paint that touches no cloth isn't part of it.
        std::vector<std::array<std::uint32_t, 3>> keptTris;
        std::vector<int> keptSource;
        for (std::size_t t = 0; t < tris.size(); ++t)
            if (source[t] >= 0) {
                keptTris.push_back(tris[t]);
                keptSource.push_back(source[t]);
            }
        tris = std::move(keptTris);
        source = std::move(keptSource);
    }
    const auto touch = [&](int a, int b) {
        if (a == b)
            return true;
        for (std::uint32_t u : cloth[std::size_t(a)])
            for (std::uint32_t v : cloth[std::size_t(b)])
                if (u == v)
                    return true;
        return false;
    };

    // Cut along the plane through the middle (real cuts through the triangles).
    V3 lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
    for (const V3& p : pts)
        for (int k = 0; k < 3; ++k) {
            lo[std::size_t(k)] = std::min(lo[std::size_t(k)], p[std::size_t(k)]);
            hi[std::size_t(k)] = std::max(hi[std::size_t(k)], p[std::size_t(k)]);
        }
    const V3 mid{(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
    std::vector<std::array<std::uint32_t, 3>> sideTris[2];
    std::vector<int> sideLabel[2];  // each one's `source`
    if (split == Split::None) {
        sideTris[0] = tris;
        sideLabel[0] = source;
    } else {
        const V3 normal = split == Split::FrontBack ? V3{0, 1, 0} : V3{1, 0, 0};
        std::vector<double> d(pts.size());
        for (std::size_t i = 0; i < pts.size(); ++i) {
            d[i] = dot(sub(pts[i], mid), normal);
            if (std::abs(d[i]) < 1e-7)
                d[i] = 0.0;
        }
        std::unordered_map<std::uint64_t, std::uint32_t> cutPoint;
        const auto onPlane = [&](std::uint32_t a, std::uint32_t b) {
            const auto key = edgeKey(a, b);
            if (const auto f = cutPoint.find(key); f != cutPoint.end())
                return f->second;
            const double s = d[a] / (d[a] - d[b]);
            const V3 p{pts[a][0] + (pts[b][0] - pts[a][0]) * s, pts[a][1] + (pts[b][1] - pts[a][1]) * s,
                       pts[a][2] + (pts[b][2] - pts[a][2]) * s};
            pts.push_back(p);
            d.push_back(0.0);
            const auto id = std::uint32_t(pts.size() - 1);
            cutPoint[key] = id;
            return id;
        };
        for (std::size_t ti = 0; ti < tris.size(); ++ti) {
            const auto& t = tris[ti];
            bool anyNeg = false, anyPos = false;
            for (std::uint32_t v : t) {
                anyNeg = anyNeg || d[v] < 0;
                anyPos = anyPos || d[v] > 0;
            }
            if (!anyPos) {
                sideTris[0].push_back(t);
                sideLabel[0].push_back(source[ti]);
                continue;
            }
            if (!anyNeg) {
                sideTris[1].push_back(t);
                sideLabel[1].push_back(source[ti]);
                continue;
            }
            for (int side = 0; side < 2; ++side) {
                const auto keep = [&](std::uint32_t v) { return side == 0 ? d[v] <= 0 : d[v] >= 0; };
                std::vector<std::uint32_t> poly;
                for (int k = 0; k < 3; ++k) {
                    const std::uint32_t a = t[std::size_t(k)], b = t[std::size_t((k + 1) % 3)];
                    if (keep(a))
                        poly.push_back(a);
                    if ((d[a] < 0 && d[b] > 0) || (d[a] > 0 && d[b] < 0))
                        poly.push_back(onPlane(a, b));
                }
                for (std::size_t k = 1; k + 1 < poly.size(); ++k)
                    if (poly[0] != poly[k] && poly[k] != poly[k + 1] && poly[0] != poly[k + 1]) {
                        sideTris[side].push_back({poly[0], poly[k], poly[k + 1]});
                        sideLabel[side].push_back(source[ti]);
                    }
            }
        }
    }

    // Each side's connected pieces.
    std::vector<Part> parts;
    double totalArea = 0;
    for (int side = 0; side < 2; ++side) {
        const auto& st = sideTris[side];
        std::vector<std::size_t> parent(st.size());
        std::iota(parent.begin(), parent.end(), 0);
        const std::function<std::size_t(std::size_t)> root = [&](std::size_t i) {
            while (parent[i] != i)
                i = parent[i] = parent[parent[i]];
            return i;
        };
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> withEdge;
        for (std::size_t t = 0; t < st.size(); ++t)
            for (int k = 0; k < 3; ++k)
                withEdge[edgeKey(st[t][std::size_t(k)], st[t][std::size_t((k + 1) % 3)])].push_back(t);
        for (const auto& [key, list] : withEdge)
            for (std::size_t i = 0; i < list.size(); ++i)
                for (std::size_t k = i + 1; k < list.size(); ++k)
                    // Not across a painted seam's middle.
                    if (touch(sideLabel[side][list[i]], sideLabel[side][list[k]]))
                        parent[root(list[i])] = root(list[k]);
        std::map<std::size_t, std::size_t> partOf;
        for (std::size_t t = 0; t < st.size(); ++t) {
            const std::size_t r = root(t);
            auto [it, fresh] = partOf.try_emplace(r, parts.size());
            if (fresh) {
                parts.emplace_back();
                parts.back().side = side;
            }
            Part& p = parts[it->second];
            p.tris.push_back(st[t]);
            p.source.push_back(sideLabel[side][t]);
        }
    }
    for (Part& p : parts) {
        // The part's own copy of each point (a seam's points go to both sides), and where a
        // painted seam's middle goes through a point, a copy for each side of it.
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> withEdge;
        for (std::size_t t = 0; t < p.tris.size(); ++t)
            for (int k = 0; k < 3; ++k)
                withEdge[edgeKey(p.tris[t][std::size_t(k)], p.tris[t][std::size_t((k + 1) % 3)])].push_back(t);
        std::vector<std::size_t> corner(p.tris.size() * 3);  // (triangle, corner) -> its group at that point
        std::iota(corner.begin(), corner.end(), 0);
        const std::function<std::size_t(std::size_t)> group = [&](std::size_t i) {
            while (corner[i] != i)
                i = corner[i] = corner[corner[i]];
            return i;
        };
        for (const auto& [key, list] : withEdge) {
            const auto a = std::uint32_t(key >> 32), b = std::uint32_t(key & 0xFFFFFFFFu);
            for (std::size_t i = 0; i < list.size(); ++i)
                for (std::size_t k = i + 1; k < list.size(); ++k) {
                    if (!touch(p.source[list[i]], p.source[list[k]]))
                        continue;  // across a seam's middle
                    for (std::uint32_t v : {a, b}) {
                        const auto at = [&](std::size_t t) {
                            for (int c = 0; c < 3; ++c)
                                if (p.tris[t][std::size_t(c)] == v)
                                    return t * 3 + std::size_t(c);
                            return t * 3;
                        };
                        corner[group(at(list[i]))] = group(at(list[k]));
                    }
                }
        }
        std::map<std::pair<std::uint32_t, std::size_t>, std::uint32_t> own;
        for (std::size_t t = 0; t < p.tris.size(); ++t)
            for (int k = 0; k < 3; ++k) {
                std::uint32_t& v = p.tris[t][std::size_t(k)];
                const auto [it, fresh] = own.try_emplace({v, group(t * 3 + std::size_t(k))}, std::uint32_t(p.points.size()));
                if (fresh)
                    p.points.push_back(pts[v]);
                v = it->second;
            }
        for (const auto& t : p.tris) {
            const V3 c = cross(sub(p.points[t[1]], p.points[t[0]]), sub(p.points[t[2]], p.points[t[0]]));
            const double a = 0.5 * len(c);
            p.area += a;
            for (int k = 0; k < 3; ++k)
                p.centre[std::size_t(k)] += a * (p.points[t[0]][std::size_t(k)] + p.points[t[1]][std::size_t(k)] +
                                                 p.points[t[2]][std::size_t(k)]) / 3.0;
        }
        if (p.area > 0)
            for (double& c : p.centre)
                c /= p.area;
        totalArea += p.area;
    }
    // Crumbs (a dab or two off on its own) aren't pieces.
    parts.erase(std::remove_if(parts.begin(), parts.end(),
                               [&](const Part& p) { return p.area < std::max(1.0, totalArea * 0.005); }),
                parts.end());
    std::sort(parts.begin(), parts.end(), [](const Part& a, const Part& b) {
        return a.side != b.side ? a.side < b.side : a.centre[0] < b.centre[0];
    });

    Pattern out;
    const auto sideName = [&](int side) -> std::string {
        switch (split) {
        case Split::FrontBack: return side == 0 ? "Front" : "Back";
        case Split::LeftRight: return side == 0 ? "Right" : "Left";  // the figure's own left is +X
        default: return "Piece";
        }
    };
    std::map<int, int> perSide, seen;
    for (const Part& p : parts)
        ++perSide[p.side];
    std::vector<std::string> problems;
    for (const Part& part : parts) {
        Piece piece;
        piece.name = sideName(part.side);
        if (perSide[part.side] > 1)
            piece.name += " " + std::to_string(++seen[part.side]);
        // Thinned out inside (its edges stay as they are): a pattern needs a few thousand
        // triangles, not the paint's tens of thousands.
        Part work = part;
        if (work.tris.size() > kMostTriangles) {
            Mesh m;
            for (const V3& q : work.points)
                m.positions.insert(m.positions.end(), {float(q[0]), float(q[1]), float(q[2])});
            for (const auto& t : work.tris)
                m.indices.insert(m.indices.end(), {t[0], t[1], t[2]});
            m = remesh::simplified(m, kMostTriangles);
            work.points.clear();
            work.tris.clear();
            for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
                work.points.push_back({m.positions[i], m.positions[i + 1], m.positions[i + 2]});
            for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3)
                work.tris.push_back({m.indices[i], m.indices[i + 1], m.indices[i + 2]});
        }
        std::string why;
        std::vector<P2> uv = flatten(work, why);
        if (uv.empty()) {
            problems.push_back(piece.name + ": " + why);
            continue;
        }
        // How much lengths changed.
        std::vector<double> change;
        std::unordered_map<std::uint64_t, bool> done;
        for (const auto& t : work.tris)
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = t[std::size_t(k)], b = t[std::size_t((k + 1) % 3)];
                if (done.count(edgeKey(a, b)))
                    continue;
                done[edgeKey(a, b)] = true;
                const double real = len(sub(work.points[a], work.points[b]));
                if (real < 0.05)
                    continue;
                const double laid = std::hypot(uv[a][0] - uv[b][0], uv[a][1] - uv[b][1]);
                change.push_back(std::abs(laid / real - 1.0));
            }
        std::sort(change.begin(), change.end());
        if (!change.empty()) {
            piece.stretch = change[std::min(change.size() - 1, std::size_t(double(change.size()) * 0.95))];
            piece.worst = change.back();
        }
        // Folded over itself anywhere (where it can't lie flat: an armpit's tight corner)?
        double foldedArea = 0, allArea = 0;
        for (const auto& t : work.tris) {
            const double a = signedArea(uv[t[0]], uv[t[1]], uv[t[2]]);
            allArea += std::abs(a);
            if (a < 0)
                foldedArea -= a;
        }
        if (allArea > 0 && foldedArea / allArea > 0.002)
            problems.push_back(piece.name + " folds over itself in a small place (" +
                               std::to_string(std::max(1, int(std::round(foldedArea / allArea * 100)))) +
                               "% of it), where it can't lie flat: a seam there would help (paint a thin gap)");
        // The way up the figure, on the piece: turn it so that's up the page.
        double gx = 0, gy = 0;
        for (const auto& t : work.tris) {
            const V3 e1 = sub(work.points[t[1]], work.points[t[0]]), e2 = sub(work.points[t[2]], work.points[t[0]]);
            const double a11 = dot(e1, e1), a12 = dot(e1, e2), a22 = dot(e2, e2);
            const double det = a11 * a22 - a12 * a12;
            if (det < 1e-18)
                continue;
            const V3 up{0, 0, 1};
            const double r1 = dot(e1, up), r2 = dot(e2, up);
            // Up in the triangle's own terms (along e1, e2), then on the piece.
            const double c1 = (a22 * r1 - a12 * r2) / det, c2 = (a11 * r2 - a12 * r1) / det;
            const double w = 0.5 * std::sqrt(det);
            gx += w * (c1 * (uv[t[1]][0] - uv[t[0]][0]) + c2 * (uv[t[2]][0] - uv[t[0]][0]));
            gy += w * (c1 * (uv[t[1]][1] - uv[t[0]][1]) + c2 * (uv[t[2]][1] - uv[t[0]][1]));
        }
        const double turn = std::hypot(gx, gy) > 1e-9 ? std::numbers::pi / 2 - std::atan2(gy, gx) : 0.0;
        const double c = std::cos(turn), s = std::sin(turn);
        double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
        for (P2& p : uv) {
            p = {c * p[0] - s * p[1], s * p[0] + c * p[1]};
            minX = std::min(minX, p[0]);
            minY = std::min(minY, p[1]);
            maxX = std::max(maxX, p[0]);
            maxY = std::max(maxY, p[1]);
        }
        for (P2& p : uv)
            p = {p[0] - minX, p[1] - minY};
        piece.width = maxX - minX;
        piece.height = maxY - minY;
        piece.points = uv;
        for (const auto& t : work.tris)
            piece.indices.insert(piece.indices.end(), {t[0], t[1], t[2]});
        // Its edges, smoothed a little (the paint's edge follows the triangles).
        for (const auto& loop : loops(work.tris)) {
            std::vector<P2> line;
            for (std::uint32_t v : loop)
                line.push_back(uv[v]);
            for (int pass = 0; pass < 2; ++pass) {
                std::vector<P2> next = line;
                for (std::size_t i = 0; i < line.size(); ++i) {
                    const P2& a = line[(i + line.size() - 1) % line.size()];
                    const P2& b = line[(i + 1) % line.size()];
                    next[i] = {0.5 * line[i][0] + 0.25 * (a[0] + b[0]), 0.5 * line[i][1] + 0.25 * (a[1] + b[1])};
                }
                line = std::move(next);
            }
            line = simplifiedLine(line, 0.05);
            double a2 = 0;
            for (std::size_t i = 0; i < line.size(); ++i) {
                const P2& p = line[i];
                const P2& q = line[(i + 1) % line.size()];
                a2 += p[0] * q[1] - q[0] * p[1];
            }
            if (a2 < 0)
                std::reverse(line.begin(), line.end());
            piece.outline.push_back(std::move(line));
        }
        std::sort(piece.outline.begin(), piece.outline.end(), [](const std::vector<P2>& a, const std::vector<P2>& b) {
            const auto area = [](const std::vector<P2>& l) {
                double s = 0;
                for (std::size_t i = 0; i < l.size(); ++i)
                    s += l[i][0] * l[(i + 1) % l.size()][1] - l[(i + 1) % l.size()][0] * l[i][1];
                return std::abs(s);
            };
            return area(a) > area(b);
        });
        if (piece.worst > 0.15 && piece.stretch > 0.08)
            problems.push_back(piece.name + " stretches up to " + std::to_string(int(std::round(piece.worst * 100))) +
                               "%: it can't lie flat well (try another split)");
        out.pieces.push_back(std::move(piece));
    }
    for (std::size_t i = 0; i < problems.size(); ++i)
        out.problem += (i ? "; " : "") + problems[i];
    if (out.pieces.empty() && out.problem.empty())
        out.problem = "Nothing could be laid flat.";
    return out;
}

std::vector<std::uint32_t> paintedColours(const Document& doc, ObjectId top)
{
    return paint::colours(paint::objectPaint(doc, top));
}

Pattern fromPaint(const Document& doc, ObjectId top, std::uint32_t colour, Split split,
                  std::optional<std::uint32_t> seamColour)
{
    const std::vector<PaintDab> dabs = paint::objectPaint(doc, top);
    if (dabs.empty())
        throw std::invalid_argument("That shape has no paint: paint the clothes on it first (Sculpt → Paint).");
    Mesh m;
    meshkernel::triangles(doc.meshSolid(top), m.positions, m.indices);
    m = sculpt::welded(m, 1e-4);
    const paint::Field field(dabs);
    const std::vector<std::uint32_t> colours = paint::triangleColours(m.positions, m.indices, field);
    std::vector<bool> chosen(colours.size()), seams;
    for (std::size_t t = 0; t < colours.size(); ++t)
        chosen[t] = colours[t] != paint::kOwnColour && (colours[t] & 0xFFFFFFu) == (colour & 0xFFFFFFu);
    if (seamColour) {
        seams.resize(colours.size());
        for (std::size_t t = 0; t < colours.size(); ++t)
            seams[t] = colours[t] != paint::kOwnColour && (colours[t] & 0xFFFFFFu) == (*seamColour & 0xFFFFFFu);
    }
    // Standing as it is on the plate (which way is up and front).
    const Transform world = doc.worldTransform(top);
    std::vector<float> placed(m.positions.size());
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
        const Vec3 p = world.apply({m.positions[i], m.positions[i + 1], m.positions[i + 2]});
        placed[i] = float(p[0]);
        placed[i + 1] = float(p[1]);
        placed[i + 2] = float(p[2]);
    }
    return make(placed, m.indices, chosen, split, seams);
}

}  // namespace forge::sewing
