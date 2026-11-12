// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Emboss.h"

#include "forge/core/Document.h"
#include "forge/core/Export.h"
#include "forge/core/Text.h"

#include <algorithm>
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
    std::vector<std::vector<std::uint32_t>> facesAt;  // built on first need
    const auto buildFacesAt = [&] {
        facesAt.assign(positions.size() / 3, {});
        for (std::size_t t = 0; t < s.triangles(); ++t)
            for (int k = 0; k < 3; ++k)
                facesAt[indices[3 * t + k]].push_back(std::uint32_t(t));
    };
    std::unordered_map<std::uint64_t, Vec3> cornerNormals;
    const auto cornerNormal = [&](std::uint32_t t, int k) {
        const std::uint64_t key = std::uint64_t(t) * 3 + std::uint64_t(k);
        if (const auto it = cornerNormals.find(key); it != cornerNormals.end())
            return it->second;
        if (facesAt.empty())
            buildFacesAt();
        const Vec3 own = unit(s.areaNormal(t));
        Vec3 sum{0, 0, 0};
        for (std::uint32_t g : facesAt[indices[3 * t + k]]) {
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
        bool at(double x, Vec3& outP, Vec3& outN) const
        {
            if (s.size() < 2 || x < s.front() - 1e-9 || x > s.back() + 1e-9)
                return false;
            std::size_t j = std::size_t(std::upper_bound(s.begin(), s.end(), x) - s.begin());
            j = std::clamp<std::size_t>(j, 1, s.size() - 1);
            const double span = s[j] - s[j - 1];
            const double f = span > 0.0 ? std::clamp((x - s[j - 1]) / span, 0.0, 1.0) : 0.0;
            outP = add(mul(p[j - 1], 1.0 - f), mul(p[j], f));
            outN = add(mul(n[j - 1], 1.0 - f), mul(n[j], f));
            return true;
        }
    };
    const auto edgeKey = [&indices](std::uint32_t t, int a, int b) {
        const std::uint32_t x = indices[3 * t + a], y = indices[3 * t + b];
        return x < y ? (std::uint64_t(x) << 32 | y) : (std::uint64_t(y) << 32 | x);
    };
    // Cuts the surface near q with the plane through q facing planeN and walks the cut
    // from q both ways (positive along dir) until `limit` is covered or the surface ends.
    const auto slice = [&](const Vec3& q, const Vec3& planeN, const Vec3& dir, double limit) -> Curve {
        struct Seg {
            std::uint64_t e[2];
            Vec3 p[2], n[2];
        };
        std::vector<Seg> segs;
        std::unordered_map<std::uint64_t, std::array<int, 2>> byEdge;
        for (std::uint32_t t : nearby) {
            double d[3];
            for (int k = 0; k < 3; ++k) {
                d[k] = dot(sub(s.corner(t, k), q), planeN);
                if (d[k] == 0.0)
                    d[k] = 1e-12;  // a corner exactly on the plane counts as just above it
            }
            Seg sg{};
            int count = 0;
            for (int k = 0; k < 3 && count < 2; ++k) {
                const int a = k, b = (k + 1) % 3;
                if ((d[a] < 0.0) == (d[b] < 0.0))
                    continue;
                const double f = d[a] / (d[a] - d[b]);
                sg.e[count] = edgeKey(t, a, b);
                sg.p[count] = add(mul(s.corner(t, a), 1.0 - f), mul(s.corner(t, b), f));
                sg.n[count] = add(mul(cornerNormal(t, a), 1.0 - f), mul(cornerNormal(t, b), f));
                ++count;
            }
            if (count != 2)
                continue;
            const int index = int(segs.size());
            segs.push_back(sg);
            for (int k = 0; k < 2; ++k) {
                auto [it, fresh] = byEdge.try_emplace(sg.e[k], std::array<int, 2>{-1, -1});
                (it->second[0] < 0 ? it->second[0] : it->second[1]) = index;
            }
        }
        Curve curve;
        // The piece of the cut nearest q (the sheet that was clicked).
        int start = -1;
        double bestD = 1e300, bestF = 0.0;
        for (std::size_t i = 0; i < segs.size(); ++i) {
            const Vec3 ab = sub(segs[i].p[1], segs[i].p[0]);
            const double l2 = dot(ab, ab);
            const double f = l2 > 0.0 ? std::clamp(dot(sub(q, segs[i].p[0]), ab) / l2, 0.0, 1.0) : 0.0;
            const Vec3 c = add(segs[i].p[0], mul(ab, f));
            const double dd = dot(sub(q, c), sub(q, c));
            if (dd < bestD) {
                bestD = dd;
                start = int(i);
                bestF = f;
            }
        }
        if (start < 0)
            return curve;
        const Seg& first = segs[std::size_t(start)];
        const Vec3 s0 = add(mul(first.p[0], 1.0 - bestF), mul(first.p[1], bestF));
        const Vec3 n0 = add(mul(first.n[0], 1.0 - bestF), mul(first.n[1], bestF));
        std::vector<std::tuple<double, Vec3, Vec3>> points{{0.0, s0, n0}};
        for (const double sign : {1.0, -1.0}) {
            int seg = start;
            // Leave by the end that lies the way we're walking.
            int end = dot(sub(first.p[1], first.p[0]), dir) * sign >= 0.0 ? 1 : 0;
            Vec3 cur = s0;
            double walked = 0.0;
            for (std::size_t guard = 0; guard < segs.size() + 2; ++guard) {
                const Seg& sg = segs[std::size_t(seg)];
                walked += length(sub(sg.p[end], cur));
                cur = sg.p[end];
                points.emplace_back(sign * walked, cur, sg.n[end]);
                if (walked >= limit)
                    break;
                const auto it = byEdge.find(sg.e[end]);
                const int other = it->second[0] == seg ? it->second[1] : it->second[0];
                if (other < 0)
                    break;  // the surface ends here
                const std::uint64_t via = sg.e[end];
                seg = other;
                end = segs[std::size_t(seg)].e[0] == via ? 1 : 0;
            }
        }
        std::sort(points.begin(), points.end(),
                  [](const auto& x, const auto& y) { return std::get<0>(x) < std::get<0>(y); });
        for (const auto& [ds, pp, nn] : points) {
            if (!curve.s.empty() && ds - curve.s.back() < 1e-12)
                continue;
            curve.s.push_back(ds);
            curve.p.push_back(pp);
            curve.n.push_back(nn);
        }
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
    // Up the surface from the spot, then a line across it at every step up: a letter
    // point (u, v) goes v along the first and u along its row, like a sticker smoothed on.
    const Curve upward = slice(point, right, up, std::max(std::abs(v0), std::abs(v1)) + maxEdge);
    const double step = maxEdge;
    const int rowCount = std::max(2, int(std::ceil((v1 - v0) / step)) + 1);
    const double limitAcross = std::max(std::abs(u0), std::abs(u1)) + maxEdge;
    std::vector<Curve> rows;
    rows.reserve(std::size_t(rowCount));
    for (int k = 0; k < rowCount; ++k) {
        const double v = v0 + k * step;
        Vec3 q, qn, a, an, b, bn;
        if (!upward.at(v, q, qn))
            throw hangsOff();
        const double h = step * 0.25;
        Vec3 tangent = up;
        if (upward.at(v + h, b, bn) && upward.at(v - h, a, an))
            tangent = sub(b, a);
        else if (upward.at(v + h, b, bn))
            tangent = sub(b, q);
        else if (upward.at(v - h, a, an))
            tangent = sub(q, a);
        tangent = length(tangent) > 0.0 ? unit(tangent) : up;
        const Vec3 qnUnit = length(qn) > 0.0 ? unit(qn) : n;
        Vec3 across = cross(tangent, qnUnit);
        across = length(across) > 1e-9 ? unit(across) : right;
        rows.push_back(slice(q, tangent, across, limitAcross));
    }
    const auto place = [&](double u, double v, Vec3& at, Vec3& facing) {
        const double x = (v - v0) / step;
        const int k = std::clamp(int(std::floor(x)), 0, rowCount - 2);
        const double f = std::clamp(x - k, 0.0, 1.0);
        Vec3 pa, na, pb, nb;
        if (!rows[std::size_t(k)].at(u, pa, na) || !rows[std::size_t(k) + 1].at(u, pb, nb))
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
    if (!doc.isTopLevel(target))
        throw std::invalid_argument("Only a shape on the plate can be embossed");
    BooleanSpec spec;
    spec.op = BooleanOp::Emboss;
    spec.operands = {target};
    // A new result sits where its parts are, so plate coordinates are its own.
    spec.planePoint = where.point;
    spec.planeNormal = unit(where.normal);
    spec.emboss = words;
    const auto centre = doc.worldBounds(target).center();
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
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
