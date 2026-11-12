// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/SketchShape.h"

#include "forge/core/Document.h"  // GeometryError

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <gp_Ax1.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GProp_GProps.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace forge::sketch {

namespace {

gp_Pnt at(const Sketch& s, int i)
{
    return gp_Pnt(s.points[std::size_t(i)].x, s.points[std::size_t(i)].y, 0.0);
}

// The wire as a flat polygon (for "is this inside that").
std::vector<std::array<double, 2>> polygon(const TopoDS_Wire& wire)
{
    std::vector<std::array<double, 2>> out;
    for (BRepTools_WireExplorer it(wire); it.More(); it.Next()) {
        BRepAdaptor_Curve curve(it.Current());
        GCPnts_QuasiUniformDeflection sampler(curve, 1e-3);
        if (!sampler.IsDone())
            continue;
        const bool forward = it.Current().Orientation() != TopAbs_REVERSED;
        const int n = sampler.NbPoints();
        for (int k = 1; k <= n; ++k) {
            const gp_Pnt p = sampler.Value(forward ? k : n + 1 - k);
            out.push_back({p.X(), p.Y()});
        }
    }
    return out;
}

bool inside(const std::vector<std::array<double, 2>>& poly, double x, double y)
{
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const auto& a = poly[i];
        const auto& b = poly[j];
        if ((a[1] > y) != (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0])
            in = !in;
    }
    return in;
}

double area(const std::vector<std::array<double, 2>>& poly)
{
    double a = 0;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        a += (poly[j][0] - poly[i][0]) * (poly[j][1] + poly[i][1]);
    return std::abs(a) / 2;
}

}  // namespace

TopoDS_Shape faces(const Sketch& s)
{
    if (!looseEnds(s).empty())
        throw GeometryError("The outline isn't closed: some lines end without joining anything (shown in red).");
    // Every outline as a closed wire.
    std::vector<TopoDS_Wire> wires;
    Handle(TopTools_HSequenceOfShape) edges = new TopTools_HSequenceOfShape;
    try {
        for (const Geo& g : s.geos) {
            if (g.construction)
                continue;
            switch (g.kind) {
            case GeoKind::Line: {
                const gp_Pnt a = at(s, g.a), b = at(s, g.b);
                if (a.Distance(b) < 1e-9)
                    continue;
                edges->Append(BRepBuilderAPI_MakeEdge(a, b).Edge());
                break;
            }
            case GeoKind::Circle: {
                const double r = radiusOf(s, g);
                if (!(r > 1e-9))
                    continue;
                wires.push_back(BRepBuilderAPI_MakeWire(
                                    BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(at(s, g.a), gp::DZ()), r)).Edge())
                                    .Wire());
                break;
            }
            case GeoKind::Arc: {
                const double r = radiusOf(s, g);
                if (!(r > 1e-9))
                    continue;
                const gp_Circ circle(gp_Ax2(at(s, g.a), gp::DZ()), r);
                GC_MakeArcOfCircle arc(circle, at(s, g.b), at(s, g.c), /*sense=*/true);
                if (!arc.IsDone())
                    throw GeometryError("An arc in the sketch is too small to draw");
                edges->Append(BRepBuilderAPI_MakeEdge(arc.Value(), at(s, g.b), at(s, g.c)).Edge());
                break;
            }
            }
        }
        if (edges->Length() > 0) {
            Handle(TopTools_HSequenceOfShape) joined;
            ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, 1e-6, /*shared=*/false, joined);
            for (int i = 1; i <= joined->Length(); ++i) {
                const TopoDS_Wire w = TopoDS::Wire(joined->Value(i));
                if (!BRep_Tool::IsClosed(w))
                    throw GeometryError("The outline isn't closed: some lines end without joining anything.");
                wires.push_back(w);
            }
        }
    } catch (const Standard_Failure& e) {
        throw GeometryError(std::string("The sketch could not be drawn: ") + e.GetMessageString());
    }
    if (wires.empty())
        throw GeometryError("Draw a closed outline (a rectangle, a circle, or lines that join up) to make a shape.");

    // Outlines inside an odd number of others are holes in the one just around them.
    struct Loop {
        TopoDS_Wire wire;
        std::vector<std::array<double, 2>> poly;
        double area = 0;
        int depth = 0;
        int parent = -1;
    };
    std::vector<Loop> loops;
    for (const TopoDS_Wire& w : wires) {
        Loop l;
        l.wire = w;
        l.poly = polygon(w);
        l.area = area(l.poly);
        if (l.poly.size() >= 3 && l.area > 1e-12)
            loops.push_back(std::move(l));
    }
    for (std::size_t i = 0; i < loops.size(); ++i) {
        // A point of this loop, nudged off any shared edge: its first polygon point.
        const auto p = loops[i].poly.front();
        double best = 1e300;
        for (std::size_t j = 0; j < loops.size(); ++j) {
            if (i == j || loops[j].area <= loops[i].area)
                continue;
            if (inside(loops[j].poly, p[0], p[1])) {
                ++loops[i].depth;
                if (loops[j].area < best) {
                    best = loops[j].area;
                    loops[i].parent = int(j);
                }
            }
        }
    }
    BRep_Builder builder;
    TopoDS_Compound out;
    builder.MakeCompound(out);
    int made = 0;
    const gp_Pln plane(gp::Origin(), gp::DZ());
    for (std::size_t i = 0; i < loops.size(); ++i) {
        if (loops[i].depth % 2 != 0)
            continue;
        BRepBuilderAPI_MakeFace face(plane, loops[i].wire, /*inside=*/true);
        for (std::size_t j = 0; j < loops.size(); ++j)
            if (loops[j].parent == int(i) && loops[j].depth % 2 == 1)
                face.Add(TopoDS::Wire(loops[j].wire.Reversed()));
        if (!face.IsDone())
            throw GeometryError("An outline in the sketch could not be filled in (does it cross itself?)");
        ShapeFix_Face fix(face.Face());
        fix.FixOrientation();
        fix.Perform();
        builder.Add(out, fix.Face());
        ++made;
    }
    if (made == 0)
        throw GeometryError("Draw a closed outline to make a shape.");
    return out;
}

TopoDS_Shape padShape(const PadSpec& pad)
{
    if (!(pad.length > 0.0))
        throw std::invalid_argument("The pad's length must be greater than zero");
    const TopoDS_Shape flat = faces(pad.sketch);
    double from = 0.0, to = pad.length;
    if (pad.reversed) {
        from = -pad.length;
        to = 0.0;
    } else if (pad.midplane) {
        from = -pad.length / 2;
        to = pad.length / 2;
    }
    try {
        TopoDS_Shape start = flat;
        if (from != 0.0) {
            gp_Trsf down;
            down.SetTranslation(gp_Vec(0, 0, from));
            start = BRepBuilderAPI_Transform(flat, down, true).Shape();
        }
        TopoDS_Shape result;
        bool first = true;
        for (TopExp_Explorer it(start, TopAbs_FACE); it.More(); it.Next()) {
            const TopoDS_Shape solid = BRepPrimAPI_MakePrism(it.Current(), gp_Vec(0, 0, to - from)).Shape();
            if (first) {
                result = solid;
                first = false;
            } else {
                BRepAlgoAPI_Fuse fuse(result, solid);
                if (!fuse.IsDone() || fuse.HasErrors())
                    throw GeometryError("Outlines in the sketch overlap in a way that can't be joined");
                result = fuse.Shape();
            }
        }
        if (first)
            throw GeometryError("Draw a closed outline to make a shape.");
        ShapeUpgrade_UnifySameDomain unify(result, true, true, false);
        unify.Build();
        return unify.Shape();
    } catch (const Standard_Failure& e) {
        throw GeometryError(std::string("The sketch could not be pushed up: ") + e.GetMessageString());
    }
}

TopoDS_Shape revolveShape(const RevolveSpec& revolve)
{
    const Sketch& s = revolve.sketch;
    if (revolve.axis < 0 || std::size_t(revolve.axis) >= s.geos.size() ||
        s.geos[std::size_t(revolve.axis)].kind != GeoKind::Line)
        throw GeometryError("Pick the helper line to spin the outline around.");
    if (!(revolve.angle > 0.0) || revolve.angle > 360.0)
        throw std::invalid_argument("The angle must be more than 0 and at most 360 degrees");
    const Geo& axisLine = s.geos[std::size_t(revolve.axis)];
    const Point a = s.points[std::size_t(axisLine.a)], b = s.points[std::size_t(axisLine.b)];
    const double len = std::hypot(b.x - a.x, b.y - a.y);
    if (len < 1e-9)
        throw GeometryError("The axis line has no length.");
    const double ux = (b.x - a.x) / len, uy = (b.y - a.y) / len;
    // Everything spun must stay on one side of the axis.
    double lo = 0.0, hi = 0.0, size = len;
    const auto side = [&](double x, double y) {
        const double d = ux * (y - a.y) - uy * (x - a.x);
        lo = std::min(lo, d);
        hi = std::max(hi, d);
        size = std::max(size, std::abs(d));
    };
    for (std::size_t i = 0; i < s.geos.size(); ++i) {
        const Geo& g = s.geos[i];
        if (g.construction || int(i) == revolve.axis)
            continue;
        if (g.kind == GeoKind::Line) {
            side(s.points[std::size_t(g.a)].x, s.points[std::size_t(g.a)].y);
            side(s.points[std::size_t(g.b)].x, s.points[std::size_t(g.b)].y);
            continue;
        }
        const Point c = s.points[std::size_t(g.a)];
        const double r = radiusOf(s, g);
        double a0 = 0.0, span = 2 * std::numbers::pi;
        if (g.kind == GeoKind::Arc) {
            const Point p0 = s.points[std::size_t(g.b)], p1 = s.points[std::size_t(g.c)];
            a0 = std::atan2(p0.y - c.y, p0.x - c.x);
            span = std::atan2(p1.y - c.y, p1.x - c.x) - a0;
            while (span <= 0)
                span += 2 * std::numbers::pi;
        }
        for (int k = 0; k <= 64; ++k)
            side(c.x + r * std::cos(a0 + span * k / 64), c.y + r * std::sin(a0 + span * k / 64));
    }
    const double tol = 1e-6 * std::max(size, 1.0);
    if (lo < -tol && hi > tol)
        throw GeometryError("The outline crosses the axis it spins around. Keep it all on one side.");
    Sketch without = s;
    without.geos[std::size_t(revolve.axis)].construction = true;
    const TopoDS_Shape flat = faces(without);
    const double angle = revolve.angle * std::numbers::pi / 180.0;
    gp_Ax1 axis(gp_Pnt(a.x, a.y, 0), gp_Dir(ux, uy, 0));
    try {
        TopoDS_Shape start = flat;
        double sweepBy = revolve.reversed ? -angle : angle;
        if (revolve.midplane && revolve.angle < 360.0) {
            gp_Trsf back;
            back.SetRotation(axis, -angle / 2);
            start = BRepBuilderAPI_Transform(flat, back, true).Shape();
            sweepBy = angle;
        }
        TopoDS_Shape result;
        bool first = true;
        for (TopExp_Explorer it(start, TopAbs_FACE); it.More(); it.Next()) {
            BRepPrimAPI_MakeRevol revol(it.Current(), axis, sweepBy, Standard_True);
            if (!revol.IsDone())
                throw GeometryError("The outline could not be spun round.");
            if (first) {
                result = revol.Shape();
                first = false;
            } else {
                BRepAlgoAPI_Fuse fuse(result, revol.Shape());
                if (!fuse.IsDone() || fuse.HasErrors())
                    throw GeometryError("Outlines in the sketch overlap in a way that can't be joined");
                result = fuse.Shape();
            }
        }
        if (first)
            throw GeometryError("Draw a closed outline to make a shape.");
        ShapeUpgrade_UnifySameDomain unify(result, true, true, false);
        unify.Build();
        result = unify.Shape();
        if (!BRepCheck_Analyzer(result).IsValid())
            throw GeometryError("The spun shape came out broken (does the outline touch the axis at a point?)");
        return result;
    } catch (const Standard_Failure& e) {
        throw GeometryError(std::string("The sketch could not be spun round: ") + e.GetMessageString());
    }
}

const std::vector<HoleSize>& holeSizes()
{
    static const std::vector<HoleSize> sizes{
        {"M2", 2.4, 1.6, 4.3, 2.4, 4.4},      {"M2.5", 2.9, 2.05, 5.0, 2.9, 5.5},
        {"M3", 3.4, 2.5, 6.0, 3.4, 6.72},     {"M4", 4.5, 3.3, 8.0, 4.4, 8.96},
        {"M5", 5.5, 4.2, 10.0, 5.4, 11.2},    {"M6", 6.6, 5.0, 11.0, 6.4, 13.44},
        {"M8", 9.0, 6.8, 15.0, 8.6, 17.92},   {"M10", 11.0, 8.5, 18.0, 10.6, 22.4},
        {"M12", 13.5, 10.2, 20.0, 12.6, 26.88},
    };
    return sizes;
}

HoleSpec sizedAs(HoleSpec hole, const std::string& size, bool tapped)
{
    hole.size = size;
    hole.tapped = tapped;
    for (const HoleSize& h : holeSizes())
        if (size == h.name) {
            hole.diameter = tapped ? h.tapDrill : h.clearance;
            hole.headDiameter = h.headDiameter;
            hole.headDepth = h.headDepth;
            hole.countersinkDiameter = h.countersinkDiameter;
            hole.countersinkAngle = 90.0;
        }
    return hole;
}

TopoDS_Shape holeShape(const HoleSpec& hole)
{
    if (!(hole.diameter > 0.0) || !(hole.depth > 0.0))
        throw std::invalid_argument("A hole needs a diameter and a depth greater than zero");
    const double r = hole.diameter / 2;
    const double above = std::max(0.5, hole.diameter * 0.1);  // starts a little above the face
    // The cutter's outline, as (radius, height) going down one side, spun around Z.
    std::vector<std::pair<double, double>> outline;
    switch (hole.kind) {
    case HoleKind::Plain: outline = {{r, above}, {r, -hole.depth}}; break;
    case HoleKind::Counterbored: {
        const double R = hole.headDiameter / 2;
        if (!(R > r))
            throw GeometryError("The counterbore must be wider than the hole");
        if (!(hole.headDepth > 0.0) || hole.headDepth >= hole.depth)
            throw GeometryError("The counterbore must be less deep than the hole");
        outline = {{R, above}, {R, -hole.headDepth}, {r, -hole.headDepth}, {r, -hole.depth}};
        break;
    }
    case HoleKind::Countersunk: {
        const double R = hole.countersinkDiameter / 2;
        if (!(R > r))
            throw GeometryError("The countersink must be wider than the hole");
        if (!(hole.countersinkAngle > 1.0) || !(hole.countersinkAngle < 179.0))
            throw GeometryError("The countersink angle must be between 1 and 179 degrees");
        const double sink = (R - r) / std::tan(hole.countersinkAngle * std::numbers::pi / 360.0);
        if (sink >= hole.depth)
            throw GeometryError("The countersink is deeper than the hole");
        outline = {{R, above}, {R, 0.0}, {r, -sink}, {r, -hole.depth}};
        break;
    }
    }
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(0, 0, above));
    for (const auto& [x, z] : outline)
        polygon.Add(gp_Pnt(x, 0, z));
    polygon.Add(gp_Pnt(0, 0, -hole.depth));
    polygon.Close();
    const TopoDS_Face profile = BRepBuilderAPI_MakeFace(polygon.Wire(), /*OnlyPlane=*/true).Face();
    const TopoDS_Shape one = BRepPrimAPI_MakeRevol(profile, gp_Ax1(gp::Origin(), gp::DZ()), 2 * std::numbers::pi).Shape();
    const std::vector<int> spots = loosePoints(hole.sketch);
    if (spots.empty())
        throw GeometryError("Click on the face to put a hole there.");
    TopoDS_Shape result;
    bool first = true;
    for (int p : spots) {
        gp_Trsf at;
        const Point q = hole.sketch.points[std::size_t(p)];
        at.SetTranslation(gp_Vec(q.x, q.y, 0));
        const TopoDS_Shape placed = BRepBuilderAPI_Transform(one, at, true).Shape();
        if (first) {
            result = placed;
            first = false;
        } else {
            BRepAlgoAPI_Fuse fuse(result, placed);
            if (!fuse.IsDone() || fuse.HasErrors())
                throw GeometryError("Holes overlap in a way that can't be joined");
            result = fuse.Shape();
        }
    }
    return result;
}

std::optional<FacePlane> planeOfFace(const TopoDS_Shape& shape, int face)
{
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    if (face < 0 || face >= faces.Extent())
        return std::nullopt;
    const TopoDS_Face f = TopoDS::Face(faces.FindKey(face + 1));
    try {
        BRepAdaptor_Surface surface(f, true);
        if (surface.GetType() != GeomAbs_Plane)
            return std::nullopt;
        const gp_Pln plane = surface.Plane();
        gp_Dir n = plane.Axis().Direction();
        // Out of the solid: the plane's own direction, flipped for a reversed face.
        if (f.Orientation() == TopAbs_REVERSED)
            n.Reverse();
        // Origin: the middle of the face (its centre of area).
        GProp_GProps props;
        BRepGProp::SurfaceProperties(f, props);
        const gp_Pnt o = props.CentreOfMass();
        // X: the plate's X on faces looking up or down; else level, to the right when
        // looking at the face from outside.
        gp_Dir x(1, 0, 0);
        if (std::abs(n.Z()) < 0.9) {
            const gp_Vec level = gp_Vec(0, 0, 1).Crossed(gp_Vec(n));
            x = gp_Dir(level);
        } else {
            // Square to the normal (the face may lean a little).
            const gp_Vec v = gp_Vec(1, 0, 0) - gp_Vec(n) * n.X();
            x = gp_Dir(v);
        }
        const gp_Dir y = n.Crossed(x);
        FacePlane out;
        for (int r = 0; r < 3; ++r) {
            out.frame.m[std::size_t(r)] = {x.Coord(r + 1), y.Coord(r + 1), n.Coord(r + 1), o.Coord(r + 1)};
        }
        // The outlines, flattened into sketch coordinates.
        double diag = 1.0;
        {
            GProp_GProps p2;
            BRepGProp::LinearProperties(f, p2);
            diag = std::max(p2.Mass() / 40.0, 1e-3);
        }
        for (TopExp_Explorer w(f, TopAbs_WIRE); w.More(); w.Next()) {
            for (TopExp_Explorer e(w.Current(), TopAbs_EDGE); e.More(); e.Next()) {
                const TopoDS_Edge edge = TopoDS::Edge(e.Current());
                if (BRep_Tool::Degenerated(edge))
                    continue;
                BRepAdaptor_Curve curve(edge);
                std::vector<Point> line;
                if (curve.GetType() == GeomAbs_Line) {
                    for (const double t : {curve.FirstParameter(), curve.LastParameter()}) {
                        const gp_Vec d(o, curve.Value(t));
                        line.push_back({d.Dot(gp_Vec(x)), d.Dot(gp_Vec(y))});
                    }
                } else {
                    GCPnts_QuasiUniformDeflection sampler(curve, diag * 0.01);
                    if (!sampler.IsDone())
                        continue;
                    for (int k = 1; k <= sampler.NbPoints(); ++k) {
                        const gp_Vec d(o, sampler.Value(k));
                        line.push_back({d.Dot(gp_Vec(x)), d.Dot(gp_Vec(y))});
                    }
                }
                if (line.size() >= 2)
                    out.outlines.push_back(std::move(line));
            }
        }
        return out;
    } catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

std::vector<std::vector<Point>> outlinesOnPlane(const TopoDS_Shape& shape, const Transform& frame)
{
    std::vector<std::vector<Point>> out;
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    const Transform toSketch = frame.inverse();
    const Vec3 n = frame.applyDirection({0, 0, 1});
    const Vec3 o = frame.apply({0, 0, 0});
    for (int f = 0; f < faces.Extent(); ++f) {
        const auto plane = planeOfFace(shape, f);
        if (!plane)
            continue;
        const auto& m = plane->frame.m;
        const Vec3 fn{m[0][2], m[1][2], m[2][2]}, fo{m[0][3], m[1][3], m[2][3]};
        const double facing = std::abs(fn[0] * n[0] + fn[1] * n[1] + fn[2] * n[2]);
        const double off = (fo[0] - o[0]) * n[0] + (fo[1] - o[1]) * n[1] + (fo[2] - o[2]) * n[2];
        if (facing < 1 - 1e-6 || std::abs(off) > 1e-4)
            continue;
        for (const auto& line : plane->outlines) {
            std::vector<Point> q;
            for (const Point& p : line) {
                const Vec3 local = toSketch.apply(plane->frame.apply({p.x, p.y, 0}));
                q.push_back({local[0], local[1]});
            }
            out.push_back(std::move(q));
        }
    }
    return out;
}

}  // namespace forge::sketch
