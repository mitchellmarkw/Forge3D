// SPDX-License-Identifier: GPL-3.0-or-later
#include "SketchView.h"

#include <QFontMetricsF>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace forge::app {

using namespace forge::sketch;

namespace {

constexpr double pi = std::numbers::pi;
constexpr double kPickPixels = 7.0;
constexpr double kSnapPixels = 8.0;

double distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a, ap = p - a;
    const double l2 = QPointF::dotProduct(ab, ab);
    const double t = l2 > 0 ? std::clamp(QPointF::dotProduct(ap, ab) / l2, 0.0, 1.0) : 0.0;
    const QPointF d = ap - ab * t;
    return std::hypot(d.x(), d.y());
}

// Angle of v, 0..2π.
double angleOf(double x, double y)
{
    double a = std::atan2(y, x);
    return a < 0 ? a + 2 * pi : a;
}

// The anticlockwise sweep from a to b, in (0, 2π].
double sweep(double a, double b)
{
    double s = b - a;
    while (s <= 1e-12)
        s += 2 * pi;
    while (s > 2 * pi)
        s -= 2 * pi;
    return s;
}

// Lines shrunk to nothing, or circles to a point: a rule that does that is refused.
int collapsedCount(const Sketch& s)
{
    int n = 0;
    for (const Geo& g : s.geos) {
        if (g.kind == GeoKind::Line) {
            const Point a = s.points[std::size_t(g.a)], b = s.points[std::size_t(g.b)];
            n += std::hypot(b.x - a.x, b.y - a.y) < 1e-4 ? 1 : 0;
        } else {
            n += radiusOf(s, g) < 1e-4 ? 1 : 0;
        }
    }
    return n;
}

}  // namespace

SketchView::SketchView(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
    setCursor(Qt::CrossCursor);
}

void SketchView::start(const Sketch& sketch, std::vector<std::vector<QPointF>> reference)
{
    m_sketch = sketch;
    m_reference = std::move(reference);
    m_undo.clear();
    m_redo.clear();
    m_picks.clear();
    m_hover.reset();
    m_clicks.clear();
    m_chainStart = -1;
    setTool(Tool::Select);
    solveNow();
    fit();
}

void SketchView::setTool(Tool tool)
{
    m_tool = tool;
    m_clicks.clear();
    m_chainStart = -1;
    emit toolChanged(tool);
    switch (tool) {
    case Tool::Select:
        emit message(tr("Click to pick (Ctrl+click for more), drag to move. Then add rules or dimensions."), false);
        break;
    case Tool::Line:
        emit message(tr("Line: click where it starts, then each corner. Click the first point to close it; "
                        "right-click or Esc to stop."),
                     false);
        break;
    case Tool::Rectangle: emit message(tr("Rectangle: click one corner, then the opposite one."), false); break;
    case Tool::Circle: emit message(tr("Circle: click the middle, then a point on its edge."), false); break;
    case Tool::Arc:
        emit message(tr("Arc: click the middle, then where it starts, then where it ends (going anticlockwise)."),
                     false);
        break;
    case Tool::Slot:
        emit message(tr("Slot: click the middle of one round end, then the other, then how wide."), false);
        break;
    case Tool::Point:
        emit message(tr("Point: click to put a point (a hole goes at each point on its own)."), false);
        break;
    }
    update();
}

// ---- View ---------------------------------------------------------------------------

QPointF SketchView::toScreen(const Point& p) const
{
    return {width() / 2.0 + (p.x - m_centre.x()) * m_scale, height() / 2.0 - (p.y - m_centre.y()) * m_scale};
}

Point SketchView::toSketch(const QPointF& p) const
{
    return {m_centre.x() + (p.x() - width() / 2.0) / m_scale, m_centre.y() - (p.y() - height() / 2.0) / m_scale};
}

double SketchView::gridStep() const
{
    for (double step : {0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0})
        if (step * m_scale >= 14.0)
            return step;
    return 1000.0;
}

void SketchView::fit()
{
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    const auto grow = [&](double x, double y) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
    };
    for (std::size_t i = 1; i < m_sketch.points.size(); ++i)
        grow(m_sketch.points[i].x, m_sketch.points[i].y);
    for (const Geo& g : m_sketch.geos)
        if (g.kind == GeoKind::Circle) {
            const Point c = m_sketch.points[std::size_t(g.a)];
            const double r = radiusOf(m_sketch, g);
            grow(c.x - r, c.y - r);
            grow(c.x + r, c.y + r);
        }
    for (const auto& line : m_reference)
        for (const QPointF& p : line)
            grow(p.x(), p.y());
    if (x0 > x1) {
        x0 = y0 = -25;
        x1 = y1 = 25;
    }
    grow(0, 0);
    const double w = std::max(x1 - x0, 1.0), h = std::max(y1 - y0, 1.0);
    m_scale = std::clamp(std::min(width() / (w * 1.3), height() / (h * 1.3)), 0.05, 2000.0);
    m_centre = QPointF((x0 + x1) / 2, (y0 + y1) / 2);
    update();
}

// ---- Picking and snapping ---------------------------------------------------------------

SketchView::Snap SketchView::snapAt(const QPointF& pixel) const
{
    Snap s;
    s.at = toSketch(pixel);
    double best = kSnapPixels;
    for (std::size_t i = 0; i < m_sketch.points.size(); ++i) {
        const QPointF q = toScreen(m_sketch.points[i]);
        const double d = std::hypot(q.x() - pixel.x(), q.y() - pixel.y());
        if (d < best) {
            best = d;
            s.point = int(i);
            s.at = m_sketch.points[i];
        }
    }
    if (s.point >= 0)
        return s;
    for (const auto& line : m_reference)
        for (const QPointF& r : line) {
            const QPointF q = toScreen({r.x(), r.y()});
            const double d = std::hypot(q.x() - pixel.x(), q.y() - pixel.y());
            if (d < best) {
                best = d;
                s.at = {r.x(), r.y()};
            }
        }
    if (best < kSnapPixels)
        return s;
    // The grid, when close to one of its points.
    const double step = gridStep();
    const Point g{std::round(s.at.x / step) * step, std::round(s.at.y / step) * step};
    const QPointF q = toScreen(g);
    if (std::hypot(q.x() - pixel.x(), q.y() - pixel.y()) < 6.0)
        s.at = g;
    return s;
}

std::optional<SketchView::Pick> SketchView::pickAt(const QPointF& pixel) const
{
    for (std::size_t i = 0; i < m_ruleRects.size(); ++i)
        if (m_ruleRects[i].contains(pixel))
            return Pick{Pick::Rule, int(i)};
    std::optional<Pick> best;
    double bestD = kPickPixels;
    for (std::size_t i = 0; i < m_sketch.points.size(); ++i) {
        const QPointF q = toScreen(m_sketch.points[i]);
        const double d = std::hypot(q.x() - pixel.x(), q.y() - pixel.y());
        if (d < bestD) {
            bestD = d;
            best = Pick{Pick::Point, int(i)};
        }
    }
    if (best)
        return best;
    bestD = kPickPixels;
    for (std::size_t i = 0; i < m_sketch.geos.size(); ++i) {
        const Geo& g = m_sketch.geos[i];
        double d = 1e300;
        if (g.kind == GeoKind::Line) {
            d = distanceToSegment(pixel, toScreen(m_sketch.points[std::size_t(g.a)]),
                                  toScreen(m_sketch.points[std::size_t(g.b)]));
        } else {
            const Point c = m_sketch.points[std::size_t(g.a)];
            const Point p = toSketch(pixel);
            const double r = radiusOf(m_sketch, g);
            d = std::abs(std::hypot(p.x - c.x, p.y - c.y) - r) * m_scale;
            if (g.kind == GeoKind::Arc) {
                const Point s = m_sketch.points[std::size_t(g.b)], e = m_sketch.points[std::size_t(g.c)];
                const double a0 = angleOf(s.x - c.x, s.y - c.y);
                const double span = sweep(a0, angleOf(e.x - c.x, e.y - c.y));
                if (sweep(a0, angleOf(p.x - c.x, p.y - c.y)) > span)
                    d = 1e300;
            }
        }
        if (d < bestD) {
            bestD = d;
            best = Pick{Pick::Geo, int(i)};
        }
    }
    return best;
}

std::vector<int> SketchView::picked(Pick::Kind kind) const
{
    std::vector<int> out;
    for (const Pick& p : m_picks)
        if (p.kind == kind)
            out.push_back(p.index);
    return out;
}

// ---- Changing the sketch ------------------------------------------------------------------

void SketchView::remember()
{
    m_undo.push_back(m_sketch);
    if (m_undo.size() > 200)
        m_undo.erase(m_undo.begin());
    m_redo.clear();
}

void SketchView::solveNow()
{
    try {
        m_solved = solve(m_sketch);
    } catch (const std::exception& e) {
        m_solved = Solved{};
        m_solved.ok = false;
        emit message(QString::fromUtf8(e.what()), true);
    }
    update();
    emit changed();
}

void SketchView::undo()
{
    if (m_undo.empty())
        return;
    m_redo.push_back(m_sketch);
    m_sketch = m_undo.back();
    m_undo.pop_back();
    m_picks.clear();
    m_clicks.clear();
    solveNow();
}

void SketchView::redo()
{
    if (m_redo.empty())
        return;
    m_undo.push_back(m_sketch);
    m_sketch = m_redo.back();
    m_redo.pop_back();
    m_picks.clear();
    solveNow();
}

int SketchView::pointFor(const Snap& snap)
{
    if (snap.point >= 0 && std::size_t(snap.point) < m_sketch.points.size())
        return snap.point;
    return addPoint(m_sketch, snap.at);
}

int SketchView::addLine(int a, int b)
{
    m_sketch.geos.push_back({GeoKind::Line, a, b, -1, -1, false});
    return int(m_sketch.geos.size()) - 1;
}

void SketchView::finishChain()
{
    m_clicks.clear();
    m_chainStart = -1;
    update();
}

void SketchView::clickTool(const Snap& snapIn)
{
    Snap snap = snapIn;
    switch (m_tool) {
    case Tool::Select: return;
    case Tool::Point: {
        if (snap.point >= 0)
            return;  // already a point there
        remember();
        addPoint(m_sketch, snap.at);
        solveNow();
        return;
    }
    case Tool::Line: {
        if (m_clicks.empty()) {
            m_clicks.push_back(snap);
            return;
        }
        Snap& from = m_clicks.back();
        const Point a0 = from.point >= 0 ? m_sketch.points[std::size_t(from.point)] : from.at;
        if (std::hypot(snap.at.x - a0.x, snap.at.y - a0.y) * m_scale < 2.0)
            return;  // the same spot twice
        remember();
        const int a = pointFor(from);
        if (m_chainStart < 0)
            m_chainStart = a;
        // Nearly level or upright: make it so, and keep it so.
        std::optional<Rule> straight;
        if (snap.point < 0) {
            const double dx = snap.at.x - a0.x, dy = snap.at.y - a0.y;
            const double angle = std::abs(std::atan2(dy, dx)) * 180 / pi;
            if (angle < 3 || angle > 177) {
                snap.at.y = a0.y;
                straight = Rule::Horizontal;
            } else if (std::abs(angle - 90) < 3) {
                snap.at.x = a0.x;
                straight = Rule::Vertical;
            }
        }
        const int b = pointFor(snap);
        const int line = addLine(a, b);
        if (straight)
            m_sketch.constraints.push_back(makeRule(m_sketch, *straight, line));
        solveNow();
        if (b == m_chainStart) {
            finishChain();  // closed the loop
            return;
        }
        m_clicks = {Snap{m_sketch.points[std::size_t(b)], b}};
        return;
    }
    case Tool::Rectangle: {
        if (m_clicks.empty()) {
            m_clicks.push_back(snap);
            return;
        }
        const Point p1 = m_clicks[0].at, p3 = snap.at;
        if (std::abs(p3.x - p1.x) * m_scale < 2 || std::abs(p3.y - p1.y) * m_scale < 2)
            return;
        remember();
        const int a = pointFor(m_clicks[0]);
        const int c = pointFor(snap);
        const int b = addPoint(m_sketch, {p3.x, p1.y});
        const int d = addPoint(m_sketch, {p1.x, p3.y});
        const int l0 = addLine(a, b), l1 = addLine(b, c), l2 = addLine(c, d), l3 = addLine(d, a);
        m_sketch.constraints.push_back(makeRule(m_sketch, Rule::Horizontal, l0));
        m_sketch.constraints.push_back(makeRule(m_sketch, Rule::Vertical, l1));
        m_sketch.constraints.push_back(makeRule(m_sketch, Rule::Horizontal, l2));
        m_sketch.constraints.push_back(makeRule(m_sketch, Rule::Vertical, l3));
        m_clicks.clear();
        solveNow();
        return;
    }
    case Tool::Circle: {
        if (m_clicks.empty()) {
            m_clicks.push_back(snap);
            return;
        }
        const Point c = m_clicks[0].point >= 0 ? m_sketch.points[std::size_t(m_clicks[0].point)] : m_clicks[0].at;
        const double r = std::hypot(snap.at.x - c.x, snap.at.y - c.y);
        if (r * m_scale < 2)
            return;
        remember();
        const int centre = pointFor(m_clicks[0]);
        m_sketch.radii.push_back(r);
        m_sketch.geos.push_back({GeoKind::Circle, centre, -1, -1, int(m_sketch.radii.size()) - 1, false});
        m_clicks.clear();
        solveNow();
        return;
    }
    case Tool::Arc: {
        if (m_clicks.size() < 2) {
            m_clicks.push_back(snap);
            return;
        }
        const Point c = m_clicks[0].point >= 0 ? m_sketch.points[std::size_t(m_clicks[0].point)] : m_clicks[0].at;
        const Point s = m_clicks[1].at;
        const double r = std::hypot(s.x - c.x, s.y - c.y);
        const double ea = std::atan2(snap.at.y - c.y, snap.at.x - c.x);
        if (r * m_scale < 2)
            return;
        Snap end = snap;
        if (end.point < 0)
            end.at = {c.x + r * std::cos(ea), c.y + r * std::sin(ea)};
        remember();
        const int centre = pointFor(m_clicks[0]);
        const int start = pointFor(m_clicks[1]);
        const int finish = pointFor(end);
        m_sketch.geos.push_back({GeoKind::Arc, centre, start, finish, -1, false});
        m_clicks.clear();
        solveNow();
        return;
    }
    case Tool::Slot: {
        if (m_clicks.size() < 2) {
            m_clicks.push_back(snap);
            return;
        }
        const Point c1 = m_clicks[0].at, c2 = m_clicks[1].at;
        const double len = std::hypot(c2.x - c1.x, c2.y - c1.y);
        if (len * m_scale < 2)
            return;
        const double ux = (c2.x - c1.x) / len, uy = (c2.y - c1.y) / len;
        const double nx = -uy, ny = ux;
        const double r = std::max(std::abs((snap.at.x - c1.x) * nx + (snap.at.y - c1.y) * ny), 2.0 / m_scale);
        remember();
        const int k1 = pointFor(m_clicks[0]), k2 = pointFor(m_clicks[1]);
        const int a1 = addPoint(m_sketch, {c1.x + nx * r, c1.y + ny * r});
        const int b1 = addPoint(m_sketch, {c2.x + nx * r, c2.y + ny * r});
        const int a2 = addPoint(m_sketch, {c2.x - nx * r, c2.y - ny * r});
        const int b2 = addPoint(m_sketch, {c1.x - nx * r, c1.y - ny * r});
        const int top = addLine(a1, b1);
        const int bottom = addLine(a2, b2);
        m_sketch.geos.push_back({GeoKind::Arc, k2, a2, b1, -1, false});  // round the far end
        const int arc2 = int(m_sketch.geos.size()) - 1;
        m_sketch.geos.push_back({GeoKind::Arc, k1, a1, b2, -1, false});  // round the near end
        const int arc1 = int(m_sketch.geos.size()) - 1;
        for (const auto& [g1, g2] : {std::pair{top, arc1}, std::pair{top, arc2}, std::pair{bottom, arc1},
                                     std::pair{bottom, arc2}})
            m_sketch.constraints.push_back(makeRule(m_sketch, Rule::Tangent, g1, g2));
        m_sketch.constraints.push_back(makeRule(m_sketch, Rule::Equal, arc1, arc2));
        m_clicks.clear();
        solveNow();
        return;
    }
    }
}

void SketchView::addRule(Rule rule)
{
    const std::vector<int> points = picked(Pick::Point), geos = picked(Pick::Geo);
    const auto isLine = [this](int g) { return m_sketch.geos[std::size_t(g)].kind == GeoKind::Line; };
    std::vector<Constraint> adding;
    bool mergeTwo = false;
    switch (rule) {
    case Rule::Horizontal:
    case Rule::Vertical:
        if (!geos.empty() && std::all_of(geos.begin(), geos.end(), isLine) && points.empty()) {
            for (int g : geos)
                adding.push_back(makeRule(m_sketch, rule, g));
        } else if (points.size() == 2 && geos.empty()) {
            adding.push_back(makeRule(m_sketch, rule, points[0], points[1]));
        } else {
            emit message(tr("Pick one or more lines (or two points) first."), true);
            return;
        }
        break;
    case Rule::Parallel:
    case Rule::Perpendicular:
        if (geos.size() != 2 || !points.empty() || !isLine(geos[0]) || !isLine(geos[1])) {
            emit message(tr("Pick two lines first."), true);
            return;
        }
        adding.push_back(makeRule(m_sketch, rule, geos[0], geos[1]));
        break;
    case Rule::Equal:
        if (geos.size() < 2 || !points.empty() ||
            !std::all_of(geos.begin(), geos.end(), [&](int g) { return isLine(g) == isLine(geos[0]); })) {
            emit message(tr("Pick two or more lines, or two or more circles / arcs, first."), true);
            return;
        }
        for (std::size_t i = 1; i < geos.size(); ++i)
            adding.push_back(makeRule(m_sketch, rule, geos[0], geos[i]));
        break;
    case Rule::Tangent:
        if (geos.size() != 2 || !points.empty() || (isLine(geos[0]) && isLine(geos[1]))) {
            emit message(tr("Pick a line and a circle / arc, or two circles / arcs, first."), true);
            return;
        }
        adding.push_back(makeRule(m_sketch, rule, geos[0], geos[1]));
        break;
    case Rule::Coincident:
        if (points.size() == 2 && geos.empty()) {
            mergeTwo = true;
        } else if (points.size() == 1 && geos.size() == 1) {
            adding.push_back(makeRule(m_sketch, Rule::PointOn, points[0], geos[0]));
        } else {
            emit message(tr("Pick two points (to join them), or a point and a line / circle (to put it on it)."),
                         true);
            return;
        }
        break;
    case Rule::Symmetric:
        if (points.size() != 2 || geos.size() != 1 || !isLine(geos[0])) {
            emit message(tr("Pick two points and the line to mirror them about."), true);
            return;
        }
        adding.push_back(makeRule(m_sketch, rule, points[0], points[1], geos[0]));
        break;
    case Rule::Fixed:
        if (points.empty() || !geos.empty()) {
            emit message(tr("Pick the points to lock in place."), true);
            return;
        }
        for (int p : points)
            if (p != 0)
                adding.push_back(makeRule(m_sketch, rule, p));
        break;
    default: return;
    }
    remember();
    const Sketch before = m_sketch;
    if (mergeTwo) {
        // Joined: one point from now on (the origin stays the origin).
        int into = points[0], from = points[1];
        if (from == 0)
            std::swap(into, from);
        mergePoints(m_sketch, into, from);
    }
    for (const Constraint& c : adding)
        m_sketch.constraints.push_back(c);
    Solved result;
    try {
        result = solve(m_sketch);
    } catch (const std::exception& e) {
        result.ok = false;
    }
    if (!result.ok || collapsedCount(m_sketch) > collapsedCount(before)) {
        m_sketch = before;
        m_undo.pop_back();
        solveNow();
        emit message(result.ok ? tr("That rule would shrink a line or circle to nothing, so it wasn't added.")
                               : tr("That rule fights with the ones already there, so it wasn't added."),
                     true);
        return;
    }
    m_picks.clear();
    solveNow();
    emit message({}, false);
}

void SketchView::addDimension(int how)
{
    const std::vector<int> points = picked(Pick::Point), geos = picked(Pick::Geo);
    const auto kindOf = [this](int g) { return m_sketch.geos[std::size_t(g)].kind; };
    Constraint c;
    bool ok = true;
    if (how == 0) {
        if (geos.size() == 1 && points.empty() && kindOf(geos[0]) == GeoKind::Line)
            c = makeRule(m_sketch, Rule::Distance, geos[0]);
        else if (geos.size() == 1 && points.empty())
            c = makeRule(m_sketch, Rule::Radius, geos[0]);
        else if (points.size() == 2 && geos.empty())
            c = makeRule(m_sketch, Rule::Distance, points[0], points[1]);
        else if (geos.size() == 2 && points.empty() && kindOf(geos[0]) == GeoKind::Line &&
                 kindOf(geos[1]) == GeoKind::Line)
            c = makeRule(m_sketch, Rule::Angle, geos[0], geos[1]);
        else
            ok = false;
    } else {
        const Rule r = how == 1 ? Rule::DistanceX : Rule::DistanceY;
        if (geos.size() == 1 && points.empty() && kindOf(geos[0]) == GeoKind::Line)
            c = makeRule(m_sketch, r, m_sketch.geos[std::size_t(geos[0])].a, m_sketch.geos[std::size_t(geos[0])].b);
        else if (points.size() == 2 && geos.empty())
            c = makeRule(m_sketch, r, points[0], points[1]);
        else if (points.size() == 1 && geos.empty())
            c = makeRule(m_sketch, r, 0, points[0]);  // from the origin
        else
            ok = false;
    }
    if (!ok) {
        emit message(how == 0 ? tr("Pick a line (its length), two points (their distance), a circle or arc (its "
                                   "radius) or two lines (their angle) first.")
                              : tr("Pick a line, two points, or one point (from the origin) first."),
                     true);
        return;
    }
    remember();
    m_sketch.constraints.push_back(c);
    m_picks.clear();
    solveNow();
    emit message({}, false);
    editDimension(int(m_sketch.constraints.size()) - 1);
}

void SketchView::editDimension(int index)
{
    if (index < 0 || std::size_t(index) >= m_sketch.constraints.size())
        return;
    Constraint& c = m_sketch.constraints[std::size_t(index)];
    if (!isDimension(c.rule))
        return;
    const bool signedValue = c.rule == Rule::DistanceX || c.rule == Rule::DistanceY;
    const bool angle = c.rule == Rule::Angle;
    QString label = angle ? tr("Angle (degrees):")
                    : c.rule == Rule::Radius ? tr("Radius (mm):")
                                             : tr("Distance (mm):");
    bool accepted = false;
    const double shown = signedValue ? std::abs(c.value) : c.value;
    const double value = QInputDialog::getDouble(this, tr("Dimension"), label, shown, angle ? -360.0 : signedValue ? 0.0 : 0.001,
                                                 angle ? 360.0 : 100000.0, 3, &accepted);
    if (!accepted)
        return;
    const Sketch before = m_sketch;
    remember();
    Constraint& now = m_sketch.constraints[std::size_t(index)];
    now.value = signedValue ? (now.value < 0 ? -value : value) : value;
    Solved result;
    try {
        result = solve(m_sketch);
    } catch (const std::exception&) {
        result.ok = false;
    }
    if (!result.ok || collapsedCount(m_sketch) > collapsedCount(before)) {
        m_sketch = before;
        m_undo.pop_back();
        solveNow();
        emit message(tr("That size can't work with the other rules, so it wasn't changed."), true);
        return;
    }
    solveNow();
}

void SketchView::pickGeo(int geo)
{
    m_picks.clear();
    if (geo >= 0 && std::size_t(geo) < m_sketch.geos.size())
        m_picks.push_back({Pick::Geo, geo});
    update();
}

void SketchView::toggleConstruction()
{
    const std::vector<int> geos = picked(Pick::Geo);
    if (geos.empty()) {
        emit message(tr("Pick the lines or circles to turn into (or back from) helper lines."), true);
        return;
    }
    remember();
    const bool to = !m_sketch.geos[std::size_t(geos[0])].construction;
    for (int g : geos)
        m_sketch.geos[std::size_t(g)].construction = to;
    solveNow();
}

void SketchView::deleteSelection()
{
    if (m_picks.empty())
        return;
    remember();
    std::vector<int> geos = picked(Pick::Geo);
    const std::vector<int> rules = picked(Pick::Rule);
    // Picked points take the curves using them with them; points on their own just go.
    const std::vector<int> loose = loosePoints(m_sketch);
    std::vector<int> lonePicked;
    for (int p : picked(Pick::Point)) {
        if (p == 0)
            continue;
        if (std::find(loose.begin(), loose.end(), p) != loose.end()) {
            lonePicked.push_back(p);
            continue;
        }
        for (std::size_t g = 0; g < m_sketch.geos.size(); ++g) {
            const Geo& geo = m_sketch.geos[g];
            if (geo.a == p || (geo.kind != GeoKind::Circle && geo.b == p) || (geo.kind == GeoKind::Arc && geo.c == p))
                geos.push_back(int(g));
        }
    }
    // Each removal renumbers only its own kind (and lone points belong to no curve).
    removeConstraints(m_sketch, rules);
    removePoints(m_sketch, lonePicked);
    removeGeos(m_sketch, geos);
    m_picks.clear();
    m_hover.reset();
    solveNow();
}

// ---- Drawing --------------------------------------------------------------------------------

QString SketchView::describeRule(const Constraint& c) const
{
    switch (c.rule) {
    case Rule::Horizontal: return QStringLiteral("H");
    case Rule::Vertical: return QStringLiteral("V");
    case Rule::Parallel: return QStringLiteral("∥");
    case Rule::Perpendicular: return QStringLiteral("⟂");
    case Rule::Equal: return QStringLiteral("=");
    case Rule::Tangent: return QStringLiteral("T");
    case Rule::PointOn: return QStringLiteral("on");
    case Rule::Coincident: return QStringLiteral("●");
    case Rule::Symmetric: return QStringLiteral("⇆");
    case Rule::Midpoint: return QStringLiteral("mid");
    case Rule::Fixed: return tr("lock");
    default: return QString::fromStdString(sketch::describe(m_sketch, c));
    }
}

QPointF SketchView::ruleAnchor(const Constraint& c, int index) const
{
    const auto P = [this](int i) { return toScreen(m_sketch.points[std::size_t(i)]); };
    const auto geoMid = [&](int g) {
        const Geo& geo = m_sketch.geos[std::size_t(g)];
        if (geo.kind == GeoKind::Line)
            return (P(geo.a) + P(geo.b)) / 2.0;
        const Point ctr = m_sketch.points[std::size_t(geo.a)];
        const double r = radiusOf(m_sketch, geo);
        return toScreen({ctr.x + r * 0.7071, ctr.y + r * 0.7071});
    };
    QPointF at;
    switch (c.rule) {
    case Rule::Horizontal:
    case Rule::Vertical:
    case Rule::Distance: at = c.b < 0 ? geoMid(c.a) : (P(c.a) + P(c.b)) / 2.0; break;
    case Rule::Parallel:
    case Rule::Perpendicular:
    case Rule::Equal:
    case Rule::Tangent:
    case Rule::Radius:
    case Rule::Angle: at = geoMid(c.a); break;
    case Rule::PointOn:
    case Rule::Fixed: at = P(c.a); break;
    case Rule::Coincident:
    case Rule::Symmetric:
    case Rule::Midpoint:
    case Rule::DistanceX:
    case Rule::DistanceY: at = (P(c.a) + P(c.b)) / 2.0; break;
    }
    (void)index;
    return at + QPointF(6, -10);
}

void SketchView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(250, 251, 253));

    // Grid
    const double step = gridStep();
    const Point lo = toSketch({0, double(height())}), hi = toSketch({double(width()), 0});
    const QPen minor(QColor(232, 236, 242), 1), major(QColor(210, 216, 226), 1);
    for (double x = std::floor(lo.x / step) * step; x <= hi.x; x += step) {
        const bool big = std::abs(std::remainder(x, step * 5)) < step * 0.01;
        p.setPen(big ? major : minor);
        const double sx = toScreen({x, 0}).x();
        p.drawLine(QPointF(sx, 0), QPointF(sx, height()));
    }
    for (double y = std::floor(lo.y / step) * step; y <= hi.y; y += step) {
        const bool big = std::abs(std::remainder(y, step * 5)) < step * 0.01;
        p.setPen(big ? major : minor);
        const double sy = toScreen({0, y}).y();
        p.drawLine(QPointF(0, sy), QPointF(width(), sy));
    }
    // Axes through the origin
    const QPointF o = toScreen({0, 0});
    p.setPen(QPen(QColor(220, 90, 90, 140), 1));
    p.drawLine(QPointF(0, o.y()), QPointF(width(), o.y()));
    p.setPen(QPen(QColor(90, 170, 90, 140), 1));
    p.drawLine(QPointF(o.x(), 0), QPointF(o.x(), height()));

    // The face sketched on
    p.setPen(QPen(QColor(150, 150, 160), 1.5, Qt::DashLine));
    for (const auto& line : m_reference) {
        QPolygonF poly;
        for (const QPointF& q : line)
            poly << toScreen({q.x(), q.y()});
        p.drawPolyline(poly);
    }

    const bool held = m_solved.ok && m_solved.freedom == 0 && !m_sketch.geos.empty();
    const QColor normal = held ? QColor(30, 140, 60) : QColor(30, 70, 160);
    const QColor pickedColour(240, 130, 20), hoverColour(250, 175, 80);
    const auto colourFor = [&](Pick pick, const QColor& base) {
        if (std::find(m_picks.begin(), m_picks.end(), pick) != m_picks.end())
            return pickedColour;
        if (m_hover && *m_hover == pick)
            return hoverColour;
        return base;
    };
    const auto drawArc = [&](const Point& c, double r, double a0, double span) {
        QPolygonF poly;
        const int n = std::max(8, int(span * r * m_scale / 4));
        for (int k = 0; k <= n; ++k) {
            const double a = a0 + span * k / n;
            poly << toScreen({c.x + r * std::cos(a), c.y + r * std::sin(a)});
        }
        p.drawPolyline(poly);
    };
    for (std::size_t i = 0; i < m_sketch.geos.size(); ++i) {
        const Geo& g = m_sketch.geos[i];
        QPen pen(colourFor({Pick::Geo, int(i)}, g.construction ? QColor(90, 140, 220) : normal),
                 g.construction ? 1.2 : 2.0, g.construction ? Qt::DashLine : Qt::SolidLine);
        p.setPen(pen);
        const Point c = m_sketch.points[std::size_t(g.a)];
        switch (g.kind) {
        case GeoKind::Line:
            p.drawLine(toScreen(c), toScreen(m_sketch.points[std::size_t(g.b)]));
            break;
        case GeoKind::Circle: drawArc(c, radiusOf(m_sketch, g), 0, 2 * pi); break;
        case GeoKind::Arc: {
            const Point s = m_sketch.points[std::size_t(g.b)], e = m_sketch.points[std::size_t(g.c)];
            const double a0 = angleOf(s.x - c.x, s.y - c.y);
            drawArc(c, radiusOf(m_sketch, g), a0, sweep(a0, angleOf(e.x - c.x, e.y - c.y)));
            break;
        }
        }
    }

    // Points: loose ends in red.
    const std::vector<int> loose = looseEnds(m_sketch);
    for (std::size_t i = 0; i < m_sketch.points.size(); ++i) {
        const QPointF q = toScreen(m_sketch.points[i]);
        const bool isLoose = std::find(loose.begin(), loose.end(), int(i)) != loose.end();
        const QColor c = colourFor({Pick::Point, int(i)}, i == 0 ? QColor(120, 120, 120) : isLoose ? QColor(210, 40, 40) : normal);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const double r = i == 0 ? 4.0 : 3.0;
        if (i == 0)
            p.drawEllipse(q, r, r);
        else
            p.drawRect(QRectF(q.x() - r, q.y() - r, 2 * r, 2 * r));
    }

    // Rules and dimensions
    m_ruleRects.assign(m_sketch.constraints.size(), QRectF());
    QFont font = p.font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    p.setFont(font);
    const QFontMetricsF fm(font);
    for (std::size_t i = 0; i < m_sketch.constraints.size(); ++i) {
        const Constraint& c = m_sketch.constraints[i];
        const QString text = describeRule(c);
        const QPointF at = ruleAnchor(c, int(i));
        // Distances between two points: a thin line between them.
        if ((c.rule == Rule::Distance && c.b >= 0) || c.rule == Rule::DistanceX || c.rule == Rule::DistanceY) {
            const Point a = m_sketch.points[std::size_t(c.a)], b = m_sketch.points[std::size_t(c.b)];
            Point end = b;
            if (c.rule == Rule::DistanceX)
                end = {b.x, a.y};
            if (c.rule == Rule::DistanceY)
                end = {a.x, b.y};
            p.setPen(QPen(QColor(120, 120, 140), 1, Qt::DotLine));
            p.drawLine(toScreen(a), toScreen(end));
        }
        QRectF box(at.x(), at.y() - fm.height() / 2, fm.horizontalAdvance(text) + 8, fm.height() + 2);
        // Not on top of another label.
        for (int tries = 0; tries < 12; ++tries) {
            const auto clash = std::find_if(m_ruleRects.begin(), m_ruleRects.begin() + std::ptrdiff_t(i),
                                            [&box](const QRectF& r) { return r.intersects(box); });
            if (clash == m_ruleRects.begin() + std::ptrdiff_t(i))
                break;
            box.moveLeft(clash->right() + 3);
        }
        m_ruleRects[i] = box;
        const bool failing = std::find(m_solved.failing.begin(), m_solved.failing.end(), int(i)) != m_solved.failing.end();
        const QColor edge = colourFor({Pick::Rule, int(i)}, failing ? QColor(210, 40, 40) : QColor(150, 150, 165));
        p.setPen(QPen(edge, 1));
        p.setBrush(isDimension(c.rule) ? QColor(255, 255, 255, 235) : QColor(240, 243, 250, 235));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(failing ? QColor(200, 30, 30) : QColor(40, 40, 60));
        p.drawText(box, Qt::AlignCenter, text);
    }

    // What's being drawn
    if (m_tool != Tool::Select && !m_clicks.empty()) {
        p.setPen(QPen(QColor(240, 130, 20), 1.5, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        const Point a = m_clicks[0].at, cur = m_cursor.at;
        switch (m_tool) {
        case Tool::Line: p.drawLine(toScreen(m_clicks.back().at), toScreen(cur)); break;
        case Tool::Rectangle: p.drawRect(QRectF(toScreen(a), toScreen(cur)).normalized()); break;
        case Tool::Circle: drawArc(a, std::hypot(cur.x - a.x, cur.y - a.y), 0, 2 * pi); break;
        case Tool::Arc: {
            const double r = std::hypot((m_clicks.size() > 1 ? m_clicks[1].at : cur).x - a.x,
                                        (m_clicks.size() > 1 ? m_clicks[1].at : cur).y - a.y);
            if (m_clicks.size() == 1) {
                p.drawLine(toScreen(a), toScreen(cur));
                drawArc(a, r, 0, 2 * pi);
            } else {
                const Point s = m_clicks[1].at;
                const double a0 = angleOf(s.x - a.x, s.y - a.y);
                drawArc(a, r, a0, sweep(a0, angleOf(cur.x - a.x, cur.y - a.y)));
            }
            break;
        }
        case Tool::Slot: {
            if (m_clicks.size() == 1) {
                p.drawLine(toScreen(a), toScreen(cur));
            } else {
                const Point b = m_clicks[1].at;
                const double len = std::max(std::hypot(b.x - a.x, b.y - a.y), 1e-9);
                const double nx = -(b.y - a.y) / len, ny = (b.x - a.x) / len;
                const double r = std::abs((cur.x - a.x) * nx + (cur.y - a.y) * ny);
                p.drawLine(toScreen({a.x + nx * r, a.y + ny * r}), toScreen({b.x + nx * r, b.y + ny * r}));
                p.drawLine(toScreen({a.x - nx * r, a.y - ny * r}), toScreen({b.x - nx * r, b.y - ny * r}));
                drawArc(a, r, 0, 2 * pi);
                drawArc(b, r, 0, 2 * pi);
            }
            break;
        }
        default: break;
        }
    }
    // The cursor's snap
    if (m_tool != Tool::Select) {
        const QPointF q = toScreen(m_cursor.at);
        p.setPen(QPen(m_cursor.point >= 0 ? QColor(240, 130, 20) : QColor(120, 120, 140), 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(q, 5, 5);
    }
    // Coordinates in the corner
    p.setPen(QColor(110, 110, 125));
    p.drawText(QRectF(8, height() - 22, 300, 18), Qt::AlignLeft | Qt::AlignVCenter,
               tr("x %1   y %2 mm   (grid %3 mm)")
                   .arg(m_cursor.at.x, 0, 'f', 2)
                   .arg(m_cursor.at.y, 0, 'f', 2)
                   .arg(step));
}

// ---- Mouse and keys ---------------------------------------------------------------------------

void SketchView::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    const QPointF pos = event->position();
    m_dragFrom = pos;
    m_dragMoved = false;
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::RightButton && (m_tool == Tool::Select || m_clicks.empty()))) {
        m_drag = Drag::Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() == Qt::RightButton) {
        finishChain();  // right-click ends a chain of lines (or what's being drawn)
        m_clicks.clear();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    if (m_tool != Tool::Select) {
        clickTool(snapAt(pos));
        return;
    }
    const auto pick = pickAt(pos);
    if (event->modifiers() & Qt::ControlModifier) {
        if (pick) {
            const auto it = std::find(m_picks.begin(), m_picks.end(), *pick);
            if (it == m_picks.end())
                m_picks.push_back(*pick);
            else
                m_picks.erase(it);
        }
        update();
        return;
    }
    m_picks.clear();
    if (pick)
        m_picks.push_back(*pick);
    // Get ready to drag what was picked.
    m_dragPoints.clear();
    m_dragRadius = -1;
    if (pick && pick->kind == Pick::Point && pick->index != 0) {
        m_dragPoints.push_back({pick->index, m_sketch.points[std::size_t(pick->index)]});
    } else if (pick && pick->kind == Pick::Geo) {
        const Geo& g = m_sketch.geos[std::size_t(pick->index)];
        if (g.kind == GeoKind::Circle) {
            m_dragRadius = pick->index;  // grabbing the edge sizes the circle
        } else {
            for (int p : {g.a, g.b, g.kind == GeoKind::Arc ? g.c : -1})
                if (p > 0)
                    m_dragPoints.push_back({p, m_sketch.points[std::size_t(p)]});
        }
    }
    if (!m_dragPoints.empty() || m_dragRadius >= 0) {
        m_drag = Drag::Move;
        m_dragStartSketch = toSketch(pos);
    }
    update();
}

void SketchView::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    m_cursorPixel = pos;
    m_cursor = m_tool == Tool::Select ? Snap{toSketch(pos), -1} : snapAt(pos);
    if (m_drag == Drag::Pan) {
        const QPointF d = pos - m_dragFrom;
        m_dragFrom = pos;
        m_centre -= QPointF(d.x() / m_scale, -d.y() / m_scale);
        update();
        return;
    }
    if (m_drag == Drag::Move) {
        if (!m_dragMoved) {
            if ((pos - m_dragFrom).manhattanLength() < 3)
                return;
            remember();
            m_dragMoved = true;
        }
        const Point now = toSketch(pos);
        const double dx = now.x - m_dragStartSketch.x, dy = now.y - m_dragStartSketch.y;
        try {
            if (m_dragRadius >= 0) {
                const Geo& g = m_sketch.geos[std::size_t(m_dragRadius)];
                const Point c = m_sketch.points[std::size_t(g.a)];
                m_sketch.radii[std::size_t(g.r)] = std::max(std::hypot(now.x - c.x, now.y - c.y), 1e-3);
                m_solved = solve(m_sketch);
            } else {
                std::vector<std::pair<int, Point>> pull;
                for (const auto& [p, from] : m_dragPoints)
                    pull.push_back({p, Point{from.x + dx, from.y + dy}});
                m_solved = solve(m_sketch, pull);
            }
        } catch (const std::exception&) {
        }
        update();
        return;
    }
    const auto hover = m_tool == Tool::Select ? pickAt(pos) : std::nullopt;
    if (hover != m_hover || m_tool != Tool::Select) {
        m_hover = hover;
        update();
    }
}

void SketchView::mouseReleaseEvent(QMouseEvent*)
{
    if (m_drag == Drag::Move && m_dragMoved)
        solveNow();  // settle without the pull
    m_drag = Drag::None;
    m_dragPoints.clear();
    m_dragRadius = -1;
    setCursor(Qt::CrossCursor);
}

void SketchView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_tool == Tool::Line && !m_clicks.empty()) {
        finishChain();
        return;
    }
    const auto pick = pickAt(event->position());
    if (pick && pick->kind == Pick::Rule)
        editDimension(pick->index);
}

void SketchView::wheelEvent(QWheelEvent* event)
{
    const QPointF pos = event->position();
    const Point before = toSketch(pos);
    const double steps = event->angleDelta().y() / 120.0;
    m_scale = std::clamp(m_scale * std::pow(1.2, steps), 0.05, 2000.0);
    const Point after = toSketch(pos);
    m_centre += QPointF(before.x - after.x, before.y - after.y);
    m_cursor = m_tool == Tool::Select ? Snap{toSketch(pos), -1} : snapAt(pos);
    update();
}

void SketchView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (!m_clicks.empty())
            finishChain();
        else if (m_tool != Tool::Select)
            setTool(Tool::Select);
        else {
            m_picks.clear();
            update();
        }
        return;
    case Qt::Key_Delete:
    case Qt::Key_Backspace: deleteSelection(); return;
    default: QWidget::keyPressEvent(event);
    }
}

void SketchView::resizeEvent(QResizeEvent*)
{
    update();
}

}  // namespace forge::app
