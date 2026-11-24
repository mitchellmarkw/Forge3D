// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/PrintOrient.h"

#include "forge/core/Arrange.h"
#include "forge/core/Document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace orient {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a, const Vec3& fallback = {0, 0, -1})
{
    const double l = length(a);
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}

// Each triangle's middle, unit normal and area, worked out once.
struct Tri {
    Vec3 mid, n;
    double area;
};

struct Shape {
    std::vector<Tri> tris;
    std::vector<Vec3> points;
    double area = 0.0;
    Vec3 longest{0, 0, 1};  // the part's length: its main axis
    double elongation = 0.0;  // 0: as long as it's wide; towards 1: long and thin
};

Shape shapeOf(const Mesh& mesh)
{
    Shape s;
    s.points.reserve(mesh.vertexCount());
    for (std::size_t i = 0; i + 2 < mesh.positions.size(); i += 3)
        s.points.push_back({mesh.positions[i], mesh.positions[i + 1], mesh.positions[i + 2]});
    s.tris.reserve(mesh.triangleCount());
    Vec3 centre{0, 0, 0};
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const Vec3 a = s.points[mesh.indices[t]], b = s.points[mesh.indices[t + 1]], c = s.points[mesh.indices[t + 2]];
        const Vec3 x = cross(sub(b, a), sub(c, a));
        const double l = length(x);
        if (l < 1e-12)
            continue;
        Tri tri{mul(add(add(a, b), c), 1.0 / 3.0), mul(x, 1.0 / l), 0.5 * l};
        s.area += tri.area;
        centre = add(centre, mul(tri.mid, tri.area));
        s.tris.push_back(tri);
    }
    if (s.tris.empty())
        throw std::invalid_argument("The shape has no surface to print.");
    centre = mul(centre, 1.0 / s.area);
    // The main axis: the surface's spread, the largest way (power iteration).
    std::array<std::array<double, 3>, 3> cov{};
    for (const Tri& t : s.tris) {
        const Vec3 d = sub(t.mid, centre);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                cov[std::size_t(i)][std::size_t(j)] += t.area * d[std::size_t(i)] * d[std::size_t(j)];
    }
    Vec3 v = unit({1.0, 0.7, 0.4});
    double big = 0.0;
    for (int it = 0; it < 100; ++it) {
        Vec3 w{0, 0, 0};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                w[std::size_t(i)] += cov[std::size_t(i)][std::size_t(j)] * v[std::size_t(j)];
        big = length(w);
        v = unit(w, v);
    }
    s.longest = v;
    const double trace = cov[0][0] + cov[1][1] + cov[2][2];
    // As long as wide: the largest a third of the whole; a rod: nearly all of it.
    s.elongation = trace > 0 ? std::clamp((big / trace - 1.0 / 3.0) * 1.5, 0.0, 1.0) : 0.0;
    return s;
}

Facts measure(const Shape& shape, const Vec3& downIn, Goal goal, const Settings& set)
{
    Facts f;
    f.down = unit(downIn);
    const Vec3 up = mul(f.down, -1.0);
    double lo = 1e300, hi = -1e300;
    for (const Vec3& p : shape.points) {
        const double h = dot(p, up);
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    f.height = hi - lo;
    const double overhangCos = std::cos((90.0 - set.overhangAngle) * std::numbers::pi / 180.0);
    const double onPlate = 0.2;  // mm: faces this close to the plate lie on it
    for (const Tri& t : shape.tris) {
        const double facing = dot(t.n, f.down);  // 1: straight down
        const double above = dot(t.mid, up) - lo;
        if (facing > 0.985 && above < onPlate) {
            f.baseArea += t.area;
            continue;
        }
        if (facing > overhangCos && above > onPlate) {
            f.overhangArea += t.area;
            f.supportVolume += t.area * facing * above;
        }
        const double slope = std::abs(facing);
        if (slope > 0.5 && slope < 0.985)
            f.staircase += t.area * (slope - 0.5) * 2.0;
    }
    f.alongLength = shape.elongation * std::abs(dot(shape.longest, f.down));
    // Scores, each in millimetres-ish so the parts weigh against each other.
    const double across = std::sqrt(shape.area / 6.0);  // a typical width
    const double wobbly = f.baseArea < 0.02 * shape.area ? 1.0 - f.baseArea / (0.02 * shape.area) : 0.0;
    switch (goal) {
    case Goal::Fastest:
        // Time goes with layers, and with support to print.
        f.score = f.height + f.supportVolume / (across * across) + 0.2 * across * wobbly;
        break;
    case Goal::Quality:
        // Support scars and layer steps on the surface, as a share of it; a little for height.
        f.score = across * (3.0 * f.overhangArea + f.staircase) / shape.area + 0.05 * f.height + 0.3 * across * wobbly;
        break;
    case Goal::Strongest:
        // Layers along the length (it breaks between layers), standing steady, then little support.
        f.score = 4.0 * across * f.alongLength + 0.6 * across * wobbly + 0.5 * f.supportVolume / (across * across) +
                  0.02 * f.height;
        break;
    }
    return f;
}

// Directions to try: the six sides, the twelve edges and eight corners between them, the
// biggest flat faces (as they'd lie down), and a spread all round.
std::vector<Vec3> candidates(const Shape& shape)
{
    std::vector<Vec3> out;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            for (int z = -1; z <= 1; ++z)
                if (x || y || z)
                    out.push_back(unit({double(x), double(y), double(z)}));
    // Flat faces: triangles' normals gathered by direction (rounded), the biggest few.
    std::map<std::array<int, 3>, std::pair<double, Vec3>> faces;
    for (const Tri& t : shape.tris) {
        const std::array<int, 3> key{int(std::lround(t.n[0] * 40)), int(std::lround(t.n[1] * 40)),
                                     int(std::lround(t.n[2] * 40))};
        auto& [area, sum] = faces[key];
        area += t.area;
        sum = add(sum, mul(t.n, t.area));
    }
    std::vector<std::pair<double, Vec3>> flat;
    for (const auto& [k, v] : faces)
        flat.push_back(v);
    std::sort(flat.begin(), flat.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (std::size_t i = 0; i < flat.size() && i < 24; ++i)
        if (flat[i].first > 0.005 * shape.area)
            out.push_back(unit(flat[i].second));
    const int spread = 200;
    const double golden = std::numbers::pi * (3.0 - std::sqrt(5.0));
    for (int i = 0; i < spread; ++i) {
        const double z = 1.0 - 2.0 * (i + 0.5) / spread, r = std::sqrt(1.0 - z * z);
        out.push_back({r * std::cos(golden * i), r * std::sin(golden * i), z});
    }
    return out;
}

}  // namespace

const char* title(Goal goal)
{
    switch (goal) {
    case Goal::Quality: return "Best Quality";
    case Goal::Strongest: return "Strongest Print";
    default: return "Fastest Print";
    }
}

Facts facts(const Mesh& mesh, const Vec3& down, Goal goal, const Settings& s)
{
    return measure(shapeOf(mesh), down, goal, s);
}

Choice best(const Mesh& mesh, Goal goal, const Vec3& current, const Settings& s)
{
    const Shape shape = shapeOf(mesh);
    Choice c;
    c.now = measure(shape, current, goal, s);
    c.best = c.now;
    for (const Vec3& d : candidates(shape)) {
        const Facts f = measure(shape, d, goal, s);
        // Better by a little, so a near tie keeps the way it is (or the plainer way).
        if (f.score < c.best.score - 1e-6 * (1.0 + std::abs(c.best.score)))
            c.best = f;
    }
    // Round in a little: a hair off a side counts as that side.
    if (c.best.score > c.now.score * 0.98 && c.best.score > c.now.score - 0.05)
        c.best = c.now;
    return c;
}

Placement bestPlacement(const Document& doc, ObjectId id, Goal goal, Choice* choice, const Settings& s)
{
    const auto shown = doc.mesh(id);
    if (!shown || shown->empty())
        throw std::invalid_argument("The shape has no surface to print.");
    Mesh mesh = *shown;
    const Transform world = doc.worldTransform(id);
    for (std::size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        const Vec3 p = world.apply(Vec3{mesh.positions[i], mesh.positions[i + 1], mesh.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            mesh.positions[i + k] = float(p[k]);
    }
    const Choice c = best(mesh, goal, {0, 0, -1}, s);
    if (choice)
        *choice = c;
    return laidOnFace(doc, id, c.best.down);
}

}  // namespace orient

}  // namespace forge
