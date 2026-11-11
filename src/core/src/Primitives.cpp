// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Primitives.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
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

TopoDS_Shape makeSphere(double radius)
{
    requirePositive(radius, "radius");
    return BRepPrimAPI_MakeSphere(gp_Pnt(0.0, 0.0, radius), radius).Shape();
}

TopoDS_Shape makeCone(double radiusBottom, double radiusTop, double height)
{
    requirePositive(height, "height");
    if (radiusBottom < 0.0 || radiusTop < 0.0 || (radiusBottom == 0.0 && radiusTop == 0.0))
        throw std::invalid_argument("cone radii must not be negative, and not both zero");
    if (radiusBottom == radiusTop)
        return makeCylinder(radiusBottom, height);  // OpenCASCADE refuses a "cone" with equal radii
    const gp_Ax2 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    return BRepPrimAPI_MakeCone(axis, radiusBottom, radiusTop, height).Shape();
}

TopoDS_Shape makeTorus(double majorRadius, double minorRadius)
{
    requirePositive(majorRadius, "major radius");
    requirePositive(minorRadius, "minor radius");
    if (!(minorRadius < majorRadius))
        throw std::invalid_argument("torus minor radius must be less than the major radius");
    const gp_Ax2 axis(gp_Pnt(0.0, 0.0, minorRadius), gp_Dir(0.0, 0.0, 1.0));
    return BRepPrimAPI_MakeTorus(axis, majorRadius, minorRadius).Shape();
}

TopoDS_Shape makeTube(double outerRadius, double innerRadius, double height)
{
    requirePositive(outerRadius, "outer radius");
    requirePositive(innerRadius, "inner radius");
    requirePositive(height, "height");
    if (!(innerRadius < outerRadius))
        throw std::invalid_argument("tube inner radius must be less than the outer radius");
    // A ring-shaped face (outer circle with an inner hole), extruded up.
    const gp_Ax2 frame(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Wire outer = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame, outerRadius)).Edge()).Wire();
    TopoDS_Wire inner = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame, innerRadius)).Edge()).Wire();
    inner.Reverse();  // a hole runs the other way round
    BRepBuilderAPI_MakeFace face(outer);
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
                return makeSphere(p.radius);
            else if constexpr (std::is_same_v<T, ConeSpec>)
                return makeCone(p.radiusBottom, p.radiusTop, p.height);
            else if constexpr (std::is_same_v<T, TorusSpec>)
                return makeTorus(p.majorRadius, p.minorRadius);
            else if constexpr (std::is_same_v<T, TubeSpec>)
                return makeTube(p.outerRadius, p.innerRadius, p.height);
            else {
                static_assert(std::is_same_v<T, WedgeSpec>, "buildShape: unhandled primitive");
                return makeWedge(p.width, p.depth, p.height);
            }
        },
        spec);
}

}  // namespace forge::primitives
