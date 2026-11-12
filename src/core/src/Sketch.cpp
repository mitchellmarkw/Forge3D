// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Sketch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <numbers>
#include <stdexcept>

namespace forge::sketch {

namespace {

constexpr double pi = std::numbers::pi;

// What a rule's a / b / c refer to: 'p' point, 'g' geometry, '-' nothing.
std::array<char, 3> slots(const Constraint& c)
{
    switch (c.rule) {
    case Rule::Horizontal:
    case Rule::Vertical: return c.b < 0 ? std::array<char, 3>{'g', '-', '-'} : std::array<char, 3>{'p', 'p', '-'};
    case Rule::Parallel:
    case Rule::Perpendicular:
    case Rule::Equal:
    case Rule::Tangent: return {'g', 'g', '-'};
    case Rule::PointOn: return {'p', 'g', '-'};
    case Rule::Coincident: return {'p', 'p', '-'};
    case Rule::Symmetric: return {'p', 'p', 'g'};
    case Rule::Midpoint: return {'p', 'p', 'p'};
    case Rule::Fixed: return {'p', '-', '-'};
    case Rule::Distance: return c.b < 0 ? std::array<char, 3>{'g', '-', '-'} : std::array<char, 3>{'p', 'p', '-'};
    case Rule::DistanceX:
    case Rule::DistanceY: return {'p', 'p', '-'};
    case Rule::Radius: return {'g', '-', '-'};
    case Rule::Angle: return c.b < 0 ? std::array<char, 3>{'g', '-', '-'} : std::array<char, 3>{'g', 'g', '-'};
    }
    return {'-', '-', '-'};
}

int* slot(Constraint& c, int k) { return k == 0 ? &c.a : k == 1 ? &c.b : &c.c; }
int slotValue(const Constraint& c, int k) { return k == 0 ? c.a : k == 1 ? c.b : c.c; }

// The state being solved: point coordinates and radii.
struct State {
    std::vector<double> x, y, r;
};

// Variables: every point but the origin (x, y), then every radius.
struct Layout {
    std::size_t points = 0, radii = 0;
    std::size_t size() const { return 2 * (points - 1) + radii; }
};

void toVars(const State& s, std::vector<double>& v)
{
    v.clear();
    for (std::size_t i = 1; i < s.x.size(); ++i) {
        v.push_back(s.x[i]);
        v.push_back(s.y[i]);
    }
    v.insert(v.end(), s.r.begin(), s.r.end());
}

void fromVars(const std::vector<double>& v, State& s)
{
    std::size_t k = 0;
    for (std::size_t i = 1; i < s.x.size(); ++i) {
        s.x[i] = v[k++];
        s.y[i] = v[k++];
    }
    for (double& r : s.r)
        r = v[k++];
}

double wrap(double a)
{
    while (a > pi)
        a -= 2 * pi;
    while (a <= -pi)
        a += 2 * pi;
    return a;
}

struct Residuals {
    const Sketch& sk;
    double L;  // the sketch's size: angle-type rules are scaled to millimetres by it

    double radius(const State& s, const Geo& g) const
    {
        if (g.kind == GeoKind::Circle)
            return g.r >= 0 && std::size_t(g.r) < s.r.size() ? s.r[std::size_t(g.r)] : 0.0;
        return std::hypot(s.x[std::size_t(g.b)] - s.x[std::size_t(g.a)], s.y[std::size_t(g.b)] - s.y[std::size_t(g.a)]);
    }

    // Each rule's residuals (0 when it holds), appended; `starts` records where each begins.
    void operator()(const State& s, std::vector<double>& f, std::vector<std::size_t>* starts) const
    {
        f.clear();
        const auto P = [&s](int i) { return std::pair{s.x[std::size_t(i)], s.y[std::size_t(i)]}; };
        const auto dir = [&](const Geo& g) {
            const auto [ax, ay] = P(g.a);
            const auto [bx, by] = P(g.b);
            return std::pair{bx - ax, by - ay};
        };
        // Arcs keep start and end the same distance from the centre.
        for (const Geo& g : sk.geos)
            if (g.kind == GeoKind::Arc) {
                const auto [cx, cy] = P(g.a);
                const auto [sx, sy] = P(g.b);
                const auto [ex, ey] = P(g.c);
                f.push_back(std::hypot(sx - cx, sy - cy) - std::hypot(ex - cx, ey - cy));
            }
        for (const Constraint& c : sk.constraints) {
            if (starts)
                starts->push_back(f.size());
            const Geo* ga = slots(c)[0] == 'g' && c.a >= 0 ? &sk.geos[std::size_t(c.a)] : nullptr;
            const Geo* gb = slots(c)[1] == 'g' && c.b >= 0 ? &sk.geos[std::size_t(c.b)] : nullptr;
            switch (c.rule) {
            case Rule::Horizontal:
            case Rule::Vertical: {
                int p = c.a, q = c.b;
                if (ga) {
                    p = ga->a;
                    q = ga->b;
                }
                const auto [px, py] = P(p);
                const auto [qx, qy] = P(q);
                f.push_back(c.rule == Rule::Horizontal ? qy - py : qx - px);
                break;
            }
            case Rule::Parallel:
            case Rule::Perpendicular: {
                const auto [ax, ay] = dir(*ga);
                const auto [bx, by] = dir(*gb);
                const double n = std::max(std::hypot(ax, ay) * std::hypot(bx, by), 1e-12);
                f.push_back(L * (c.rule == Rule::Parallel ? (ax * by - ay * bx) : (ax * bx + ay * by)) / n);
                break;
            }
            case Rule::Equal: {
                if (ga->kind == GeoKind::Line && gb->kind == GeoKind::Line) {
                    const auto [ax, ay] = dir(*ga);
                    const auto [bx, by] = dir(*gb);
                    f.push_back(std::hypot(ax, ay) - std::hypot(bx, by));
                } else if (ga->kind != GeoKind::Line && gb->kind != GeoKind::Line) {
                    f.push_back(radius(s, *ga) - radius(s, *gb));
                } else {
                    f.push_back(0.0);
                }
                break;
            }
            case Rule::Tangent: {
                const Geo* line = ga->kind == GeoKind::Line ? ga : gb->kind == GeoKind::Line ? gb : nullptr;
                // Joined at an end (a slot's corners): the distance form has its peak there
                // (no slope to follow), so ask for the same thing another way: the line
                // square to the radius at the joint, or the two centres in line with it.
                const auto endsOf = [](const Geo& g) {
                    return g.kind == GeoKind::Line  ? std::array<int, 2>{g.a, g.b}
                           : g.kind == GeoKind::Arc ? std::array<int, 2>{g.b, g.c}
                                                    : std::array<int, 2>{-1, -1};
                };
                int joint = -1;
                for (int p : endsOf(*ga))
                    for (int q : endsOf(*gb))
                        if (p >= 0 && p == q)
                            joint = p;
                if (joint >= 0) {
                    const auto [jx, jy] = P(joint);
                    if (line) {
                        const Geo* round = line == ga ? gb : ga;
                        const auto [dx, dy] = dir(*line);
                        const auto [cx, cy] = P(round->a);
                        const double len = std::max(std::hypot(dx, dy), 1e-12);
                        f.push_back((dx * (jx - cx) + dy * (jy - cy)) / len);
                    } else {
                        const auto [ax, ay] = P(ga->a);
                        const auto [bx, by] = P(gb->a);
                        const double n = std::max(std::hypot(ax - jx, ay - jy), 1e-12);
                        f.push_back(((ax - jx) * (by - jy) - (ay - jy) * (bx - jx)) / n);
                    }
                    break;
                }
                if (line) {
                    const Geo* round = line == ga ? gb : ga;
                    const auto [dx, dy] = dir(*line);
                    const auto [lx, ly] = P(line->a);
                    const auto [cx, cy] = P(round->a);
                    const double len = std::max(std::hypot(dx, dy), 1e-12);
                    const double side = c.value2 < 0 ? -1.0 : 1.0;
                    f.push_back(side * (dx * (cy - ly) - dy * (cx - lx)) / len - radius(s, *round));
                } else {
                    const auto [ax, ay] = P(ga->a);
                    const auto [bx, by] = P(gb->a);
                    const double d = std::hypot(bx - ax, by - ay);
                    const double ra = radius(s, *ga), rb = radius(s, *gb);
                    f.push_back(c.value < 0 ? d - std::abs(ra - rb) : d - (ra + rb));
                }
                break;
            }
            case Rule::PointOn: {
                const auto [px, py] = P(c.a);
                if (gb->kind == GeoKind::Line) {
                    const auto [dx, dy] = dir(*gb);
                    const auto [lx, ly] = P(gb->a);
                    f.push_back((dx * (py - ly) - dy * (px - lx)) / std::max(std::hypot(dx, dy), 1e-12));
                } else {
                    const auto [cx, cy] = P(gb->a);
                    f.push_back(std::hypot(px - cx, py - cy) - radius(s, *gb));
                }
                break;
            }
            case Rule::Coincident: {
                const auto [ax, ay] = P(c.a);
                const auto [bx, by] = P(c.b);
                f.push_back(bx - ax);
                f.push_back(by - ay);
                break;
            }
            case Rule::Symmetric: {
                const Geo& line = sk.geos[std::size_t(c.c)];
                const auto [dx, dy] = dir(line);
                const auto [lx, ly] = P(line.a);
                const auto [ax, ay] = P(c.a);
                const auto [bx, by] = P(c.b);
                const double len = std::max(std::hypot(dx, dy), 1e-12);
                const double mx = (ax + bx) / 2 - lx, my = (ay + by) / 2 - ly;
                f.push_back((dx * my - dy * mx) / len);                    // the middle is on the line
                f.push_back(((bx - ax) * dx + (by - ay) * dy) / len);      // and the pair is square to it
                break;
            }
            case Rule::Midpoint: {
                const auto [ax, ay] = P(c.a);
                const auto [bx, by] = P(c.b);
                const auto [mx, my] = P(c.c);
                f.push_back((ax + bx) / 2 - mx);
                f.push_back((ay + by) / 2 - my);
                break;
            }
            case Rule::Fixed: {
                const auto [px, py] = P(c.a);
                f.push_back(px - c.value);
                f.push_back(py - c.value2);
                break;
            }
            case Rule::Distance: {
                if (ga) {
                    const auto [dx, dy] = dir(*ga);
                    f.push_back(std::hypot(dx, dy) - c.value);
                } else {
                    const auto [ax, ay] = P(c.a);
                    const auto [bx, by] = P(c.b);
                    f.push_back(std::hypot(bx - ax, by - ay) - c.value);
                }
                break;
            }
            case Rule::DistanceX:
            case Rule::DistanceY: {
                const auto [ax, ay] = P(c.a);
                const auto [bx, by] = P(c.b);
                f.push_back((c.rule == Rule::DistanceX ? bx - ax : by - ay) - c.value);
                break;
            }
            case Rule::Radius: f.push_back(radius(s, *ga) - c.value); break;
            case Rule::Angle: {
                const auto [ax, ay] = dir(*ga);
                double from = 0.0;
                if (gb) {
                    const auto [bx, by] = dir(*gb);
                    from = std::atan2(ay, ax);
                    f.push_back(L * wrap(std::atan2(by, bx) - from - c.value * pi / 180.0) / pi);
                } else {
                    f.push_back(L * wrap(std::atan2(ay, ax) - c.value * pi / 180.0) / pi);
                }
                break;
            }
            }
        }
    }
};

State stateOf(const Sketch& s)
{
    State st;
    for (const Point& p : s.points) {
        st.x.push_back(p.x);
        st.y.push_back(p.y);
    }
    st.r = s.radii;
    return st;
}

double sizeOf(const Sketch& s)
{
    double lo = 0, hi = 0;
    for (const Point& p : s.points) {
        lo = std::min({lo, p.x, p.y});
        hi = std::max({hi, p.x, p.y});
    }
    for (double r : s.radii)
        hi = std::max(hi, std::abs(r));
    return std::max(1.0, hi - lo);
}

void checkRefs(const Sketch& s)
{
    const auto np = int(s.points.size()), ng = int(s.geos.size()), nr = int(s.radii.size());
    for (const Geo& g : s.geos) {
        const int need = g.kind == GeoKind::Line ? 2 : g.kind == GeoKind::Circle ? 1 : 3;
        for (int k = 0; k < need; ++k) {
            const int p = k == 0 ? g.a : k == 1 ? g.b : g.c;
            if (p < 0 || p >= np)
                throw std::invalid_argument("The sketch is damaged (a curve's point is missing)");
        }
        if (g.kind == GeoKind::Circle && (g.r < 0 || g.r >= nr))
            throw std::invalid_argument("The sketch is damaged (a circle's radius is missing)");
    }
    for (const Constraint& c : s.constraints) {
        const auto sl = slots(c);
        for (int k = 0; k < 3; ++k) {
            const int v = slotValue(c, k);
            if (sl[std::size_t(k)] == 'p' && (v < 0 || v >= np))
                throw std::invalid_argument("The sketch is damaged (a rule's point is missing)");
            if (sl[std::size_t(k)] == 'g' && (v < 0 || v >= ng))
                throw std::invalid_argument("The sketch is damaged (a rule's curve is missing)");
        }
        const auto kindOf = [&s](int g) { return s.geos[std::size_t(g)].kind; };
        switch (c.rule) {
        case Rule::Parallel:
        case Rule::Perpendicular:
            if (kindOf(c.a) != GeoKind::Line || kindOf(c.b) != GeoKind::Line)
                throw std::invalid_argument("Parallel and square go between lines");
            break;
        case Rule::Symmetric:
            if (kindOf(c.c) != GeoKind::Line)
                throw std::invalid_argument("Symmetric needs a line to mirror about");
            break;
        case Rule::Radius:
            if (kindOf(c.a) == GeoKind::Line)
                throw std::invalid_argument("A radius goes on a circle or arc");
            break;
        case Rule::Tangent:
            if (kindOf(c.a) == GeoKind::Line && kindOf(c.b) == GeoKind::Line)
                throw std::invalid_argument("Tangent needs a circle or arc");
            break;
        default:
            if (slots(c)[0] == 'g' && (c.rule == Rule::Horizontal || c.rule == Rule::Vertical ||
                                        c.rule == Rule::Distance || c.rule == Rule::Angle) &&
                kindOf(c.a) != GeoKind::Line)
                throw std::invalid_argument("That rule goes on a line");
            break;
        }
    }
}

// Linear solve of a small dense symmetric system (Gaussian elimination, pivoting).
bool solveLinear(std::vector<double> A, std::vector<double> b, std::size_t n, std::vector<double>& x)
{
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        for (std::size_t r = col + 1; r < n; ++r)
            if (std::abs(A[r * n + col]) > std::abs(A[piv * n + col]))
                piv = r;
        if (std::abs(A[piv * n + col]) < 1e-300)
            return false;
        if (piv != col) {
            for (std::size_t k = 0; k < n; ++k)
                std::swap(A[col * n + k], A[piv * n + k]);
            std::swap(b[col], b[piv]);
        }
        for (std::size_t r = col + 1; r < n; ++r) {
            const double m = A[r * n + col] / A[col * n + col];
            if (m == 0.0)
                continue;
            for (std::size_t k = col; k < n; ++k)
                A[r * n + k] -= m * A[col * n + k];
            b[r] -= m * b[col];
        }
    }
    x.assign(n, 0.0);
    for (std::size_t i = n; i-- > 0;) {
        double sum = b[i];
        for (std::size_t k = i + 1; k < n; ++k)
            sum -= A[i * n + k] * x[k];
        x[i] = sum / A[i * n + i];
    }
    return true;
}

int rankOf(std::vector<double> J, std::size_t m, std::size_t n)
{
    double biggest = 0.0;
    for (double v : J)
        biggest = std::max(biggest, std::abs(v));
    const double tol = std::max(biggest, 1.0) * 1e-9;
    int rank = 0;
    std::size_t row = 0;
    for (std::size_t col = 0; col < n && row < m; ++col) {
        std::size_t piv = row;
        for (std::size_t r = row + 1; r < m; ++r)
            if (std::abs(J[r * n + col]) > std::abs(J[piv * n + col]))
                piv = r;
        if (std::abs(J[piv * n + col]) <= tol)
            continue;
        for (std::size_t k = 0; k < n; ++k)
            std::swap(J[row * n + k], J[piv * n + k]);
        for (std::size_t r = row + 1; r < m; ++r) {
            const double f = J[r * n + col] / J[row * n + col];
            for (std::size_t k = col; k < n; ++k)
                J[r * n + k] -= f * J[row * n + k];
        }
        ++row;
        ++rank;
    }
    return rank;
}

template <typename F_t>
void jacobian(const F_t& F, State st, const std::vector<double>& vars, std::size_t m, double h,
              std::vector<double>& J)
{
    const std::size_t n = vars.size();
    J.assign(m * n, 0.0);
    std::vector<double> v = vars, fp, fm;
    for (std::size_t j = 0; j < n; ++j) {
        const double keep = v[j];
        v[j] = keep + h;
        fromVars(v, st);
        F(st, fp, nullptr);
        v[j] = keep - h;
        fromVars(v, st);
        F(st, fm, nullptr);
        v[j] = keep;
        for (std::size_t i = 0; i < m; ++i)
            J[i * n + j] = (fp[i] - fm[i]) / (2 * h);
    }
}

}  // namespace

bool isDimension(Rule rule)
{
    switch (rule) {
    case Rule::Distance:
    case Rule::DistanceX:
    case Rule::DistanceY:
    case Rule::Radius:
    case Rule::Angle: return true;
    default: return false;
    }
}

double radiusOf(const Sketch& s, const Geo& g)
{
    if (g.kind == GeoKind::Circle)
        return g.r >= 0 && std::size_t(g.r) < s.radii.size() ? s.radii[std::size_t(g.r)] : 0.0;
    if (g.kind == GeoKind::Arc)
        return std::hypot(s.points[std::size_t(g.b)].x - s.points[std::size_t(g.a)].x,
                          s.points[std::size_t(g.b)].y - s.points[std::size_t(g.a)].y);
    return 0.0;
}

Solved solve(Sketch& sketch, const std::vector<std::pair<int, Point>>& pull)
{
    checkRefs(sketch);
    const double L = sizeOf(sketch);
    const Residuals rules{sketch, L};
    State st = stateOf(sketch);
    std::vector<double> vars;
    toVars(st, vars);
    const std::size_t n = vars.size();
    const double pullWeight = 0.05;
    // All residuals: the rules, then the pulls (weak).
    const auto all = [&](const State& s, std::vector<double>& f) {
        rules(s, f, nullptr);
        for (const auto& [p, to] : pull) {
            f.push_back(pullWeight * (s.x[std::size_t(p)] - to.x));
            f.push_back(pullWeight * (s.y[std::size_t(p)] - to.y));
        }
    };
    std::vector<double> f;
    all(st, f);
    const std::size_t m = f.size();
    const auto cost = [](const std::vector<double>& r) {
        double c = 0;
        for (double v : r)
            c += v * v;
        return c;
    };
    const double tol = 1e-10 * L;
    if (n > 0 && m > 0) {
        double c = cost(f);
        double lambda = 1e-3;
        const double h = 1e-7 * L;
        std::vector<double> J, A(n * n), g(n), delta, trial, ft;
        for (int iter = 0; iter < 200 && c > tol * tol; ++iter) {
            jacobian(
                [&](const State& s, std::vector<double>& out, std::vector<std::size_t>*) { all(s, out); }, st, vars,
                m, h, J);
            std::fill(A.begin(), A.end(), 0.0);
            std::fill(g.begin(), g.end(), 0.0);
            for (std::size_t i = 0; i < m; ++i)
                for (std::size_t a = 0; a < n; ++a) {
                    const double ja = J[i * n + a];
                    if (ja == 0.0)
                        continue;
                    g[a] += ja * f[i];
                    for (std::size_t b = a; b < n; ++b)
                        A[a * n + b] += ja * J[i * n + b];
                }
            for (std::size_t a = 0; a < n; ++a)
                for (std::size_t b = 0; b < a; ++b)
                    A[a * n + b] = A[b * n + a];
            bool improved = false;
            while (lambda < 1e14) {
                std::vector<double> M = A;
                for (std::size_t a = 0; a < n; ++a)
                    M[a * n + a] += lambda * (A[a * n + a] + 1e-9);
                std::vector<double> rhs(n);
                for (std::size_t a = 0; a < n; ++a)
                    rhs[a] = -g[a];
                if (!solveLinear(M, rhs, n, delta)) {
                    lambda *= 10;
                    continue;
                }
                trial = vars;
                for (std::size_t a = 0; a < n; ++a)
                    trial[a] += delta[a];
                State ts = st;
                fromVars(trial, ts);
                all(ts, ft);
                const double ct = cost(ft);
                if (ct < c) {
                    vars = trial;
                    st = ts;
                    f = ft;
                    c = ct;
                    lambda = std::max(lambda / 3, 1e-12);
                    improved = true;
                    break;
                }
                lambda *= 4;
            }
            if (!improved)
                break;
        }
    }
    // Write back, then judge the rules on their own (without pulls).
    for (std::size_t i = 0; i < sketch.points.size(); ++i)
        sketch.points[i] = {st.x[i], st.y[i]};
    sketch.radii = st.r;
    Solved out;
    std::vector<double> r;
    std::vector<std::size_t> starts;
    rules(st, r, &starts);
    const double ok = 1e-6 * L;
    for (std::size_t k = 0; k < starts.size(); ++k) {
        const std::size_t end = k + 1 < starts.size() ? starts[k + 1] : r.size();
        for (std::size_t i = starts[k]; i < end; ++i)
            if (std::abs(r[i]) > ok) {
                out.failing.push_back(int(k));
                break;
            }
    }
    out.ok = out.failing.empty();
    out.freedom = freedom(sketch);
    return out;
}

int freedom(const Sketch& sketch)
{
    checkRefs(sketch);
    const double L = sizeOf(sketch);
    const Residuals rules{sketch, L};
    const State st = stateOf(sketch);
    std::vector<double> vars, f;
    toVars(st, vars);
    rules(st, f, nullptr);
    const std::size_t n = vars.size(), m = f.size();
    if (n == 0)
        return 0;
    if (m == 0)
        return int(n);
    std::vector<double> J;
    jacobian(rules, st, vars, m, 1e-7 * L, J);
    return int(n) - rankOf(J, m, n);
}

int addPoint(Sketch& s, Point p)
{
    s.points.push_back(p);
    return int(s.points.size()) - 1;
}

int mergePoints(Sketch& s, int into, int from)
{
    if (into == from || from <= 0 && into <= 0)
        return into;
    if (from == 0)
        std::swap(into, from);  // the origin stays
    const auto remap = [&](int& p) {
        if (p == from)
            p = into;
        if (p > from)
            --p;
    };
    for (Geo& g : s.geos) {
        remap(g.a);
        if (g.kind != GeoKind::Circle)
            remap(g.b);
        if (g.kind == GeoKind::Arc)
            remap(g.c);
    }
    std::vector<Constraint> kept;
    for (Constraint c : s.constraints) {
        const auto sl = slots(c);
        for (int k = 0; k < 3; ++k)
            if (sl[std::size_t(k)] == 'p')
                remap(*slot(c, k));
        const bool degenerate = (c.rule == Rule::Coincident || ((c.rule == Rule::Distance || c.rule == Rule::DistanceX ||
                                                                  c.rule == Rule::DistanceY || c.rule == Rule::Horizontal ||
                                                                  c.rule == Rule::Vertical) &&
                                                                 sl[1] == 'p')) &&
                                c.a == c.b;
        if (!degenerate)
            kept.push_back(c);
    }
    s.constraints = std::move(kept);
    s.points.erase(s.points.begin() + from);
    return into;
}

void removeConstraints(Sketch& s, std::vector<int> rules)
{
    std::sort(rules.begin(), rules.end());
    rules.erase(std::unique(rules.begin(), rules.end()), rules.end());
    for (std::size_t i = rules.size(); i-- > 0;)
        if (rules[i] >= 0 && std::size_t(rules[i]) < s.constraints.size())
            s.constraints.erase(s.constraints.begin() + rules[i]);
}

void removeGeos(Sketch& s, std::vector<int> geos)
{
    std::sort(geos.begin(), geos.end());
    geos.erase(std::unique(geos.begin(), geos.end()), geos.end());
    std::vector<int> newIndex(s.geos.size());
    std::vector<Geo> kept;
    for (std::size_t i = 0; i < s.geos.size(); ++i) {
        if (std::binary_search(geos.begin(), geos.end(), int(i))) {
            newIndex[i] = -1;
        } else {
            newIndex[i] = int(kept.size());
            kept.push_back(s.geos[i]);
        }
    }
    s.geos = std::move(kept);
    std::vector<Constraint> rules;
    for (Constraint c : s.constraints) {
        const auto sl = slots(c);
        bool keep = true;
        for (int k = 0; k < 3; ++k)
            if (sl[std::size_t(k)] == 'g') {
                int* v = slot(c, k);
                if (*v >= 0)
                    *v = newIndex[std::size_t(*v)];
                if (*v < 0)
                    keep = false;
            }
        if (keep)
            rules.push_back(c);
    }
    s.constraints = std::move(rules);
    // Points and radii nothing uses any more.
    std::vector<bool> used(s.points.size(), false), radiusUsed(s.radii.size(), false);
    used[0] = true;
    for (const Geo& g : s.geos) {
        used[std::size_t(g.a)] = true;
        if (g.kind != GeoKind::Circle)
            used[std::size_t(g.b)] = true;
        if (g.kind == GeoKind::Arc)
            used[std::size_t(g.c)] = true;
        if (g.kind == GeoKind::Circle)
            radiusUsed[std::size_t(g.r)] = true;
    }
    for (std::size_t i = s.points.size(); i-- > 1;)
        if (!used[i]) {
            // Drop rules on it, then the point.
            std::vector<Constraint> left;
            for (const Constraint& c : s.constraints) {
                const auto sl = slots(c);
                bool refers = false;
                for (int k = 0; k < 3; ++k)
                    refers = refers || (sl[std::size_t(k)] == 'p' && slotValue(c, k) == int(i));
                if (!refers)
                    left.push_back(c);
            }
            s.constraints = std::move(left);
            mergePoints(s, 0, int(i));  // nothing refers to it: this just removes it
            // mergePoints keeps the origin; re-add nothing.
        }
    std::vector<int> radiusIndex(s.radii.size(), -1);
    std::vector<double> radii;
    for (std::size_t i = 0; i < s.radii.size(); ++i)
        if (radiusUsed[i]) {
            radiusIndex[i] = int(radii.size());
            radii.push_back(s.radii[i]);
        }
    s.radii = std::move(radii);
    for (Geo& g : s.geos)
        if (g.kind == GeoKind::Circle)
            g.r = radiusIndex[std::size_t(g.r)];
}

double measure(const Sketch& s, const Constraint& c)
{
    const auto P = [&s](int i) { return s.points[std::size_t(i)]; };
    switch (c.rule) {
    case Rule::Distance:
        if (c.b < 0) {
            const Geo& g = s.geos[std::size_t(c.a)];
            return std::hypot(P(g.b).x - P(g.a).x, P(g.b).y - P(g.a).y);
        }
        return std::hypot(P(c.b).x - P(c.a).x, P(c.b).y - P(c.a).y);
    case Rule::DistanceX: return P(c.b).x - P(c.a).x;
    case Rule::DistanceY: return P(c.b).y - P(c.a).y;
    case Rule::Radius: return radiusOf(s, s.geos[std::size_t(c.a)]);
    case Rule::Angle: {
        const Geo& ga = s.geos[std::size_t(c.a)];
        const double a = std::atan2(P(ga.b).y - P(ga.a).y, P(ga.b).x - P(ga.a).x);
        if (c.b < 0)
            return a * 180.0 / pi;
        const Geo& gb = s.geos[std::size_t(c.b)];
        const double b = std::atan2(P(gb.b).y - P(gb.a).y, P(gb.b).x - P(gb.a).x);
        return wrap(b - a) * 180.0 / pi;
    }
    default: return c.value;
    }
}

Constraint makeRule(const Sketch& s, Rule rule, int a, int b, int c)
{
    Constraint out{rule, a, b, c, 0.0, 0.0};
    if (isDimension(rule)) {
        out.value = measure(s, out);
    } else if (rule == Rule::Fixed) {
        out.value = s.points[std::size_t(a)].x;
        out.value2 = s.points[std::size_t(a)].y;
    } else if (rule == Rule::Tangent) {
        const Geo& ga = s.geos[std::size_t(a)];
        const Geo& gb = s.geos[std::size_t(b)];
        const Geo* line = ga.kind == GeoKind::Line ? &ga : gb.kind == GeoKind::Line ? &gb : nullptr;
        if (line) {
            // Which side of the line the circle is on now.
            const Geo& round = line == &ga ? gb : ga;
            const Point p = s.points[std::size_t(line->a)], q = s.points[std::size_t(line->b)];
            const Point m = s.points[std::size_t(round.a)];
            out.value2 = (q.x - p.x) * (m.y - p.y) - (q.y - p.y) * (m.x - p.x) >= 0 ? 1.0 : -1.0;
            out.value = 1.0;
        } else {
            // Touching outside, or one inside the other.
            const Point m1 = s.points[std::size_t(ga.a)], m2 = s.points[std::size_t(gb.a)];
            const double d = std::hypot(m2.x - m1.x, m2.y - m1.y);
            const double ra = radiusOf(s, ga), rb = radiusOf(s, gb);
            out.value = std::abs(d - (ra + rb)) <= std::abs(d - std::abs(ra - rb)) ? 1.0 : -1.0;
        }
    }
    return out;
}

bool scale(Sketch& s, double sx, double sy)
{
    const bool even = std::abs(sx - sy) <= 1e-12 * std::max(std::abs(sx), std::abs(sy));
    if (!even) {
        for (const Geo& g : s.geos)
            if (g.kind != GeoKind::Line)
                return false;
        for (const Constraint& c : s.constraints)
            if (c.rule == Rule::Distance || c.rule == Rule::Angle || c.rule == Rule::Radius ||
                c.rule == Rule::Tangent)
                return false;
    }
    for (Point& p : s.points) {
        p.x *= sx;
        p.y *= sy;
    }
    for (double& r : s.radii)
        r *= sx;
    for (Constraint& c : s.constraints) {
        switch (c.rule) {
        case Rule::Distance:
        case Rule::Radius: c.value *= sx; break;
        case Rule::DistanceX: c.value *= sx; break;
        case Rule::DistanceY: c.value *= sy; break;
        case Rule::Fixed:
            c.value *= sx;
            c.value2 *= sy;
            break;
        default: break;
        }
    }
    return true;
}

std::vector<int> looseEnds(const Sketch& s)
{
    std::map<int, int> uses;
    for (const Geo& g : s.geos) {
        if (g.construction)
            continue;
        if (g.kind == GeoKind::Line) {
            ++uses[g.a];
            ++uses[g.b];
        } else if (g.kind == GeoKind::Arc) {
            ++uses[g.b];
            ++uses[g.c];
        }
    }
    std::vector<int> out;
    for (const auto& [p, n] : uses)
        if (n == 1)
            out.push_back(p);
    return out;
}

std::string describe(const Sketch& s, const Constraint& c)
{
    const auto num = [](double v) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.2f", v);
        std::string t = buf;
        while (!t.empty() && t.back() == '0')
            t.pop_back();
        if (!t.empty() && t.back() == '.')
            t.pop_back();
        return t;
    };
    (void)s;
    switch (c.rule) {
    case Rule::Horizontal: return "Horizontal";
    case Rule::Vertical: return "Vertical";
    case Rule::Parallel: return "Parallel";
    case Rule::Perpendicular: return "Square";
    case Rule::Equal: return "Equal";
    case Rule::Tangent: return "Tangent";
    case Rule::PointOn: return "On";
    case Rule::Coincident: return "Joined";
    case Rule::Symmetric: return "Symmetric";
    case Rule::Midpoint: return "Middle";
    case Rule::Fixed: return "Locked";
    case Rule::Distance: return num(c.value) + " mm";
    case Rule::DistanceX: return "\xE2\x86\x94 " + num(std::abs(c.value)) + " mm";
    case Rule::DistanceY: return "\xE2\x86\x95 " + num(std::abs(c.value)) + " mm";
    case Rule::Radius: return "R" + num(c.value);
    case Rule::Angle: return num(c.value) + "\xC2\xB0";
    }
    return {};
}

}  // namespace forge::sketch
