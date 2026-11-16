// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/SketchEdit.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>

namespace forge::sketch {

namespace {

constexpr double pi = std::numbers::pi;

Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
Point operator*(Point a, double k) { return {a.x * k, a.y * k}; }
double dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }
double cross(Point a, Point b) { return a.x * b.y - a.y * b.x; }
double len(Point a) { return std::hypot(a.x, a.y); }
Point unit(Point a)
{
    const double l = len(a);
    return l > 0 ? a * (1.0 / l) : Point{};
}
Point perp(Point a) { return {-a.y, a.x}; }  // a quarter turn anticlockwise

double angleOf(Point v)
{
    const double a = std::atan2(v.y, v.x);
    return a < 0 ? a + 2 * pi : a;
}
// Anticlockwise from a0 to a1, in [0, 2π).
double ccw(double a0, double a1)
{
    double d = std::fmod(a1 - a0, 2 * pi);
    return d < 0 ? d + 2 * pi : d;
}

Point at(const Sketch& s, int i) { return s.points[std::size_t(i)]; }

// How far apart counts as the same place: tiny next to the sketch's size.
double tolerance(const Sketch& s)
{
    double big = 1.0;
    for (const Point& p : s.points)
        big = std::max({big, std::abs(p.x), std::abs(p.y)});
    for (double r : s.radii)
        big = std::max(big, std::abs(r));
    return 1e-7 * big;
}

// Where along a curve: a line from 0 (a) to 1 (b); a circle by angle; an arc by the
// angle round from its start.
double spanOf(const Sketch& s, const Geo& g)
{
    if (g.kind == GeoKind::Line)
        return 1.0;
    if (g.kind == GeoKind::Circle)
        return 2 * pi;
    const Point c = at(s, g.a);
    const double span = ccw(angleOf(at(s, g.b) - c), angleOf(at(s, g.c) - c));
    return span > 0 ? span : 2 * pi;
}

double paramOf(const Sketch& s, const Geo& g, Point p)
{
    if (g.kind == GeoKind::Line) {
        const Point a = at(s, g.a), d = at(s, g.b) - a;
        const double l2 = dot(d, d);
        return l2 > 0 ? dot(p - a, d) / l2 : 0.0;
    }
    const Point c = at(s, g.a);
    if (g.kind == GeoKind::Circle)
        return angleOf(p - c);
    double t = ccw(angleOf(at(s, g.b) - c), angleOf(p - c));
    // Just before the start: count as at the start (not nearly all the way round).
    if (t > spanOf(s, g) && t > pi + spanOf(s, g) / 2)
        t -= 2 * pi;
    return t;
}

Point pointAt(const Sketch& s, const Geo& g, double t)
{
    if (g.kind == GeoKind::Line) {
        const Point a = at(s, g.a);
        return a + (at(s, g.b) - a) * t;
    }
    const Point c = at(s, g.a);
    const double r = radiusOf(s, g);
    const double a0 = g.kind == GeoKind::Circle ? 0.0 : angleOf(at(s, g.b) - c);
    return {c.x + r * std::cos(a0 + t), c.y + r * std::sin(a0 + t)};
}

// Whether a point already on the curve's line or circle is within the curve itself.
bool within(const Sketch& s, const Geo& g, Point p, double tol)
{
    if (g.kind == GeoKind::Circle)
        return true;
    const double t = paramOf(s, g, p);
    const double scale = g.kind == GeoKind::Line ? len(at(s, g.b) - at(s, g.a)) : radiusOf(s, g);
    const double e = scale > 0 ? tol / scale : 0.0;
    return t >= -e && t <= spanOf(s, g) + e;
}

// Where the full line / circle of g meets that of h.
std::vector<Point> crossings(const Sketch& s, const Geo& g, const Geo& h, double tol)
{
    const auto lineLine = [&](const Geo& l1, const Geo& l2) -> std::vector<Point> {
        const Point p = at(s, l1.a), r = at(s, l1.b) - p;
        const Point q = at(s, l2.a), u = at(s, l2.b) - q;
        const double d = cross(r, u);
        if (std::abs(d) <= 1e-12 * len(r) * len(u))
            return {};  // parallel
        return {p + r * (cross(q - p, u) / d)};
    };
    const auto lineCircle = [&](const Geo& l, const Geo& c) -> std::vector<Point> {
        const Point p = at(s, l.a), d = unit(at(s, l.b) - p);
        const Point m = at(s, c.a);
        const double r = radiusOf(s, c);
        const double along = dot(m - p, d);
        const Point foot = p + d * along;
        const double off = len(m - foot);
        if (off > r + tol)
            return {};
        const double half = std::sqrt(std::max(r * r - off * off, 0.0));
        if (half <= tol)
            return {foot};  // just touching
        return {foot - d * half, foot + d * half};
    };
    const auto circleCircle = [&](const Geo& c1, const Geo& c2) -> std::vector<Point> {
        const Point m1 = at(s, c1.a), m2 = at(s, c2.a);
        const double r1 = radiusOf(s, c1), r2 = radiusOf(s, c2);
        const double d = len(m2 - m1);
        if (d <= tol || d > r1 + r2 + tol || d < std::abs(r1 - r2) - tol)
            return {};
        const double a = (d * d + r1 * r1 - r2 * r2) / (2 * d);
        const double h2 = r1 * r1 - a * a;
        const Point u = (m2 - m1) * (1.0 / d);
        const Point base = m1 + u * a;
        if (h2 <= tol * tol)
            return {base};
        const double hh = std::sqrt(h2);
        return {base + perp(u) * hh, base - perp(u) * hh};
    };
    const bool gl = g.kind == GeoKind::Line, hl = h.kind == GeoKind::Line;
    if (gl && hl)
        return lineLine(g, h);
    if (gl)
        return lineCircle(g, h);
    if (hl)
        return lineCircle(h, g);
    return circleCircle(g, h);
}

struct Cut {
    double t = 0.0;  // along the curve being cut
    Point at;
    int by = -1;  // the curve that crosses it
};

// Places inside curve g where other curves cross or touch it, in order along it.
std::vector<Cut> cutsOf(const Sketch& s, int geo, double tol)
{
    const Geo& g = s.geos[std::size_t(geo)];
    const double span = spanOf(s, g);
    const double scale = g.kind == GeoKind::Line ? len(at(s, g.b) - at(s, g.a)) : radiusOf(s, g);
    const double e = scale > 0 ? 10 * tol / scale : 0.0;
    std::vector<Cut> cuts;
    for (std::size_t h = 0; h < s.geos.size(); ++h) {
        if (int(h) == geo)
            continue;
        for (const Point& p : crossings(s, g, s.geos[h], tol)) {
            if (!within(s, s.geos[h], p, 10 * tol))
                continue;
            const double t = paramOf(s, g, p);
            if (g.kind != GeoKind::Circle && (t <= e || t >= span - e))
                continue;  // at (or beyond) its own ends
            cuts.push_back({t, p, int(h)});
        }
    }
    std::sort(cuts.begin(), cuts.end(), [](const Cut& a, const Cut& b) { return a.t < b.t; });
    // The same place twice (two curves crossing there): once.
    std::vector<Cut> out;
    for (const Cut& c : cuts)
        if (out.empty() || c.t - out.back().t > e)
            out.push_back(c);
    if (g.kind == GeoKind::Circle && out.size() > 1 && out.front().t + 2 * pi - out.back().t <= e)
        out.pop_back();
    return out;
}

// What trimming curve `geo` at `p` takes away: all of it, or from cut lo to cut hi
// (either missing: from the curve's own end).
struct Plan {
    bool whole = false;
    std::optional<Cut> lo, hi;
    double from = 0.0, to = 0.0;  // along the curve
};

Plan planTrim(const Sketch& s, int geo, Point p, double tol)
{
    const Geo& g = s.geos[std::size_t(geo)];
    const std::vector<Cut> cuts = cutsOf(s, geo, tol);
    Plan plan;
    const double span = spanOf(s, g);
    double t = std::clamp(paramOf(s, g, p), 0.0, span);
    if (g.kind == GeoKind::Circle) {
        if (cuts.size() < 2) {
            plan.whole = true;
            plan.to = span;
            return plan;
        }
        std::size_t k = 0;
        while (k < cuts.size() && cuts[k].t < t)
            ++k;
        plan.hi = k < cuts.size() ? cuts[k] : cuts.front();
        plan.lo = k > 0 ? cuts[k - 1] : cuts.back();
        plan.from = plan.lo->t;
        plan.to = plan.hi->t;
        if (plan.to <= plan.from)
            plan.to += 2 * pi;
        return plan;
    }
    for (const Cut& c : cuts) {
        if (c.t < t)
            plan.lo = c;
        else if (!plan.hi)
            plan.hi = c;
    }
    plan.whole = !plan.lo && !plan.hi;
    plan.from = plan.lo ? plan.lo->t : 0.0;
    plan.to = plan.hi ? plan.hi->t : span;
    return plan;
}

bool inUse(const Sketch& s, int p)
{
    if (p == 0)
        return true;
    for (const Geo& g : s.geos)
        if (g.a == p || (g.kind != GeoKind::Circle && g.b == p) || (g.kind == GeoKind::Arc && g.c == p))
            return true;
    return false;
}

// A point for the end made by a cut: one already there, or a new one held on the
// curve that made the cut.
int cutPoint(Sketch& s, const Cut& cut, const Geo& own, double tol)
{
    for (std::size_t i = 0; i < s.points.size(); ++i) {
        const int p = int(i);
        if (p == own.a || p == own.b || (own.kind == GeoKind::Arc && p == own.c))
            continue;
        if (len(s.points[i] - cut.at) <= 10 * tol)
            return p;
    }
    const int p = addPoint(s, cut.at);
    s.constraints.push_back(Constraint{Rule::PointOn, p, cut.by, -1, 0.0, 0.0});
    return p;
}

// Tangent rules on `geo` that worked at its end `end` (now gone from it): moved to
// `to`, or dropped (to < 0).
void moveJointRules(Sketch& s, int geo, int end, int to)
{
    std::vector<Constraint> kept;
    for (Constraint c : s.constraints) {
        if (c.rule == Rule::Tangent && (c.a == geo || c.b == geo)) {
            const int other = c.a == geo ? c.b : c.a;
            const Geo& o = s.geos[std::size_t(other)];
            const bool atEnd = o.a == end || (o.kind != GeoKind::Circle && o.b == end) ||
                               (o.kind == GeoKind::Arc && o.c == end);
            if (atEnd && o.kind != GeoKind::Circle) {
                if (to < 0)
                    continue;
                (c.a == geo ? c.a : c.b) = to;
            }
        }
        kept.push_back(c);
    }
    s.constraints = std::move(kept);
}

// Lengths on a line, and Equal (length) rules with it: no longer meant once it's cut.
void dropLengthRules(Sketch& s, int geo)
{
    std::vector<Constraint> kept;
    for (const Constraint& c : s.constraints) {
        if (c.rule == Rule::Distance && c.b < 0 && c.a == geo)
            continue;
        if (c.rule == Rule::Equal && (c.a == geo || c.b == geo))
            continue;
        kept.push_back(c);
    }
    s.constraints = std::move(kept);
}

void dropIfLoose(Sketch& s, std::vector<int> points)
{
    std::vector<int> loose;
    for (int p : points)
        if (p > 0 && std::size_t(p) < s.points.size() && !inUse(s, p))
            loose.push_back(p);
    if (!loose.empty())
        removePoints(s, loose);
}

}  // namespace

// ---- Round a corner ------------------------------------------------------------------------

void roundCorner(Sketch& s, int point, double radius)
{
    if (point < 0 || std::size_t(point) >= s.points.size())
        throw std::invalid_argument("Pick a corner to round");
    if (!(radius > 0))
        throw std::invalid_argument("The radius must be more than 0");
    std::vector<int> lines;
    for (std::size_t i = 0; i < s.geos.size(); ++i) {
        const Geo& g = s.geos[i];
        if (g.construction)
            continue;
        if (g.kind == GeoKind::Line && (g.a == point || g.b == point))
            lines.push_back(int(i));
        else if (g.kind == GeoKind::Arc && (g.b == point || g.c == point))
            throw std::invalid_argument("Round corner works where two straight lines meet");
    }
    if (lines.size() != 2)
        throw std::invalid_argument("Round corner works where two straight lines meet");
    const int l1 = lines[0], l2 = lines[1];
    const auto farEnd = [&](int l) {
        const Geo& g = s.geos[std::size_t(l)];
        return g.a == point ? g.b : g.a;
    };
    const int e1 = farEnd(l1), e2 = farEnd(l2);
    const Point p = at(s, point);
    const double len1 = len(at(s, e1) - p), len2 = len(at(s, e2) - p);
    const Point u1 = unit(at(s, e1) - p), u2 = unit(at(s, e2) - p);
    const double theta = std::acos(std::clamp(dot(u1, u2), -1.0, 1.0));
    if (theta < pi / 180 || theta > pi * 179 / 180)
        throw std::invalid_argument("Those lines carry straight on, so there's no corner to round");
    const double back = radius / std::tan(theta / 2);  // from the corner to where the arc starts
    if (back >= std::min(len1, len2) * (1 - 1e-9)) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "That radius is too big for these lines (up to %.2f mm fits)",
                      std::min(len1, len2) * std::tan(theta / 2));
        throw std::invalid_argument(buf);
    }
    const Point t1 = p + u1 * back, t2 = p + u2 * back;
    const Point centre = p + unit(u1 + u2) * (radius / std::sin(theta / 2));

    // A length on either line: now from its far end to the corner.
    for (Constraint& c : s.constraints)
        if (c.rule == Rule::Distance && c.b < 0 && (c.a == l1 || c.a == l2))
            c = Constraint{Rule::Distance, c.a == l1 ? e1 : e2, point, -1, c.value, 0.0};
    dropLengthRules(s, l1);
    dropLengthRules(s, l2);

    const int k1 = addPoint(s, t1), k2 = addPoint(s, t2), m = addPoint(s, centre);
    for (const auto& [l, k] : {std::pair{l1, k1}, std::pair{l2, k2}}) {
        Geo& g = s.geos[std::size_t(l)];
        (g.a == point ? g.a : g.b) = k;
    }
    // The corner stays, on helper lines in line with the two lines.
    s.geos.push_back({GeoKind::Line, point, k1, -1, -1, true});
    const int h1 = int(s.geos.size()) - 1;
    s.geos.push_back({GeoKind::Line, point, k2, -1, -1, true});
    const int h2 = int(s.geos.size()) - 1;
    // Anticlockwise from start to end, the short way round.
    const bool forward = cross(t1 - centre, t2 - centre) > 0;
    s.geos.push_back({GeoKind::Arc, m, forward ? k1 : k2, forward ? k2 : k1, -1, false});
    const int arc = int(s.geos.size()) - 1;
    s.constraints.push_back(makeRule(s, Rule::Parallel, h1, l1));
    s.constraints.push_back(makeRule(s, Rule::Parallel, h2, l2));
    s.constraints.push_back(makeRule(s, Rule::Tangent, l1, arc));
    s.constraints.push_back(makeRule(s, Rule::Tangent, l2, arc));
    Constraint r = makeRule(s, Rule::Radius, arc);
    r.value = radius;
    s.constraints.push_back(r);
}

// ---- Trim -------------------------------------------------------------------------------------

std::optional<TrimPiece> trimPiece(const Sketch& s, int geo, Point p)
{
    if (geo < 0 || std::size_t(geo) >= s.geos.size())
        return std::nullopt;
    const Geo& g = s.geos[std::size_t(geo)];
    const Plan plan = planTrim(s, geo, p, tolerance(s));
    TrimPiece piece;
    piece.geo = geo;
    const int steps = g.kind == GeoKind::Line ? 1 : std::max(2, int(std::ceil((plan.to - plan.from) / (pi / 48))));
    for (int i = 0; i <= steps; ++i)
        piece.path.push_back(pointAt(s, g, plan.from + (plan.to - plan.from) * i / steps));
    return piece;
}

void trim(Sketch& s, int geo, Point p)
{
    if (geo < 0 || std::size_t(geo) >= s.geos.size())
        throw std::invalid_argument("Click the piece of a line or curve to trim");
    const double tol = tolerance(s);
    const Plan plan = planTrim(s, geo, p, tol);
    if (plan.whole) {
        removeGeos(s, {geo});
        return;
    }
    const Geo g = s.geos[std::size_t(geo)];
    const auto self = [&s, geo]() -> Geo& { return s.geos[std::size_t(geo)]; };
    if (g.kind == GeoKind::Circle) {
        // What's left goes round from the far cut back to the near one.
        const int start = cutPoint(s, *plan.hi, g, tol);
        const int end = cutPoint(s, *plan.lo, g, tol);
        self() = Geo{GeoKind::Arc, g.a, start, end, -1, g.construction};
        removeGeos(s, {});  // tidies away the circle's radius
        return;
    }
    // Lines run a to b, arcs b to c: the two ends in order along the curve.
    const int first = g.kind == GeoKind::Line ? g.a : g.b;
    const int last = g.kind == GeoKind::Line ? g.b : g.c;
    const auto setFirst = [&](int q) { (g.kind == GeoKind::Line ? self().a : self().b) = q; };
    const auto setLast = [&](int q) { (g.kind == GeoKind::Line ? self().b : self().c) = q; };
    if (g.kind == GeoKind::Line)
        dropLengthRules(s, geo);
    if (!plan.lo) {
        const int q = cutPoint(s, *plan.hi, g, tol);
        setFirst(q);
        moveJointRules(s, geo, first, -1);
        dropIfLoose(s, {first});
        return;
    }
    if (!plan.hi) {
        const int q = cutPoint(s, *plan.lo, g, tol);
        setLast(q);
        moveJointRules(s, geo, last, -1);
        dropIfLoose(s, {last});
        return;
    }
    // From the middle: two pieces, the second a new curve.
    const int q1 = cutPoint(s, *plan.lo, g, tol);
    const int q2 = cutPoint(s, *plan.hi, g, tol);
    if (g.kind == GeoKind::Line)
        s.geos.push_back({GeoKind::Line, q2, last, -1, -1, g.construction});
    else
        s.geos.push_back({GeoKind::Arc, g.a, q2, last, -1, g.construction});
    const int second = int(s.geos.size()) - 1;
    setLast(q1);
    moveJointRules(s, geo, last, second);
    if (g.kind == GeoKind::Line) {
        s.constraints.push_back(makeRule(s, Rule::Parallel, geo, second));
        s.constraints.push_back(Constraint{Rule::PointOn, q2, geo, -1, 0.0, 0.0});
    } else {
        s.constraints.push_back(makeRule(s, Rule::Equal, geo, second));
    }
}

// ---- Offset -----------------------------------------------------------------------------------

namespace {

struct Step {
    int geo = -1;
    int from = -1, to = -1;  // points, in the order walked
};

struct Chain {
    std::vector<Step> steps;
    bool closed = false;
};

std::array<int, 2> endsOf(const Geo& g)
{
    return g.kind == GeoKind::Line ? std::array<int, 2>{g.a, g.b} : std::array<int, 2>{g.b, g.c};
}

// The chain of joined lines and arcs that `start` is part of.
Chain chainOf(const Sketch& s, int start, std::set<int>& seen)
{
    const bool helper = s.geos[std::size_t(start)].construction;
    std::vector<int> members;
    std::vector<int> todo{start};
    std::set<int> in{start};
    while (!todo.empty()) {
        const int g = todo.back();
        todo.pop_back();
        members.push_back(g);
        for (int p : endsOf(s.geos[std::size_t(g)]))
            for (std::size_t h = 0; h < s.geos.size(); ++h) {
                const Geo& o = s.geos[h];
                if (o.kind == GeoKind::Circle || o.construction != helper || in.count(int(h)))
                    continue;
                const auto e = endsOf(o);
                if (e[0] == p || e[1] == p) {
                    in.insert(int(h));
                    todo.push_back(int(h));
                }
            }
    }
    seen.insert(in.begin(), in.end());
    std::map<int, int> degree;
    for (int g : members)
        for (int p : endsOf(s.geos[std::size_t(g)]))
            ++degree[p];
    int begin = endsOf(s.geos[std::size_t(members.front())])[0];
    bool closed = true;
    for (const auto& [p, d] : degree) {
        if (d > 2)
            throw std::invalid_argument(
                "Offset works on outlines that don't branch (here more than two lines meet at a point)");
        if (d == 1 && closed) {
            closed = false;
            begin = p;  // the lowest-numbered end: the first point drawn
        }
    }
    // Walk it from one end (or anywhere round a loop).
    Chain chain;
    chain.closed = closed;
    std::set<int> used;
    int here = begin;
    while (used.size() < members.size()) {
        bool moved = false;
        for (int g : members) {
            if (used.count(g))
                continue;
            const auto e = endsOf(s.geos[std::size_t(g)]);
            if (e[0] != here && e[1] != here)
                continue;
            const int next = e[0] == here ? e[1] : e[0];
            chain.steps.push_back({g, here, next});
            used.insert(g);
            here = next;
            moved = true;
            break;
        }
        if (!moved)
            break;
    }
    return chain;
}

// The left of the way walked, at point p of the step.
Point leftAt(const Sketch& s, const Step& st, Point p)
{
    const Geo& g = s.geos[std::size_t(st.geo)];
    if (g.kind == GeoKind::Line)
        return perp(unit(at(s, st.to) - at(s, st.from)));
    const Point out = unit(p - at(s, g.a));
    const bool anticlockwise = st.from == g.b;
    return anticlockwise ? out * -1.0 : out;
}

// The step's line or circle moved k to its left: a line (point, direction) or a circle
// (centre, radius).
struct Moved {
    bool line = true;
    Point p, d;
    double r = 0.0;
};

Moved moved(const Sketch& s, const Step& st, double k)
{
    const Geo& g = s.geos[std::size_t(st.geo)];
    Moved m;
    if (g.kind == GeoKind::Line) {
        const Point a = at(s, st.from), b = at(s, st.to);
        m.p = a + leftAt(s, st, a) * k;
        m.d = unit(b - a);
        return m;
    }
    m.line = false;
    m.p = at(s, g.a);
    const bool anticlockwise = st.from == g.b;
    m.r = radiusOf(s, g) + (anticlockwise ? -k : k);
    if (m.r <= 0)
        throw std::invalid_argument("That's further than an arc's radius, so the outline would turn inside out");
    return m;
}

std::vector<Point> meet(const Moved& m1, const Moved& m2)
{
    if (m1.line && m2.line) {
        const double d = cross(m1.d, m2.d);
        if (std::abs(d) < 1e-12)
            return {};
        return {m1.p + m1.d * (cross(m2.p - m1.p, m2.d) / d)};
    }
    if (!m1.line && !m2.line) {
        const double dd = len(m2.p - m1.p);
        if (dd < 1e-12 || dd > m1.r + m2.r || dd < std::abs(m1.r - m2.r))
            return {};
        const double a = (dd * dd + m1.r * m1.r - m2.r * m2.r) / (2 * dd);
        const double h = std::sqrt(std::max(m1.r * m1.r - a * a, 0.0));
        const Point u = (m2.p - m1.p) * (1.0 / dd);
        const Point base = m1.p + u * a;
        return {base + perp(u) * h, base - perp(u) * h};
    }
    const Moved& l = m1.line ? m1 : m2;
    const Moved& c = m1.line ? m2 : m1;
    const double along = dot(c.p - l.p, l.d);
    const Point foot = l.p + l.d * along;
    const double off = len(c.p - foot);
    if (off > c.r)
        return {};
    const double half = std::sqrt(std::max(c.r * c.r - off * off, 0.0));
    return {foot - l.d * half, foot + l.d * half};
}

double signedArea(const Sketch& s, const Chain& chain)
{
    std::vector<Point> path;
    for (const Step& st : chain.steps) {
        const Geo& g = s.geos[std::size_t(st.geo)];
        if (g.kind == GeoKind::Line) {
            path.push_back(at(s, st.from));
            continue;
        }
        const double span = spanOf(s, g);
        const bool anticlockwise = st.from == g.b;
        for (int i = 0; i < 24; ++i) {
            const double t = span * i / 24.0;
            path.push_back(pointAt(s, g, anticlockwise ? t : span - t));
        }
    }
    double area = 0.0;
    for (std::size_t i = 0; i < path.size(); ++i)
        area += cross(path[i], path[(i + 1) % path.size()]);
    return area / 2;
}

}  // namespace

std::vector<int> offset(Sketch& s, const std::vector<int>& geos, double distance)
{
    if (geos.empty())
        throw std::invalid_argument("Pick a line, arc or circle of the outline to offset");
    if (std::abs(distance) <= tolerance(s))
        throw std::invalid_argument("Offset by more than 0");
    Sketch work = s;
    std::vector<int> made;
    std::set<int> seen;
    const std::size_t originalCount = s.geos.size();
    for (int geo : geos) {
        if (geo < 0 || std::size_t(geo) >= originalCount || seen.count(geo))
            continue;
        const Geo g = s.geos[std::size_t(geo)];
        if (g.kind == GeoKind::Circle) {
            seen.insert(geo);
            const double r = radiusOf(s, g) + distance;
            if (r <= 0)
                throw std::invalid_argument("That's further in than the circle's radius");
            work.radii.push_back(r);
            work.geos.push_back({GeoKind::Circle, g.a, -1, -1, int(work.radii.size()) - 1, g.construction});
            made.push_back(int(work.geos.size()) - 1);
            continue;
        }
        const Chain chain = chainOf(s, geo, seen);
        const double k = chain.closed ? (signedArea(s, chain) > 0 ? -distance : distance) : distance;
        // Where each point of the chain goes.
        std::map<int, Point> to;
        const std::size_t n = chain.steps.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Step& st = chain.steps[i];
            const bool lastOpen = !chain.closed && i + 1 == n;
            if (!chain.closed && i == 0)
                to[st.from] = at(s, st.from) + leftAt(s, st, at(s, st.from)) * k;
            if (lastOpen) {
                to[st.to] = at(s, st.to) + leftAt(s, st, at(s, st.to)) * k;
                continue;
            }
            const Step& next = chain.steps[(i + 1) % n];
            const Point j = at(s, st.to);
            const Point n1 = leftAt(s, st, j), n2 = leftAt(s, next, j);
            if (len(n1 - n2) < 1e-9) {
                to[st.to] = j + n1 * k;  // a smooth joint
                continue;
            }
            const Point guess = j + (n1 + n2) * (0.5 * k);
            const std::vector<Point> options = meet(moved(s, st, k), moved(s, next, k));
            if (options.empty())
                throw std::invalid_argument("That's too far: the offset outline would fold over itself");
            Point best = options.front();
            for (const Point& o : options)
                if (len(o - guess) < len(best - guess))
                    best = o;
            to[st.to] = best;
        }
        // Check nothing turned round (offset too far in).
        for (const Step& st : chain.steps) {
            const Geo& og = s.geos[std::size_t(st.geo)];
            if (og.kind == GeoKind::Line) {
                if (dot(to[st.to] - to[st.from], at(s, st.to) - at(s, st.from)) <= 0)
                    throw std::invalid_argument("That's too far: a line of the offset outline would turn round");
            } else {
                (void)moved(s, st, k);  // radius check
                const Point c = at(s, og.a);
                const double before = spanOf(s, og);
                const double after = ccw(angleOf(to[og.b] - c), angleOf(to[og.c] - c));
                if (std::abs(after - before) > pi / 2 && std::abs(after - before) < 2 * pi - 1e-6)
                    throw std::invalid_argument("That's too far: an arc of the offset outline would turn round");
            }
        }
        std::map<int, int> index;
        for (const auto& [p, where] : to)
            index[p] = addPoint(work, where);
        std::map<int, int> copyOf;
        for (const Step& st : chain.steps) {
            const Geo& og = s.geos[std::size_t(st.geo)];
            if (og.kind == GeoKind::Line)
                work.geos.push_back({GeoKind::Line, index[og.a], index[og.b], -1, -1, og.construction});
            else
                work.geos.push_back({GeoKind::Arc, og.a, index[og.b], index[og.c], -1, og.construction});
            const int copy = int(work.geos.size()) - 1;
            copyOf[st.geo] = copy;
            made.push_back(copy);
            if (og.kind == GeoKind::Line)
                work.constraints.push_back(makeRule(work, Rule::Parallel, st.geo, copy));
        }
        // Smooth joints stay smooth.
        for (const Constraint& c : s.constraints)
            if (c.rule == Rule::Tangent && copyOf.count(c.a) && copyOf.count(c.b)) {
                Constraint t = c;
                t.a = copyOf[c.a];
                t.b = copyOf[c.b];
                work.constraints.push_back(makeRule(work, Rule::Tangent, t.a, t.b));
            }
    }
    s = std::move(work);
    return made;
}

// ---- Mirror -----------------------------------------------------------------------------------

std::vector<int> mirror(Sketch& s, const std::vector<int>& geos, const std::vector<int>& points, int axis)
{
    if (axis < 0 || std::size_t(axis) >= s.geos.size() || s.geos[std::size_t(axis)].kind != GeoKind::Line)
        throw std::invalid_argument("Pick a line to mirror about (Ctrl+click it last)");
    const Point la = at(s, s.geos[std::size_t(axis)].a);
    const Point ld = unit(at(s, s.geos[std::size_t(axis)].b) - la);
    if (len(ld) == 0)
        throw std::invalid_argument("The mirror line has no length");
    const double tol = tolerance(s);
    std::vector<int> what;
    for (int g : geos)
        if (g != axis && g >= 0 && std::size_t(g) < s.geos.size() &&
            std::find(what.begin(), what.end(), g) == what.end())
            what.push_back(g);
    if (what.empty() && points.empty())
        throw std::invalid_argument("Pick what to mirror, then Ctrl+click the line to mirror it about");

    std::map<int, int> image;
    const auto imageOf = [&](int p) {
        if (const auto it = image.find(p); it != image.end())
            return it->second;
        const Point q = at(s, p);
        const Point foot = la + ld * dot(q - la, ld);
        int out = p;
        if (len(q - foot) > 10 * tol) {
            out = addPoint(s, foot * 2.0 - q);
            s.constraints.push_back(Constraint{Rule::Symmetric, p, out, axis, 0.0, 0.0});
        }
        image[p] = out;
        return out;
    };
    std::vector<int> made;
    for (int gi : what) {
        const Geo g = s.geos[std::size_t(gi)];
        Geo copy = g;
        copy.a = imageOf(g.a);
        if (g.kind == GeoKind::Line) {
            copy.b = imageOf(g.b);
        } else if (g.kind == GeoKind::Arc) {
            // Mirrored, anticlockwise runs the other way: start and end swap.
            copy.b = imageOf(g.c);
            copy.c = imageOf(g.b);
        } else {
            s.radii.push_back(s.radii[std::size_t(g.r)]);
            copy.r = int(s.radii.size()) - 1;
        }
        s.geos.push_back(copy);
        made.push_back(int(s.geos.size()) - 1);
        if (g.kind == GeoKind::Circle)
            s.constraints.push_back(makeRule(s, Rule::Equal, gi, made.back()));
    }
    for (int p : points)
        if (p > 0 && std::size_t(p) < s.points.size())
            imageOf(p);
    return made;
}

}  // namespace forge::sketch
