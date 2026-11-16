// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 27: lofts and sweeps.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/Primitives.h"
#include "forge/core/Scale.h"
#include "forge/core/Sketch.h"
#include "forge/core/SketchShape.h"

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge;
using namespace forge::sketch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double pi = std::numbers::pi;

int line(Sketch& s, int a, int b)
{
    s.geos.push_back({GeoKind::Line, a, b, -1, -1, false});
    return int(s.geos.size()) - 1;
}

Sketch square(double side, double cx = 0, double cy = 0)
{
    Sketch s;
    const double h = side / 2;
    const int a = addPoint(s, {cx - h, cy - h}), b = addPoint(s, {cx + h, cy - h}), c = addPoint(s, {cx + h, cy + h}),
              d = addPoint(s, {cx - h, cy + h});
    line(s, a, b);
    line(s, b, c);
    line(s, c, d);
    line(s, d, a);
    return s;
}

Sketch circle(double r, double cx = 0, double cy = 0)
{
    Sketch s;
    const int centre = addPoint(s, {cx, cy});
    s.radii.push_back(r);
    s.geos.push_back({GeoKind::Circle, centre, -1, -1, 0, false});
    return s;
}

double volumeOf(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

Bnd_Box boxOf(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    return box;
}

}  // namespace

TEST_CASE("Loft: blends between outlines stacked up", "[loft]")
{
    LoftSpec loft;
    loft.sections = {{square(20), 0.0}, {circle(10), 30.0}};
    const TopoDS_Shape smooth = primitives::buildShape(loft);
    CHECK(BRepCheck_Analyzer(smooth).IsValid());
    const double v = volumeOf(smooth);
    // Between a 20 mm square and a 10 mm-radius circle, 30 high: between the prism of
    // the circle and of the square.
    CHECK(v > pi * 100 * 30 * 0.95);
    CHECK(v < 400 * 30);
    double x0, y0, z0, x1, y1, z1;
    boxOf(smooth).Get(x0, y0, z0, x1, y1, z1);
    CHECK_THAT(z0, WithinAbs(0.0, 1e-6));
    CHECK_THAT(z1, WithinAbs(30.0, 1e-6));

    // Three outlines, straight sides: a square tower narrowing.
    LoftSpec tower;
    tower.ruled = true;
    tower.sections = {{square(20), 0.0}, {square(10), 10.0}, {square(10), 20.0}};
    const double t = volumeOf(primitives::buildShape(tower));
    // A frustum (20 to 10, 10 high) and a 10 mm cube column.
    CHECK_THAT(t, WithinRel(10.0 / 3.0 * (400 + 100 + 200) + 1000.0, 1e-6));

    // Not enough, out of order, or two shapes in one outline: told so.
    LoftSpec one;
    one.sections = {{square(20), 0.0}};
    CHECK_THROWS_AS(primitives::buildShape(one), GeometryError);
    LoftSpec down;
    down.sections = {{square(20), 10.0}, {square(10), 0.0}};
    CHECK_THROWS_AS(primitives::buildShape(down), GeometryError);
    Sketch two = square(4, -10);
    {
        const Sketch other = square(4, 10);
        const int base = int(two.points.size());
        for (std::size_t i = 1; i < other.points.size(); ++i)
            two.points.push_back(other.points[i]);
        for (const Geo& g : other.geos)
            two.geos.push_back({g.kind, g.a + base - 1, g.b + base - 1, -1, -1, false});
    }
    LoftSpec split;
    split.sections = {{square(20), 0.0}, {two, 10.0}};
    CHECK_THROWS_AS(primitives::buildShape(split), GeometryError);
}

TEST_CASE("Sweep: along a drawn path, and round a spiral", "[loft]")
{
    SECTION("A pipe up and over: the outline swept along an L-shaped path")
    {
        SweepSpec sweep;
        sweep.profile = circle(2);
        Sketch& p = sweep.path;
        const int a = addPoint(p, {0, 0}), b = addPoint(p, {0, 20}), c = addPoint(p, {15, 20});
        line(p, a, b);
        line(p, b, c);
        const TopoDS_Shape pipe = primitives::buildShape(sweep);
        CHECK(BRepCheck_Analyzer(pipe).IsValid());
        // About the outline's area times the path's length (a little more or less at the bend).
        CHECK_THAT(volumeOf(pipe), WithinRel(pi * 4 * 35, 0.06));
        double x0, y0, z0, x1, y1, z1;
        boxOf(pipe).Get(x0, y0, z0, x1, y1, z1);
        CHECK_THAT(z1, WithinAbs(22.0, 0.1));  // up 20, then the pipe's radius
        CHECK_THAT(x1, WithinAbs(15.0, 0.1));  // and 15 across

        // Drawn from the far end: turned round to start at the outline.
        SweepSpec back = sweep;
        back.path = {};
        const int d = addPoint(back.path, {15, 20}), e = addPoint(back.path, {0, 20}), f = addPoint(back.path, {0, 0});
        line(back.path, d, e);
        line(back.path, e, f);
        CHECK_THAT(volumeOf(primitives::buildShape(back)), WithinRel(volumeOf(pipe), 1e-6));

        // Not starting at the outline: told so.
        SweepSpec away = sweep;
        away.path = {};
        const int g = addPoint(away.path, {5, 5}), h = addPoint(away.path, {5, 25});
        line(away.path, g, h);
        CHECK_THROWS_AS(primitives::buildShape(away), GeometryError);
    }

    SECTION("A spring: a circle 10 from the line, round it three times")
    {
        SweepSpec spring;
        spring.spiral = true;
        spring.profile = circle(1, 10, 0);
        spring.pitch = 4;
        spring.turns = 3;
        const TopoDS_Shape coil = primitives::buildShape(spring);
        CHECK(BRepCheck_Analyzer(coil).IsValid());
        const double wire = 3 * std::hypot(2 * pi * 10, 4.0);
        CHECK_THAT(volumeOf(coil), WithinRel(pi * wire, 0.03));
        double x0, y0, z0, x1, y1, z1;
        boxOf(coil).Get(x0, y0, z0, x1, y1, z1);
        CHECK_THAT(x1, WithinAbs(11.0, 0.1));
        CHECK_THAT(z1 - z0, WithinAbs(22.0, 0.2));  // across the coil, the Z way
        CHECK_THAT(y1 - y0, WithinAbs(3 * 4 + 2, 0.2));  // up the line: 3 turns and the wire

        SweepSpec left = spring;
        left.leftHand = true;
        CHECK_THAT(volumeOf(primitives::buildShape(left)), WithinRel(volumeOf(coil), 1e-3));

        SweepSpec tight = spring;
        tight.pitch = 1.5;  // less than the wire's 2 mm: the turns would touch
        CHECK_THROWS_AS(primitives::buildShape(tight), GeometryError);
        SweepSpec across = spring;
        across.profile = circle(3, 1, 0);  // over the line
        CHECK_THROWS_AS(primitives::buildShape(across), GeometryError);
    }
}

TEST_CASE("Lofts and sweeps as shapes: saved, described, resized", "[loft]")
{
    Document doc;
    CommandStack stack(doc);
    Body loft;
    loft.id = doc.allocateId();
    loft.name = "Loft";
    LoftSpec l;
    l.sections = {{square(20), 0.0}, {circle(10), 30.0}};
    loft.spec = l;
    stack.execute(std::make_unique<AddBodyCommand>(loft));
    Body spring;
    spring.id = doc.allocateId();
    spring.name = "Spring";
    SweepSpec w;
    w.spiral = true;
    w.profile = circle(1, 10, 0);
    w.turns = 2;
    spring.spec = w;
    stack.execute(std::make_unique<AddBodyCommand>(spring));
    CHECK(history::describe(doc, loft.id).find("2 outlines, 30 mm high") != std::string::npos);
    CHECK(history::describe(doc, spring.id).find("spiral, 2 turns") != std::string::npos);

    const auto loaded = file::fromJson(file::toJson(doc, "test"));
    Document other;
    other.replaceAll(loaded);
    CHECK(*other.find(loft.id) == *doc.find(loft.id));
    CHECK(*other.find(spring.id) == *doc.find(spring.id));

    // Resized evenly: everything twice the size; unevenly: not allowed.
    for (const Body& b : scaledTree(doc, loft.id, {2, 2, 2}, {0, 0, 0}))
        if (b.id == loft.id)
            CHECK_THAT(std::get<LoftSpec>(b.spec).sections[1].height, WithinAbs(60.0, 1e-9));
    for (const Body& b : scaledTree(doc, spring.id, {2, 2, 2}, {0, 0, 0}))
        if (b.id == spring.id)
            CHECK_THAT(std::get<SweepSpec>(b.spec).pitch, WithinAbs(10.0, 1e-9));
    CHECK_THROWS(scaledTree(doc, loft.id, {2, 1, 1}, {0, 0, 0}));
}
