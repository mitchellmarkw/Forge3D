// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Edges.h"

#include "forge/core/Document.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <Geom2d_Curve.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_State.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Lin.hxx>
#include <gp_Trsf.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec2d.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace forge {

using edges::Bend;
using edges::ShapeEdge;
using edges::ShapeEdges;

namespace {

double diagonal(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    if (box.IsVoid())
        return 1.0;
    return std::sqrt(box.SquareExtent());
}

Vec3 toVec(const gp_Pnt& p) { return {p.X(), p.Y(), p.Z()}; }
Vec3 toVec(const gp_Vec& v) { return {v.X(), v.Y(), v.Z()}; }

// The shape's tight box, as min and size per axis.
struct Box {
    Vec3 min{0, 0, 0}, size{1, 1, 1};
    Vec3 relative(const Vec3& p) const
    {
        Vec3 r{};
        for (int k = 0; k < 3; ++k)
            r[std::size_t(k)] = size[std::size_t(k)] > 1e-9 ? (p[std::size_t(k)] - min[std::size_t(k)]) / size[std::size_t(k)]
                                                             : 0.5;
        return r;
    }
};
Box boxOf(const TopoDS_Shape& shape)
{
    Bnd_Box b;
    BRepBndLib::AddOptimal(shape, b, /*useTriangulation=*/false, /*useShapeTolerance=*/false);
    Box out;
    if (b.IsVoid())
        return out;
    double x0, y0, z0, x1, y1, z1;
    b.Get(x0, y0, z0, x1, y1, z1);
    out.min = {x0, y0, z0};
    out.size = {x1 - x0, y1 - y0, z1 - z0};
    return out;
}
double distance3(const Vec3& a, const Vec3& b)
{
    return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}

// The face's outward facing at a point of it (face orientation included).
std::optional<gp_Vec> outward(const TopoDS_Face& face, const gp_Pnt2d& uv)
{
    BRepGProp_Face prop(face);
    gp_Pnt p;
    gp_Vec n;
    prop.Normal(uv.X(), uv.Y(), p, n);
    if (n.Magnitude() < 1e-12)
        return std::nullopt;
    return n.Normalized();
}

// A point on `face` a little way in from the edge, at the edge's middle.
std::optional<gp_Pnt> insideNear(const TopoDS_Edge& edge, const TopoDS_Face& face, double step)
{
    double f = 0, l = 0;
    const auto pcurve = BRep_Tool::CurveOnSurface(edge, face, f, l);
    if (pcurve.IsNull())
        return std::nullopt;
    gp_Pnt2d uv;
    gp_Vec2d tangent;
    pcurve->D1((f + l) / 2.0, uv, tangent);
    if (tangent.Magnitude() < 1e-14)
        return std::nullopt;
    tangent.Normalize();
    const auto surface = BRep_Tool::Surface(face);
    double u0 = 0, u1 = 0, v0 = 0, v1 = 0;
    BRepTools::UVBounds(face, u0, u1, v0, v1);
    const double span = std::max(u1 - u0, v1 - v0);
    for (double frac : {1e-3, 1e-2, 3e-2}) {
        for (double side : {1.0, -1.0}) {
            const gp_Pnt2d tryUv(uv.X() - side * tangent.Y() * span * frac, uv.Y() + side * tangent.X() * span * frac);
            BRepClass_FaceClassifier inFace(face, tryUv, 1e-7);
            if (inFace.State() == TopAbs_IN) {
                const gp_Pnt p = surface->Value(tryUv.X(), tryUv.Y());
                if (p.Distance(surface->Value(uv.X(), uv.Y())) <= step * 50.0)
                    return p;
            }
        }
    }
    return std::nullopt;
}

double pointToPolyline(const Vec3& p, const std::vector<Vec3>& line)
{
    double best = 1e300;
    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        const Vec3& a = line[i];
        const Vec3& b = line[i + 1];
        const Vec3 ab{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const Vec3 ap{p[0] - a[0], p[1] - a[1], p[2] - a[2]};
        const double l2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];
        const double t = l2 > 0 ? std::clamp((ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / l2, 0.0, 1.0) : 0.0;
        const Vec3 d{ap[0] - ab[0] * t, ap[1] - ab[1] * t, ap[2] - ab[2] * t};
        best = std::min(best, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
    }
    if (line.size() == 1)
        best = std::hypot(p[0] - line[0][0], p[1] - line[0][1], p[2] - line[0][2]);
    return best;
}

struct Maps {
    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    std::vector<TopoDS_Edge> edges;  // the usable ones, in ShapeEdges order
};

// Both analyse() and apply() walk the shape the same way, so indices agree.
ShapeEdges analyseWith(const TopoDS_Shape& shape, Maps& maps)
{
    TopExp::MapShapes(shape, TopAbs_FACE, maps.faces);
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, maps.edgeFaces);
    const double diag = diagonal(shape);
    const double deflection = std::max(diag * 1e-3, 1e-4);
    ShapeEdges out;
    out.faceEdges.resize(std::size_t(maps.faces.Extent()));
    for (int i = 1; i <= maps.edgeFaces.Extent(); ++i) {
        const TopoDS_Edge edge = TopoDS::Edge(maps.edgeFaces.FindKey(i));
        if (BRep_Tool::Degenerated(edge))
            continue;
        // The distinct faces on either side.
        std::vector<TopoDS_Face> faces;
        for (TopTools_ListOfShape::Iterator it(maps.edgeFaces.FindFromIndex(i)); it.More(); it.Next()) {
            const TopoDS_Face f = TopoDS::Face(it.Value());
            if (std::none_of(faces.begin(), faces.end(), [&](const TopoDS_Face& g) { return g.IsSame(f); }))
                faces.push_back(f);
        }
        if (faces.size() != 2)
            continue;  // a seam (one face on both sides) or an open edge
        ShapeEdge e;
        BRepAdaptor_Curve curve(edge);
        GCPnts_QuasiUniformDeflection sampler(curve, deflection);
        if (sampler.IsDone() && sampler.NbPoints() >= 2) {
            for (int k = 1; k <= sampler.NbPoints(); ++k)
                e.polyline.push_back(toVec(sampler.Value(k)));
        } else {
            e.polyline = {toVec(curve.Value(curve.FirstParameter())), toVec(curve.Value(curve.LastParameter()))};
        }
        const double midParam = (curve.FirstParameter() + curve.LastParameter()) / 2.0;
        const gp_Pnt mid = curve.Value(midParam);
        e.mid = toVec(mid);

        // How the faces meet at the middle of the edge.
        std::array<std::optional<gp_Vec>, 2> normals;
        for (int k = 0; k < 2; ++k) {
            double f = 0, l = 0;
            const auto pcurve = BRep_Tool::CurveOnSurface(edge, faces[std::size_t(k)], f, l);
            if (!pcurve.IsNull())
                normals[std::size_t(k)] = outward(faces[std::size_t(k)], pcurve->Value(midParam));
            e.faces[std::size_t(k)] = maps.faces.FindIndex(faces[std::size_t(k)]) - 1;
        }
        for (int k = 0; k < 2; ++k)
            if (normals[std::size_t(k)])
                e.normals[std::size_t(k)] = toVec(*normals[std::size_t(k)]);
        if (normals[0] && normals[1]) {
            const double cosine = normals[0]->Dot(*normals[1]);
            if (cosine > std::cos(5.0 * std::numbers::pi / 180.0)) {
                e.bend = Bend::Smooth;
            } else {
                // Walking off the edge across one face: on an outside edge that heads
                // against the other face's outward side (round a box's corner); in an
                // inside corner it heads the same way (out from a peg across the plate).
                e.bend = Bend::Convex;
                if (const auto across = insideNear(edge, faces[0], diag * 1e-3)) {
                    const gp_Vec away(mid, *across);
                    if (away.Magnitude() > 1e-12)
                        e.bend = away.Dot(*normals[1]) > 0.0 ? Bend::Concave : Bend::Convex;
                }
            }
        }
        const int index = int(out.edges.size());
        out.edges.push_back(std::move(e));
        maps.edges.push_back(edge);
        for (int f : out.edges.back().faces)
            if (f >= 0)
                out.faceEdges[std::size_t(f)].push_back(index);
    }
    return out;
}


// Everything within `radius` of the edges, cut out of the shape: a tube along each edge
// (a cylinder for a straight one, part of a ring for an arc, a swept circle otherwise)
// and a ball at each end, so edges meeting at a corner join smoothly.
TopoDS_Shape cutCoves(const TopoDS_Shape& shape, const std::vector<TopoDS_Edge>& along, double radius)
{
    TopTools_ListOfShape tools;
    std::vector<gp_Pnt> ends;
    const auto addEnd = [&](const gp_Pnt& p) {
        for (const gp_Pnt& q : ends)
            if (q.Distance(p) < 1e-7)
                return;
        ends.push_back(p);
        tools.Append(BRepPrimAPI_MakeSphere(p, radius).Shape());
    };
    for (const TopoDS_Edge& edge : along) {
        BRepAdaptor_Curve curve(edge);
        const double f = curve.FirstParameter(), l = curve.LastParameter();
        const gp_Pnt a = curve.Value(f), b = curve.Value(l);
        switch (curve.GetType()) {
        case GeomAbs_Line: {
            const double length = a.Distance(b);
            if (length < 1e-9)
                continue;
            tools.Append(BRepPrimAPI_MakeCylinder(gp_Ax2(a, gp_Dir(gp_Vec(a, b))), radius, length).Shape());
            break;
        }
        case GeomAbs_Circle: {
            const gp_Circ circle = curve.Circle();
            if (radius >= circle.Radius())
                throw GeometryError("The cove radius must be smaller than the round edge's own radius");
            // The ring starts where the arc does and runs the same way round.
            gp_Ax2 axes = circle.Position();
            const double span = std::min(l - f, 2.0 * std::numbers::pi);
            axes.Rotate(axes.Axis(), f);
            tools.Append(BRepPrimAPI_MakeTorus(axes, circle.Radius(), radius, span).Shape());
            break;
        }
        default: {
            const TopoDS_Wire spine = BRepBuilderAPI_MakeWire(edge).Wire();
            gp_Pnt start;
            gp_Vec tangent;
            curve.D1(f, start, tangent);
            const TopoDS_Face profile = BRepBuilderAPI_MakeFace(
                BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(start, gp_Dir(tangent)), radius)))
                    .Wire())
                                            .Face();
            BRepOffsetAPI_MakePipe pipe(spine, profile);
            pipe.Build();
            if (!pipe.IsDone())
                throw GeometryError("");
            tools.Append(pipe.Shape());
            break;
        }
        }
        if (!BRep_Tool::IsClosed(edge)) {
            addEnd(a);
            addEnd(b);
        }
    }
    TopTools_ListOfShape arguments;
    arguments.Append(shape);
    BRepAlgoAPI_Cut cut;
    cut.SetArguments(arguments);
    cut.SetTools(tools);
    cut.Build();
    if (!cut.IsDone() || cut.HasErrors())
        throw GeometryError("");
    ShapeUpgrade_UnifySameDomain unify(cut.Shape(), true, true, false);
    unify.Build();
    return unify.Shape();
}

}  // namespace

namespace edges {

ShapeEdges analyse(const TopoDS_Shape& shape)
{
    Maps maps;
    return analyseWith(shape, maps);
}

bool suits(EdgeFinishKind kind, Bend bend)
{
    switch (kind) {
    case EdgeFinishKind::Rounding: return bend == Bend::Convex;
    case EdgeFinishKind::Fillet: return bend != Bend::Smooth;  // inside: filled; outside: a cove cut in
    case EdgeFinishKind::Chamfer:
    case EdgeFinishKind::Bevel: return bend != Bend::Smooth;
    }
    return false;
}

struct FaceFinder::Impl {
    IntCurvesFace_ShapeIntersector hits;
    TopTools_IndexedMapOfShape faces;
};

FaceFinder::FaceFinder(const TopoDS_Shape& shape) : m_impl(std::make_unique<Impl>())
{
    m_impl->hits.Load(shape, 1e-7);
    TopExp::MapShapes(shape, TopAbs_FACE, m_impl->faces);
}

FaceFinder::~FaceFinder() = default;

std::optional<FaceHit> FaceFinder::under(const Vec3& origin, const Vec3& direction) const
{
    const gp_Vec d(direction[0], direction[1], direction[2]);
    if (d.Magnitude() < 1e-12)
        return std::nullopt;
    auto& hits = m_impl->hits;
    hits.Perform(gp_Lin(gp_Pnt(origin[0], origin[1], origin[2]), gp_Dir(d)), 0.0, 1e12);
    if (!hits.IsDone() || hits.NbPnt() == 0)
        return std::nullopt;
    int best = 1;
    for (int i = 2; i <= hits.NbPnt(); ++i)
        if (hits.WParameter(i) < hits.WParameter(best))
            best = i;
    FaceHit hit;
    hit.face = m_impl->faces.FindIndex(hits.Face(best)) - 1;
    hit.point = toVec(hits.Pnt(best));
    hit.distance = hits.WParameter(best);  // mm: the line runs along a unit direction
    if (hit.face < 0)
        return std::nullopt;
    return hit;
}

std::optional<FaceHit> faceUnder(const TopoDS_Shape& shape, const Vec3& origin, const Vec3& direction)
{
    return FaceFinder(shape).under(origin, direction);
}

EdgeRef refFor(const TopoDS_Shape& shape, const ShapeEdges& found, int edge, int face)
{
    if (edge < 0 || std::size_t(edge) >= found.edges.size())
        throw std::out_of_range("No such edge");
    Maps maps;
    analyseWith(shape, maps);  // the same order as `found`
    const ShapeEdge& e = found.edges[std::size_t(edge)];
    int side = 0;
    if (face >= 0 && e.faces[1] == face)
        side = 1;
    EdgeRef ref;
    ref.point = e.mid;
    ref.facePoint = e.mid;
    const TopoDS_Face f = TopoDS::Face(maps.faces.FindKey(e.faces[std::size_t(side)] + 1));
    if (const auto p = insideNear(maps.edges[std::size_t(edge)], f, diagonal(shape) * 1e-3))
        ref.facePoint = toVec(*p);
    return ref;
}

int edgeFor(const ShapeEdges& found, const EdgeRef& ref, double tolerance)
{
    int best = -1;
    double bestD = tolerance;
    for (std::size_t i = 0; i < found.edges.size(); ++i) {
        const double d = pointToPolyline(ref.point, found.edges[i].polyline);
        if (d <= bestD) {
            bestD = d;
            best = int(i);
        }
    }
    return best;
}

void locate(const TopoDS_Shape& part, std::vector<EdgeRef>& refs)
{
    Maps maps;
    const ShapeEdges found = analyseWith(part, maps);
    const Box box = boxOf(part);
    const double tolerance = std::max(diagonal(part) * 2e-3, 1e-3);
    for (EdgeRef& ref : refs) {
        const int e = edgeFor(found, ref, tolerance);
        if (e < 0) {
            ref.located = false;
            continue;
        }
        const ShapeEdge& edge = found.edges[std::size_t(e)];
        // The face it was picked from: the one its face point lies on.
        const TopoDS_Vertex at =
            BRepBuilderAPI_MakeVertex(gp_Pnt(ref.facePoint[0], ref.facePoint[1], ref.facePoint[2])).Vertex();
        int side = 0;
        double bestD = 1e300;
        for (int k = 0; k < 2; ++k) {
            BRepExtrema_DistShapeShape dist(at, maps.faces.FindKey(edge.faces[std::size_t(k)] + 1));
            if (dist.IsDone() && dist.Value() < bestD) {
                bestD = dist.Value();
                side = k;
            }
        }
        ref.relative = box.relative(edge.mid);
        ref.faceNormal = edge.normals[std::size_t(side)];
        ref.located = true;
    }
}

std::vector<int> match(const TopoDS_Shape& shape, const ShapeEdges& found, const std::vector<EdgeRef>& refs,
                       EdgeFinishKind kind)
{
    std::vector<int> out(refs.size(), -1);
    std::vector<bool> taken(found.edges.size(), false);
    const double diag = diagonal(shape);
    const double tolerance = std::max(diag * 2e-3, 1e-3);
    // 1. Still exactly where it was.
    for (std::size_t i = 0; i < refs.size(); ++i) {
        const int e = edgeFor(found, refs[i], tolerance);
        if (e >= 0 && !taken[std::size_t(e)]) {
            out[i] = e;
            taken[std::size_t(e)] = true;
        }
    }
    // 2. At the same spot in the shape's box (the part was resized), facing the same way.
    const Box box = boxOf(shape);
    for (std::size_t i = 0; i < refs.size(); ++i) {
        if (out[i] >= 0 || !refs[i].located)
            continue;
        double best = 0.12;
        for (std::size_t k = 0; k < found.edges.size(); ++k) {
            const ShapeEdge& edge = found.edges[k];
            if (taken[k] || !suits(kind, edge.bend))
                continue;
            double d = distance3(box.relative(edge.mid), refs[i].relative);
            // An edge between faces facing other ways isn't the same edge.
            const auto& n = refs[i].faceNormal;
            const double facing = std::max(edge.normals[0][0] * n[0] + edge.normals[0][1] * n[1] + edge.normals[0][2] * n[2],
                                           edge.normals[1][0] * n[0] + edge.normals[1][1] * n[1] + edge.normals[1][2] * n[2]);
            if (facing < 0.9)
                continue;
            if (d < best) {
                best = d;
                out[i] = int(k);
            }
        }
        if (out[i] >= 0)
            taken[std::size_t(out[i])] = true;
    }
    // 3. The nearest one it suits, if close enough to be the same one moved.
    for (std::size_t i = 0; i < refs.size(); ++i) {
        if (out[i] >= 0)
            continue;
        double best = diag * 0.2;
        for (std::size_t k = 0; k < found.edges.size(); ++k) {
            if (taken[k] || !suits(kind, found.edges[k].bend))
                continue;
            const double d = pointToPolyline(refs[i].point, found.edges[k].polyline);
            if (d < best) {
                best = d;
                out[i] = int(k);
            }
        }
        if (out[i] >= 0)
            taken[std::size_t(out[i])] = true;
    }
    return out;
}

TopoDS_Shape apply(const TopoDS_Shape& shape, const EdgeFinish& finish)
{
    if (!(finish.size > 0.0))
        throw std::invalid_argument("The size must be greater than zero");
    if (finish.kind == EdgeFinishKind::Bevel && !(finish.angle > 0.5 && finish.angle < 89.5))
        throw std::invalid_argument("The bevel angle must be between 0.5° and 89.5°");
    if (finish.edges.empty())
        throw std::invalid_argument("Pick at least one edge");
    Maps maps;
    const ShapeEdges found = analyseWith(shape, maps);
    std::vector<std::pair<int, int>> picks;  // edge, face index for Bevel
    const std::vector<int> matched = match(shape, found, finish.edges, finish.kind);
    for (std::size_t i = 0; i < matched.size(); ++i) {
        const int e = matched[i];
        const EdgeRef& ref = finish.edges[i];
        if (e < 0)
            throw GeometryError("A picked edge is no longer there (the shape under it changed). Pick the "
                                "edges again.");
        int face = found.edges[std::size_t(e)].faces[0];
        if (finish.kind == EdgeFinishKind::Bevel) {
            const auto& edge = found.edges[std::size_t(e)];
            if (ref.located) {
                // The face pointing the way the picked one did.
                const auto dot = [&ref](const Vec3& n) {
                    return n[0] * ref.faceNormal[0] + n[1] * ref.faceNormal[1] + n[2] * ref.faceNormal[2];
                };
                face = dot(edge.normals[1]) > dot(edge.normals[0]) ? edge.faces[1] : edge.faces[0];
            } else {
                // Older files: the face its face point lies on.
                const TopoDS_Vertex at =
                    BRepBuilderAPI_MakeVertex(gp_Pnt(ref.facePoint[0], ref.facePoint[1], ref.facePoint[2])).Vertex();
                double bestD = 1e300;
                for (int f : edge.faces) {
                    BRepExtrema_DistShapeShape dist(at, maps.faces.FindKey(f + 1));
                    if (dist.IsDone() && dist.Value() < bestD) {
                        bestD = dist.Value();
                        face = f;
                    }
                }
            }
        }
        picks.emplace_back(e, face);
    }
    try {
        TopoDS_Shape result;
        if (finish.kind == EdgeFinishKind::Fillet) {
            // Inside corners are filled; outside edges get a cove: everything within the
            // radius of the edge is cut away, as if a cylinder centred on it were subtracted.
            std::vector<int> inside, outside;
            for (const auto& [e, f] : picks)
                (found.edges[std::size_t(e)].bend == Bend::Concave ? inside : outside).push_back(e);
            result = shape;
            if (!inside.empty()) {
                BRepFilletAPI_MakeFillet fillet(shape);
                for (int e : inside)
                    fillet.Add(finish.size, maps.edges[std::size_t(e)]);
                fillet.Build();
                if (!fillet.IsDone())
                    throw GeometryError("");
                result = fillet.Shape();
            }
            if (!outside.empty()) {
                std::vector<TopoDS_Edge> along;
                for (int e : outside)
                    along.push_back(maps.edges[std::size_t(e)]);
                result = cutCoves(result, along, finish.size);
            }
        } else if (finish.kind == EdgeFinishKind::Rounding) {
            BRepFilletAPI_MakeFillet fillet(shape);
            for (const auto& [e, f] : picks)
                fillet.Add(finish.size, maps.edges[std::size_t(e)]);
            fillet.Build();
            if (!fillet.IsDone())
                throw GeometryError("");
            result = fillet.Shape();
        } else {
            BRepFilletAPI_MakeChamfer chamfer(shape);
            for (const auto& [e, f] : picks) {
                if (finish.kind == EdgeFinishKind::Chamfer)
                    chamfer.Add(finish.size, maps.edges[std::size_t(e)]);
                else
                    chamfer.AddDA(finish.size, finish.angle * std::numbers::pi / 180.0, maps.edges[std::size_t(e)],
                                  TopoDS::Face(maps.faces.FindKey(f + 1)));
            }
            chamfer.Build();
            if (!chamfer.IsDone())
                throw GeometryError("");
            result = chamfer.Shape();
        }
        if (result.IsNull())
            throw GeometryError("");
        return result;
    } catch (const Standard_Failure&) {
    } catch (const GeometryError& e) {
        if (*e.what() != '\0')
            throw;  // already says what's wrong
    }
    switch (finish.kind) {
    case EdgeFinishKind::Rounding:
        throw GeometryError("Those edges could not be rounded at that radius. Try a smaller radius, or fewer "
                            "edges at once.");
    case EdgeFinishKind::Fillet:
        throw GeometryError("Those edges could not be filleted at that radius. Try a smaller radius, or fewer "
                            "edges at once.");
    case EdgeFinishKind::Chamfer:
        throw GeometryError("Those edges could not be chamfered that far. Try a smaller size.");
    case EdgeFinishKind::Bevel:
        throw GeometryError("Those edges could not be bevelled like that. Try a smaller width or angle.");
    }
    throw GeometryError("The edges could not be changed");
}

}  // namespace edges
}  // namespace forge

namespace forge {

namespace {
gp_Trsf toTrsf(const Transform& t)
{
    gp_Trsf trsf;
    trsf.SetValues(t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3],  //
                   t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3],  //
                   t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);
    return trsf;
}
const Body& edgesResult(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Edges || !body->boolean->edges)
        throw std::invalid_argument("Not a shape with finished edges");
    return *body;
}
EdgeRef moved(const EdgeRef& r, const Transform& t)
{
    EdgeRef out = r;
    out.point = t.apply(r.point);
    out.facePoint = t.apply(r.facePoint);
    out.faceNormal = t.applyDirection(r.faceNormal);
    return out;
}
}  // namespace

TopoDS_Shape pickableShape(const Document& doc, ObjectId target)
{
    const Body* body = doc.find(target);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(target));
    ObjectId part = target;
    if (body->isBoolean() && (body->boolean->op == BooleanOp::Edges || body->boolean->op == BooleanOp::Shell ||
                               body->boolean->op == BooleanOp::Follow))
        part = body->boolean->operands.front();
    if (doc.usesMeshEngine(part))
        throw std::invalid_argument("This works on solid shapes, not on imported meshes or shapes made with them.");
    return BRepBuilderAPI_Transform(doc.shape(part), toTrsf(doc.worldTransform(part)), /*copy=*/true).Shape();
}

BooleanSpec edgesSpec(const Document& doc, ObjectId target, const EdgeFinish& finish)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (doc.usesMeshEngine(target))
        throw std::invalid_argument("Rounding and chamfering work on solid shapes, not on imported meshes or "
                                    "shapes made with them.");
    BooleanSpec spec;
    spec.op = BooleanOp::Edges;
    spec.operands = {target};
    // The new step sits where its part is: in the coordinates of whatever holds the part
    // (the plate for a shape on it; the next step up when added inside a history).
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    EdgeFinish local = finish;
    for (EdgeRef& r : local.edges)
        r = moved(r, toStep);
    edges::locate(BRepBuilderAPI_Transform(doc.shape(target), toTrsf(doc.localTransform(target)), true).Shape(),
                  local.edges);
    spec.edges = local;
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

std::vector<int> pickedEdges(const Document& doc, ObjectId result)
{
    const Body& body = edgesResult(doc, result);
    // Matched where they're stored (the result's own coordinates); the edges come out
    // in the same order as those of pickableShape().
    const ObjectId part = body.boolean->operands.front();
    const TopoDS_Shape placed =
        BRepBuilderAPI_Transform(doc.shape(part), toTrsf(doc.localTransform(part)), true).Shape();
    const auto found = edges::analyse(placed);
    return edges::match(placed, found, body.boolean->edges->edges, body.boolean->edges->kind);
}

EdgeFinish edgesInPlate(const Document& doc, ObjectId result)
{
    const Body& body = edgesResult(doc, result);
    EdgeFinish f = *body.boolean->edges;
    const Transform t = doc.worldTransform(result);
    for (EdgeRef& r : f.edges)
        r = moved(r, t);
    return f;
}

Body withEdges(const Document& doc, ObjectId result, const EdgeFinish& inPlate)
{
    Body next = edgesResult(doc, result);
    EdgeFinish f = inPlate;
    const Transform t = doc.worldTransform(result).inverse();
    for (EdgeRef& r : f.edges)
        r = moved(r, t);
    const ObjectId part = next.boolean->operands.front();
    edges::locate(BRepBuilderAPI_Transform(doc.shape(part), toTrsf(doc.localTransform(part)), true).Shape(), f.edges);
    next.boolean->edges = f;
    return next;
}

}  // namespace forge
