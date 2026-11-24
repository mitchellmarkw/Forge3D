// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/SplitJoint.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/LimbCut.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/PrintSplit.h"
#include "forge/core/Socket.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace splitjoint {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a, const Vec3& fallback = {0, 0, 1})
{
    const double l = length(a);
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}

std::string mm(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.3g", v);
    return buf;
}

double halfStep(double v) { return std::round(v * 2.0) / 2.0; }

// A closed round slab: from `a` along `axis` for `height`, `radius` round.
Mesh disc(const Vec3& a, const Vec3& axis, double radius, double height)
{
    const Vec3 n = unit(axis);
    const Vec3 u = unit(cross(std::abs(n[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}, n));
    const Vec3 v = cross(n, u);
    const int around = 48;
    Mesh m;
    const auto put = [&](const Vec3& p) {
        m.positions.insert(m.positions.end(), {float(p[0]), float(p[1]), float(p[2])});
    };
    const Vec3 b = add(a, mul(n, height));
    put(a);
    put(b);
    for (int k = 0; k < around; ++k) {
        const double t = 2.0 * std::numbers::pi * k / around;
        const Vec3 off = add(mul(u, radius * std::cos(t)), mul(v, radius * std::sin(t)));
        put(add(a, off));
        put(add(b, off));
    }
    const auto lo = [](int k) { return std::uint32_t(2 + 2 * (k % 48)); };
    const auto hi = [](int k) { return std::uint32_t(3 + 2 * (k % 48)); };
    for (int k = 0; k < around; ++k) {
        m.indices.insert(m.indices.end(), {0, lo(k + 1), lo(k)});
        m.indices.insert(m.indices.end(), {1, hi(k), hi(k + 1)});
        m.indices.insert(m.indices.end(), {lo(k), lo(k + 1), hi(k + 1), lo(k), hi(k + 1), hi(k)});
    }
    return m;
}

// The step `spec` on `of`, keeping its name; the new step's id.
ObjectId step(CommandStack& stack, Document& doc, BooleanSpec spec, ObjectId of)
{
    Body s;
    s.id = doc.allocateId();
    s.name = doc.find(of)->name;
    s.boolean = std::move(spec);
    const ObjectId id = s.id;
    stack.execute(std::make_unique<CombineCommand>(std::move(s)));
    return id;
}

// `op` of `piece` with a shape made from a closed surface (plate coordinates).
ObjectId withShape(CommandStack& stack, Document& doc, ObjectId piece, const Mesh& mesh, const std::string& name,
                   BooleanOp op)
{
    Body shape = hinge::pieceBody(mesh, name);
    shape.id = doc.allocateId();
    shape.hardware = true;  // keeps its size when rescaled
    const ObjectId shapeId = shape.id;
    stack.execute(std::make_unique<AddBodyCommand>(std::move(shape)));
    BooleanSpec spec;
    spec.op = op;
    spec.operands = {piece, shapeId};
    const Document::Preview preview = doc.previewResult(spec);
    if (preview.empty)
        throw std::invalid_argument("The joint left nothing of '" + doc.find(piece)->name + "'.");
    const auto c = preview.bounds.center();
    spec.pivot = {c[0], c[1], c[2]};
    return step(stack, doc, std::move(spec), piece);
}

// Distance from p to the segment a-b (2D).
double segDist(double px, double py, double ax, double ay, double bx, double by)
{
    const double dx = bx - ax, dy = by - ay;
    const double l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(px - ax - t * dx, py - ay - t * dy);
}

}  // namespace

const char* title(Kind kind)
{
    switch (kind) {
    case Kind::Ball: return "Ball joint";
    case Kind::Hinge: return "Hinge";
    case Kind::DoubleHinge: return "Double hinge";
    case Kind::Swivel: return "Swivel";
    case Kind::Magnets: return "Magnets";
    default: return "None";
    }
}

Spot planeSpot(const Document& doc, ObjectId id, const SplitPlane& plane)
{
    const Vec3 n = unit(plane.normal);
    const Vec3 u = unit(cross(std::abs(n[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}, n));
    const Vec3 v = cross(n, u);
    const auto loops3 = sectionLoops(doc, id, plane);
    std::vector<std::vector<std::array<double, 2>>> loops;
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
    for (const auto& l : loops3) {
        if (l.size() < 3)
            continue;
        auto& out = loops.emplace_back();
        for (const Vec3& p : l) {
            const Vec3 d = sub(p, plane.point);
            const std::array<double, 2> q{dot(d, u), dot(d, v)};
            out.push_back(q);
            for (int k = 0; k < 2; ++k) {
                lo[k] = std::min(lo[k], q[std::size_t(k)]);
                hi[k] = std::max(hi[k], q[std::size_t(k)]);
            }
        }
    }
    if (loops.empty())
        throw std::invalid_argument("The plane doesn't cut through the shape. Move it so it crosses the shape.");
    // How far in from the cut's edges a point is (negative: outside the cut).
    const auto depth = [&](double x, double y) {
        bool inside = false;
        double d = 1e300;
        for (const auto& l : loops)
            for (std::size_t i = 0, j = l.size() - 1; i < l.size(); j = i++) {
                const auto& a = l[i];
                const auto& b = l[j];
                if ((a[1] > y) != (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0])
                    inside = !inside;
                d = std::min(d, segDist(x, y, a[0], a[1], b[0], b[1]));
            }
        return inside ? d : -d;
    };
    // The point farthest in: a grid over the cut, then finer grids round the best.
    double bx = 0.5 * (lo[0] + hi[0]), by = 0.5 * (lo[1] + hi[1]);
    double best = depth(bx, by);
    double cell = std::max(hi[0] - lo[0], hi[1] - lo[1]) / 40.0;
    for (int i = 0; i <= 40; ++i)
        for (int j = 0; j <= 40; ++j) {
            const double x = lo[0] + (hi[0] - lo[0]) * i / 40.0, y = lo[1] + (hi[1] - lo[1]) * j / 40.0;
            const double d = depth(x, y);
            if (d > best) {
                best = d;
                bx = x;
                by = y;
            }
        }
    for (int round = 0; round < 6; ++round) {
        const double cx = bx, cy = by;
        for (int i = -4; i <= 4; ++i)
            for (int j = -4; j <= 4; ++j) {
                const double x = cx + cell * i / 4.0, y = cy + cell * j / 4.0;
                const double d = depth(x, y);
                if (d > best) {
                    best = d;
                    bx = x;
                    by = y;
                }
            }
        cell *= 0.4;
    }
    if (best <= 0.0)
        throw std::invalid_argument("The cut is too thin for a joint.");
    Spot s;
    s.point = add(plane.point, add(mul(u, bx), mul(v, by)));
    s.normal = mul(n, -1.0);  // out of the side the plane faces
    s.room = best;
    for (const auto& l : loops)
        for (const auto& q : l)
            s.reach = std::max(s.reach, std::hypot(q[0] - bx, q[1] - by));
    return s;
}

Spot maskSpot(const Document& doc, ObjectId id, const std::vector<MaskDab>& dabs, MaskCut cut)
{
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(doc.meshSolid(id), pos, idx);
    const Transform toPlate = doc.worldTransform(id);
    for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
        const Vec3 p = toPlate.apply(Vec3{pos[i], pos[i + 1], pos[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            pos[i + k] = float(p[k]);
    }
    const mask::Pieces pieces = mask::split(pos, idx, dabs, cut == MaskCut::Dome ? MaskCut::Flat : cut);
    if (pieces.keySpots.empty())
        throw std::invalid_argument("The mask has no edge to put a joint on.");
    // On the main cut: the biggest (small gaps and specks in the paint make little ones).
    const mask::KeySpot& k =
        *std::max_element(pieces.keySpots.begin(), pieces.keySpots.end(),
                          [](const mask::KeySpot& a, const mask::KeySpot& b) { return a.reach < b.reach; });
    Spot s;
    s.point = k.point;
    s.normal = unit(k.normal);
    s.room = k.room;
    s.reach = k.reach;
    return s;
}

SocketFinish ballFor(const Options& o, double room)
{
    // As Cut into Parts: the ball half as wide as the cut is across.
    SocketFinish f = limbcut::ballFor(room);
    if (o.size > 0.0)
        f.size = o.size;
    f.fit = o.fit;
    f.resin = o.fit < 0.15;
    return f;
}

HingeFinish hingeFor(const Options& o, double room, HingeKind kind)
{
    HingeFinish f = limbcut::hingeFor(room, kind);
    if (o.size > 0.0)
        f.width = o.size;
    f.fit = o.fit;
    return f;
}

SocketFinish magnetFor(const Options& o, double room)
{
    SocketFinish f;
    f.kind = SocketKind::Magnet;
    f.fit = o.fit;
    f.resin = o.fit < 0.15;
    if (o.size > 0.0) {
        f.size = o.size;
    } else {
        // The biggest usual size with a millimetre of wall round it.
        f.size = 0.0;
        for (double d : {2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0})
            if (d / 2.0 + o.fit + 1.0 <= room)
                f.size = d;
        if (f.size == 0.0)
            f.size = 2.0;
    }
    f.depth = f.size <= 3.0 ? 1.0 : f.size <= 6.0 ? 2.0 : 3.0;
    return f;
}

double gapFor(const Options& o, double room)
{
    limbcut::Choice c;
    switch (o.kind) {
    case Kind::Ball:
        c.kind = limbcut::JointKind::Ball;
        c.ball = ballFor(o, room);
        break;
    case Kind::DoubleHinge:
        c.kind = limbcut::JointKind::Hinge;
        c.hinge = hingeFor(o, room, HingeKind::Double);
        break;
    default: return 0.0;
    }
    return limbcut::gapFor(c);
}

Made addJoint(CommandStack& stack, Document& doc, ObjectId first, ObjectId second, const Spot& spot, double gap,
              const Options& o, std::uint32_t link)
{
    Made made;
    const Vec3 out = unit(spot.normal);  // out of the first piece
    const Vec3 n = mul(out, -1.0);        // from the second into the first, as Cut into Parts has it
    const Vec3 firstAt = spot.point, secondAt = add(spot.point, mul(out, gap));
    const double room = spot.room;
    switch (o.kind) {
    case Kind::None: break;
    case Kind::Ball: {
        const SocketFinish f = ballFor(o, room);
        socket::check(f);
        const socket::BallSeat seat = socket::ballSeat(f);
        if (seat.radius + 0.6 > room)
            throw std::invalid_argument("The cut is too small for a " + mm(f.size) + " mm ball joint: it needs " +
                                        mm(2 * (seat.radius + 0.6)) + " mm across, and has " + mm(2 * room) +
                                        ". Choose a smaller ball.");
        first = step(stack, doc, socketSpec(doc, first, {firstAt, out}, f), first);
        second = withShape(stack, doc, second, limbcut::ballOnNeck(f, firstAt, out, secondAt), "Ball", BooleanOp::Merge);
        break;
    }
    case Kind::Hinge:
    case Kind::DoubleHinge:
    case Kind::Swivel: {
        const HingeKind kind = o.kind == Kind::Hinge         ? HingeKind::Single
                               : o.kind == Kind::DoubleHinge ? HingeKind::Double
                                                             : HingeKind::Swivel;
        HingeFinish f = hingeFor(o, room, kind);
        f.half = HingeHalf::Fork;
        f.link = link;
        if (kind == HingeKind::Double)
            f.gap = gap;
        // The slot and pin hole across the whole cut, not on into the rest of the piece.
        if (kind != HingeKind::Swivel)
            f.reach = std::max(1.05 * spot.reach, 0.5 * f.width + 1.0);
        if (kind == HingeKind::Swivel && f.width / 2.0 + f.fit + 0.6 > room)
            throw std::invalid_argument("The cut is too small for a " + mm(f.width) + " mm swivel. Choose a smaller one.");
        if (kind != HingeKind::Swivel && f.width + 2.0 * f.fit + 1.2 > 2.0 * room)
            throw std::invalid_argument("The cut is too thin for a " + mm(f.width) + " mm hinge. Choose a thinner one.");
        hinge::check(f);
        const hinge::Half other = hinge::otherHalf(f, secondAt, n);
        second = step(stack, doc, hingeSpec(doc, second, {secondAt, n}, f), second);
        first = step(stack, doc, hingeSpec(doc, first, {other.point, other.normal}, other.finish), first);
        const double span = std::max(0.95 * room, 0.5 * f.width + 0.5);
        for (const hinge::Piece& piece : hinge::pieces(f, secondAt, n, false, {span, span})) {
            Body extra = hinge::pieceBody(piece.mesh, piece.name);
            extra.id = doc.allocateId();
            extra.hardware = true;
            made.extras.push_back(extra.id);
            stack.execute(std::make_unique<AddBodyCommand>(std::move(extra)));
        }
        break;
    }
    case Kind::Magnets: {
        const SocketFinish f = magnetFor(o, room);
        socket::check(f);
        if (f.size / 2.0 + f.fit + 0.4 > room)
            throw std::invalid_argument("The cut is too small for " + mm(f.size) + " mm magnets. Choose smaller ones.");
        first = step(stack, doc, socketSpec(doc, first, {firstAt, out}, f), first);
        second = step(stack, doc, socketSpec(doc, second, {secondAt, n}, f), second);
        break;
    }
    }
    made.pieces = {first, second};
    return made;
}

Made splitWithJoint(CommandStack& stack, Document& doc, ObjectId id, const SplitPlane& plane, const Options& o,
                    std::uint32_t link)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    const Spot spot = planeSpot(doc, id, plane);
    const double gap = gapFor(o, spot.room);
    const std::string name = body->name;
    Made made;
    stack.beginMacro("Split " + name + " with a " + [&] {
        std::string t = title(o.kind);
        t[0] = char(std::tolower(static_cast<unsigned char>(t[0])));
        return t;
    }());
    try {
        const std::vector<ObjectId> halves = splitBody(stack, doc, id, plane, SplitKeep::Both);
        const ObjectId front = halves.at(0), back = halves.at(1);
        if (gap > 0.0) {
            // The other side cut back, for the ball's neck to tilt (or the link to sit).
            const Vec3 n = unit(plane.normal);
            const Body before = *doc.find(back);
            const SplitPlane own = splitPlaneOf(doc, back);  // facing the back's way
            const Body after = withSplitPlane(doc, back, {sub(plane.point, mul(n, gap)), own.normal});
            stack.execute(std::make_unique<EditBodyCommand>(before, after, "Move cut"));
        }
        made = addJoint(stack, doc, front, back, spot, gap, o, link);
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

Made splitByMaskWithJoint(CommandStack& stack, Document& doc, ObjectId id, const std::vector<MaskDab>& dabs,
                          MaskCut cut, const Options& o, std::uint32_t link)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (cut == MaskCut::Dome)
        throw std::invalid_argument("A ball and socket cut already joins the pieces: choose a flat or curved cut for a "
                                    "joint.");
    const Spot spot = maskSpot(doc, id, dabs, cut);
    const double gap = gapFor(o, spot.room);
    const std::string name = body->name;
    Made made;
    stack.beginMacro("Split " + name + " by mask with a joint");
    try {
        const std::vector<ObjectId> pieces = splitByMask(stack, doc, id, dabs, SplitKeep::Both, -1.0, cut);
        ObjectId masked = pieces.at(0), rest = pieces.at(1);
        if (gap > 0.0)  // the rest's face cut back round the joint
            rest = withShape(stack, doc, rest, disc(sub(spot.point, mul(spot.normal, 0.01)), spot.normal,
                                                    spot.reach + 0.5, gap + 0.01),
                             "Gap", BooleanOp::Subtract);
        made = addJoint(stack, doc, masked, rest, spot, gap, o, link);
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

}  // namespace splitjoint

}  // namespace forge
