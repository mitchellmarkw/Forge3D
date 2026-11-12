// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Emboss.h"

#include "forge/core/Document.h"
#include "forge/core/Export.h"
#include "forge/core/Text.h"

#include <algorithm>
#include <numbers>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace forge {

namespace {

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a)
{
    const double l = length(a);
    if (!(l > 1e-12))
        throw std::invalid_argument("A direction is needed");
    return mul(a, 1.0 / l);
}

struct Surface {
    const std::vector<float>& p;
    const std::vector<std::uint32_t>& idx;
    Vec3 vertex(std::uint32_t i) const { return {p[3 * i], p[3 * i + 1], p[3 * i + 2]}; }
    std::size_t triangles() const { return idx.size() / 3; }
    Vec3 corner(std::size_t t, int k) const { return vertex(idx[3 * t + k]); }
    // Not normalised: length = twice the area.
    Vec3 areaNormal(std::size_t t) const
    {
        const Vec3 a = corner(t, 0);
        return cross(sub(corner(t, 1), a), sub(corner(t, 2), a));
    }
};

// Möller–Trumbore; t along the ray, or negative for a miss.
double rayTriangle(const Vec3& o, const Vec3& d, const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 e1 = sub(b, a), e2 = sub(c, a);
    const Vec3 pv = cross(d, e2);
    const double det = dot(e1, pv);
    if (std::abs(det) < 1e-14)
        return -1.0;
    const double inv = 1.0 / det;
    const Vec3 s = sub(o, a);
    const double u = dot(s, pv) * inv;
    if (u < 0.0 || u > 1.0)
        return -1.0;
    const Vec3 q = cross(s, e1);
    const double v = dot(d, q) * inv;
    if (v < 0.0 || u + v > 1.0)
        return -1.0;
    return dot(e2, q) * inv;
}

constexpr double kCrease = 0.82;  // cos 35°: faces bent more than this are a corner, not a curve

}  // namespace

namespace emboss {

std::optional<SurfacePoint> surfaceAt(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices,
                                      const Vec3& origin, const Vec3& direction, double radius)
{
    const Surface s{positions, indices};
    const Vec3 d = unit(direction);
    double best = std::numeric_limits<double>::infinity();
    std::size_t hitTri = 0;
    for (std::size_t t = 0; t < s.triangles(); ++t) {
        const double hit = rayTriangle(origin, d, s.corner(t, 0), s.corner(t, 1), s.corner(t, 2));
        if (hit > 1e-9 && hit < best) {
            best = hit;
            hitTri = t;
        }
    }
    if (!std::isfinite(best))
        return std::nullopt;
    SurfacePoint out;
    out.point = add(origin, mul(d, best));
    const Vec3 own = unit(s.areaNormal(hitTri));
    // Average the facing of the nearby surface that bends smoothly from the hit.
    Vec3 sum{0, 0, 0};
    const double r2 = radius * radius;
    for (std::size_t t = 0; t < s.triangles(); ++t) {
        const Vec3 c = mul(add(add(s.corner(t, 0), s.corner(t, 1)), s.corner(t, 2)), 1.0 / 3.0);
        const Vec3 off = sub(c, out.point);
        if (dot(off, off) > r2 && t != hitTri)
            continue;
        const Vec3 n = s.areaNormal(t);
        const double l = length(n);
        if (l > 0.0 && dot(mul(n, 1.0 / l), own) > kCrease)
            sum = add(sum, n);
    }
    out.normal = length(sum) > 0.0 ? unit(sum) : own;
    return out;
}

Vec3 upFor(const Vec3& normal)
{
    const Vec3 n = unit(normal);
    const Vec3 pick = std::abs(n[2]) < 0.95 ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    return unit(sub(pick, mul(n, dot(pick, n))));
}

MeshSolid letters(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices, const Vec3& point,
                  const Vec3& normal, const EmbossText& words)
{
    if (!(words.depth > 0.0))
        throw std::invalid_argument("The depth must be greater than zero");
    const Surface s{positions, indices};
    const Vec3 n = unit(normal);
    const Vec3 up = upFor(n);
    const Vec3 right = cross(up, n);

    const double maxEdge = std::max(words.letterHeight / 25.0, 0.05);
    const std::vector<text::FlatPiece> pieces = text::flatLetters(words, maxEdge);
    double reach = 0.0;
    for (const auto& piece : pieces)
        for (const auto& q : piece.points)
            reach = std::max(reach, std::hypot(q[0], q[1]));
    reach += maxEdge;

    // Only triangles near the spot matter: letters measured along the surface never
    // get further from it than they are long.
    const double radius = 1.5 * reach + maxEdge;
    std::vector<std::uint32_t> nearby;
    for (std::size_t t = 0; t < s.triangles(); ++t) {
        double d2 = 0.0;
        for (int axis = 0; axis < 3; ++axis) {
            double lo = 1e300, hi = -1e300;
            for (int k = 0; k < 3; ++k) {
                lo = std::min(lo, s.corner(t, k)[axis]);
                hi = std::max(hi, s.corner(t, k)[axis]);
            }
            const double c = point[axis];
            const double gap = c < lo ? lo - c : (c > hi ? c - hi : 0.0);
            d2 += gap * gap;
        }
        if (d2 <= radius * radius)
            nearby.push_back(std::uint32_t(t));
    }

    // Each point's surface facing, blended across the triangle from its corners. A
    // corner's facing averages the faces around it that bend smoothly from this one,
    // so letters wrap around curves but stay square to the faces of a box.
    // (vertex, triangle) pairs of the nearby triangles, sorted: the faces round a corner.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> facesAt;
    facesAt.reserve(nearby.size() * 3);
    for (std::uint32_t t : nearby)
        for (int k = 0; k < 3; ++k)
            facesAt.emplace_back(indices[3 * t + k], t);
    std::sort(facesAt.begin(), facesAt.end());
    std::unordered_map<std::uint64_t, Vec3> cornerNormals;
    const auto cornerNormal = [&](std::uint32_t t, int k) {
        const std::uint64_t key = std::uint64_t(t) * 3 + std::uint64_t(k);
        if (const auto it = cornerNormals.find(key); it != cornerNormals.end())
            return it->second;
        const Vec3 own = unit(s.areaNormal(t));
        Vec3 sum{0, 0, 0};
        const std::uint32_t corner = indices[3 * t + k];
        const auto first = std::lower_bound(facesAt.begin(), facesAt.end(), std::make_pair(corner, std::uint32_t(0)));
        for (auto it = first; it != facesAt.end() && it->first == corner; ++it) {
            const std::uint32_t g = it->second;
            const Vec3 an = s.areaNormal(g);
            const double l = length(an);
            if (l > 0.0 && dot(mul(an, 1.0 / l), own) > kCrease)
                sum = add(sum, an);
        }
        const Vec3 result = length(sum) > 0.0 ? unit(sum) : own;
        cornerNormals.emplace(key, result);
        return result;
    };

    // A line drawn on the surface: where a plane cuts it, measured by distance along the
    // surface from a starting spot (negative one way, positive the other).
    struct Curve {
        std::vector<double> s;
        std::vector<Vec3> p, n;
        std::vector<std::uint32_t> segTri;  // segTri[j]: the triangle the piece from point j-1 to j crosses
        double loop = 0.0;                   // the length of the way round, if the cut closes on itself
        // Where the line is `x` along, its facing, and the triangle there.
        bool at(double x, Vec3& outP, Vec3& outN, std::uint32_t* tri = nullptr) const
        {
            if (s.size() < 2 || x < s.front() - 1e-9 || x > s.back() + 1e-9)
                return false;
            std::size_t j = std::size_t(std::upper_bound(s.begin(), s.end(), x) - s.begin());
            j = std::clamp<std::size_t>(j, 1, s.size() - 1);
            const double span = s[j] - s[j - 1];
            const double f = span > 0.0 ? std::clamp((x - s[j - 1]) / span, 0.0, 1.0) : 0.0;
            outP = add(mul(p[j - 1], 1.0 - f), mul(p[j], f));
            outN = add(mul(n[j - 1], 1.0 - f), mul(n[j], f));
            if (tri)
                *tri = segTri[j];
            return true;
        }
    };
    const auto edgeKey = [&indices](std::uint32_t t, int a, int b) {
        const std::uint32_t x = indices[3 * t + a], y = indices[3 * t + b];
        return x < y ? (std::uint64_t(x) << 32 | y) : (std::uint64_t(y) << 32 | x);
    };
    // Which nearby triangles share each edge (sorted, for lookups).
    std::vector<std::pair<std::uint64_t, std::uint32_t>> edgeTris;
    edgeTris.reserve(nearby.size() * 3);
    for (std::uint32_t t : nearby)
        for (int k = 0; k < 3; ++k)
            edgeTris.emplace_back(edgeKey(t, k, (k + 1) % 3), t);
    std::sort(edgeTris.begin(), edgeTris.end());
    const auto across = [&](std::uint64_t edge, std::uint32_t from) -> std::int64_t {
        auto it = std::lower_bound(edgeTris.begin(), edgeTris.end(), std::make_pair(edge, std::uint32_t(0)));
        for (; it != edgeTris.end() && it->first == edge; ++it)
            if (it->second != from)
                return it->second;
        return -1;  // the surface ends (or leaves the nearby part)
    };

    // Cuts the surface with the plane through q facing planeN and follows the cut from q
    // both ways (positive along dir), triangle to neighbouring triangle, until `limit`
    // is covered or the surface ends. startTri: a triangle at q, if known.
    const auto slice = [&](const Vec3& q, const Vec3& planeN, const Vec3& dir, double limit,
                           std::int64_t startTri) -> Curve {
        const auto side = [&](std::uint32_t t, int k) {
            const double d = dot(sub(s.corner(t, k), q), planeN);
            return d == 0.0 ? 1e-12 : d;  // a corner exactly on the plane counts as just above it
        };
        // The cut through triangle t: crossing edges (as corner pairs) and points.
        struct Cross {
            int count = 0;
            int a[2]{}, b[2]{};
            Vec3 p[2], n[2];
        };
        const auto cut = [&](std::uint32_t t) {
            Cross c;
            const double d[3] = {side(t, 0), side(t, 1), side(t, 2)};
            for (int k = 0; k < 3 && c.count < 2; ++k) {
                const int a = k, b = (k + 1) % 3;
                if ((d[a] < 0.0) == (d[b] < 0.0))
                    continue;
                const double f = d[a] / (d[a] - d[b]);
                c.a[c.count] = a;
                c.b[c.count] = b;
                c.p[c.count] = add(mul(s.corner(t, a), 1.0 - f), mul(s.corner(t, b), f));
                c.n[c.count] = add(mul(cornerNormal(t, a), 1.0 - f), mul(cornerNormal(t, b), f));
                ++c.count;
            }
            return c;
        };
        const auto distanceToCut = [&](std::uint32_t t, double& fOut) {
            const Cross c = cut(t);
            if (c.count != 2)
                return 1e300;
            const Vec3 ab = sub(c.p[1], c.p[0]);
            const double l2 = dot(ab, ab);
            const double f = l2 > 0.0 ? std::clamp(dot(sub(q, c.p[0]), ab) / l2, 0.0, 1.0) : 0.0;
            fOut = f;
            const Vec3 d = sub(q, add(c.p[0], mul(ab, f)));
            return dot(d, d);
        };
        Curve curve;
        // The triangle to start in: the one given, or else the nearest one the plane cuts.
        double f0 = 0.0;
        std::int64_t start = -1;
        if (startTri >= 0 && distanceToCut(std::uint32_t(startTri), f0) < 1e299)
            start = startTri;
        if (start < 0) {
            double best = 1e300;
            for (std::uint32_t t : nearby) {
                double f = 0.0;
                const double d = distanceToCut(t, f);
                if (d < best) {
                    best = d;
                    start = t;
                    f0 = f;
                }
            }
            if (start < 0)
                return curve;
        }
        const auto t0 = std::uint32_t(start);
        const Cross c0 = cut(t0);
        const Vec3 s0 = add(mul(c0.p[0], 1.0 - f0), mul(c0.p[1], f0));
        const Vec3 n0 = add(mul(c0.n[0], 1.0 - f0), mul(c0.n[1], f0));
        struct Pt {
            double s;
            Vec3 p, n;
            std::uint32_t via;  // the triangle crossed to get here from the point nearer q
        };
        std::vector<Pt> points{{0.0, s0, n0, t0}};
        double loop = 0.0;
        for (const double sign : {1.0, -1.0}) {
            std::uint32_t t = t0;
            Cross c = c0;
            int exit = dot(sub(c0.p[1], c0.p[0]), dir) * sign >= 0.0 ? 1 : 0;
            Vec3 cur = s0;
            double walked = 0.0;
            for (std::size_t guard = 0; guard < nearby.size() + 2; ++guard) {
                walked += length(sub(c.p[exit], cur));
                cur = c.p[exit];
                points.push_back({sign * walked, cur, c.n[exit], t});
                if (walked >= limit)
                    break;
                const std::uint64_t edge = edgeKey(t, c.a[exit], c.b[exit]);
                const std::int64_t next = across(edge, t);
                if (next < 0)
                    break;
                t = std::uint32_t(next);
                if (t == t0) {
                    // All the way round, back where it started.
                    loop = walked + length(sub(s0, cur));
                    break;
                }
                c = cut(t);
                if (c.count != 2)
                    break;
                // Leave by the other crossing edge.
                exit = edgeKey(t, c.a[0], c.b[0]) == edge ? 1 : 0;
            }
        }
        std::sort(points.begin(), points.end(), [](const Pt& x, const Pt& y) { return x.s < y.s; });
        for (std::size_t i = 0; i < points.size(); ++i) {
            const Pt& pt = points[i];
            if (!curve.s.empty() && pt.s - curve.s.back() < 1e-12)
                continue;
            // The piece from the previous point to this one: crossed on the way out to
            // this point (ahead of q), or on the way out to the previous one (behind q).
            const std::uint32_t seg = pt.s > 0.0 ? pt.via : (i > 0 ? points[i - 1].via : pt.via);
            curve.s.push_back(pt.s);
            curve.p.push_back(pt.p);
            curve.n.push_back(pt.n);
            curve.segTri.push_back(seg);
        }
        curve.loop = loop;
        return curve;
    };

    const auto hangsOff = [] {
        return GeometryError("Some of the letters would hang off the edge of the surface. Make the text smaller "
                             "or shorter, or move it.");
    };
    // The letters' extent.
    double u0 = 1e300, u1 = -1e300, v0 = 1e300, v1 = -1e300;
    for (const auto& piece : pieces)
        for (const auto& q : piece.points) {
            u0 = std::min(u0, q[0]);
            u1 = std::max(u1, q[0]);
            v0 = std::min(v0, q[1]);
            v1 = std::max(v1, q[1]);
        }
    // Like smoothing on a sticker: the baseline runs across the surface through the spot,
    // measured along it; from each step along it a line runs straight up the surface
    // (square to the baseline). A letter point (u, v) goes u along the baseline, then v up
    // that line. Long words follow curves round; the short way (up) bends with the
    // surface too, so letters keep their size on a ball as well as on a cylinder.
    const Curve baseline = slice(point, up, right, std::max(std::abs(u0), std::abs(u1)) + maxEdge, -1);
    const auto tooLongForLoop = [](const Curve& c, double span) { return c.loop > 0.0 && span > c.loop - 1e-6; };
    if (tooLongForLoop(baseline, u1 - u0))
        throw GeometryError("The words are longer than the way round the shape, so they would run into "
                            "themselves. Make them smaller or shorter.");
    const double step = maxEdge;
    const int columnCount = std::max(2, int(std::ceil((u1 - u0) / step)) + 1);
    const double limitUp = std::max(std::abs(v0), std::abs(v1)) + maxEdge;
    std::vector<Curve> columns;
    columns.reserve(std::size_t(columnCount));
    for (int k = 0; k < columnCount; ++k) {
        const double u = u0 + k * step;
        Vec3 q, qn, a, an, b, bn;
        std::uint32_t tri = 0;
        if (!baseline.at(u, q, qn, &tri))
            throw hangsOff();
        const double h = step * 0.25;
        Vec3 tangent = right;
        if (baseline.at(u + h, b, bn) && baseline.at(u - h, a, an))
            tangent = sub(b, a);
        else if (baseline.at(u + h, b, bn))
            tangent = sub(b, q);
        else if (baseline.at(u - h, a, an))
            tangent = sub(q, a);
        tangent = length(tangent) > 0.0 ? unit(tangent) : right;
        const Vec3 qnUnit = length(qn) > 0.0 ? unit(qn) : n;
        Vec3 upHere = cross(qnUnit, tangent);
        upHere = length(upHere) > 1e-9 ? unit(upHere) : up;
        columns.push_back(slice(q, tangent, upHere, limitUp, tri));
        if (tooLongForLoop(columns.back(), v1 - v0))
            throw GeometryError("The letters are taller than the way round the shape. Make them smaller.");
    }
    const auto place = [&](double u, double v, Vec3& at, Vec3& facing) {
        const double x = (u - u0) / step;
        const int k = std::clamp(int(std::floor(x)), 0, columnCount - 2);
        const double f = std::clamp(x - k, 0.0, 1.0);
        Vec3 pa, na, pb, nb;
        if (!columns[std::size_t(k)].at(v, pa, na) || !columns[std::size_t(k) + 1].at(v, pb, nb))
            throw hangsOff();
        at = add(mul(pa, 1.0 - f), mul(pb, f));
        facing = add(mul(na, 1.0 - f), mul(nb, f));
    };

    const double overlap = std::clamp(words.depth * 0.3, 0.05, 0.5);
    const double out = words.raised ? words.depth : overlap;
    const double in = words.raised ? overlap : words.depth;

    std::vector<MeshSolid> solids;
    for (const auto& piece : pieces) {
        const std::size_t count = piece.points.size();
        std::vector<float> pos(count * 6);  // tops, then bottoms
        for (std::size_t i = 0; i < count; ++i) {
            const double u = piece.points[i][0], v = piece.points[i][1];
            Vec3 at{}, facing{};
            place(u, v, at, facing);
            const Vec3 f = length(facing) > 0.0 ? unit(facing) : n;
            const Vec3 top = add(at, mul(f, out)), bottom = sub(at, mul(f, in));
            for (int k = 0; k < 3; ++k) {
                pos[3 * i + k] = float(top[k]);
                pos[3 * (count + i) + k] = float(bottom[k]);
            }
        }
        // Tops as laid out, bottoms turned over, and a wall along every outline edge.
        std::vector<std::uint32_t> tri;
        std::unordered_map<std::uint64_t, int> edgeUse;
        const auto key = [](std::uint32_t a, std::uint32_t b) {
            return a < b ? (std::uint64_t(a) << 32 | b) : (std::uint64_t(b) << 32 | a);
        };
        for (std::size_t t = 0; t + 2 < piece.triangles.size(); t += 3) {
            const std::uint32_t a = piece.triangles[t], b = piece.triangles[t + 1], c = piece.triangles[t + 2];
            tri.insert(tri.end(), {a, b, c});
            const auto lo = std::uint32_t(count);
            tri.insert(tri.end(), {a + lo, c + lo, b + lo});
            for (const auto& [x, y] : {std::pair{a, b}, std::pair{b, c}, std::pair{c, a}})
                ++edgeUse[key(x, y)];
        }
        for (std::size_t t = 0; t + 2 < piece.triangles.size(); t += 3)
            for (int e = 0; e < 3; ++e) {
                const std::uint32_t a = piece.triangles[t + e], b = piece.triangles[t + (e + 1) % 3];
                if (edgeUse[key(a, b)] != 1)
                    continue;
                const auto lo = std::uint32_t(count);
                tri.insert(tri.end(), {a + lo, b + lo, b});
                tri.insert(tri.end(), {a + lo, b, a});
            }
        // Bent hard enough, a piece can come out inside out: turn it if so.
        double volume6 = 0.0;
        for (std::size_t t = 0; t + 2 < tri.size(); t += 3) {
            const Vec3 a{pos[3 * tri[t]], pos[3 * tri[t] + 1], pos[3 * tri[t] + 2]};
            const Vec3 b{pos[3 * tri[t + 1]], pos[3 * tri[t + 1] + 1], pos[3 * tri[t + 1] + 2]};
            const Vec3 c{pos[3 * tri[t + 2]], pos[3 * tri[t + 2] + 1], pos[3 * tri[t + 2] + 2]};
            volume6 += dot(a, cross(b, c));
        }
        if (volume6 < 0.0)
            for (std::size_t t = 0; t + 2 < tri.size(); t += 3)
                std::swap(tri[t + 1], tri[t + 2]);
        solids.push_back(meshkernel::fromTriangles(pos, tri));
    }
    return solids.size() == 1 ? solids.front() : meshkernel::merged(solids);
}


std::optional<SurfacePoint> centeredOnFace(const std::vector<float>& positions,
                                           const std::vector<std::uint32_t>& indices, const SurfacePoint& spot,
                                           double angleDegrees, Center how, double radius)
{
    const Surface s{positions, indices};
    const Vec3 n = unit(spot.normal);
    // The triangle at the spot: where a short ray down onto it lands.
    std::int64_t seed = -1;
    {
        double best = 1e300;
        const double lift = 1e-3 + radius * 1e-3;
        const Vec3 from = add(spot.point, mul(n, lift));
        for (std::size_t t = 0; t < s.triangles(); ++t) {
            const double hit = rayTriangle(from, mul(n, -1.0), s.corner(t, 0), s.corner(t, 1), s.corner(t, 2));
            if (hit > -1e-9 && std::abs(hit - lift) < best) {
                best = std::abs(hit - lift);
                seed = std::int64_t(t);
            }
        }
        if (seed < 0)
            return std::nullopt;
    }
    // The face: spread across edges where the surface bends less than 35°.
    std::vector<std::pair<std::uint64_t, std::uint32_t>> edges;
    edges.reserve(indices.size());
    for (std::size_t t = 0; t < s.triangles(); ++t)
        for (int k = 0; k < 3; ++k) {
            std::uint64_t a = indices[3 * t + k], b = indices[3 * t + (k + 1) % 3];
            if (a > b)
                std::swap(a, b);
            edges.emplace_back(a << 32 | b, std::uint32_t(t));
        }
    std::sort(edges.begin(), edges.end());
    std::vector<char> inFace(s.triangles(), 0);
    std::vector<std::uint32_t> face{std::uint32_t(seed)}, todo{std::uint32_t(seed)};
    inFace[std::size_t(seed)] = 1;
    while (!todo.empty()) {
        const std::uint32_t t = todo.back();
        todo.pop_back();
        const Vec3 nt = s.areaNormal(t);
        const double lt = length(nt);
        for (int k = 0; k < 3; ++k) {
            std::uint64_t a = indices[3 * t + k], b = indices[3 * t + (k + 1) % 3];
            if (a > b)
                std::swap(a, b);
            const std::uint64_t key = a << 32 | b;
            for (auto it = std::lower_bound(edges.begin(), edges.end(), std::make_pair(key, std::uint32_t(0)));
                 it != edges.end() && it->first == key; ++it) {
                const std::uint32_t o = it->second;
                if (inFace[o])
                    continue;
                const Vec3 no = s.areaNormal(o);
                const double lo = length(no);
                if (lt > 0 && lo > 0 && dot(nt, no) / (lt * lo) > kCrease) {
                    inFace[o] = 1;
                    face.push_back(o);
                    todo.push_back(o);
                }
            }
        }
    }
    // How far the face reaches across and up, as the words read.
    const Vec3 up0 = upFor(n), right0 = cross(up0, n);
    const double a = angleDegrees * std::numbers::pi / 180.0;
    const Vec3 right = add(mul(right0, std::cos(a)), mul(up0, std::sin(a)));
    const Vec3 up = add(mul(right0, -std::sin(a)), mul(up0, std::cos(a)));
    double u0 = 1e300, u1 = -1e300, v0 = 1e300, v1 = -1e300, w1 = -1e300;
    for (std::uint32_t t : face)
        for (int k = 0; k < 3; ++k) {
            const Vec3 d = sub(s.corner(t, k), spot.point);
            u0 = std::min(u0, dot(d, right));
            u1 = std::max(u1, dot(d, right));
            v0 = std::min(v0, dot(d, up));
            v1 = std::max(v1, dot(d, up));
            w1 = std::max(w1, dot(d, n));
        }
    const double du = how == Center::TopBottom ? 0.0 : (u0 + u1) / 2.0;
    const double dv = how == Center::LeftRight ? 0.0 : (v0 + v1) / 2.0;
    // Down onto the face from above that middle.
    std::vector<std::uint32_t> faceIdx;
    faceIdx.reserve(face.size() * 3);
    for (std::uint32_t t : face)
        faceIdx.insert(faceIdx.end(), {indices[3 * t], indices[3 * t + 1], indices[3 * t + 2]});
    const Vec3 above = add(add(add(spot.point, mul(right, du)), mul(up, dv)), mul(n, std::max(w1, 0.0) + 1.0));
    return surfaceAt(positions, faceIdx, above, mul(n, -1.0), radius);
}

}  // namespace emboss

namespace {

// The surface of `id` as triangles in its own coordinates, and the matching transform
// to the plate.
void surfaceOf(const Document& doc, ObjectId id, std::vector<float>& pos, std::vector<std::uint32_t>& idx)
{
    meshkernel::triangles(doc.meshSolid(id), pos, idx);
}

ObjectId embossedPart(const Document& doc, ObjectId id)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (body->isBoolean() && body->boolean->op == BooleanOp::Emboss)
        return body->boolean->operands.front();
    return id;
}

}  // namespace

std::optional<emboss::SurfacePoint> surfaceUnder(const Document& doc, ObjectId id, const Vec3& origin,
                                                 const Vec3& direction, double radius)
{
    const ObjectId part = embossedPart(doc, id);
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    surfaceOf(doc, part, pos, idx);
    const Transform toPlate = doc.worldTransform(part);
    const Transform toLocal = toPlate.inverse();
    const auto hit =
        emboss::surfaceAt(pos, idx, toLocal.apply(origin), toLocal.applyDirection(direction), radius);
    if (!hit)
        return std::nullopt;
    return emboss::SurfacePoint{toPlate.apply(hit->point), unit(toPlate.applyDirection(hit->normal))};
}

BooleanSpec embossSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                       const EmbossText& words)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    BooleanSpec spec;
    spec.op = BooleanOp::Emboss;
    spec.operands = {target};
    // The new step sits where its part is: in the coordinates of whatever holds the part
    // (the plate for a shape on it; the next step up when added inside a history).
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.planePoint = toStep.apply(where.point);
    spec.planeNormal = unit(toStep.applyDirection(where.normal));
    spec.emboss = words;
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

std::optional<emboss::SurfacePoint> centeredSpot(const Document& doc, ObjectId id, const emboss::SurfacePoint& spot,
                                                 const EmbossText& words, emboss::Center how)
{
    const ObjectId part = embossedPart(doc, id);
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    surfaceOf(doc, part, pos, idx);
    const Transform toPlate = doc.worldTransform(part);
    const Transform toLocal = toPlate.inverse();
    const emboss::SurfacePoint local{toLocal.apply(spot.point), unit(toLocal.applyDirection(spot.normal))};
    const auto moved = emboss::centeredOnFace(pos, idx, local, words.angle, how, words.letterHeight / 2.0);
    if (!moved)
        return std::nullopt;
    return emboss::SurfacePoint{toPlate.apply(moved->point), unit(toPlate.applyDirection(moved->normal))};
}

Body embossBody(Document& doc, ObjectId target, const emboss::SurfacePoint& where, const EmbossText& words)
{
    Body result;
    result.boolean = embossSpec(doc, target, where, words);
    result.id = doc.allocateId();
    result.name = doc.uniqueName("Emboss");
    return result;
}

emboss::SurfacePoint embossSpot(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Emboss)
        throw std::invalid_argument("Not an embossed shape");
    const Transform t = doc.worldTransform(result);
    return {t.apply(body->boolean->planePoint), unit(t.applyDirection(body->boolean->planeNormal))};
}

Body withEmboss(const Document& doc, ObjectId result, const emboss::SurfacePoint& where, const EmbossText& words)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Emboss)
        throw std::invalid_argument("Not an embossed shape");
    const Transform toLocal = doc.worldTransform(result).inverse();
    Body next = *body;
    next.boolean->planePoint = toLocal.apply(where.point);
    next.boolean->planeNormal = unit(toLocal.applyDirection(where.normal));
    next.boolean->emboss = words;
    return next;
}

}  // namespace forge
