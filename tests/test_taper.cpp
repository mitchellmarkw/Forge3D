// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 29: tapered (drafted) pads and pockets.
#include "forge/core/Body.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/History.h"
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

void square(Sketch& s, double side)
{
    const double h = side / 2;
    const int a = addPoint(s, {-h, -h}), b = addPoint(s, {h, -h}), c = addPoint(s, {h, h}), d = addPoint(s, {-h, h});
    for (const auto& [p, q] : {std::pair{a, b}, std::pair{b, c}, std::pair{c, d}, std::pair{d, a}})
        s.geos.push_back({GeoKind::Line, p, q, -1, -1, false});
}

void circle(Sketch& s, double r)
{
    s.radii.push_back(r);
    s.geos.push_back({GeoKind::Circle, 0, -1, -1, int(s.radii.size()) - 1, false});
}

double volumeOf(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

// A frustum between squares of sides a and b, h apart.
double frustum(double a, double b, double h) { return h / 3 * (a * a + a * b + b * b); }

}  // namespace

TEST_CASE("Taper: a pad's sides lean in (or out) as they go", "[taper]")
{
    const double t = std::tan(10 * pi / 180);
    PadSpec pad;
    square(pad.sketch, 20);
    pad.length = 10;
    pad.taper = 10;
    const TopoDS_Shape in = padShape(pad);
    CHECK(BRepCheck_Analyzer(in).IsValid());
    CHECK_THAT(volumeOf(in), WithinRel(frustum(20, 20 - 2 * 10 * t, 10), 1e-6));
    Bnd_Box box;
    BRepBndLib::AddOptimal(in, box, false, false);
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    CHECK_THAT(x1, WithinAbs(10.0, 1e-6));  // full size at the sketch
    CHECK_THAT(z1, WithinAbs(10.0, 1e-6));

    pad.taper = -10;
    CHECK_THAT(volumeOf(padShape(pad)), WithinRel(frustum(20, 20 + 2 * 10 * t, 10), 1e-6));

    // Downwards: still full size at the sketch, narrower further away.
    pad.taper = 10;
    pad.reversed = true;
    const TopoDS_Shape down = padShape(pad);
    CHECK_THAT(volumeOf(down), WithinRel(frustum(20, 20 - 2 * 10 * t, 10), 1e-6));
    Bnd_Box db;
    BRepBndLib::AddOptimal(down, db, false, false);
    db.Get(x0, y0, z0, x1, y1, z1);
    CHECK_THAT(z0, WithinAbs(-10.0, 1e-6));
    CHECK_THAT(z1, WithinAbs(0.0, 1e-6));

    // Both ways: narrowing each way from the middle.
    pad.reversed = false;
    pad.midplane = true;
    CHECK_THAT(volumeOf(padShape(pad)), WithinRel(2 * frustum(20, 20 - 2 * 5 * t, 5), 1e-6));

    // A round one becomes a cone; a hole in the outline opens out as the outside narrows.
    PadSpec round;
    circle(round.sketch, 10);
    round.length = 10;
    round.taper = 10;
    const double top = 10 - 10 * t;
    CHECK_THAT(volumeOf(padShape(round)), WithinRel(pi * 10 / 3 * (100 + 10 * top + top * top), 1e-6));
    PadSpec holed;
    square(holed.sketch, 30);
    circle(holed.sketch, 5);
    holed.length = 10;
    holed.taper = 10;
    const double wide = 5 + 10 * t;
    CHECK_THAT(volumeOf(padShape(holed)),
               WithinRel(frustum(30, 30 - 2 * 10 * t, 10) - pi * 10 / 3 * (25 + 5 * wide + wide * wide), 1e-6));

    // Too steep (the sides meet before the end), or out of range: told so.
    PadSpec steep;
    square(steep.sketch, 4);
    steep.length = 20;
    steep.taper = 30;
    CHECK_THROWS_AS(padShape(steep), GeometryError);
    steep.taper = 80;
    CHECK_THROWS(padShape(steep));
}

TEST_CASE("Taper: saved, described, and kept in shape when resized", "[taper]")
{
    Document doc;
    CommandStack stack(doc);
    Body body;
    body.id = doc.allocateId();
    body.name = "Pad";
    PadSpec pad;
    square(pad.sketch, 20);
    pad.length = 10;
    pad.taper = 10;
    body.spec = pad;
    stack.execute(std::make_unique<AddBodyCommand>(body));
    CHECK(history::describe(doc, body.id).find("taper") != std::string::npos);
    Document other;
    other.replaceAll(file::fromJson(file::toJson(doc, "test")));
    CHECK(*other.find(body.id) == *doc.find(body.id));

    // Twice as tall: the sides lean half as much (by their slope), so the top is the same size.
    for (const Body& b : scaledTree(doc, body.id, {1, 1, 2}, {0, 0, 0}))
        if (b.id == body.id) {
            const auto& p = std::get<PadSpec>(b.spec);
            CHECK_THAT(std::tan(p.taper * pi / 180) * p.length, WithinRel(std::tan(10 * pi / 180) * 10, 1e-9));
        }
    CHECK_THROWS(scaledTree(doc, body.id, {2, 1, 1}, {0, 0, 0}));
}
