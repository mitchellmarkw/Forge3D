// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Primitives.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
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

TopoDS_Shape buildShape(const PrimitiveSpec& spec)
{
    return std::visit(
        [](const auto& p) -> TopoDS_Shape {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
                return makeBox(p.width, p.depth, p.height);
            else
                return makeCylinder(p.radiusX, p.radiusY, p.height);
        },
        spec);
}

}  // namespace forge::primitives
