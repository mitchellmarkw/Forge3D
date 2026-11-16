// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/SketchShape.h"

#include "forge/core/Document.h"  // GeometryError

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <gp_Ax3.hxx>
#include <gp_Lin2d.hxx>
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
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <gp_Pln.hxx>
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
#include <TopoDS_Vertex.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cstdio>
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

namespace {

// One face of the sketch pushed `height` along Z (negative: down) from the sketch
// plane, its sides leaning in by `taper` degrees as they go.
TopoDS_Shape taperedPrism(const TopoDS_Shape& face, double height, double taper)
{
    // The outline at the far end is the sketch's moved in by this much: if part of it
    // (the outline, or a hole) closes up before then, the taper is too steep.
    const char* const tooSteep = "That taper is too steep for this shape: the sides would meet before the end";
    {
        const double inward = std::abs(height) * std::tan(taper * std::numbers::pi / 180.0);
        int wires = 0, left = 0;
        for (TopExp_Explorer it(face, TopAbs_WIRE); it.More(); it.Next())
            ++wires;
        BRepOffsetAPI_MakeOffset far(TopoDS::Face(face), GeomAbs_Intersection);
        far.Perform(-inward);
        if (far.IsDone() && !far.Shape().IsNull())
            for (TopExp_Explorer it(far.Shape(), TopAbs_WIRE); it.More(); it.Next())
                ++left;
        if (left != wires)
            throw GeometryError(tooSteep);
    }
    const TopoDS_Shape prism = BRepPrimAPI_MakePrism(face, gp_Vec(0, 0, height)).Shape();
    const gp_Dir pull(0, 0, height > 0 ? 1 : -1);
    const gp_Pln neutral(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    BRepOffsetAPI_DraftAngle draft(prism);
    for (TopExp_Explorer it(prism, TopAbs_FACE); it.More(); it.Next()) {
        const TopoDS_Face side = TopoDS::Face(it.Current());
        const BRepAdaptor_Surface surface(side);
        if (surface.GetType() == GeomAbs_Plane && std::abs(surface.Plane().Axis().Direction().Z()) > 0.5)
            continue;  // the two ends
        draft.Add(side, pull, taper * std::numbers::pi / 180.0, neutral);
        if (!draft.AddDone())
            throw GeometryError("The sides of this outline can't be tapered (try straight lines and whole arcs)");
    }
    draft.Build();
    if (!draft.IsDone() || !BRepCheck_Analyzer(draft.Shape()).IsValid())
        throw GeometryError(tooSteep);
    return draft.Shape();
}

}  // namespace

TopoDS_Shape padShape(const PadSpec& pad)
{
    if (!(pad.length > 0.0))
        throw std::invalid_argument("The pad's length must be greater than zero");
    if (!(std::abs(pad.taper) <= 60.0))
        throw std::invalid_argument("The taper must be between -60 and 60 degrees");
    const TopoDS_Shape flat = faces(pad.sketch);
    if (pad.taper != 0.0) {
        try {
            TopoDS_Shape result;
            bool first = true;
            const auto add = [&](const TopoDS_Shape& solid) {
                if (first) {
                    result = solid;
                    first = false;
                    return;
                }
                BRepAlgoAPI_Fuse fuse(result, solid);
                if (!fuse.IsDone() || fuse.HasErrors())
                    throw GeometryError("Outlines in the sketch overlap in a way that can't be joined");
                result = fuse.Shape();
            };
            for (TopExp_Explorer it(flat, TopAbs_FACE); it.More(); it.Next()) {
                if (pad.midplane) {
                    add(taperedPrism(it.Current(), pad.length / 2, pad.taper));
                    add(taperedPrism(it.Current(), -pad.length / 2, pad.taper));
                } else {
                    add(taperedPrism(it.Current(), pad.reversed ? -pad.length : pad.length, pad.taper));
                }
            }
            if (first)
                throw GeometryError("Draw a closed outline to make a shape.");
            ShapeUpgrade_UnifySameDomain unify(result, true, true, false);
            unify.Build();
            return unify.Shape();
        } catch (const Standard_Failure& e) {
            throw GeometryError(std::string("The sketch could not be pushed up with that taper: ") +
                                e.GetMessageString());
        }
    }
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

namespace {

// The faces of a sketch, one by one.
std::vector<TopoDS_Face> faceList(const Sketch& s)
{
    std::vector<TopoDS_Face> out;
    for (TopExp_Explorer it(faces(s), TopAbs_FACE); it.More(); it.Next())
        out.push_back(TopoDS::Face(it.Current()));
    return out;
}

TopoDS_Shape fused(const std::vector<TopoDS_Shape>& solids, const char* what)
{
    if (solids.empty())
        throw GeometryError(std::string("Nothing to make the ") + what + " from");
    TopoDS_Shape result = solids.front();
    for (std::size_t i = 1; i < solids.size(); ++i) {
        BRepAlgoAPI_Fuse fuse(result, solids[i]);
        if (!fuse.IsDone() || fuse.HasErrors())
            throw GeometryError(std::string("The parts of the ") + what + " overlap in a way that can't be joined");
        result = fuse.Shape();
    }
    return result;
}

// One face's outline swept (holes swept too and cut out) by a pipe set up by `setUp`.
template <typename SetUp>
TopoDS_Shape sweptFace(const TopoDS_Face& face, const TopoDS_Wire& spine, SetUp setUp)
{
    const auto sweepWire = [&](const TopoDS_Wire& wire) {
        BRepOffsetAPI_MakePipeShell pipe(spine);
        setUp(pipe);
        pipe.Add(wire);
        pipe.Build();
        if (!pipe.IsDone() || !pipe.MakeSolid())
            throw GeometryError("The outline could not be swept along the path (does it turn too tightly for the "
                                "outline's size?)");
        return pipe.Shape();
    };
    const TopoDS_Wire outer = BRepTools::OuterWire(face);
    TopoDS_Shape solid = sweepWire(outer);
    for (TopExp_Explorer it(face, TopAbs_WIRE); it.More(); it.Next()) {
        const TopoDS_Wire w = TopoDS::Wire(it.Current());
        if (w.IsSame(outer))
            continue;
        BRepAlgoAPI_Cut cut(solid, sweepWire(w));
        if (!cut.IsDone() || cut.HasErrors())
            throw GeometryError("A hole in the outline could not be swept");
        solid = cut.Shape();
    }
    return solid;
}

Bnd_Box boxOf(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    return box;
}

}  // namespace

std::vector<std::vector<Point>> outlinesOf(const Sketch& s)
{
    std::vector<std::vector<Point>> out;
    for (const Geo& g : s.geos) {
        if (g.construction)
            continue;
        const Point a = s.points[std::size_t(g.a)];
        if (g.kind == GeoKind::Line) {
            out.push_back({a, s.points[std::size_t(g.b)]});
            continue;
        }
        const double r = radiusOf(s, g);
        double a0 = 0.0, span = 2.0 * std::numbers::pi;
        if (g.kind == GeoKind::Arc) {
            const Point p0 = s.points[std::size_t(g.b)], p1 = s.points[std::size_t(g.c)];
            a0 = std::atan2(p0.y - a.y, p0.x - a.x);
            span = std::atan2(p1.y - a.y, p1.x - a.x) - a0;
            if (span <= 0)
                span += 2.0 * std::numbers::pi;
        }
        std::vector<Point> line;
        const int n = std::max(8, int(48 * span / (2.0 * std::numbers::pi)));
        for (int i = 0; i <= n; ++i) {
            const double t = a0 + span * i / n;
            line.push_back({a.x + r * std::cos(t), a.y + r * std::sin(t)});
        }
        out.push_back(std::move(line));
    }
    return out;
}

TopoDS_Shape loftShape(const LoftSpec& loft)
{
    if (loft.sections.size() < 2)
        throw GeometryError("A loft needs at least two outlines: add one above the first.");
    try {
        BRepOffsetAPI_ThruSections through(/*isSolid=*/true, loft.ruled);
        double last = -1e300;
        for (std::size_t i = 0; i < loft.sections.size(); ++i) {
            const LoftSection& section = loft.sections[i];
            if (!(section.height > last))
                throw GeometryError("Each outline of a loft must be higher than the one before it.");
            last = section.height;
            const std::vector<TopoDS_Face> list = faceList(section.sketch);
            if (list.size() != 1)
                throw GeometryError("Outline " + std::to_string(i + 1) +
                                    " of the loft must be one closed shape (it has " + std::to_string(list.size()) +
                                    ").");
            TopoDS_Wire wire = BRepTools::OuterWire(list.front());
            if (section.height != 0.0) {
                gp_Trsf up;
                up.SetTranslation(gp_Vec(0, 0, section.height));
                wire = TopoDS::Wire(BRepBuilderAPI_Transform(wire, up, true).Shape());
            }
            through.AddWire(wire);
        }
        through.CheckCompatibility(Standard_True);
        through.Build();
        if (!through.IsDone())
            throw GeometryError("The outlines could not be blended into one shape.");
        return through.Shape();
    } catch (const Standard_Failure& e) {
        throw GeometryError(std::string("The loft could not be made: ") + e.GetMessageString());
    }
}

TopoDS_Shape sweepPath(const SweepSpec& sweep)
{
    Handle(TopTools_HSequenceOfShape) edges = new TopTools_HSequenceOfShape;
    const Sketch& s = sweep.path;
    for (const Geo& g : s.geos) {
        if (g.construction)
            continue;
        switch (g.kind) {
        case GeoKind::Line: {
            const gp_Pnt a = at(s, g.a), b = at(s, g.b);
            if (a.Distance(b) > 1e-9)
                edges->Append(BRepBuilderAPI_MakeEdge(a, b).Edge());
            break;
        }
        case GeoKind::Arc: {
            const double r = radiusOf(s, g);
            if (!(r > 1e-9))
                break;
            GC_MakeArcOfCircle arc(gp_Circ(gp_Ax2(at(s, g.a), gp::DZ()), r), at(s, g.b), at(s, g.c), true);
            if (arc.IsDone())
                edges->Append(BRepBuilderAPI_MakeEdge(arc.Value(), at(s, g.b), at(s, g.c)).Edge());
            break;
        }
        case GeoKind::Circle:
            throw GeometryError("A sweep's path can't be a whole circle: use arcs, or a Revolve.");
        }
    }
    if (edges->Length() == 0)
        throw GeometryError("Draw the path: lines and arcs joined end to end, starting at the dot.");
    Handle(TopTools_HSequenceOfShape) joined;
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, 1e-6, /*shared=*/false, joined);
    if (joined->Length() != 1)
        throw GeometryError("The path must be one line, joined end to end (it's in " +
                            std::to_string(joined->Length()) + " pieces).");
    TopoDS_Wire wire = TopoDS::Wire(joined->Value(1));
    // It starts at the origin (where the outline is): turned round if drawn the other way.
    TopoDS_Vertex first, last;
    TopExp::Vertices(wire, first, last);
    const double tolerance = 1e-6 * std::max(1.0, std::sqrt(boxOf(wire).SquareExtent()));
    const auto atOrigin = [&](const TopoDS_Vertex& v) {
        return !v.IsNull() && BRep_Tool::Pnt(v).Distance(gp::Origin()) <= std::max(tolerance, 1e-6);
    };
    if (!atOrigin(first)) {
        if (!atOrigin(last))
            throw GeometryError("Start the path at the dot (where the outline is).");
        BRepBuilderAPI_MakeWire reversed;
        std::vector<TopoDS_Edge> order;
        for (BRepTools_WireExplorer it(wire); it.More(); it.Next())
            order.push_back(it.Current());
        for (auto it = order.rbegin(); it != order.rend(); ++it)
            reversed.Add(TopoDS::Edge(it->Reversed()));
        wire = reversed.Wire();
    }
    // Standing up: the path sketch's Y is the sweep's Z.
    gp_Trsf stand;
    stand.SetRotation(gp_Ax1(gp::Origin(), gp::DX()), std::numbers::pi / 2);
    return BRepBuilderAPI_Transform(wire, stand, true).Shape();
}

TopoDS_Shape sweepShape(const SweepSpec& sweep)
{
    try {
        const std::vector<TopoDS_Face> list = faceList(sweep.profile);
        std::vector<TopoDS_Shape> solids;
        if (!sweep.spiral) {
            const TopoDS_Wire spine = TopoDS::Wire(sweepPath(sweep));
            for (const TopoDS_Face& f : list)
                solids.push_back(sweptFace(f, spine, [](BRepOffsetAPI_MakePipeShell& pipe) {
                    pipe.SetMode(Standard_False);  // corrected Frenet: no twisting along the way
                    pipe.SetTransitionMode(BRepBuilderAPI_RoundCorner);
                }));
            return fused(solids, "sweep");
        }
        // A spiral round the sketch's Y line, through the outline's middle.
        if (!(sweep.pitch > 0.0) || !(sweep.turns > 0.0))
            throw GeometryError("A spiral needs a pitch and a number of turns above zero.");
        const Bnd_Box box = boxOf(faces(sweep.profile));
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        if (x0 < -1e-9 && x1 > 1e-9)
            throw GeometryError("Keep the outline to one side of the up-down line through the dot: the spiral "
                                "goes round that line.");
        if (sweep.pitch <= (y1 - y0) + 1e-6)
        {
            char height[32];
            std::snprintf(height, sizeof height, "%.2f", y1 - y0);
            throw GeometryError(std::string("The pitch must be more than the outline's height (") + height +
                                " mm), or the turns run into each other.");
        }
        const bool left = x1 <= 1e-9;  // drawn on the left of the line
        const double radius = std::abs((x0 + x1) / 2), middleY = (y0 + y1) / 2;
        // On a cylinder round Y: the helix is a straight line in its (angle, height) space.
        const gp_Ax3 frame(gp::Origin(), gp::DY(), left ? gp::DX().Reversed() : gp::DX());
        Handle(Geom_CylindricalSurface) cylinder = new Geom_CylindricalSurface(frame, radius);
        const double hand = (sweep.leftHand ? -1.0 : 1.0) * (left ? -1.0 : 1.0);
        const gp_Dir2d along(hand * 2.0 * std::numbers::pi, sweep.pitch);
        const double length = sweep.turns * std::hypot(2.0 * std::numbers::pi, sweep.pitch);
        Handle(Geom2d_TrimmedCurve) line =
            new Geom2d_TrimmedCurve(new Geom2d_Line(gp_Pnt2d(0.0, middleY), along), 0.0, length);
        TopoDS_Edge helix = BRepBuilderAPI_MakeEdge(line, cylinder).Edge();
        BRepLib::BuildCurves3d(helix);
        const TopoDS_Wire spine = BRepBuilderAPI_MakeWire(helix).Wire();
        for (const TopoDS_Face& f : list)
            solids.push_back(sweptFace(f, spine, [](BRepOffsetAPI_MakePipeShell& pipe) {
                pipe.SetMode(gp::DY());  // keeps facing the axis, as a spring does
            }));
        return fused(solids, "spiral");
    } catch (const Standard_Failure& e) {
        throw GeometryError(std::string("The sweep could not be made: ") + e.GetMessageString());
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

Transform middlePlane(int which, const Vec3& centre)
{
    Transform t;
    // Columns: X, Y, Z of the sketch in plate coordinates.
    const auto set = [&t](const Vec3& x, const Vec3& y, const Vec3& z) {
        for (std::size_t r = 0; r < 3; ++r) {
            t.m[r][0] = x[r];
            t.m[r][1] = y[r];
            t.m[r][2] = z[r];
        }
    };
    if (which == 0)
        set({1, 0, 0}, {0, 0, 1}, {0, -1, 0});
    else if (which == 1)
        set({0, 1, 0}, {0, 0, 1}, {1, 0, 0});
    else
        set({1, 0, 0}, {0, 1, 0}, {0, 0, 1});
    for (std::size_t r = 0; r < 3; ++r)
        t.m[r][3] = centre[r];
    return t;
}

std::vector<std::vector<Point>> sectionOutlines(const TopoDS_Shape& shape, const Transform& frame)
{
    std::vector<std::vector<Point>> out;
    const Vec3 o = frame.apply({0, 0, 0});
    const Vec3 n = frame.applyDirection({0, 0, 1});
    const Transform toSketch = frame.inverse();
    try {
        BRepAlgoAPI_Section section(shape, gp_Pln(gp_Pnt(o[0], o[1], o[2]), gp_Dir(n[0], n[1], n[2])), Standard_False);
        section.Approximation(Standard_True);
        section.Build();
        if (!section.IsDone())
            return out;
        Bnd_Box box;
        BRepBndLib::Add(shape, box);
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        const double deflection = std::max(std::hypot(x1 - x0, y1 - y0, z1 - z0) * 2e-3, 1e-3);
        for (TopExp_Explorer e(section.Shape(), TopAbs_EDGE); e.More(); e.Next()) {
            BRepAdaptor_Curve curve(TopoDS::Edge(e.Current()));
            std::vector<Point> line;
            const auto add = [&](const gp_Pnt& p) {
                const Vec3 q = toSketch.apply({p.X(), p.Y(), p.Z()});
                line.push_back({q[0], q[1]});
            };
            if (curve.GetType() == GeomAbs_Line) {
                add(curve.Value(curve.FirstParameter()));
                add(curve.Value(curve.LastParameter()));
            } else {
                GCPnts_QuasiUniformDeflection sampler(curve, deflection);
                if (!sampler.IsDone())
                    continue;
                for (int k = 1; k <= sampler.NbPoints(); ++k)
                    add(sampler.Value(k));
            }
            if (line.size() >= 2)
                out.push_back(std::move(line));
        }
    } catch (const Standard_Failure&) {
    }
    return out;
}

}  // namespace forge::sketch
