// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/MaskSplit.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Lasso.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <unordered_set>

namespace forge {
namespace mask {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot3(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross3(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit3(const Vec3& a, const Vec3& fallback)
{
    const double l = std::sqrt(dot3(a, a));
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}

// A three-sided prism (its point aiming along +X: not the same turned any other way),
// `size` from its middle to its point, from z0 to z1, set in `frame`.
MeshSolid keyPrism(const Transform& frame, double size, double z0, double z1)
{
    const double c[3][2] = {{size, 0.0}, {-0.5 * size, 0.6 * size}, {-0.5 * size, -0.6 * size}};
    std::vector<float> pos;
    for (double z : {z0, z1})
        for (const auto& xy : c) {
            const Vec3 p = frame.apply(Vec3{xy[0], xy[1], z});
            pos.insert(pos.end(), {float(p[0]), float(p[1]), float(p[2])});
        }
    // Counter-clockwise from above (+Z): bottom faces down, top up, sides out.
    const std::vector<std::uint32_t> idx{0, 2, 1, 3, 4, 5, 0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4, 2, 0, 3, 2, 3, 5};
    return meshkernel::fromTriangles(pos, idx);
}

double dist2(const Vec3& a, const Vec3& b)
{
    const Vec3 d = sub(a, b);
    return d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
}

std::uint64_t edgeKey(std::uint32_t a, std::uint32_t b)
{
    return a < b ? (std::uint64_t(a) << 32 | b) : (std::uint64_t(b) << 32 | a);
}

}  // namespace

MaskDab movedDab(const MaskDab& d, const Transform& t)
{
    MaskDab out = d;
    out.centre = t.apply(d.centre);
    for (auto& p : out.loop)
        p = t.apply(p);
    if (d.isLasso())
        out.along = t.applyDirection(d.along);
    return out;
}

MaskDab lassoDab(const std::vector<Vec3>& loop, const Vec3& along, bool erase)
{
    MaskDab d;
    d.erase = erase;
    d.along = along;
    d.loop.assign(loop.begin(), loop.end());
    Vec3 c{0, 0, 0};
    for (const Vec3& p : loop)
        for (int k = 0; k < 3; ++k)
            c[std::size_t(k)] += p[std::size_t(k)] / double(std::max<std::size_t>(loop.size(), 1));
    d.centre = c;
    d.radius = 1e-3;
    for (const Vec3& p : loop)
        d.radius = std::max(d.radius, std::sqrt(dist2(p, c)));
    return d;
}

std::array<double, 2> Field::Lasso::flat(const Vec3& p) const
{
    const Vec3 d = sub(p, origin);
    return {d[0] * u[0] + d[1] * u[1] + d[2] * u[2], d[0] * v[0] + d[1] * v[1] + d[2] * v[2]};
}

bool Field::Lasso::covers(const Vec3& p) const
{
    const auto q = flat(p);
    if (q[0] < lo[0] || q[0] > hi[0] || q[1] < lo[1] || q[1] > hi[1])
        return false;
    return lasso::insideFlat(outline, q[0], q[1]);
}

Field::Field(const std::vector<MaskDab>& dabs) : m_dabs(dabs)
{
    double biggest = 0.0;
    m_smallest = dabs.empty() ? 1.0 : 1e300;
    for (std::uint32_t i = 0; i < m_dabs.size(); ++i) {
        const MaskDab& d = m_dabs[i];
        if (d.isLasso()) {
            lasso::Loop loop;
            loop.along = d.along;
            loop.points.assign(d.loop.begin(), d.loop.end());
            const lasso::Frame f = lasso::frameOf(loop);
            Lasso l;
            l.index = i;
            l.origin = f.origin;
            l.u = f.u;
            l.v = f.v;
            l.outline = lasso::flat(loop, f);
            l.lo[0] = l.lo[1] = 1e300;
            l.hi[0] = l.hi[1] = -1e300;
            for (const auto& q : l.outline)
                for (int k = 0; k < 2; ++k) {
                    l.lo[k] = std::min(l.lo[k], q[std::size_t(k)]);
                    l.hi[k] = std::max(l.hi[k], q[std::size_t(k)]);
                }
            // Looked at about a 60th of its size apart (no coarser than a ball's).
            m_smallest = std::min(m_smallest, std::max(0.1, std::max(l.hi[0] - l.lo[0], l.hi[1] - l.lo[1]) / 60.0));
            m_lassos.push_back(std::move(l));
            continue;
        }
        biggest = std::max(biggest, d.radius);
        m_smallest = std::min(m_smallest, d.radius);
    }
    m_cell = std::max(biggest, 1e-3);
    for (std::uint32_t i = 0; i < m_dabs.size(); ++i) {
        const MaskDab& d = m_dabs[i];
        if (d.isLasso())
            continue;
        std::int64_t lo[3], hi[3];
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::int64_t(std::floor((d.centre[std::size_t(k)] - d.radius) / m_cell));
            hi[k] = std::int64_t(std::floor((d.centre[std::size_t(k)] + d.radius) / m_cell));
        }
        for (std::int64_t a = lo[0]; a <= hi[0]; ++a)
            for (std::int64_t b = lo[1]; b <= hi[1]; ++b)
                for (std::int64_t c = lo[2]; c <= hi[2]; ++c)
                    m_cells[key(a, b, c)].push_back(i);  // in order, so later dabs come later
    }
}

bool Field::changesAcross(const Vec3& a, const Vec3& b, const Vec3& c, double finest) const
{
    // Nothing painted (or rubbed out) anywhere near: no change.
    std::int64_t lo[3], hi[3];
    for (int k = 0; k < 3; ++k) {
        const auto x = std::size_t(k);
        lo[k] = std::int64_t(std::floor(std::min({a[x], b[x], c[x]}) / m_cell));
        hi[k] = std::int64_t(std::floor(std::max({a[x], b[x], c[x]}) / m_cell));
    }
    bool any = false;
    for (const Lasso& l : m_lassos) {
        // Near a lasso if the triangle, seen its way, overlaps the lasso's box.
        double tlo[2] = {1e300, 1e300}, thi[2] = {-1e300, -1e300};
        for (const Vec3* p : {&a, &b, &c}) {
            const auto q = l.flat(*p);
            for (int k = 0; k < 2; ++k) {
                tlo[k] = std::min(tlo[k], q[std::size_t(k)]);
                thi[k] = std::max(thi[k], q[std::size_t(k)]);
            }
        }
        if (thi[0] >= l.lo[0] && tlo[0] <= l.hi[0] && thi[1] >= l.lo[1] && tlo[1] <= l.hi[1]) {
            any = true;
            break;
        }
    }
    if (any) {
    } else if ((hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1) > 4096) {
        any = true;  // a huge triangle: just look
    } else {
        for (std::int64_t i = lo[0]; i <= hi[0] && !any; ++i)
            for (std::int64_t j = lo[1]; j <= hi[1] && !any; ++j)
                for (std::int64_t k = lo[2]; k <= hi[2] && !any; ++k)
                    any = m_cells.count(key(i, j, k)) > 0;
    }
    if (!any)
        return false;
    // Looked at on a grid over the triangle, about half the smallest dab apart (a coarser
    // grid on a big triangle: it's looked at again, finer, once it's split).
    const double longestSide = std::sqrt(std::max({dist2(a, b), dist2(b, c), dist2(c, a)}));
    const double spacing = std::max(finest, 0.5 * m_smallest);
    const int n = std::clamp(int(std::ceil(longestSide / spacing)), 1, 48);
    const bool first = masked(a);
    for (int i = 0; i <= n; ++i)
        for (int j = 0; i + j <= n; ++j) {
            const double u = double(i) / n, w = double(j) / n;
            const Vec3 p = add(a, add(mul(sub(b, a), u), mul(sub(c, a), w)));
            if (masked(p) != first)
                return true;
        }
    return false;
}

bool Field::changesAlong(const Vec3& a, const Vec3& b, double finest) const
{
    const double spacing = std::max(finest, 0.5 * m_smallest);
    const int n = std::clamp(int(std::ceil(std::sqrt(dist2(a, b)) / spacing)), 1, 256);
    const bool first = masked(a);
    for (int i = 1; i <= n; ++i)
        if (masked(add(a, mul(sub(b, a), double(i) / n))) != first)
            return true;
    return false;
}

std::uint64_t Field::key(std::int64_t i, std::int64_t j, std::int64_t k) const
{
    const auto q = [](std::int64_t v) { return std::uint64_t(v + (1 << 20)) & 0x1FFFFF; };
    return q(i) | q(j) << 21 | q(k) << 42;
}

bool Field::masked(const Vec3& p) const
{
    bool state = false;
    std::int64_t last = -1;  // the dab that decided
    const auto found = m_cells.find(key(std::int64_t(std::floor(p[0] / m_cell)), std::int64_t(std::floor(p[1] / m_cell)),
                                        std::int64_t(std::floor(p[2] / m_cell))));
    if (found != m_cells.end())
        for (std::uint32_t i : found->second) {
            const MaskDab& d = m_dabs[i];
            if (dist2(p, d.centre) <= d.radius * d.radius) {
                state = !d.erase;  // the last dab covering it decides
                last = i;
            }
        }
    for (const Lasso& l : m_lassos)
        if (std::int64_t(l.index) > last && l.covers(p)) {
            state = !m_dabs[l.index].erase;
            last = l.index;
        }
    return state;
}

bool Field::nearEdge(const Vec3& p, double slack) const
{
    const std::int64_t ci = std::int64_t(std::floor(p[0] / m_cell)), cj = std::int64_t(std::floor(p[1] / m_cell)),
                       ck = std::int64_t(std::floor(p[2] / m_cell));
    for (std::int64_t a = ci - 1; a <= ci + 1; ++a)
        for (std::int64_t b = cj - 1; b <= cj + 1; ++b)
            for (std::int64_t c = ck - 1; c <= ck + 1; ++c) {
                const auto found = m_cells.find(key(a, b, c));
                if (found == m_cells.end())
                    continue;
                for (std::uint32_t i : found->second) {
                    const MaskDab& d = m_dabs[i];
                    if (std::abs(std::sqrt(dist2(p, d.centre)) - d.radius) < slack)
                        return true;
                }
            }
    for (const Lasso& l : m_lassos) {
        const auto q = l.flat(p);
        if (lasso::distanceFlat(l.outline, q[0], q[1]) < slack)
            return true;
    }
    return false;
}

namespace {

// Splits the long edges near the mask's edge (in two, over and over) until they're
// no longer than `longest` there, so the cut can follow the mask's curve. Triangles
// stay joined: every triangle on a split edge is divided.
void refineNearEdge(std::vector<Vec3>& points, std::vector<std::uint32_t>& tris, const Field& field, double longest)
{
    // Only triangles made by the last pass are looked at again: the rest had no edge to
    // split then, and their edges haven't changed (a neighbour splitting a shared edge
    // splits them too, so they're new).
    std::vector<char> fresh(tris.size() / 3, 1);
    for (int pass = 0; pass < 16; ++pass) {
        std::unordered_map<std::uint64_t, std::uint32_t> middle;
        const double l2 = longest * longest;
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
            if (!fresh[t / 3])
                continue;
            const std::uint32_t v[3] = {tris[t], tris[t + 1], tris[t + 2]};
            bool longEdge = false;
            for (int k = 0; k < 3; ++k)
                longEdge = longEdge || dist2(points[v[k]], points[v[(k + 1) % 3]]) > l2;
            // Split only where the paint starts or stops across the triangle: not inside
            // the paint, nor away from it, however many strokes overlap there.
            if (!longEdge || !field.changesAcross(points[v[0]], points[v[1]], points[v[2]], longest))
                continue;
            // Its longest side halved: always the longest, so long thin triangles (as merges
            // leave) get squarer as they're split, not thinner.
            int longestSide = 0;
            for (int k = 1; k < 3; ++k)
                if (dist2(points[v[k]], points[v[(k + 1) % 3]]) >
                    dist2(points[v[longestSide]], points[v[(longestSide + 1) % 3]]))
                    longestSide = k;
            const std::uint32_t a = v[longestSide], b = v[(longestSide + 1) % 3];
            if (!middle.count(edgeKey(a, b))) {
                middle.emplace(edgeKey(a, b), std::uint32_t(points.size()));
                points.push_back(mul(add(points[a], points[b]), 0.5));
            }
        }
        if (middle.empty())
            return;
        std::vector<std::uint32_t> next;
        std::vector<char> nextFresh;
        next.reserve(tris.size() * 2);
        nextFresh.reserve(tris.size() * 2 / 3);
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
            const std::uint32_t v[3] = {tris[t], tris[t + 1], tris[t + 2]};
            std::int64_t e[3];
            for (int k = 0; k < 3; ++k) {
                const auto f = middle.find(edgeKey(v[k], v[(k + 1) % 3]));
                e[k] = f == middle.end() ? -1 : std::int64_t(f->second);
            }
            const int splits = (e[0] >= 0) + (e[1] >= 0) + (e[2] >= 0);
            if (splits == 0) {
                next.insert(next.end(), {v[0], v[1], v[2]});
            } else if (splits == 3) {
                const auto a = std::uint32_t(e[0]), b = std::uint32_t(e[1]), c = std::uint32_t(e[2]);
                next.insert(next.end(), {v[0], a, c, a, v[1], b, c, b, v[2], a, b, c});
            } else if (splits == 1) {
                int r = 0;
                while (e[r] < 0)
                    ++r;
                const std::uint32_t a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3], m = std::uint32_t(e[r]);
                next.insert(next.end(), {a, m, c, m, b, c});
            } else {
                // Two split: the unsplit edge is r+2 (from c to a); fan from the corner between them.
                int r = 0;
                while (e[r] < 0 || e[(r + 1) % 3] < 0)
                    ++r;
                const std::uint32_t a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3];
                const auto mab = std::uint32_t(e[r]), mbc = std::uint32_t(e[(r + 1) % 3]);
                next.insert(next.end(), {mab, b, mbc, a, mab, mbc, a, mbc, c});
            }
            nextFresh.resize(next.size() / 3, splits > 0 ? 1 : 0);
        }
        tris.swap(next);
        fresh.swap(nextFresh);
    }
}

}  // namespace

Pieces split(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices,
             const std::vector<MaskDab>& dabs, MaskCut cut)
{
    const Field field(dabs);
    std::vector<Vec3> points(positions.size() / 3);
    for (std::size_t v = 0; v < points.size(); ++v)
        points[v] = {positions[3 * v], positions[3 * v + 1], positions[3 * v + 2]};
    std::vector<std::uint32_t> refined = indices;
    refineNearEdge(points, refined, field, std::max(0.02, field.smallest() / 4.0));
    const std::size_t n0 = points.size();
    std::vector<char> inside(n0, 0);
    std::size_t in = 0;
    for (std::size_t v = 0; v < n0; ++v) {
        inside[v] = field.masked(points[v]) ? 1 : 0;
        in += std::size_t(inside[v]);
    }
    if (in == 0)
        throw std::invalid_argument("Nothing of the shape is masked: paint over the part to cut off.");
    if (in == n0)
        throw std::invalid_argument("All of the shape is masked: leave some of it unpainted.");

    // Where each edge crossing the mask's edge is cut: found by halving from the masked end.
    std::unordered_map<std::uint64_t, std::uint32_t> cuts;
    const auto cutOn = [&](std::uint32_t a, std::uint32_t b) {
        const std::uint64_t k = edgeKey(a, b);
        if (const auto f = cuts.find(k); f != cuts.end())
            return f->second;
        const std::uint32_t from = inside[a] ? a : b, to = inside[a] ? b : a;
        double lo = 0.0, hi = 1.0;
        const Vec3 p = points[from], d = sub(points[to], points[from]);
        for (int i = 0; i < 32; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (field.masked(add(p, mul(d, mid))))
                lo = mid;
            else
                hi = mid;
        }
        const auto id = std::uint32_t(points.size());
        points.push_back(add(p, mul(d, 0.5 * (lo + hi))));
        cuts.emplace(k, id);
        return id;
    };

    std::vector<std::uint32_t> inTris, outTris;
    const auto put = [&](bool masked, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        auto& to = masked ? inTris : outTris;
        to.insert(to.end(), {a, b, c});
    };
    for (std::size_t t = 0; t + 2 < refined.size(); t += 3) {
        const std::uint32_t v[3] = {refined[t], refined[t + 1], refined[t + 2]};
        const int count = inside[v[0]] + inside[v[1]] + inside[v[2]];
        if (count == 0 || count == 3) {
            put(count == 3, v[0], v[1], v[2]);
            continue;
        }
        // The corner on its own side first, the turn kept.
        int odd = 0;
        for (int k = 0; k < 3; ++k)
            if (inside[v[k]] != inside[v[(k + 1) % 3]] && inside[v[k]] != inside[v[(k + 2) % 3]])
                odd = k;
        const std::uint32_t a = v[odd], b = v[(odd + 1) % 3], c = v[(odd + 2) % 3];
        const std::uint32_t ab = cutOn(a, b), ac = cutOn(a, c);
        put(inside[a], a, ab, ac);
        put(!inside[a], ab, b, c);
        put(!inside[a], ab, c, ac);
    }

    // The masked piece's open edges, chained into loops (each edge as the piece runs it).
    const auto loopsOf = [&](const std::vector<std::uint32_t>& piece) {
        std::unordered_set<std::uint64_t> directed;
        for (std::size_t t = 0; t + 2 < piece.size(); t += 3)
            for (int k = 0; k < 3; ++k)
                directed.insert(std::uint64_t(piece[t + std::size_t(k)]) << 32 | piece[t + std::size_t((k + 1) % 3)]);
        std::unordered_map<std::uint32_t, std::uint32_t> next;
        for (std::uint64_t e : directed) {
            const auto u = std::uint32_t(e >> 32), w = std::uint32_t(e & 0xFFFFFFFFu);
            if (!directed.count(std::uint64_t(w) << 32 | u))
                next[u] = w;
        }
        std::vector<std::vector<std::uint32_t>> found;
        std::unordered_set<std::uint32_t> seen;
        std::vector<std::uint32_t> starts;
        for (const auto& [u, w] : next)
            starts.push_back(u);
        std::sort(starts.begin(), starts.end());  // the same caps every time
        for (std::uint32_t start : starts) {
            if (seen.count(start))
                continue;
            std::vector<std::uint32_t> loop;
            std::uint32_t at = start;
            do {
                if (!seen.insert(at).second)
                    throw std::invalid_argument("The mask's edge couldn't be followed round (it may cross itself).");
                loop.push_back(at);
                const auto f = next.find(at);
                if (f == next.end())
                    throw std::invalid_argument("The mask's edge couldn't be followed round.");
                at = f->second;
            } while (at != start);
            if (loop.size() >= 3)
                found.push_back(std::move(loop));
        }
        return found;
    };
    std::vector<std::vector<std::uint32_t>> loops = loopsOf(inTris);

    if (cut != MaskCut::Curved) {
        // Flat: through the plane that best fits the mask's edge, the masked side being
        // only what joins the masked part on that side (nothing else of the shape is cut).
        // The mask's main edge: the longest. Small gaps left in the paint and specks of it
        // elsewhere make little edges of their own; they don't change where the cut goes.
        const auto lengthOf = [&](const std::vector<std::uint32_t>& loop) {
            double l = 0.0;
            for (std::size_t i = 0; i < loop.size(); ++i)
                l += std::sqrt(dist2(points[loop[i]], points[loop[(i + 1) % loop.size()]]));
            return l;
        };
        if (loops.empty())
            throw std::invalid_argument("The mask has no edge to cut along.");
        std::size_t main = 0;
        std::vector<double> lengths;
        for (std::size_t i = 0; i < loops.size(); ++i) {
            lengths.push_back(lengthOf(loops[i]));
            if (lengths[i] > lengths[main])
                main = i;
        }
        int big = 0;
        for (double l : lengths)
            big += l >= 0.5 * lengths[main] ? 1 : 0;
        if (big > 1)
            throw std::invalid_argument("A flat cut needs the mask's edge to go round the shape just once (this one "
                                        "goes round " + std::to_string(big) +
                                        " times, like a band): paint one end off, or use the curved cut.");
        const auto& edge = loops[main];
        Vec3 c{0, 0, 0};
        for (std::size_t i = 0; i < edge.size(); ++i) {
            // Weighted by the edge's length on each side, so close-set points don't pull it.
            const double w = std::sqrt(dist2(points[edge[i]], points[edge[(i + 1) % edge.size()]])) +
                             std::sqrt(dist2(points[edge[i]], points[edge[(i + edge.size() - 1) % edge.size()]]));
            c = add(c, mul(points[edge[i]], 0.5 * w));
        }
        c = mul(c, 1.0 / lengths[main]);
        Vec3 area{0, 0, 0};
        for (std::size_t i = 0; i < edge.size(); ++i)
            area = add(area, cross3(sub(points[edge[i]], c), sub(points[edge[(i + 1) % edge.size()]], c)));
        // Out of the masked part: the way its own triangles' middle isn't.
        Vec3 n = unit3(area, {0, 0, 1});
        Vec3 maskedMiddle{0, 0, 0};
        double weight = 0;
        for (std::size_t t = 0; t + 2 < inTris.size(); t += 3) {
            const Vec3 a = points[inTris[t]], b = points[inTris[t + 1]], d = points[inTris[t + 2]];
            const double w = std::sqrt(dist2(cross3(sub(b, a), sub(d, a)), {0, 0, 0}));
            maskedMiddle = add(maskedMiddle, mul(add(add(a, b), d), w / 3.0));
            weight += w;
        }
        if (weight > 0 && dot3(sub(mul(maskedMiddle, 1.0 / weight), c), n) > 0)
            n = mul(n, -1.0);

        // Start again from the surface before the mask's cut, and cut it by the plane.
        points.resize(n0);
        cuts.clear();
        std::vector<double> d(n0);
        for (std::size_t v = 0; v < n0; ++v) {
            d[v] = dot3(sub(points[v], c), n);
            if (std::abs(d[v]) < 1e-9)
                d[v] = 0.0;
        }
        const auto onPlane = [&](std::uint32_t a, std::uint32_t b) {
            const std::uint64_t k = edgeKey(a, b);
            if (const auto f = cuts.find(k); f != cuts.end())
                return f->second;
            const double t = d[a] / (d[a] - d[b]);
            const auto id = std::uint32_t(points.size());
            points.push_back(add(points[a], mul(sub(points[b], points[a]), t)));
            d.push_back(0.0);
            cuts.emplace(k, id);
            return id;
        };
        std::vector<std::array<std::uint32_t, 3>> below, above;  // the masked side; the other
        for (std::size_t t = 0; t + 2 < refined.size(); t += 3) {
            const std::uint32_t v[3] = {refined[t], refined[t + 1], refined[t + 2]};
            bool neg = false, pos = false;
            for (std::uint32_t x : v) {
                neg = neg || d[x] < 0;
                pos = pos || d[x] > 0;
            }
            if (!pos) {
                below.push_back({v[0], v[1], v[2]});
                continue;
            }
            if (!neg) {
                above.push_back({v[0], v[1], v[2]});
                continue;
            }
            for (int side = 0; side < 2; ++side) {
                std::vector<std::uint32_t> poly;
                for (int k = 0; k < 3; ++k) {
                    const std::uint32_t a = v[k], b = v[(k + 1) % 3];
                    if (side == 0 ? d[a] <= 0 : d[a] >= 0)
                        poly.push_back(a);
                    if ((d[a] < 0 && d[b] > 0) || (d[a] > 0 && d[b] < 0))
                        poly.push_back(onPlane(a, b));
                }
                for (std::size_t k = 1; k + 1 < poly.size(); ++k)
                    if (poly[0] != poly[k] && poly[k] != poly[k + 1] && poly[0] != poly[k + 1])
                        (side == 0 ? below : above).push_back({poly[0], poly[k], poly[k + 1]});
            }
        }
        // The masked side's triangles joined (not across the plane) to masked ones.
        std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> byEdge;
        for (std::size_t t = 0; t < below.size(); ++t)
            for (int k = 0; k < 3; ++k)
                byEdge[edgeKey(below[t][std::size_t(k)], below[t][std::size_t((k + 1) % 3)])].push_back(std::uint32_t(t));
        std::vector<char> keep(below.size(), 0);
        std::vector<std::uint32_t> stack;
        const auto areaOf = [&](std::uint32_t t) {
            return std::sqrt(dist2(cross3(sub(points[below[t][1]], points[below[t][0]]),
                                          sub(points[below[t][2]], points[below[t][0]])),
                                   {0, 0, 0}));
        };
        const auto acrossPlane = [&](std::uint32_t a, std::uint32_t b) { return d[a] == 0.0 && d[b] == 0.0; };
        {
            // The masked part: the biggest painted patch on this side (a speck of paint
            // elsewhere doesn't take what's round it along).
            std::vector<char> painted(below.size(), 0);
            for (std::size_t t = 0; t < below.size(); ++t) {
                const Vec3 mid =
                    mul(add(add(points[below[t][0]], points[below[t][1]]), points[below[t][2]]), 1.0 / 3.0);
                painted[t] = field.masked(mid) ? 1 : 0;
            }
            std::vector<int> patch(below.size(), -1);
            std::vector<double> patchArea;
            for (std::size_t s = 0; s < below.size(); ++s) {
                if (!painted[s] || patch[s] >= 0)
                    continue;
                const int id = int(patchArea.size());
                patchArea.push_back(0.0);
                std::vector<std::uint32_t> todo{std::uint32_t(s)};
                patch[s] = id;
                while (!todo.empty()) {
                    const std::uint32_t t = todo.back();
                    todo.pop_back();
                    patchArea[std::size_t(id)] += areaOf(t);
                    for (int k = 0; k < 3; ++k) {
                        const std::uint32_t a = below[t][std::size_t(k)], b = below[t][std::size_t((k + 1) % 3)];
                        if (acrossPlane(a, b))
                            continue;
                        for (std::uint32_t o : byEdge[edgeKey(a, b)])
                            if (painted[o] && patch[o] < 0) {
                                patch[o] = id;
                                todo.push_back(o);
                            }
                    }
                }
            }
            if (patchArea.empty())
                throw std::invalid_argument("The flat cut leaves nothing of the masked part: paint a little further in.");
            const int biggest =
                int(std::max_element(patchArea.begin(), patchArea.end()) - patchArea.begin());
            for (std::size_t t = 0; t < below.size(); ++t)
                if (patch[t] == biggest) {
                    keep[t] = 1;
                    stack.push_back(std::uint32_t(t));
                }
        }
        while (!stack.empty()) {
            const std::uint32_t t = stack.back();
            stack.pop_back();
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = below[t][std::size_t(k)], b = below[t][std::size_t((k + 1) % 3)];
                if (acrossPlane(a, b))
                    continue;  // along the plane: not across it
                for (std::uint32_t o : byEdge[edgeKey(a, b)])
                    if (!keep[o]) {
                        keep[o] = 1;
                        stack.push_back(o);
                    }
            }
        }
        inTris.clear();
        outTris.clear();
        for (std::size_t t = 0; t < below.size(); ++t)
            (keep[t] ? inTris : outTris).insert(keep[t] ? inTris.end() : outTris.end(), below[t].begin(), below[t].end());
        for (const auto& t : above)
            outTris.insert(outTris.end(), t.begin(), t.end());
        if (outTris.empty())
            throw std::invalid_argument("The flat cut leaves nothing of the rest.");
        loops = loopsOf(inTris);
    }

    // A cap for each loop: rings drawn in to the loop's middle, then smoothed like a
    // stretched membrane (each inner point to the middle of its neighbours).
    std::vector<std::uint32_t> caps;
    std::vector<KeySpot> keySpots;
    for (const auto& loop : loops) {
        const std::size_t n = loop.size();
        Vec3 c{0, 0, 0};
        for (std::uint32_t v : loop)
            c = add(c, points[v]);
        c = mul(c, 1.0 / double(n));
        // The inner rings no finer than 96 points round (a finely cut edge would otherwise
        // fill the cap with hundreds of thousands of triangles), stitched to the edge itself.
        const std::size_t m = std::min<std::size_t>(n, 96);
        const int rings = std::clamp(int(double(m) / (2.0 * std::numbers::pi)), 2, 40);
        std::vector<std::vector<std::uint32_t>> ring{loop};
        const std::size_t firstInner = points.size();
        for (int k = 1; k < rings; ++k) {
            std::vector<std::uint32_t> r(m);
            const double t = 1.0 - double(k) / rings;
            for (std::size_t i = 0; i < m; ++i) {
                r[i] = std::uint32_t(points.size());
                points.push_back(add(c, mul(sub(points[loop[i * n / m]], c), t)));
            }
            ring.push_back(std::move(r));
        }
        const auto centre = std::uint32_t(points.size());
        points.push_back(c);
        std::vector<std::uint32_t> tris;
        // Each edge run the other way from the masked piece. The edge (n points) to the first
        // ring (m): along both together, stepping whichever is behind.
        if (ring.size() > 1) {
            const auto& o = ring[0];
            const auto& x = ring[1];
            std::size_t i = 0, j = 0;
            while (i < n || j < m) {
                const bool outer = j == m || (i < n && double(i + 1) * double(m) <= double(j + 1) * double(n));
                if (outer) {
                    tris.insert(tris.end(), {o[(i + 1) % n], o[i], x[j % m]});
                    ++i;
                } else {
                    tris.insert(tris.end(), {o[i % n], x[j], x[(j + 1) % m]});
                    ++j;
                }
            }
        }
        for (std::size_t k = 1; k + 1 < ring.size(); ++k)
            for (std::size_t i = 0; i < m; ++i) {
                const std::size_t j = (i + 1) % m;
                const std::uint32_t o0 = ring[k][i], o1 = ring[k][j], i0 = ring[k + 1][i], i1 = ring[k + 1][j];
                tris.insert(tris.end(), {o1, o0, i0, o1, i0, i1});
            }
        const std::size_t last = ring.back().size();
        for (std::size_t i = 0; i < last; ++i)
            tris.insert(tris.end(), {ring.back()[(i + 1) % last], ring.back()[i], centre});
        // Neighbours of the inner points, then the smoothing.
        const std::size_t inner = points.size() - firstInner;
        std::vector<std::vector<std::uint32_t>> around(inner);
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = tris[t + std::size_t(k)], b = tris[t + std::size_t((k + 1) % 3)];
                if (a >= firstInner)
                    around[a - firstInner].push_back(b);
                if (b >= firstInner)
                    around[b - firstInner].push_back(a);
            }
        for (int pass = 0; pass < 300; ++pass)
            for (std::size_t i = 0; i < inner; ++i) {
                Vec3 sum{0, 0, 0};
                for (std::uint32_t o : around[i])
                    sum = add(sum, points[o]);
                if (!around[i].empty())
                    points[firstInner + i] = mul(sum, 1.0 / double(around[i].size()));
            }
        // Where an alignment key could go: the cap's middle point (smoothed with the rest).
        KeySpot spot;
        spot.point = points[centre];
        Vec3 way{0, 0, 0};
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
            way = add(way, cross3(sub(points[tris[t + 1]], points[tris[t]]), sub(points[tris[t + 2]], points[tris[t]])));
        spot.normal = unit3(way, {0, 0, 1});
        double room = 1e300, reach = 0.0;
        for (std::uint32_t v : loop) {
            room = std::min(room, std::sqrt(dist2(points[v], spot.point)));
            reach = std::max(reach, std::sqrt(dist2(points[v], spot.point)));
        }
        spot.room = room;
        spot.reach = reach;
        const Vec3 toEdge = sub(points[loop.front()], spot.point);
        spot.across = unit3(sub(toEdge, mul(spot.normal, dot3(toEdge, spot.normal))), {1, 0, 0});
        keySpots.push_back(spot);
        caps.insert(caps.end(), tris.begin(), tris.end());
    }

    // Each piece with its own points.
    const auto build = [&](const std::vector<std::uint32_t>& sides, bool flipCaps) {
        Mesh m;
        std::unordered_map<std::uint32_t, std::uint32_t> local;
        const auto at = [&](std::uint32_t v) {
            const auto [it, added] = local.emplace(v, std::uint32_t(local.size()));
            if (added)
                for (int k = 0; k < 3; ++k)
                    m.positions.push_back(float(points[v][std::size_t(k)]));
            return it->second;
        };
        for (std::uint32_t v : sides)
            m.indices.push_back(at(v));
        for (std::size_t t = 0; t + 2 < caps.size(); t += 3) {
            if (flipCaps)
                m.indices.insert(m.indices.end(), {at(caps[t]), at(caps[t + 2]), at(caps[t + 1])});
            else
                m.indices.insert(m.indices.end(), {at(caps[t]), at(caps[t + 1]), at(caps[t + 2])});
        }
        return m;
    };
    Pieces out;
    {
        // The painted surface alone: the masked piece without its caps.
        std::unordered_map<std::uint32_t, std::uint32_t> local;
        for (std::uint32_t v : inTris) {
            const auto [it, added] = local.emplace(v, std::uint32_t(local.size()));
            if (added)
                for (int k = 0; k < 3; ++k)
                    out.surface.positions.push_back(float(points[v][std::size_t(k)]));
            out.surface.indices.push_back(it->second);
        }
    }
    out.masked = build(inTris, false);
    out.rest = build(outTris, true);
    out.caps = int(loops.size());
    if (cut == MaskCut::Dome && !keySpots.empty()) {
        // A dome on the main cut's middle (the biggest, not a speck's): well inside its
        // edge, a little under half as high as it is across at its foot.
        const KeySpot& spot = *std::max_element(keySpots.begin(), keySpots.end(),
                                                [](const KeySpot& a, const KeySpot& b) { return a.reach < b.reach; });
        if (spot.room >= 1.0) {
            Dome dome;
            dome.centre = spot.point;
            dome.normal = spot.normal;
            dome.radius = 0.6 * spot.room;
            dome.height = 0.45 * dome.radius;
            out.dome = dome;
        }
    }
    out.keySpots = std::move(keySpots);
    return out;
}

const char* title(MaskCut cut)
{
    switch (cut) {
    case MaskCut::Flat: return "Flat";
    case MaskCut::Dome: return "Ball and socket";
    default: return "Curved";
    }
}

namespace {

// A ball of triangles, about `segments` round.
MeshSolid ball(const Vec3& centre, double r, int segments)
{
    const int rings = std::max(4, segments / 2), around = std::max(8, segments);
    std::vector<float> p;
    std::vector<std::uint32_t> idx;
    p.insert(p.end(), {float(centre[0]), float(centre[1]), float(centre[2] + r)});
    for (int i = 1; i < rings; ++i)
        for (int k = 0; k < around; ++k) {
            const double t = std::numbers::pi * i / rings, f = 2.0 * std::numbers::pi * k / around;
            p.insert(p.end(), {float(centre[0] + r * std::sin(t) * std::cos(f)),
                               float(centre[1] + r * std::sin(t) * std::sin(f)), float(centre[2] + r * std::cos(t))});
        }
    p.insert(p.end(), {float(centre[0]), float(centre[1]), float(centre[2] - r)});
    const auto bottom = std::uint32_t(p.size() / 3 - 1);
    const auto v = [&](int i, int k) { return std::uint32_t(1 + (i - 1) * around + (k % around)); };
    for (int k = 0; k < around; ++k) {
        idx.insert(idx.end(), {0, v(1, k), v(1, k + 1)});
        idx.insert(idx.end(), {bottom, v(rings - 1, k + 1), v(rings - 1, k)});
    }
    for (int i = 1; i < rings - 1; ++i)
        for (int k = 0; k < around; ++k)
            idx.insert(idx.end(), {v(i, k), v(i + 1, k), v(i + 1, k + 1), v(i, k), v(i + 1, k + 1), v(i, k + 1)});
    return meshkernel::fromTriangles(p, idx);
}

// A box in a frame: X, Y from -half to half, Z from z0 to z1.
MeshSolid slab(const Transform& frame, double half, double z0, double z1)
{
    std::vector<float> p;
    for (int c = 0; c < 8; ++c) {
        const Vec3 q = frame.apply(Vec3{(c & 1) ? half : -half, (c & 2) ? half : -half, (c & 4) ? z1 : z0});
        p.insert(p.end(), {float(q[0]), float(q[1]), float(q[2])});
    }
    const std::vector<std::uint32_t> idx{0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
                                         2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5};
    return meshkernel::fromTriangles(p, idx);
}

}  // namespace

MeshSolid domed(const MeshSolid& piece, const Dome& dome, bool masked, double fit)
{
    // The ball the dome is a slice of: its foot `radius` across where it meets the cut.
    const double h = dome.height, r = dome.radius;
    const double big = (r * r + h * h) / (2.0 * h);
    const Vec3 n = unit3(dome.normal, {0, 0, 1});
    const Vec3 x = unit3(cross3(std::abs(n[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}, n), {1, 0, 0});
    const Vec3 y = cross3(n, x);
    Transform frame;
    for (int row = 0; row < 3; ++row) {
        frame.m[row][0] = x[std::size_t(row)];
        frame.m[row][1] = y[std::size_t(row)];
        frame.m[row][2] = n[std::size_t(row)];
        frame.m[row][3] = dome.centre[std::size_t(row)];
    }
    const int segments = std::clamp(int(big * 24.0), 48, 160);
    if (masked) {
        // Standing out of the cut: the ball's top, from a hair under the cut (so it joins).
        const MeshSolid cap = meshkernel::intersected(
            {ball(add(dome.centre, mul(n, h - big)), big, segments), slab(frame, r + 0.5, -0.02, h + 0.5)});
        return meshkernel::merged({piece, cap});
    }
    const double g = big + fit;
    const MeshSolid hollow = meshkernel::intersected(
        {ball(add(dome.centre, mul(n, h - big)), g, segments), slab(frame, r + fit + 0.5, -0.5, h + fit + 0.5)});
    return meshkernel::subtracted(piece, {hollow});
}

double keySize(double room)
{
    const double size = std::min(0.3 * room, 3.0);
    return size >= 0.5 ? size : 0.0;
}

double keyLength(double size)
{
    return std::clamp(1.5 * size, 1.0, 4.0);
}

MeshSolid keyed(const Mesh& piece, const std::vector<KeySpot>& spots, bool masked, double fit)
{
    const MeshSolid solid = meshkernel::fromTriangles(piece.positions, piece.indices);
    std::vector<MeshSolid> keys;
    for (const KeySpot& s : spots) {
        const double size = keySize(s.room);
        if (size <= 0.0)
            continue;
        // The key's frame: X across the cap, Z out of the masked piece into the rest.
        const Vec3 y = cross3(s.normal, s.across);
        Transform frame;
        for (int r = 0; r < 3; ++r) {
            frame.m[r][0] = s.across[std::size_t(r)];
            frame.m[r][1] = y[std::size_t(r)];
            frame.m[r][2] = s.normal[std::size_t(r)];
            frame.m[r][3] = s.point[std::size_t(r)];
        }
        const double length = keyLength(size);
        if (masked)
            keys.push_back(keyPrism(frame, size, -length, length));  // half in, half standing out
        else
            keys.push_back(keyPrism(frame, size + 2.0 * fit, -0.5, length + fit));
    }
    if (keys.empty())
        return solid;
    if (masked) {
        keys.insert(keys.begin(), solid);
        return meshkernel::merged(keys);
    }
    return meshkernel::subtracted(solid, keys);
}

std::string describe(const MaskSplitFinish& f)
{
    const std::size_t lassos = std::size_t(std::count_if(f.dabs.begin(), f.dabs.end(),
                                                         [](const MaskDab& d) { return d.isLasso(); }));
    const std::size_t n = f.dabs.size() - lassos;
    std::string what;
    if (n > 0 || lassos == 0)
        what = std::to_string(n) + (n == 1 ? " dab" : " dabs");
    if (lassos > 0)
        what += (what.empty() ? "" : ", ") + std::to_string(lassos) + (lassos == 1 ? " lasso" : " lassos");
    return std::string(f.masked ? "the masked part" : "the rest") + " (" + what + (f.cut == MaskCut::Flat ? ", flat" : f.cut == MaskCut::Dome ? ", ball and socket" : "") +
           (f.keys ? ", keyed)" : ")");
}

}  // namespace mask

namespace {

const Body& maskStep(const Document& doc, ObjectId result)
{
    const Body* b = doc.find(result);
    if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::MaskSplit || !b->boolean->maskSplit)
        throw std::invalid_argument("Not a shape split by a mask");
    return *b;
}

std::vector<MaskDab> moved(const std::vector<MaskDab>& dabs, const Transform& t)
{
    std::vector<MaskDab> out;
    out.reserve(dabs.size());
    for (const MaskDab& d : dabs)
        out.push_back(mask::movedDab(d, t));
    return out;
}

}  // namespace

BooleanSpec maskSplitSpec(const Document& doc, ObjectId target, const std::vector<MaskDab>& dabs, bool masked,
                          double keyFit, MaskCut cut, double fit)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (dabs.empty())
        throw std::invalid_argument("Paint the mask first.");
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    BooleanSpec spec;
    spec.op = BooleanOp::MaskSplit;
    spec.operands = {target};
    spec.maskSplit = MaskSplitFinish{moved(dabs, toStep), masked};
    if (!(fit >= 0.0 && fit <= 1.0))
        throw std::invalid_argument("The socket's fit must be from 0 to 1 mm.");
    spec.maskSplit->cut = cut;
    spec.maskSplit->fit = fit;
    if (keyFit >= 0.0 && cut != MaskCut::Dome) {  // the dome lines them up
        if (keyFit > 1.0)
            throw std::invalid_argument("The keys' fit must be from 0 to 1 mm.");
        spec.maskSplit->keys = true;
        spec.maskSplit->keyFit = keyFit;
    }
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

std::vector<ObjectId> splitByMask(CommandStack& stack, Document& doc, ObjectId id, const std::vector<MaskDab>& dabs,
                                  SplitKeep keep, double keyFit, MaskCut cut, double fit)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!doc.isTopLevel(id))
        throw std::invalid_argument("Only a shape on the plate can be split");
    // Both pieces worked out first: refuse before changing anything.
    BooleanSpec front = maskSplitSpec(doc, id, dabs, true, keyFit, cut, fit),
                back = maskSplitSpec(doc, id, dabs, false, keyFit, cut, fit);
    const Document::Preview frontShape = doc.previewResult(front), backShape = doc.previewResult(back);
    if (frontShape.empty || backShape.empty)
        throw std::invalid_argument("The mask doesn't cut '" + body->name + "' in two.");
    const std::string base = body->name;
    const auto makeResult = [&](BooleanSpec spec, const Document::Preview& preview, const std::string& side) {
        const auto c = preview.bounds.center();
        spec.pivot = {c[0], c[1], c[2]};
        Body r;
        r.id = doc.allocateId();
        r.name = base + " (" + side + ")";
        r.boolean = std::move(spec);
        return r;
    };
    std::vector<ObjectId> made;
    stack.beginMacro("Split " + base + " by mask");
    try {
        if (keep == SplitKeep::Both) {
            const std::vector<Body> copies = duplicateTree(doc, id);
            for (const Body& c : copies)
                stack.execute(std::make_unique<AddBodyCommand>(c));
            back.operands = {copies.back().id};
            Body a = makeResult(front, frontShape, "masked");
            Body b = makeResult(back, backShape, "rest");
            made = {a.id, b.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(a)));
            stack.execute(std::make_unique<CombineCommand>(std::move(b)));
        } else {
            const bool keepMasked = keep == SplitKeep::Front;
            Body r = keepMasked ? makeResult(front, frontShape, "masked") : makeResult(back, backShape, "rest");
            made = {r.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

std::vector<MaskDab> maskDabs(const Document& doc, ObjectId result)
{
    const Body& b = maskStep(doc, result);
    return moved(b.boolean->maskSplit->dabs, doc.worldTransform(result));
}

std::vector<Body> withMask(const Document& doc, ObjectId result, const std::vector<MaskDab>& dabs,
                           std::optional<double> keyFit, std::optional<std::pair<MaskCut, double>> cut)
{
    const Body& b = maskStep(doc, result);
    if (dabs.empty())
        throw std::invalid_argument("Paint the mask first.");
    const std::vector<MaskDab> local = moved(dabs, doc.worldTransform(result).inverse());
    std::vector<Body> out;
    if (keyFit && *keyFit > 1.0)
        throw std::invalid_argument("The keys' fit must be from 0 to 1 mm.");
    if (cut && !(cut->second >= 0.0 && cut->second <= 1.0))
        throw std::invalid_argument("The socket's fit must be from 0 to 1 mm.");
    const auto setKeys = [&keyFit, &cut](MaskSplitFinish& f) {
        if (cut) {
            f.cut = cut->first;
            f.fit = cut->second;
        }
        if (!keyFit)
            return;
        f.keys = *keyFit >= 0.0 && f.cut != MaskCut::Dome;
        if (f.keys)
            f.keyFit = *keyFit;
    };
    Body next = b;
    next.boolean->maskSplit->dabs = local;
    setKeys(*next.boolean->maskSplit);
    out.push_back(next);
    // Its other piece, if it's still on the plate with the same mask: changed alike (the
    // dabs are in the same place on the part, wherever the piece has been moved since).
    const MaskSplitFinish& old = *b.boolean->maskSplit;
    for (const Body& other : doc.bodies()) {
        if (other.id == result || !doc.isTopLevel(other.id) || !other.isBoolean() ||
            other.boolean->op != BooleanOp::MaskSplit || !other.boolean->maskSplit)
            continue;
        const MaskSplitFinish& f = *other.boolean->maskSplit;
        if (f.masked != old.masked && f.dabs == old.dabs) {
            Body o = other;
            o.boolean->maskSplit->dabs = local;
            setKeys(*o.boolean->maskSplit);
            out.push_back(o);
            break;
        }
    }
    return out;
}

}  // namespace forge
