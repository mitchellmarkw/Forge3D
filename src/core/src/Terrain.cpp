// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Terrain.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"
#include "RockField.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

namespace forge::terrain {

namespace {

using namespace rockfield;

double smoothstep(double a, double b, double x)
{
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

// Model mm: the outline's distance (negative inside), the base's half sizes a and b.
double outline(const TerrainSpec& s, double x, double y)
{
    const double k = 1.0 / s.scale;
    const double a = s.width * k / 2, b = s.depth * k / 2;
    if (s.base == TerrainBase::Round)
        return (std::sqrt((x / a) * (x / a) + (y / b) * (y / b)) - 1.0) * std::min(a, b);
    return std::max(std::abs(x) - a, std::abs(y) - b);
}

// The point of a segment nearest p, and how far along it (0..1).
double nearestOnSegment(double px, double py, double ax, double ay, double bx, double by, double& cx, double& cy)
{
    const double dx = bx - ax, dy = by - ay;
    const double l2 = dx * dx + dy * dy;
    const double t = l2 > 1e-12 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / l2, 0.0, 1.0) : 0.0;
    cx = ax + t * dx;
    cy = ay + t * dy;
    return t;
}

// The ground before paths (model mm, above the underside), at model (x, y).
double groundNoPaths(const TerrainSpec& s, const Noise& noise, double x, double y)
{
    const double k = 1.0 / s.scale;
    const double hh = s.hillHeight * k, size = std::max(s.hillSize * k, 1e-3);
    double h = 0.0;
    if (hh > 0) {
        // fbm's spread is about +-0.45: stretched to 0..1, then lowered (wide valleys, rarer tops).
        const double t = std::clamp((noise.fbm(x / size, y / size, 0.5, 4) / 0.45 + 1.0) / 2.0, 0.0, 1.0);
        h = hh * std::pow(t, 1.6);
    }
    h += s.roughness * (0.07 * hh + 60.0 * k) * noise.fbm(x * 9.0 / size + 40.0, y * 9.0 / size, 2.5, 3);
    if (s.levelEdges) {
        const double margin = std::min(0.25 * std::min(s.width, s.depth) * k, 0.5 * size);
        h *= smoothstep(0.0, std::max(margin, 1e-3), -outline(s, x, y));
    }
    for (const TerrainFeature& f : s.features) {
        if (f.kind != TerrainFeatureKind::Crater)
            continue;
        const double R = std::max(f.size * k / 2, 1e-3), D = f.depth * k;
        const double r = std::hypot(x - f.x * k, y - f.y * k);
        if (r > 2.2 * R)
            continue;
        if (r < R)
            h -= D * (1.0 - (r / R) * (r / R));
        h += 0.22 * D * std::exp(-((r - R) / (0.32 * R)) * ((r - R) / (0.32 * R)));
    }
    return h;
}

double ground(const TerrainSpec& s, const Noise& noise, double x, double y)
{
    const double k = 1.0 / s.scale;
    double h = groundNoPaths(s, noise, x, y);
    for (const TerrainFeature& f : s.features) {
        if (f.kind != TerrainFeatureKind::Path)
            continue;
        const double w = std::max(f.size * k, 1e-3);
        double cx, cy;
        nearestOnSegment(x, y, f.x * k, f.y * k, f.x2 * k, f.y2 * k, cx, cy);
        const double d = std::hypot(x - cx, y - cy);
        if (d > 1.3 * w)
            continue;
        const double level = groundNoPaths(s, noise, cx, cy) - f.depth * k;
        const double t = 1.0 - smoothstep(w / 2, w / 2 + 0.8 * w, d);  // its banks sloping off
        h += (level - h) * t;
    }
    return std::max(s.baseThickness + h, std::max(0.4 * s.baseThickness, 2 * s.smallest));
}

double tallest(const TerrainSpec& s)
{
    const double k = 1.0 / s.scale;
    double top = s.baseThickness + s.hillHeight * k * 1.05 + s.roughness * (0.07 * s.hillHeight * k + 60 * k) + 1.0;
    double extra = 0;
    for (const TerrainFeature& f : s.features)
        extra = std::max(extra, f.kind == TerrainFeatureKind::Crag ? f.depth * k * 1.3
                                : f.kind == TerrainFeatureKind::Crater ? 0.25 * f.depth * k
                                                                         : 0.0);
    return top + extra;
}

// A closed solid between two convex outlines (counter-clockwise, as many points each): the
// lower at z0, the upper at z1.
Mesh loft(const std::vector<std::array<double, 2>>& lower, const std::vector<std::array<double, 2>>& upper, double z0,
          double z1)
{
    Mesh m;
    const std::size_t n = lower.size();
    for (const auto& [ring, z] : {std::pair{&lower, z0}, std::pair{&upper, z1}})
        for (const auto& p : *ring) {
            m.positions.push_back(float(p[0]));
            m.positions.push_back(float(p[1]));
            m.positions.push_back(float(z));
        }
    const auto tri = [&](std::size_t a, std::size_t b, std::size_t c) {
        m.indices.push_back(std::uint32_t(a));
        m.indices.push_back(std::uint32_t(b));
        m.indices.push_back(std::uint32_t(c));
    };
    for (std::size_t i = 1; i + 1 < n; ++i) {
        tri(0, i + 1, i);          // bottom, facing down
        tri(n, n + i, n + i + 1);  // top, facing up
    }
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t j = (i + 1) % n;
        tri(i, j, n + j);
        tri(i, n + j, n + i);
    }
    return m;
}

Mesh prism(const std::vector<std::array<double, 2>>& outlineCCW, double z0, double z1)
{
    return loft(outlineCCW, outlineCCW, z0, z1);
}

// The base's outline (model mm), `inset` in from its edge.
std::vector<std::array<double, 2>> outlinePoints(const TerrainSpec& s, double inset)
{
    const double k = 1.0 / s.scale;
    const double a = s.width * k / 2 - inset, b = s.depth * k / 2 - inset;
    if (s.base == TerrainBase::Rectangle)
        return {{-a, -b}, {a, -b}, {a, b}, {-a, b}};
    std::vector<std::array<double, 2>> ring;
    const int n = 160;
    for (int i = 0; i < n; ++i)
        ring.push_back({a * std::cos(i * 2 * kPi / n), b * std::sin(i * 2 * kPi / n)});
    return ring;
}

}  // namespace

TerrainSpec defaults()
{
    return TerrainSpec{};
}

const char* title(TerrainFeatureKind k)
{
    switch (k) {
    case TerrainFeatureKind::Crater: return "Crater";
    case TerrainFeatureKind::Crag: return "Crag";
    case TerrainFeatureKind::Path: return "Path";
    }
    return "";
}

TerrainFeature feature(const TerrainSpec& s, TerrainFeatureKind k, double x, double y, double x2, double y2)
{
    TerrainFeature f;
    f.kind = k;
    f.x = x;
    f.y = y;
    f.x2 = x2;
    f.y2 = y2;
    const double most = 0.35 * std::min(s.width, s.depth);
    switch (k) {
    case TerrainFeatureKind::Crater: f.size = std::min(2400.0, most), f.depth = std::min(600.0, f.size * 0.3); break;
    case TerrainFeatureKind::Crag: f.size = std::min(1800.0, most), f.depth = 0.45 * f.size; break;
    case TerrainFeatureKind::Path: f.size = std::min(1500.0, most), f.depth = 80.0; break;
    }
    return f;
}

void check(const TerrainSpec& s)
{
    if (!(s.scale >= 1.0 && s.scale <= 1000.0))
        throw std::invalid_argument("The scale must be from 1:1 to 1:1000.");
    if (!(s.width >= 100 && s.depth >= 100 && s.width <= 1000000 && s.depth <= 1000000))
        throw std::invalid_argument("The terrain must be from 100 mm to 1 km each way.");
    const double k = 1.0 / s.scale;
    if (s.width * k > 1000 || s.depth * k > 1000)
        throw std::invalid_argument("The terrain is over a metre across at this scale: make it smaller.");
    if (std::min(s.width, s.depth) * k < 10)
        throw std::invalid_argument("The terrain is under 10 mm across at this scale: make it bigger.");
    if (!(s.baseThickness >= 0.5 && s.baseThickness <= 50))
        throw std::invalid_argument("The base must be from 0.5 to 50 mm thick.");
    if (s.edge == TerrainEdge::Bevelled &&
        !(s.bevel >= 0 && s.bevel < std::min(s.width, s.depth) * k / 4))
        throw std::invalid_argument("The bevel is too wide for the terrain.");
    if (!(s.hillHeight >= 0 && s.hillHeight <= 1000000 && s.hillSize >= 100))
        throw std::invalid_argument("Hills must be at least 100 mm across, and not below the ground.");
    if (!(s.roughness >= 0 && s.roughness <= 1))
        throw std::invalid_argument("Roughness must be from 0 to 100%.");
    if (!(s.smallest >= 0.02 && s.smallest <= 2))
        throw std::invalid_argument("The smallest detail must be from 0.02 to 2 mm.");
    if (s.features.size() > 200)
        throw std::invalid_argument("A terrain piece can have at most 200 craters, crags and paths.");
    for (const TerrainFeature& f : s.features)
        if (!(f.size > 0 && f.depth >= 0 && f.size * k >= 2 * s.smallest))
            throw std::invalid_argument(std::string("A ") + title(f.kind) +
                                        " is too small to print at this scale: make it bigger.");
}

bool inside(const TerrainSpec& s, double x, double y)
{
    return outline(s, x / s.scale, y / s.scale) <= 0;
}

double heightAt(const TerrainSpec& s, double x, double y)
{
    const Noise noise{s.seed * 2654435761u + 7u};
    return ground(s, noise, x / s.scale, y / s.scale);
}

std::optional<std::array<double, 2>> hitAt(const TerrainSpec& s, const std::array<double, 3>& o,
                                           const std::array<double, 3>& d)
{
    const Noise noise{s.seed * 2654435761u + 7u};
    const double k = 1.0 / s.scale;
    const double a = s.width * k / 2, b = s.depth * k / 2, top = tallest(s);
    // The ray's stretch inside the terrain's box.
    double t0 = 0, t1 = 1e9;
    const double lo[3] = {-a, -b, 0}, hi[3] = {a, b, top};
    for (int i = 0; i < 3; ++i) {
        if (std::abs(d[i]) < 1e-12) {
            if (o[i] < lo[i] || o[i] > hi[i])
                return std::nullopt;
            continue;
        }
        double ta = (lo[i] - o[i]) / d[i], tb = (hi[i] - o[i]) / d[i];
        if (ta > tb)
            std::swap(ta, tb);
        t0 = std::max(t0, ta);
        t1 = std::min(t1, tb);
    }
    if (t0 > t1)
        return std::nullopt;
    const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    const double step = std::max(std::min(a, b) / 300.0, 0.05) / len;
    const auto below = [&](double t) {
        const double x = o[0] + d[0] * t, y = o[1] + d[1] * t, z = o[2] + d[2] * t;
        return outline(s, x, y) <= 0 && z <= ground(s, noise, x, y);
    };
    double prev = t0;
    for (double t = t0; t <= t1 + step; t += step) {
        if (below(t)) {
            double lo2 = prev, hi2 = t;
            for (int i = 0; i < 30; ++i) {
                const double m = (lo2 + hi2) / 2;
                (below(m) ? hi2 : lo2) = m;
            }
            return std::array<double, 2>{(o[0] + d[0] * hi2) / k, (o[1] + d[1] * hi2) / k};
        }
        prev = t;
    }
    return std::nullopt;
}

std::optional<std::size_t> pickedFeature(const TerrainSpec& s, double x, double y)
{
    std::optional<std::size_t> best;
    double bestScore = 1e300;
    for (std::size_t i = 0; i < s.features.size(); ++i) {
        const TerrainFeature& f = s.features[i];
        double d;
        if (f.kind == TerrainFeatureKind::Path) {
            double cx, cy;
            nearestOnSegment(x, y, f.x, f.y, f.x2, f.y2, cx, cy);
            d = std::hypot(x - cx, y - cy);
        } else {
            d = std::hypot(x - f.x, y - f.y);
        }
        const double score = d / (f.size / 2);
        if (score <= 1.0 && score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

void scatter(TerrainSpec& s, int craters, int crags, int paths, std::uint32_t seed)
{
    std::erase_if(s.features, [](const TerrainFeature& f) { return f.scattered; });
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    const double a = s.width / 2, b = s.depth / 2;
    const auto spot = [&](double margin) {
        for (int tries = 0; tries < 200; ++tries) {
            const double x = (uni(rng) * 2 - 1) * a, y = (uni(rng) * 2 - 1) * b;
            const double inX = a - std::abs(x), inY = b - std::abs(y);
            const bool ok = s.base == TerrainBase::Round
                                ? (x / a) * (x / a) + (y / b) * (y / b) <= std::pow(1.0 - margin / std::min(a, b), 2)
                                : std::min(inX, inY) >= margin;
            if (ok)
                return std::array<double, 2>{x, y};
        }
        return std::array<double, 2>{0.0, 0.0};
    };
    const auto add = [&](TerrainFeatureKind k) {
        TerrainFeature f = feature(s, k, 0, 0);
        const double vary = 0.6 + 0.8 * uni(rng);
        f.size *= vary;
        f.depth *= 0.7 + 0.6 * uni(rng);
        if (k == TerrainFeatureKind::Path) {
            // Across the piece, off its edges.
            const double t = uni(rng) * 2 * kPi, turn = kPi + (uni(rng) * 2 - 1) * 0.7;
            f.size = feature(s, k, 0, 0).size * (0.8 + 0.4 * uni(rng));
            f.x = std::cos(t) * a * 1.1, f.y = std::sin(t) * b * 1.1;
            f.x2 = std::cos(t + turn) * a * 1.1, f.y2 = std::sin(t + turn) * b * 1.1;
        } else {
            const auto p = spot(f.size / 2);
            f.x = p[0], f.y = p[1];
        }
        f.scattered = true;
        s.features.push_back(f);
    };
    for (int i = 0; i < craters; ++i)
        add(TerrainFeatureKind::Crater);
    for (int i = 0; i < crags; ++i)
        add(TerrainFeatureKind::Crag);
    for (int i = 0; i < paths; ++i)
        add(TerrainFeatureKind::Path);
}

Mesh make(const TerrainSpec& s, bool quick)
{
    check(s);
    const double k = 1.0 / s.scale;
    const Noise noise{s.seed * 2654435761u + 7u};
    const double a = s.width * k / 2, b = s.depth * k / 2;
    const double top = tallest(s);
    double edge = std::max({s.smallest * 0.75, std::max(a, b) / 90.0});
    if (quick)
        edge *= 2.5;
    // The ground's height on a grid (half the mesh's step), looked up between.
    const double cell = edge / 2;
    const int nx = int(std::ceil(2 * a / cell)) + 3, ny = int(std::ceil(2 * b / cell)) + 3;
    const double x0 = -a - cell, y0 = -b - cell;
    std::vector<float> grid(std::size_t(nx) * std::size_t(ny));
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i)
            grid[std::size_t(j) * std::size_t(nx) + std::size_t(i)] = float(ground(s, noise, x0 + i * cell, y0 + j * cell));
    const auto groundAt = [&](double x, double y) {
        const double fx = std::clamp((x - x0) / cell, 0.0, double(nx - 1.001)),
                     fy = std::clamp((y - y0) / cell, 0.0, double(ny - 1.001));
        const int i = int(fx), j = int(fy);
        const double u = fx - i, v = fy - j;
        const auto g = [&](int ii, int jj) { return double(grid[std::size_t(jj) * std::size_t(nx) + std::size_t(ii)]); };
        return (g(i, j) * (1 - u) + g(i + 1, j) * u) * (1 - v) + (g(i, j + 1) * (1 - u) + g(i + 1, j + 1) * u) * v;
    };
    // Crags: rocks pushed up through the ground.
    struct Crag {
        Field f;
        Vec3 c;
        double reach, ratio, blend;
    };
    std::vector<Crag> crags;
    for (std::size_t i = 0; i < s.features.size(); ++i) {
        const TerrainFeature& f = s.features[i];
        if (f.kind != TerrainFeatureKind::Crag)
            continue;
        const double R = f.size * k / 2, H = f.depth * k;
        const double g = groundAt(f.x * k, f.y * k);
        // Its top about H above the ground (the planes cut it at 0.72-0.92 of its reach).
        const Vec3 c{f.x * k, f.y * k, g - 0.5 * H};
        const Vec3 r{R, R * 0.85, std::max(1.7 * H, 0.2 * R)};
        crags.push_back({oneRock(c, r, true, std::max(0.35, s.roughness), s.seed * 131u + std::uint32_t(i) * 7919u, 0), c,
                         std::max({r[0], r[1], r[2]}) * 1.2,
                         0.5 * std::min({r[0], r[1], r[2]}) / std::max({r[0], r[1], r[2]}), 0.12 * R});
    }
    const double bevel = s.edge == TerrainEdge::Bevelled ? s.bevel : 0.0;
    // The base's rim is made exactly (a clean edge, bevelled or not); the ground is tucked
    // inside it below the base's top, and meets its top edge above.
    const double rim = std::max(3 * edge, 2.0);
    const auto shape = [&](const Vec3& p) {
        double d = p[2] - groundAt(p[0], p[1]);
        for (const Crag& c : crags) {
            const double far = (std::sqrt((p[0] - c.c[0]) * (p[0] - c.c[0]) + (p[1] - c.c[1]) * (p[1] - c.c[1]) +
                                          (p[2] - c.c[2]) * (p[2] - c.c[2])) -
                                c.reach) *
                               c.ratio;
            if (far > d + c.blend)
                continue;
            d = smoothMin(d, c.f(p), c.blend);
        }
        const double side =
            outline(s, p[0], p[1]) + bevel + 0.5 * rim * std::clamp((s.baseThickness - p[2]) / edge, 0.0, 1.0);
        return std::max({d, -p[2], side});
    };
    Bounds bb;
    bb.expand(float(-a - edge), float(-b - edge), float(-0.01));
    bb.expand(float(a + edge), float(b + edge), float(top + edge));
    const MeshSolid ground = meshkernel::levelSet([&](const Vec3& p) { return -shape(p); }, bb, edge, 0.0);
    const Mesh outer = loft(outlinePoints(s, 0.0), outlinePoints(s, bevel), 0.0, s.baseThickness);
    const Mesh inner = prism(outlinePoints(s, bevel + rim), -1.0, s.baseThickness + 1.0);
    const MeshSolid ring = meshkernel::subtracted(meshkernel::fromTriangles(outer.positions, outer.indices),
                                                  {meshkernel::fromTriangles(inner.positions, inner.indices)});
    Mesh m;
    meshkernel::triangles(meshkernel::merged({ring, ground}), m.positions, m.indices);
    if (m.indices.empty())
        throw std::invalid_argument("The terrain came out empty.");
    return m;
}

Mesh marker(const TerrainSpec& s, const TerrainFeature& f)
{
    const double k = 1.0 / s.scale;
    const double z1 = tallest(s) + 1.0;
    std::vector<std::array<double, 2>> ring;
    if (f.kind == TerrainFeatureKind::Path) {
        const double ax = f.x * k, ay = f.y * k, bx = f.x2 * k, by = f.y2 * k;
        const double l = std::max(std::hypot(bx - ax, by - ay), 1e-6), w = f.size * k / 2;
        const double nx = -(by - ay) / l * w, ny = (bx - ax) / l * w;
        ring = {{ax - nx, ay - ny}, {bx - nx, by - ny}, {bx + nx, by + ny}, {ax + nx, ay + ny}};
    } else {
        const double R = f.size * k / 2;
        for (int i = 0; i < 32; ++i)
            ring.push_back({f.x * k + R * std::cos(i * 2 * kPi / 32), f.y * k + R * std::sin(i * 2 * kPi / 32)});
    }
    return prism(ring, -0.2, z1);
}

Body body(const TerrainSpec& spec, const Mesh& made, const std::string& name, double x, double y)
{
    TerrainSpec s = spec;
    Body b = hinge::pieceBody(made, name);
    s.offset = {b.placement.x, b.placement.y, b.placement.z};
    b.terrain = s;
    b.colour = 0x7D6B4F;
    b.placement.x += x;
    b.placement.y += y;
    return b;
}

ObjectId add(CommandStack& stack, Document& doc, TerrainSpec s, const std::string& name, double x, double y)
{
    Body b = body(s, make(s), doc.uniqueName(name), x, y);
    b.id = doc.allocateId();
    const ObjectId id = b.id;
    stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
    return id;
}

void update(CommandStack& stack, Document& doc, ObjectId id, TerrainSpec s)
{
    const Body* old = doc.find(id);
    if (!old || !old->terrain)
        throw std::invalid_argument("That shape isn't terrain.");
    const double bx = old->placement.x - old->terrain->offset[0];
    const double by = old->placement.y - old->terrain->offset[1];
    const Body before = *old;
    Body after = body(s, make(s), before.name, bx, by);
    after.id = id;
    after.colour = before.colour;
    after.placement.z = before.placement.z - before.terrain->offset[2] + after.terrain->offset[2];
    after.placement.rotationX = before.placement.rotationX;
    after.placement.rotationY = before.placement.rotationY;
    after.placement.rotationZ = before.placement.rotationZ;
    stack.execute(std::make_unique<EditBodyCommand>(before, after, "Change Terrain"));
}

}  // namespace forge::terrain
