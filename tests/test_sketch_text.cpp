// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 30: words in sketches.
#include "forge/core/Body.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Primitives.h"
#include "forge/core/Sketch.h"
#include "forge/core/SketchShape.h"

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using namespace forge::sketch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

double areaOf(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props);
    return props.Mass();
}

double volumeOf(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

Words hi(Sketch& s, Point at)
{
    Words w;
    w.text = "HI";
    w.height = 10;
    w.at = addPoint(s, at);
    s.texts.push_back(w);
    return w;
}

void rectangle(Sketch& s, double x0, double y0, double x1, double y1)
{
    const int a = addPoint(s, {x0, y0}), b = addPoint(s, {x1, y0}), c = addPoint(s, {x1, y1}), d = addPoint(s, {x0, y1});
    for (const auto& [p, q] : {std::pair{a, b}, std::pair{b, c}, std::pair{c, d}, std::pair{d, a}})
        s.geos.push_back({GeoKind::Line, p, q, -1, -1, false});
}

}  // namespace

TEST_CASE("Sketch words: letters are outlines", "[sketchtext]")
{
    Sketch s;
    hi(s, {5, 3});
    CHECK_FALSE(isEmpty(s));
    CHECK(loosePoints(s).empty());  // the words' point isn't a spot for a hole
    CHECK(solve(s).ok);
    const TopoDS_Shape flat = faces(s);
    int count = 0;
    for (TopExp_Explorer it(flat, TopAbs_FACE); it.More(); it.Next())
        ++count;
    CHECK(count == 2);  // H and I
    Bnd_Box box;
    BRepBndLib::AddOptimal(flat, box, false, false);
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    CHECK(x0 >= 5.0);  // starts at its point
    CHECK(x0 < 7.0);
    CHECK_THAT(y0, WithinAbs(3.0, 0.2));        // stands on its baseline
    CHECK_THAT(y1 - y0, WithinAbs(10.0, 0.3));  // capitals as tall as asked
    const double letters = areaOf(flat);
    CHECK(letters > 10);
    CHECK(letters < 150);

    // Padded: letters standing up.
    PadSpec pad;
    pad.sketch = s;
    pad.length = 2;
    const TopoDS_Shape solid = primitives::buildShape(pad);
    CHECK(BRepCheck_Analyzer(solid).IsValid());
    CHECK_THAT(volumeOf(solid), WithinRel(2 * letters, 1e-6));

    // Turned a quarter: now as wide as it was tall.
    s.texts[0].angle = 90;
    Bnd_Box turned;
    BRepBndLib::AddOptimal(faces(s), turned, false, false);
    turned.Get(x0, y0, z0, x1, y1, z1);
    CHECK_THAT(x1 - x0, WithinAbs(10.0, 0.3));
    CHECK_THAT(x1, WithinAbs(5.0, 0.2));
    s.texts[0].angle = 0;

    // Inside an outline: cut out of it (a stencil).
    Sketch plate = s;
    rectangle(plate, 0, 0, 40, 16);
    CHECK_THAT(areaOf(faces(plate)), WithinRel(40.0 * 16.0 - letters, 1e-6));

    // The outlines for drawing: closed lines, at the origin.
    const auto lines = wordsOutlines(s.texts[0]);
    CHECK(lines.size() == 2);
    CHECK(lines[0].front() == lines[0].back());

    // Nothing to draw: told so.
    Sketch blank;
    Words none;
    none.text = "   ";
    none.at = 0;
    blank.texts.push_back(none);
    CHECK_THROWS(faces(blank));
}

TEST_CASE("Sketch words: moved, resized, removed and saved with the sketch", "[sketchtext]")
{
    Sketch s;
    rectangle(s, 0, 0, 40, 16);
    hi(s, {5, 3});
    const int at = s.texts[0].at;

    // A dimension holds the words' point like any other.
    s.constraints.push_back(makeRule(s, Rule::DistanceX, 0, at));
    s.constraints.back().value = 12;
    CHECK(solve(s).ok);
    CHECK_THAT(s.points[std::size_t(at)].x, WithinAbs(12.0, 1e-6));

    // Twice the size: letters twice as tall. Stretched one way: not allowed.
    Sketch big = s;
    CHECK(scale(big, 2, 2));
    CHECK_THAT(big.texts[0].height, WithinAbs(20.0, 1e-12));
    Sketch wide = s;
    CHECK_FALSE(scale(wide, 2, 1));

    // Removing an earlier point keeps the words on theirs.
    Sketch fewer = s;
    removeGeos(fewer, {0, 1, 2, 3});
    REQUIRE(fewer.texts.size() == 1);
    CHECK(fewer.points.size() == 2);
    CHECK_THAT(fewer.points[std::size_t(fewer.texts[0].at)].x, WithinAbs(12.0, 1e-6));
    removeWords(fewer, 0);
    CHECK(fewer.texts.empty());
    CHECK(fewer.points.size() == 1);
    CHECK(isEmpty(fewer));
    Sketch gone = s;
    removePoints(gone, {at});
    CHECK(gone.texts.empty());
    CHECK(gone.geos.size() == 4);

    // Saved and opened.
    Document doc;
    CommandStack stack(doc);
    Body body;
    body.id = doc.allocateId();
    body.name = "Plate";
    PadSpec pad;
    pad.sketch = s;
    pad.sketch.texts[0].font = "Some Font";
    pad.sketch.texts[0].bold = true;
    pad.sketch.texts[0].angle = 15;
    body.spec = pad;
    stack.execute(std::make_unique<AddBodyCommand>(body));
    Document other;
    other.replaceAll(file::fromJson(file::toJson(doc, "test")));
    CHECK(*other.find(body.id) == *doc.find(body.id));
}
