// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 7: sketches (the solver, outlines) and pads.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
#include "forge/core/Sketch.h"
#include "forge/core/SketchShape.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <random>

using namespace forge;
using namespace forge::sketch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

constexpr double pi = std::numbers::pi;

int line(Sketch& s, int a, int b)
{
    s.geos.push_back({GeoKind::Line, a, b, -1, -1, false});
    return int(s.geos.size()) - 1;
}

int circle(Sketch& s, Point c, double r)
{
    const int centre = addPoint(s, c);
    s.radii.push_back(r);
    s.geos.push_back({GeoKind::Circle, centre, -1, -1, int(s.radii.size()) - 1, false});
    return int(s.geos.size()) - 1;
}

void rule(Sketch& s, Rule r, int a, int b = -1, int c = -1, double v = 0, double v2 = 0)
{
    s.constraints.push_back({r, a, b, c, v, v2});
}

// A rough rectangle (corners 1..4) with its four sides (geos 0..3), not yet held.
Sketch roughRectangle()
{
    Sketch s;
    const int p1 = addPoint(s, {0.3, -0.2}), p2 = addPoint(s, {19, 0.4}), p3 = addPoint(s, {20.5, 9}),
              p4 = addPoint(s, {-0.4, 10.3});
    line(s, p1, p2);
    line(s, p2, p3);
    line(s, p3, p4);
    line(s, p4, p1);
    return s;
}

double volumeOf(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

}  // namespace

TEST_CASE("The solver squares up a rectangle and counts what's left free", "[sketch]")
{
    Sketch s = roughRectangle();
    CHECK(freedom(s) == 8);  // four free points
    rule(s, Rule::Horizontal, 0);
    rule(s, Rule::Horizontal, 2);
    rule(s, Rule::Vertical, 1);
    rule(s, Rule::Vertical, 3);
    Solved r = solve(s);
    CHECK(r.ok);
    CHECK(r.freedom == 4);  // a rectangle: where it is (2) and its size (2)
    CHECK_THAT(s.points[1].y, WithinAbs(s.points[2].y, 1e-9));
    CHECK_THAT(s.points[2].x, WithinAbs(s.points[3].x, 1e-9));

    rule(s, Rule::Coincident, 0, 1);  // a corner on the origin
    rule(s, Rule::Distance, 0, -1, -1, 30.0);
    rule(s, Rule::Distance, 1, -1, -1, 12.0);
    r = solve(s);
    CHECK(r.ok);
    CHECK(r.freedom == 0);
    CHECK_THAT(s.points[1].x, WithinAbs(0.0, 1e-9));
    CHECK_THAT(std::abs(s.points[2].x - s.points[1].x), WithinAbs(30.0, 1e-7));
    CHECK_THAT(std::abs(s.points[3].y - s.points[2].y), WithinAbs(12.0, 1e-7));

    SECTION("changing a dimension moves things")
    {
        s.constraints.back().value = 5.0;
        CHECK(solve(s).ok);
        CHECK_THAT(std::abs(s.points[3].y - s.points[2].y), WithinAbs(5.0, 1e-7));
    }
    SECTION("rules that fight are found")
    {
        rule(s, Rule::Distance, 1, 3, -1, 100.0);  // a diagonal far too long
        r = solve(s);
        CHECK_FALSE(r.ok);
        CHECK_FALSE(r.failing.empty());
    }
    SECTION("dragging a free point follows the mouse as far as the rules allow")
    {
        Sketch t = roughRectangle();
        rule(t, Rule::Horizontal, 0);
        rule(t, Rule::Vertical, 1);
        solve(t);
        solve(t, {{3, Point{25, 15}}});
        CHECK_THAT(t.points[3].x, WithinAbs(25, 0.5));
        CHECK_THAT(t.points[3].y, WithinAbs(15, 0.5));
        CHECK_THAT(t.points[2].y, WithinAbs(t.points[1].y, 1e-6));  // still level
    }
}

TEST_CASE("Circles, arcs and the other rules", "[sketch]")
{
    SECTION("a line touching a circle")
    {
        Sketch s;
        const int c = circle(s, {0, 0}, 4.0);
        const int a = addPoint(s, {-10, 6}), b = addPoint(s, {10, 7});
        const int l = line(s, a, b);
        rule(s, Rule::Coincident, 0, s.geos[std::size_t(c)].a);  // centre on the origin
        rule(s, Rule::Radius, c, -1, -1, 5.0);
        rule(s, Rule::Horizontal, l);
        rule(s, Rule::Tangent, l, c, -1, 1, 1);
        CHECK(solve(s).ok);
        CHECK_THAT(s.points[std::size_t(a)].y, WithinAbs(5.0, 1e-7));
        CHECK_THAT(radiusOf(s, s.geos[std::size_t(c)]), WithinAbs(5.0, 1e-9));
    }
    SECTION("parallel, square, equal, angle")
    {
        Sketch s;
        const int a = addPoint(s, {0, 0.1}), b = addPoint(s, {10, 1}), c = addPoint(s, {0, 5}), d = addPoint(s, {7, 6});
        const int l1 = line(s, a, b), l2 = line(s, c, d);
        rule(s, Rule::Parallel, l1, l2);
        rule(s, Rule::Equal, l1, l2);
        rule(s, Rule::Angle, l1, -1, -1, 30.0);
        CHECK(solve(s).ok);
        const auto dir = [&](int p, int q) {
            return std::atan2(s.points[std::size_t(q)].y - s.points[std::size_t(p)].y,
                              s.points[std::size_t(q)].x - s.points[std::size_t(p)].x) * 180 / pi;
        };
        CHECK_THAT(dir(a, b), WithinAbs(30.0, 1e-6));
        CHECK_THAT(dir(c, d), WithinAbs(30.0, 1e-6));
        const double len1 = std::hypot(s.points[std::size_t(b)].x - s.points[std::size_t(a)].x,
                                       s.points[std::size_t(b)].y - s.points[std::size_t(a)].y);
        const double len2 = std::hypot(s.points[std::size_t(d)].x - s.points[std::size_t(c)].x,
                                       s.points[std::size_t(d)].y - s.points[std::size_t(c)].y);
        CHECK_THAT(len1, WithinAbs(len2, 1e-6));
        s.constraints.front().rule = Rule::Perpendicular;
        CHECK(solve(s).ok);
        CHECK_THAT(std::cos((dir(c, d) - dir(a, b)) * pi / 180), WithinAbs(0.0, 1e-7));  // square, either way
        // A second angle on the same line that disagrees: they fight.
        rule(s, Rule::Angle, l1, -1, -1, 45.0);
        CHECK_FALSE(solve(s).ok);
    }
    SECTION("symmetric about a line, middle of two points, point on a circle")
    {
        Sketch s;
        const int axisA = addPoint(s, {0, -10}), axisB = addPoint(s, {0.5, 10});
        const int axis = line(s, axisA, axisB);
        s.geos[std::size_t(axis)].construction = true;
        rule(s, Rule::Vertical, axis);
        rule(s, Rule::Coincident, 0, axisA);
        const int p = addPoint(s, {-6, 2}), q = addPoint(s, {5, 3});
        rule(s, Rule::Symmetric, p, q, axis);
        const int m = addPoint(s, {1, 1});
        rule(s, Rule::Midpoint, p, q, m);
        const int c = circle(s, {20, 0}, 3);
        const int on = addPoint(s, {25, 1});
        rule(s, Rule::PointOn, on, c);
        CHECK(solve(s).ok);
        CHECK_THAT(s.points[std::size_t(p)].x, WithinAbs(-s.points[std::size_t(q)].x, 1e-7));
        CHECK_THAT(s.points[std::size_t(p)].y, WithinAbs(s.points[std::size_t(q)].y, 1e-7));
        CHECK_THAT(s.points[std::size_t(m)].x, WithinAbs(0.0, 1e-7));
        const Point centre = s.points[std::size_t(s.geos[std::size_t(c)].a)];
        CHECK_THAT(std::hypot(s.points[std::size_t(on)].x - centre.x, s.points[std::size_t(on)].y - centre.y),
                   WithinAbs(radiusOf(s, s.geos[std::size_t(c)]), 1e-7));
    }
    SECTION("arcs keep their ends on the circle")
    {
        Sketch s;
        const int c = addPoint(s, {0, 0}), b = addPoint(s, {5, 0}), e = addPoint(s, {0, 4});
        s.geos.push_back({GeoKind::Arc, c, b, e, -1, false});
        rule(s, Rule::Radius, 0, -1, -1, 6.0);
        CHECK(solve(s).ok);
        const Point o = s.points[std::size_t(c)];
        CHECK_THAT(std::hypot(s.points[std::size_t(b)].x - o.x, s.points[std::size_t(b)].y - o.y), WithinAbs(6.0, 1e-7));
        CHECK_THAT(std::hypot(s.points[std::size_t(e)].x - o.x, s.points[std::size_t(e)].y - o.y), WithinAbs(6.0, 1e-7));
    }
}

TEST_CASE("Editing helpers", "[sketch]")
{
    Sketch s = roughRectangle();
    rule(s, Rule::Horizontal, 0);
    rule(s, Rule::Distance, 2, -1, -1, 20.0);
    CHECK(looseEnds(s).empty());
    removeGeos(s, {1});
    CHECK(s.geos.size() == 3);
    CHECK(s.constraints.size() == 2);  // the rules were on other lines
    CHECK(s.constraints[1].a == 1);    // line 2 is now line 1
    CHECK(looseEnds(s).size() == 2);
    removeGeos(s, {0, 1, 2});
    CHECK(s.points.size() == 1);  // just the origin
    CHECK(s.constraints.empty());

    Sketch t = roughRectangle();
    // Joining two points: corner 2 into corner 1 (a triangle now).
    removeGeos(t, {0});
    const int kept = mergePoints(t, 1, 2);
    CHECK(kept == 1);
    CHECK(t.points.size() == 4);
    CHECK(looseEnds(t).empty());
}

TEST_CASE("Pads: outlines pushed up", "[sketch][pad]")
{
    Sketch s = roughRectangle();
    rule(s, Rule::Horizontal, 0);
    rule(s, Rule::Horizontal, 2);
    rule(s, Rule::Vertical, 1);
    rule(s, Rule::Vertical, 3);
    rule(s, Rule::Coincident, 0, 1);
    rule(s, Rule::Distance, 0, -1, -1, 30.0);
    rule(s, Rule::Distance, 1, -1, -1, 20.0);
    REQUIRE(solve(s).ok);
    const int hole = circle(s, {15, 10}, 4);
    (void)hole;
    PadSpec pad;
    pad.sketch = s;
    pad.length = 5;
    CHECK_THAT(volumeOf(padShape(pad)), WithinRel((600 - pi * 16) * 5, 1e-6));

    SECTION("an island in the hole is solid again")
    {
        circle(pad.sketch, {15, 10}, 2);
        CHECK_THAT(volumeOf(padShape(pad)), WithinRel((600 - pi * 16 + pi * 4) * 5, 1e-6));
    }
    SECTION("reversed and both ways")
    {
        pad.reversed = true;
        Document doc;
        CommandStack stack(doc);
        Body b;
        b.id = doc.allocateId();
        b.name = "Pad";
        b.spec = pad;
        stack.execute(std::make_unique<AddBodyCommand>(b));
        CHECK_THAT(doc.worldBounds(b.id).max[2], WithinAbs(0.0, 1e-6));
        CHECK_THAT(doc.worldBounds(b.id).min[2], WithinAbs(-5.0, 1e-6));
        pad.reversed = false;
        pad.midplane = true;
        stack.execute(std::make_unique<SetSpecCommand>(b.id, PrimitiveSpec{pad}));
        CHECK_THAT(doc.worldBounds(b.id).max[2], WithinAbs(2.5, 1e-6));
    }
    SECTION("an open outline says so")
    {
        removeGeos(pad.sketch, {2});
        CHECK_THROWS_AS(padShape(pad), GeometryError);
        CHECK(looseEnds(pad.sketch).size() == 2);
    }
    SECTION("construction lines are left out")
    {
        const int a = addPoint(pad.sketch, {0, 0}), b = addPoint(pad.sketch, {40, 40});
        pad.sketch.geos.push_back({GeoKind::Line, a, b, -1, -1, true});
        CHECK_THAT(volumeOf(padShape(pad)), WithinRel((600 - pi * 16) * 5, 1e-6));
    }
    SECTION("an arc and lines make a D shape")
    {
        Sketch d;
        const int p = addPoint(d, {0, -5}), q = addPoint(d, {0, 5}), c = addPoint(d, {0, 0});
        line(d, q, p);
        d.geos.push_back({GeoKind::Arc, c, p, q, -1, false});  // round the right side
        PadSpec dp;
        dp.sketch = d;
        dp.length = 2;
        CHECK_THAT(volumeOf(padShape(dp)), WithinRel(pi * 25 / 2 * 2, 1e-6));
    }
    SECTION("saved, loaded, resized, through all")
    {
        Document doc;
        CommandStack stack(doc);
        Body b;
        b.id = doc.allocateId();
        b.name = "Pad";
        b.spec = pad;
        stack.execute(std::make_unique<AddBodyCommand>(b));
        CHECK(history::describe(doc, b.id) == "sketch, 5 mm");
        std::random_device rd;
        const fs::path dir = fs::temp_directory_path() / ("forge3d-pad-" + std::to_string(rd()));
        fs::create_directories(dir);
        file::save(doc, dir / "p.f3d", "test");
        Document loaded;
        loaded.replaceAll(file::load(dir / "p.f3d"));
        CHECK(*loaded.find(b.id) == *doc.find(b.id));
        std::error_code ec;
        fs::remove_all(dir, ec);
        // Twice the size evenly.
        stack.execute(std::make_unique<SetSpecCommand>(b.id, scaled(PrimitiveSpec{pad}, 2.0)));
        CHECK_THAT(volumeOf(doc.shape(b.id)), WithinRel((600 - pi * 16) * 5 * 8, 1e-6));
    }
}
