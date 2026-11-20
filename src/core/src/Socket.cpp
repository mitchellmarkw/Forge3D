// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Socket.h"

#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace forge {
namespace socket {

namespace {

constexpr double kPi = std::numbers::pi;

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 scaled(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const Vec3& a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }
Vec3 unit(const Vec3& a)
{
    const double l = length(a);
    if (!(l > 1e-12))
        throw std::invalid_argument("The socket's direction is missing");
    return scaled(a, 1.0 / l);
}

// A frame at the opening: z goes into the part, x and y lie across the surface (x the
// way the words of an Emboss would read, turned by `turn` degrees).
struct Frame {
    Vec3 origin, x, y, z;
    Vec3 at(double u, double v, double w) const
    {
        return add(origin, add(scaled(x, u), add(scaled(y, v), scaled(z, w))));
    }
};

Frame frameAt(const Vec3& point, const Vec3& normal, double turn)
{
    const Vec3 n = unit(normal);
    const Vec3 up = emboss::upFor(n);
    Vec3 x = unit(cross(up, n));  // across, as Emboss lays words
    Vec3 y0 = cross(n, x);
    const double a = turn * kPi / 180.0;
    const Vec3 xt = add(scaled(x, std::cos(a)), scaled(y0, std::sin(a)));
    Frame f;
    f.origin = point;
    f.z = scaled(n, -1.0);
    f.x = xt;
    f.y = cross(f.z, f.x);  // right-handed: x × y = z
    return f;
}

// Sides for a round shape of radius r: the polygon's edge never more than about 5 µm in
// from the true circle.
int roundSides(double r)
{
    const double tol = 0.005;
    if (r <= tol)
        return 48;
    const double step = 2.0 * std::acos(std::max(-1.0, 1.0 - tol / r));
    return std::clamp(int(std::ceil(2.0 * kPi / step)), 48, 192);
}

void pushPoint(Mesh& m, const Vec3& p)
{
    m.positions.push_back(float(p[0]));
    m.positions.push_back(float(p[1]));
    m.positions.push_back(float(p[2]));
}

void pushTriangle(Mesh& m, std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    m.indices.insert(m.indices.end(), {a, b, c});
}

// Turns every triangle round if the surface faces in (a negative volume).
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

// A prism: a regular polygon of `sides`, its corners `radius` from the axis (the first
// at `start` radians from x), from depth z0 to z1.
Mesh prism(const Frame& f, int sides, double radius, double start, double z0, double z1)
{
    Mesh m;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < sides; ++i) {
            const double a = start + 2.0 * kPi * i / sides;
            pushPoint(m, f.at(radius * std::cos(a), radius * std::sin(a), k == 0 ? z0 : z1));
        }
    const auto n = std::uint32_t(sides);
    const std::uint32_t c0 = 2 * n, c1 = 2 * n + 1;
    pushPoint(m, f.at(0, 0, z0));
    pushPoint(m, f.at(0, 0, z1));
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t j = (i + 1) % n;
        pushTriangle(m, i, j, n + j);
        pushTriangle(m, i, n + j, n + i);
        pushTriangle(m, c0, j, i);
        pushTriangle(m, c1, n + i, n + j);
    }
    faceOut(m);
    return m;
}

// A ball of `radius` with its middle at depth `z`. The facets reach out past the true
// sphere (never inside it), so the hollow is never smaller than asked.
Mesh ball(const Frame& f, double radius, double z)
{
    const int around = roundSides(radius);
    const int rings = around / 2;
    // Corners further out, so the middles of the facets are on the sphere.
    const double r = radius / (std::cos(kPi / around) * std::cos(kPi / (2 * rings)));
    Mesh m;
    pushPoint(m, f.at(0, 0, z - r));  // 0: the top (towards the opening)
    for (int ring = 1; ring < rings; ++ring) {
        const double phi = kPi * ring / rings;
        for (int i = 0; i < around; ++i) {
            const double a = 2.0 * kPi * i / around;
            pushPoint(m, f.at(r * std::sin(phi) * std::cos(a), r * std::sin(phi) * std::sin(a),
                              z - r * std::cos(phi)));
        }
    }
    const auto bottom = std::uint32_t(m.positions.size() / 3);
    pushPoint(m, f.at(0, 0, z + r));
    const auto at = [&](int ring, int i) { return std::uint32_t(1 + (ring - 1) * around + (i % around)); };
    for (int i = 0; i < around; ++i) {
        pushTriangle(m, 0, at(1, i + 1), at(1, i));
        pushTriangle(m, bottom, at(rings - 1, i), at(rings - 1, i + 1));
    }
    for (int ring = 1; ring + 1 < rings; ++ring)
        for (int i = 0; i < around; ++i) {
            pushTriangle(m, at(ring, i), at(ring, i + 1), at(ring + 1, i + 1));
            pushTriangle(m, at(ring, i), at(ring + 1, i + 1), at(ring + 1, i));
        }
    faceOut(m);
    return m;
}

// A slot: `width` across, from the axis out to `reach`, at `angle` radians from x,
// from depth z0 to z1.
Mesh slot(const Frame& f, double angle, double width, double reach, double z0, double z1)
{
    Frame turned = f;
    turned.x = add(scaled(f.x, std::cos(angle)), scaled(f.y, std::sin(angle)));
    turned.y = cross(turned.z, turned.x);
    Mesh m;
    const double h = width / 2.0;
    for (int k = 0; k < 2; ++k) {
        const double z = k == 0 ? z0 : z1;
        pushPoint(m, turned.at(0, -h, z));
        pushPoint(m, turned.at(reach, -h, z));
        pushPoint(m, turned.at(reach, h, z));
        pushPoint(m, turned.at(0, h, z));
    }
    const std::uint32_t q[6][4] = {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1}, {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0}};
    for (const auto& face : q) {
        pushTriangle(m, face[0], face[1], face[2]);
        pushTriangle(m, face[0], face[2], face[3]);
    }
    faceOut(m);
    return m;
}

int sidesOf(PegShape shape)
{
    switch (shape) {
    case PegShape::Square: return 4;
    case PegShape::Hexagon: return 6;
    case PegShape::Octagon: return 8;
    case PegShape::Round: break;
    }
    return 0;
}

const char* shapeName(PegShape shape)
{
    switch (shape) {
    case PegShape::Square: return "Square";
    case PegShape::Hexagon: return "Hexagon";
    case PegShape::Octagon: return "Octagon";
    case PegShape::Round: break;
    }
    return "Round";
}

std::string num(double v)
{
    std::ostringstream out;
    out.precision(3);
    out << v;
    return out.str();
}

// How far the hollow starts outside the surface (so it opens cleanly where the surface
// curves away from the spot).
double margin(const SocketFinish& f)
{
    return std::max(0.5, 0.5 * f.size);
}

// The hollow's radius out to its widest (for the relief cuts' reach).
double hollowRadius(const SocketFinish& f)
{
    if (f.kind == SocketKind::Ball)
        return ballSeat(f).radius;
    const int sides = sidesOf(f.shape);
    const double half = f.size / 2.0 + f.fit;
    return sides == 0 ? half : half / std::cos(kPi / sides);
}

}  // namespace

BallSeat ballSeat(const SocketFinish& f)
{
    BallSeat s;
    s.radius = f.size / 2.0 + f.fit;
    if (f.snap) {
        // The opening is grip% narrower than the ball: the ball's middle sits that much
        // further in.
        s.opening = std::min(f.size * (1.0 - f.grip / 100.0), 2.0 * s.radius);
        const double half = s.opening / 2.0;
        s.centreDepth = std::sqrt(std::max(0.0, s.radius * s.radius - half * half));
    } else {
        // Open: the ball drops in until its top is level with the surface.
        s.opening = 2.0 * s.radius;
        s.centreDepth = f.size / 2.0;
    }
    return s;
}

double reliefDepth(const SocketFinish& f)
{
    if (f.reliefDepth > 0.0)
        return f.reliefDepth;
    return f.kind == SocketKind::Ball ? ballSeat(f).centreDepth : f.depth / 2.0;
}

void check(const SocketFinish& f)
{
    if (!(f.size > 0.0) || !std::isfinite(f.size))
        throw std::invalid_argument(f.kind == SocketKind::Ball ? "The ball needs a size." : "The peg needs a size.");
    if (f.size > 1000.0)
        throw std::invalid_argument("That socket is too big.");
    if (!(f.fit >= 0.0) || f.fit > 5.0)
        throw std::invalid_argument("The fit must be between 0 and 5 mm.");
    if (f.kind == SocketKind::Peg && (!(f.depth > 0.0) || f.depth > 1000.0))
        throw std::invalid_argument("The peg needs a length (how far it goes in).");
    if (f.kind == SocketKind::Magnet && (!(f.depth > 0.0) || f.depth > 100.0))
        throw std::invalid_argument("The magnet needs a thickness.");
    if (f.kind == SocketKind::Ball && f.snap && (!(f.grip >= 0.0) || f.grip > 50.0))
        throw std::invalid_argument("The grip must be between 0% and 50%.");
    if (f.reliefCuts < 0 || f.reliefCuts > 12)
        throw std::invalid_argument("A socket can have up to 12 relief cuts.");
    if (f.reliefCuts > 0 && (!(f.reliefWidth > 0.0) || f.reliefWidth > f.size))
        throw std::invalid_argument("The relief cuts need a width, less than the socket's size.");
    if (!(f.reliefDepth >= 0.0) || !std::isfinite(f.turn))
        throw std::invalid_argument("The relief cuts' depth can't be less than 0.");
    if (f.kind == SocketKind::Magnet) {
        if (f.size < 0.5)
            throw std::invalid_argument("The magnet must be at least 0.5 mm across.");
        if (!(f.cover >= 0.0) || f.cover > 5.0)
            throw std::invalid_argument("The skin over a magnet can be up to 5 mm.");
    }
    if (f.ridges < 0 || f.ridges > 24)
        throw std::invalid_argument("A ball socket can have up to 24 ridges.");
    if (f.kind == SocketKind::Ball && f.ridges > 0) {
        if (!std::isfinite(f.ridgeHeight) || !(f.ridgeHeight > f.fit) || f.ridgeHeight > f.size / 6.0)
            throw std::invalid_argument("The ridges must stand in more than the fit (so they press on the ball), "
                                        "and no more than a sixth of the ball.");
    }
}

std::vector<Mesh> cutters(const SocketFinish& f, const Vec3& point, const Vec3& normal)
{
    check(f);
    const Frame frame = frameAt(point, normal, f.turn);
    const double out = margin(f);
    std::vector<Mesh> cuts;
    if (f.kind == SocketKind::Ball) {
        const BallSeat seat = ballSeat(f);
        if (f.ridges > 0) {
            // Ridges left standing inside the cup, from the opening down: slices of the
            // shell between the cup and `ridgeHeight` in from it are not cut away.
            const auto solidOf = [](const Mesh& m) { return meshkernel::fromTriangles(m.positions, m.indices); };
            const double width = std::max(0.4, 0.08 * f.size);
            const MeshSolid inner = solidOf(ball(frame, seat.radius - f.ridgeHeight, seat.centreDepth));
            std::vector<MeshSolid> ribs;
            for (int i = 0; i < f.ridges; ++i) {
                const double a = 2.0 * kPi * (i + 0.5) / f.ridges;  // between the relief cuts
                ribs.push_back(meshkernel::subtracted(
                    solidOf(slot(frame, a, width, seat.radius + 1.0, seat.centreDepth - seat.radius - 1.0,
                                 seat.centreDepth + seat.radius + 1.0)),
                    {inner}));
            }
            Mesh cup;
            meshkernel::triangles(meshkernel::subtracted(solidOf(ball(frame, seat.radius, seat.centreDepth)), ribs),
                                  cup.positions, cup.indices);
            cuts.push_back(std::move(cup));
        } else {
            cuts.push_back(ball(frame, seat.radius, seat.centreDepth));
        }
        if (!f.snap)  // straight down to the ball's middle, so it drops in
            cuts.push_back(prism(frame, roundSides(seat.radius), seat.radius / std::cos(kPi / roundSides(seat.radius)),
                                 0.0, -out, seat.centreDepth));
        else  // above the opening, so a curved surface doesn't leave a lip over it
            cuts.push_back(prism(frame, roundSides(seat.opening / 2.0),
                                 seat.opening / 2.0 / std::cos(kPi / roundSides(seat.opening / 2.0)), 0.0, -out, 0.0));
    } else if (f.kind == SocketKind::Magnet) {
        // Round, the magnet's thickness plus the fit deep: flush with the surface, or
        // under a skin (a closed pocket).
        const double half = f.size / 2.0 + f.fit;
        const int n = roundSides(half);
        cuts.push_back(prism(frame, n, half / std::cos(kPi / n), 0.0, f.cover > 0.0 ? f.cover : -out,
                             f.cover + f.depth + f.fit));
        return cuts;  // no relief cuts: nothing snaps in
    } else {
        const int sides = sidesOf(f.shape);
        const double half = f.size / 2.0 + f.fit;  // across the flats (or the radius), with room
        if (sides == 0) {
            const int n = roundSides(half);
            cuts.push_back(prism(frame, n, half / std::cos(kPi / n), 0.0, -out, f.depth + f.fit));
        } else {
            // A flat facing along x: the first corner half a side round from it.
            cuts.push_back(prism(frame, sides, half / std::cos(kPi / sides), kPi / sides, -out, f.depth + f.fit));
        }
    }
    if (f.reliefCuts > 0) {
        const double radius = hollowRadius(f);
        const double reach = radius + std::max(1.0, 0.75 * radius);
        const double down = reliefDepth(f);
        for (int i = 0; i < f.reliefCuts; ++i)
            cuts.push_back(slot(frame, 2.0 * kPi * i / f.reliefCuts, f.reliefWidth, reach, -out, down));
    }
    return cuts;
}

std::string describe(const SocketFinish& f)
{
    std::string text;
    if (f.kind == SocketKind::Ball)
        text = "Ball socket \xC3\x98" + num(f.size) + (f.snap ? " (snap-in)" : " (open)");
    else if (f.kind == SocketKind::Magnet)
        return "Magnet pocket \xC3\x98" + num(f.size) + " \xC3\x97 " + num(f.depth) +
               (f.cover > 0.0 ? ", under " + num(f.cover) + " mm" : std::string());
    else
        text = std::string(shapeName(f.shape)) + " peg socket " +
               (f.shape == PegShape::Round ? "\xC3\x98" : "") + num(f.size) + " \xC3\x97 " + num(f.depth);
    if (f.reliefCuts > 0)
        text += ", " + std::to_string(f.reliefCuts) + " relief cuts";
    if (f.kind == SocketKind::Ball && f.ridges > 0)
        text += ", " + std::to_string(f.ridges) + " ridges";
    return text;
}

PrimitiveSpec matchingPart(const SocketFinish& f)
{
    check(f);
    if (f.kind == SocketKind::Ball)
        return SphereSpec(f.size / 2.0);
    if (f.kind == SocketKind::Magnet)  // a stand-in, to see it in place
        return CylinderSpec(f.size / 2.0, f.depth);
    const int sides = sidesOf(f.shape);
    if (sides == 0)
        return CylinderSpec(f.size / 2.0, f.depth);
    if (sides == 4)
        return BoxSpec{f.size, f.size, f.depth};
    // A hexagon or octagon drawn in a sketch and padded up, a flat facing along X.
    PadSpec pad;
    pad.length = f.depth;
    const double corner = f.size / 2.0 / std::cos(kPi / sides);
    const int first = int(pad.sketch.points.size());
    for (int i = 0; i < sides; ++i) {
        const double a = kPi / sides + 2.0 * kPi * i / sides;
        pad.sketch.points.push_back({corner * std::cos(a), corner * std::sin(a)});
    }
    for (int i = 0; i < sides; ++i) {
        sketch::Geo line;
        line.kind = sketch::GeoKind::Line;
        line.a = first + i;
        line.b = first + (i + 1) % sides;
        pad.sketch.geos.push_back(line);
    }
    return pad;
}

std::string matchingName(const SocketFinish& f)
{
    if (f.kind == SocketKind::Ball)
        return "Ball \xC3\x98" + num(f.size);
    if (f.kind == SocketKind::Magnet)
        return "Magnet \xC3\x98" + num(f.size) + " \xC3\x97 " + num(f.depth);
    return std::string(shapeName(f.shape)) + " peg " + (f.shape == PegShape::Round ? "\xC3\x98" : "") + num(f.size) +
           " \xC3\x97 " + num(f.depth);
}

}  // namespace socket

namespace {

Vec3 unitOf(const Vec3& a)
{
    const double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    if (!(l > 1e-12))
        throw std::invalid_argument("The socket's direction is missing");
    return {a[0] / l, a[1] / l, a[2] / l};
}

const Body& socketStep(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Socket)
        throw std::invalid_argument("Not a socket");
    return *body;
}

}  // namespace

BooleanSpec socketSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                       const SocketFinish& finish)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    socket::check(finish);
    BooleanSpec spec;
    spec.op = BooleanOp::Socket;
    spec.operands = {target};
    // In the coordinates of whatever holds the part (the plate, or the next step up).
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.planePoint = toStep.apply(where.point);
    spec.planeNormal = unitOf(toStep.applyDirection(where.normal));
    spec.socket = finish;
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

emboss::SurfacePoint socketSpot(const Document& doc, ObjectId result)
{
    const Body& body = socketStep(doc, result);
    const Transform t = doc.worldTransform(result);
    return {t.apply(body.boolean->planePoint), unitOf(t.applyDirection(body.boolean->planeNormal))};
}

Body withSocket(const Document& doc, ObjectId result, const emboss::SurfacePoint& where, const SocketFinish& finish)
{
    const Body& body = socketStep(doc, result);
    socket::check(finish);
    const Transform toLocal = doc.worldTransform(result).inverse();
    Body next = body;
    next.boolean->planePoint = toLocal.apply(where.point);
    next.boolean->planeNormal = unitOf(toLocal.applyDirection(where.normal));
    next.boolean->socket = finish;
    return next;
}

std::optional<emboss::SurfacePoint> facingSpot(const Document& doc, ObjectId other, const emboss::SurfacePoint& spot,
                                               double within)
{
    const Vec3& n = spot.normal;
    const double l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (!(l > 1e-12))
        return std::nullopt;
    const Vec3 u{n[0] / l, n[1] / l, n[2] / l};
    // From just inside this part, straight out: the first of the other's surface met.
    const Vec3 from{spot.point[0] - u[0] * 0.5, spot.point[1] - u[1] * 0.5, spot.point[2] - u[2] * 0.5};
    const auto hit = surfaceUnder(doc, other, from, u, 0.0);
    if (!hit)
        return std::nullopt;
    const Vec3 d{hit->point[0] - spot.point[0], hit->point[1] - spot.point[1], hit->point[2] - spot.point[2]};
    if (std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) > within)
        return std::nullopt;
    if (hit->normal[0] * u[0] + hit->normal[1] * u[1] + hit->normal[2] * u[2] > -0.5)
        return std::nullopt;  // not a face turned back towards this one
    return hit;
}

}  // namespace forge
