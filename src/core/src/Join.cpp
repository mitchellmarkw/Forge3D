// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Join.h"

#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/History.h"
#include "forge/core/Measure.h"
#include "forge/core/Socket.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <array>
#include <unordered_map>
#include <cmath>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace forge::join {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a, const Vec3& fallback = {0, 0, 1})
{
    const double l = norm(a);
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}

std::string num(double v)
{
    std::ostringstream out;
    out.precision(3);
    out << v;
    return out.str();
}

const std::string kDiameter = "\xC3\x98";

// The turn about `axis` (unit) by `angle` radians.
Transform rotationAbout(const Vec3& axis, double angle)
{
    const double c = std::cos(angle), s = std::sin(angle), t = 1.0 - c;
    const double x = axis[0], y = axis[1], z = axis[2];
    Transform r;
    r.m = {{{t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0.0},
            {t * x * y + s * z, t * y * y + c, t * y * z - s * x, 0.0},
            {t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0.0}}};
    return r;
}

// The least turn taking unit vector a to unit vector b.
Transform turnOnto(const Vec3& a, const Vec3& b)
{
    const Vec3 axis = cross(a, b);
    const double s = norm(axis), c = dot(a, b);
    if (s < 1e-12) {
        if (c > 0.0)
            return Transform::identity();
        // Opposite: half a turn about any line across a.
        Vec3 across = cross(a, {1.0, 0.0, 0.0});
        if (norm(across) < 1e-6)
            across = cross(a, {0.0, 1.0, 0.0});
        return rotationAbout(unit(across), std::numbers::pi);
    }
    return rotationAbout(mul(axis, 1.0 / s), std::atan2(s, c));
}

gp_Trsf toGp(const Transform& t)
{
    gp_Trsf trsf;
    trsf.SetValues(t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3],  //
                   t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3],  //
                   t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);
    return trsf;
}

// A Quick Socket in the shape's history that the clicked point is in (or at the mouth of).
std::optional<Feature> socketAt(const Document& doc, ObjectId id, const Vec3& hit)
{
    for (ObjectId step : history::steps(doc, id)) {
        const Body* b = doc.find(step);
        if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::Socket || !b->boolean->socket)
            continue;
        const SocketFinish& f = *b->boolean->socket;
        emboss::SurfacePoint mouth;
        try {
            mouth = socketSpot(doc, step);
        } catch (const std::exception&) {
            continue;
        }
        const Vec3 n = mouth.normal;
        const Vec3 v = sub(hit, mouth.point);
        const double in = -dot(v, n);  // how far into the part
        const double radial = norm(sub(v, mul(n, -in)));
        double reach = 0.0, deepest = 0.0;
        Feature out;
        out.kind = Feature::Kind::Socket;
        out.direction = n;
        out.text = socket::describe(f);
        if (f.kind == SocketKind::Ball) {
            const auto seat = socket::ballSeat(f);
            reach = seat.radius;
            deepest = seat.centreDepth + seat.radius;
            out.point = sub(mouth.point, mul(n, seat.centreDepth));
            out.size = f.size;
        } else {
            reach = (f.size / 2.0 + f.fit) / std::cos(std::numbers::pi / 4.0);  // the widest a square one gets
            deepest = f.depth + f.fit;
            out.point = sub(mouth.point, mul(n, f.depth + f.fit));
            out.size = f.size;
            out.gap = f.fit;  // the peg's end stops short of the bottom by the fit: its shoulder meets the surface
        }
        if (radial <= reach + 0.3 && in >= -0.5 && in <= deepest + 0.3)
            return out;
    }
    return std::nullopt;
}

// A ball fitted to the surface round `hit`: the biggest neighbourhood that is still
// round. Its middle and radius, if any.
std::optional<std::pair<Vec3, double>> ballAround(const std::vector<Vec3>& points, const Vec3& hit)
{
    std::optional<std::pair<Vec3, double>> best;
    for (const double reach : {0.6, 1.0, 1.6, 2.5, 4.0, 6.0, 10.0, 16.0}) {
        // Least squares: |p|^2 = 2 c.p + d, for c (the middle) and d.
        double a[4][5] = {};
        int count = 0;
        for (const Vec3& p : points) {
            if (norm(sub(p, hit)) > reach)
                continue;
            const Vec3 q = sub(p, hit);  // about the hit, for accuracy
            const double row[4] = {2 * q[0], 2 * q[1], 2 * q[2], 1.0};
            const double rhs = dot(q, q);
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j)
                    a[i][j] += row[i] * row[j];
                a[i][4] += row[i] * rhs;
            }
            ++count;
        }
        if (count < 20)
            continue;
        // Gaussian elimination with partial pivoting.
        bool ok = true;
        for (int col = 0; col < 4 && ok; ++col) {
            int piv = col;
            for (int r = col + 1; r < 4; ++r)
                if (std::abs(a[r][col]) > std::abs(a[piv][col]))
                    piv = r;
            if (std::abs(a[piv][col]) < 1e-12) {
                ok = false;
                break;
            }
            std::swap(a[col], a[piv]);
            for (int r = 0; r < 4; ++r) {
                if (r == col)
                    continue;
                const double k = a[r][col] / a[col][col];
                for (int c = col; c < 5; ++c)
                    a[r][c] -= k * a[col][c];
            }
        }
        if (!ok)
            continue;
        const Vec3 c{a[0][4] / a[0][0], a[1][4] / a[1][1], a[2][4] / a[2][2]};
        const double d = a[3][4] / a[3][3];
        const double r2 = d + dot(c, c);
        if (!(r2 > 0.0))
            continue;
        const double r = std::sqrt(r2);
        if (r > 2.5 * reach || r < 0.2 * reach) {
            if (best)
                break;
            continue;  // nearly flat, or noise
        }
        double err = 0.0;
        for (const Vec3& p : points)
            if (norm(sub(p, hit)) <= reach) {
                const double e = norm(sub(sub(p, hit), c)) - r;
                err += e * e;
            }
        // Round all over, and the spot itself on it (a box's corners lie on a ball too).
        if (std::sqrt(err / count) > 0.02 * r || std::abs(norm(c) - r) > 0.02 * r) {
            if (best)
                break;  // no longer round: the last one that was is the ball
            continue;
        }
        best = std::make_pair(add(hit, c), r);
    }
    return best;
}

// The eigenvector of a symmetric 3x3 matrix with the smallest eigenvalue, and all three
// eigenvalues (ascending), by Jacobi turns.
std::pair<Vec3, std::array<double, 3>> smallestAxis(std::array<std::array<double, 3>, 3> a)
{
    std::array<std::array<double, 3>, 3> v{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    for (int sweep = 0; sweep < 50; ++sweep) {
        const double off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        if (off < 1e-20)
            break;
        for (int p = 0; p < 2; ++p)
            for (int q = p + 1; q < 3; ++q) {
                if (std::abs(a[p][q]) < 1e-15)
                    continue;
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t = (theta >= 0 ? 1.0 : -1.0) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0), sn = t * c;
                for (int k = 0; k < 3; ++k) {
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - sn * akq;
                    a[k][q] = sn * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - sn * aqk;
                    a[q][k] = sn * apk + c * aqk;
                }
                for (int k = 0; k < 3; ++k) {
                    const double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - sn * vkq;
                    v[k][q] = sn * vkp + c * vkq;
                }
            }
    }
    std::array<int, 3> order{0, 1, 2};
    std::sort(order.begin(), order.end(), [&](int x, int y) { return a[x][x] < a[y][y]; });
    const int m = order[0];
    return {Vec3{v[0][m], v[1][m], v[2][m]}, {a[order[0]][order[0]], a[order[1]][order[1]], a[order[2]][order[2]]}};
}

struct Round {
    Vec3 axisPoint;  // a point on the axis
    Vec3 axis;       // unit
    double radius;
    double lo, hi;   // how far the round side goes each way along the axis from axisPoint
};

// A triangle of the surface, in plate coordinates.
struct Tri {
    Vec3 a, b, c, n;
    double area;
};

// The smooth patch of surface round `hit`: the triangle under it and those joined to it
// across edges that bend less than 30° (a hole's wall, not the face round its opening).
std::vector<Tri> patchAround(const std::vector<float>& pos, const std::vector<std::uint32_t>& idx, const Vec3& hit,
                             double reach)
{
    // Triangles near enough, with corners joined by place (a display mesh may repeat them).
    std::vector<Tri> tris;
    std::vector<std::array<std::uint64_t, 3>> keys;
    const auto key = [](const Vec3& p) {
        const auto q = [](double v) { return std::uint64_t(std::int64_t(std::llround(v * 1e4)) & 0x1FFFFF); };
        return q(p[0]) | q(p[1]) << 21 | q(p[2]) << 42;
    };
    for (std::size_t t = 0; t + 2 < idx.size(); t += 3) {
        Vec3 c[3];
        bool close = false;
        for (int k = 0; k < 3; ++k) {
            const float* p = &pos[3 * idx[t + std::size_t(k)]];
            c[k] = {p[0], p[1], p[2]};
            close = close || (std::abs(c[k][0] - hit[0]) <= reach && std::abs(c[k][1] - hit[1]) <= reach &&
                              std::abs(c[k][2] - hit[2]) <= reach);
        }
        if (!close)
            continue;
        const Vec3 n = cross(sub(c[1], c[0]), sub(c[2], c[0]));
        const double area = norm(n) / 2.0;
        if (!(area > 1e-12))
            continue;
        tris.push_back({c[0], c[1], c[2], mul(n, 0.5 / area), area});
        keys.push_back({key(c[0]), key(c[1]), key(c[2])});
    }
    if (tris.empty())
        return {};
    // The triangle under the hit: the nearest by its plane, among those the hit is over.
    std::size_t seed = 0;
    double best = 1e300;
    for (std::size_t t = 0; t < tris.size(); ++t) {
        const Tri& tr = tris[t];
        const double d = std::abs(dot(sub(hit, tr.a), tr.n));
        const Vec3 foot = sub(hit, mul(tr.n, dot(sub(hit, tr.a), tr.n)));
        const auto inside = [&](const Vec3& p, const Vec3& q) { return dot(cross(sub(q, p), sub(foot, p)), tr.n) >= -1e-9; };
        const bool over = inside(tr.a, tr.b) && inside(tr.b, tr.c) && inside(tr.c, tr.a);
        const double score = d + (over ? 0.0 : 1e3);
        if (score < best) {
            best = score;
            seed = t;
        }
    }
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> byEdge;
    const auto edgeKey = [](std::uint64_t x, std::uint64_t y) { return x < y ? x * 0x9E3779B97F4A7C15ULL ^ y : y * 0x9E3779B97F4A7C15ULL ^ x; };
    for (std::size_t t = 0; t < tris.size(); ++t)
        for (int k = 0; k < 3; ++k)
            byEdge[edgeKey(keys[t][std::size_t(k)], keys[t][std::size_t((k + 1) % 3)])].push_back(std::uint32_t(t));
    const double bend = std::cos(30.0 * std::numbers::pi / 180.0);
    std::vector<char> taken(tris.size(), 0);
    std::vector<std::size_t> todo{seed};
    taken[seed] = 1;
    std::vector<Tri> out;
    while (!todo.empty()) {
        const std::size_t t = todo.back();
        todo.pop_back();
        out.push_back(tris[t]);
        for (int k = 0; k < 3; ++k) {
            const auto found = byEdge.find(edgeKey(keys[t][std::size_t(k)], keys[t][std::size_t((k + 1) % 3)]));
            if (found == byEdge.end())
                continue;
            for (std::uint32_t o : found->second)
                if (!taken[o] && dot(tris[o].n, tris[t].n) > bend) {
                    taken[o] = 1;
                    todo.push_back(o);
                }
        }
    }
    return out;
}

// A round side (a peg's or a hole's) fitted to the smooth patch round `hit`: its facings
// all lie across one line (the axis), and its corners on a circle round it.
std::optional<Round> roundAround(const std::vector<Tri>& patch, const Vec3& hit)
{
    if (patch.size() < 6)
        return std::nullopt;
    std::array<std::array<double, 3>, 3> cov{};
    for (const Tri& t : patch)
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                cov[std::size_t(r)][std::size_t(c)] += t.area * t.n[std::size_t(r)] * t.n[std::size_t(c)];
    const auto [axis0, eig] = smallestAxis(cov);
    // Facings all across the axis, and spread round it (not a flat face).
    if (!(eig[2] > 0.0) || eig[0] > 0.03 * eig[2] || eig[1] < 0.02 * eig[2])
        return std::nullopt;
    const Vec3 axis = unit(axis0);
    const Vec3 u = unit(cross(axis, std::abs(axis[0]) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0}));
    const Vec3 w = cross(axis, u);
    // Circle through the corners, across the axis: x^2 + y^2 = 2ax + 2by + c, about the hit.
    double m[3][4] = {};
    std::vector<Vec3> corners;
    for (const Tri& t : patch)
        for (const Vec3* p : {&t.a, &t.b, &t.c})
            corners.push_back(*p);
    for (const Vec3& p : corners) {
        const Vec3 q = sub(p, hit);
        const double x = dot(q, u), y = dot(q, w);
        const double row[3] = {2 * x, 2 * y, 1.0};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c)
                m[r][c] += row[r] * row[c];
            m[r][3] += row[r] * (x * x + y * y);
        }
    }
    for (int col = 0; col < 3; ++col) {
        int piv = col;
        for (int r = col + 1; r < 3; ++r)
            if (std::abs(m[r][col]) > std::abs(m[piv][col]))
                piv = r;
        if (std::abs(m[piv][col]) < 1e-12)
            return std::nullopt;
        std::swap(m[col], m[piv]);
        for (int r = 0; r < 3; ++r) {
            if (r == col)
                continue;
            const double k = m[r][col] / m[col][col];
            for (int c = col; c < 4; ++c)
                m[r][c] -= k * m[col][c];
        }
    }
    const double a = m[0][3] / m[0][0], b = m[1][3] / m[1][1], cc = m[2][3] / m[2][2];
    const double r2 = cc + a * a + b * b;
    if (!(r2 > 0.0))
        return std::nullopt;
    const double radius = std::sqrt(r2);
    double err = 0.0;
    for (const Vec3& p : corners) {
        const Vec3 q = sub(p, hit);
        const double e = std::hypot(dot(q, u) - a, dot(q, w) - b) - radius;
        err += e * e;
    }
    // The corners of a polygon round a circle are on it; its facets' middles a little in.
    if (std::sqrt(err / double(corners.size())) > 0.03 * radius ||
        std::abs(std::hypot(a, b) - radius) > 0.05 * radius)
        return std::nullopt;
    Round out{add(hit, add(mul(u, a), mul(w, b))), axis, radius, 1e300, -1e300};
    for (const Vec3& p : corners) {
        const double s = dot(sub(p, out.axisPoint), axis);
        out.lo = std::min(out.lo, s);
        out.hi = std::max(out.hi, s);
    }
    return out;
}

}  // namespace

std::optional<Feature> featureAt(const Document& doc, ObjectId id, const Vec3& origin, const Vec3& direction)
{
    const auto mesh = doc.mesh(id);
    if (!mesh || mesh->empty())
        return std::nullopt;
    const Transform toPlate = doc.worldTransform(id);
    std::vector<float> pos(mesh->positions.size());
    for (std::size_t i = 0; i + 2 < mesh->positions.size(); i += 3) {
        const Vec3 p = toPlate.apply({mesh->positions[i], mesh->positions[i + 1], mesh->positions[i + 2]});
        pos[i] = float(p[0]);
        pos[i + 1] = float(p[1]);
        pos[i + 2] = float(p[2]);
    }
    const auto hit = emboss::surfaceAt(pos, mesh->indices, origin, direction, 0.3);
    if (!hit)
        return std::nullopt;
    if (auto s = socketAt(doc, id, hit->point))
        return s;

    const auto c = doc.worldBounds(id).center();
    const Vec3 middle{c[0], c[1], c[2]};
    Feature spot;
    spot.kind = Feature::Kind::Spot;
    spot.point = hit->point;
    spot.direction = unit(hit->normal);
    spot.text = "A spot on the surface";

    if (!doc.usesMeshEngine(id)) {
        // A shape made in Forge3D: what kind of face it is.
        try {
            const TopoDS_Shape placed = BRepBuilderAPI_Transform(doc.shape(id), toGp(toPlate), true).Shape();
            const measure::Shape shape(placed);
            const auto face = shape.faceUnder(origin, direction);
            if (!face)
                return spot;
            const measure::About about = measure::describe(shape.face(face->face, face->point));
            const Mesh faceMesh = shape.faceMesh(face->face);
            switch (about.kind) {
            case measure::About::Kind::Flat: {
                // Its middle (by area).
                Vec3 sum{0, 0, 0};
                double area = 0.0;
                for (std::size_t t = 0; t + 2 < faceMesh.indices.size(); t += 3) {
                    const auto at = [&](std::size_t k) {
                        const float* p = &faceMesh.positions[3 * faceMesh.indices[t + k]];
                        return Vec3{p[0], p[1], p[2]};
                    };
                    const Vec3 a = at(0), b = at(1), cc = at(2);
                    const double w = norm(cross(sub(b, a), sub(cc, a))) / 2.0;
                    sum = add(sum, mul(add(add(a, b), cc), w / 3.0));
                    area += w;
                }
                Feature f;
                f.kind = Feature::Kind::Flat;
                f.point = area > 0.0 ? mul(sum, 1.0 / area) : face->point;
                f.direction = unit(about.direction);
                f.text = "Flat face";
                return f;
            }
            case measure::About::Kind::Cylinder: {
                const Vec3 axis = unit(about.direction);
                // Its two ends along the axis.
                double lo = 1e300, hi = -1e300;
                for (std::size_t i = 0; i + 2 < faceMesh.positions.size(); i += 3) {
                    const Vec3 p{faceMesh.positions[i], faceMesh.positions[i + 1], faceMesh.positions[i + 2]};
                    const double s = dot(sub(p, about.centre), axis);
                    lo = std::min(lo, s);
                    hi = std::max(hi, s);
                }
                const Vec3 endLo = add(about.centre, mul(axis, lo)), endHi = add(about.centre, mul(axis, hi));
                // The open end: the one further from the shape's middle.
                const bool hiOut = norm(sub(endHi, middle)) >= norm(sub(endLo, middle));
                const Vec3 out = hiOut ? axis : mul(axis, -1.0);
                // A peg (the surface faces away from the axis) or a hole (towards it).
                const Vec3 foot = add(about.centre, mul(axis, dot(sub(hit->point, about.centre), axis)));
                const bool peg = dot(hit->normal, sub(hit->point, foot)) > 0.0;
                Feature f;
                f.direction = out;
                f.size = 2.0 * about.radius;
                if (peg) {
                    f.kind = Feature::Kind::Peg;
                    f.point = hiOut ? endHi : endLo;  // its tip
                    f.text = "Peg " + kDiameter + num(f.size);
                } else {
                    f.kind = Feature::Kind::Hole;
                    f.point = hiOut ? endLo : endHi;  // its bottom
                    f.text = "Hole " + kDiameter + num(f.size);
                }
                return f;
            }
            case measure::About::Kind::Sphere: {
                const bool ball = dot(hit->normal, sub(hit->point, about.centre)) > 0.0;
                Feature f;
                f.kind = ball ? Feature::Kind::Ball : Feature::Kind::BallSocket;
                f.point = about.centre;
                f.direction = unit(sub(about.centre, middle), unit(hit->normal));
                if (!ball)
                    f.direction = unit(hit->normal);  // a cup: out of its opening, roughly where clicked
                f.size = 2.0 * about.radius;
                f.text = (ball ? "Ball " : "Round cup ") + kDiameter + num(f.size);
                return f;
            }
            default:
                return spot;
            }
        } catch (const std::exception&) {
            return spot;
        }
    }

    // A mesh: is it a ball here?
    std::vector<Vec3> points;  // only those near enough to matter
    for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
        const Vec3 p{pos[i], pos[i + 1], pos[i + 2]};
        if (std::abs(p[0] - hit->point[0]) <= 16.0 && std::abs(p[1] - hit->point[1]) <= 16.0 &&
            std::abs(p[2] - hit->point[2]) <= 16.0)
            points.push_back(p);
    }
    if (const auto ball = ballAround(points, hit->point)) {
        const auto& [centre, radius] = *ball;
        const bool convex = dot(hit->normal, sub(hit->point, centre)) > 0.0;
        Feature f;
        f.kind = convex ? Feature::Kind::Ball : Feature::Kind::BallSocket;
        f.point = centre;
        f.direction = convex ? unit(sub(centre, middle), unit(hit->normal)) : unit(hit->normal);
        f.size = 2.0 * radius;
        f.text = (convex ? "Ball " : "Round cup ") + kDiameter + num(f.size);
        return f;
    }
    {
        const auto patch = patchAround(pos, mesh->indices, hit->point, 30.0);
        // The biggest part of the patch round the click that's still round (a peg on a
        // sculpted arm blends smoothly into the arm).
        std::optional<Round> found;
        for (const double reach : {1.0, 2.0, 4.0, 8.0, 16.0, 30.0}) {
            std::vector<Tri> part;
            for (const Tri& t : patch) {
                const Vec3 mid = mul(add(add(t.a, t.b), t.c), 1.0 / 3.0);
                if (norm(sub(mid, hit->point)) <= reach)
                    part.push_back(t);
            }
            if (const auto r = roundAround(part, hit->point))
                found = r;
            else if (found)
                break;
            if (part.size() == patch.size())
                break;  // all of it
        }
        if (const auto round = found) {
            const Vec3 axis = round->axis;
            const Vec3 endLo = add(round->axisPoint, mul(axis, round->lo));
            const Vec3 endHi = add(round->axisPoint, mul(axis, round->hi));
            const bool hiOut = norm(sub(endHi, middle)) >= norm(sub(endLo, middle));
            const Vec3 foot = add(round->axisPoint, mul(axis, dot(sub(hit->point, round->axisPoint), axis)));
            const bool peg = dot(hit->normal, sub(hit->point, foot)) > 0.0;
            Feature f;
            f.direction = hiOut ? axis : mul(axis, -1.0);
            f.size = 2.0 * round->radius;
            if (peg) {
                f.kind = Feature::Kind::Peg;
                f.point = hiOut ? endHi : endLo;
                f.text = "Peg " + kDiameter + num(f.size);
            } else {
                f.kind = Feature::Kind::Hole;
                f.point = hiOut ? endLo : endHi;
                f.text = "Hole " + kDiameter + num(f.size);
            }
            return f;
        }
    }
    return spot;
}

Transform mating(const Feature& moving, const Feature& fixed, const Offsets& offsets)
{
    const Vec3 a = offsets.flip ? mul(unit(moving.direction), -1.0) : unit(moving.direction);
    const Vec3 b = unit(fixed.direction);
    // Facing each other, then spun round the shared line.
    const Transform face = turnOnto(a, mul(b, -1.0));
    const Transform spin = rotationAbout(b, offsets.turn * std::numbers::pi / 180.0);
    const Transform turn = spin * face;
    // Across the fixed feature, as Emboss lays words.
    const Vec3 up = emboss::upFor(b);
    const Vec3 x = unit(cross(up, b), {1, 0, 0});
    const Vec3 y = cross(b, x);
    const Vec3 target =
        add(add(add(fixed.point, mul(b, offsets.along)), mul(x, offsets.acrossX)), mul(y, offsets.acrossY));
    const Vec3 moved = turn.apply(moving.point);
    return Transform::translation(sub(target, moved)) * turn;
}

Placement joinedPlacement(const Document& doc, ObjectId moving, const Feature& movingFeature, const Feature& fixed,
                          const Offsets& offsets)
{
    const Body* body = doc.find(moving);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(moving));
    if (!doc.isTopLevel(moving))
        throw std::invalid_argument("'" + body->name + "' is part of another shape");
    const Transform next = mating(movingFeature, fixed, offsets) * doc.worldTransform(moving);
    return placementFromTransform(next, pivotOf(*body));
}

ObjectId apply(CommandStack& stack, Document& doc, ObjectId moving, ObjectId fixed, const Placement& placed,
               bool together)
{
    const Body* m = doc.find(moving);
    const Body* f = doc.find(fixed);
    if (!m || !f)
        throw std::out_of_range("No such shape");
    if (!doc.isTopLevel(moving) || !doc.isTopLevel(fixed))
        throw std::invalid_argument("Join works on shapes on the plate");
    if (moving == fixed)
        throw std::invalid_argument("A part can't be joined to itself");
    ObjectId out = moving;
    stack.beginMacro("Join " + m->name + " to " + f->name);
    try {
        stack.execute(std::make_unique<SetPlacementCommand>(moving, placed, false));
        if (together) {
            std::vector<ObjectId> members{fixed};
            std::string name;
            const Body* g = doc.find(fixed);
            if (g->isBoolean() && g->boolean->op == BooleanOp::Group) {
                members = g->boolean->operands;
                name = g->name;
                stack.execute(std::make_unique<SeparateCommand>(fixed));
            }
            members.push_back(moving);
            out = groupBodies(stack, doc, members);
            if (!name.empty())
                stack.execute(std::make_unique<RenameBodyCommand>(out, name));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return out;
}

}  // namespace forge::join
