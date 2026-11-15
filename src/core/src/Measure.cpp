// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Measure.h"

#include "forge/core/Tessellator.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Circ.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Sphere.hxx>
#include <gp_Torus.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace forge::measure {

namespace {

Vec3 toVec(const gp_Pnt& p) { return {p.X(), p.Y(), p.Z()}; }
Vec3 toVec(const gp_Dir& d) { return {d.X(), d.Y(), d.Z()}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }

// The acute angle between two directions, in degrees (0..90).
double acuteAngle(const Vec3& a, const Vec3& b)
{
    const double c = std::abs(dot(a, b)) / std::max(norm(a) * norm(b), 1e-300);
    return std::acos(std::clamp(c, 0.0, 1.0)) * 180.0 / std::numbers::pi;
}

// How far point p lies from the line through q along d.
double fromLine(const Vec3& p, const Vec3& q, const Vec3& d)
{
    return norm(cross(sub(p, q), d)) / std::max(norm(d), 1e-300);
}

bool isAxis(About::Kind k) { return k == About::Kind::Cylinder || k == About::Kind::Cone; }

}  // namespace

struct Shape::Impl {
    TopoDS_Shape shape;
    TopTools_IndexedMapOfShape faces;
    std::vector<TopoDS_Edge> edges;
    std::unique_ptr<edges::FaceFinder> finder;
};

Shape::Shape(const TopoDS_Shape& placed) : m_impl(std::make_unique<Impl>())
{
    m_impl->shape = placed;
    TopExp::MapShapes(placed, TopAbs_FACE, m_impl->faces);
    Bnd_Box box;
    BRepBndLib::Add(placed, box);
    if (!box.IsVoid()) {
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        m_size = std::max(std::hypot(x1 - x0, y1 - y0, z1 - z0), 1e-6);
    }
    const double deflection = std::max(m_size * 1e-3, 1e-4);
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(placed, TopAbs_EDGE, TopAbs_FACE, edgeFaces);
    for (int i = 1; i <= edgeFaces.Extent(); ++i) {
        const TopoDS_Edge edge = TopoDS::Edge(edgeFaces.FindKey(i));
        if (BRep_Tool::Degenerated(edge))
            continue;
        std::vector<TopoDS_Face> faces;
        for (TopTools_ListOfShape::Iterator it(edgeFaces.FindFromIndex(i)); it.More(); it.Next()) {
            const TopoDS_Face f = TopoDS::Face(it.Value());
            if (std::none_of(faces.begin(), faces.end(), [&](const TopoDS_Face& g) { return g.IsSame(f); }))
                faces.push_back(f);
        }
        if (faces.size() < 2)
            continue;  // a seam down a round face: not a real edge
        BRepAdaptor_Curve curve(edge);
        std::vector<Vec3> line;
        GCPnts_QuasiUniformDeflection sampler(curve, deflection);
        if (sampler.IsDone() && sampler.NbPoints() >= 2)
            for (int k = 1; k <= sampler.NbPoints(); ++k)
                line.push_back(toVec(sampler.Value(k)));
        else
            line = {toVec(curve.Value(curve.FirstParameter())), toVec(curve.Value(curve.LastParameter()))};
        m_impl->edges.push_back(edge);
        m_lines.push_back(std::move(line));
        TopoDS_Vertex first, last;
        TopExp::Vertices(edge, first, last);
        for (const TopoDS_Vertex& v : {first, last}) {
            if (v.IsNull())
                continue;
            const Vec3 p = toVec(BRep_Tool::Pnt(v));
            if (std::none_of(m_corners.begin(), m_corners.end(),
                             [&](const Vec3& q) { return norm(sub(p, q)) < m_size * 1e-7; }))
                m_corners.push_back(p);
        }
    }
    m_impl->finder = std::make_unique<edges::FaceFinder>(placed);
}

Shape::~Shape() = default;

std::optional<edges::FaceHit> Shape::faceUnder(const Vec3& origin, const Vec3& direction) const
{
    return m_impl->finder->under(origin, direction);
}

Target Shape::edge(int index, const Vec3& clicked) const
{
    if (index < 0 || index >= int(m_impl->edges.size()))
        throw std::out_of_range("No such edge");
    return {Target::Kind::Edge, m_impl->edges[std::size_t(index)], clicked};
}

Target Shape::face(int index, const Vec3& clicked) const
{
    if (index < 0 || index >= m_impl->faces.Extent())
        throw std::out_of_range("No such face");
    return {Target::Kind::Face, m_impl->faces(index + 1), clicked};
}

Mesh Shape::faceMesh(int index) const
{
    if (index < 0 || index >= m_impl->faces.Extent())
        return {};
    return tessellate(m_impl->faces(index + 1));
}

About describe(const Target& t)
{
    About a;
    a.point = t.point;
    if (t.kind == Target::Kind::Point || t.shape.IsNull())
        return a;
    if (t.kind == Target::Kind::Edge) {
        const TopoDS_Edge edge = TopoDS::Edge(t.shape);
        BRepAdaptor_Curve curve(edge);
        a.length = GCPnts_AbscissaPoint::Length(curve);
        switch (curve.GetType()) {
        case GeomAbs_Line:
            a.kind = About::Kind::Line;
            a.hasDirection = true;
            a.direction = toVec(curve.Line().Direction());
            break;
        case GeomAbs_Circle: {
            const gp_Circ c = curve.Circle();
            const double sweep = std::abs(curve.LastParameter() - curve.FirstParameter());
            a.kind = std::abs(sweep - 2.0 * std::numbers::pi) < 1e-6 ? About::Kind::Circle : About::Kind::Arc;
            a.radius = c.Radius();
            a.hasCentre = true;
            a.centre = toVec(c.Location());
            break;
        }
        default: a.kind = About::Kind::Curve; break;
        }
        return a;
    }
    const TopoDS_Face face = TopoDS::Face(t.shape);
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    a.area = props.Mass();
    BRepAdaptor_Surface surface(face);
    switch (surface.GetType()) {
    case GeomAbs_Plane: {
        a.kind = About::Kind::Flat;
        a.hasDirection = true;
        gp_Dir n = surface.Plane().Axis().Direction();
        if (face.Orientation() == TopAbs_REVERSED)
            n.Reverse();
        a.direction = toVec(n);
        break;
    }
    case GeomAbs_Cylinder: {
        const gp_Cylinder c = surface.Cylinder();
        a.kind = About::Kind::Cylinder;
        a.radius = c.Radius();
        a.hasCentre = a.hasDirection = true;
        a.centre = toVec(c.Location());
        a.direction = toVec(c.Axis().Direction());
        break;
    }
    case GeomAbs_Cone: {
        const gp_Cone c = surface.Cone();
        a.kind = About::Kind::Cone;
        a.halfAngle = std::abs(c.SemiAngle()) * 180.0 / std::numbers::pi;
        a.hasCentre = a.hasDirection = true;
        a.centre = toVec(c.Apex());
        a.direction = toVec(c.Axis().Direction());
        break;
    }
    case GeomAbs_Sphere: {
        const gp_Sphere s = surface.Sphere();
        a.kind = About::Kind::Sphere;
        a.radius = s.Radius();
        a.hasCentre = true;
        a.centre = toVec(s.Location());
        break;
    }
    case GeomAbs_Torus: {
        const gp_Torus s = surface.Torus();
        a.kind = About::Kind::Torus;
        a.radius = s.MinorRadius();
        a.bigRadius = s.MajorRadius();
        a.hasCentre = true;
        a.centre = toVec(s.Location());
        break;
    }
    default: a.kind = About::Kind::Surface; break;
    }
    return a;
}

Between between(const Target& a, const Target& b)
{
    Between out;
    const auto shapeOf = [](const Target& t) {
        if (t.kind == Target::Kind::Point || t.shape.IsNull())
            return TopoDS_Shape(BRepBuilderAPI_MakeVertex(gp_Pnt(t.point[0], t.point[1], t.point[2])).Vertex());
        return t.shape;
    };
    BRepExtrema_DistShapeShape nearest(shapeOf(a), shapeOf(b));
    if (nearest.IsDone() && nearest.NbSolution() > 0) {
        out.distance = nearest.Value();
        out.from = toVec(nearest.PointOnShape1(1));
        out.to = toVec(nearest.PointOnShape2(1));
    } else {
        out.from = a.point;
        out.to = b.point;
        out.distance = norm(sub(b.point, a.point));
    }

    const About da = describe(a), db = describe(b);
    // Angles: lines and axes by their directions; flat faces by their facing.
    if (da.hasDirection && db.hasDirection) {
        const bool flatA = da.kind == About::Kind::Flat, flatB = db.kind == About::Kind::Flat;
        double angle = acuteAngle(da.direction, db.direction);
        if (flatA != flatB)
            angle = 90.0 - angle;  // a line against a face: measured from the face
        out.angle = angle;
        out.parallel = angle < 0.01;
        if (out.parallel) {
            // How far apart they lie, across them.
            if (flatA && flatB) {
                out.apart = std::abs(dot(sub(b.point, a.point), da.direction)) / norm(da.direction);
            } else if ((flatA && db.kind == About::Kind::Line) || (flatB && da.kind == About::Kind::Line)) {
                const About& flat = flatA ? da : db;
                const Target& other = flatA ? b : a;
                const Target& face = flatA ? a : b;
                out.apart = std::abs(dot(sub(other.point, face.point), flat.direction)) / norm(flat.direction);
            } else if (da.kind == About::Kind::Line && db.kind == About::Kind::Line) {
                out.apart = fromLine(b.point, a.point, da.direction);
            }
        }
    }
    // A flat face and a centre, or an axis along the face: straight from it to the face.
    for (int k = 0; k < 2; ++k) {
        const About& flat = k == 0 ? da : db;
        const About& other = k == 0 ? db : da;
        const Target& face = k == 0 ? a : b;
        if (flat.kind != About::Kind::Flat || !other.hasCentre)
            continue;
        if (isAxis(other.kind) && acuteAngle(other.direction, flat.direction) < 89.99)
            continue;  // an axis through the face, not along it
        const Vec3 n = flat.direction;
        const double along = dot(sub(other.centre, face.point), n) / dot(n, n);
        out.centreToFace = std::abs(along) * norm(n);
        const Vec3 foot{other.centre[0] - n[0] * along, other.centre[1] - n[1] * along,
                        other.centre[2] - n[2] * along};
        out.centreFrom = k == 0 ? foot : other.centre;
        out.centreTo = k == 0 ? other.centre : foot;
    }
    // Centres: points (circles, spheres) and axes (cylinders, cones).
    if (da.hasCentre && db.hasCentre) {
        const bool axisA = isAxis(da.kind), axisB = isAxis(db.kind);
        if (!axisA && !axisB) {
            out.centres = norm(sub(db.centre, da.centre));
            out.centreFrom = da.centre;
            out.centreTo = db.centre;
        } else if (axisA != axisB) {
            const About& axis = axisA ? da : db;
            const About& point = axisA ? db : da;
            out.centres = fromLine(point.centre, axis.centre, axis.direction);
            // The foot of the point on the axis.
            const Vec3 d = axis.direction;
            const double t = dot(sub(point.centre, axis.centre), d) / std::max(dot(d, d), 1e-300);
            const Vec3 foot{axis.centre[0] + d[0] * t, axis.centre[1] + d[1] * t, axis.centre[2] + d[2] * t};
            out.centreFrom = axisA ? foot : point.centre;
            out.centreTo = axisA ? point.centre : foot;
        } else if (acuteAngle(da.direction, db.direction) < 0.01) {
            // Two parallel axes (two holes, two pegs): between the axes, across them.
            out.centres = fromLine(db.centre, da.centre, da.direction);
            const Vec3 d = da.direction;
            const double t = dot(sub(db.centre, da.centre), d) / std::max(dot(d, d), 1e-300);
            out.centreFrom = {da.centre[0] + d[0] * t, da.centre[1] + d[1] * t, da.centre[2] + d[2] * t};
            out.centreTo = db.centre;
        }
    }
    return out;
}

}  // namespace forge::measure
