// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Shell.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <GProp_GProps.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace forge {

namespace {

struct Box {
    Vec3 min{0, 0, 0}, size{1, 1, 1};
    Vec3 relative(const Vec3& p) const
    {
        Vec3 r{};
        for (std::size_t k = 0; k < 3; ++k)
            r[k] = size[k] > 1e-9 ? (p[k] - min[k]) / size[k] : 0.5;
        return r;
    }
    Vec3 absolute(const Vec3& r) const
    {
        return {min[0] + r[0] * size[0], min[1] + r[1] * size[1], min[2] + r[2] * size[2]};
    }
    double diagonal() const { return std::sqrt(size[0] * size[0] + size[1] * size[1] + size[2] * size[2]); }
};

Box boxOf(const TopoDS_Shape& shape)
{
    Bnd_Box b;
    BRepBndLib::AddOptimal(shape, b, false, false);
    Box out;
    if (b.IsVoid())
        return out;
    double x0, y0, z0, x1, y1, z1;
    b.Get(x0, y0, z0, x1, y1, z1);
    out.min = {x0, y0, z0};
    out.size = {x1 - x0, y1 - y0, z1 - z0};
    return out;
}

struct Near {
    double distance = 1e300;
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 0, 1};
};

// How far `p` is from the face, the nearest point on it and which way the face points
// there (out of the solid).
Near nearOnFace(const TopoDS_Face& face, const Vec3& p)
{
    Near out;
    const TopoDS_Vertex v = BRepBuilderAPI_MakeVertex(gp_Pnt(p[0], p[1], p[2])).Vertex();
    BRepExtrema_DistShapeShape dist(v, face);
    if (!dist.IsDone() || dist.NbSolution() == 0)
        return out;
    out.distance = dist.Value();
    const gp_Pnt q = dist.PointOnShape2(1);
    out.point = {q.X(), q.Y(), q.Z()};
    double u = 0, w = 0;
    if (dist.SupportTypeShape2(1) == BRepExtrema_IsInFace) {
        dist.ParOnFaceS2(1, u, w);
    } else {
        // On an edge or corner of the face: use the middle of its parameters instead.
        BRepGProp_Face props(face);
        double u0, u1, w0, w1;
        props.Bounds(u0, u1, w0, w1);
        u = (u0 + u1) / 2;
        w = (w0 + w1) / 2;
    }
    BRepGProp_Face props(face);
    gp_Pnt at;
    gp_Vec n;
    props.Normal(u, w, at, n);  // already flipped for reversed faces
    if (n.Magnitude() > 1e-12) {
        n.Normalize();
        out.normal = {n.X(), n.Y(), n.Z()};
    }
    return out;
}

double dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

TopoDS_Shape onlySolid(const TopoDS_Shape& shape)
{
    TopoDS_Shape found;
    int n = 0;
    for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) {
        found = it.Current();
        ++n;
    }
    if (n == 0)
        throw GeometryError("Shell works on solid shapes");
    if (n > 1)
        throw GeometryError("Shell works on one solid at a time (this shape is several pieces)");
    return found;
}

gp_Trsf toTrsf(const Transform& t)
{
    gp_Trsf trsf;
    trsf.SetValues(t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3],  //
                   t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3],  //
                   t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);
    return trsf;
}

FaceRef moved(const FaceRef& r, const Transform& t)
{
    FaceRef out = r;
    out.point = t.apply(r.point);
    out.normal = t.applyDirection(r.normal);
    const double len = std::sqrt(dot(out.normal, out.normal));
    if (len > 1e-12)
        for (double& v : out.normal)
            v /= len;
    return out;
}

const Body& shellResult(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Shell || !body->boolean->shell)
        throw std::invalid_argument("Not a shelled shape");
    return *body;
}

TopoDS_Shape placedPart(const Document& doc, ObjectId part)
{
    return BRepBuilderAPI_Transform(doc.shape(part), toTrsf(doc.localTransform(part)), true).Shape();
}

}  // namespace

namespace shell {

FaceRef refFor(const TopoDS_Shape& shape, int face, const Vec3& at)
{
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    if (face < 0 || face >= faces.Extent())
        throw std::out_of_range("No such face");
    const Near n = nearOnFace(TopoDS::Face(faces.FindKey(face + 1)), at);
    FaceRef ref;
    ref.point = n.point;
    ref.normal = n.normal;
    return ref;
}

void locate(const TopoDS_Shape& part, std::vector<FaceRef>& refs)
{
    const Box box = boxOf(part);
    for (FaceRef& r : refs) {
        r.relative = box.relative(r.point);
        r.located = true;
    }
}

std::vector<int> match(const TopoDS_Shape& shape, const std::vector<FaceRef>& refs)
{
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    const Box box = boxOf(shape);
    const double tolerance = std::max(box.diagonal() * 2e-3, 1e-3);
    std::vector<int> out(refs.size(), -1);
    std::vector<bool> taken(std::size_t(faces.Extent()), false);
    const auto best = [&](const Vec3& p, const Vec3& normal, double within, double facing) {
        int found = -1;
        double bestD = within;
        for (int f = 0; f < faces.Extent(); ++f) {
            if (taken[std::size_t(f)])
                continue;
            const Near n = nearOnFace(TopoDS::Face(faces.FindKey(f + 1)), p);
            if (n.distance < bestD && dot(n.normal, normal) >= facing) {
                bestD = n.distance;
                found = f;
            }
        }
        return found;
    };
    // 1. Still where it was, facing the same way.
    for (std::size_t i = 0; i < refs.size(); ++i)
        if ((out[i] = best(refs[i].point, refs[i].normal, tolerance, 0.9)) >= 0)
            taken[std::size_t(out[i])] = true;
    // 2. At the same spot in the shape's box (it was resized).
    for (std::size_t i = 0; i < refs.size(); ++i)
        if (out[i] < 0 && refs[i].located)
            if ((out[i] = best(box.absolute(refs[i].relative), refs[i].normal, box.diagonal() * 0.1, 0.9)) >= 0)
                taken[std::size_t(out[i])] = true;
    // 3. The nearest face facing about the same way.
    for (std::size_t i = 0; i < refs.size(); ++i)
        if (out[i] < 0)
            if ((out[i] = best(refs[i].point, refs[i].normal, box.diagonal() * 0.25, 0.5)) >= 0)
                taken[std::size_t(out[i])] = true;
    return out;
}

TopoDS_Shape apply(const TopoDS_Shape& shape, const ShellFinish& finish)
{
    if (!(finish.thickness > 0.0))
        throw std::invalid_argument("The wall thickness must be greater than zero");
    const TopoDS_Shape solid = onlySolid(shape);
    const Box box = boxOf(solid);
    const double smallest = std::min({box.size[0], box.size[1], box.size[2]});
    if (!finish.outward && 2.0 * finish.thickness >= smallest)
        throw GeometryError("Walls that thick would fill the whole shape. Try thinner walls.");
    const double tol = 1e-4;
    TopoDS_Shape result;
    try {
        if (finish.open.empty()) {
            // Sealed: the solid minus a copy of itself grown inwards (or the solid grown
            // outwards, minus the solid).
            BRepOffsetAPI_MakeOffsetShape offset;
            offset.PerformByJoin(solid, finish.outward ? finish.thickness : -finish.thickness, tol);
            if (!offset.IsDone())
                throw GeometryError("The walls can't be made this thick here. Try thinner walls.");
            const TopoDS_Shape other = offset.Shape();
            BRepAlgoAPI_Cut cut(finish.outward ? other : solid, finish.outward ? solid : other);
            if (!cut.IsDone() || cut.HasErrors())
                throw GeometryError("The walls can't be made this thick here. Try thinner walls.");
            result = cut.Shape();
        } else {
            TopTools_IndexedMapOfShape faces;
            TopExp::MapShapes(solid, TopAbs_FACE, faces);
            const std::vector<int> matched = match(solid, finish.open);
            TopTools_ListOfShape open;
            for (std::size_t i = 0; i < matched.size(); ++i) {
                if (matched[i] < 0)
                    throw GeometryError("A face picked to be left open is no longer there (the shape under it "
                                        "changed). Pick the faces again.");
                open.Append(faces.FindKey(matched[i] + 1));
            }
            BRepOffsetAPI_MakeThickSolid thick;
            thick.MakeThickSolidByJoin(solid, open, finish.outward ? finish.thickness : -finish.thickness, tol,
                                       BRepOffset_Skin, Standard_False, Standard_False, GeomAbs_Intersection);
            if (!thick.IsDone())
                throw GeometryError("The walls can't be made this thick here. Try thinner walls.");
            result = thick.Shape();
        }
    } catch (const Standard_Failure&) {
        throw GeometryError("The walls can't be made this thick here. Try thinner walls.");
    }
    if (result.IsNull() || !BRepCheck_Analyzer(result).IsValid())
        throw GeometryError("The walls can't be made this thick here. Try thinner walls.");
    // A sound result: some solid left, less than before (walls inside) and inside the
    // old box, or around it (walls outside).
    const Box after = boxOf(result);
    const auto volume = [](const TopoDS_Shape& s) {
        GProp_GProps props;
        BRepGProp::VolumeProperties(s, props);
        return props.Mass();
    };
    const double before = volume(solid), left = volume(result);
    const double slack = box.diagonal() * 1e-3;
    bool sound = left > before * 1e-6 && after.diagonal() >= box.diagonal() * 0.5;
    if (!finish.outward) {
        sound = sound && left < before;
        for (std::size_t k = 0; k < 3; ++k)
            sound = sound && after.min[k] >= box.min[k] - slack &&
                    after.min[k] + after.size[k] <= box.min[k] + box.size[k] + slack;
    }
    if (!sound)
        throw GeometryError("The walls can't be made this thick here. Try thinner walls.");
    ShapeUpgrade_UnifySameDomain unify(result, true, true, false);
    unify.Build();
    return unify.Shape();
}

}  // namespace shell

BooleanSpec shellSpec(const Document& doc, ObjectId target, const ShellFinish& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (doc.usesMeshEngine(target))
        throw std::invalid_argument("Shell works on solid shapes, not on imported meshes or shapes made with them.");
    BooleanSpec spec;
    spec.op = BooleanOp::Shell;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    ShellFinish local = inPlate;
    for (FaceRef& r : local.open)
        r = moved(r, toStep);
    shell::locate(placedPart(doc, target), local.open);
    spec.shell = local;
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

std::vector<int> pickedFaces(const Document& doc, ObjectId result)
{
    const Body& body = shellResult(doc, result);
    return shell::match(placedPart(doc, body.boolean->operands.front()), body.boolean->shell->open);
}

ShellFinish shellInPlate(const Document& doc, ObjectId result)
{
    const Body& body = shellResult(doc, result);
    ShellFinish f = *body.boolean->shell;
    const Transform t = doc.worldTransform(result);
    for (FaceRef& r : f.open)
        r = moved(r, t);
    return f;
}

Body withShell(const Document& doc, ObjectId result, const ShellFinish& inPlate)
{
    Body next = shellResult(doc, result);
    ShellFinish f = inPlate;
    const Transform t = doc.worldTransform(result).inverse();
    for (FaceRef& r : f.open)
        r = moved(r, t);
    shell::locate(placedPart(doc, next.boolean->operands.front()), f.open);
    next.boolean->shell = f;
    return next;
}

}  // namespace forge
