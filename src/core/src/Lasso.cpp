// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Lasso.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace forge::lasso {

namespace {

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 0 ? mul(a, 1.0 / l) : Vec3{0, 0, 1};
}

using P2 = std::array<double, 2>;

double segDist(const P2& p, const P2& a, const P2& b)
{
    const double dx = b[0] - a[0], dy = b[1] - a[1];
    const double l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const double ex = a[0] + t * dx - p[0], ey = a[1] + t * dy - p[1];
    return std::sqrt(ex * ex + ey * ey);
}

double orient(const P2& a, const P2& b, const P2& c)
{
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

bool segmentsCross(const P2& a, const P2& b, const P2& c, const P2& d)
{
    const double o1 = orient(a, b, c), o2 = orient(a, b, d), o3 = orient(c, d, a), o4 = orient(c, d, b);
    return ((o1 > 0) != (o2 > 0)) && ((o3 > 0) != (o4 > 0)) && o1 != 0 && o2 != 0 && o3 != 0 && o4 != 0;
}

// Ramer-Douglas-Peucker on an open run of points: which to keep.
void keepFrom(const std::vector<P2>& pts, std::size_t first, std::size_t last, double tol, std::vector<char>& keep)
{
    if (last <= first + 1)
        return;
    double worst = -1.0;
    std::size_t at = first;
    for (std::size_t i = first + 1; i < last; ++i) {
        const double d = segDist(pts[i], pts[first], pts[last]);
        if (d > worst) {
            worst = d;
            at = i;
        }
    }
    if (worst > tol) {
        keep[at] = 1;
        keepFrom(pts, first, at, tol, keep);
        keepFrom(pts, at, last, tol, keep);
    }
}

double area(const std::vector<P2>& outline)
{
    double a = 0.0;
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const P2& p = outline[i];
        const P2& q = outline[(i + 1) % outline.size()];
        a += p[0] * q[1] - q[0] * p[1];
    }
    return a / 2.0;
}

}  // namespace

Frame frameOf(const Loop& loop)
{
    Frame f;
    f.w = unit(mul(loop.along, -1.0));
    const Vec3 ref = std::abs(f.w[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    f.u = unit(cross(ref, f.w));
    f.v = cross(f.w, f.u);
    if (!loop.points.empty()) {
        Vec3 c{0, 0, 0};
        for (const Vec3& p : loop.points)
            c = add(c, p);
        c = mul(c, 1.0 / double(loop.points.size()));
        // On the loop's plane (its first point's, along w).
        f.origin = sub(c, mul(f.w, dot(sub(c, loop.points.front()), f.w)));
    }
    return f;
}

std::vector<std::array<double, 2>> flat(const Loop& loop, const Frame& frame)
{
    std::vector<P2> out;
    out.reserve(loop.points.size());
    for (const Vec3& p : loop.points) {
        const Vec3 d = sub(p, frame.origin);
        out.push_back({dot(d, frame.u), dot(d, frame.v)});
    }
    return out;
}

Loop simplified(const Loop& loop, double tolerance, std::size_t most)
{
    if (loop.points.size() < 4)
        return loop;
    const Frame f = frameOf(loop);
    const std::vector<P2> pts = flat(loop, f);
    // Split the ring at its first point and the point furthest from it.
    std::size_t far = 0;
    double best = -1.0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double dx = pts[i][0] - pts[0][0], dy = pts[i][1] - pts[0][1];
        if (dx * dx + dy * dy > best) {
            best = dx * dx + dy * dy;
            far = i;
        }
    }
    std::vector<P2> ring = pts;
    ring.push_back(pts.front());
    double tol = std::max(tolerance, 1e-9);
    for (int round = 0; round < 40; ++round, tol *= 1.5) {
        std::vector<char> keep(ring.size(), 0);
        keep[0] = keep[far] = keep[ring.size() - 1] = 1;
        keepFrom(ring, 0, far, tol, keep);
        keepFrom(ring, far, ring.size() - 1, tol, keep);
        Loop out;
        out.along = loop.along;
        for (std::size_t i = 0; i + 1 < ring.size(); ++i)
            if (keep[i])
                out.points.push_back(loop.points[i]);
        if (out.points.size() <= most || round == 39)
            return out;
    }
    return loop;
}

bool crossesItself(const Loop& loop)
{
    const std::vector<P2> p = flat(loop, frameOf(loop));
    const std::size_t n = p.size();
    if (n < 4)
        return false;
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1)
                continue;  // neighbours round the end
            if (segmentsCross(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n]))
                return true;
        }
    return false;
}

bool insideFlat(const std::vector<std::array<double, 2>>& outline, double x, double y)
{
    bool in = false;
    const std::size_t n = outline.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const P2& a = outline[i];
        const P2& b = outline[j];
        if ((a[1] > y) != (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0])
            in = !in;
    }
    return in;
}

double distanceFlat(const std::vector<std::array<double, 2>>& outline, double x, double y)
{
    double best = 1e300;
    const P2 p{x, y};
    for (std::size_t i = 0; i < outline.size(); ++i)
        best = std::min(best, segDist(p, outline[i], outline[(i + 1) % outline.size()]));
    return best;
}

Body cutter(const Loop& raw, const Bounds& through, const std::string& name)
{
    if (raw.points.size() < 3)
        throw std::invalid_argument("Draw a loop round the part to cut: it was too short.");
    // Small wobbles of the hand smoothed out (a 200th of the loop's size).
    double size = 0.0;
    {
        const Frame f = frameOf(raw);
        const auto p = flat(raw, f);
        double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
        for (const P2& q : p)
            for (int k = 0; k < 2; ++k) {
                lo[k] = std::min(lo[k], q[std::size_t(k)]);
                hi[k] = std::max(hi[k], q[std::size_t(k)]);
            }
        size = std::max(hi[0] - lo[0], hi[1] - lo[1]);
    }
    const Loop loop = simplified(raw, size / 200.0);
    const Frame f = frameOf(loop);
    std::vector<P2> outline = flat(loop, f);
    if (outline.size() < 3 || std::abs(area(outline)) < 1e-4 * size * size || size < 0.05)
        throw std::invalid_argument("Draw a loop round the part to cut: it was too small.");
    if (crossesItself(loop))
        throw std::invalid_argument("The loop crosses itself: draw it round once, without crossing.");
    if (area(outline) < 0)
        std::reverse(outline.begin(), outline.end());  // anticlockwise, like a drawn outline

    PadSpec pad;
    pad.sketch.points.clear();
    pad.sketch.points.push_back({0.0, 0.0});  // the sketch's origin
    for (const P2& q : outline)
        pad.sketch.points.push_back({q[0], q[1]});
    const int n = int(outline.size());
    for (int i = 0; i < n; ++i) {
        sketch::Geo g;
        g.kind = sketch::GeoKind::Line;
        g.a = 1 + i;
        g.b = 1 + (i + 1) % n;
        pad.sketch.geos.push_back(g);
    }
    // Right through the shape both ways, whatever side of it the loop's plane is.
    const auto c = through.center();
    const Vec3 centre{c[0], c[1], c[2]};
    const double reach = std::abs(dot(sub(centre, f.origin), f.w)) + double(through.radius()) + 1.0;
    pad.length = 2.0 * reach;
    pad.midplane = true;

    Body b;
    b.name = name;
    b.spec = pad;
    Transform t;
    for (int r = 0; r < 3; ++r) {
        const auto k = std::size_t(r);
        t.m[k] = {f.u[k], f.v[k], f.w[k], f.origin[k]};
    }
    b.placement = placementFromTransform(t, pivotOf(b));
    return b;
}

const char* title(Keep keep)
{
    switch (keep) {
    case Keep::Outside: return "Cut away inside the loop";
    case Keep::Inside: return "Keep inside the loop";
    case Keep::Both: return "Both pieces";
    }
    return "";
}

std::vector<ObjectId> cut(CommandStack& stack, Document& doc, ObjectId id, const Loop& loop, Keep keep)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!doc.isTopLevel(id))
        throw std::invalid_argument("Only a shape on the plate can be lasso cut");
    const std::string base = body->name;
    const Bounds through = doc.worldBounds(id);
    Body tool = cutter(loop, through);

    std::vector<ObjectId> made;
    stack.beginMacro("Lasso cut " + base);
    try {
        const auto addTool = [&] {
            Body t = tool;
            t.id = doc.allocateId();
            stack.execute(std::make_unique<AddBodyCommand>(t));
            return t.id;
        };
        const auto step = [&](ObjectId target, ObjectId cutterId, BooleanOp op, const std::string& name,
                              const char* what) {
            BooleanSpec spec;
            spec.op = op;
            spec.operands = {target, cutterId};
            const Document::Preview preview = doc.previewResult(spec);
            if (preview.empty)
                throw std::invalid_argument(what);
            const auto c = preview.bounds.center();
            spec.pivot = {c[0], c[1], c[2]};
            Body r;
            r.id = doc.allocateId();
            r.name = name;
            r.boolean = std::move(spec);
            return r;
        };
        const std::string misses = "The loop doesn't reach '" + base + "': draw it round part of the shape.";
        const std::string all = "The loop goes all round '" + base + "': nothing would be left outside it.";
        if (keep == Keep::Inside) {
            Body r = step(id, addTool(), BooleanOp::Intersect, base, misses.c_str());
            made = {r.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
        } else if (keep == Keep::Outside) {
            const ObjectId t = addTool();
            {
                BooleanSpec probe;
                probe.op = BooleanOp::Intersect;
                probe.operands = {id, t};
                if (doc.previewResult(probe).empty)
                    throw std::invalid_argument(misses);
            }
            Body r = step(id, t, BooleanOp::Subtract, base, all.c_str());
            made = {r.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
        } else {
            const std::vector<Body> copies = duplicateTree(doc, id);
            for (const Body& c : copies)
                stack.execute(std::make_unique<AddBodyCommand>(c));
            Body in = step(id, addTool(), BooleanOp::Intersect, base + " (inside)", misses.c_str());
            Body out = step(copies.back().id, addTool(), BooleanOp::Subtract, base + " (outside)", all.c_str());
            made = {in.id, out.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(in)));
            stack.execute(std::make_unique<CombineCommand>(std::move(out)));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

}  // namespace forge::lasso
