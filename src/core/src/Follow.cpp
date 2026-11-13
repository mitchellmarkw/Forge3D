// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Follow.h"
#include "forge/core/Document.h"
#include "forge/core/SketchShape.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS_Compound.hxx>
#include <Bnd_Box.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Surface.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace {

constexpr double pi = std::numbers::pi;

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
    return l > 1e-12 ? mul(a, 1.0 / l) : Vec3{0, 0, 1};
}
gp_Pnt pnt(const Vec3& p) { return gp_Pnt(p[0], p[1], p[2]); }
gp_Dir dir(const Vec3& d) { return gp_Dir(d[0], d[1], d[2]); }

const char* kFailed = "The groove could not follow this path. Try a smaller cross-section or a smoother path.";

struct Box {
    Vec3 min{0, 0, 0}, size{1, 1, 1};
    double diagonal() const { return length(size); }
};

Box boxOf(const TopoDS_Shape& shape)
{
    Bnd_Box b;
    BRepBndLib::Add(shape, b);
    Box out;
    if (b.IsVoid())
        return out;
    double x0, y0, z0, x1, y1, z1;
    b.Get(x0, y0, z0, x1, y1, z1);
    out.min = {x0, y0, z0};
    out.size = {x1 - x0, y1 - y0, z1 - z0};
    return out;
}

// The surface of a part, for walking over: nearest points and which way it faces.
class Surface {
public:
    explicit Surface(const TopoDS_Shape& shape)
    {
        TopExp::MapShapes(shape, TopAbs_FACE, m_faces);
        // Just the faces: a point inside a solid is 0 from the solid, not from its skin.
        BRep_Builder builder;
        TopoDS_Compound skin;
        builder.MakeCompound(skin);
        for (int f = 1; f <= m_faces.Extent(); ++f)
            builder.Add(skin, m_faces(f));
        m_shape = skin;
    }

    struct Hit {
        Vec3 point{0, 0, 0};
        Vec3 normal{0, 0, 1};
        int face = -1;
        double distance = 1e300;
    };

    // The nearest point on the surface; tries the face it was on last first.
    Hit nearest(const Vec3& p, int hint) const
    {
        if (hint >= 0) {
            Hit h = onFace(p, hint);
            if (h.face >= 0)
                return h;
        }
        Hit best;
        const TopoDS_Vertex v = BRepBuilderAPI_MakeVertex(pnt(p)).Vertex();
        BRepExtrema_DistShapeShape dist(v, m_shape);
        if (!dist.IsDone() || dist.NbSolution() == 0)
            return best;
        const TopoDS_Shape support = dist.SupportOnShape2(1);
        const gp_Pnt q = dist.PointOnShape2(1);
        best.point = {q.X(), q.Y(), q.Z()};
        best.distance = dist.Value();
        // The face it's on (an edge or corner: one of its faces).
        int face = -1;
        if (support.ShapeType() == TopAbs_FACE) {
            face = m_faces.FindIndex(support) - 1;
        } else {
            for (int f = 1; f <= m_faces.Extent() && face < 0; ++f)
                for (TopExp_Explorer e(m_faces(f), support.ShapeType()); e.More(); e.Next())
                    if (e.Current().IsSame(support)) {
                        face = f - 1;
                        break;
                    }
        }
        best.face = face;
        if (face >= 0)
            best.normal = normalAt(face, best.point);
        return best;
    }

private:
    Hit onFace(const Vec3& p, int index) const
    {
        Hit h;
        const TopoDS_Face face = TopoDS::Face(m_faces(index + 1));
        const Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        GeomAPI_ProjectPointOnSurf project(pnt(p), surface);
        if (!project.IsDone() || project.NbPoints() == 0)
            return h;
        double u = 0, v = 0;
        project.LowerDistanceParameters(u, v);
        BRepClass_FaceClassifier inside(face, gp_Pnt2d(u, v), 1e-7);
        if (inside.State() != TopAbs_IN)
            return h;
        const gp_Pnt q = project.NearestPoint();
        h.point = {q.X(), q.Y(), q.Z()};
        h.distance = project.LowerDistance();
        h.face = index;
        h.normal = normalAt(index, h.point, u, v);
        return h;
    }

    Vec3 normalAt(int index, const Vec3& p, double u = 1e300, double v = 1e300) const
    {
        const TopoDS_Face face = TopoDS::Face(m_faces(index + 1));
        if (u > 1e299) {
            GeomAPI_ProjectPointOnSurf project(pnt(p), BRep_Tool::Surface(face));
            if (project.IsDone() && project.NbPoints() > 0)
                project.LowerDistanceParameters(u, v);
            else
                return {0, 0, 1};
        }
        BRepGProp_Face props(face);
        gp_Pnt at;
        gp_Vec n;
        props.Normal(u, v, at, n);  // flipped for reversed faces: out of the solid
        if (n.Magnitude() < 1e-12)
            return {0, 0, 1};
        n.Normalize();
        return {n.X(), n.Y(), n.Z()};
    }

    TopoDS_Shape m_shape;
    TopTools_IndexedMapOfShape m_faces;
};

// A walker on the surface: where it is, and its own across / ahead directions, carried
// along so the path keeps its shape as the surface bends.
struct Walker {
    Vec3 point, x, y, normal;
    int face = -1;
};

// One curve of the path: sample points in sketch coordinates.
struct Piece {
    std::vector<sketch::Point> points;
};

// The path's runs: lines and arcs joined end to end, each a list of pieces.
struct Run {
    std::vector<Piece> pieces;
    bool closed = false;
};

std::vector<sketch::Point> sample(const sketch::Sketch& s, const sketch::Geo& g, bool forward, double step)
{
    std::vector<sketch::Point> out;
    const auto P = [&s](int i) { return s.points[std::size_t(i)]; };
    if (g.kind == sketch::GeoKind::Line) {
        const sketch::Point a = P(forward ? g.a : g.b), b = P(forward ? g.b : g.a);
        const int n = std::max(1, int(std::ceil(std::hypot(b.x - a.x, b.y - a.y) / step)));
        for (int k = 0; k <= n; ++k)
            out.push_back({a.x + (b.x - a.x) * k / n, a.y + (b.y - a.y) * k / n});
        return out;
    }
    const sketch::Point c = P(g.a);
    const double r = sketch::radiusOf(s, g);
    double a0 = 0, span = 2 * pi;
    if (g.kind == sketch::GeoKind::Arc) {
        const sketch::Point p0 = P(g.b), p1 = P(g.c);
        a0 = std::atan2(p0.y - c.y, p0.x - c.x);
        span = std::atan2(p1.y - c.y, p1.x - c.x) - a0;
        while (span <= 1e-12)
            span += 2 * pi;
    }
    const int n = std::max(8, int(std::ceil(r * span / step)));
    for (int k = 0; k <= n; ++k) {
        const double t = forward ? double(k) / n : double(n - k) / n;
        const double a = a0 + span * t;
        out.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
    }
    return out;
}

// The drawn lines and arcs (not helper lines) joined into runs.
std::vector<Run> runsOf(const sketch::Sketch& s, double step)
{
    std::vector<int> geos;
    for (std::size_t i = 0; i < s.geos.size(); ++i)
        if (!s.geos[i].construction)
            geos.push_back(int(i));
    const auto endsOf = [&s](int g) {
        const sketch::Geo& geo = s.geos[std::size_t(g)];
        if (geo.kind == sketch::GeoKind::Line)
            return std::pair{geo.a, geo.b};
        if (geo.kind == sketch::GeoKind::Arc)
            return std::pair{geo.b, geo.c};
        return std::pair{-1, -1};
    };
    std::vector<bool> used(s.geos.size(), false);
    std::vector<Run> runs;
    // Circles are runs of their own.
    for (int g : geos)
        if (s.geos[std::size_t(g)].kind == sketch::GeoKind::Circle) {
            used[std::size_t(g)] = true;
            Run run;
            run.closed = true;
            run.pieces.push_back({sample(s, s.geos[std::size_t(g)], true, step)});
            runs.push_back(std::move(run));
        }
    const auto countAt = [&](int p) {
        int n = 0;
        for (int g : geos) {
            const auto [a, b] = endsOf(g);
            n += (a == p) + (b == p);
        }
        return n;
    };
    const auto extend = [&](Run& run, int from, bool appendAtEnd) {
        int at = from;
        for (;;) {
            int next = -1;
            bool forward = true;
            for (int g : geos) {
                if (used[std::size_t(g)])
                    continue;
                const auto [a, b] = endsOf(g);
                if (a == at) {
                    next = g;
                    forward = true;
                    break;
                }
                if (b == at) {
                    next = g;
                    forward = false;
                    break;
                }
            }
            if (next < 0)
                return at;
            used[std::size_t(next)] = true;
            const auto [a, b] = endsOf(next);
            if (appendAtEnd) {
                run.pieces.push_back({sample(s, s.geos[std::size_t(next)], forward, step)});
            } else {
                // Walking backwards from the run's start: put it in front, reversed.
                run.pieces.insert(run.pieces.begin(), {sample(s, s.geos[std::size_t(next)], !forward, step)});
            }
            at = forward ? b : a;
        }
    };
    // Start runs at loose ends first (so open runs go end to end), then any loops left.
    for (int pass = 0; pass < 2; ++pass)
        for (int g : geos) {
            if (used[std::size_t(g)])
                continue;
            const auto [a, b] = endsOf(g);
            if (pass == 0 && countAt(a) != 1 && countAt(b) != 1)
                continue;
            const bool fromA = pass == 1 || countAt(a) == 1;
            Run run;
            used[std::size_t(g)] = true;
            run.pieces.push_back({sample(s, s.geos[std::size_t(g)], fromA, step)});
            const int end = extend(run, fromA ? b : a, true);
            const int start = fromA ? a : b;
            run.closed = end == start;
            runs.push_back(std::move(run));
        }
    return runs;
}

// The frame at the path's origin, on the surface as it is now (after resizing the
// origin may be off the surface: it's moved to the same spot in the part's box).
Walker startOf(const Surface& surface, const TopoDS_Shape& part, const FollowFinish& f)
{
    const Box box = boxOf(part);
    const double tolerance = std::max(box.diagonal() * 1e-3, 1e-4);
    Surface::Hit hit = surface.nearest(f.origin, -1);
    if (hit.face < 0)
        throw GeometryError("The path is not on the shape.");
    const bool moved = hit.distance > tolerance || dot(hit.normal, f.normal) < 0.5;
    if (moved && f.located) {
        const Vec3 again{box.min[0] + f.relative[0] * box.size[0], box.min[1] + f.relative[1] * box.size[1],
                         box.min[2] + f.relative[2] * box.size[2]};
        hit = surface.nearest(again, -1);
        if (hit.face < 0)
            throw GeometryError("The path is not on the shape.");
    }
    Walker w;
    w.point = hit.point;
    w.normal = moved ? hit.normal : unit(f.normal);
    w.x = unit(sub(f.xAxis, mul(w.normal, dot(f.xAxis, w.normal))));
    w.y = cross(w.normal, w.x);
    w.face = hit.face;
    return w;
}

// One small step over the surface, d along the walker's own across / ahead.
void stepOn(const Surface& surface, Walker& w, double dx, double dy)
{
    const Vec3 aim = add(w.point, add(mul(w.x, dx), mul(w.y, dy)));
    const Surface::Hit hit = surface.nearest(aim, w.face);
    if (hit.face < 0)
        throw GeometryError("The path runs off the shape.");
    w.point = hit.point;
    w.face = hit.face;
    // Carry the directions over to the surface there.
    w.normal = hit.normal;
    w.x = unit(sub(w.x, mul(w.normal, dot(w.x, w.normal))));
    w.y = cross(w.normal, w.x);
}

void walkTo(const Surface& surface, Walker& w, sketch::Point& at, const sketch::Point& to, double step)
{
    const double dx = to.x - at.x, dy = to.y - at.y;
    const int n = std::max(1, int(std::ceil(std::hypot(dx, dy) / step)));
    for (int k = 0; k < n; ++k)
        stepOn(surface, w, dx / n, dy / n);
    at = to;
}

struct Laid {
    // Per run, per piece: points on the surface and the surface's facing there.
    std::vector<std::vector<std::vector<Vec3>>> points, normals;
    std::vector<bool> closed;
};

Laid layOut(const TopoDS_Shape& part, const FollowFinish& f)
{
    const Surface surface(part);
    const Box box = boxOf(part);
    const double step = std::clamp(box.diagonal() / 400.0, 0.05, 0.5);
    const std::vector<Run> runs = runsOf(f.path, step);
    if (runs.empty())
        throw GeometryError("Draw the path first (lines, arcs or circles).");
    const Walker start = startOf(surface, part, f);
    Laid laid;
    for (const Run& run : runs) {
        Walker w = start;
        sketch::Point at{0, 0};
        // From the origin to where the run starts, then along it.
        walkTo(surface, w, at, run.pieces.front().points.front(), step);
        std::vector<std::vector<Vec3>> pts, nrm;
        for (const Piece& piece : run.pieces) {
            std::vector<Vec3> p{w.point}, n{w.normal};
            for (std::size_t k = 1; k < piece.points.size(); ++k) {
                walkTo(surface, w, at, piece.points[k], step);
                p.push_back(w.point);
                n.push_back(w.normal);
            }
            pts.push_back(std::move(p));
            nrm.push_back(std::move(n));
        }
        if (run.closed && !pts.empty())  // the loop meets itself again exactly
            pts.back().back() = pts.front().front();
        laid.points.push_back(std::move(pts));
        laid.normals.push_back(std::move(nrm));
        laid.closed.push_back(run.closed);
    }
    return laid;
}

TopoDS_Edge edgeThrough(const std::vector<Vec3>& points)
{
    // Close points dropped (they upset the fit), the ends kept.
    std::vector<Vec3> keep{points.front()};
    for (std::size_t i = 1; i + 1 < points.size(); ++i)
        if (length(sub(points[i], keep.back())) > 1e-4)
            keep.push_back(points[i]);
    if (length(sub(points.back(), keep.back())) > 1e-6 || keep.size() == 1)
        keep.push_back(points.back());
    if (keep.size() == 2)
        return BRepBuilderAPI_MakeEdge(pnt(keep[0]), pnt(keep[1])).Edge();
    TColgp_Array1OfPnt array(1, int(keep.size()));
    for (std::size_t i = 0; i < keep.size(); ++i)
        array.SetValue(int(i) + 1, pnt(keep[i]));
    GeomAPI_PointsToBSpline fit(array, 3, 8, GeomAbs_C2, 1e-4);
    if (!fit.IsDone())
        throw GeometryError(kFailed);
    return BRepBuilderAPI_MakeEdge(fit.Curve()).Edge();
}

TopoDS_Wire wireThrough(const std::vector<std::vector<Vec3>>& pieces)
{
    BRepBuilderAPI_MakeWire wire;
    for (const auto& piece : pieces) {
        if (piece.size() < 2 || (length(sub(piece.back(), piece.front())) < 1e-6 && piece.size() < 3))
            continue;
        wire.Add(edgeThrough(piece));
        if (!wire.IsDone())
            throw GeometryError(kFailed);
    }
    if (!wire.IsDone())
        throw GeometryError("Draw the path first (lines, arcs or circles).");
    return wire.Wire();
}

// The cross-section at the path's start: across = `across`, up = `up` from `at`.
TopoDS_Wire profileAt(const FollowFinish& f, const Vec3& at, const Vec3& across, const Vec3& up)
{
    const double w = f.width, d = f.depth;
    if (f.profile != FollowProfile::Custom && (!(w > 0.0) || !(d > 0.0)))
        throw std::invalid_argument("The width and depth must be greater than zero");
    // A little past the surface on the other side, so the cut / join is clean.
    const double past = std::max(0.2, 0.2 * d);
    const auto P = [&](double u, double v) { return pnt(add(at, add(mul(across, u), mul(up, v)))); };
    switch (f.profile) {
    case FollowProfile::Round: {
        if (d > w)
            throw GeometryError("A round groove or ridge can't be deeper than it is wide.");
        const double r = w / 2;
        const double centre = f.ridge ? d - r : r - d;
        // In the plane across the path: its axis is the path's direction.
        const gp_Circ circle(gp_Ax2(P(0, centre), dir(cross(across, up)), dir(across)), r);
        return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(circle).Edge()).Wire();
    }
    case FollowProfile::Square: {
        const double v0 = f.ridge ? -past : -d, v1 = f.ridge ? d : past;
        BRepBuilderAPI_MakePolygon poly(P(-w / 2, v0), P(w / 2, v0), P(w / 2, v1), P(-w / 2, v1), Standard_True);
        return poly.Wire();
    }
    case FollowProfile::V: {
        // The V's sides carried on past the surface, so it stays w wide at the surface.
        const double grow = w / 2 * (1 + past / d);
        BRepBuilderAPI_MakePolygon poly;
        if (f.ridge) {
            poly.Add(P(-grow, -past));
            poly.Add(P(grow, -past));
            poly.Add(P(0, d));
        } else {
            poly.Add(P(-grow, past));
            poly.Add(P(0, -d));
            poly.Add(P(grow, past));
        }
        poly.Close();
        return poly.Wire();
    }
    case FollowProfile::Custom: {
        const TopoDS_Shape faces = sketch::faces(f.custom);
        TopExp_Explorer first(faces, TopAbs_FACE);
        if (!first.More())
            throw GeometryError("Draw the cross-section first (a closed outline).");
        gp_Trsf place;
        const Vec3 along = cross(across, up);
        place.SetValues(across[0], up[0], along[0], at[0],  //
                        across[1], up[1], along[1], at[1],  //
                        across[2], up[2], along[2], at[2]);
        const TopoDS_Face face = TopoDS::Face(BRepBuilderAPI_Transform(first.Current(), place, true).Shape());
        return BRepTools::OuterWire(face);
    }
    }
    throw std::invalid_argument("Unknown cross-section");
}

// One run swept: the cross-section carried along, kept upright to the surface.
TopoDS_Shape sweepRun(const std::vector<std::vector<Vec3>>& pts, const std::vector<std::vector<Vec3>>& nrm,
                      const FollowFinish& f)
{
    const TopoDS_Wire spine = wireThrough(pts);
    // A second path just above, along the surface's facing: its direction from the
    // spine keeps the cross-section upright.
    std::vector<std::vector<Vec3>> above;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        std::vector<Vec3> a;
        for (std::size_t k = 0; k < pts[i].size(); ++k)
            a.push_back(add(pts[i][k], nrm[i][k]));
        above.push_back(std::move(a));
    }
    if (length(sub(pts.back().back(), pts.front().front())) < 1e-9)
        above.back().back() = above.front().front();
    const TopoDS_Wire guide = wireThrough(above);
    // The cross-section at the start, square to the path.
    const Vec3& p0 = pts.front().front();
    const Vec3 up = unit(nrm.front().front());
    Vec3 ahead{0, 0, 0};
    for (const auto& piece : pts)
        for (std::size_t k = 1; k < piece.size() && length(ahead) < 1e-9; ++k)
            if (length(sub(piece[k], p0)) > 1e-6)
                ahead = sub(piece[k], p0);
    ahead = unit(sub(ahead, mul(up, dot(ahead, up))));
    const Vec3 across = cross(ahead, up);
    BRepOffsetAPI_MakePipeShell pipe(spine);
    pipe.SetMode(guide, Standard_True, BRepFill_NoContact);
    pipe.SetTransitionMode(BRepBuilderAPI_RoundCorner);
    pipe.Add(profileAt(f, p0, across, up), Standard_False, Standard_False);
    pipe.Build();
    if (!pipe.IsDone())
        throw GeometryError(kFailed);
    if (!pipe.MakeSolid())
        throw GeometryError(kFailed);
    return pipe.Shape();
}

}  // namespace

namespace follow {

Transform frameOf(const FollowFinish& f)
{
    const Vec3 n = unit(f.normal);
    const Vec3 x = unit(sub(f.xAxis, mul(n, dot(f.xAxis, n))));
    const Vec3 y = cross(n, x);
    Transform t;
    for (std::size_t r = 0; r < 3; ++r) {
        t.m[r][0] = x[r];
        t.m[r][1] = y[r];
        t.m[r][2] = n[r];
        t.m[r][3] = f.origin[r];
    }
    return t;
}

std::vector<std::vector<Vec3>> laidPath(const TopoDS_Shape& part, const FollowFinish& f)
{
    const Laid laid = layOut(part, f);
    std::vector<std::vector<Vec3>> out;
    for (const auto& run : laid.points) {
        std::vector<Vec3> line;
        for (const auto& piece : run)
            for (const Vec3& p : piece)
                if (line.empty() || length(sub(p, line.back())) > 1e-9)
                    line.push_back(p);
        out.push_back(std::move(line));
    }
    return out;
}

TopoDS_Shape swept(const TopoDS_Shape& part, const FollowFinish& f)
{
    try {
        const Laid laid = layOut(part, f);
        TopoDS_Shape all;
        bool first = true;
        for (std::size_t r = 0; r < laid.points.size(); ++r) {
            const TopoDS_Shape one = sweepRun(laid.points[r], laid.normals[r], f);
            if (first) {
                all = one;
                first = false;
            } else {
                BRepAlgoAPI_Fuse fuse(all, one);
                if (!fuse.IsDone() || fuse.HasErrors())
                    throw GeometryError(kFailed);
                all = fuse.Shape();
            }
        }
        return all;
    } catch (const Standard_Failure&) {
        throw GeometryError(kFailed);
    }
}

TopoDS_Shape apply(const TopoDS_Shape& part, const FollowFinish& f)
{
    const TopoDS_Shape tool = swept(part, f);
    TopoDS_Shape result;
    try {
        if (f.ridge) {
            BRepAlgoAPI_Fuse fuse(part, tool);
            if (!fuse.IsDone() || fuse.HasErrors())
                throw GeometryError(kFailed);
            result = fuse.Shape();
        } else {
            BRepAlgoAPI_Cut cut(part, tool);
            if (!cut.IsDone() || cut.HasErrors())
                throw GeometryError(kFailed);
            result = cut.Shape();
        }
        ShapeUpgrade_UnifySameDomain unify(result, true, true, false);
        unify.Build();
        result = unify.Shape();
    } catch (const Standard_Failure&) {
        throw GeometryError(kFailed);
    }
    if (result.IsNull() || !BRepCheck_Analyzer(result).IsValid())
        throw GeometryError(kFailed);
    return result;
}

}  // namespace follow

namespace {

gp_Trsf toTrsf(const Transform& t)
{
    gp_Trsf trsf;
    trsf.SetValues(t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3],  //
                   t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3],  //
                   t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);
    return trsf;
}

FollowFinish moved(const FollowFinish& f, const Transform& t)
{
    FollowFinish out = f;
    out.origin = t.apply(f.origin);
    out.xAxis = unit(t.applyDirection(f.xAxis));
    out.normal = unit(t.applyDirection(f.normal));
    return out;
}

TopoDS_Shape placedPart(const Document& doc, ObjectId part)
{
    return BRepBuilderAPI_Transform(doc.shape(part), toTrsf(doc.localTransform(part)), true).Shape();
}

void locate(const TopoDS_Shape& part, FollowFinish& f)
{
    const Box box = boxOf(part);
    for (std::size_t k = 0; k < 3; ++k)
        f.relative[k] = box.size[k] > 1e-9 ? (f.origin[k] - box.min[k]) / box.size[k] : 0.5;
    f.located = true;
}

const Body& followResult(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Follow || !body->boolean->follow)
        throw std::invalid_argument("Not a Follow step");
    return *body;
}

}  // namespace

BooleanSpec followSpec(const Document& doc, ObjectId target, const FollowFinish& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (doc.usesMeshEngine(target))
        throw std::invalid_argument("Follow works on solid shapes, not on imported meshes or shapes made with them.");
    BooleanSpec spec;
    spec.op = BooleanOp::Follow;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    FollowFinish local = moved(inPlate, toStep);
    locate(placedPart(doc, target), local);
    spec.follow = local;
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

FollowFinish followInPlate(const Document& doc, ObjectId result)
{
    const Body& body = followResult(doc, result);
    return moved(*body.boolean->follow, doc.worldTransform(result));
}

Body withFollow(const Document& doc, ObjectId result, const FollowFinish& inPlate)
{
    Body next = followResult(doc, result);
    FollowFinish f = moved(inPlate, doc.worldTransform(result).inverse());
    locate(placedPart(doc, next.boolean->operands.front()), f);
    next.boolean->follow = f;
    return next;
}

std::vector<std::vector<Vec3>> followPathInPlate(const Document& doc, ObjectId target, const FollowFinish& inPlate,
                                                  bool editing)
{
    // The shape walked over, and the finish, both in plate coordinates.
    ObjectId part = target;
    if (editing)
        part = followResult(doc, target).boolean->operands.front();
    const TopoDS_Shape shape =
        BRepBuilderAPI_Transform(doc.shape(part), toTrsf(doc.worldTransform(part)), true).Shape();
    return follow::laidPath(shape, inPlate);
}

}  // namespace forge
