// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Building.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <stdexcept>

namespace forge::building {

namespace {

using V3 = std::array<double, 3>;
constexpr double kPi = 3.14159265358979323846;

V3 add(const V3& a, const V3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V3 mul(const V3& a, double k) { return {a[0] * k, a[1] * k, a[2] * k}; }
V3 cross(const V3& a, const V3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

std::string mm(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f mm", v);
    return buf;
}

// A convex outline (counter-clockwise in u, v) run from `a` along `axis` (its length the
// run's): a closed solid. u x v must be the axis's way.
MeshSolid prism(const std::vector<std::array<double, 2>>& outline, const V3& a, const V3& axis, const V3& u,
                const V3& v)
{
    Mesh m;
    const std::size_t n = outline.size();
    for (int end = 0; end < 2; ++end)
        for (const auto& p : outline) {
            const V3 q = add(add(a, mul(axis, end)), add(mul(u, p[0]), mul(v, p[1])));
            for (double c : q)
                m.positions.push_back(float(c));
        }
    const auto at = [n](int end, std::size_t i) { return std::uint32_t(std::size_t(end) * n + i % n); };
    for (std::size_t i = 1; i + 1 < n; ++i) {
        for (std::uint32_t k : {at(1, 0), at(1, i), at(1, i + 1)})  // the far end, facing out along the axis
            m.indices.push_back(k);
        for (std::uint32_t k : {at(0, 0), at(0, i + 1), at(0, i)})
            m.indices.push_back(k);
    }
    for (std::size_t i = 0; i < n; ++i)
        for (std::uint32_t k : {at(0, i), at(0, i + 1), at(1, i + 1), at(0, i), at(1, i + 1), at(1, i)})
            m.indices.push_back(k);
    return meshkernel::fromTriangles(m.positions, m.indices);
}

MeshSolid box(const V3& lo, const V3& hi)
{
    return prism({{lo[0], lo[1]}, {hi[0], lo[1]}, {hi[0], hi[1]}, {lo[0], hi[1]}}, {0, 0, lo[2]}, {0, 0, hi[2] - lo[2]},
                 {1, 0, 0}, {0, 1, 0});
}

// A box in a wall's terms: from a to b along it, z0 to z1 up, out0 to out1 out of its
// outside face.
MeshSolid wallBox(const Face& f, double a, double b, double z0, double z1, double out0, double out1)
{
    const V3 along{f.along[0], f.along[1], f.along[2]}, out{f.out[0], f.out[1], f.out[2]};
    const V3 start{f.start[0], f.start[1], f.start[2]};
    // u = along, v = out: u x v = along x out = -up for every wall (out is along x up
    // turned): so run it downwards from z1.
    const V3 axisWay = cross(along, out);  // straight down or up
    const double from = axisWay[2] < 0 ? z1 : z0;
    return prism({{a, out0}, {b, out0}, {b, out1}, {a, out1}}, add(start, {0, 0, from}), mul(axisWay, z1 - z0), along,
                 out);
}

// A wall's face made rough (slice 116): a slab from `back` (out of the face; negative: in
// it) to `out(u, v)` (model mm out of the face), u from -e0 to the length + e1, v from 0 to
// `height`, on a grid `step` apart.
MeshSolid faceSkin(const Face& f, double e0, double e1, double height, double back, double step,
                   const std::function<double(double, double)>& out)
{
    const int nu = std::max(1, int(std::ceil((f.length + e0 + e1) / step)));
    const int nv = std::max(1, int(std::ceil(height / step)));
    const double du = (f.length + e0 + e1) / nu, dv = height / nv;
    const V3 along{f.along[0], f.along[1], f.along[2]}, outWay{f.out[0], f.out[1], f.out[2]};
    const V3 start{f.start[0], f.start[1], f.start[2]};
    Mesh m;
    const auto put = [&](double u, double v, double w) {
        const V3 p = add(add(start, mul(along, u)), add(V3{0, 0, v}, mul(outWay, w)));
        for (double c : p)
            m.positions.push_back(float(c));
        return std::uint32_t(m.positions.size() / 3 - 1);
    };
    std::vector<std::uint32_t> front, rear;
    for (int j = 0; j <= nv; ++j)
        for (int i = 0; i <= nu; ++i) {
            const double u = -e0 + i * du, v = j * dv;
            front.push_back(put(u, v, std::max(out(u, v), back + 0.05)));
            rear.push_back(put(u, v, back));
        }
    const auto at = [nu](const std::vector<std::uint32_t>& g, int i, int j) { return g[std::size_t(j * (nu + 1) + i)]; };
    const auto pos = [&](std::uint32_t i) { return V3{m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]}; };
    // Two triangles for a quad, turned to face `facing`.
    const auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d, const V3& facing) {
        const V3 pa = pos(a), pb = pos(b), pc = pos(c);
        const V3 n = cross({pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]}, {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]});
        if (dot(n, facing) < 0)
            std::swap(b, d);
        for (std::uint32_t k : {a, b, c, a, c, d})
            m.indices.push_back(k);
    };
    const V3 up{0, 0, 1}, down{0, 0, -1}, left = mul(along, -1), in = mul(outWay, -1);
    for (int j = 0; j < nv; ++j)
        for (int i = 0; i < nu; ++i) {
            quad(at(front, i, j), at(front, i + 1, j), at(front, i + 1, j + 1), at(front, i, j + 1), outWay);
            quad(at(rear, i, j), at(rear, i + 1, j), at(rear, i + 1, j + 1), at(rear, i, j + 1), in);
        }
    for (int i = 0; i < nu; ++i) {
        quad(at(front, i, 0), at(front, i + 1, 0), at(rear, i + 1, 0), at(rear, i, 0), down);
        quad(at(front, i, nv), at(front, i + 1, nv), at(rear, i + 1, nv), at(rear, i, nv), up);
    }
    for (int j = 0; j < nv; ++j) {
        quad(at(front, 0, j), at(front, 0, j + 1), at(rear, 0, j + 1), at(rear, 0, j), left);
        quad(at(front, nu, j), at(front, nu, j + 1), at(rear, nu, j + 1), at(rear, nu, j), along);
    }
    return meshkernel::fromTriangles(m.positions, m.indices);
}

// Hashed noise in 2D, -1..1, smooth.
double noise2(double x, double y, std::uint32_t seed)
{
    const auto h = [seed](int i, int j) {
        std::uint32_t v = std::uint32_t(i) * 374761393u + std::uint32_t(j) * 668265263u + seed * 2246822519u;
        v = (v ^ (v >> 13)) * 1274126177u;
        return double((v ^ (v >> 16)) & 0xFFFF) / 32767.5 - 1.0;
    };
    const int i = int(std::floor(x)), j = int(std::floor(y));
    const double fx = x - i, fy = y - j, sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const double a = h(i, j) + (h(i + 1, j) - h(i, j)) * sx, b = h(i, j + 1) + (h(i + 1, j + 1) - h(i, j + 1)) * sx;
    return a + (b - a) * sy;
}

Mesh meshOf(const MeshSolid& s)
{
    Mesh m;
    meshkernel::triangles(s, m.positions, m.indices);
    return m;
}

double thicknessOf(const BuildingSpec& s)
{
    return s.material == WallMaterial::Logs ? s.unitHeight : s.wallThickness;
}

using P2 = std::array<double, 2>;
using Ring = std::vector<P2>;

// A ring moved in towards its material (on its left) by d: each side moved, the corners where
// the moved sides meet.
Ring offsetRing(const Ring& r, double d)
{
    const std::size_t n = r.size();
    Ring out(n);
    for (std::size_t i = 0; i < n; ++i) {
        const P2 prev = r[(i + n - 1) % n], cur = r[i], next = r[(i + 1) % n];
        const auto unit2 = [](P2 v) {
            const double l = std::hypot(v[0], v[1]);
            return l > 1e-12 ? P2{v[0] / l, v[1] / l} : P2{1, 0};
        };
        const P2 e1 = unit2({cur[0] - prev[0], cur[1] - prev[1]}), e2 = unit2({next[0] - cur[0], next[1] - cur[1]});
        const P2 n1{-e1[1], e1[0]}, n2{-e2[1], e2[0]};  // their left: the material's side
        const double c1 = n1[0] * prev[0] + n1[1] * prev[1] + d, c2 = n2[0] * cur[0] + n2[1] * cur[1] + d;
        const double det = n1[0] * n2[1] - n1[1] * n2[0];
        out[i] = std::abs(det) < 1e-12 ? P2{cur[0] + n1[0] * d, cur[1] + n1[1] * d}
                                       : P2{(c1 * n2[1] - c2 * n1[1]) / det, (n1[0] * c2 - n2[0] * c1) / det};
    }
    return out;
}

// What's inside the rings (even-odd; their sides along X or Y) as rectangles: x0, y0, x1, y1.
std::vector<std::array<double, 4>> rectsOf(const std::vector<Ring>& rings)
{
    std::vector<double> xs;
    for (const Ring& r : rings)
        for (const P2& p : r)
            xs.push_back(p[0]);
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end(), [](double a, double b) { return std::abs(a - b) < 1e-9; }), xs.end());
    std::vector<std::array<double, 4>> out;
    std::vector<double> lastYs;
    for (std::size_t i = 0; i + 1 < xs.size(); ++i) {
        const double xm = (xs[i] + xs[i + 1]) / 2;
        std::vector<double> ys;
        for (const Ring& r : rings)
            for (std::size_t j = 0; j < r.size(); ++j) {
                const P2 a = r[j], b = r[(j + 1) % r.size()];
                if (std::abs(a[1] - b[1]) < 1e-9 && std::min(a[0], b[0]) < xm && xm < std::max(a[0], b[0]))
                    ys.push_back(a[1]);
            }
        std::sort(ys.begin(), ys.end());
        // The same as the slab before: widen its rectangles.
        const bool same = ys.size() == lastYs.size() &&
                          std::equal(ys.begin(), ys.end(), lastYs.begin(), [](double a, double b) { return std::abs(a - b) < 1e-9; });
        if (same && !ys.empty()) {
            for (std::size_t k = out.size() - ys.size() / 2; k < out.size(); ++k)
                out[k][2] = xs[i + 1];
        } else {
            for (std::size_t k = 0; k + 1 < ys.size(); k += 2)
                out.push_back({xs[i], ys[k], xs[i + 1], ys[k + 1]});
        }
        lastYs = ys;
    }
    return out;
}

MeshSolid regionOf(const std::vector<Ring>& rings, double z0, double z1)
{
    std::vector<MeshSolid> boxes;
    for (const auto& r : rectsOf(rings))
        boxes.push_back(box({r[0], r[1], z0}, {r[2], r[3], z1}));
    return boxes.size() == 1 ? boxes[0] : meshkernel::merged(boxes);
}

// A wall: its face, whether its ends are outside corners (a turn to the left, walking the
// outline) or inside ones, and whether it runs along X.
struct Wall {
    Face f;
    bool convexStart = true, convexEnd = true, alongX = true;
};

std::vector<Ring> modelRings(const BuildingSpec& s)
{
    std::vector<Ring> rings = outlines(s);
    for (Ring& r : rings)
        for (P2& p : r)
            p = {p[0] / s.scale, p[1] / s.scale};
    return rings;
}

std::vector<Wall> wallsOf(const BuildingSpec& s)
{
    std::vector<Wall> out;
    const double H = s.height / s.scale;
    for (const Ring& r : modelRings(s)) {
        const std::size_t n = r.size();
        for (std::size_t i = 0; i < n; ++i) {
            const P2 prev = r[(i + n - 1) % n], a = r[i], b = r[(i + 1) % n], next = r[(i + 2) % n];
            const double len = std::hypot(b[0] - a[0], b[1] - a[1]);
            const V3 along{(b[0] - a[0]) / len, (b[1] - a[1]) / len, 0};
            Wall w;
            w.f.start = {a[0], a[1], 0};
            w.f.along = along;
            w.f.out = {along[1], -along[0], 0};
            w.f.length = len;
            w.f.height = H;
            const auto turn = [](P2 p, P2 q, P2 r2) {
                return (q[0] - p[0]) * (r2[1] - q[1]) - (q[1] - p[1]) * (r2[0] - q[0]);
            };
            w.convexStart = turn(prev, a, b) > 0;
            w.convexEnd = turn(a, b, next) > 0;
            w.alongX = std::abs(along[0]) > 0.5;
            out.push_back(w);
        }
    }
    return out;
}

}  // namespace

const char* title(BuildingShape s)
{
    switch (s) {
    case BuildingShape::Rectangle: return "Rectangle";
    case BuildingShape::L: return "L-shape";
    case BuildingShape::T: return "T-shape";
    case BuildingShape::U: return "U-shape";
    case BuildingShape::Courtyard: return "Courtyard (wings all round)";
    }
    return "";
}

std::vector<std::vector<std::array<double, 2>>> outlines(const BuildingSpec& s)
{
    const double w = s.width / 2, d = s.depth / 2, a = s.wingDepth, b = s.wingWidth;
    switch (s.shape) {
    case BuildingShape::Rectangle: break;
    case BuildingShape::L: return {{{-w, -d}, {w, -d}, {w, -d + a}, {-w + b, -d + a}, {-w + b, d}, {-w, d}}};
    case BuildingShape::T:
        return {{{-b / 2, -d}, {b / 2, -d}, {b / 2, d - a}, {w, d - a}, {w, d}, {-w, d}, {-w, d - a}, {-b / 2, d - a}}};
    case BuildingShape::U:
        return {{{-w, -d}, {-w + b, -d}, {-w + b, d - a}, {w - b, d - a}, {w - b, -d}, {w, -d}, {w, d}, {-w, d}}};
    case BuildingShape::Courtyard:
        return {{{-w, -d}, {w, -d}, {w, d}, {-w, d}}, {{-w + b, -d + a}, {-w + b, d - a}, {w - b, d - a}, {w - b, -d + a}}};
    }
    return {{{-w, -d}, {w, -d}, {w, d}, {-w, d}}};
}

int wallCount(const BuildingSpec& s)
{
    int n = 0;
    for (const auto& r : outlines(s))
        n += int(r.size());
    return n;
}

std::string wallName(const BuildingSpec& s, int wall)
{
    const auto rings = outlines(s);
    int i = wall;
    for (std::size_t ring = 0; ring < rings.size(); ++ring) {
        const int n = int(rings[ring].size());
        if (i < n) {
            const P2 a = rings[ring][std::size_t(i)], b = rings[ring][std::size_t((i + 1) % n)];
            // Which way its outside faces.
            const double ox = b[1] - a[1], oy = -(b[0] - a[0]);
            const char* side = std::abs(ox) > std::abs(oy) ? (ox > 0 ? "right" : "left") : (oy > 0 ? "back" : "front");
            std::string name = ring > 0 ? std::string("inner ") + side : std::string(side);
            if (s.shape != BuildingShape::Rectangle)
                name += " (" + std::to_string(wall + 1) + ")";
            return name;
        }
        i -= n;
    }
    return "?";
}

std::vector<std::array<double, 4>> wings(const BuildingSpec& s)
{
    const double w = s.width / 2, d = s.depth / 2, a = s.wingDepth, b = s.wingWidth;
    switch (s.shape) {
    case BuildingShape::Rectangle: break;
    case BuildingShape::L: return {{-w, -d, w, -d + a}, {-w, -d, -w + b, d}};
    case BuildingShape::T: return {{-w, d - a, w, d}, {-b / 2, -d, b / 2, d}};
    case BuildingShape::U: return {{-w, d - a, w, d}, {-w, -d, -w + b, d}, {w - b, -d, w, d}};
    case BuildingShape::Courtyard: return {{-w, -d, w, -d + a}, {-w, d - a, w, d}, {-w, -d, -w + b, d}, {w - b, -d, w, d}};
    }
    return {{-w, -d, w, d}};
}

BuildingSpec kept(BuildingSpec s)
{
    const double least = 2.5 * thicknessOf(s), open = 500.0;
    const auto keep = [](double v, double lo, double hi) { return lo > hi ? lo : std::clamp(v, lo, hi); };
    switch (s.shape) {
    case BuildingShape::Rectangle: break;
    case BuildingShape::L:
        s.wingDepth = keep(s.wingDepth, least, s.depth - least);
        s.wingWidth = keep(s.wingWidth, least, s.width - least);
        break;
    case BuildingShape::T:
        s.wingDepth = keep(s.wingDepth, least, s.depth - least);
        s.wingWidth = keep(s.wingWidth, least, s.width - 2 * least);
        break;
    case BuildingShape::U:
        s.wingDepth = keep(s.wingDepth, least, s.depth - least);
        s.wingWidth = keep(s.wingWidth, least, (s.width - open) / 2);
        break;
    case BuildingShape::Courtyard:
        s.wingDepth = keep(s.wingDepth, least, (s.depth - open) / 2);
        s.wingWidth = keep(s.wingWidth, least, (s.width - open) / 2);
        break;
    }
    const int walls = wallCount(s);
    std::erase_if(s.openings, [walls](const BuildingOpening& o) { return o.wall < 0 || o.wall >= walls; });
    return s;
}

BuildingSpec withShape(const BuildingSpec& from, BuildingShape shape)
{
    BuildingSpec s = from;
    s.shape = shape;
    const auto r = [](double v) { return std::round(v / 100.0) * 100.0; };
    switch (shape) {
    case BuildingShape::Rectangle: break;
    case BuildingShape::L: s.wingDepth = r(s.depth * 0.45), s.wingWidth = r(s.width * 0.4); break;
    case BuildingShape::T: s.wingDepth = r(s.depth * 0.4), s.wingWidth = r(s.width * 0.35); break;
    case BuildingShape::U: s.wingDepth = r(s.depth * 0.35), s.wingWidth = r(s.width * 0.3); break;
    case BuildingShape::Courtyard: s.wingDepth = r(s.depth * 0.3), s.wingWidth = r(s.width * 0.3); break;
    }
    if (shape != from.shape)
        s.openings.clear();  // the walls are others now
    return kept(s);
}

std::vector<std::array<double, 2>> corners(const BuildingSpec& s)
{
    const double w = s.width / 2, d = s.depth / 2, a = s.wingDepth, b = s.wingWidth;
    std::vector<std::array<double, 2>> c{{-w, -d}, {w, -d}, {w, d}, {-w, d}};
    switch (s.shape) {
    case BuildingShape::Rectangle: break;
    case BuildingShape::L: c.push_back({-w + b, -d + a}); break;
    case BuildingShape::T: c.push_back({-b / 2, d - a}), c.push_back({b / 2, d - a}); break;
    case BuildingShape::U: c.push_back({-w + b, d - a}), c.push_back({w - b, d - a}); break;
    case BuildingShape::Courtyard:
        c.push_back({-w + b, -d + a}), c.push_back({w - b, -d + a}), c.push_back({w - b, d - a}),
            c.push_back({-w + b, d - a});
        break;
    }
    return c;
}

BuildingSpec movedCorner(const BuildingSpec& from, std::size_t i, double x, double y)
{
    BuildingSpec s = from;
    const auto r = [](double v) { return std::round(v / 10.0) * 10.0; };
    const double least = 2.5 * thicknessOf(s);
    if (i < 4) {  // the outside: wider or narrower, deeper or shallower (about the middle)
        s.width = std::clamp(r(2 * std::abs(x)), std::max(500.0, least), 200000.0);
        s.depth = std::clamp(r(2 * std::abs(y)), std::max(500.0, least), 200000.0);
    } else {
        const double w = s.width / 2, d = s.depth / 2;
        switch (s.shape) {
        case BuildingShape::Rectangle: break;
        case BuildingShape::L: s.wingWidth = r(x + w), s.wingDepth = r(y + d); break;
        case BuildingShape::T: s.wingWidth = r(2 * std::abs(x)), s.wingDepth = r(d - y); break;
        case BuildingShape::U: s.wingWidth = r(w - std::abs(x)), s.wingDepth = r(d - y); break;
        case BuildingShape::Courtyard: s.wingWidth = r(w - std::abs(x)), s.wingDepth = r(d - std::abs(y)); break;
        }
    }
    return kept(s);
}

namespace {

double roofTopOf(const BuildingSpec& s)
{
    const double k = 1.0 / s.scale;
    double top = s.height * k + std::max(150.0 * k, 2 * s.smallest);
    if (s.roof == RoofKind::Gable || s.roof == RoofKind::Hip) {
        double span = 0;
        for (const auto& w : wings(s))
            span = std::max(span, std::min(w[2] - w[0], w[3] - w[1]) / 2 + s.overhang);
        top += span * k * std::tan(s.pitch * kPi / 180.0);
    }
    return top;
}

double postSize(const BuildingSpec& s)
{
    return std::max(0.8, 0.012 * std::max(s.width, s.depth) / s.scale);
}

}  // namespace

Mesh cornerMarker(const BuildingSpec& s, std::size_t i)
{
    const auto c = corners(s);
    if (i >= c.size())
        return {};
    const double k = 1.0 / s.scale, r = postSize(s);
    const double x = c[i][0] * k, y = c[i][1] * k;
    return meshOf(box({x - r, y - r, 0}, {x + r, y + r, roofTopOf(s) + 3 * r}));
}

std::optional<std::size_t> pickedCorner(const BuildingSpec& s, const std::array<double, 3>& o,
                                        const std::array<double, 3>& d, double* height)
{
    const auto c = corners(s);
    const double k = 1.0 / s.scale, r = postSize(s) * 2.0, top = roofTopOf(s) + 3 * postSize(s);
    std::optional<std::size_t> best;
    double bestT = 1e300;
    const double dd = d[0] * d[0] + d[1] * d[1];
    for (std::size_t i = 0; i < c.size(); ++i) {
        const double cx = c[i][0] * k, cy = c[i][1] * k;
        // Where the ray passes nearest the post's line, seen from above.
        const double t = dd > 1e-12 ? ((cx - o[0]) * d[0] + (cy - o[1]) * d[1]) / dd : 0.0;
        const double px = o[0] + d[0] * t, py = o[1] + d[1] * t, z = o[2] + d[2] * t;
        double near = std::hypot(px - cx, py - cy), zz = z;
        if (dd <= 1e-12)
            zz = top;
        if (t < 0 || near > r || zz < 0 || zz > top || t >= bestT)
            continue;
        bestT = t;
        best = i;
        if (height)
            *height = std::clamp(zz, 0.0, top);
    }
    return best;
}

const char* title(WallMaterial m)
{
    switch (m) {
    case WallMaterial::Brick: return "Brick";
    case WallMaterial::Stone: return "Stone blocks";
    case WallMaterial::Logs: return "Logs";
    case WallMaterial::Planks: return "Wood planks";
    case WallMaterial::RoughStone: return "Rough-hewn stone";
    case WallMaterial::Fieldstone: return "Fieldstone (irregular)";
    }
    return "";
}

const char* title(RoofKind r)
{
    switch (r) {
    case RoofKind::None: return "None (open)";
    case RoofKind::Flat: return "Flat";
    case RoofKind::Gable: return "Gable (two slopes)";
    case RoofKind::Hip: return "Hip (four slopes)";
    }
    return "";
}

BuildingSpec defaults(WallMaterial m, const BuildingSpec& from)
{
    BuildingSpec s = from;
    s.material = m;
    switch (m) {
    case WallMaterial::Brick:  // a standard brick, 194 x 92 x 57 mm, 10 mm joints
        s.unitLength = 194, s.unitHeight = 57, s.unitDepth = 92, s.joint = 10, s.wallThickness = 230, s.mortar = true;
        break;
    case WallMaterial::Stone:  // cut stone blocks
        s.unitLength = 450, s.unitHeight = 300, s.unitDepth = 300, s.joint = 15, s.wallThickness = 450, s.mortar = true;
        break;
    case WallMaterial::Logs:  // 250 mm logs
        s.unitLength = 0, s.unitHeight = 250, s.unitDepth = 250, s.joint = 0, s.wallThickness = 250, s.mortar = false;
        break;
    case WallMaterial::Planks:  // 150 mm showing, on a 150 mm wall
        s.unitLength = 0, s.unitHeight = 150, s.unitDepth = 25, s.joint = 0, s.wallThickness = 150, s.mortar = false;
        break;
    case WallMaterial::RoughStone:  // roughly squared stones, rock-faced, courses of varying height
        s.unitLength = 550, s.unitHeight = 320, s.unitDepth = 350, s.joint = 30, s.wallThickness = 450, s.mortar = true;
        break;
    case WallMaterial::Fieldstone:  // rounded stones of every shape, wide mortar: a "giraffe" stone house
        s.unitLength = 400, s.unitHeight = 400, s.unitDepth = 300, s.joint = 40, s.wallThickness = 450, s.mortar = true;
        break;
    }
    return s;
}

void check(const BuildingSpec& s)
{
    if (!(s.scale >= 1.0 && s.scale <= 1000.0))
        throw std::invalid_argument("The scale must be from 1:1 to 1:1000.");
    if (!(s.width >= 500 && s.depth >= 500 && s.width <= 200000 && s.depth <= 200000))
        throw std::invalid_argument("The building must be from 0.5 to 200 m wide and deep.");
    if (!(s.height >= 300 && s.height <= 100000))
        throw std::invalid_argument("The walls must be from 0.3 to 100 m high.");
    const double t = thicknessOf(s);
    if (!(t >= 20 && 2.5 * t <= std::min(s.width, s.depth)))
        throw std::invalid_argument("The walls are too thick for the building's size (or thinner than 20 mm).");
    if (!(s.unitHeight >= 10))
        throw std::invalid_argument("The units (bricks, blocks, logs or planks) must be at least 10 mm high.");
    if ((s.material == WallMaterial::Brick || s.material == WallMaterial::Stone ||
         s.material == WallMaterial::RoughStone || s.material == WallMaterial::Fieldstone) &&
        !(s.unitLength >= 20))
        throw std::invalid_argument("The bricks or blocks must be at least 20 mm long.");
    if (!(s.joint >= 0 && s.joint <= 100))
        throw std::invalid_argument("The joint must be from 0 to 100 mm.");
    if (!(s.pitch >= 5 && s.pitch <= 75))
        throw std::invalid_argument("The roof's pitch must be from 5 to 75 degrees.");
    if (!(s.overhang >= 0 && s.overhang <= 3000))
        throw std::invalid_argument("The roof's overhang must be from 0 to 3 m.");
    if (!(s.smallest >= 0.02 && s.smallest <= 2))
        throw std::invalid_argument("The smallest detail must be from 0.02 to 2 mm.");
    if (s.shape != BuildingShape::Rectangle) {
        const BuildingSpec k = kept(s);
        if (std::abs(k.wingDepth - s.wingDepth) > 0.5 || std::abs(k.wingWidth - s.wingWidth) > 0.5)
            throw std::invalid_argument("The wings don't fit: each at least 2.5 times the wall's thickness, and an open "
                                        "middle at least 500 mm across. Make the building bigger, or change the wings.");
    }
    const int walls = wallCount(s);
    for (const auto& o : s.openings)
        if (o.wall < 0 || o.wall >= walls || !(o.width >= 100) || !(o.height >= 100))
            throw std::invalid_argument("A door or window is too small (under 100 mm) or on no wall.");
}

Face face(const BuildingSpec& s, int wall)
{
    const std::vector<Wall> walls = wallsOf(s);
    return walls[std::size_t(std::clamp(wall, 0, int(walls.size()) - 1))].f;
}

std::optional<Spot> spotAt(const BuildingSpec& s, const std::array<double, 3>& origin,
                           const std::array<double, 3>& direction)
{
    std::optional<Spot> best;
    double bestT = 1e300;
    const std::vector<Wall> walls = wallsOf(s);
    for (int w = 0; w < int(walls.size()); ++w) {
        const Face& f = walls[std::size_t(w)].f;
        const double facing = dot(direction, f.out);
        if (facing >= -1e-9)
            continue;  // from inside, or along it
        const V3 rel{f.start[0] - origin[0], f.start[1] - origin[1], f.start[2] - origin[2]};
        const double t = dot(rel, f.out) / facing;
        if (t < 0 || t >= bestT)
            continue;
        const V3 p = add(origin, mul(direction, t));
        const V3 q{p[0] - f.start[0], p[1] - f.start[1], p[2] - f.start[2]};
        const double a = dot(q, f.along), up = p[2];
        if (a < 0 || a > f.length || up < 0 || up > f.height)
            continue;
        bestT = t;
        best = Spot{w, a * s.scale, up * s.scale};
    }
    return best;
}

BuildingOpening keptIn(const BuildingSpec& s, BuildingOpening o)
{
    const double length = face(s, o.wall).length * s.scale;
    const double margin = thicknessOf(s) + 120.0;  // clear of the corner, its frame too
    o.width = std::clamp(o.width, 100.0, std::max(100.0, length - 2 * margin));
    o.along = std::clamp(o.along, margin + o.width / 2, std::max(margin + o.width / 2, length - margin - o.width / 2));
    if (o.kind == OpeningKind::Door) {
        o.bottom = 0;
        o.height = std::clamp(o.height, 100.0, std::max(100.0, s.height - 150.0));
    } else {
        o.height = std::clamp(o.height, 100.0, std::max(100.0, s.height - 300.0));
        o.bottom = std::clamp(o.bottom, 150.0, std::max(150.0, s.height - o.height - 150.0));
    }
    return o;
}

BuildingOpening openingAt(const BuildingSpec& s, OpeningKind kind, const Spot& spot)
{
    BuildingOpening o;
    o.kind = kind;
    o.wall = spot.wall;
    o.along = spot.along;
    if (kind == OpeningKind::Door) {
        o.width = 900, o.height = 2000, o.bottom = 0;
    } else {
        o.width = 900, o.height = 1200;
        o.bottom = spot.up - o.height / 2;
    }
    return keptIn(s, o);
}

Mesh openingMarker(const BuildingSpec& s, const BuildingOpening& raw)
{
    const BuildingOpening o = keptIn(s, raw);
    const double k = 1.0 / s.scale;
    const Face f = face(s, o.wall);
    const double c = o.along * k, w = o.width * k, b = o.bottom * k, h = o.height * k, m = 70.0 * k;
    const double out = std::max(60.0 * k, 0.6) + (s.material == WallMaterial::Planks ? 25.0 * k : 0.0);
    return meshOf(wallBox(f, c - w / 2 - m, c + w / 2 + m, std::max(0.0, b - m), b + h + m, out, out + 0.3));
}

bool clashes(const BuildingSpec& s, const BuildingOpening& raw, std::optional<std::size_t> skip)
{
    const BuildingOpening o = keptIn(s, raw);
    const double frame = 2 * 70.0 + 20.0;  // both frames, a little apart
    for (std::size_t i = 0; i < s.openings.size(); ++i) {
        if (skip && *skip == i)
            continue;
        const BuildingOpening p = keptIn(s, s.openings[i]);
        if (p.wall != o.wall)
            continue;
        const bool across = std::abs(p.along - o.along) < (p.width + o.width) / 2 + frame;
        const bool up = o.bottom < p.bottom + p.height + frame && p.bottom < o.bottom + o.height + frame;
        if (across && up)
            return true;
    }
    return false;
}

std::optional<std::size_t> pickedOpening(const BuildingSpec& s, const Spot& spot)
{
    for (std::size_t i = 0; i < s.openings.size(); ++i) {
        const BuildingOpening o = keptIn(s, s.openings[i]);
        if (o.wall == spot.wall && std::abs(spot.along - o.along) <= o.width / 2 + 70 && spot.up >= o.bottom - 70 &&
            spot.up <= o.bottom + o.height + 70)
            return i;
    }
    return std::nullopt;
}

Made make(const BuildingSpec& s, bool quick)
{
    check(s);
    Made made;
    const double k = 1.0 / s.scale;
    const double W = s.width * k, D = s.depth * k, H = s.height * k;
    const double sm = s.smallest;
    const double T = thicknessOf(s) * k;
    const double ul = s.unitLength * k, uh = s.unitHeight * k;
    std::vector<MeshSolid> parts;     // merged into the walls
    std::vector<MeshSolid> units;     // bricks, blocks: apart from each other, merged as one
    double top = H;                   // the walls' top
    double faceOut = 0;               // how far the face's detail stands out (planks)

    const std::vector<Ring> rings = modelRings(s);
    const std::vector<Wall> wallList = wallsOf(s);
    // The walls' plain shell: the footprint less the inside, each face set in by `inset`.
    const auto shells = [&](double inset) {
        std::vector<Ring> outer, inner;
        for (const Ring& r : rings) {
            outer.push_back(offsetRing(r, inset));
            inner.push_back(offsetRing(r, T - inset));
        }
        return std::vector<MeshSolid>{meshkernel::subtracted(regionOf(outer, 0, top), {regionOf(inner, -1.0, top + 1.0)})};
    };

    bool pattern = !quick;
    switch (s.material) {
    case WallMaterial::Brick:
    case WallMaterial::Stone: {
        double jw = 0, recess = 0;
        if (s.mortar) {
            jw = std::max(s.joint * k, sm);
            if (s.joint * k < sm)
                made.notes.push_back("Mortar joints widened to " + mm(jw) + " so they print.");
            recess = std::clamp(std::max(0.5 * jw, 0.5 * sm), 0.0, 0.2 * T);
        } else {
            jw = 0.5 * sm;
            recess = 0.5 * sm;
            made.notes.push_back("Laid without mortar: the joints are shown as fine lines.");
        }
        if (uh < 2.5 * sm || ul < 2.5 * sm) {
            pattern = false;
            char buf[160];
            std::snprintf(buf, sizeof buf, "The %s are too small to print at 1:%g (%.2f mm high): plain walls.",
                          s.material == WallMaterial::Brick ? "bricks" : "blocks", s.scale, uh);
            made.notes.push_back(buf);
        }
        if (!pattern) {
            for (auto& p : shells(0))
                parts.push_back(std::move(p));
            break;
        }
        for (auto& p : shells(recess))
            parts.push_back(std::move(p));
        std::mt19937 rng(s.seed);
        std::uniform_real_distribution<double> vary(0.6, 1.4);
        for (int course = 0;; ++course) {
            const double z0 = course * (uh + jw);
            if (z0 >= H - sm)
                break;
            const double z1 = std::min(z0 + uh, H);
            for (const Wall& wl : wallList) {
                const Face& f = wl.f;
                // Outside corners bonded: walls along X run to the corners on even courses,
                // those along Y on odd ones; the other stops a joint short of them. At an
                // inside corner each runs to the corner.
                const bool full = wl.alongX == (course % 2 == 0);
                const double a0 = wl.convexStart && !full ? T + jw : 0.0;
                const double a1 = wl.convexEnd && !full ? f.length - T - jw : f.length;
                double a = a0 - (course % 2) * (ul + jw) / 2;
                while (a < a1) {
                    const double len = s.material == WallMaterial::Stone ? ul * vary(rng) : ul;
                    const double b0 = std::max(a, a0), b1 = std::min(a + len, a1);
                    if (b1 - b0 >= sm)
                        units.push_back(wallBox(f, b0, b1, z0, z1, -T, 0));
                    a += len + jw;
                }
            }
        }
        break;
    }
    case WallMaterial::RoughStone:
    case WallMaterial::Fieldstone: {
        // Stones standing out of the mortar, each domed and rough: the wall's face as a
        // height field, a skin over the plain shell.
        const bool field = s.material == WallMaterial::Fieldstone;
        const double size = field ? ul : std::min(ul, uh);
        const double step = std::max(0.75 * sm, size / 12.0);
        double jw = s.mortar ? std::max(s.joint * k, sm) : 0.6 * sm;
        if (s.mortar && jw < 1.6 * step) {
            jw = 1.6 * step;
            made.notes.push_back("Mortar joints widened to " + mm(jw) + " so they print.");
        }
        const double relief = std::clamp(0.14 * size, sm, 0.25 * T);
        const double recess = s.mortar ? std::clamp(0.4 * relief, 0.5 * sm, 0.15 * T) : 0.0;
        if (size < 4 * sm) {
            pattern = false;
            char buf[160];
            std::snprintf(buf, sizeof buf, "The stones are too small to print at 1:%g (%.2f mm): plain walls.", s.scale,
                          size);
            made.notes.push_back(buf);
        }
        if (!pattern) {
            for (auto& p : shells(0))
                parts.push_back(std::move(p));
            break;
        }
        if (!s.mortar)
            made.notes.push_back("Laid dry (no mortar): the stones touch, the joints shown as fine lines.");
        for (auto& p : shells(recess))
            parts.push_back(std::move(p));
        int wallNo = 0;
        for (const Wall& wl : wallList) {
            const Face& f = wl.f;
            const std::uint32_t seed = s.seed * 7919u + std::uint32_t(wallNo++) * 104729u;
            // How far a point is inside its stone (model mm; below 0: in the mortar).
            std::function<double(double, double)> inside;
            if (field) {
                // Irregular stones: the cells round points scattered a stone apart.
                inside = [=](double u, double v) {
                    const double cu = u / size, cv = v / size;
                    const int i0 = int(std::floor(cu)), j0 = int(std::floor(cv));
                    double f1 = 1e9, f2 = 1e9;
                    for (int j = j0 - 1; j <= j0 + 1; ++j)
                        for (int i = i0 - 1; i <= i0 + 1; ++i) {
                            const double px = i + 0.5 + 0.42 * noise2(i * 1.7, j * 1.3, seed),
                                         py = j + 0.5 + 0.42 * noise2(i * 1.1 + 9.0, j * 1.9, seed + 1);
                            const double d = std::hypot(cu - px, (cv - py) * 1.15);
                            if (d < f1)
                                f2 = f1, f1 = d;
                            else if (d < f2)
                                f2 = d;
                        }
                    return (f2 - f1) / 2 * size - jw / 2;
                };
            } else {
                // Courses of varying height; stones of varying length in each.
                std::vector<double> courses{0.0};
                std::mt19937 rng(seed);
                std::uniform_real_distribution<double> vary(0.0, 1.0);
                while (courses.back() < f.height)
                    courses.push_back(courses.back() + uh * (0.7 + 0.6 * vary(rng)));
                std::vector<std::vector<double>> joints(courses.size());
                for (std::size_t c = 0; c + 1 < courses.size(); ++c) {
                    double a = -T - ul * vary(rng);
                    joints[c].push_back(a);
                    while (a < f.length + T + ul) {
                        a += ul * (0.55 + 0.9 * vary(rng));
                        joints[c].push_back(a);
                    }
                }
                inside = [courses, joints, jw](double u, double v) {
                    const auto c = std::size_t(std::max<std::ptrdiff_t>(
                        0, std::upper_bound(courses.begin(), courses.end(), v) - courses.begin() - 1));
                    if (c + 1 >= courses.size())
                        return -1.0;
                    const auto& js = joints[c];
                    const auto b = std::upper_bound(js.begin(), js.end(), u);
                    if (b == js.begin() || b == js.end())
                        return -1.0;
                    const double u0 = *(b - 1), u1 = *b;
                    return std::min({u - u0, u1 - u, v - courses[c], courses[c + 1] - v}) - jw / 2;
                };
            }
            const double ramp = 0.3 * size;
            const auto out = [=](double u, double v) {
                const double d = inside(u, v);
                if (d <= 0)
                    return -recess;  // the mortar
                const double dome = std::sqrt(std::clamp(d / ramp, 0.0, 1.0));  // rounded at its edges
                const double rough = 0.5 * noise2(u / (0.18 * size), v / (0.18 * size), seed + 7) +
                                     0.25 * noise2(u / (0.07 * size), v / (0.07 * size), seed + 8);
                return -recess + (recess + relief) * dome * (0.8 + (field ? 0.25 : 0.45) * rough);
            };
            parts.push_back(faceSkin(f, wl.convexStart ? relief : 0.0, wl.convexEnd ? relief : 0.0, H,
                                     -std::min(0.5 * T, 2 * relief + recess), step, out));
        }
        break;
    }
    case WallMaterial::Logs: {
        const double d = T;
        if (d < 3 * sm) {
            pattern = false;
            made.notes.push_back("The logs are too small to print at this scale: plain walls.");
        }
        if (!pattern) {
            for (auto& p : shells(0))
                parts.push_back(std::move(p));
            break;
        }
        const double pitch = 0.85 * d, ext = 0.5 * d;
        const int sides = std::clamp(int(d / std::max(0.2, sm) * 2), 12, 32);
        std::vector<std::array<double, 2>> circle;
        for (int i = 0; i < sides; ++i) {
            const double t = 2 * kPi * i / sides;
            circle.push_back({d / 2 * std::cos(t), d / 2 * std::sin(t)});
        }
        double logTop = 0;
        for (int course = 0;; ++course) {
            bool any = false;
            // Front and back logs, then the sides' half a log higher.
            const double zf = d / 2 + course * pitch, zs = d / 2 + (course + 0.5) * pitch;
            // Walls along X take the lower logs, those along Y half a log higher; the logs
            // cross past outside corners, and meet the other wall's logs at inside ones.
            for (const Wall& wl : wallList) {
                const double z = wl.alongX ? zf : zs;
                if (z + d / 2 > H + 0.1 * d)
                    continue;
                any = true;
                const Face& f = wl.f;
                const double e0 = wl.convexStart ? ext : d / 2, e1 = wl.convexEnd ? ext : d / 2;
                const V3 along = f.along, out = f.out;
                const V3 a = add(add(f.start, mul(along, -e0)), add(mul(out, -d / 2), {0, 0, z}));
                parts.push_back(prism(circle, a, mul(along, f.length + e0 + e1), cross({0, 0, 1}, along), {0, 0, 1}));
                logTop = std::max(logTop, z + d / 2);
            }
            if (!any)
                break;
        }
        // The chinking between them, and up to the top log's middle.
        top = std::max(logTop - d / 2, d);
        for (auto& p : shells(0.2 * d))
            parts.push_back(std::move(p));
        top = logTop;
        break;
    }
    case WallMaterial::Planks: {
        for (auto& p : shells(0))
            parts.push_back(std::move(p));
        const double e = uh;
        // Each plank's lower edge stands out its full thickness; it tapers to almost
        // nothing at its top, under the next one's edge: the shadow line between them.
        const double thick = std::max(25.0 * k, sm), thin = std::min(0.15 * thick, 0.1);
        if (e < 2.5 * sm) {
            made.notes.push_back("The planks are too narrow to print at this scale: plain walls.");
            break;
        }
        if (!pattern)
            break;
        faceOut = thick;
        for (int course = 0;; ++course) {
            const double z0 = course * e;
            if (z0 >= H - sm)
                break;
            const double z1 = std::min(z0 + e, H);
            for (const Wall& wl : wallList) {
                const Face& f = wl.f;
                const V3 along{f.along[0], f.along[1], f.along[2]}, out{f.out[0], f.out[1], f.out[2]};
                // Its edge on: thick at the bottom, thin at the top; run along the wall,
                // past outside corners to meet the next wall's.
                const double e0 = wl.convexStart ? thick : 0.0, e1 = wl.convexEnd ? thick : 0.0;
                const V3 axisWay = cross(out, V3{0, 0, 1});  // the run's way (out x up)
                const double sign = dot(axisWay, along) > 0 ? 1.0 : -1.0;
                const V3 a = add(V3{f.start[0], f.start[1], 0}, mul(along, sign > 0 ? -e0 : f.length + e1));
                parts.push_back(prism({{0, z0}, {thick, z0}, {thin, z1}, {0, z1}}, a,
                                      mul(along, sign * (f.length + e0 + e1)), out, {0, 0, 1}));
            }
        }
        // Corner boards over the planks' ends, at the outside corners: on the face of the
        // wall that ends there, in from the corner and round it; and on the next one's.
        const double cb = std::max(100.0 * k, 2 * sm), ct = thick + std::max(5.0 * k, 0.5 * sm);
        for (std::size_t i = 0, base = 0; i < rings.size(); base += rings[i].size(), ++i) {
            const std::size_t n = rings[i].size();
            for (std::size_t j = 0; j < n; ++j) {
                const Wall& ends = wallList[base + j];
                const Wall& next = wallList[base + (j + 1) % n];
                if (!ends.convexEnd)
                    continue;
                parts.push_back(wallBox(ends.f, ends.f.length - cb, ends.f.length + ct, 0, H, 0, ct));
                parts.push_back(wallBox(next.f, -ct, cb, 0, H, 0, ct));
            }
        }
        break;
    }
    }
    if (!units.empty())
        parts.push_back(meshkernel::together(units));
    MeshSolid walls = meshkernel::merged(parts);

    // Doors and windows: cut through, then their frames (and a window's sill and bars, a
    // door's leaf) on the outside.
    std::vector<MeshSolid> holes, fittings;
    const double fw = std::max(70.0 * k, 1.5 * sm), fp = std::max(30.0 * k, sm) + faceOut;
    const double inside = s.material == WallMaterial::Logs ? T : T;
    const double fi = 0.5 * inside;
    for (const BuildingOpening& raw : s.openings) {
        const BuildingOpening o = keptIn(s, raw);
        const Face f = face(s, o.wall);
        const double c = o.along * k, w = o.width * k, b = o.bottom * k, h = o.height * k;
        const double a0 = c - w / 2, a1 = c + w / 2;
        holes.push_back(wallBox(f, a0, a1, b, b + h, -inside - 1.0, fp + 1.0));
        const double fb = o.kind == OpeningKind::Door ? b : b - fw;
        fittings.push_back(wallBox(f, a0 - fw, a0, fb, b + h + fw, -fi, fp));
        fittings.push_back(wallBox(f, a1, a1 + fw, fb, b + h + fw, -fi, fp));
        fittings.push_back(wallBox(f, a0 - fw, a1 + fw, b + h, b + h + fw, -fi, fp));
        if (o.kind == OpeningKind::Window) {
            const double sillH = std::max(50.0 * k, sm), sillOut = fp + std::max(50.0 * k, sm);
            const double ext = std::max(40.0 * k, sm);
            fittings.push_back(wallBox(f, a0 - fw - ext, a1 + fw + ext, b - fw - sillH, b - fw + 0.01, -fi, sillOut));
            fittings.push_back(wallBox(f, a0 - fw, a1 + fw, b - fw, b, -fi, fp));
            const double bar = 30.0 * k;
            if (bar >= sm) {
                const double g0 = -std::min(0.5 * inside, 80.0 * k) - bar, g1 = g0 + bar;
                fittings.push_back(wallBox(f, c - bar / 2, c + bar / 2, b, b + h, g0, g1));
                fittings.push_back(wallBox(f, a0, a1, b + h / 2 - bar / 2, b + h / 2 + bar / 2, g0, g1));
            }
        } else {
            const double inset = std::min(50.0 * k, 0.3 * inside), leaf = std::min(std::max(40.0 * k, 2 * sm), inside - inset);
            fittings.push_back(wallBox(f, a0, a1, b, b + h, -inset - leaf, -inset));
        }
    }
    if (!holes.empty())
        walls = meshkernel::subtracted(walls, {meshkernel::together(holes)});
    if (!fittings.empty()) {
        std::vector<MeshSolid> all{walls};
        for (auto& f : fittings)
            all.push_back(std::move(f));
        walls = meshkernel::merged(all);
    }

    // The roof, on the walls' top.
    if (s.roof != RoofKind::None) {
        const double o = s.overhang * k, e = std::max(150.0 * k, 2 * sm);
        const double t = std::tan(s.pitch * kPi / 180.0);
        // A roof over each wing, joined: where wings meet, their roofs cross.
        std::vector<MeshSolid> roofs;
        for (const auto& wg : wings(s)) {
            const double cx = (wg[0] + wg[2]) / 2 * k, cy = (wg[1] + wg[3]) / 2 * k;
            const double X = (wg[2] - wg[0]) / 2 * k + o, Y = (wg[3] - wg[1]) / 2 * k + o;
            if (s.roof == RoofKind::Flat) {
                roofs.push_back(box({cx - X, cy - Y, top}, {cx + X, cy + Y, top + e}));
                continue;
            }
            // A gable along X (its slopes to the front and back), one along Y; a hip roof
            // is where both are.
            const auto gableX = [&] {
                return prism({{cy - Y, top}, {cy + Y, top}, {cy + Y, top + e}, {cy, top + e + Y * t}, {cy - Y, top + e}},
                             {cx - X, 0, 0}, {2 * X, 0, 0}, {0, 1, 0}, {0, 0, 1});
            };
            const auto gableY = [&] {
                return prism({{-cx - X, top}, {-cx + X, top}, {-cx + X, top + e}, {-cx, top + e + X * t}, {-cx - X, top + e}},
                             {0, cy - Y, 0}, {0, 2 * Y, 0}, {-1, 0, 0}, {0, 0, 1});
            };
            if (s.roof == RoofKind::Gable)
                roofs.push_back(X >= Y ? gableX() : gableY());  // the ridge along the longer side
            else
                roofs.push_back(meshkernel::intersected({gableX(), gableY()}));
        }
        MeshSolid roof = roofs.size() == 1 ? roofs[0] : meshkernel::merged(roofs);
        if (s.roofSeparate)
            made.roof = meshOf(roof);
        else
            walls = meshkernel::merged({walls, roof});
    }
    made.walls = meshOf(walls);
    return made;
}

std::vector<Body> bodies(const BuildingSpec& spec, const Made& made, const std::string& name, double x, double y)
{
    std::vector<Body> out;
    BuildingSpec s = spec;
    Body walls = hinge::pieceBody(made.walls, name);
    s.offset = {walls.placement.x, walls.placement.y, walls.placement.z};
    walls.building = s;
    // The material's own colour (bricks red, stone grey, logs and planks wood).
    static const std::uint32_t colours[] = {0xB5563A, 0x9C978F, 0x8B5A2B, 0xC9B48C, 0x8F8579, 0x86735F};
    walls.colour = colours[int(s.material)];
    walls.placement.x += x;
    walls.placement.y += y;
    out.push_back(std::move(walls));
    if (!made.roof.indices.empty()) {
        Body roof = hinge::pieceBody(made.roof, name + " roof");
        roof.colour = 0x6E5A52;
        roof.placement.x += x;
        roof.placement.y += y;
        out.push_back(std::move(roof));
    }
    return out;
}

ObjectId add(CommandStack& stack, Document& doc, BuildingSpec s, const std::string& name, double x, double y)
{
    const Made made = make(s);
    std::vector<Body> list = bodies(s, made, doc.uniqueName(name), x, y);
    stack.beginMacro("Add Building");
    ObjectId walls = kInvalidId;
    try {
        for (std::size_t i = 0; i < list.size(); ++i) {
            list[i].id = doc.allocateId();
            if (i == 0)
                walls = list[i].id;
            else
                list[i].roofOf = walls;
            stack.execute(std::make_unique<AddBodyCommand>(std::move(list[i])));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return walls;
}

void update(CommandStack& stack, Document& doc, ObjectId wallsId, BuildingSpec s)
{
    const Body* old = doc.find(wallsId);
    if (!old || !old->building)
        throw std::invalid_argument("That shape isn't a building.");
    // Where the building's own middle is on the plate (kept).
    const double bx = old->placement.x - old->building->offset[0];
    const double by = old->placement.y - old->building->offset[1];
    const Made made = make(s);
    std::vector<Body> list = bodies(s, made, old->name, bx, by);
    const Body before = *old;
    ObjectId roofId = kInvalidId;
    for (ObjectId id : doc.topLevelIds())
        if (const Body* b = doc.find(id); b && b->roofOf == wallsId)
            roofId = id;
    stack.beginMacro("Change Building");
    try {
        Body after = list[0];
        after.id = wallsId;
        if (before.building && before.building->material == s.material)
            after.colour = before.colour;  // its own colour, unless it's built of something else now
        after.placement.z = before.placement.z - before.building->offset[2] + after.building->offset[2];
        after.placement.rotationX = before.placement.rotationX;
        after.placement.rotationY = before.placement.rotationY;
        after.placement.rotationZ = before.placement.rotationZ;
        stack.execute(std::make_unique<EditBodyCommand>(before, after, "Change Building"));
        if (list.size() > 1) {
            Body roof = list[1];
            roof.roofOf = wallsId;
            roof.placement.z += before.placement.z - before.building->offset[2];
            if (roofId != kInvalidId) {
                const Body oldRoof = *doc.find(roofId);
                roof.id = roofId;
                roof.name = oldRoof.name;
                roof.colour = oldRoof.colour;
                stack.execute(std::make_unique<EditBodyCommand>(oldRoof, roof, "Change Building"));
            } else {
                roof.id = doc.allocateId();
                stack.execute(std::make_unique<AddBodyCommand>(std::move(roof)));
            }
        } else if (roofId != kInvalidId) {
            stack.execute(std::make_unique<RemoveBodyCommand>(roofId));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
}

}  // namespace forge::building
