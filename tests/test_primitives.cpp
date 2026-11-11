// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Primitives.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <numbers>
#include <stdexcept>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
int countFaces(const TopoDS_Shape& shape)
{
    int n = 0;
    for (TopExp_Explorer e(shape, TopAbs_FACE); e.More(); e.Next())
        ++n;
    return n;
}

double volumeOf(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}
}  // namespace

TEST_CASE("makeBox builds a valid solid with the requested size", "[primitives]")
{
    const TopoDS_Shape box = forge::primitives::makeBox(40.0, 30.0, 20.0);

    REQUIRE_FALSE(box.IsNull());
    CHECK(BRepCheck_Analyzer(box).IsValid());
    CHECK(countFaces(box) == 6);
    CHECK_THAT(volumeOf(box), WithinRel(40.0 * 30.0 * 20.0, 1e-9));
}

TEST_CASE("makeCylinder builds a valid solid with the requested size", "[primitives]")
{
    const TopoDS_Shape cyl = forge::primitives::makeCylinder(10.0, 25.0);

    REQUIRE_FALSE(cyl.IsNull());
    CHECK(BRepCheck_Analyzer(cyl).IsValid());
    CHECK(countFaces(cyl) == 3);  // side, top, bottom
    CHECK_THAT(volumeOf(cyl), WithinRel(std::numbers::pi * 10.0 * 10.0 * 25.0, 1e-6));
}

TEST_CASE("primitives reject zero and negative sizes", "[primitives]")
{
    CHECK_THROWS_AS(forge::primitives::makeBox(0.0, 1.0, 1.0), std::invalid_argument);
    CHECK_THROWS_AS(forge::primitives::makeBox(1.0, -2.0, 1.0), std::invalid_argument);
    CHECK_THROWS_AS(forge::primitives::makeCylinder(-1.0, 1.0), std::invalid_argument);
    CHECK_THROWS_AS(forge::primitives::makeCylinder(1.0, 0.0), std::invalid_argument);
}
