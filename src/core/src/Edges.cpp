// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Edges.h"

#include "forge/core/Document.h"

#include <BRepAdaptor_Curve.hxx>
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
    case EdgeFinishKind::Fillet: return bend == Bend::Concave;
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
    const double diag = diagonal(shape);
    const double tolerance = std::max(diag * 2e-3, 1e-3);
    std::vector<std::pair<int, int>> picks;  // edge, face index for Bevel
    for (const EdgeRef& ref : finish.edges) {
        int e = edgeFor(found, ref, tolerance);
        if (e < 0) {
            // The shape under it changed (resized, say): take the nearest edge it suits
            // that isn't picked already, if it's close enough to be the same one moved.
            double best = diag * 0.2;
            for (std::size_t i = 0; i < found.edges.size(); ++i) {
                if (!suits(finish.kind, found.edges[i].bend))
                    continue;
                if (std::any_of(picks.begin(), picks.end(), [i](const auto& p) { return p.first == int(i); }))
                    continue;
                const double d = pointToPolyline(ref.point, found.edges[i].polyline);
                if (d < best) {
                    best = d;
                    e = int(i);
                }
            }
        }
        if (e < 0)
            throw GeometryError("A picked edge is no longer there (the shape under it changed). Pick the "
                                "edges again.");
        int face = found.edges[std::size_t(e)].faces[0];
        if (finish.kind == EdgeFinishKind::Bevel) {
            // The face it was picked from: the one its face point lies on.
            const TopoDS_Vertex at =
                BRepBuilderAPI_MakeVertex(gp_Pnt(ref.facePoint[0], ref.facePoint[1], ref.facePoint[2])).Vertex();
            double bestD = 1e300;
            for (int f : found.edges[std::size_t(e)].faces) {
                BRepExtrema_DistShapeShape dist(at, maps.faces.FindKey(f + 1));
                if (dist.IsDone() && dist.Value() < bestD) {
                    bestD = dist.Value();
                    face = f;
                }
            }
        }
        if (std::find_if(picks.begin(), picks.end(), [e](const auto& p) { return p.first == e; }) == picks.end())
            picks.emplace_back(e, face);
    }
    try {
        TopoDS_Shape result;
        if (finish.kind == EdgeFinishKind::Rounding || finish.kind == EdgeFinishKind::Fillet) {
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
    } catch (const GeometryError&) {
    }
    switch (finish.kind) {
    case EdgeFinishKind::Rounding:
    case EdgeFinishKind::Fillet:
        throw GeometryError("Those edges could not be rounded at that radius. Try a smaller radius, or fewer "
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
    return {t.apply(r.point), t.apply(r.facePoint)};
}
}  // namespace

TopoDS_Shape pickableShape(const Document& doc, ObjectId target)
{
    const Body* body = doc.find(target);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(target));
    ObjectId part = target;
    if (body->isBoolean() && body->boolean->op == BooleanOp::Edges)
        part = body->boolean->operands.front();
    if (doc.usesMeshEngine(part))
        throw std::invalid_argument("Rounding and chamfering work on solid shapes, not on imported meshes or "
                                    "shapes made with them.");
    return BRepBuilderAPI_Transform(doc.shape(part), toTrsf(doc.worldTransform(part)), /*copy=*/true).Shape();
}

BooleanSpec edgesSpec(const Document& doc, ObjectId target, const EdgeFinish& finish)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (!doc.isTopLevel(target))
        throw std::invalid_argument("Only a shape on the plate can have its edges changed");
    if (doc.usesMeshEngine(target))
        throw std::invalid_argument("Rounding and chamfering work on solid shapes, not on imported meshes or "
                                    "shapes made with them.");
    BooleanSpec spec;
    spec.op = BooleanOp::Edges;
    spec.operands = {target};
    spec.edges = finish;  // a new result sits where its part is: plate coordinates are its own
    const auto centre = doc.worldBounds(target).center();
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
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
    next.boolean->edges = f;
    return next;
}

}  // namespace forge
