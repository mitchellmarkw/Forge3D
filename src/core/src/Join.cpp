// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Join.h"

#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/History.h"
#include "forge/core/Measure.h"
#include "forge/core/Socket.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
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

}  // namespace forge::join
