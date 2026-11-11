// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Primitives.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <stdexcept>
#include <string>

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

}  // namespace forge::primitives
