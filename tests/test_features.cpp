// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 8: revolve, holes, shell.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Follow.h"
#include "forge/core/History.h"
#include "forge/core/Pattern.h"
#include "forge/core/Primitives.h"
#include "forge/core/Scale.h"
#include "forge/core/Shell.h"
#include "forge/core/Sketch.h"
#include "forge/core/SketchShape.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <cmath>
#include <numbers>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double pi = std::numbers::pi;

double volumeOf(const TopoDS_Shape& shape)
{
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

int line(sketch::Sketch& s, int a, int b, bool helper = false)
{
    s.geos.push_back({sketch::GeoKind::Line, a, b, -1, -1, helper});
    return int(s.geos.size()) - 1;
}

// A rectangle x0..x1 by y0..y1, and a helper line up the Y axis (geo 4).
sketch::Sketch ringProfile(double x0, double x1, double y0, double y1)
{
    sketch::Sketch s;
    const int a = sketch::addPoint(s, {x0, y0}), b = sketch::addPoint(s, {x1, y0}), c = sketch::addPoint(s, {x1, y1}),
              d = sketch::addPoint(s, {x0, y1});
    line(s, a, b);
    line(s, b, c);
    line(s, c, d);
    line(s, d, a);
    const int p = sketch::addPoint(s, {0, -5}), q = sketch::addPoint(s, {0, 20});
    line(s, p, q, true);
    return s;
}

ObjectId addBox(CommandStack& stack, Document& doc, double w, double d, double h)
{
    Body box;
    box.id = doc.allocateId();
    box.name = "Box";
    box.spec = BoxSpec{w, d, h};
    stack.execute(std::make_unique<AddBodyCommand>(box));
    return box.id;
}

std::unique_ptr<Document> roundTrip(const Document& doc)
{
    auto back = std::make_unique<Document>();
    back->replaceAll(file::fromJson(file::toJson(doc, "test")));
    return back;
}

}  // namespace

TEST_CASE("Revolve spins an outline around a helper line", "[revolve]")
{
    RevolveSpec r;
    r.sketch = ringProfile(5, 8, 0, 10);
    r.axis = 4;
    // A tube: outside 8, inside 5, 10 long.
    CHECK_THAT(volumeOf(sketch::revolveShape(r)), WithinRel(pi * (64 - 25) * 10, 1e-6));
    r.angle = 90;
    CHECK_THAT(volumeOf(sketch::revolveShape(r)), WithinRel(pi * (64 - 25) * 10 / 4, 1e-6));
    r.midplane = true;
    CHECK_THAT(volumeOf(sketch::revolveShape(r)), WithinRel(pi * (64 - 25) * 10 / 4, 1e-6));

    SECTION("No axis, or one crossed by the outline, is refused")
    {
        RevolveSpec bad = r;
        bad.axis = -1;
        CHECK_THROWS_WITH(sketch::revolveShape(bad), Catch::Matchers::ContainsSubstring("helper line"));
        bad.axis = 4;
        bad.sketch = ringProfile(-3, 8, 0, 10);
        CHECK_THROWS_WITH(sketch::revolveShape(bad), Catch::Matchers::ContainsSubstring("crosses the axis"));
    }
    SECTION("Saved and read back, evenly scaled")
    {
        Document doc;
        Body b;
        b.id = doc.allocateId();
        b.name = "Revolve";
        b.spec = r;
        doc.insertBody(b);
        const auto backPtr = roundTrip(doc);
        const Document& back = *backPtr;
        REQUIRE(back.find(b.id));
        CHECK(back.find(b.id)->spec == b.spec);
        const RevolveSpec bigger = std::get<RevolveSpec>(scaled(b.spec, 2.0));
        CHECK_THAT(volumeOf(sketch::revolveShape(bigger)), WithinRel(8 * pi * (64 - 25) * 10 / 4, 1e-6));
        CHECK(history::describe(doc, b.id).find("spun 90") != std::string::npos);
    }
}

TEST_CASE("Holes: plain, counterbored, countersunk, standard sizes", "[hole]")
{
    HoleSpec h;
    sketch::addPoint(h.sketch, {0, 0});
    sketch::addPoint(h.sketch, {10, 0});
    h.diameter = 4;
    h.depth = 6;
    const double above = 0.5;
    CHECK_THAT(volumeOf(sketch::holeShape(h)), WithinRel(2 * pi * 4 * (6 + above), 1e-6));
    CHECK(sketch::loosePoints(h.sketch).size() == 2);

    h.kind = HoleKind::Counterbored;
    h.headDiameter = 8;
    h.headDepth = 2;
    CHECK_THAT(volumeOf(sketch::holeShape(h)), WithinRel(2 * (pi * 16 * (2 + above) + pi * 4 * 4), 1e-6));
    h.headDepth = 7;
    CHECK_THROWS(sketch::holeShape(h));

    h.kind = HoleKind::Countersunk;
    h.countersinkDiameter = 8;
    h.countersinkAngle = 90;  // sinks 2 mm
    const double cone = pi * 2 / 3 * (16 + 4 + 8);
    CHECK_THAT(volumeOf(sketch::holeShape(h)), WithinRel(2 * (pi * 16 * above + cone + pi * 4 * 4), 1e-6));

    const HoleSpec m3 = sketch::sizedAs(h, "M3", false);
    CHECK(m3.diameter == 3.4);
    CHECK(m3.headDiameter == 6.0);
    CHECK(sketch::sizedAs(h, "M3", true).diameter == 2.5);
    CHECK(sketch::sizedAs(h, "", true).diameter == h.diameter);

    HoleSpec none;
    CHECK_THROWS_WITH(sketch::holeShape(none), Catch::Matchers::ContainsSubstring("Click on the face"));
}

TEST_CASE("Holes in a box: a Subtract step, through all, saved", "[hole]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 40, 20, 10);
    // On the top face (z = 10), looking down into the box.
    const TopoDS_Shape shape = pickableShape(doc, box);
    std::optional<sketch::FacePlane> top;
    for (int f = 0; f < 6 && !top; ++f)
        if (auto p = sketch::planeOfFace(shape, f); p && p->frame.m[2][2] > 0.9)
            top = p;
    REQUIRE(top);
    HoleSpec h = sketch::sizedAs(HoleSpec{}, "M4", false);  // 4.5 clearance
    sketch::addPoint(h.sketch, {-10, 0});
    sketch::addPoint(h.sketch, {10, 0});
    h.depth = 4;
    Body tool;
    tool.id = doc.allocateId();
    tool.name = "Hole";
    tool.spec = h;
    tool.placement = placementFromTransform(top->frame, pivotOf(tool.spec));
    stack.execute(std::make_unique<AddBodyCommand>(tool));
    Body result;
    result.id = doc.allocateId();
    result.name = "Drilled";
    BooleanSpec spec;
    spec.op = BooleanOp::Subtract;
    spec.operands = {box, tool.id};
    result.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(result));
    const double full = 40 * 20 * 10;
    const double each = pi * 2.25 * 2.25;
    CHECK_THAT(volumeOf(doc.shape(result.id)), WithinRel(full - 2 * each * 4, 1e-6));
    CHECK(history::describe(doc, result.id).find("2 holes M4") != std::string::npos);

    Body through = *doc.find(result.id);
    through.boolean->throughAll = true;
    stack.execute(std::make_unique<EditBodyCommand>(*doc.find(result.id), through, "Through all"));
    CHECK_THAT(volumeOf(doc.shape(result.id)), WithinRel(full - 2 * each * 10, 1e-6));

    const auto backPtr = roundTrip(doc);
        const Document& back = *backPtr;
    CHECK(back.find(tool.id)->spec == doc.find(tool.id)->spec);
    CHECK_THAT(volumeOf(back.shape(result.id)), WithinRel(full - 2 * each * 10, 1e-6));
}

TEST_CASE("Sketch points on their own survive edits and can be removed", "[hole]")
{
    sketch::Sketch s;
    const int lone = sketch::addPoint(s, {3, 3});
    const int a = sketch::addPoint(s, {0, 0.5}), b = sketch::addPoint(s, {10, 0.5});
    line(s, a, b);
    s.constraints.push_back(sketch::makeRule(s, sketch::Rule::DistanceX, 0, lone));
    sketch::removeGeos(s, {0});
    CHECK(s.points.size() == 2);  // the line's points went; the lone one and the origin stay
    CHECK(sketch::loosePoints(s) == std::vector<int>{1});
    CHECK(s.constraints.size() == 1);
    sketch::removePoints(s, {1});
    CHECK(s.points.size() == 1);
    CHECK(s.constraints.empty());
}

TEST_CASE("Shell hollows a box, open where picked, and keeps its faces when resized", "[shell]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 40, 30, 20);
    const TopoDS_Shape shape = pickableShape(doc, box);
    const auto top = edges::faceUnder(shape, {0, 0, 50}, {0, 0, -1});
    REQUIRE(top);
    ShellFinish f;
    f.thickness = 2;
    f.open = {shell::refFor(shape, top->face, top->point)};
    CHECK_THAT(f.open[0].normal[2], WithinAbs(1, 1e-9));
    Body result;
    result.id = doc.allocateId();
    result.name = "Shelled";
    result.boolean = shellSpec(doc, box, f);
    stack.execute(std::make_unique<CombineCommand>(result));
    const double cup = 40 * 30 * 20 - 36 * 26 * 18;
    CHECK_THAT(volumeOf(doc.shape(result.id)), WithinRel(cup, 1e-6));
    CHECK(history::describe(doc, result.id) == "hollowed, walls 2 mm, 1 face open");
    REQUIRE(pickedFaces(doc, result.id).size() == 1);
    CHECK(pickedFaces(doc, result.id)[0] == top->face);

    SECTION("Taller box: still open at the (new) top")
    {
        Body taller = *doc.find(box);
        std::get<BoxSpec>(taller.spec).height = 50;
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box), taller, "Taller"));
        CHECK_THAT(volumeOf(doc.shape(result.id)), WithinRel(40 * 30 * 50 - 36 * 26 * 48, 1e-6));
    }
    SECTION("Closed, and outward")
    {
        ShellFinish closed = shellInPlate(doc, result.id);
        closed.open.clear();
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(result.id), withShell(doc, result.id, closed),
                                                        "Closed"));
        CHECK_THAT(volumeOf(doc.shape(result.id)), WithinRel(40 * 30 * 20 - 36 * 26 * 16, 1e-6));
        ShellFinish out = closed;
        out.outward = true;
        out.open = shellInPlate(doc, result.id).open;
        f.outward = true;
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(result.id), withShell(doc, result.id, f),
                                                        "Outward"));
        // Walls outside the old box, open at the top (which rises by 2... the top is removed).
        CHECK(volumeOf(doc.shape(result.id)) > 40 * 30 * 20 * 0.1);
    }
    SECTION("Too thick is refused with a reason")
    {
        ShellFinish thick = shellInPlate(doc, result.id);
        thick.thickness = 40;
        Body next = withShell(doc, result.id, thick);
        CHECK_THROWS_WITH(doc.previewCombine(*next.boolean), Catch::Matchers::ContainsSubstring("thinner walls"));
    }
    SECTION("Saved and read back")
    {
        const auto backPtr = roundTrip(doc);
        const Document& back = *backPtr;
        CHECK(back.find(result.id)->boolean == doc.find(result.id)->boolean);
        CHECK_THAT(volumeOf(back.shape(result.id)), WithinRel(cup, 1e-6));
    }
}

TEST_CASE("Middle planes: a groove round a cylinder", "[planes]")
{
    // The front plane faces you: X right, Z up, its Z towards you (-Y).
    const Transform front = sketch::middlePlane(0, {1, 2, 3});
    const Vec3 up = front.applyDirection({0, 1, 0}), out = front.applyDirection({0, 0, 1});
    CHECK(up[2] == 1.0);
    CHECK(out[1] == -1.0);
    const Transform side = sketch::middlePlane(1, {0, 0, 0});
    CHECK(side.applyDirection({1, 0, 0})[1] == 1.0);
    CHECK(side.applyDirection({0, 0, 1})[0] == 1.0);

    Document doc;
    CommandStack stack(doc);
    Body cylinder;
    cylinder.id = doc.allocateId();
    cylinder.name = "Cylinder";
    cylinder.spec = CylinderSpec{10, 10, 30};
    stack.execute(std::make_unique<AddBodyCommand>(cylinder));
    const auto c = doc.worldBounds(cylinder.id).center();
    const Transform plane = sketch::middlePlane(0, {c[0], c[1], c[2]});
    // Its cross-section: a 20 x 30 rectangle round the middle.
    const auto section = sketch::sectionOutlines(pickableShape(doc, cylinder.id), plane);
    REQUIRE(section.size() >= 4);
    for (const auto& line : section)
        for (const auto& p : line) {
            CHECK(std::abs(p.x) <= 10 + 1e-6);
            CHECK(std::abs(p.y) <= 15 + 1e-6);
        }
    // A 2 x 2 square on the right-hand edge, spun round the cylinder's axis (Y in the
    // sketch): a groove all the way round.
    RevolveSpec groove;
    sketch::Sketch& s = groove.sketch;
    const int a = sketch::addPoint(s, {9, -1}), b = sketch::addPoint(s, {11, -1}), d = sketch::addPoint(s, {11, 1}),
              e = sketch::addPoint(s, {9, 1});
    line(s, a, b);
    line(s, b, d);
    line(s, d, e);
    line(s, e, a);
    const int p = sketch::addPoint(s, {0, -20}), q = sketch::addPoint(s, {0, 20});
    groove.axis = line(s, p, q, true);
    Body tool;
    tool.id = doc.allocateId();
    tool.name = "Groove";
    tool.spec = groove;
    tool.placement = placementFromTransform(plane, pivotOf(tool.spec));
    stack.execute(std::make_unique<AddBodyCommand>(tool));
    Body result;
    result.id = doc.allocateId();
    result.name = "Grooved";
    BooleanSpec spec;
    spec.op = BooleanOp::Subtract;
    spec.operands = {cylinder.id, tool.id};
    result.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(result));
    // The ring removed: radius 9..10, 2 high.
    CHECK_THAT(volumeOf(doc.shape(result.id)), WithinRel(pi * 100 * 30 - pi * (100 - 81) * 2, 1e-6));
}

namespace {

FollowFinish onTop(double z)
{
    FollowFinish f;
    f.origin = {0, 0, z};
    f.xAxis = {1, 0, 0};
    f.normal = {0, 0, 1};
    return f;
}

double removed(Document& doc, CommandStack& stack, ObjectId target, const FollowFinish& f, ObjectId* resultOut = nullptr)
{
    const double before = volumeOf(doc.shape(target));
    Body result;
    result.id = doc.allocateId();
    result.name = "Followed";
    result.boolean = followSpec(doc, target, f);
    stack.execute(std::make_unique<CombineCommand>(result));
    if (resultOut)
        *resultOut = result.id;
    return before - volumeOf(doc.shape(result.id));
}

}  // namespace

TEST_CASE("Follow: grooves and ridges along a path on a flat face", "[follow]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 40, 40, 10);
    FollowFinish f = onTop(10);
    f.width = 2;
    f.depth = 1;

    SECTION("A straight square groove")
    {
        line(f.path, sketch::addPoint(f.path, {-10, 0}), sketch::addPoint(f.path, {10, 0}));
        ObjectId id = kInvalidId;
        CHECK_THAT(removed(doc, stack, box, f, &id), WithinRel(2 * 1 * 20, 1e-2));
        CHECK(history::describe(doc, id) == "square groove, 2 x 1 mm");
        // Saved and read back.
        const auto back = roundTrip(doc);
        CHECK(back->find(id)->boolean == doc.find(id)->boolean);
        // A taller box: the groove is still in its top.
        Body taller = *doc.find(box);
        std::get<BoxSpec>(taller.spec).height = 25;
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box), taller, "Taller"));
        CHECK(doc.problemWith(id).value_or("") == "");
        CHECK_THAT(40 * 40 * 25 - volumeOf(doc.shape(id)), WithinRel(40.0, 1e-2));
    }
    SECTION("A ring: a circle")
    {
        const int c = sketch::addPoint(f.path, {0, 0});
        f.path.radii.push_back(10);
        f.path.geos.push_back({sketch::GeoKind::Circle, c, -1, -1, 0, false});
        CHECK_THAT(removed(doc, stack, box, f), WithinRel(pi * (11 * 11 - 9 * 9) * 1, 1e-2));
    }
    SECTION("A round ridge and a V groove")
    {
        line(f.path, sketch::addPoint(f.path, {-10, 0}), sketch::addPoint(f.path, {10, 0}));
        f.ridge = true;
        f.profile = FollowProfile::Round;  // 2 wide, 1 high: a half-round bead
        CHECK_THAT(-removed(doc, stack, box, f), WithinRel(pi / 2 * 20, 0.03));
        FollowFinish v = f;
        v.ridge = false;
        v.profile = FollowProfile::V;
        const ObjectId other = addBox(stack, doc, 40, 40, 10);
        CHECK_THAT(removed(doc, stack, other, v), WithinRel(1.0 * 20, 1e-2));  // a 2 x 1 triangle, 20 long
    }
    SECTION("Its own cross-section, and a closed outline with corners")
    {
        // A 3-wide, 1.5-deep triangle drawn as the cross-section.
        const int a = sketch::addPoint(f.custom, {-1.5, 0.5}), b = sketch::addPoint(f.custom, {1.5, 0.5}),
                  c = sketch::addPoint(f.custom, {0, -1.5});
        line(f.custom, a, b);
        line(f.custom, b, c);
        line(f.custom, c, a);
        f.profile = FollowProfile::Custom;
        const int p = sketch::addPoint(f.path, {-10, -10}), q = sketch::addPoint(f.path, {10, -10}),
                  r = sketch::addPoint(f.path, {10, 10}), t = sketch::addPoint(f.path, {-10, 10});
        line(f.path, p, q);
        line(f.path, q, r);
        line(f.path, r, t);
        line(f.path, t, p);
        const double cut = removed(doc, stack, box, f);
        // Below the surface the triangle is 2.25 wide and 1.5 deep: 1.6875; 80 long (the
        // corners add and take a little).
        CHECK_THAT(cut, WithinRel(1.6875 * 80, 0.05));
    }
    SECTION("Nothing drawn, or too deep a round one, is refused")
    {
        CHECK_THROWS_WITH(doc.previewCombine(followSpec(doc, box, f)),
                          Catch::Matchers::ContainsSubstring("Draw the path"));
        line(f.path, sketch::addPoint(f.path, {-10, 0}), sketch::addPoint(f.path, {10, 0}));
        f.profile = FollowProfile::Round;
        f.depth = 3;
        Body result;
        result.id = doc.allocateId();
        result.boolean = followSpec(doc, box, f);
        CHECK_THROWS_WITH(doc.previewCombine(*result.boolean), Catch::Matchers::ContainsSubstring("deeper"));
    }
}

TEST_CASE("Follow: a path on a curved surface bends round with it", "[follow]")
{
    Document doc;
    CommandStack stack(doc);
    Body cylinder;
    cylinder.id = doc.allocateId();
    cylinder.name = "Cylinder";
    cylinder.spec = CylinderSpec{10, 10, 30};
    stack.execute(std::make_unique<AddBodyCommand>(cylinder));
    // On the front (facing -Y), halfway up; a 10 mm line across.
    FollowFinish f;
    f.origin = {0, -10, 15};
    f.xAxis = {1, 0, 0};
    f.normal = {0, -1, 0};
    f.width = 2;
    f.depth = 1;
    line(f.path, sketch::addPoint(f.path, {-5, 0}), sketch::addPoint(f.path, {5, 0}));
    const auto laid = followPathInPlate(doc, cylinder.id, f, false);
    REQUIRE(laid.size() == 1);
    for (const Vec3& p : laid.front())  // on the surface all the way
        CHECK_THAT(std::hypot(p[0], p[1]), WithinAbs(10, 1e-3));
    // 10 mm round the surface: 1 radian of the cylinder.
    const Vec3 end = laid.front().back();
    CHECK_THAT(std::atan2(end[0], -end[1]), WithinAbs(0.5, 0.02));
    // An annular sector 2 high, 1 deep, 1 radian.
    CHECK_THAT(removed(doc, stack, cylinder.id, f), WithinRel(0.5 * (100 - 81) * 2, 0.03));
}

TEST_CASE("Follow: a path off the edge of its face is refused quickly", "[follow]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 20, 20, 10);
    FollowFinish f = onTop(10);
    const auto started = std::chrono::steady_clock::now();
    SECTION("A circle bigger than the box")
    {
        const int c = sketch::addPoint(f.path, {0, 0});
        f.path.radii.push_back(15);
        f.path.geos.push_back({sketch::GeoKind::Circle, c, -1, -1, 0, false});
        CHECK_THROWS_WITH(doc.previewCombine(followSpec(doc, box, f)),
                          Catch::Matchers::ContainsSubstring("runs off the edge"));
    }
    SECTION("The box made smaller than its groove")
    {
        const int c = sketch::addPoint(f.path, {0, 0});
        f.path.radii.push_back(6);
        f.path.geos.push_back({sketch::GeoKind::Circle, c, -1, -1, 0, false});
        ObjectId id = kInvalidId;
        removed(doc, stack, box, f, &id);
        Body smaller = *doc.find(box);
        std::get<BoxSpec>(smaller.spec).width = 8;
        std::get<BoxSpec>(smaller.spec).depth = 8;
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(box), smaller, "Smaller"));
        // The step can't be made: the shape shows without it, with the reason.
        CHECK(doc.problemWith(id).value_or("").find("runs off the edge") != std::string::npos);
    }
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    CHECK(seconds < 5.0);
}

namespace {

// Holes drilled into the top of `target` (top at z) at the given spots: the Drilled step.
ObjectId drill(Document& doc, CommandStack& stack, ObjectId target, double z, std::vector<sketch::Point> spots,
               double diameter, double depth, bool throughAll = false)
{
    HoleSpec h;
    h.diameter = diameter;
    h.depth = depth;
    for (const auto& p : spots)
        sketch::addPoint(h.sketch, p);
    Body tool;
    tool.id = doc.allocateId();
    tool.name = "Hole";
    tool.spec = h;
    tool.placement.z = z;
    stack.execute(std::make_unique<AddBodyCommand>(tool));
    Body result;
    result.id = doc.allocateId();
    result.name = "Drilled";
    BooleanSpec spec;
    spec.op = BooleanOp::Subtract;
    spec.operands = {target, tool.id};
    spec.throughAll = throughAll;
    result.boolean = spec;
    stack.execute(std::make_unique<CombineCommand>(result));
    return result.id;
}

ObjectId repeat(Document& doc, CommandStack& stack, ObjectId target, const PatternFinish& f)
{
    Body result;
    result.id = doc.allocateId();
    result.name = "Pattern";
    result.boolean = patternSpec(doc, target, f);
    stack.execute(std::make_unique<CombineCommand>(result));
    return result.id;
}

}  // namespace

TEST_CASE("Pattern: copies of a whole shape in a row, a grid, a circle", "[pattern]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 10, 10, 10);
    PatternFinish f;
    f.spacing1 = 15;
    SECTION("Row")
    {
        const ObjectId id = repeat(doc, stack, box, f);
        CHECK_THAT(volumeOf(doc.shape(id)), WithinRel(3000.0, 1e-9));
        const Bounds b = doc.worldBounds(id);
        CHECK_THAT(b.max[0] - b.min[0], WithinAbs(40.0, 1e-3));
        CHECK(history::describe(doc, id) == "row of 3, 15 mm apart");
        const auto back = roundTrip(doc);
        CHECK(back->find(id)->boolean == doc.find(id)->boolean);
    }
    SECTION("Grid")
    {
        f.kind = PatternKind::Grid;
        f.spacing2 = 12;
        const ObjectId id = repeat(doc, stack, box, f);
        CHECK_THAT(volumeOf(doc.shape(id)), WithinRel(6000.0, 1e-9));
        const Bounds b = doc.worldBounds(id);
        CHECK_THAT(b.max[1] - b.min[1], WithinAbs(22.0, 1e-3));
    }
    SECTION("Circle")
    {
        f.kind = PatternKind::Circle;
        f.count = 4;
        f.centre = {-20, 0, 0};
        const ObjectId id = repeat(doc, stack, box, f);
        CHECK_THAT(volumeOf(doc.shape(id)), WithinRel(4000.0, 1e-9));
        const Bounds b = doc.worldBounds(id);
        CHECK_THAT(b.min[0], WithinAbs(-45.0, 1e-3));  // the one opposite: centred 40 to the left
        CHECK(history::describe(doc, id) == "circle of 4");
    }
    SECTION("Too many, or none, is refused")
    {
        f.count1 = 2000;
        Body result;
        result.id = doc.allocateId();
        result.boolean = patternSpec(doc, box, f);
        CHECK_THROWS_WITH(doc.previewCombine(*result.boolean), Catch::Matchers::ContainsSubstring("1000"));
        f.count1 = 3;
        f.feature = true;  // a plain box has no feature to repeat
        result.boolean = patternSpec(doc, box, f);
        CHECK_THROWS_WITH(doc.previewCombine(*result.boolean), Catch::Matchers::ContainsSubstring("can be repeated"));
    }
}

TEST_CASE("Pattern: a feature repeated on its part, and linked to it", "[pattern]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = addBox(stack, doc, 60, 40, 10);
    const double hole = pi * 4 * 4;  // diameter 4, 4 deep
    const ObjectId drilled = drill(doc, stack, box, 10, {{-20, 0}}, 4, 4);
    PatternFinish f;
    f.feature = true;
    f.spacing1 = 12;
    const ObjectId id = repeat(doc, stack, drilled, f);
    CHECK_THAT(60 * 40 * 10 - volumeOf(doc.shape(id)), WithinRel(3 * hole, 1e-6));
    CHECK(history::describe(doc, id) == "row of 3, 12 mm apart (Hole)");

    SECTION("Change the hole: the copies follow")
    {
        const ObjectId toolId = doc.find(drilled)->boolean->operands[1];
        Body wider = *doc.find(toolId);
        std::get<HoleSpec>(wider.spec).diameter = 6;
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(toolId), wider, "Wider"));
        CHECK_THAT(60 * 40 * 10 - volumeOf(doc.shape(id)), WithinRel(3 * pi * 9 * 4, 1e-6));
    }
    SECTION("Through all, in a circle")
    {
        Body through = *doc.find(drilled);
        through.boolean->throughAll = true;
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(drilled), through, "Through"));
        Body circle = *doc.find(id);
        circle.boolean->pattern->kind = PatternKind::Circle;
        circle.boolean->pattern->centre = {0, 0, 5};
        circle.boolean->pattern->count = 6;  // 20 out from the middle: room for 6
        stack.execute(std::make_unique<EditBodyCommand>(*doc.find(id), circle, "Circle"));
        CHECK_THAT(60 * 40 * 10 - volumeOf(doc.shape(id)), WithinRel(6 * pi * 4 * 10, 1e-6));
    }
    SECTION("A Follow groove repeated")
    {
        Document d2;
        CommandStack s2(d2);
        const ObjectId plate = addBox(s2, d2, 40, 40, 10);
        FollowFinish g = onTop(10);
        g.origin = {0, -10, 10};
        line(g.path, sketch::addPoint(g.path, {-10, 0}), sketch::addPoint(g.path, {10, 0}));
        ObjectId grooved = kInvalidId;
        removed(d2, s2, plate, g, &grooved);
        PatternFinish r;
        r.feature = true;
        r.dir1 = {0, 1, 0};
        r.spacing1 = 10;
        const ObjectId rows = repeat(d2, s2, grooved, r);
        CHECK_THAT(40 * 40 * 10 - volumeOf(d2.shape(rows)), WithinRel(3 * 40.0, 1e-2));
    }
}
