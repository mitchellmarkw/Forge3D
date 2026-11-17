// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Thread.h"

#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Shell.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepTools.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace {

double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 0 ? Vec3{a[0] / l, a[1] / l, a[2] / l} : Vec3{0, 0, 1};
}

// Two directions square to the axis and to each other.
std::pair<Vec3, Vec3> across(const Vec3& axis)
{
    const Vec3 ref = std::abs(axis[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    const Vec3 e1 = unit(cross(ref, axis));
    return {e1, cross(axis, e1)};
}

double signedVolume(const Mesh& m)
{
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &m.positions[3 * m.indices[t]];
        const float* b = &m.positions[3 * m.indices[t + 1]];
        const float* c = &m.positions[3 * m.indices[t + 2]];
        v += (a[0] * (double(b[1]) * c[2] - double(b[2]) * c[1]) - a[1] * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
              a[2] * (double(b[0]) * c[1] - double(b[1]) * c[0])) /
             6.0;
    }
    return v;
}

// Faces turned to point outwards (the volume comes out positive).
void outwards(Mesh& m)
{
    if (signedVolume(m) < 0)
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
            std::swap(m.indices[t + 1], m.indices[t + 2]);
}

// A round tube (inner radius r0; 0 for a solid round bar) along the axis, from..to.
Mesh tube(const thread::Round& round, double r0, double r1, double from, double to, int sides)
{
    Mesh m;
    const auto [e1, e2] = across(round.axis);
    const auto at = [&](double r, double phi, double s) {
        const double c = std::cos(phi) * r, n = std::sin(phi) * r;
        for (int k = 0; k < 3; ++k)
            m.positions.push_back(float(round.axisPoint[k] + round.axis[k] * s + e1[k] * c + e2[k] * n));
    };
    const bool solid = r0 <= 0.0;
    // Rings: outer bottom, outer top, then (a tube) inner bottom, inner top.
    for (double r : solid ? std::vector<double>{r1} : std::vector<double>{r1, r0})
        for (double s : {from, to})
            for (int i = 0; i < sides; ++i)
                at(r, 2.0 * std::numbers::pi * i / sides, s);
    const auto ring = [&](int which, int i) { return std::uint32_t(which * sides + (i % sides)); };
    const auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.indices.insert(m.indices.end(), {a, b, c, a, c, d});
    };
    for (int i = 0; i < sides; ++i) {
        quad(ring(0, i), ring(0, i + 1), ring(1, i + 1), ring(1, i));  // outside
        if (solid)
            continue;
        quad(ring(2, i), ring(3, i), ring(3, i + 1), ring(2, i + 1));  // inside
        quad(ring(0, i), ring(2, i), ring(2, i + 1), ring(0, i + 1));  // bottom
        quad(ring(1, i), ring(1, i + 1), ring(3, i + 1), ring(3, i));  // top
    }
    if (solid) {
        // Ends: fans round a middle point.
        const auto middle = [&](double s) {
            at(0.0, 0.0, s);
            return std::uint32_t(m.positions.size() / 3 - 1);
        };
        const std::uint32_t bottom = middle(from), top = middle(to);
        for (int i = 0; i < sides; ++i) {
            m.indices.insert(m.indices.end(), {bottom, ring(0, i + 1), ring(0, i)});
            m.indices.insert(m.indices.end(), {top, ring(1, i), ring(1, i + 1)});
        }
    }
    outwards(m);
    return m;
}

// The thread's groove: a four-sided section (radius rA half-width hA, radius rB
// half-width hB) swept round a helix, from `start` along the axis for `turns` turns.
Mesh helix(const thread::Round& round, double pitch, bool leftHand, double rA, double hA, double rB, double hB,
           double start, double turns)
{
    constexpr int kPerTurn = 64;
    const int steps = std::max(2, int(std::ceil(turns * kPerTurn)));
    const auto [e1, e2] = across(round.axis);
    const double hand = leftHand ? -1.0 : 1.0;
    const double section[4][2] = {{rA, -hA}, {rB, -hB}, {rB, hB}, {rA, hA}};
    Mesh m;
    for (int k = 0; k <= steps; ++k) {
        const double theta = 2.0 * std::numbers::pi * turns * double(k) / steps;
        const double s = start + pitch * theta / (2.0 * std::numbers::pi);
        const double phi = hand * theta;
        for (const auto& [r, ds] : section)
            for (int j = 0; j < 3; ++j)
                m.positions.push_back(float(round.axisPoint[j] + round.axis[j] * (s + ds) +
                                            (e1[j] * std::cos(phi) + e2[j] * std::sin(phi)) * r));
    }
    const auto v = [](int k, int j) { return std::uint32_t(4 * k + (j % 4)); };
    for (int k = 0; k < steps; ++k)
        for (int j = 0; j < 4; ++j)
            m.indices.insert(m.indices.end(),
                             {v(k, j), v(k + 1, j), v(k + 1, j + 1), v(k, j), v(k + 1, j + 1), v(k, j + 1)});
    // Ends: each the reverse of how the sides go round it.
    m.indices.insert(m.indices.end(), {v(0, 0), v(0, 1), v(0, 2), v(0, 0), v(0, 2), v(0, 3)});
    m.indices.insert(m.indices.end(),
                     {v(steps, 0), v(steps, 2), v(steps, 1), v(steps, 0), v(steps, 3), v(steps, 2)});
    outwards(m);
    return m;
}

TopoDS_Shape placedPart(const Document& doc, ObjectId part)
{
    const Transform t = doc.localTransform(part);
    gp_Trsf trsf;
    trsf.SetValues(t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3], t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3], t.m[2][0],
                   t.m[2][1], t.m[2][2], t.m[2][3]);
    return BRepBuilderAPI_Transform(doc.shape(part), trsf, true).Shape();
}

FaceRef moved(const FaceRef& r, const Transform& t)
{
    FaceRef out = r;
    out.point = t.apply(r.point);
    out.normal = unit(t.applyDirection(r.normal));
    return out;
}

const Body& threadResult(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Thread || !body->boolean->thread)
        throw std::invalid_argument("Not a threaded shape");
    return *body;
}

}  // namespace

namespace thread {

const std::vector<Size>& metric()
{
    static const std::vector<Size> sizes{
        {"M2", 2, 0.4},   {"M2.5", 2.5, 0.45}, {"M3", 3, 0.5}, {"M4", 4, 0.7},   {"M5", 5, 0.8},
        {"M6", 6, 1.0},   {"M8", 8, 1.25},     {"M10", 10, 1.5}, {"M12", 12, 1.75}, {"M14", 14, 2.0},
        {"M16", 16, 2.0}, {"M20", 20, 2.5},    {"M24", 24, 3.0}, {"M30", 30, 3.5},
    };
    return sizes;
}

std::optional<Round> roundFace(const TopoDS_Shape& shape, int face)
{
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    if (face < 0 || face >= faces.Extent())
        return std::nullopt;
    const TopoDS_Face f = TopoDS::Face(faces(face + 1));
    BRepAdaptor_Surface surface(f);
    if (surface.GetType() != GeomAbs_Cylinder)
        return std::nullopt;
    const gp_Cylinder c = surface.Cylinder();
    double u0, u1, v0, v1;
    BRepTools::UVBounds(f, u0, u1, v0, v1);
    Round r;
    r.radius = c.Radius();
    r.axisPoint = {c.Location().X(), c.Location().Y(), c.Location().Z()};
    r.axis = {c.Axis().Direction().X(), c.Axis().Direction().Y(), c.Axis().Direction().Z()};
    r.from = std::min(v0, v1);
    r.to = std::max(v0, v1);
    // Which way it faces, from the side the material is on.
    gp_Pnt p;
    gp_Vec n;
    BRepGProp_Face(f).Normal((u0 + u1) / 2, (v0 + v1) / 2, p, n);
    const Vec3 out{p.X() - r.axisPoint[0], p.Y() - r.axisPoint[1], p.Z() - r.axisPoint[2]};
    r.hole = n.X() * out[0] + n.Y() * out[1] + n.Z() * out[2] < 0.0;
    return r;
}

Size suggest(const Round& round)
{
    const auto& sizes = metric();
    const auto error = [&](const Size& s) {
        // A hole is drilled for the thread: about its diameter less one pitch.
        return std::abs((round.hole ? s.diameter - s.pitch : s.diameter) - 2.0 * round.radius);
    };
    return *std::min_element(sizes.begin(), sizes.end(),
                             [&](const Size& a, const Size& b) { return error(a) < error(b); });
}

Cut cutFor(const Round& round, const ThreadFinish& f)
{
    if (!(f.pitch > 0.0) || !(f.size > 0.0))
        throw std::invalid_argument("A thread needs a size and a pitch above zero");
    if (!(round.to > round.from))
        throw std::invalid_argument("The round face has no length to thread");
    if ((round.to - round.from) / f.pitch > 2000.0)
        throw std::invalid_argument("That pitch is too fine for this length (over 2000 turns): choose a bigger pitch");
    const double P = f.pitch;
    const double depth = 0.6134 * P;    // ISO: the outside thread's depth
    const double crestHalf = 0.4375 * P;  // the groove's half-width where it meets the crest
    const double slope = std::tan(std::numbers::pi / 6.0);  // 60° thread: 30° each flank
    const double over = 0.05 * P;       // the groove goes a little past the surface
    Cut cut;
    double rA, hA, rB, hB, clearTo = 0.0;
    if (!round.hole) {
        // A peg: the thread's crest at its outside diameter (less the fit), if the peg is bigger.
        const double crest = std::min(round.radius, f.size / 2.0 - f.fit);
        if (crest - depth <= 0.0)
            throw std::invalid_argument("The thread is too deep for this peg: choose a smaller pitch");
        if (round.radius > crest + 1e-6)
            cut.clear = tube(round, crest, round.radius + over, round.from, round.to, 96);
        rA = crest - depth;
        hA = crestHalf - depth * slope;
        rB = crest + over;
        hB = crestHalf + over * slope;
        clearTo = rB;
    } else {
        // A hole: the thread's crest at its inside diameter (plus the fit), if the hole is smaller.
        const double crest = std::max(round.radius, f.size / 2.0 - 0.5413 * P + f.fit);
        if (crest > round.radius + 1e-6)
            cut.clear = tube(round, 0.0, crest, round.from, round.to, 96);
        rA = crest - over;
        hA = crestHalf + over * slope;
        rB = crest + depth;
        hB = crestHalf - depth * slope;
        clearTo = rB;
    }
    // Long enough to run past both ends, then trimmed to the face's length (so it
    // doesn't cut into what's beyond, such as the plate a peg stands on).
    const double start = round.from - P;
    const double turns = (round.to - round.from + 2.0 * P) / P;
    const Mesh groove = helix(round, P, f.leftHand, rA, hA, rB, hB, start, turns);
    const Mesh slab = tube(round, 0.0, clearTo + 1.0, round.from, round.to, 96);
    const MeshSolid trimmed = meshkernel::intersected(
        {meshkernel::fromTriangles(groove.positions, groove.indices), meshkernel::fromTriangles(slab.positions, slab.indices)});
    meshkernel::triangles(trimmed, cut.groove.positions, cut.groove.indices);
    return cut;
}

}  // namespace thread

BooleanSpec threadSpec(const Document& doc, ObjectId target, const ThreadFinish& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (doc.usesMeshEngine(target))
        throw std::invalid_argument("Threads go on solid shapes, not on imported meshes or shapes made with them.");
    BooleanSpec spec;
    spec.op = BooleanOp::Thread;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    ThreadFinish local = inPlate;
    local.face = moved(local.face, toStep);
    std::vector<FaceRef> refs{local.face};
    shell::locate(placedPart(doc, target), refs);
    local.face = refs.front();
    spec.thread = local;
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

ThreadFinish threadInPlate(const Document& doc, ObjectId result)
{
    const Body& body = threadResult(doc, result);
    ThreadFinish f = *body.boolean->thread;
    f.face = moved(f.face, doc.worldTransform(result));
    return f;
}

Body withThread(const Document& doc, ObjectId result, const ThreadFinish& inPlate)
{
    Body next = threadResult(doc, result);
    ThreadFinish f = inPlate;
    f.face = moved(f.face, doc.worldTransform(result).inverse());
    std::vector<FaceRef> refs{f.face};
    shell::locate(placedPart(doc, next.boolean->operands.front()), refs);
    f.face = refs.front();
    next.boolean->thread = f;
    return next;
}

std::optional<thread::Round> threadedFace(const Document& doc, const BooleanSpec& spec)
{
    if (spec.op != BooleanOp::Thread || !spec.thread || spec.operands.size() != 1)
        return std::nullopt;
    const TopoDS_Shape part = placedPart(doc, spec.operands.front());
    const std::vector<int> found = shell::match(part, {spec.thread->face});
    if (found.empty() || found.front() < 0)
        return std::nullopt;
    return thread::roundFace(part, found.front());
}

}  // namespace forge
