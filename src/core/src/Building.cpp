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

}  // namespace

const char* title(WallMaterial m)
{
    switch (m) {
    case WallMaterial::Brick: return "Brick";
    case WallMaterial::Stone: return "Stone blocks";
    case WallMaterial::Logs: return "Logs";
    case WallMaterial::Planks: return "Wood planks";
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
    if ((s.material == WallMaterial::Brick || s.material == WallMaterial::Stone) && !(s.unitLength >= 20))
        throw std::invalid_argument("The bricks or blocks must be at least 20 mm long.");
    if (!(s.joint >= 0 && s.joint <= 100))
        throw std::invalid_argument("The joint must be from 0 to 100 mm.");
    if (!(s.pitch >= 5 && s.pitch <= 75))
        throw std::invalid_argument("The roof's pitch must be from 5 to 75 degrees.");
    if (!(s.overhang >= 0 && s.overhang <= 3000))
        throw std::invalid_argument("The roof's overhang must be from 0 to 3 m.");
    if (!(s.smallest >= 0.02 && s.smallest <= 2))
        throw std::invalid_argument("The smallest detail must be from 0.02 to 2 mm.");
    for (const auto& o : s.openings)
        if (o.wall < 0 || o.wall > 3 || !(o.width >= 100) || !(o.height >= 100))
            throw std::invalid_argument("A door or window is too small (under 100 mm) or on no wall.");
}

Face face(const BuildingSpec& s, int wall)
{
    const double k = 1.0 / s.scale, W = s.width * k, D = s.depth * k, H = s.height * k;
    Face f;
    f.height = H;
    switch (wall) {
    default:
    case 0: f.start = {-W / 2, -D / 2, 0}, f.along = {1, 0, 0}, f.out = {0, -1, 0}, f.length = W; break;
    case 1: f.start = {W / 2, -D / 2, 0}, f.along = {0, 1, 0}, f.out = {1, 0, 0}, f.length = D; break;
    case 2: f.start = {W / 2, D / 2, 0}, f.along = {-1, 0, 0}, f.out = {0, 1, 0}, f.length = W; break;
    case 3: f.start = {-W / 2, D / 2, 0}, f.along = {0, -1, 0}, f.out = {-1, 0, 0}, f.length = D; break;
    }
    return f;
}

std::optional<Spot> spotAt(const BuildingSpec& s, const std::array<double, 3>& origin,
                           const std::array<double, 3>& direction)
{
    std::optional<Spot> best;
    double bestT = 1e300;
    for (int w = 0; w < 4; ++w) {
        const Face f = face(s, w);
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
    const double length = (o.wall % 2 == 0) ? s.width : s.depth;
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

    // The four walls' plain shells: front and back the full width, the sides between.
    const auto shells = [&](double inset) {
        std::vector<MeshSolid> out;
        out.push_back(box({-W / 2 + inset, -D / 2 + inset, 0}, {W / 2 - inset, -D / 2 + T - inset, top}));
        out.push_back(box({-W / 2 + inset, D / 2 - T + inset, 0}, {W / 2 - inset, D / 2 - inset, top}));
        out.push_back(box({-W / 2 + inset, -D / 2 + inset, 0}, {-W / 2 + T - inset, D / 2 - inset, top}));
        out.push_back(box({W / 2 - T + inset, -D / 2 + inset, 0}, {W / 2 - inset, D / 2 - inset, top}));
        return out;
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
            for (int wall = 0; wall < 4; ++wall) {
                const Face f = face(s, wall);
                // Corners bonded: the front and back run to the corners on even courses,
                // the sides on odd ones; the other stops a joint short of them.
                const bool full = (wall % 2 == 0) == (course % 2 == 0);
                const double a0 = full ? 0.0 : T + jw, a1 = full ? f.length : f.length - T - jw;
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
            if (zf + d / 2 <= H + 0.1 * d) {
                any = true;
                for (double y : {-D / 2 + d / 2, D / 2 - d / 2})
                    parts.push_back(prism(circle, {-W / 2 - ext, y, zf}, {W + 2 * ext, 0, 0}, {0, 1, 0}, {0, 0, 1}));
                logTop = std::max(logTop, zf + d / 2);
            }
            if (zs + d / 2 <= H + 0.1 * d) {
                any = true;
                for (double x : {-W / 2 + d / 2, W / 2 - d / 2})
                    parts.push_back(prism(circle, {x, -D / 2 - ext, zs}, {0, D + 2 * ext, 0}, {0, 0, 1}, {1, 0, 0}));
                logTop = std::max(logTop, zs + d / 2);
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
            for (int wall = 0; wall < 4; ++wall) {
                const Face f = face(s, wall);
                const V3 along{f.along[0], f.along[1], f.along[2]}, out{f.out[0], f.out[1], f.out[2]};
                // Its edge on: thick at the bottom, thin at the top; run along the wall,
                // past its ends to meet the next wall's.
                const V3 axisWay = cross(out, V3{0, 0, 1});  // the run's way (out x up)
                const double sign = dot(axisWay, along) > 0 ? 1.0 : -1.0;
                const V3 a = add(add(V3{f.start[0], f.start[1], 0}, mul(along, sign > 0 ? -thick : f.length + thick)),
                                 V3{0, 0, 0});
                parts.push_back(prism({{0, z0}, {thick, z0}, {thin, z1}, {0, z1}}, a,
                                      mul(along, sign * (f.length + 2 * thick)), out, {0, 0, 1}));
            }
        }
        // Corner boards over the planks' ends.
        const double cb = std::max(100.0 * k, 2 * sm), ct = thick + std::max(5.0 * k, 0.5 * sm);
        const auto span = [](double a, double b) { return std::array<double, 2>{std::min(a, b), std::max(a, b)}; };
        for (int sx : {-1, 1})
            for (int sy : {-1, 1}) {
                const double x = sx * W / 2, y = sy * D / 2;
                // On the front or back face: in from the corner, and round it over the planks.
                auto bx = span(x - sx * cb, x + sx * ct), by = span(y, y + sy * ct);
                parts.push_back(box({bx[0], by[0], 0}, {bx[1], by[1], H}));
                bx = span(x, x + sx * ct), by = span(y - sy * cb, y + sy * ct);
                parts.push_back(box({bx[0], by[0], 0}, {bx[1], by[1], H}));
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
        const double X = W / 2 + o, Y = D / 2 + o;
        MeshSolid roof;
        if (s.roof == RoofKind::Flat) {
            roof = box({-X, -Y, top}, {X, Y, top + e});
        } else {
            // A gable along X (its slopes to the front and back), one along Y; a hip roof
            // is where both are.
            const auto gableX = [&] {
                return prism({{-Y, top}, {Y, top}, {Y, top + e}, {0, top + e + Y * t}, {-Y, top + e}}, {-X, 0, 0},
                             {2 * X, 0, 0}, {0, 1, 0}, {0, 0, 1});
            };
            const auto gableY = [&] {
                return prism({{-X, top}, {X, top}, {X, top + e}, {0, top + e + X * t}, {-X, top + e}}, {0, -Y, 0},
                             {0, 2 * Y, 0}, {-1, 0, 0}, {0, 0, 1});
            };
            if (s.roof == RoofKind::Gable)
                roof = W >= D ? gableX() : gableY();  // the ridge along the longer side
            else
                roof = meshkernel::intersected({gableX(), gableY()});
        }
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
    static const std::uint32_t colours[] = {0xB5563A, 0x9C978F, 0x8B5A2B, 0xC9B48C};
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
