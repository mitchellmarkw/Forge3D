// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 28: sketch tools (round corner, trim, offset, mirror).
#include "forge/core/Sketch.h"
#include "forge/core/SketchEdit.h"
#include "forge/core/SketchShape.h"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace forge::sketch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double pi = std::numbers::pi;

int line(Sketch& s, int a, int b, bool helper = false)
{
    s.geos.push_back({GeoKind::Line, a, b, -1, -1, helper});
    return int(s.geos.size()) - 1;
}

int circle(Sketch& s, Point centre, double r)
{
    const int c = addPoint(s, centre);
    s.radii.push_back(r);
    s.geos.push_back({GeoKind::Circle, c, -1, -1, int(s.radii.size()) - 1, false});
    return int(s.geos.size()) - 1;
}

// A w × h rectangle from (x, y), drawn as the Rectangle tool does (level and upright
// sides). Returns its corner points (bottom left, bottom right, top right, top left).
std::array<int, 4> rectangle(Sketch& s, double x, double y, double w, double h)
{
    const int a = addPoint(s, {x, y}), b = addPoint(s, {x + w, y}), c = addPoint(s, {x + w, y + h}),
              d = addPoint(s, {x, y + h});
    const int l0 = line(s, a, b), l1 = line(s, b, c), l2 = line(s, c, d), l3 = line(s, d, a);
    s.constraints.push_back(makeRule(s, Rule::Horizontal, l0));
    s.constraints.push_back(makeRule(s, Rule::Vertical, l1));
    s.constraints.push_back(makeRule(s, Rule::Horizontal, l2));
    s.constraints.push_back(makeRule(s, Rule::Vertical, l3));
    return {a, b, c, d};
}

double area(const Sketch& s)
{
    GProp_GProps props;
    BRepGProp::SurfaceProperties(faces(s), props);
    return props.Mass();
}

void box(const Sketch& s, double& x0, double& y0, double& x1, double& y1, bool helpersToo = false)
{
    x0 = y0 = 1e300;
    x1 = y1 = -1e300;
    for (const Geo& g : s.geos) {
        if (g.construction && !helpersToo)
            continue;
        const auto take = [&](Point p) {
            x0 = std::min(x0, p.x);
            y0 = std::min(y0, p.y);
            x1 = std::max(x1, p.x);
            y1 = std::max(y1, p.y);
        };
        if (g.kind == GeoKind::Circle) {
            const Point c = s.points[std::size_t(g.a)];
            const double r = radiusOf(s, g);
            take({c.x - r, c.y - r});
            take({c.x + r, c.y + r});
        } else {
            take(s.points[std::size_t(g.a)]);
            take(s.points[std::size_t(g.b)]);
            if (g.kind == GeoKind::Arc)
                take(s.points[std::size_t(g.c)]);
        }
    }
}

}  // namespace

TEST_CASE("Round corner: an arc where two lines meet", "[sketchedit]")
{
    Sketch s;
    const auto corners = rectangle(s, 0, 0, 20, 10);
    // Pinned down: the bottom side 20 long, 10 high, its first corner locked.
    s.constraints.push_back(makeRule(s, Rule::Distance, 0));
    s.constraints.push_back(makeRule(s, Rule::Fixed, corners[0]));
    s.constraints.push_back(makeRule(s, Rule::DistanceY, corners[0], corners[3]));
    const int before = freedom(s);
    roundCorner(s, corners[2], 3.0);
    const Solved solved = solve(s);
    CHECK(solved.ok);
    CHECK(solved.freedom == before);  // nothing more (or less) held than before
    CHECK(looseEnds(s).empty());
    CHECK(loosePoints(s).empty());
    // A quarter circle of radius 3 in place of the corner.
    CHECK_THAT(area(s), WithinRel(200 - 9 * (1 - pi / 4), 1e-6));
    // The corner stays where it was, on helper lines: a size up to it still holds.
    CHECK_THAT(s.points[std::size_t(corners[2])].x, WithinAbs(20.0, 1e-9));
    CHECK_THAT(s.points[std::size_t(corners[2])].y, WithinAbs(10.0, 1e-9));
    // Round the next corner too, then make the first one bigger.
    roundCorner(s, corners[1], 2.0);
    CHECK(solve(s).ok);
    CHECK_THAT(area(s), WithinRel(200 - (9 + 4) * (1 - pi / 4), 1e-6));
    for (Constraint& c : s.constraints)
        if (c.rule == Rule::Radius && std::abs(c.value - 3.0) < 1e-9)
            c.value = 4.0;
    CHECK(solve(s).ok);
    CHECK_THAT(area(s), WithinRel(200 - (16 + 4) * (1 - pi / 4), 1e-6));

    // A length on a rounded line: now from its far end to the corner (so still 20 wide).
    bool converted = false;
    for (const Constraint& c : s.constraints)
        converted = converted || (c.rule == Rule::Distance && c.b >= 0 && std::abs(c.value - 20.0) < 1e-9);
    CHECK(converted);

    // Not a corner of two lines, or too big: told so, nothing changed.
    Sketch t;
    const auto k = rectangle(t, 0, 0, 10, 4);
    const Sketch kept = t;
    CHECK_THROWS_AS(roundCorner(t, k[0], 5.0), std::invalid_argument);  // more than 4 fits
    CHECK(t == kept);
    CHECK_THROWS_AS(roundCorner(t, 0, 1.0), std::invalid_argument);  // the origin: no lines there
    Sketch slant;
    const int a = addPoint(slant, {0, 0}), b = addPoint(slant, {10, 0}), c = addPoint(slant, {20, 0});
    line(slant, a, b);
    line(slant, b, c);
    CHECK_THROWS_AS(roundCorner(slant, b, 1.0), std::invalid_argument);  // straight on
}

TEST_CASE("Trim: takes the piece clicked back to where other curves cross", "[sketchedit]")
{
    SECTION("A cross: one arm off, then the middle of a line crossed twice")
    {
        Sketch s;
        const int a = addPoint(s, {-7, 2}), b = addPoint(s, {13, 2});
        const int c = addPoint(s, {3, -8}), d = addPoint(s, {3, 12});
        const int across = line(s, a, b);
        line(s, c, d);
        auto piece = trimPiece(s, across, {10, 2.1});
        REQUIRE(piece);
        CHECK_THAT(piece->path.front().x, WithinAbs(3.0, 1e-9));
        CHECK_THAT(piece->path.back().x, WithinAbs(13.0, 1e-9));
        trim(s, across, {10, 2.1});
        CHECK(solve(s).ok);
        const Geo& g = s.geos[std::size_t(across)];
        CHECK_THAT(s.points[std::size_t(g.b)].x, WithinAbs(3.0, 1e-9));  // ends at the crossing
        // Held there: moving the upright line takes the end with it.
        bool held = false;
        for (const Constraint& r : s.constraints)
            held = held || (r.rule == Rule::PointOn && r.a == g.b);
        CHECK(held);

        Sketch t;
        const int p = addPoint(t, {0, 0}), q = addPoint(t, {30, 0});
        const int base = line(t, p, q);
        for (double x : {10.0, 20.0})
            line(t, addPoint(t, {x, -5}), addPoint(t, {x, 5}));
        t.constraints.push_back(makeRule(t, Rule::Horizontal, base));
        trim(t, base, {15, 0});
        CHECK(solve(t).ok);
        REQUIRE(t.geos.size() == 4);  // split in two
        const Geo& left = t.geos[std::size_t(base)];
        const Geo& right = t.geos.back();
        CHECK_THAT(t.points[std::size_t(left.b)].x, WithinAbs(10.0, 1e-9));
        CHECK_THAT(t.points[std::size_t(right.a)].x, WithinAbs(20.0, 1e-9));
        CHECK_THAT(t.points[std::size_t(right.b)].x, WithinAbs(30.0, 1e-9));
        // The two pieces stay in line: move one end up and both end up level.
        t.points[std::size_t(left.a)].y = 2.0;
        CHECK(solve(t).ok);
        CHECK_THAT(t.points[std::size_t(right.a)].y, WithinAbs(t.points[std::size_t(left.a)].y, 1e-6));
    }

    SECTION("A circle cut by a line becomes an arc; a lone line goes")
    {
        Sketch s;
        const int round = circle(s, {0, 0}, 5);
        line(s, addPoint(s, {-10, 3}), addPoint(s, {10, 3}));
        trim(s, round, {0, 5});  // the cap above the line
        CHECK(solve(s).ok);
        const Geo& g = s.geos[std::size_t(round)];
        REQUIRE(g.kind == GeoKind::Arc);
        CHECK(s.radii.empty());
        CHECK_THAT(radiusOf(s, g), WithinAbs(5.0, 1e-9));
        CHECK_THAT(s.points[std::size_t(g.b)].y, WithinAbs(3.0, 1e-9));
        CHECK_THAT(s.points[std::size_t(g.c)].y, WithinAbs(3.0, 1e-9));
        CHECK(s.points[std::size_t(g.b)].x < 0);  // round from the left, under, to the right

        Sketch t;
        const int alone = line(t, addPoint(t, {0, 0}), addPoint(t, {5, 5}));
        trim(t, alone, {2, 2});
        CHECK(t.geos.empty());
        CHECK(t.points.size() == 1);
    }
}

TEST_CASE("Offset: a copy of an outline, further out or in", "[sketchedit]")
{
    SECTION("A rectangle out and in")
    {
        Sketch s;
        rectangle(s, 0, 0, 20, 10);
        const auto made = offset(s, {1}, 2.0);
        CHECK(made.size() == 4);
        CHECK(solve(s).ok);
        double x0, y0, x1, y1;
        box(s, x0, y0, x1, y1);
        CHECK_THAT(x0, WithinAbs(-2.0, 1e-9));
        CHECK_THAT(y1, WithinAbs(12.0, 1e-9));
        CHECK_THAT(area(s), WithinRel(24.0 * 14.0 - 200.0, 1e-6));  // a frame between them

        Sketch t;
        rectangle(t, 0, 0, 20, 10);
        offset(t, {0}, -2.0);
        CHECK(solve(t).ok);
        CHECK_THAT(area(t), WithinRel(200.0 - 16.0 * 6.0, 1e-6));
        // The gap is held: four Gap dimensions (a sharp-cornered outline: one a side),
        // and nothing more can move than before the offset.
        int gaps = 0;
        for (const Constraint& c : t.constraints)
            gaps += c.rule == Rule::Gap;
        CHECK(gaps == 4);
        {
            Sketch plain;
            rectangle(plain, 0, 0, 20, 10);
            CHECK(freedom(t) == freedom(plain));
        }
        // Drag the original's corner a long way: the copy keeps its distance.
        solve(t, {{3, Point{40, 25}}});
        CHECK(solve(t).ok);
        const double w = t.points[3].x - t.points[1].x, h = t.points[3].y - t.points[1].y;
        CHECK(w > 30);
        CHECK_THAT(area(t), WithinRel(w * h - (w - 4) * (h - 4), 1e-6));
        // Change one gap: that side moves.
        for (Constraint& c : t.constraints)
            if (c.rule == Rule::Gap) {
                c.value = c.value < 0 ? -1.0 : 1.0;
                break;
            }
        CHECK(solve(t).ok);
        Sketch far;
        rectangle(far, 0, 0, 20, 10);
        CHECK_THROWS_AS(offset(far, {0}, -6.0), std::invalid_argument);  // more than half of 10
    }

    SECTION("A slot, a circle and an open line")
    {
        // A slot: two half circles joined by lines (as the Slot tool makes it).
        Sketch s;
        const int k1 = addPoint(s, {0, 0}), k2 = addPoint(s, {10, 0});
        const int a1 = addPoint(s, {0, 2}), b1 = addPoint(s, {10, 2}), a2 = addPoint(s, {10, -2}),
                  b2 = addPoint(s, {0, -2});
        const int top = line(s, a1, b1), bottom = line(s, a2, b2);
        s.geos.push_back({GeoKind::Arc, k2, a2, b1, -1, false});
        s.geos.push_back({GeoKind::Arc, k1, a1, b2, -1, false});
        for (int arc : {2, 3})
            for (int l : {top, bottom})
                s.constraints.push_back(makeRule(s, Rule::Tangent, l, arc));
        const int free = freedom(s);
        offset(s, {top}, 1.0);
        CHECK(solve(s).ok);
        // Joined smoothly all round: one Gap holds the lot.
        int gaps = 0;
        for (const Constraint& c : s.constraints)
            gaps += c.rule == Rule::Gap;
        CHECK(gaps == 1);
        CHECK(freedom(s) == free);
        double x0, y0, x1, y1;
        box(s, x0, y0, x1, y1);
        CHECK_THAT(y1, WithinAbs(3.0, 1e-9));
        const double inner = pi * 4 + 40, outer = pi * 9 + 60;
        CHECK_THAT(area(s), WithinRel(outer - inner, 1e-3));

        Sketch c;
        const int round = circle(c, {5, 5}, 4);
        const auto made = offset(c, {round}, -1.0);
        REQUIRE(made.size() == 1);
        CHECK_THAT(radiusOf(c, c.geos[std::size_t(made[0])]), WithinAbs(3.0, 1e-12));
        CHECK(c.geos[std::size_t(made[0])].a == c.geos[std::size_t(round)].a);  // same centre
        c.radii[0] = 10;  // the original grows: the copy stays 1 inside
        CHECK(solve(c).ok);
        CHECK_THAT(radiusOf(c, c.geos[std::size_t(made[0])]), WithinAbs(radiusOf(c, c.geos[std::size_t(round)]) - 1.0, 1e-6));

        // An L, open: copied to its left (going from its first point).
        Sketch l;
        const int p = addPoint(l, {0, 0}), q = addPoint(l, {10, 0}), r = addPoint(l, {10, 10});
        line(l, p, q);
        line(l, q, r);
        offset(l, {0}, 1.0);
        CHECK(solve(l).ok);
        bool corner = false;
        for (const Point& pt : l.points)
            corner = corner || (std::abs(pt.x - 9) < 1e-9 && std::abs(pt.y - 1) < 1e-9);
        CHECK(corner);
    }
}

TEST_CASE("Mirror: a copy the other side of a line, held mirror-image", "[sketchedit]")
{
    Sketch s;
    // An upright helper line through the origin, and half a shape against it.
    const int top = addPoint(s, {0, 20});
    const int axis = line(s, 0, top, true);
    s.constraints.push_back(makeRule(s, Rule::Vertical, axis));
    const int a = addPoint(s, {0, 5}), b = addPoint(s, {8, 5}), c = addPoint(s, {0, 12});
    // Half a kite: open along the line, which the mirror closes.
    const int l0 = line(s, a, b), l1 = line(s, b, c);
    const int round = circle(s, {2, 7}, 1);
    const std::size_t pointsBefore = s.points.size();
    const auto made = mirror(s, {l0, l1, round, axis}, {}, axis);
    CHECK(made.size() == 3);
    CHECK(solve(s).ok);
    // Points on the line are shared; the others copied.
    CHECK(s.points.size() == pointsBefore + 2);
    CHECK(looseEnds(s).empty());
    double x0, y0, x1, y1;
    box(s, x0, y0, x1, y1);
    CHECK_THAT(x0, WithinAbs(-8.0, 1e-9));
    // A kite (two triangles joined along the line), and a hole each side.
    CHECK_THAT(area(s), WithinRel(2 * (8.0 * 7.0 / 2 - pi), 1e-6));
    // Move the original corner; the copy follows.
    s.points[std::size_t(b)] = {10, 6};
    CHECK(solve(s).ok);
    const Point moved = s.points[std::size_t(b)];
    CHECK(moved.x > 8.5);
    bool follows = false;
    for (const Point& p : s.points)
        follows = follows || (std::abs(p.x + moved.x) < 1e-6 && std::abs(p.y - moved.y) < 1e-6);
    CHECK(follows);

    // An arc mirrored still runs the right way round.
    Sketch t;
    const int up = line(t, 0, addPoint(t, {0, 10}), true);
    const int centre = addPoint(t, {5, 0});
    t.geos.push_back({GeoKind::Arc, centre, addPoint(t, {7, 0}), addPoint(t, {5, 2}), -1, false});
    const auto copy = mirror(t, {1}, {}, up);
    CHECK(solve(t).ok);
    const Geo& g = t.geos[std::size_t(copy[0])];
    CHECK_THAT(t.points[std::size_t(g.b)].x, WithinAbs(-5.0, 1e-9));  // starts at the top
    CHECK_THAT(t.points[std::size_t(g.c)].x, WithinAbs(-7.0, 1e-9));
    CHECK_THROWS_AS(mirror(t, {1}, {}, 1), std::invalid_argument);  // not a line

    // Half a shape whose ends just miss the line: put on it, so the outline closes.
    Sketch h;
    const int ax = line(h, 0, addPoint(h, {0, 20}), true);
    h.constraints.push_back(makeRule(h, Rule::Vertical, ax));
    const int p0 = addPoint(h, {0.3, 2}), p1 = addPoint(h, {10, 2}), p2 = addPoint(h, {10, 12}), p3 = addPoint(h, {0.2, 12});
    const int m0 = line(h, p0, p1), m1 = line(h, p1, p2), m2 = line(h, p2, p3);
    mirror(h, {m0, m1, m2}, {}, ax);
    CHECK(solve(h).ok);
    CHECK(looseEnds(h).empty());
    CHECK_THAT(h.points[std::size_t(p0)].x, WithinAbs(0.0, 1e-6));
    CHECK_THAT(area(h), WithinRel(200.0, 1e-6));

    // A mirror line leaning a little: made upright, the ends drawn on it going with it.
    Sketch lean;
    const int tip = addPoint(lean, {3, 25});
    const int la = line(lean, 0, tip, true);
    const int q0 = addPoint(lean, {0.6, 5}), q1 = addPoint(lean, {18, 5}), q2 = addPoint(lean, {18, 20}),
              q3 = addPoint(lean, {2.4, 20});
    const int n0 = line(lean, q0, q1), n1 = line(lean, q1, q2), n2 = line(lean, q2, q3);
    lean.constraints.push_back(makeRule(lean, Rule::Horizontal, n0));
    lean.constraints.push_back(makeRule(lean, Rule::Vertical, n1));
    lean.constraints.push_back(makeRule(lean, Rule::Horizontal, n2));
    CHECK(straighten(lean, {n0, n1, n2}, la));
    CHECK(solve(lean).ok);
    CHECK_THAT(lean.points[std::size_t(tip)].x, WithinAbs(0.0, 1e-6));
    CHECK(lean.points[std::size_t(tip)].y > 24);  // still about as long, still going up
    mirror(lean, {n0, n1, n2}, {}, la);
    CHECK(solve(lean).ok);
    CHECK(looseEnds(lean).empty());
    CHECK_THAT(area(lean), WithinRel(36.0 * 15.0, 0.02));
    CHECK_FALSE(straighten(lean, {}, la));  // already upright
    Sketch slanted;
    const int sl = line(slanted, 0, addPoint(slanted, {10, 10}), true);
    CHECK_FALSE(straighten(slanted, {}, sl));  // really at an angle: left alone
}
