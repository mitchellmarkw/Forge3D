// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Port.h"

#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>

namespace forge {

namespace {

constexpr double kPi = std::numbers::pi;

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const Vec3& a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
Vec3 unit(const Vec3& a)
{
    const double l = length(a);
    if (!(l > 1e-12))
        throw std::invalid_argument("The port's direction is missing.");
    return mul(a, 1.0 / l);
}

std::string num(double v)
{
    std::ostringstream s;
    s.precision(3);
    s << v;
    return s.str();
}

int sidesOf(PegShape s)
{
    switch (s) {
    case PegShape::Square: return 4;
    case PegShape::Hexagon: return 6;
    case PegShape::Octagon: return 8;
    default: return 0;
    }
}

const char* shapeName(PegShape s)
{
    switch (s) {
    case PegShape::Square: return "Square";
    case PegShape::Hexagon: return "Hexagon";
    case PegShape::Octagon: return "Octagon";
    default: return "Round";
    }
}

// The peg's frame: z out along the peg, x across (as Emboss lays words, turned by `turn`).
struct Frame {
    Vec3 origin, x, y, z;
    Vec3 at(double u, double v, double w) const { return add(origin, add(mul(x, u), add(mul(y, v), mul(z, w)))); }
};

Frame frameAt(const Vec3& point, const Vec3& out, double turn)
{
    const Vec3 n = unit(out);
    const Vec3 up = emboss::upFor(n);
    const Vec3 x = unit(cross(up, n));
    const Vec3 y0 = cross(n, x);
    const double a = turn * kPi / 180.0;
    Frame f;
    f.origin = point;
    f.z = n;
    f.x = add(mul(x, std::cos(a)), mul(y0, std::sin(a)));
    f.y = cross(f.z, f.x);
    return f;
}

void put(Mesh& m, const Vec3& p)
{
    for (std::size_t k = 0; k < 3; ++k)
        m.positions.push_back(float(p[k]));
}

void faceOut(Mesh& m)
{
    double v = 0.0;
    const auto& p = m.positions;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &p[3 * m.indices[t]];
        const float* b = &p[3 * m.indices[t + 1]];
        const float* c = &p[3 * m.indices[t + 2]];
        v += double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) -
             double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
             double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0]);
    }
    if (v < 0.0)
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
            std::swap(m.indices[t + 1], m.indices[t + 2]);
}

// A closed shape of stacked rings round the frame's z: each ring (z, corner radius), the
// polygon's first corner at `start` radians from x. Capped at both ends.
Mesh stack(const Frame& f, int sides, double start, const std::vector<std::pair<double, double>>& rings)
{
    Mesh m;
    for (const auto& [z, r] : rings)
        for (int i = 0; i < sides; ++i) {
            const double a = start + 2.0 * kPi * i / sides;
            put(m, f.at(r * std::cos(a), r * std::sin(a), z));
        }
    const auto n = std::uint32_t(sides);
    const auto last = std::uint32_t(rings.size() - 1);
    const auto c0 = std::uint32_t(m.positions.size() / 3);
    put(m, f.at(0, 0, rings.front().first));
    put(m, f.at(0, 0, rings.back().first));
    for (std::uint32_t k = 0; k < last; ++k)
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint32_t j = (i + 1) % n, a = k * n;
            m.indices.insert(m.indices.end(), {a + i, a + j, a + n + j, a + i, a + n + j, a + n + i});
        }
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t j = (i + 1) % n;
        m.indices.insert(m.indices.end(), {c0, j, i, c0 + 1, last * n + i, last * n + j});
    }
    faceOut(m);
    return m;
}

int roundSides(double r) { return std::clamp(int(std::ceil(2.0 * kPi / (2.0 * std::acos(std::max(-1.0, 1.0 - 0.005 / std::max(r, 0.01)))))), 48, 192); }

// A ball of `radius` with its middle at height z on the frame's axis.
Mesh ball(const Frame& f, double radius, double z)
{
    const int around = std::max(32, roundSides(radius) / 2);
    const int rings = around / 2;
    Mesh m;
    put(m, f.at(0, 0, z - radius));
    for (int ring = 1; ring < rings; ++ring) {
        const double phi = kPi * ring / rings;
        for (int i = 0; i < around; ++i) {
            const double a = 2.0 * kPi * i / around;
            put(m, f.at(radius * std::sin(phi) * std::cos(a), radius * std::sin(phi) * std::sin(a),
                        z - radius * std::cos(phi)));
        }
    }
    const auto bottom = std::uint32_t(m.positions.size() / 3);
    put(m, f.at(0, 0, z + radius));
    const auto at = [&](int ring, int i) { return std::uint32_t(1 + (ring - 1) * around + (i % around)); };
    for (int i = 0; i < around; ++i) {
        m.indices.insert(m.indices.end(), {0, at(1, i + 1), at(1, i)});
        m.indices.insert(m.indices.end(), {bottom, at(rings - 1, i), at(rings - 1, i + 1)});
    }
    for (int ring = 1; ring + 1 < rings; ++ring)
        for (int i = 0; i < around; ++i)
            m.indices.insert(m.indices.end(), {at(ring, i), at(ring, i + 1), at(ring + 1, i + 1), at(ring, i),
                                               at(ring + 1, i + 1), at(ring + 1, i)});
    faceOut(m);
    return m;
}

void append(Mesh& to, const Mesh& from)
{
    const auto first = std::uint32_t(to.positions.size() / 3);
    to.positions.insert(to.positions.end(), from.positions.begin(), from.positions.end());
    for (std::uint32_t i : from.indices)
        to.indices.push_back(first + i);
}

// How far a peg starts inside the part, so it joins a curved surface without a gap.
double sinkOf(const PortFinish& f) { return std::max(1.0, f.size * 0.75); }

}  // namespace

namespace port {

void check(const PortFinish& f)
{
    if (!(f.size >= 0.5 && f.size <= 50.0))
        throw std::invalid_argument("A port must be from 0.5 to 50 mm across.");
    if (f.kind == PortKind::Peg && !(f.length >= 0.5 && f.length <= 100.0))
        throw std::invalid_argument("A peg must stand out from 0.5 to 100 mm.");
    if (!(f.fit >= 0.0 && f.fit <= 1.0))
        throw std::invalid_argument("A grip hole's fit must be from 0 to 1 mm.");
    if (f.kind == PortKind::Peg && f.end == PegEnd::Ball && !(f.ball >= f.size * 0.8 && f.ball <= 60.0))
        throw std::invalid_argument("The ball on a peg's end must be at least most of the peg's size across.");
    for (double v : f.point)
        if (!std::isfinite(v))
            throw std::invalid_argument("The port's spot isn't a number.");
    unit(f.direction);
    if (f.kind == PortKind::Grip && f.hasExit && length(sub(f.exit, f.point)) < 0.1)
        throw std::invalid_argument("A grip hole's way out must be away from its way in.");
}

std::string describe(const PortFinish& f)
{
    if (f.kind == PortKind::Grip)
        return "Grip hole \xC3\x98" + num(f.size + 2.0 * f.fit) + " for a \xC3\x98" + num(f.size) + " handle";
    std::string s = std::string(shapeName(f.shape)) + " peg " + (f.shape == PegShape::Round ? "\xC3\x98" : "") +
                    num(f.size) + " \xC3\x97 " + num(f.length);
    if (f.end == PegEnd::Ball)
        s += ", ball \xC3\x98" + num(f.ball);
    else if (f.end == PegEnd::Chamfer)
        s += " (chamfered)";
    return s;
}

Mesh shape(const PortFinish& f, double reach)
{
    check(f);
    const Vec3 point{f.point[0], f.point[1], f.point[2]};
    if (f.kind == PortKind::Grip) {
        // In along -direction (or to the way out), 2 mm past each end.
        const Vec3 in = f.hasExit ? unit(sub(f.exit, f.point)) : mul(unit(f.direction), -1.0);
        const double to = f.hasExit ? length(sub(f.exit, f.point)) + 2.0 : reach;
        const double r = f.size / 2.0 + f.fit;
        const int n = roundSides(r);
        const Frame frame = frameAt(point, in, 0.0);
        return stack(frame, n, 0.0, {{-2.0, r / std::cos(kPi / n)}, {to, r / std::cos(kPi / n)}});
    }
    const Frame frame = frameAt(point, f.direction, f.turn);
    const int sides = sidesOf(f.shape);
    const int n = sides == 0 ? roundSides(f.size / 2.0) : sides;
    const double start = sides == 0 ? 0.0 : kPi / sides;  // a flat facing along x
    const double corner = f.size / 2.0 / std::cos(kPi / n);
    const double sink = sinkOf(f);
    std::vector<std::pair<double, double>> rings{{-sink, corner}};
    const double top = f.length;  // a ball sits on the end
    if (f.end == PegEnd::Chamfer) {
        const double c = std::min(0.4, std::min(f.size * 0.15, f.length * 0.3));
        rings.push_back({top - c, corner});
        rings.push_back({top, corner * (1.0 - 2.0 * c / f.size)});
    } else {
        rings.push_back({top, corner});
    }
    Mesh m = stack(frame, n, start, rings);
    if (f.end == PegEnd::Ball)
        append(m, ball(frame, f.ball / 2.0, f.length));
    return m;
}

MeshSolid apply(const MeshSolid& part, const PortFinish& f)
{
    const Bounds b = meshkernel::bounds(part);
    if (!b.valid)
        throw std::invalid_argument("The part is empty.");
    const auto sz = b.size();
    const double reach = length({sz[0], sz[1], sz[2]}) + 2.0;
    const Mesh m = shape(f, reach);
    if (f.kind == PortKind::Grip)
        return meshkernel::subtracted(part, {meshkernel::fromTriangles(m.positions, m.indices)});
    if (f.end == PegEnd::Ball) {
        // The peg and its ball as two closed surfaces, joined to the part together.
        const Frame frame = frameAt({f.point[0], f.point[1], f.point[2]}, f.direction, f.turn);
        PortFinish neck = f;
        neck.end = PegEnd::Flat;
        const Mesh stem = shape(neck, reach);
        const Mesh round = ball(frame, f.ball / 2.0, f.length);
        return meshkernel::merged({part, meshkernel::fromTriangles(stem.positions, stem.indices),
                                   meshkernel::fromTriangles(round.positions, round.indices)});
    }
    return meshkernel::merged({part, meshkernel::fromTriangles(m.positions, m.indices)});
}

SocketFinish socketFor(const PortFinish& peg, double fit, bool resin)
{
    SocketFinish s;
    s.fit = fit;
    s.resin = resin;
    if (peg.end == PegEnd::Ball) {
        s.kind = SocketKind::Ball;
        s.size = peg.ball;
        s.snap = true;
        return s;
    }
    s.kind = SocketKind::Peg;
    s.shape = peg.shape;
    s.size = peg.size;
    s.depth = peg.length;
    s.turn = peg.turn;
    return s;
}

PortFinish fromSocket(const SocketFinish& joint, PortFinish now)
{
    if (now.kind == PortKind::Grip) {
        now.size = joint.size;  // the fit is the printer's, not the standard's
        return now;
    }
    if (joint.kind == SocketKind::Ball) {
        now.end = PegEnd::Ball;
        now.ball = joint.size;
        now.size = std::min(now.size, joint.size * 0.8);
        return now;
    }
    now.shape = joint.shape;
    now.size = joint.size;
    now.length = joint.depth;
    if (now.end == PegEnd::Ball)
        now.end = PegEnd::Chamfer;
    return now;
}

SocketFinish jointFor(const PortFinish& f, bool resin)
{
    if (f.kind == PortKind::Grip) {
        SocketFinish s;
        s.kind = SocketKind::Peg;
        s.shape = PegShape::Round;
        s.size = f.size;
        s.depth = 4.0;
        s.fit = f.fit;
        s.resin = resin;
        return s;
    }
    return socketFor(f, resin ? 0.1 : 0.2, resin);
}

}  // namespace port

namespace {

const Body& portStep(const Document& doc, ObjectId result)
{
    const Body* b = doc.find(result);
    if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::Port || !b->boolean->port)
        throw std::invalid_argument("That isn't a port.");
    return *b;
}

}  // namespace

BooleanSpec portSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where, PortFinish f)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (!doc.isTopLevel(target))
        throw std::invalid_argument("A port goes on a shape on the plate.");
    f.point = where.point;
    f.direction = unit(where.normal);
    port::check(f);
    BooleanSpec spec;
    spec.op = BooleanOp::Port;
    spec.operands = {target};
    spec.port = f;
    return spec;
}

const PortFinish& portOf(const Document& doc, ObjectId result) { return *portStep(doc, result).boolean->port; }

Body withPort(const Document& doc, ObjectId result, const PortFinish& f)
{
    port::check(f);
    Body next = portStep(doc, result);
    next.boolean->port = f;
    return next;
}

}  // namespace forge
