// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Hinge.h"

#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace forge {
namespace hinge {

namespace {

constexpr double kPi = std::numbers::pi;
// How far outside the surface the cuts start, so they open cleanly on a curved one.
constexpr double kOut = 1.0;

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
    if (!(l > 1e-12) || !std::isfinite(l))
        throw std::invalid_argument("The hinge's direction is missing");
    return scaled(a, 1.0 / l);
}

// x along the pin, z into the fork, y across the slot (the way the tab swings).
struct Frame {
    Vec3 origin, x, y, z;
    Vec3 at(double u, double v, double w) const
    {
        return add(origin, add(scaled(x, u), add(scaled(y, v), scaled(z, w))));
    }
};

Frame frameAt(const HingeFinish& f, const Vec3& point, const Vec3& normal)
{
    Frame fr;
    fr.origin = point;
    fr.z = scaled(unit(normal), -1.0);
    fr.x = pinAxis(f, normal);
    fr.y = cross(fr.z, fr.x);
    return fr;
}

// The same frame turned so its z runs along the pin, with its origin on the pin's line
// at depth `depth`: for round things lying along the pin.
Frame alongPin(const Frame& f, double depth)
{
    Frame p;
    p.origin = f.at(0, 0, depth);
    p.x = f.y;
    p.y = f.z;
    p.z = f.x;
    return p;
}

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

// A round shape along the frame's z from z0 to z1, radius r0 at z0 and r1 at z1.
// `hole`: the facets reach out past the true circle (a hole never smaller than asked);
// else they're on it (a part never bigger).
Mesh round(const Frame& f, double r0, double r1, double z0, double z1, bool hole)
{
    const int sides = roundSides(std::max(r0, r1));
    const double grow = hole ? 1.0 / std::cos(kPi / sides) : 1.0;
    Mesh m;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < sides; ++i) {
            const double a = 2.0 * kPi * i / sides;
            const double r = (k == 0 ? r0 : r1) * grow;
            pushPoint(m, f.at(r * std::cos(a), r * std::sin(a), k == 0 ? z0 : z1));
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

Mesh box(const Frame& f, double x0, double x1, double y0, double y1, double z0, double z1)
{
    Mesh m;
    for (int k = 0; k < 8; ++k)
        pushPoint(m, f.at(k & 1 ? x1 : x0, k & 2 ? y1 : y0, k & 4 ? z1 : z0));
    const std::uint32_t faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
    for (const auto& q : faces) {
        pushTriangle(m, q[0], q[1], q[2]);
        pushTriangle(m, q[0], q[2], q[3]);
    }
    faceOut(m);
    return m;
}

MeshSolid solid(const Mesh& m) { return meshkernel::fromTriangles(m.positions, m.indices); }

Mesh meshOf(const MeshSolid& s)
{
    Mesh m;
    meshkernel::triangles(s, m.positions, m.indices);
    return m;
}

std::string num(double v)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << v;
    std::string t = out.str();
    while (!t.empty() && t.back() == '0')
        t.pop_back();
    if (!t.empty() && t.back() == '.')
        t.pop_back();
    return t;
}

const char* kDia = "\xC3\x98";
const char* kTimes = " \xC3\x97 ";

// How far the tab's stem (or the peg) stands out of the fork's surface.
double stemOut(const HingeFinish& f) { return std::max(2.0, f.barrel); }

// The tab: its rounded end round the pin, a stem out of the fork, its pin hole cut.
MeshSolid tabSolid(const HingeFinish& f, const Frame& fr)
{
    const double r = f.barrel / 2.0, half = f.width / 2.0, d = pinDepth(f);
    const MeshSolid end = solid(round(alongPin(fr, d), r, r, -half, half, false));
    const MeshSolid stem = solid(box(fr, -half, half, -r, r, -stemOut(f), d));
    const MeshSolid hole = solid(round(alongPin(fr, d), tabHole(f) / 2.0, tabHole(f) / 2.0, -half - 1, half + 1, true));
    return meshkernel::subtracted(meshkernel::merged({end, stem}), {hole});
}

// The link of a double hinge: a rounded end on each pin, the second pin outside the
// fork's surface, where the other part's fork will be.
MeshSolid linkSolid(const HingeFinish& f, const Frame& fr)
{
    const double r = f.barrel / 2.0, half = f.width / 2.0, d = pinDepth(f);
    const double d2 = d - (2.0 * d + f.gap);  // the other pin: as deep into the other part
    const auto hole = [&](double depth) {
        return solid(round(alongPin(fr, depth), tabHole(f) / 2.0, tabHole(f) / 2.0, -half - 1, half + 1, true));
    };
    const MeshSolid body = meshkernel::merged({solid(round(alongPin(fr, d), r, r, -half, half, false)),
                                               solid(round(alongPin(fr, d2), r, r, -half, half, false)),
                                               solid(box(fr, -half, half, -r, r, d2, d))});
    return meshkernel::subtracted(body, {hole(d), hole(d2)});
}

// The swivel's peg: down to the bottom of the bore, a lip round its end (with a lead-in
// so it goes in) and a slit across it so the lip can squeeze through.
double lipHeight(const HingeFinish& f) { return std::max(0.8, 2.0 * f.lip); }

MeshSolid pegBody(const HingeFinish& f, const Frame& fr)
{
    const double r = f.width / 2.0, depth = f.barrel, lip = f.lip, h = lipHeight(f);
    return meshkernel::merged({solid(round(fr, r, r, -stemOut(f), depth, false)),
                               solid(round(fr, r + lip, r + lip, depth - h, depth - h / 2.0, false)),
                               solid(round(fr, r + lip, r, depth - h / 2.0, depth, false))});
}

Mesh pegSlit(const HingeFinish& f, const Frame& fr)
{
    const double r = f.width / 2.0, depth = f.barrel;
    const double width = std::max(0.4, 0.15 * f.width);
    const double along = std::min(depth * 0.8, lipHeight(f) * 2.5);
    return box(fr, -width / 2.0, width / 2.0, -(r + f.lip + 1.0), r + f.lip + 1.0, depth - along, depth + 1.0);
}

}  // namespace

void check(const HingeFinish& f)
{
    for (double v : {f.width, f.barrel, f.pinSize, f.fit, f.turn, f.reach, f.gap, f.lip})
        if (!std::isfinite(v))
            throw std::invalid_argument("A hinge setting is broken.");
    if (f.fit < 0.0 || f.fit > 1.0)
        throw std::invalid_argument("The fit must be from 0 to 1 mm.");
    if (f.reach < 0.0)
        throw std::invalid_argument("The reach can't be less than 0.");
    if (f.kind == HingeKind::Swivel) {
        if (!(f.width >= 1.0))
            throw std::invalid_argument("The swivel's peg must be at least 1 mm across.");
        if (!(f.barrel >= 1.0))
            throw std::invalid_argument("The swivel's peg must go in at least 1 mm.");
        if (!(f.lip > 0.0) || f.lip > f.width / 4.0)
            throw std::invalid_argument("The lip must be more than 0 and no more than a quarter of the peg.");
        if (lipHeight(f) > f.barrel * 0.6)
            throw std::invalid_argument("The swivel's peg is too short for its lip: make it go in further.");
        return;
    }
    if (!(f.width >= 0.8))
        throw std::invalid_argument("The tab must be at least 0.8 mm thick.");
    if (!(f.pinSize >= 0.5))
        throw std::invalid_argument("The pin must be at least 0.5 mm across.");
    if (!(f.barrel >= tabHole(f) + 1.0))
        throw std::invalid_argument("The tab's end must be at least 1 mm wider than the pin hole (0.5 mm of wall "
                                    "each side).");
    if (f.kind == HingeKind::Double && f.gap < 0.0)
        throw std::invalid_argument("The gap between the parts can't be less than 0.");
}

double reachOf(const HingeFinish& f)
{
    if (f.reach > 0.0)
        return f.reach;
    return f.kind == HingeKind::Swivel ? f.width + 2.0 * f.lip + 2.0 : 2.0 * f.barrel + f.width;
}

double pinDepth(const HingeFinish& f) { return f.barrel / 2.0 + f.fit; }

Vec3 pinAxis(const HingeFinish& f, const Vec3& normal)
{
    const Vec3 n = unit(normal);
    const Vec3 up = emboss::upFor(n);
    const Vec3 x = unit(cross(up, n));
    const Vec3 y = cross(n, x);
    const double a = f.turn * kPi / 180.0;
    return unit(add(scaled(x, std::cos(a)), scaled(y, std::sin(a))));
}

double armHole(const HingeFinish& f) { return f.pinSize + f.fit; }

double tabHole(const HingeFinish& f) { return f.pinSize + (f.stiff ? 0.5 : 2.0) * f.fit; }

std::vector<Mesh> forkCutters(const HingeFinish& f, const Vec3& point, const Vec3& normal)
{
    check(f);
    const Frame fr = frameAt(f, point, normal);
    const double g = f.fit, reach = reachOf(f);
    std::vector<Mesh> cuts;
    if (f.kind == HingeKind::Swivel) {
        const double r = f.width / 2.0, depth = f.barrel;
        cuts.push_back(round(fr, r + g, r + g, -kOut, depth + g, true));
        // The groove the lip snaps into.
        cuts.push_back(round(fr, r + f.lip + g, r + f.lip + g, depth - lipHeight(f) - g, depth + g, true));
        return cuts;
    }
    const double half = f.width / 2.0 + g, d = pinDepth(f), r = f.barrel / 2.0 + g;
    // The slot, down to the pin: the tab stands straight in it.
    cuts.push_back(box(fr, -half, half, -reach, reach, -kOut, d));
    // Room round the tab's rounded end, so it turns.
    cuts.push_back(round(alongPin(fr, d), r, r, -half, half, true));
    // Deeper on the side(s) it bends to: room for the tab lying flat.
    if (f.swing != HingeSwing::Back)
        cuts.push_back(box(fr, -half, half, 0.0, reach, -kOut, d + r));
    if (f.swing != HingeSwing::Forward)
        cuts.push_back(box(fr, -half, half, -reach, 0.0, -kOut, d + r));
    // The pin hole, right through the arms.
    cuts.push_back(round(alongPin(fr, d), armHole(f) / 2.0, armHole(f) / 2.0, -reach, reach, true));
    return cuts;
}

TabHalf tabHalf(const HingeFinish& f, const Vec3& point, const Vec3& normal)
{
    check(f);
    if (f.kind == HingeKind::Double)
        throw std::invalid_argument("A double hinge's middle is a link of its own: make a fork in each part.");
    const Frame fr = frameAt(f, point, normal);
    const double reach = reachOf(f);
    TabHalf t;
    if (f.kind == HingeKind::Swivel) {
        const double deep = f.barrel + f.lip + f.fit + reach;
        t.clear = box(fr, -reach, reach, -reach, reach, -f.fit, deep);
        t.add = meshOf(pegBody(f, fr));
        t.cut.push_back(pegSlit(f, fr));
        return t;
    }
    const double deep = pinDepth(f) + f.barrel / 2.0 + f.fit + reach;
    t.clear = box(fr, -reach, reach, -reach, reach, -f.fit, deep);
    const double r = f.barrel / 2.0, half = f.width / 2.0, d = pinDepth(f);
    t.add = meshOf(meshkernel::merged(
        {solid(round(alongPin(fr, d), r, r, -half, half, false)), solid(box(fr, -half, half, -r, r, -stemOut(f), d))}));
    t.cut.push_back(round(alongPin(fr, d), tabHole(f) / 2.0, tabHole(f) / 2.0, -half - 1, half + 1, true));
    return t;
}

std::vector<Piece> pieces(const HingeFinish& f, const Vec3& point, const Vec3& normal, bool withTab,
                          std::array<double, 2> pinSpan)
{
    check(f);
    const Frame fr = frameAt(f, point, normal);
    std::vector<Piece> out;
    if (f.kind == HingeKind::Swivel) {
        if (withTab) {
            Mesh peg = meshOf(meshkernel::subtracted(pegBody(f, fr), {solid(pegSlit(f, fr))}));
            out.push_back({"Swivel peg " + std::string(kDia) + num(f.width), std::move(peg)});
        }
        return out;
    }
    if (f.kind == HingeKind::Double)
        out.push_back({"Hinge link " + num(f.width) + kTimes + kDia + num(f.barrel), meshOf(linkSolid(f, fr))});
    else if (withTab)
        out.push_back({"Hinge tab " + num(f.width) + kTimes + kDia + num(f.barrel), meshOf(tabSolid(f, fr))});
    if (f.pin == PinKind::Printed) {
        if (!(pinSpan[0] > 0.0) || !(pinSpan[1] > 0.0))
            pinSpan = {reachOf(f), reachOf(f)};
        const double len = pinSpan[0] + pinSpan[1];
        const double d = pinDepth(f);
        const auto pin = [&](double depth) {
            return round(alongPin(fr, depth), f.pinSize / 2.0, f.pinSize / 2.0, -pinSpan[0], pinSpan[1], false);
        };
        const std::string name = "Hinge pin " + std::string(kDia) + num(f.pinSize) + kTimes + num(len);
        out.push_back({name, pin(d)});
        if (f.kind == HingeKind::Double)
            out.push_back({name, pin(d - (2.0 * d + f.gap))});
    }
    return out;
}

Half otherHalf(const HingeFinish& f, const Vec3& point, const Vec3& normal)
{
    Half h{point, unit(normal), f};
    if (f.kind != HingeKind::Double) {
        h.finish.half = f.half == HingeHalf::Fork ? HingeHalf::Tab : HingeHalf::Fork;
        return h;
    }
    const Vec3 n = unit(normal);
    const Vec3 axis = pinAxis(f, n);
    // The second fork's face is `gap` out from the first, facing back at it.
    h.point = add(point, scaled(n, f.gap));
    h.normal = scaled(n, -1.0);
    h.finish.half = HingeHalf::Fork;
    // Turned so its pin lies along the first one's.
    HingeFinish straight = f;
    straight.turn = 0.0;
    const Vec3 x = pinAxis(straight, h.normal);
    const Vec3 y = cross(h.normal, x);
    const double dx = axis[0] * x[0] + axis[1] * x[1] + axis[2] * x[2];
    const double dy = axis[0] * y[0] + axis[1] * y[1] + axis[2] * y[2];
    h.finish.turn = std::atan2(dy, dx) * 180.0 / kPi;
    // Seen from the other side, forward is back.
    if (f.swing == HingeSwing::Forward)
        h.finish.swing = HingeSwing::Back;
    else if (f.swing == HingeSwing::Back)
        h.finish.swing = HingeSwing::Forward;
    return h;
}

std::string describe(const HingeFinish& f)
{
    if (f.kind == HingeKind::Swivel)
        return std::string(f.half == HingeHalf::Fork ? "Swivel bore " : "Swivel peg ") + kDia + num(f.width) + kTimes +
               num(f.barrel);
    std::string text = f.kind == HingeKind::Double ? "Double hinge fork " :
                       f.half == HingeHalf::Fork  ? "Hinge fork " :
                                                    "Hinge tab ";
    text += num(f.width) + kTimes + kDia + num(f.barrel) + ", ";
    switch (f.pin) {
    case PinKind::Printed: text += "printed pin "; break;
    case PinKind::Filament: text += "filament pin "; break;
    case PinKind::Rod: text += "rod "; break;
    }
    return text + kDia + num(f.pinSize);
}

Body pieceBody(const Mesh& mesh, const std::string& name)
{
    if (mesh.indices.empty())
        throw std::invalid_argument("The piece came out empty.");
    Vec3 lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
    for (std::size_t i = 0; i + 2 < mesh.positions.size(); i += 3)
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::min(lo[c], double(mesh.positions[i + c]));
            hi[c] = std::max(hi[c], double(mesh.positions[i + c]));
        }
    // Centred like a primitive (middle of X and Y at 0, lowest point at Z = 0), and
    // moved back to where it was.
    const Vec3 offset{(lo[0] + hi[0]) / 2.0, (lo[1] + hi[1]) / 2.0, lo[2]};
    auto data = std::make_shared<MeshData>();
    data->positions.reserve(mesh.positions.size());
    for (std::size_t i = 0; i + 2 < mesh.positions.size(); i += 3)
        for (int c = 0; c < 3; ++c)
            data->positions.push_back(float(mesh.positions[i + c] - offset[c]));
    data->indices = mesh.indices;
    data->size = {float(std::max(hi[0] - lo[0], 1e-6)), float(std::max(hi[1] - lo[1], 1e-6)),
                  float(std::max(hi[2] - lo[2], 1e-6))};
    data->closed = true;
    MeshSpec spec;
    spec.data = data;
    spec.width = data->size[0];
    spec.depth = data->size[1];
    spec.height = data->size[2];
    Body body;
    body.name = name;
    body.spec = spec;
    body.placement.x = offset[0];
    body.placement.y = offset[1];
    body.placement.z = offset[2];
    return body;
}

}  // namespace hinge

namespace {

Vec3 unitOf(const Vec3& a)
{
    const double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    if (!(l > 1e-12))
        throw std::invalid_argument("The hinge's direction is missing");
    return {a[0] / l, a[1] / l, a[2] / l};
}

const Body& hingeStep(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Hinge || !body->boolean->hinge)
        throw std::invalid_argument("Not a hinge");
    return *body;
}

}  // namespace

BooleanSpec hingeSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                      const HingeFinish& finish)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    hinge::check(finish);
    BooleanSpec spec;
    spec.op = BooleanOp::Hinge;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.planePoint = toStep.apply(where.point);
    spec.planeNormal = unitOf(toStep.applyDirection(where.normal));
    spec.hinge = finish;
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

emboss::SurfacePoint hingeSpot(const Document& doc, ObjectId result)
{
    const Body& body = hingeStep(doc, result);
    const Transform t = doc.worldTransform(result);
    return {t.apply(body.boolean->planePoint), unitOf(t.applyDirection(body.boolean->planeNormal))};
}

Body withHinge(const Document& doc, ObjectId result, const emboss::SurfacePoint& where, const HingeFinish& finish)
{
    const Body& body = hingeStep(doc, result);
    hinge::check(finish);
    const Transform toLocal = doc.worldTransform(result).inverse();
    Body next = body;
    next.boolean->planePoint = toLocal.apply(where.point);
    next.boolean->planeNormal = unitOf(toLocal.applyDirection(where.normal));
    next.boolean->hinge = finish;
    return next;
}

ObjectId hingePartner(const Document& doc, ObjectId result)
{
    const Body& body = hingeStep(doc, result);
    const std::uint32_t link = body.boolean->hinge->link;
    if (link == 0)
        return kInvalidId;
    for (const Body& b : doc.bodies())
        if (b.id != result && b.isBoolean() && b.boolean->op == BooleanOp::Hinge && b.boolean->hinge &&
            b.boolean->hinge->link == link)
            return b.id;
    return kInvalidId;
}

}  // namespace forge
