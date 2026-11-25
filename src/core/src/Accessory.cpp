// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Accessory.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace {

using P2 = std::array<double, 2>;
constexpr double kPi = std::numbers::pi;

double len2(double x, double y) { return std::sqrt(x * x + y * y); }
double len3(double x, double y, double z) { return std::sqrt(x * x + y * y + z * z); }

// ---- Distances (negative inside) ----------------------------------------------------

double sphere(const Vec3& p, const Vec3& c, double r) { return len3(p[0] - c[0], p[1] - c[1], p[2] - c[2]) - r; }

// A rod with round ends from a to b.
double capsule(const Vec3& p, const Vec3& a, const Vec3& b, double r)
{
    const Vec3 pa{p[0] - a[0], p[1] - a[1], p[2] - a[2]}, ba{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double l2 = ba[0] * ba[0] + ba[1] * ba[1] + ba[2] * ba[2];
    const double h = l2 > 0 ? std::clamp((pa[0] * ba[0] + pa[1] * ba[1] + pa[2] * ba[2]) / l2, 0.0, 1.0) : 0.0;
    return len3(pa[0] - ba[0] * h, pa[1] - ba[1] * h, pa[2] - ba[2] * h) - r;
}

// A box (half sizes), its corners rounded by r.
double box(const Vec3& p, const Vec3& c, const Vec3& half, double r = 0.0)
{
    const double q[3] = {std::abs(p[0] - c[0]) - half[0] + r, std::abs(p[1] - c[1]) - half[1] + r,
                         std::abs(p[2] - c[2]) - half[2] + r};
    return len3(std::max(q[0], 0.0), std::max(q[1], 0.0), std::max(q[2], 0.0)) +
           std::min(std::max({q[0], q[1], q[2]}), 0.0) - r;
}

// A flat outline (anticlockwise or not): distance to its edge, negative inside.
double outline(const std::vector<P2>& poly, double x, double y)
{
    double d = 1e300;
    bool inside = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const P2& a = poly[j];
        const P2& b = poly[i];
        const double ex = b[0] - a[0], ey = b[1] - a[1];
        const double wx = x - a[0], wy = y - a[1];
        const double l2 = ex * ex + ey * ey;
        const double t = l2 > 0 ? std::clamp((wx * ex + wy * ey) / l2, 0.0, 1.0) : 0.0;
        d = std::min(d, len2(wx - ex * t, wy - ey * t));
        if ((a[1] > y) != (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0])
            inside = !inside;
    }
    return inside ? -d : d;
}

// A flat outline given a thickness that grows from `edge` at its rim to `middle` once
// `reach` in from it: a blade's or a leaf's cross-section.
double bladed(const std::vector<P2>& poly, const Vec3& p, double edge, double middle, double reach)
{
    const double d = outline(poly, p[0], p[1]);
    const double t = edge + (middle - edge) * std::clamp(-d / std::max(reach, 1e-6), 0.0, 1.0);
    return std::max(d, std::abs(p[2]) - t / 2.0);
}

// Smooth union: shapes blend into each other over k.
double blend(double a, double b, double k)
{
    const double h = std::clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return b * (1.0 - h) + a * h - k * h * (1.0 - h);
}

using Field = std::function<double(const Vec3&)>;

struct Shape {
    Field distance;
    Bounds box;
    double edge = 0.1;  // how finely to sample
};

Bounds boundsOf(Vec3 lo, Vec3 hi, double pad)
{
    Bounds b;
    b.min = {float(lo[0] - pad), float(lo[1] - pad), float(lo[2] - pad)};
    b.max = {float(hi[0] + pad), float(hi[1] + pad), float(hi[2] + pad)};
    b.valid = true;
    return b;
}

// Fine enough for the thinnest part, but not more than about four million samples.
double sampling(const Bounds& b, double thinnest)
{
    const auto s = b.size();
    const double cells = std::cbrt(double(s[0]) * s[1] * s[2] / 4.0e6);
    return std::max({thinnest / 3.0, cells, 0.02});
}

double printed(double life, double scale, double least = accessory::kThinnest)
{
    return std::max(life / scale, least);
}

// ---- The makers (each built along +X, lying flat, centred later) -----------------

Shape blade(const AccessorySettings& s)
{
    const double L = s.bladeLength / s.scale;
    const double W = std::max(s.bladeWidth / s.scale, 1.6);
    const double T = printed(s.bladeThickness, s.scale);
    const double edgeT = std::min(0.5, T);
    const double G = std::max(s.guardWidth / s.scale, W);
    const double guardT = std::max(T * 1.6, 1.0);   // thick
    const double guardL = std::max(W * 0.35, 1.0);  // along the blade
    const double H = std::max(s.gripLength / s.scale, s.handle * 2.0);
    const double r = s.handle / 2.0;

    // The blade's outline, from the guard (x = 0) to the tip.
    std::vector<P2> poly;
    const double tipLen = s.tip == BladeTip::Round ? W / 2.0 : s.tip == BladeTip::Angled ? W * 0.9 : W * 1.6;
    const double straight = std::max(L - tipLen, 0.0);
    poly.push_back({0.0, -W / 2.0});
    poly.push_back({straight, -W / 2.0 * 0.92});
    if (s.tip == BladeTip::Point) {
        poly.push_back({L, 0.0});
    } else if (s.tip == BladeTip::Round) {
        for (int i = 0; i <= 16; ++i) {
            const double a = -kPi / 2.0 + kPi * i / 16.0;
            poly.push_back({straight + std::cos(a) * W / 2.0 * 0.92, std::sin(a) * W / 2.0 * 0.92});
        }
    } else {
        poly.push_back({L, -W / 2.0 * 0.6});  // the back runs on, the edge cut at a slant
    }
    poly.push_back({s.tip == BladeTip::Angled ? L - tipLen * 0.15 : straight, W / 2.0 * 0.92});
    poly.push_back({0.0, W / 2.0});

    const double gripEnd = -guardL - H;
    const double pommelR = r * 1.5;
    Shape out;
    out.distance = [=](const Vec3& p) {
        double d = bladed(poly, p, edgeT, T, W * 0.4);
        d = std::min(d, box(p, {-guardL / 2.0, 0, 0}, {guardL / 2.0, G / 2.0, guardT / 2.0}, std::min(guardT, guardL) * 0.3));
        d = blend(d, capsule(p, {-guardL, 0, 0}, {gripEnd, 0, 0}, r), r * 0.2);
        if (s.pommel)
            d = blend(d, sphere(p, {gripEnd - pommelR * 0.6, 0, 0}, pommelR), r * 0.3);
        return d;
    };
    const double back = gripEnd - (s.pommel ? pommelR * 1.7 : r);
    out.box = boundsOf({back, -G / 2.0, -std::max({guardT, 2 * r, 2 * pommelR}) / 2.0},
                       {L, G / 2.0, std::max({guardT, 2 * r, 2 * pommelR}) / 2.0}, 0.5);
    out.edge = sampling(out.box, edgeT * 1.2);
    return out;
}

std::vector<P2> shieldOutline(ShieldShape shape, double W, double H)
{
    std::vector<P2> poly;
    const int n = 32;
    if (shape == ShieldShape::Heater) {
        const double y0 = H / 2.0 - 0.35 * H;  // where the sides start to come in
        poly.push_back({-W / 2.0, H / 2.0});
        for (int i = 0; i <= n; ++i) {  // down the left, to the point
            const double t = double(i) / n;
            poly.push_back({-W / 2.0 * (1.0 - t * t), y0 - t * (y0 + H / 2.0)});
        }
        for (int i = n - 1; i >= 0; --i) {  // and up the right
            const double t = double(i) / n;
            poly.push_back({W / 2.0 * (1.0 - t * t), y0 - t * (y0 + H / 2.0)});
        }
        poly.push_back({W / 2.0, H / 2.0});
    } else {  // kite: a round top, then a long taper to the point
        const double top = 0.3 * H, yw = H / 2.0 - top;
        for (int i = 0; i <= n; ++i) {
            const double a = kPi * i / n;  // over the top, right to left
            poly.push_back({W / 2.0 * std::cos(a), yw + top * std::sin(a)});
        }
        for (int i = 1; i < n; ++i) {  // down the left
            const double t = double(i) / n;
            poly.push_back({-W / 2.0 * std::pow(1.0 - t, 1.3), yw - t * (yw + H / 2.0)});
        }
        poly.push_back({0.0, -H / 2.0});
        for (int i = n - 1; i >= 1; --i) {
            const double t = double(i) / n;
            poly.push_back({W / 2.0 * std::pow(1.0 - t, 1.3), yw - t * (yw + H / 2.0)});
        }
    }
    return poly;
}

Shape shield(const AccessorySettings& s)
{
    const double W = s.shieldWidth / s.scale;
    const double H = s.shield == ShieldShape::Round ? W : s.shieldHeight / s.scale;
    const double t = printed(s.shieldThickness, s.scale, 1.0);
    const std::vector<P2> poly = s.shield == ShieldShape::Round ? std::vector<P2>{} : shieldOutline(s.shield, W, H);
    // Bowed round an upright line behind it: the width runs round an arc this long.
    const double bow = std::clamp(s.shieldCurve, 0.0, 1.0) * 2.0;  // radians across the whole width
    const double R = bow > 1e-3 ? W / bow : 0.0;
    const double r = s.handle / 2.0;
    const double bossR = 0.12 * std::min(W, H);
    const double barLen = std::max(4.0 * s.handle, 0.3 * H);
    const double gap = std::max(1.6 * s.handle, 2.0);  // room for the fist between the bar and the back
    const bool round = s.shield == ShieldShape::Round;
    Shape out;
    out.distance = [=](const Vec3& p) {
        // Flattened: across the arc (u), up (y), out of the face (w).
        double u = p[0], w = p[2];
        if (R > 0) {
            const double rr = len2(p[0], p[2] + R);
            u = R * std::atan2(p[0], p[2] + R);
            w = rr - R;
        }
        const double flat = round ? len2(u, p[1]) - W / 2.0 : outline(poly, u, p[1]);
        double d = std::max(flat, std::abs(w) - t / 2.0);
        if (s.boss)
            d = blend(d, sphere({u, p[1], w}, {0, 0, t / 2.0 - bossR * 0.45}, bossR), t * 0.6);
        if (s.gripBar) {
            const double z = -t / 2.0 - gap - r;  // behind the middle
            d = std::min(d, capsule({u, p[1], w}, {0, -barLen / 2.0, z}, {0, barLen / 2.0, z}, r));
            for (double end : {-1.0, 1.0})  // posts holding its ends
                d = blend(d, box({u, p[1], w}, {0, end * barLen / 2.0, (z - t / 2.0) / 2.0},
                                 {r * 0.9, r * 0.9, (-t / 2.0 - z) / 2.0 + r * 0.3}, r * 0.4),
                          r * 0.3);
        }
        return d;
    };
    const double sag = R > 0 ? R * (1.0 - std::cos(bow / 2.0)) : 0.0;
    const double front = t / 2.0 + (s.boss ? bossR * 0.6 : 0.0);
    const double backZ = -t / 2.0 - sag - (s.gripBar ? gap + 2.0 * r : 0.0);
    out.box = boundsOf({-W / 2.0, -H / 2.0, backZ}, {W / 2.0, H / 2.0, front}, 1.0);
    out.edge = sampling(out.box, t * 1.2);
    return out;
}

Shape staff(const AccessorySettings& s)
{
    const double L = s.staffLength / s.scale;
    const double r = s.handle / 2.0;
    const double leafL = s.staffEnd == StaffEnd::Spear ? std::max(0.14 * L, 6.0 * r) : 0.0;
    const double leafW = std::max(leafL * 0.28, 3.0 * r);
    const double orbR = 1.8 * r;
    std::vector<P2> leaf;
    if (s.staffEnd == StaffEnd::Spear) {
        // A leaf from the socket to the point, widest a third of the way.
        const int n = 24;
        for (int i = 0; i <= n; ++i) {
            const double t = double(i) / n;
            leaf.push_back({L / 2.0 + t * leafL, -leafW / 2.0 * std::sin(kPi * std::pow(t, 0.7))});
        }
        for (int i = n - 1; i >= 1; --i) {
            const double t = double(i) / n;
            leaf.push_back({L / 2.0 + t * leafL, leafW / 2.0 * std::sin(kPi * std::pow(t, 0.7))});
        }
    }
    const double T = std::max(r * 0.9, accessory::kThinnest);  // the leaf's middle
    Shape out;
    out.distance = [=](const Vec3& p) {
        double d = capsule(p, {-L / 2.0, 0, 0}, {L / 2.0, 0, 0}, r);
        if (s.staffEnd == StaffEnd::Spear)
            d = blend(d, bladed(leaf, p, 0.5, T, leafW * 0.4), r * 0.5);
        if (s.staffEnd == StaffEnd::Orb) {
            d = blend(d, sphere(p, {L / 2.0 + orbR * 0.8, 0, 0}, orbR), r * 0.6);
            d = blend(d, capsule(p, {L / 2.0 - 2.5 * r, 0, 0}, {L / 2.0 - 1.5 * r, 0, 0}, r * 1.35), r * 0.3);
        }
        return d;
    };
    const double reach = std::max({r, leafW / 2.0 * (s.staffEnd == StaffEnd::Spear ? 1.0 : 0.0),
                                   s.staffEnd == StaffEnd::Orb ? orbR : 0.0});
    const double tip = L / 2.0 + (s.staffEnd == StaffEnd::Spear ? leafL : s.staffEnd == StaffEnd::Orb ? orbR * 1.9 : r);
    out.box = boundsOf({-L / 2.0 - r, -reach, -reach}, {tip, reach, reach}, 0.5);
    out.edge = sampling(out.box, std::min(r * 2.0, 0.6) * 1.2);
    return out;
}

Shape belt(const AccessorySettings& s)
{
    // The band's inside is the waist (and its fit), an ellipse a across and b front to back.
    const double a = s.waistWidth / 2.0 + s.fit, b = s.waistDepth / 2.0 + s.fit;
    const double w = std::max(s.beltWidth / s.scale, 1.2);  // up and down
    const double t = printed(5.0, s.scale);                  // thick
    const double bw = w * 1.35, bh = w * 1.15;               // the buckle, across and up
    const double bar = std::max(w * 0.18, 0.6);
    Shape out;
    out.distance = [=](const Vec3& p) {
        // How far out of the waist's ellipse, along the line from its middle.
        const double k = len2(p[0] / a, p[1] / b);
        const double rho = len2(p[0], p[1]);
        const double g = k > 1e-9 ? (k - 1.0) * rho / k : -std::min(a, b);
        double d = std::max(std::abs(g - t / 2.0) - t / 2.0, std::abs(p[2]) - w / 2.0);
        // The buckle on the front (-Y): a frame standing proud of the band, and its prong.
        const Vec3 c{0.0, -b - t - bar / 2.0, 0.0};
        const double frame = std::max(box(p, c, {bw / 2.0, bar / 2.0, bh / 2.0}, bar * 0.3),
                                      -box(p, c, {bw / 2.0 - bar, bar, bh / 2.0 - bar}));
        d = std::min(d, frame);
        d = std::min(d, box(p, {0.0, c[1] + bar * 0.2, 0.0}, {bar * 0.45, bar * 0.45, bh / 2.0}, bar * 0.2));
        if (s.beltOpen)  // the gap at the back
            d = std::max(d, -box(p, {0.0, b + t / 2.0, 0.0}, {s.beltGap / 2.0, t * 2.0, w}));
        return d;
    };
    out.box = boundsOf({-a - t, -b - t - bar * 1.5, -std::max(w, bh) / 2.0}, {a + t, b + t, std::max(w, bh) / 2.0}, 0.5);
    out.edge = sampling(out.box, t * 1.2);
    return out;
}

Shape holster(const AccessorySettings& s)
{
    const double L = s.holsterLength / s.scale, W = s.holsterWidth / s.scale, D = s.holsterDepth / s.scale;
    const double wall = accessory::kThinnest;
    const double bw = std::max(s.beltWidth / s.scale, 1.2) + 0.3;  // the belt's room through the loop
    const double bt = printed(5.0, s.scale) + 0.3;
    Shape out;
    // Standing up, open at the top (Z), the loop on the back (+Y).
    out.distance = [=](const Vec3& p) {
        const double outer = box(p, {0, 0, L / 2.0}, {W / 2.0, D / 2.0, L / 2.0}, std::min(W, D) * 0.35);
        const double inner = box(p, {0, 0, L / 2.0 + wall}, {W / 2.0 - wall, D / 2.0 - wall, L / 2.0},
                                 std::max(std::min(W, D) * 0.35 - wall, 0.05));
        double d = std::max(outer, -inner);
        // The loop: a tunnel across the back near the top, for the belt.
        const Vec3 c{0, D / 2.0 + bt / 2.0, L - bw / 2.0 - wall * 2.0};
        const double loop = std::max(box(p, c, {std::min(W / 2.0, bw), bt / 2.0 + wall, bw / 2.0 + wall}, wall * 0.4),
                                     -box(p, c, {W, bt / 2.0, bw / 2.0}));
        return std::min(d, loop);
    };
    out.box = boundsOf({-W / 2.0, -D / 2.0, 0.0}, {W / 2.0, D / 2.0 + bt + 2 * wall, L}, 0.5);
    out.edge = sampling(out.box, wall * 1.2);
    return out;
}

Shape shapeOf(const AccessorySettings& s)
{
    switch (s.kind) {
    case AccessoryKind::Blade: return blade(s);
    case AccessoryKind::Shield: return shield(s);
    case AccessoryKind::Staff: return staff(s);
    case AccessoryKind::Belt: return belt(s);
    case AccessoryKind::Holster: return holster(s);
    }
    return blade(s);
}

}  // namespace

namespace accessory {

const char* title(AccessoryKind kind)
{
    switch (kind) {
    case AccessoryKind::Blade: return "Blade";
    case AccessoryKind::Shield: return "Shield";
    case AccessoryKind::Staff: return "Staff or spear";
    case AccessoryKind::Belt: return "Belt with buckle";
    case AccessoryKind::Holster: return "Holster";
    }
    return "";
}

void check(const AccessorySettings& s)
{
    const auto between = [](double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; };
    if (!between(s.scale, 1.0, 100.0))
        throw std::invalid_argument("The scale must be from 1:1 to 1:100.");
    if (!between(s.handle, 0.3, 40.0))
        throw std::invalid_argument("The handle must be from 0.3 to 40 mm across.");
    switch (s.kind) {
    case AccessoryKind::Blade:
        if (!between(s.bladeLength, 20.0, 5000.0) || !between(s.bladeWidth, 5.0, 1000.0) ||
            !between(s.bladeThickness, 0.5, 200.0) || !between(s.guardWidth, 0.0, 2000.0) ||
            !between(s.gripLength, 10.0, 2000.0))
            throw std::invalid_argument("The blade's sizes are out of range: check its length, width and grip.");
        break;
    case AccessoryKind::Shield:
        if (!between(s.shieldWidth, 50.0, 5000.0) || !between(s.shieldHeight, 50.0, 5000.0) ||
            !between(s.shieldThickness, 1.0, 200.0) || !between(s.shieldCurve, 0.0, 1.0))
            throw std::invalid_argument("The shield's sizes are out of range: check its width, height and thickness.");
        break;
    case AccessoryKind::Staff:
        if (!between(s.staffLength, 100.0, 10000.0))
            throw std::invalid_argument("The staff must be from 10 cm to 10 m long (life size).");
        break;
    case AccessoryKind::Belt:
        if (!between(s.waistWidth, 2.0, 1000.0) || !between(s.waistDepth, 2.0, 1000.0))
            throw std::invalid_argument("The waist must be from 2 mm across.");
        if (!between(s.beltWidth, 5.0, 500.0) || !between(s.fit, 0.0, 2.0) || !between(s.beltGap, 0.0, 100.0))
            throw std::invalid_argument("The belt's sizes are out of range: check its width, fit and gap.");
        if (s.beltOpen && s.beltGap >= s.waistWidth)
            throw std::invalid_argument("The gap at the back is wider than the waist.");
        break;
    case AccessoryKind::Holster:
        if (!between(s.holsterLength, 20.0, 2000.0) || !between(s.holsterWidth, 10.0, 1000.0) ||
            !between(s.holsterDepth, 10.0, 1000.0))
            throw std::invalid_argument("The holster's sizes are out of range.");
        if (s.holsterWidth / s.scale < 2.0 * kThinnest + 0.5 || s.holsterDepth / s.scale < 2.0 * kThinnest + 0.5)
            throw std::invalid_argument("The holster is too small at this scale to have room inside.");
        break;
    }
}

Mesh make(const AccessorySettings& s)
{
    check(s);
    const Shape shape = shapeOf(s);
    const MeshSolid solid = meshkernel::levelSet([&](const Vec3& p) { return -shape.distance(p); }, shape.box,
                                                 shape.edge, 0.0);
    if (meshkernel::isEmpty(solid))
        throw std::invalid_argument("The accessory came out empty: make it bigger.");
    Mesh m;
    meshkernel::triangles(solid, m.positions, m.indices);
    // Lying ready to print: centred on X and Y, its lowest point on the plate.
    Bounds b;
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
        b.expand(m.positions[i], m.positions[i + 1], m.positions[i + 2]);
    const auto c = b.center();
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
        m.positions[i] -= c[0];
        m.positions[i + 1] -= c[1];
        m.positions[i + 2] -= b.min[2];
    }
    return m;
}

std::string nameOf(const AccessorySettings& s)
{
    switch (s.kind) {
    case AccessoryKind::Blade: return s.bladeLength < 400.0 ? "Dagger" : "Sword";
    case AccessoryKind::Shield:
        return s.shield == ShieldShape::Round ? "Round shield" : s.shield == ShieldShape::Heater ? "Heater shield"
                                                                                                 : "Kite shield";
    case AccessoryKind::Staff:
        return s.staffEnd == StaffEnd::Spear ? "Spear" : s.staffEnd == StaffEnd::Orb ? "Staff with orb" : "Staff";
    case AccessoryKind::Belt: return "Belt";
    case AccessoryKind::Holster: return "Holster";
    }
    return "Accessory";
}

ObjectId add(CommandStack& stack, Document& doc, const AccessorySettings& s, double x, double y)
{
    Mesh m = make(s);
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
        m.positions[i] += float(x);
        m.positions[i + 1] += float(y);
    }
    Body b = hinge::pieceBody(m, nameOf(s));
    b.id = doc.allocateId();
    b.name = doc.uniqueName(nameOf(s));  // "Sword 2" if there's a sword already
    const ObjectId id = b.id;
    stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
    return id;
}

}  // namespace accessory

}  // namespace forge
