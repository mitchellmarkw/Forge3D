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
#include <BRepPrimAPI_MakePrism.hxx>
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

}  // namespace forge::sketch
