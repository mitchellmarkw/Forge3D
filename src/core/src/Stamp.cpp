// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Stamp.h"

#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace forge {
namespace stamp {

namespace {

constexpr const char* kKeys[kKindCount] = {"rivet", "hexBolt", "screw", "stud", "vent", "scale", "stitch", "custom"};
constexpr const char* kTitles[kKindCount] = {"Rivet", "Hex bolt", "Screw", "Stud",
                                             "Vent",  "Scale",    "Stitch", "Custom shape"};
constexpr const char* kPlurals[kKindCount] = {"rivets", "hex bolts", "screws", "studs",
                                              "vents",  "scales",    "stitches", "stamps"};

constexpr double kPi = std::numbers::pi;

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a, const Vec3& fallback)
{
    const double l = std::sqrt(dot(a, a));
    return l > 1e-12 ? Vec3{a[0] / l, a[1] / l, a[2] / l} : fallback;
}

std::string num(double v)
{
    std::ostringstream s;
    s.precision(3);
    s << v;
    return s.str();
}

// Stretching (not a turn: only for meshes, never Transform::inverse).
Transform scaling(double sx, double sy, double sz)
{
    Transform t;
    t.m[0][0] = sx;
    t.m[1][1] = sy;
    t.m[2][2] = sz;
    return t;
}

// A closed triangle mesh as a solid, turned outward if it was built inside out.
MeshSolid solidOf(Mesh m)
{
    double volume = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const auto p = [&](std::size_t k) {
            const std::size_t i = m.indices[t + k];
            return Vec3{m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]};
        };
        volume += dot(p(0), cross(p(1), p(2)));
    }
    if (volume < 0)
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
            std::swap(m.indices[t + 1], m.indices[t + 2]);
    return meshkernel::fromTriangles(m.positions, m.indices);
}

void put(Mesh& m, double x, double y, double z)
{
    m.positions.push_back(float(x));
    m.positions.push_back(float(y));
    m.positions.push_back(float(z));
}

// A solid of turning: `profile` (radius, height) from a point on the axis at the bottom
// to one at the top.
MeshSolid revolve(const std::vector<std::pair<double, double>>& profile, int around = 32)
{
    Mesh m;
    const auto n = std::uint32_t(around);
    put(m, 0, 0, profile.front().second);
    for (std::size_t r = 1; r + 1 < profile.size(); ++r)
        for (int k = 0; k < around; ++k) {
            const double a = 2 * kPi * k / around;
            put(m, profile[r].first * std::cos(a), profile[r].first * std::sin(a), profile[r].second);
        }
    put(m, 0, 0, profile.back().second);
    const auto rings = std::uint32_t(profile.size() - 2);
    const std::uint32_t top = 1 + rings * n;
    for (std::uint32_t k = 0; k < n; ++k)
        m.indices.insert(m.indices.end(), {0, 1 + (k + 1) % n, 1 + k});
    for (std::uint32_t r = 0; r + 1 < rings; ++r)
        for (std::uint32_t k = 0; k < n; ++k) {
            const std::uint32_t a = 1 + r * n + k, b = 1 + r * n + (k + 1) % n, c = a + n, d = b + n;
            m.indices.insert(m.indices.end(), {a, b, c, b, d, c});
        }
    const std::uint32_t last = 1 + (rings - 1) * n;
    for (std::uint32_t k = 0; k < n; ++k)
        m.indices.insert(m.indices.end(), {top, last + k, last + (k + 1) % n});
    return solidOf(std::move(m));
}

// An outline (counter-clockwise, seen from above) stood up from z0 to z1.
MeshSolid prism(const std::vector<std::pair<double, double>>& outline, double z0, double z1)
{
    Mesh m;
    const auto n = std::uint32_t(outline.size());
    for (double z : {z0, z1})
        for (const auto& [x, y] : outline)
            put(m, x, y, z);
    double cx = 0, cy = 0;
    for (const auto& [x, y] : outline) {
        cx += x / n;
        cy += y / n;
    }
    put(m, cx, cy, z0);
    put(m, cx, cy, z1);
    const std::uint32_t c0 = 2 * n, c1 = c0 + 1;
    for (std::uint32_t k = 0; k < n; ++k) {
        const std::uint32_t a = k, b = (k + 1) % n, c = a + n, d = b + n;
        m.indices.insert(m.indices.end(), {a, b, c, b, d, c, c0, b, a, c1, c, d});
    }
    return solidOf(std::move(m));
}

std::vector<std::pair<double, double>> stadium(double length, double width, double y = 0.0)
{
    std::vector<std::pair<double, double>> out;
    const double r = width / 2, half = std::max(0.0, length / 2 - r);
    constexpr int kArc = 10;
    for (int k = 0; k <= kArc; ++k) {
        const double a = -kPi / 2 + kPi * k / kArc;
        out.push_back({half + r * std::cos(a), y + r * std::sin(a)});
    }
    for (int k = 0; k <= kArc; ++k) {
        const double a = kPi / 2 + kPi * k / kArc;
        out.push_back({-half + r * std::cos(a), y + r * std::sin(a)});
    }
    return out;
}

// A dome: radius r, `height` tall, on a stem down to -stem.
std::vector<std::pair<double, double>> dome(double r, double height, double stem)
{
    std::vector<std::pair<double, double>> p{{0, -stem}, {r, -stem}, {r, 0}};
    constexpr int kSteps = 10;
    for (int k = 1; k < kSteps; ++k) {
        const double a = kPi / 2 * k / kSteps;
        p.push_back({r * std::cos(a), height * std::sin(a)});
    }
    p.push_back({0, height});
    return p;
}

}  // namespace

const char* title(StampKind kind) { return kTitles[int(kind)]; }
const char* key(StampKind kind) { return kKeys[int(kind)]; }

std::optional<StampKind> fromKey(const std::string& k)
{
    for (int i = 0; i < kKindCount; ++i)
        if (k == kKeys[i])
            return StampKind(i);
    return std::nullopt;
}

double usualHeight(StampKind kind, double size)
{
    switch (kind) {
    case StampKind::Rivet: return 0.35 * size;
    case StampKind::HexBolt: return 0.4 * size;
    case StampKind::Screw: return 0.25 * size;
    case StampKind::Stud: return 0.5 * size;
    case StampKind::Vent: return 0.15 * size;
    case StampKind::Scale: return 0.15 * size;
    case StampKind::Stitch: return 0.1 * size;
    case StampKind::Custom: return 0.5 * size;
    }
    return 0.3 * size;
}

void check(const StampFinish& f)
{
    if (!(f.size >= 0.2 && f.size <= 100.0))
        throw std::invalid_argument("A stamp must be from 0.2 to 100 mm across.");
    if (!(f.height >= 0.05 && f.height <= 100.0))
        throw std::invalid_argument("A stamp must stand from 0.05 to 100 mm out (or go that far in).");
    if (!(f.spacing >= 0.1 && f.spacing <= 1000.0))
        throw std::invalid_argument("The spacing must be from 0.1 to 1000 mm.");
    if (!std::isfinite(f.spin))
        throw std::invalid_argument("The stamp's turn is broken.");
    if (f.spots.size() > 2000)
        throw std::invalid_argument("That's too many stamps in one step (2000 at most).");
    for (const StampSpot& s : f.spots)
        for (std::size_t k = 0; k < 3; ++k)
            if (!std::isfinite(s.point[k]) || !std::isfinite(s.normal[k]) || !std::isfinite(s.along[k]))
                throw std::invalid_argument("A stamp's spot is broken.");
}

std::string describe(const StampFinish& f, const std::string& customName)
{
    const std::size_t n = f.spots.size();
    std::string what;
    if (f.kind == StampKind::Custom)
        what = (customName.empty() ? std::string("stamp") : customName) + (n == 1 ? "" : " ×" + std::to_string(n));
    else {
        std::string one = title(f.kind);
        one[0] = char(std::tolower(static_cast<unsigned char>(one[0])));
        what = n == 1 ? one : std::string(kPlurals[int(f.kind)]);
        what = (n == 1 ? std::string("1 ") : std::to_string(n) + " ") + what;
    }
    return what + " \xC3\x98" + num(f.size) + (f.sunk ? ", sunk" : ", raised");
}

MeshSolid shape(const StampFinish& f, const MeshSolid* custom)
{
    const double S = f.size, H = f.height, R = S / 2;
    const double stem = std::max(0.4 * S, 0.5);
    MeshSolid up;
    switch (f.kind) {
    case StampKind::Rivet:
        up = revolve(dome(R, H, stem));
        break;
    case StampKind::Screw: {
        const MeshSolid head = revolve(dome(R, H, stem));
        const MeshSolid slot = prism({{-0.6 * S, -0.07 * S}, {0.6 * S, -0.07 * S}, {0.6 * S, 0.07 * S}, {-0.6 * S, 0.07 * S}},
                                     0.4 * H, H + 1.0);
        up = meshkernel::subtracted(head, {slot});
        break;
    }
    case StampKind::Stud: {
        const double c = std::min(0.1 * S, 0.3 * H);  // a little chamfer on top
        up = revolve({{0, -stem}, {R, -stem}, {R, H - c}, {R - c, H}, {0, H}});
        break;
    }
    case StampKind::HexBolt: {
        std::vector<std::pair<double, double>> hex;
        for (int k = 0; k < 6; ++k)
            hex.push_back({R * std::cos(kPi / 3 * k), R * std::sin(kPi / 3 * k)});
        up = prism(hex, -stem, H);
        break;
    }
    case StampKind::Vent: {
        const double w = 0.18 * S;
        std::vector<MeshSolid> slots;
        for (double y : {-0.3 * S, 0.0, 0.3 * S})
            slots.push_back(prism(stadium(S, w, y), -stem, H));
        up = meshkernel::together(slots);
        break;
    }
    case StampKind::Scale: {
        // A flattened dome, longer than wide.
        const MeshSolid round = revolve(dome(R, H, stem));
        up = meshkernel::transformed(round, scaling(1.0, 0.8, 1.0));
        break;
    }
    case StampKind::Stitch:
        up = prism(stadium(S, 0.22 * S), -stem, H);
        break;
    case StampKind::Custom: {
        if (!custom || meshkernel::isEmpty(*custom))
            throw std::invalid_argument("The custom stamp's shape is missing.");
        const Bounds b = meshkernel::bounds(*custom);
        const double wide = std::max(double(b.max[0] - b.min[0]), double(b.max[1] - b.min[1]));
        const double tall = double(b.max[2] - b.min[2]);
        if (wide <= 1e-6 || tall <= 1e-6)
            throw std::invalid_argument("The custom stamp's shape is flat.");
        const auto c = b.center();
        // Fitted to the size and height, its middle on the origin, sitting a little into
        // the surface so it meets it.
        const Transform fit = Transform::translation({0, 0, -0.15 * H}) *
                              scaling(S / wide, S / wide, H / tall) *
                              Transform::translation({-c[0], -c[1], -double(b.min[2])});
        up = meshkernel::transformed(*custom, fit);
        break;
    }
    }
    if (!f.sunk)
        return up;
    // Sunk: the same shape going into the surface, its stem standing out of it.
    return meshkernel::mirrored(up, {0, 0, 0}, {0, 0, 1});
}

Transform frame(const StampSpot& spot, double spin)
{
    const Vec3 z = unit({spot.normal[0], spot.normal[1], spot.normal[2]}, {0, 0, 1});
    Vec3 x{spot.along[0], spot.along[1], spot.along[2]};
    x = sub(x, {z[0] * dot(x, z), z[1] * dot(x, z), z[2] * dot(x, z)});
    x = unit(x, unit(cross(z, std::abs(z[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}), {1, 0, 0}));
    const Vec3 y = cross(z, x);
    Transform t;
    for (int r = 0; r < 3; ++r) {
        t.m[r][0] = x[std::size_t(r)];
        t.m[r][1] = y[std::size_t(r)];
        t.m[r][2] = z[std::size_t(r)];
        t.m[r][3] = spot.point[std::size_t(r)];
    }
    return t * Transform::rotationXYZ(0, 0, spin);
}

MeshSolid placed(const StampFinish& f, const MeshSolid* custom)
{
    check(f);
    const MeshSolid one = shape(f, custom);
    std::vector<MeshSolid> all;
    all.reserve(f.spots.size());
    for (const StampSpot& s : f.spots)
        all.push_back(meshkernel::transformed(one, frame(s, f.spin)));
    return meshkernel::together(all);
}

MeshSolid apply(const MeshSolid& part, const StampFinish& f, const MeshSolid* custom)
{
    if (f.spots.empty()) {
        check(f);
        return part;
    }
    check(f);
    const MeshSolid one = shape(f, custom);
    std::vector<MeshSolid> all;
    all.reserve(f.spots.size() + 1);
    if (!f.sunk)
        all.push_back(part);
    for (const StampSpot& s : f.spots)
        all.push_back(meshkernel::transformed(one, frame(s, f.spin)));
    if (!f.sunk)
        return meshkernel::merged(all);
    return meshkernel::subtracted(part, all);
}

std::vector<Vec3> rowPoints(const Vec3& a, const Vec3& b, double spacing)
{
    const Vec3 d = sub(b, a);
    const double length = std::sqrt(dot(d, d));
    if (!(spacing > 0.0) || length < spacing * 0.5)
        return {a};
    const int gaps = std::max(1, int(std::lround(length / spacing)));
    std::vector<Vec3> out;
    for (int k = 0; k <= gaps; ++k) {
        const double t = double(k) / gaps;
        out.push_back({a[0] + d[0] * t, a[1] + d[1] * t, a[2] + d[2] * t});
    }
    return out;
}

}  // namespace stamp

namespace {

const Body& stampStep(const Document& doc, ObjectId result)
{
    const Body* b = doc.find(result);
    if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::Stamp || !b->boolean->stamp)
        throw std::invalid_argument("That isn't a Stamp step.");
    return *b;
}

Vec3 unitOr(const Vec3& a, const Vec3& fallback)
{
    const double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    return l > 1e-12 ? Vec3{a[0] / l, a[1] / l, a[2] / l} : fallback;
}

StampFinish moved(const StampFinish& f, const Transform& t)
{
    StampFinish out = f;
    for (StampSpot& s : out.spots) {
        const Vec3 p = t.apply(Vec3{s.point[0], s.point[1], s.point[2]});
        const Vec3 n = unitOr(t.applyDirection(Vec3{s.normal[0], s.normal[1], s.normal[2]}), {0, 0, 1});
        const Vec3 a = unitOr(t.applyDirection(Vec3{s.along[0], s.along[1], s.along[2]}), {1, 0, 0});
        s.point = {p[0], p[1], p[2]};
        s.normal = {n[0], n[1], n[2]};
        s.along = {a[0], a[1], a[2]};
    }
    return out;
}

}  // namespace

BooleanSpec stampSpec(const Document& doc, ObjectId target, const StampFinish& inPlate, ObjectId custom)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    stamp::check(inPlate);
    if ((inPlate.kind == StampKind::Custom) != (custom != kInvalidId))
        throw std::invalid_argument("A custom stamp needs its shape (and only a custom one has one).");
    BooleanSpec spec;
    spec.op = BooleanOp::Stamp;
    spec.operands = {target};
    if (custom != kInvalidId)
        spec.operands.push_back(custom);
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.stamp = moved(inPlate, toStep);
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

StampFinish stampInPlate(const Document& doc, ObjectId result)
{
    const Body& body = stampStep(doc, result);
    return moved(*body.boolean->stamp, doc.worldTransform(result));
}

Body withStamp(const Document& doc, ObjectId result, const StampFinish& inPlate)
{
    const Body& body = stampStep(doc, result);
    stamp::check(inPlate);
    if (inPlate.kind == StampKind::Custom && body.boolean->operands.size() < 2)
        throw std::invalid_argument("A custom stamp needs its shape.");
    Body next = body;
    next.boolean->stamp = moved(inPlate, doc.worldTransform(result).inverse());
    return next;
}

}  // namespace forge
