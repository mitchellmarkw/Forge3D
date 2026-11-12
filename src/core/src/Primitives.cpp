// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Primitives.h"

#include "forge/core/SketchShape.h"
#include "forge/core/Text.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pln.hxx>
#include <map>
#include <utility>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <gp_Ax1.hxx>
#include <gp_GTrsf.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Elips.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <numbers>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace forge::primitives {

namespace {
void requirePositive(double value, const char* name)
{
    if (!(value > 0.0))
        throw std::invalid_argument(std::string(name) + " must be greater than zero");
}
}  // namespace

TopoDS_Shape makeBox(double width, double depth, double height)
{
    requirePositive(width, "width");
    requirePositive(depth, "depth");
    requirePositive(height, "height");

    const gp_Pnt corner(-width / 2.0, -depth / 2.0, 0.0);
    BRepPrimAPI_MakeBox maker(corner, width, depth, height);
    return maker.Shape();
}

TopoDS_Shape makeCylinder(double radius, double height)
{
    requirePositive(radius, "radius");
    requirePositive(height, "height");

    const gp_Ax2 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    BRepPrimAPI_MakeCylinder maker(axis, radius, height);
    return maker.Shape();
}

TopoDS_Shape makeCylinder(double radiusX, double radiusY, double height)
{
    requirePositive(radiusX, "radiusX");
    requirePositive(radiusY, "radiusY");
    requirePositive(height, "height");
    if (radiusX == radiusY)
        return makeCylinder(radiusX, height);  // keep a true circular cylinder

    // An ellipse in the XY plane, extruded up. gp_Elips needs major >= minor, with
    // the major radius along the frame's X direction.
    const bool alongX = radiusX > radiusY;
    const gp_Ax2 frame(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0),
                       alongX ? gp_Dir(1.0, 0.0, 0.0) : gp_Dir(0.0, 1.0, 0.0));
    const gp_Elips ellipse(frame, alongX ? radiusX : radiusY, alongX ? radiusY : radiusX);

    BRepBuilderAPI_MakeEdge edge(ellipse);
    BRepBuilderAPI_MakeWire wire(edge.Edge());
    BRepBuilderAPI_MakeFace face(wire.Wire());
    if (!face.IsDone())
        throw std::runtime_error("Could not build the elliptical cylinder's base");
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
    prism.Build();
    if (!prism.IsDone())
        throw std::runtime_error("Could not extrude the elliptical cylinder");
    return prism.Shape();
}

namespace {

// A closed ellipse (circle when rx == ry) in the plane Z = z, centred on the Z axis,
// starting at +X and running anticlockwise seen from above. Every ellipse built
// here starts at the same angle, so lofts between them don't twist.
TopoDS_Edge ellipseEdge(double z, double rx, double ry)
{
    const gp_Pnt centre(0.0, 0.0, z);
    if (rx == ry)
        return BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(centre, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)), rx)).Edge();
    if (rx > ry)
        return BRepBuilderAPI_MakeEdge(gp_Elips(gp_Ax2(centre, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)), rx, ry)).Edge();
    // gp_Elips needs its major radius along the frame's X direction, so this one's
    // frame points along +Y. Start a quarter turn earlier, at +X, to match the others.
    const gp_Elips e(gp_Ax2(centre, gp_Dir(0, 0, 1), gp_Dir(0, 1, 0)), ry, rx);
    const double start = -std::numbers::pi / 2.0;
    return BRepBuilderAPI_MakeEdge(e, start, start + 2.0 * std::numbers::pi).Edge();
}

TopoDS_Wire ellipseWire(double z, double rx, double ry)
{
    return BRepBuilderAPI_MakeWire(ellipseEdge(z, rx, ry)).Wire();
}

TopoDS_Shape requireSolid(const TopoDS_Shape& shape, const char* what)
{
    if (shape.IsNull())
        throw std::runtime_error(std::string("Could not build the ") + what);
    return shape;
}

}  // namespace

TopoDS_Shape makeSphere(double radius)
{
    requirePositive(radius, "radius");
    return BRepPrimAPI_MakeSphere(gp_Pnt(0.0, 0.0, radius), radius).Shape();
}

TopoDS_Shape makeSphere(double radiusX, double radiusY, double radiusZ)
{
    requirePositive(radiusX, "radiusX");
    requirePositive(radiusY, "radiusY");
    requirePositive(radiusZ, "radiusZ");
    if (radiusX == radiusY && radiusY == radiusZ)
        return makeSphere(radiusX);
    // A unit-ish sphere stretched along each axis (exact, as a B-spline surface).
    const double r = std::max({radiusX, radiusY, radiusZ});
    const TopoDS_Shape ball = BRepPrimAPI_MakeSphere(gp_Pnt(0.0, 0.0, 0.0), r).Shape();
    gp_GTrsf stretch;
    stretch.SetValue(1, 1, radiusX / r);
    stretch.SetValue(2, 2, radiusY / r);
    stretch.SetValue(3, 3, radiusZ / r);
    stretch.SetValue(3, 4, radiusZ);  // lift it onto the plate
    BRepBuilderAPI_GTransform maker(ball, stretch, /*copy=*/true);
    if (!maker.IsDone())
        throw std::runtime_error("Could not build the ellipsoid");
    return maker.Shape();
}

TopoDS_Shape makeCone(double radiusBottom, double radiusTop, double height)
{
    return makeCone(radiusBottom, radiusBottom, radiusTop, radiusTop, height);
}

TopoDS_Shape makeCone(double bottomX, double bottomY, double topX, double topY, double height)
{
    requirePositive(height, "height");
    const auto endOk = [](double x, double y) { return x >= 0.0 && y >= 0.0 && (x == 0.0) == (y == 0.0); };
    if (!endOk(bottomX, bottomY) || !endOk(topX, topY) || (bottomX == 0.0 && topX == 0.0))
        throw std::invalid_argument("cone ends need both radii above 0, or both 0 (a point), and not two points");
    const bool round = bottomX == bottomY && topX == topY;
    if (round && bottomX == topX)
        return makeCylinder(bottomX, height);  // OpenCASCADE refuses a "cone" with equal radii
    if (round) {
        const gp_Ax2 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
        return BRepPrimAPI_MakeCone(axis, bottomX, topX, height).Shape();
    }
    // Oval: straight lines from the bottom ellipse to the top one (or to a point).
    BRepOffsetAPI_ThruSections loft(/*isSolid=*/true, /*ruled=*/true);
    if (bottomX == 0.0)
        loft.AddVertex(BRepBuilderAPI_MakeVertex(gp_Pnt(0.0, 0.0, 0.0)).Vertex());
    else
        loft.AddWire(ellipseWire(0.0, bottomX, bottomY));
    if (topX == 0.0)
        loft.AddVertex(BRepBuilderAPI_MakeVertex(gp_Pnt(0.0, 0.0, height)).Vertex());
    else
        loft.AddWire(ellipseWire(height, topX, topY));
    loft.CheckCompatibility(false);
    loft.Build();
    if (!loft.IsDone())
        throw std::runtime_error("Could not build the oval cone");
    return requireSolid(loft.Shape(), "oval cone");
}

TopoDS_Shape makeTorus(double majorRadius, double minorRadius)
{
    return makeTorus(majorRadius, majorRadius, minorRadius, minorRadius);
}

TopoDS_Shape makeTorus(double ringX, double ringY, double tube, double tubeZ)
{
    requirePositive(ringX, "ring radius X");
    requirePositive(ringY, "ring radius Y");
    requirePositive(tube, "tube radius");
    requirePositive(tubeZ, "tube radius Z");
    const double small = std::min(ringX, ringY), big = std::max(ringX, ringY);
    if (!(tube < small * small / big))
        throw std::invalid_argument("torus tube radius too big for the ring (no hole left)");

    if (ringX == ringY && tube == tubeZ)
        return BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0.0, 0.0, tube), gp_Dir(0.0, 0.0, 1.0)), ringX, tube).Shape();

    // The tube's cross-section: an ellipse in the XZ plane, centred on the ring at +X.
    const gp_Pnt c(ringX, 0.0, tubeZ);
    const gp_Dir normal(0, -1, 0);  // the XZ plane
    TopoDS_Edge profileEdge;
    if (tube == tubeZ)
        profileEdge = BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(c, normal, gp_Dir(1, 0, 0)), tube)).Edge();
    else if (tube > tubeZ)
        profileEdge = BRepBuilderAPI_MakeEdge(gp_Elips(gp_Ax2(c, normal, gp_Dir(1, 0, 0)), tube, tubeZ)).Edge();
    else
        profileEdge = BRepBuilderAPI_MakeEdge(gp_Elips(gp_Ax2(c, normal, gp_Dir(0, 0, 1)), tubeZ, tube)).Edge();
    BRepBuilderAPI_MakeFace profile(BRepBuilderAPI_MakeWire(profileEdge).Wire(), /*OnlyPlane=*/true);
    if (!profile.IsDone())
        throw std::runtime_error("Could not build the torus cross-section");

    if (ringX == ringY) {
        // Round ring: spin the cross-section around Z (exact surface of revolution).
        BRepPrimAPI_MakeRevol revol(profile.Face(), gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
        revol.Build();
        if (!revol.IsDone())
            throw std::runtime_error("Could not build the torus");
        return requireSolid(revol.Shape(), "torus");
    }
    // Oval ring: sweep the cross-section along the oval centre line.
    BRepOffsetAPI_MakePipe pipe(ellipseWire(tubeZ, ringX, ringY), profile.Face());
    pipe.Build();
    if (!pipe.IsDone())
        throw std::runtime_error("Could not build the oval torus");
    return requireSolid(pipe.Shape(), "oval torus");
}

TopoDS_Shape makeTube(double outerRadius, double innerRadius, double height)
{
    return makeTube(outerRadius, outerRadius, innerRadius, innerRadius, height);
}

TopoDS_Shape makeTube(double outerX, double outerY, double innerX, double innerY, double height)
{
    requirePositive(outerX, "outer radius X");
    requirePositive(outerY, "outer radius Y");
    requirePositive(innerX, "inner radius X");
    requirePositive(innerY, "inner radius Y");
    requirePositive(height, "height");
    if (!(innerX < outerX && innerY < outerY))
        throw std::invalid_argument("tube inner radii must be less than the outer radii");
    // A ring-shaped face (outer ellipse with an inner hole), extruded up.
    TopoDS_Wire inner = ellipseWire(0.0, innerX, innerY);
    inner.Reverse();  // a hole runs the other way round
    BRepBuilderAPI_MakeFace face(ellipseWire(0.0, outerX, outerY));
    face.Add(inner);
    if (!face.IsDone())
        throw std::runtime_error("Could not build the tube's base");
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, 0.0, height));
    prism.Build();
    if (!prism.IsDone())
        throw std::runtime_error("Could not extrude the tube");
    return prism.Shape();
}

TopoDS_Shape makeWedge(double width, double depth, double height)
{
    requirePositive(width, "width");
    requirePositive(depth, "depth");
    requirePositive(height, "height");
    // Right triangle in the XZ plane at the back (Y = -depth/2), extruded towards +Y.
    const double x0 = -width / 2.0, x1 = width / 2.0, y = -depth / 2.0;
    BRepBuilderAPI_MakePolygon triangle(gp_Pnt(x0, y, 0.0), gp_Pnt(x1, y, 0.0), gp_Pnt(x0, y, height),
                                        /*Close=*/true);
    BRepBuilderAPI_MakeFace face(triangle.Wire(), /*OnlyPlane=*/true);
    if (!face.IsDone())
        throw std::runtime_error("Could not build the wedge's side");
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0.0, depth, 0.0));
    prism.Build();
    if (!prism.IsDone())
        throw std::runtime_error("Could not extrude the wedge");
    return prism.Shape();
}

TopoDS_Shape makePyramid(double baseWidth, double baseDepth, double topWidth, double topDepth, double height)
{
    requirePositive(baseWidth, "base width");
    requirePositive(baseDepth, "base depth");
    requirePositive(height, "height");
    if (topWidth < 0.0 || topDepth < 0.0)
        throw std::invalid_argument("pyramid top sizes must not be negative");

    // Corners anticlockwise from (-x, -y), at the base and at the top. Coinciding top
    // corners (a point or a ridge) are fine: faces drop repeated corners.
    const auto corners = [](double w, double d, double z) {
        return std::array<gp_Pnt, 4>{gp_Pnt(-w / 2, -d / 2, z), gp_Pnt(w / 2, -d / 2, z), gp_Pnt(w / 2, d / 2, z),
                                     gp_Pnt(-w / 2, d / 2, z)};
    };
    const auto base = corners(baseWidth, baseDepth, 0.0);
    const auto top = corners(topWidth, topDepth, height);

    BRepBuilderAPI_Sewing sewing(1e-6);
    const auto addFace = [&sewing](std::initializer_list<gp_Pnt> pts) {
        BRepBuilderAPI_MakePolygon poly;
        gp_Pnt last;
        bool any = false;
        int count = 0;
        for (const gp_Pnt& p : pts) {
            if (any && p.Distance(last) < 1e-9)
                continue;
            poly.Add(p);
            last = p;
            any = true;
            ++count;
        }
        if (count < 3)
            return;  // the face collapsed to a line or a point
        poly.Close();
        BRepBuilderAPI_MakeFace face(poly.Wire(), /*OnlyPlane=*/true);
        if (face.IsDone())
            sewing.Add(face.Face());
    };
    addFace({base[3], base[2], base[1], base[0]});  // bottom, facing down
    if (topWidth > 0.0 && topDepth > 0.0)
        addFace({top[0], top[1], top[2], top[3]});  // flat top, facing up
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) % 4;
        addFace({base[i], base[j], top[j], top[i]});
    }
    sewing.Perform();
    const TopoDS_Shape shell = sewing.SewedShape();
    TopExp_Explorer it(shell, TopAbs_SHELL);
    if (!it.More())
        throw std::runtime_error("Could not build the pyramid");
    BRepBuilderAPI_MakeSolid solid(TopoDS::Shell(it.Current()));
    if (!solid.IsDone())
        throw std::runtime_error("Could not build the pyramid");
    // Make sure the solid's inside is inside (outward-facing faces).
    TopoDS_Shape result = solid.Solid();
    BRepClass3d_SolidClassifier inside(result, gp_Pnt(0.0, 0.0, height * 1e-3), 1e-7);
    if (inside.State() == TopAbs_OUT)
        result.Reverse();
    return result;
}

TopoDS_Shape makeMeshSolid(const MeshData& data, const std::array<double, 3>& scale)
{
    if (data.indices.empty())
        throw std::invalid_argument("mesh has no triangles");
    // Build the faces sharing vertices and edges directly (no sewing): one plane
    // face per triangle, one straight edge per pair of neighbouring points.
    BRep_Builder builder;
    const std::size_t pointCount = data.positions.size() / 3;
    std::vector<TopoDS_Vertex> vertices(pointCount);
    std::vector<gp_Pnt> points(pointCount);
    for (std::size_t i = 0; i < pointCount; ++i) {
        points[i] = gp_Pnt(data.positions[i * 3] * scale[0], data.positions[i * 3 + 1] * scale[1],
                           data.positions[i * 3 + 2] * scale[2]);
        builder.MakeVertex(vertices[i], points[i], 1e-7);
    }
    std::map<std::pair<std::uint32_t, std::uint32_t>, TopoDS_Edge> edges;
    const auto edgeBetween = [&](std::uint32_t a, std::uint32_t b) {
        const auto key = std::minmax(a, b);
        auto it = edges.find({key.first, key.second});
        if (it == edges.end()) {
            const TopoDS_Edge e = BRepBuilderAPI_MakeEdge(vertices[key.first], vertices[key.second]).Edge();
            it = edges.emplace(std::pair{key.first, key.second}, e).first;
        }
        // The edge runs from the smaller index to the larger: reverse it if going the other way.
        return a < b ? it->second : TopoDS::Edge(it->second.Reversed());
    };

    TopoDS_Shell shell;
    builder.MakeShell(shell);
    for (std::size_t t = 0; t + 2 < data.indices.size(); t += 3) {
        const std::uint32_t a = data.indices[t], b = data.indices[t + 1], c = data.indices[t + 2];
        const gp_Vec ab(points[a], points[b]), ac(points[a], points[c]);
        const gp_Vec normal = ab.Crossed(ac);
        if (normal.Magnitude() < 1e-12)
            continue;  // a sliver with no area
        TopoDS_Wire wire;
        builder.MakeWire(wire);
        builder.Add(wire, edgeBetween(a, b));
        builder.Add(wire, edgeBetween(b, c));
        builder.Add(wire, edgeBetween(c, a));
        const gp_Pln plane(points[a], gp_Dir(normal));
        BRepBuilderAPI_MakeFace face(plane, wire, /*Inside=*/true);
        if (face.IsDone())
            builder.Add(shell, face.Face());
    }
    shell.Closed(data.closed);
    if (!data.closed)
        return shell;  // an open surface: it can be shown and exported, not combined
    TopoDS_Solid solid;
    builder.MakeSolid(solid);
    builder.Add(solid, shell);
    return solid;
}

TopoDS_Shape buildShape(const PrimitiveSpec& spec)
{
    return std::visit(
        [](const auto& p) -> TopoDS_Shape {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
                return makeBox(p.width, p.depth, p.height);
            else if constexpr (std::is_same_v<T, CylinderSpec>)
                return makeCylinder(p.radiusX, p.radiusY, p.height);
            else if constexpr (std::is_same_v<T, SphereSpec>)
                return makeSphere(p.radiusX, p.radiusY, p.radiusZ);
            else if constexpr (std::is_same_v<T, ConeSpec>)
                return makeCone(p.bottomRadiusX, p.bottomRadiusY, p.topRadiusX, p.topRadiusY, p.height);
            else if constexpr (std::is_same_v<T, TorusSpec>)
                return makeTorus(p.ringRadiusX, p.ringRadiusY, p.tubeRadius, p.tubeRadiusZ);
            else if constexpr (std::is_same_v<T, TubeSpec>)
                return makeTube(p.outerRadiusX, p.outerRadiusY, p.innerRadiusX, p.innerRadiusY, p.height);
            else if constexpr (std::is_same_v<T, WedgeSpec>)
                return makeWedge(p.width, p.depth, p.height);
            else if constexpr (std::is_same_v<T, TextSpec>)
                return text::makeText(p);
            else if constexpr (std::is_same_v<T, PadSpec>)
                return sketch::padShape(p);
            else if constexpr (std::is_same_v<T, MeshSpec>)
                return makeMeshSolid(*p.data, {p.width / p.data->size[0], p.depth / p.data->size[1],
                                               p.height / p.data->size[2]});
            else {
                static_assert(std::is_same_v<T, PyramidSpec>, "buildShape: unhandled primitive");
                return makePyramid(p.baseWidth, p.baseDepth, p.topWidth, p.topDepth, p.height);
            }
        },
        spec);
}

}  // namespace forge::primitives
