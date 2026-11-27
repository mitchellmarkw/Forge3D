// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Tree.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"
#include "RockField.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <random>
#include <stdexcept>

namespace forge::tree {

namespace {

using namespace rockfield;

Vec3 operator+(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 operator-(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 operator*(const Vec3& a, double t) { return {a[0] * t, a[1] * t, a[2] * t}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 1e-12 ? a * (1.0 / l) : Vec3{0, 0, 1};
}
// Two directions square to d, u x v = d.
void frame(const Vec3& d, Vec3& u, Vec3& v)
{
    const Vec3 other = std::abs(d[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    u = unit(cross(other, d));
    v = cross(d, u);
}
// d turned by `angle` away from itself, towards the side `around` (radians) picks.
Vec3 turned(const Vec3& d, double angle, double around)
{
    Vec3 u, v;
    frame(d, u, v);
    const Vec3 side = u * std::cos(around) + v * std::sin(around);
    return unit(d * std::cos(angle) + side * std::sin(angle));
}

// A unit sphere made of triangles (an icosahedron, its faces split `level` times).
const Mesh& unitSphere(int level)
{
    static std::map<int, Mesh> made;
    static std::mutex lock;
    std::lock_guard<std::mutex> hold(lock);
    if (const auto it = made.find(level); it != made.end())
        return it->second;
    const double t = (1.0 + std::sqrt(5.0)) / 2.0;
    std::vector<Vec3> p = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                           {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (Vec3& q : p)
        q = unit(q);
    std::vector<std::array<std::uint32_t, 3>> f = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                                                   {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                                   {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                                                   {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};
    for (int l = 0; l < level; ++l) {
        std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> mid;
        const auto middle = [&](std::uint32_t a, std::uint32_t b) {
            const auto key = std::minmax(a, b);
            if (const auto it = mid.find(key); it != mid.end())
                return it->second;
            p.push_back(unit((p[a] + p[b]) * 0.5));
            return mid[key] = std::uint32_t(p.size() - 1);
        };
        std::vector<std::array<std::uint32_t, 3>> next;
        for (const auto& [a, b, c] : f) {
            const std::uint32_t ab = middle(a, b), bc = middle(b, c), ca = middle(c, a);
            next.push_back({a, ab, ca});
            next.push_back({b, bc, ab});
            next.push_back({c, ca, bc});
            next.push_back({ab, bc, ca});
        }
        f = std::move(next);
    }
    Mesh m;
    for (const Vec3& q : p)
        for (int c = 0; c < 3; ++c)
            m.positions.push_back(float(q[c]));
    for (const auto& tri : f)
        for (std::uint32_t i : tri)
            m.indices.push_back(i);
    return made[level] = m;
}

struct Segment {
    Vec3 a, b;
    double ra, rb;
};
struct Clump {
    Vec3 c;
    Vec3 e1, e2, e3;  // its axes (right-handed)
    double a, b, h;   // half sizes along them
    double lumpy;     // how lumpy (share of its size)
};

struct Tree {
    std::vector<Segment> wood;
    std::vector<Clump> leaves;
    double crownBottom = -1.0;  // where a rounded crown starts (none: below 0)
};

Clump round(const Vec3& c, double r, double squash = 1.0)
{
    return {c, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, r, r, r * squash, 0.28};
}

// Branching, for broadleaf and dead trees: a branch from p along d, then its children.
struct Grower {
    const TreeSpec& s;
    Tree& t;
    std::mt19937& rng;
    double minR;
    int levels;
    double clumpR;
    bool dead;
    std::uniform_real_distribution<double> uni{0.0, 1.0};

    void grow(Vec3 p, Vec3 d, double length, double r, int level)
    {
        const int pieces = 3;
        const double end = r * 0.72;
        std::vector<Vec3> points{p};
        std::vector<double> radii{r};
        for (int i = 0; i < pieces; ++i) {
            const double wander = dead ? 0.45 : 0.22;
            d = unit(turned(d, wander * (uni(rng) - 0.3), uni(rng) * 2 * kPi) + Vec3{0, 0, dead ? -0.02 : 0.03});
            p = p + d * (length / pieces);
            points.push_back(p);
            radii.push_back(r + (end - r) * (i + 1.0) / pieces);
        }
        for (int i = 0; i < pieces; ++i)
            t.wood.push_back({points[std::size_t(i)], points[std::size_t(i) + 1], radii[std::size_t(i)],
                              radii[std::size_t(i) + 1]});
        const double childR = end * 0.68;
        if (level >= levels || childR < minR * 0.45) {
            if (s.leaves && !dead)
                t.leaves.push_back(round(p, clumpR * (0.8 + 0.4 * uni(rng)), 0.8));
            return;
        }
        const int children = 2 + int(std::lround(s.branches * 2.0)) - (dead && uni(rng) < 0.4 ? 1 : 0);
        for (int c = 0; c < children; ++c) {
            const double at = c == 0 ? 1.0 : 0.45 + 0.5 * uni(rng);
            const std::size_t i = std::min<std::size_t>(std::size_t(at * pieces), std::size_t(pieces));
            const Vec3 from = points[i];
            const Vec3 along = i > 0 ? unit(points[i] - points[i - 1]) : d;
            const Vec3 dir = turned(along, (c == 0 ? 0.2 : 0.6) + 0.4 * uni(rng), c * 2.4 + uni(rng));
            double len = length * (0.62 + 0.15 * uni(rng));
            if (dead && uni(rng) < 0.3)
                len *= 0.4;  // broken off
            grow(from, dir, len, childR * (c == 0 ? 1.0 : 0.85), level + 1);
        }
        // Inner foliage, so the crown isn't hollow.
        if (s.leaves && !dead && level >= levels - 1)
            t.leaves.push_back(round(points[std::size_t(pieces / 2 + 1)], clumpR * 0.85, 0.8));
    }
};

Tree skeleton(const TreeSpec& s, double minR)
{
    const double k = 1.0 / s.scale;
    const double H = s.height * k, W = s.crownWidth * k, R0 = std::max(s.trunkDiameter * k / 2, minR);
    std::mt19937 rng(s.seed * 2654435761u + std::uint32_t(s.kind) * 97u);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    Tree t;
    const double foliage = 0.7 + 0.6 * s.foliage;
    switch (s.kind) {
    case TreeKind::Broadleaf:
    case TreeKind::Dead: {
        const bool dead = s.kind == TreeKind::Dead;
        const double trunkTop = H * (dead ? 0.42 : 0.36);
        t.crownBottom = trunkTop;
        Vec3 p{0, 0, -0.4 * R0};
        for (int i = 0; i < 3; ++i) {
            const Vec3 q = Vec3{(uni(rng) - 0.5) * R0 * 0.6, (uni(rng) - 0.5) * R0 * 0.6, trunkTop * (i + 1) / 3.0};
            t.wood.push_back({p, q, R0 * (1.0 - 0.08 * i), R0 * (0.92 - 0.08 * i)});
            p = q;
        }
        Grower g{s, t, rng, minR, 2 + int(std::lround(s.branches * 1.4)), W * (0.075 + 0.05 * s.foliage), dead};
        const int mains = 3 + int(std::lround(s.branches * 3.0));
        const double r = R0 * 0.72 * 0.6;
        for (int i = 0; i < mains; ++i) {
            const double az = i * 2 * kPi / mains + uni(rng) * 0.6;
            const double up = dead ? 0.3 + 0.9 * uni(rng) : 0.25 + 0.55 * uni(rng);
            const Vec3 d = unit({std::cos(az), std::sin(az), up});
            g.grow(p, d, W * 0.36, r, 1);
        }
        g.grow(p, {0, 0, 1}, (H - trunkTop) * 0.35, r * 1.1, 1);  // the leader
        break;
    }
    case TreeKind::Conifer: {
        const int pieces = 8;
        const double tip = s.leaves ? 0.95 * H : H;  // under the top clump
        for (int i = 0; i < pieces; ++i) {
            const double z0 = -0.4 * R0 + (tip + 0.4 * R0) * i / pieces, z1 = -0.4 * R0 + (tip + 0.4 * R0) * (i + 1) / pieces;
            t.wood.push_back({{0, 0, z0}, {0, 0, z1}, std::max(R0 * (1.0 - double(i) / pieces), minR),
                              std::max(R0 * (1.0 - double(i + 1) / pieces), minR)});
        }
        const int tiers = 9 + int(std::lround(s.branches * 10.0));
        for (int i = 0; i < tiers; ++i) {
            const double f = double(i) / (tiers - 1);
            const double z = H * (0.16 + 0.76 * f);
            const double reach = (W / 2) * std::pow(1.0 - f, 0.9) * (0.85 + 0.25 * uni(rng)) + 0.04 * W;
            const int around = 5 + int(uni(rng) * 3);
            const double trunkR = std::max(R0 * (1.0 - z / H), minR);
            for (int b = 0; b < around; ++b) {
                const double az = b * 2 * kPi / around + uni(rng) * 0.7 + i * 0.9;
                Vec3 d = unit({std::cos(az), std::sin(az), -0.15 - 0.15 * uni(rng)});
                Vec3 p{0, 0, z};
                const double r = std::max(trunkR * 0.4, minR);
                for (int j = 0; j < 2; ++j) {
                    const Vec3 q = p + d * (reach / 2);
                    t.wood.push_back({p, q, j == 0 ? r : r * 0.75, j == 0 ? r * 0.75 : r * 0.5});
                    p = q;
                    d = unit(d + Vec3{0, 0, -0.2});
                }
                if (s.leaves) {
                    const double cr = std::max(reach * 0.36, W * 0.065) * foliage;
                    const Vec3 tip = p;
                    for (double at : {0.45, 0.78, 1.0}) {
                        const Vec3 c = Vec3{tip[0] * at, tip[1] * at, z + (tip[2] - z) * at};
                        t.leaves.push_back(round(c, cr * (1.15 - 0.35 * at), 0.65));
                    }
                }
            }
        }
        if (s.leaves) {
            Clump top = round({0, 0, H * 0.92}, W * 0.07 * foliage, 1.9);
            t.leaves.push_back(top);
        }
        break;
    }
    case TreeKind::Palm: {
        const double Ht = H * 0.84;
        const double bend = H * 0.12 * (0.5 + uni(rng));
        const double az = uni(rng) * 2 * kPi;
        const auto at = [&](double f) {
            return Vec3{std::cos(az) * bend * f * f, std::sin(az) * bend * f * f, -0.4 * R0 + (Ht + 0.4 * R0) * f};
        };
        const int pieces = 10;
        for (int i = 0; i < pieces; ++i) {
            const double f0 = double(i) / pieces, f1 = double(i + 1) / pieces;
            t.wood.push_back({at(f0), at(f1), R0 * (1.0 - 0.3 * f0 + (i == 0 ? 0.25 : 0.0)), R0 * (1.0 - 0.3 * f1)});
        }
        const Vec3 top = at(1.0);
        t.leaves.push_back(round(top, R0 * 1.15, 1.2));
        const int fronds = 8 + int(std::lround(s.branches * 6.0));
        const double length = W / 2 * 1.05;
        for (int i = 0; i < fronds; ++i) {
            const double a = i * 2 * kPi / fronds + uni(rng) * 0.4;
            Vec3 d = unit({std::cos(a), std::sin(a), 0.9 - 0.7 * uni(rng)});
            Vec3 p = top;
            const int steps = 8;
            const double step = length / steps;
            for (int j = 0; j < steps; ++j) {
                const Vec3 q = p + d * step;
                const double f = (j + 0.5) / steps;
                t.wood.push_back({p, q, std::max(R0 * 0.14 * (1 - f), minR), std::max(R0 * 0.14 * (1 - f - 1.0 / steps), minR)});
                if (s.leaves && j >= 1) {
                    const Vec3 e1 = unit(q - p);
                    const Vec3 e2 = unit(cross({0, 0, 1}, e1));
                    const Vec3 e3 = cross(e1, e2);
                    const double w = W * 0.07 * std::sin(kPi * f) * foliage + minR;
                    t.leaves.push_back({(p + q) * 0.5, e1, e2, e3, step * 0.62, w, std::max(minR * 1.1, w * 0.16), 0.12});
                }
                p = q;
                d = unit(d + Vec3{0, 0, -0.28});
            }
        }
        break;
    }
    }
    return t;
}

double reachUp(const Tree& t, double sz)
{
    double top = 0;
    for (const Segment& g : t.wood)
        top = std::max({top, g.a[2] * sz + g.ra, g.b[2] * sz + g.rb});
    for (const Clump& c : t.leaves)
        top = std::max(top, c.c[2] * sz + std::max({c.a * std::abs(c.e1[2]), c.b * std::abs(c.e2[2]), c.h * std::abs(c.e3[2])}));
    return top;
}

double reachOut(const Tree& t, double sx)
{
    double out = 0;
    for (const Segment& g : t.wood)
        out = std::max({out, std::hypot(g.a[0], g.a[1]) * sx + g.ra, std::hypot(g.b[0], g.b[1]) * sx + g.rb});
    for (const Clump& c : t.leaves)
        out = std::max(out, std::hypot(c.c[0], c.c[1]) * sx + std::max({c.a, c.b}));
    return out;
}

// Stretched (its positions, not its thicknesses) to the height and crown width asked for.
void fit(Tree& t, double H, double W)
{
    const auto solve = [](auto reach, double want) {
        double lo = 0.05, hi = 20.0;
        for (int i = 0; i < 40; ++i) {
            const double m = (lo + hi) / 2;
            (reach(m) < want ? lo : hi) = m;
        }
        return (lo + hi) / 2;
    };
    const double sz = solve([&](double v) { return reachUp(t, v); }, H);
    const double sx = solve([&](double v) { return reachOut(t, v); }, W / 2);
    const auto stretch = [&](Vec3& p) {
        p[0] *= sx;
        p[1] *= sx;
        if (p[2] > 0)
            p[2] *= sz;
    };
    for (Segment& g : t.wood) {
        stretch(g.a);
        stretch(g.b);
    }
    for (Clump& c : t.leaves)
        stretch(c.c);
    if (t.crownBottom < 0)
        return;
    // A rounded crown: what was fitted to a box drawn in to the ellipsoid inside it (points
    // moved, so the branches still reach their clumps).
    const double bottom = t.crownBottom * sz;
    const double zc = (bottom + H) / 2, rz = (H - bottom) / 2, rx = W / 2;
    const auto round = [&](Vec3& p) {
        if (p[2] <= bottom)
            return;
        const double qx = p[0] / rx, qy = p[1] / rx, qz = (p[2] - zc) / rz;
        const double box = std::max(std::hypot(qx, qy), std::abs(qz));
        const double ball = std::sqrt(qx * qx + qy * qy + qz * qz);
        if (ball < 1e-9)
            return;
        const double w = std::clamp((p[2] - bottom) / (0.25 * rz), 0.0, 1.0);  // none at the trunk's top
        const double f = 1.0 + (box / ball - 1.0) * w;
        p = {p[0] * f, p[1] * f, zc + (p[2] - zc) * f};
    };
    for (Segment& g : t.wood) {
        round(g.a);
        round(g.b);
    }
    for (Clump& c : t.leaves)
        round(c.c);
}

struct Parts {
    std::vector<Mesh> meshes;
    int sidesMost;
    int sphereLevel;
    double smallest;
    Noise noise;

    void frustum(const Segment& g)
    {
        const Vec3 d = unit(g.b - g.a);
        Vec3 u, v;
        frame(d, u, v);
        const int n = std::clamp(int(2 * kPi * std::max(g.ra, g.rb) / (smallest * 1.5)), 6, sidesMost);
        Mesh m;
        for (const auto& [c, r] : {std::pair{g.a, g.ra}, std::pair{g.b, g.rb}})
            for (int i = 0; i < n; ++i) {
                const double a = i * 2 * kPi / n;
                const Vec3 q = c + (u * std::cos(a) + v * std::sin(a)) * r;
                for (int k = 0; k < 3; ++k)
                    m.positions.push_back(float(q[k]));
            }
        const auto tri = [&](int a, int b, int c) {
            m.indices.push_back(std::uint32_t(a));
            m.indices.push_back(std::uint32_t(b));
            m.indices.push_back(std::uint32_t(c));
        };
        for (int i = 1; i + 1 < n; ++i) {
            tri(0, i + 1, i);
            tri(n, n + i, n + i + 1);
        }
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            tri(i, j, n + j);
            tri(i, n + j, n + i);
        }
        meshes.push_back(std::move(m));
    }
    void ball(const Vec3& c, double r) { blob({c, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, r, r, r, 0.0}, 1); }
    void blob(const Clump& c, int level)
    {
        const Mesh& sphere = unitSphere(level);
        Mesh m = sphere;
        for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
            const Vec3 q{sphere.positions[i], sphere.positions[i + 1], sphere.positions[i + 2]};
            const Vec3 w = c.c + c.e1 * (q[0] * c.a) + c.e2 * (q[1] * c.b) + c.e3 * (q[2] * c.h);
            const double f = 1.0 + c.lumpy * noise.fbm(w[0] / (c.a * 0.55), w[1] / (c.a * 0.55), w[2] / (c.a * 0.55), 2) * 1.6;
            const Vec3 p = c.c + c.e1 * (q[0] * c.a * f) + c.e2 * (q[1] * c.b * f) + c.e3 * (q[2] * c.h * f);
            for (int k = 0; k < 3; ++k)
                m.positions[i + std::size_t(k)] = float(p[std::size_t(k)]);
        }
        meshes.push_back(std::move(m));
    }
};

}  // namespace

const char* title(TreeKind k)
{
    switch (k) {
    case TreeKind::Broadleaf: return "Broadleaf (oak-like)";
    case TreeKind::Conifer: return "Conifer (pine or fir)";
    case TreeKind::Dead: return "Dead tree";
    case TreeKind::Palm: return "Palm";
    }
    return "";
}

TreeSpec defaults(TreeKind k, const TreeSpec& from)
{
    TreeSpec s = from;
    s.kind = k;
    switch (k) {
    case TreeKind::Broadleaf: s.height = 9000, s.crownWidth = 7000, s.trunkDiameter = 600, s.leaves = true; break;
    case TreeKind::Conifer: s.height = 12000, s.crownWidth = 5000, s.trunkDiameter = 450, s.leaves = true; break;
    case TreeKind::Dead: s.height = 7000, s.crownWidth = 6000, s.trunkDiameter = 550, s.leaves = false; break;
    case TreeKind::Palm: s.height = 10000, s.crownWidth = 6000, s.trunkDiameter = 350, s.leaves = true; break;
    }
    return s;
}

double thinnest(const TreeSpec& s)
{
    return std::max(2.5 * s.smallest, 0.8);
}

void check(const TreeSpec& s)
{
    if (!(s.scale >= 1.0 && s.scale <= 1000.0))
        throw std::invalid_argument("The scale must be from 1:1 to 1:1000.");
    if (!(s.height >= 300 && s.height <= 150000 && s.crownWidth >= 200 && s.crownWidth <= 100000))
        throw std::invalid_argument("A tree must be from 0.3 to 150 m tall, its crown from 0.2 to 100 m across.");
    if (!(s.trunkDiameter >= 20 && s.trunkDiameter <= std::min(s.crownWidth, s.height / 3)))
        throw std::invalid_argument("The trunk must be at least 20 mm across, and narrower than the crown.");
    if (!(s.branches >= 0 && s.branches <= 1 && s.foliage >= 0 && s.foliage <= 1))
        throw std::invalid_argument("Branches and foliage must be from 0 to 100%.");
    if (!(s.lean >= 0 && s.lean <= 30))
        throw std::invalid_argument("A tree can lean at most 30 degrees.");
    if (!(s.smallest >= 0.02 && s.smallest <= 2))
        throw std::invalid_argument("The smallest detail must be from 0.02 to 2 mm.");
    if (s.height / s.scale < 10 * thinnest(s))
        throw std::invalid_argument("The tree is too small to print at this scale: make it taller, or the smallest "
                                    "detail finer.");
}

Mesh make(const TreeSpec& s, bool quick)
{
    check(s);
    const double k = 1.0 / s.scale;
    const double minR = thinnest(s) / 2;
    Tree t = skeleton(s, minR);
    fit(t, s.height * k, s.crownWidth * k);
    // Leaning: turned about its foot.
    if (s.lean > 0) {
        const double a = s.lean * kPi / 180, c = std::cos(a), sn = std::sin(a);
        const auto turn = [&](Vec3& p) {
            const double x = p[0] * c + p[2] * sn, z = -p[0] * sn + p[2] * c;
            p[0] = x;
            p[2] = z;
        };
        for (Segment& g : t.wood) {
            turn(g.a);
            turn(g.b);
        }
        for (Clump& cl : t.leaves) {
            turn(cl.c);
            turn(cl.e1);
            turn(cl.e2);
            turn(cl.e3);
        }
    }
    // Roots, flaring out from the trunk's foot.
    const double R0 = std::max(s.trunkDiameter * k / 2, minR);
    std::mt19937 rng(s.seed * 7919u + 3u);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    const int roots = 4 + int(uni(rng) * 3);
    for (int i = 0; i < roots; ++i) {
        const double a = i * 2 * kPi / roots + uni(rng) * 0.6;
        const double out = R0 * (2.0 + 0.8 * uni(rng));
        t.wood.push_back({{0, 0, R0 * 1.3}, {std::cos(a) * out, std::sin(a) * out, -R0 * 0.45}, R0 * 0.5,
                          std::max(R0 * 0.16, minR)});
    }
    for (Segment& g : t.wood) {
        g.ra = std::max(g.ra, minR);
        g.rb = std::max(g.rb, minR);
    }

    Parts parts{{}, quick ? 8 : 16, quick ? 2 : 3, s.smallest, Noise{s.seed * 40503u + 11u}};
    for (const Segment& g : t.wood) {
        parts.frustum(g);
        parts.ball(g.b, g.rb);  // the joint to the next piece
    }
    for (const Clump& c : t.leaves)
        parts.blob(c, parts.sphereLevel);
    Mesh m;
    if (quick) {
        for (const Mesh& p : parts.meshes) {
            const std::uint32_t base = std::uint32_t(m.positions.size() / 3);
            m.positions.insert(m.positions.end(), p.positions.begin(), p.positions.end());
            for (std::uint32_t i : p.indices)
                m.indices.push_back(base + i);
        }
        return m;
    }
    std::vector<MeshSolid> solids;
    solids.reserve(parts.meshes.size());
    for (const Mesh& p : parts.meshes)
        solids.push_back(meshkernel::fromTriangles(p.positions, p.indices));
    const MeshSolid whole = meshkernel::trimmed(meshkernel::merged(solids), {0, 0, 0}, {0, 0, 1});
    meshkernel::triangles(whole, m.positions, m.indices);
    if (m.indices.empty())
        throw std::invalid_argument("The tree came out empty.");
    return m;
}

Body body(const TreeSpec& spec, const Mesh& made, const std::string& name, double x, double y)
{
    TreeSpec s = spec;
    Body b = hinge::pieceBody(made, name);
    s.offset = {b.placement.x, b.placement.y, b.placement.z};
    b.tree = s;
    b.colour = s.leaves && s.kind != TreeKind::Dead ? 0x55703F : 0x6E5843;
    b.placement.x += x;
    b.placement.y += y;
    return b;
}

ObjectId add(CommandStack& stack, Document& doc, TreeSpec s, const std::string& name, double x, double y)
{
    Body b = body(s, make(s), doc.uniqueName(name), x, y);
    b.id = doc.allocateId();
    const ObjectId id = b.id;
    stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
    return id;
}

void update(CommandStack& stack, Document& doc, ObjectId id, TreeSpec s)
{
    const Body* old = doc.find(id);
    if (!old || !old->tree)
        throw std::invalid_argument("That shape isn't a tree.");
    const double bx = old->placement.x - old->tree->offset[0];
    const double by = old->placement.y - old->tree->offset[1];
    const Body before = *old;
    Body after = body(s, make(s), before.name, bx, by);
    after.id = id;
    after.colour = before.colour;
    after.placement.z = before.placement.z - before.tree->offset[2] + after.tree->offset[2];
    after.placement.rotationX = before.placement.rotationX;
    after.placement.rotationY = before.placement.rotationY;
    after.placement.rotationZ = before.placement.rotationZ;
    stack.execute(std::make_unique<EditBodyCommand>(before, after, "Change Tree"));
}

}  // namespace forge::tree
