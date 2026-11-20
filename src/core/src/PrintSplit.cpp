// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/PrintSplit.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Transform.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace forge {

namespace {

constexpr double kDeg = std::numbers::pi / 180.0;
constexpr double kWall = 1.2;        // mm of material kept round a hole
constexpr double kEmbed = 0.3;       // a peg starts this far inside its piece, to join well
constexpr double kHoleExtra = 0.5;   // a hole is this much deeper than its peg is long
constexpr double kFitMargin = 1.0;   // mm spare when fitting the printer

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double k) { return {a[0] * k, a[1] * k, a[2] * k}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& v)
{
    const double l = std::sqrt(dot(v, v));
    if (!(l > 1e-12))
        throw std::invalid_argument("The cutting plane needs a direction");
    return mul(v, 1.0 / l);
}

struct P2 {
    double x, y;
};

// Two directions along the plane.
void basis(const Vec3& n, Vec3& u, Vec3& v)
{
    const Vec3 helper = std::abs(n[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    u = unit(cross(helper, n));
    v = cross(n, u);
}

// The mesh in plate coordinates.
std::vector<Vec3> worldPoints(const Document& doc, ObjectId id, const Mesh& mesh)
{
    const Transform t = doc.worldTransform(id);
    std::vector<Vec3> out(mesh.vertexCount());
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = t.apply({mesh.positions[3 * i], mesh.positions[3 * i + 1], mesh.positions[3 * i + 2]});
    return out;
}

std::vector<std::vector<P2>> loops2d(const std::vector<std::vector<Vec3>>& loops, const Vec3& origin, const Vec3& u,
                                     const Vec3& v)
{
    std::vector<std::vector<P2>> out;
    for (const auto& loop : loops) {
        std::vector<P2> l;
        for (const Vec3& p : loop)
            l.push_back({dot(sub(p, origin), u), dot(sub(p, origin), v)});
        out.push_back(std::move(l));
    }
    return out;
}

double signedArea(const std::vector<P2>& l)
{
    double a = 0;
    for (std::size_t i = 0; i < l.size(); ++i) {
        const P2& p = l[i];
        const P2& q = l[(i + 1) % l.size()];
        a += p.x * q.y - q.x * p.y;
    }
    return a / 2;
}

bool insideLoop(const std::vector<P2>& l, P2 p)
{
    bool in = false;
    for (std::size_t i = 0, j = l.size() - 1; i < l.size(); j = i++) {
        if ((l[i].y > p.y) != (l[j].y > p.y) &&
            p.x < (l[j].x - l[i].x) * (p.y - l[i].y) / (l[j].y - l[i].y) + l[i].x)
            in = !in;
    }
    return in;
}

double distanceToLoops(const std::vector<std::vector<P2>>& loops, P2 p)
{
    double best = std::numeric_limits<double>::max();
    for (const auto& l : loops)
        for (std::size_t i = 0; i < l.size(); ++i) {
            const P2 a = l[i], b = l[(i + 1) % l.size()];
            const double dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
            const double t = len2 > 0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0, 1.0) : 0.0;
            best = std::min(best, std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy)));
        }
    return best;
}

// How far along `dir` from `from` before leaving the solid (the first triangle hit).
double depthAlong(const std::vector<Vec3>& pts, const Mesh& mesh, const Vec3& from, const Vec3& dir)
{
    double best = std::numeric_limits<double>::max();
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const Vec3& a = pts[mesh.indices[t]];
        const Vec3& b = pts[mesh.indices[t + 1]];
        const Vec3& c = pts[mesh.indices[t + 2]];
        const Vec3 e1 = sub(b, a), e2 = sub(c, a);
        const Vec3 h = cross(dir, e2);
        const double det = dot(e1, h);
        if (std::abs(det) < 1e-14)
            continue;
        const double f = 1.0 / det;
        const Vec3 s = sub(from, a);
        const double uu = f * dot(s, h);
        if (uu < 0.0 || uu > 1.0)
            continue;
        const Vec3 q = cross(s, e1);
        const double vv = f * dot(dir, q);
        if (vv < 0.0 || uu + vv > 1.0)
            continue;
        const double d = f * dot(e2, q);
        if (d > 1e-6)
            best = std::min(best, d);
    }
    return best;
}

std::vector<std::vector<Vec3>> sliceMesh(const std::vector<Vec3>& pts, const Mesh& mesh, const SplitPlane& plane)
{
    const Vec3 n = unit(plane.normal);
    std::vector<double> d(pts.size());
    for (std::size_t i = 0; i < pts.size(); ++i) {
        d[i] = dot(sub(pts[i], plane.point), n);
        if (d[i] == 0.0)
            d[i] = 1e-9;  // on the plane: count as just in front
    }
    // A crossing point, worked out the same way from whichever triangle shares the edge.
    const auto crossing = [&](std::uint32_t i, std::uint32_t j) {
        const Vec3& a = pts[i];
        const Vec3& b = pts[j];
        if (std::lexicographical_compare(b.begin(), b.end(), a.begin(), a.end()))
            std::swap(i, j);
        const double t = d[i] / (d[i] - d[j]);
        return add(pts[i], mul(sub(pts[j], pts[i]), t));
    };
    struct Seg {
        Vec3 a, b;
    };
    std::vector<Seg> segs;
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t idx[3] = {mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
        Vec3 found[2];
        int k = 0;
        for (int e = 0; e < 3 && k < 2; ++e) {
            const std::uint32_t i = idx[e], j = idx[(e + 1) % 3];
            if ((d[i] > 0) != (d[j] > 0))
                found[k++] = crossing(i, j);
        }
        if (k == 2)
            segs.push_back({found[0], found[1]});
    }
    // Join the pieces end to end.
    double size = 1.0;
    for (const Vec3& p : pts)
        size = std::max({size, std::abs(p[0]), std::abs(p[1]), std::abs(p[2])});
    const double q = size * 1e-7;
    using Key = std::array<long long, 3>;
    const auto key = [q](const Vec3& p) {
        return Key{std::llround(p[0] / q), std::llround(p[1] / q), std::llround(p[2] / q)};
    };
    std::map<Key, std::vector<std::size_t>> at;
    for (std::size_t s = 0; s < segs.size(); ++s) {
        at[key(segs[s].a)].push_back(s);
        at[key(segs[s].b)].push_back(s);
    }
    std::vector<bool> used(segs.size(), false);
    std::vector<std::vector<Vec3>> loops;
    for (std::size_t s0 = 0; s0 < segs.size(); ++s0) {
        if (used[s0])
            continue;
        used[s0] = true;
        std::vector<Vec3> loop{segs[s0].a};
        Vec3 here = segs[s0].b;
        const Key start = key(segs[s0].a);
        for (std::size_t guard = 0; guard < segs.size(); ++guard) {
            const Key k = key(here);
            if (k == start)
                break;
            loop.push_back(here);
            bool moved = false;
            for (std::size_t s : at[k]) {
                if (used[s])
                    continue;
                used[s] = true;
                here = key(segs[s].a) == k ? segs[s].b : segs[s].a;
                moved = true;
                break;
            }
            if (!moved)
                break;
        }
        if (loop.size() >= 3)
            loops.push_back(std::move(loop));
    }
    return loops;
}

// A cone along `axis` starting at `base` (radius r0 there, r1 at the far end).
Body coneBody(Document& doc, const std::string& name, const Vec3& base, const Vec3& axis, double r0, double r1,
              double height)
{
    ConeSpec cone(r0, std::max(r1, 0.05), height);
    Body b;
    b.id = doc.allocateId();
    b.name = name;
    b.hardware = true;  // a peg, its hole or a pin end: keeps its size when rescaled
    b.spec = cone;
    double tiltX = 0, tiltY = 0, offset = 0;
    tiltsOfPlane({base, axis}, base, tiltX, tiltY, offset);
    const Transform t = Transform::translation(base) * Transform::rotationXYZ(tiltX, tiltY, 0.0);
    b.placement = placementFromTransform(t, pivotOf(b.spec));
    return b;
}

ObjectId combine(CommandStack& stack, Document& doc, BooleanOp op, std::vector<ObjectId> operands,
                 const std::string& name, bool hardware = false)
{
    BooleanSpec spec;
    spec.op = op;
    spec.operands = std::move(operands);
    const Document::Preview preview = doc.previewResult(spec);
    if (preview.empty)
        throw std::runtime_error("Keying the cut left nothing of '" + name + "'");
    const auto c = preview.bounds.center();
    spec.pivot = {c[0], c[1], c[2]};
    Body r;
    r.id = doc.allocateId();
    r.name = name;
    r.hardware = hardware;
    r.boolean = std::move(spec);
    const ObjectId id = r.id;
    stack.execute(std::make_unique<CombineCommand>(std::move(r)));
    return id;
}

}  // namespace

std::vector<std::vector<Vec3>> sectionLoops(const Document& doc, ObjectId id, const SplitPlane& plane)
{
    const auto mesh = doc.mesh(id);
    if (!mesh || mesh->empty())
        return {};
    return sliceMesh(worldPoints(doc, id, *mesh), *mesh, plane);
}

double sectionArea(const Document& doc, ObjectId id, const SplitPlane& plane)
{
    Vec3 u, v;
    basis(unit(plane.normal), u, v);
    const auto loops = loops2d(sectionLoops(doc, id, plane), plane.point, u, v);
    // Even-odd: a loop inside an odd number of others is a hole.
    double area = 0;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        int depth = 0;
        for (std::size_t j = 0; j < loops.size(); ++j)
            if (j != i && insideLoop(loops[j], loops[i].front()))
                ++depth;
        area += (depth % 2 ? -1.0 : 1.0) * std::abs(signedArea(loops[i]));
    }
    return std::max(area, 0.0);
}

std::vector<KeySpot> keySpots(const Document& doc, ObjectId id, const SplitPlane& plane, const KeyOptions& options,
                              const std::vector<SplitPlane>& keepClear)
{
    std::vector<KeySpot> spots;
    if (!options.keys)
        return spots;
    const auto mesh = doc.mesh(id);
    if (!mesh || mesh->empty())
        return spots;
    const Vec3 n = unit(plane.normal);
    Vec3 u, v;
    basis(n, u, v);
    const std::vector<Vec3> pts = worldPoints(doc, id, *mesh);
    const auto loops = loops2d(sliceMesh(pts, *mesh, plane), plane.point, u, v);
    if (loops.empty())
        return spots;
    std::vector<double> areas;
    for (const auto& l : loops)
        areas.push_back(std::abs(signedArea(l)));
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    for (const auto& l : loops)
        for (const P2& p : l) {
            x0 = std::min(x0, p.x), x1 = std::max(x1, p.x);
            y0 = std::min(y0, p.y), y1 = std::max(y1, p.y);
        }
    const double span = std::max(x1 - x0, y1 - y0);
    const double step = std::max(span / 80.0, 0.25);

    // How deep the hole (and, for pins, the other hole) has to go.
    const double holeDepth = options.loosePins ? options.length / 2 + kHoleExtra : options.length + kHoleExtra;
    const auto roomBehind = [&](const Vec3& at) {
        // The holes go into the front piece (and with pins, the back one too).
        if (depthAlong(pts, *mesh, add(at, mul(n, 1e-4)), n) < holeDepth + kWall)
            return false;
        if (options.loosePins && depthAlong(pts, *mesh, sub(at, mul(n, 1e-4)), mul(n, -1.0)) < holeDepth + kWall)
            return false;
        return true;
    };
    const auto toWorld = [&](P2 p) { return add(plane.point, add(mul(u, p.x), mul(v, p.y))); };
    // Far enough from the cuts still to come, all along the hole (both holes, with pins).
    const auto clearOfCuts = [&](P2 p, double need) {
        const Vec3 at = toWorld(p);
        for (const SplitPlane& other : keepClear) {
            const Vec3 m = unit(other.normal);
            for (const double along : {holeDepth, 0.0, options.loosePins ? -holeDepth : 0.0}) {
                const double d = dot(sub(add(at, mul(n, along)), other.point), m);
                if (std::abs(d) < need)
                    return false;
            }
        }
        return true;
    };

    // Each separate piece of the cut (an outline with any holes inside it): its own pegs.
    struct Candidate {
        P2 p;
        double room;
        std::size_t region;
    };
    for (const double scale : {1.0, 0.75, 0.5}) {
        const double diameter = options.diameter * scale;
        if (diameter < 2.0)
            break;
        const double need = diameter / 2 + options.clearance + kWall;
        std::vector<Candidate> cands;
        for (double y = y0 + step / 2; y < y1; y += step)
            for (double x = x0 + step / 2; x < x1; x += step) {
                const P2 p{x, y};
                int depth = 0;
                std::size_t smallest = loops.size();
                for (std::size_t i = 0; i < loops.size(); ++i)
                    if (insideLoop(loops[i], p)) {
                        ++depth;
                        if (smallest == loops.size() || areas[i] < areas[smallest])
                            smallest = i;
                    }
                if (depth % 2 == 0)
                    continue;  // outside, or in a hole
                const double room = distanceToLoops(loops, p);
                if (room >= need && clearOfCuts(p, need))
                    cands.push_back({p, room, smallest});
            }
        if (cands.empty())
            continue;
        std::map<std::size_t, std::vector<Candidate>> byRegion;
        for (const Candidate& c : cands)
            byRegion[c.region].push_back(c);
        std::vector<KeySpot> found;
        for (auto& [region, list] : byRegion) {
            // The roomiest spot first, then the ones furthest from those already chosen.
            std::sort(list.begin(), list.end(), [](const Candidate& a, const Candidate& b) { return a.room > b.room; });
            std::vector<P2> chosen;
            const auto farthest = [&]() -> std::optional<Candidate> {
                std::optional<Candidate> best;
                double bestD = -1;
                for (const Candidate& c : list) {
                    double d = std::numeric_limits<double>::max();
                    for (const P2& q : chosen)
                        d = std::min(d, std::hypot(c.p.x - q.x, c.p.y - q.y));
                    if (d > bestD) {
                        bestD = d;
                        best = c;
                    }
                }
                if (!best || bestD < diameter * 2.5)
                    return std::nullopt;
                return best;
            };
            // The first: the roomiest with space for its hole behind it.
            for (const Candidate& c : list)
                if (roomBehind(toWorld(c.p))) {
                    chosen.push_back(c.p);
                    break;
                }
            if (chosen.empty())
                continue;
            // A second keeps the pieces from turning; a third on a big cut.
            const int wanted = areas[region] > 4000.0 ? 3 : 2;
            for (int k = 1; k < wanted; ++k) {
                for (int attempt = 0; attempt < 6; ++attempt) {
                    const auto c = farthest();
                    if (!c)
                        break;
                    if (roomBehind(toWorld(c->p))) {
                        chosen.push_back(c->p);
                        break;
                    }
                    // No room there: try elsewhere.
                    list.erase(std::remove_if(list.begin(), list.end(),
                                              [&](const Candidate& o) {
                                                  return std::hypot(o.p.x - c->p.x, o.p.y - c->p.y) < diameter;
                                              }),
                               list.end());
                }
            }
            for (const P2& p : chosen)
                found.push_back({toWorld(p), diameter});
        }
        if (!found.empty())
            return found;
    }
    return spots;
}

KeyedSplit splitWithKeys(CommandStack& stack, Document& doc, ObjectId id, const SplitPlane& plane,
                         const KeyOptions& options, const std::vector<SplitPlane>& keepClear)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    const std::string name = body->name;
    const Vec3 n = unit(plane.normal);
    const std::vector<KeySpot> spots = keySpots(doc, id, plane, options, keepClear);
    const Bounds whole = doc.worldBounds(id);

    KeyedSplit out;
    stack.beginMacro("Split " + name + " for printing");
    try {
        const std::vector<ObjectId> halves = splitBody(stack, doc, id, plane, SplitKeep::Both);
        ObjectId front = halves.at(0), back = halves.at(1);
        if (!spots.empty()) {
            const double tanT = std::tan(std::clamp(options.taper, 0.0, 15.0) * kDeg);
            std::vector<ObjectId> pegs, frontHoles, backHoles;
            int k = 0;
            for (const KeySpot& s : spots) {
                ++k;
                const double r = s.diameter / 2;
                const auto hole = [&](const Vec3& into, double depth) {
                    const double rc = r + options.clearance;
                    Body h = coneBody(doc, "Key hole " + std::to_string(k), sub(s.point, mul(into, 0.01)), into, rc,
                                      rc - depth * tanT, depth + 0.01);
                    const ObjectId hid = h.id;
                    stack.execute(std::make_unique<AddBodyCommand>(std::move(h)));
                    return hid;
                };
                if (options.loosePins) {
                    frontHoles.push_back(hole(n, options.length / 2 + kHoleExtra));
                    backHoles.push_back(hole(mul(n, -1.0), options.length / 2 + kHoleExtra));
                } else {
                    // The peg stands out of the back piece into the front one.
                    Body peg = coneBody(doc, "Peg " + std::to_string(k), sub(s.point, mul(n, kEmbed)), n, r,
                                        r - (options.length + kEmbed) * tanT, options.length + kEmbed);
                    pegs.push_back(peg.id);
                    stack.execute(std::make_unique<AddBodyCommand>(std::move(peg)));
                    frontHoles.push_back(hole(n, options.length + kHoleExtra));
                }
            }
            const std::string frontName = doc.find(front)->name, backName = doc.find(back)->name;
            std::vector<ObjectId> f{front};
            f.insert(f.end(), frontHoles.begin(), frontHoles.end());
            front = combine(stack, doc, BooleanOp::Subtract, f, frontName);
            if (!pegs.empty()) {
                std::vector<ObjectId> b{back};
                b.insert(b.end(), pegs.begin(), pegs.end());
                back = combine(stack, doc, BooleanOp::Merge, b, backName);
            } else if (!backHoles.empty()) {
                std::vector<ObjectId> b{back};
                b.insert(b.end(), backHoles.begin(), backHoles.end());
                back = combine(stack, doc, BooleanOp::Subtract, b, backName);
            }
            if (options.loosePins) {
                // The pins, standing beside the model: tapered both ends, to start in either hole.
                double x = whole.max[0] + 6.0;
                for (std::size_t i = 0; i < spots.size(); ++i) {
                    const double r = spots[i].diameter / 2;
                    const double half = options.length / 2;
                    const Vec3 at{x + r, whole.min[1] + r, 0.0};
                    Body lower = coneBody(doc, "Pin end", {at[0], at[1], half}, {0, 0, -1}, r, r - half * tanT, half);
                    Body upper = coneBody(doc, "Pin end", {at[0], at[1], half}, {0, 0, 1}, r, r - half * tanT, half);
                    const ObjectId a = lower.id, b = upper.id;
                    stack.execute(std::make_unique<AddBodyCommand>(std::move(lower)));
                    stack.execute(std::make_unique<AddBodyCommand>(std::move(upper)));
                    out.pins.push_back(combine(stack, doc, BooleanOp::Merge, {a, b}, "Pin " + std::to_string(i + 1), true));
                    x += 2 * r + 4.0;
                }
            }
            out.keys = int(spots.size());
        }
        out.pieces = {front, back};
    } catch (...) {
        stack.abortMacro();  // undo what was done of it
        throw;
    }
    stack.endMacro();
    return out;
}

namespace {

// Room along each axis for a piece, leaving space for a peg standing out of it.
std::array<double, 3> usable(const std::array<double, 3>& volume, const KeyOptions& options)
{
    const double peg = options.keys && !options.loosePins ? options.length : 0.0;
    std::array<double, 3> out{};
    for (int a = 0; a < 3; ++a)
        out[std::size_t(a)] = volume[std::size_t(a)] - kFitMargin - peg;
    return out;
}

bool fits(const Bounds& b, const std::array<double, 3>& volume)
{
    const auto s = b.size();
    // A piece can turn round on the plate: X and Y may swap.
    const bool flat = (s[0] <= volume[0] - kFitMargin + 1e-6 && s[1] <= volume[1] - kFitMargin + 1e-6) ||
                      (s[1] <= volume[0] - kFitMargin + 1e-6 && s[0] <= volume[1] - kFitMargin + 1e-6);
    return flat && s[2] <= volume[2] - kFitMargin + 1e-6;
}

}  // namespace

SmartPlan planSmartSplit(const Document& doc, ObjectId id, const std::array<double, 3>& volume,
                         const KeyOptions& options)
{
    // Counted from the box: each axis needs this many pieces.
    SmartPlan plan;
    const Bounds b = doc.worldBounds(id);
    if (fits(b, volume))
        return plan;
    const auto room = usable(volume, options);
    const auto s = b.size();
    plan.pieces = 1;
    // X and Y may swap when that needs fewer cuts.
    const auto count = [&](double extent, double limit) {
        return extent <= limit + kFitMargin - 1e-6 ? 1 : int(std::ceil(extent / limit - 1e-9));
    };
    const int z = count(s[2], room[2]);
    const int x1 = count(s[0], room[0]), y1 = count(s[1], room[1]);
    const int x2 = count(s[0], room[1]), y2 = count(s[1], room[0]);
    const bool swap = x2 * y2 < x1 * y1;
    const int x = swap ? x2 : x1, y = swap ? y2 : y1;
    plan.cutsAcross = {x - 1, y - 1, z - 1};
    plan.pieces = x * y * z;
    return plan;
}

KeyedSplit smartSplit(CommandStack& stack, Document& doc, ObjectId id, const std::array<double, 3>& volume,
                      const KeyOptions& options)
{
    if (!doc.find(id))
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!doc.isTopLevel(id))
        throw std::invalid_argument("Only a shape on the plate can be split");
    return splitAlong(stack, doc, id, planFitCuts(doc, id, volume, options, false).cuts, options);
}

// ---- Planned cuts ---------------------------------------------------------------------

SplitPlane planeOf(const FitCut& cut, const Bounds& box)
{
    if (cut.axis < 0 || cut.axis > 2 || cut.tiltAxis < 0 || cut.tiltAxis > 2 || cut.tiltAxis == cut.axis)
        throw std::invalid_argument("A cut needs an axis and a different axis to tilt around");
    const auto c = box.center();
    SplitPlane p{{c[0], c[1], c[2]}, {0, 0, 0}};
    p.point[std::size_t(cut.axis)] = cut.position;
    // The cut's axis turned `tilt` degrees around tiltAxis (right-handed).
    const double t = cut.tilt * kDeg;
    const int other = 3 - cut.axis - cut.tiltAxis;
    // Turning axis a around axis b moves it towards the third axis c = a x b's sign.
    Vec3 ea{0, 0, 0}, eb{0, 0, 0};
    ea[std::size_t(cut.axis)] = 1;
    eb[std::size_t(cut.tiltAxis)] = 1;
    const Vec3 towards = cross(eb, ea);  // where a turns to
    p.normal[std::size_t(cut.axis)] = std::cos(t);
    p.normal[std::size_t(other)] = std::sin(t) * towards[std::size_t(other)];
    return p;
}

namespace {

// Where a plane crosses `axis` over the model's box: the lowest and highest values along
// that axis at the box's four edges running that way.
std::pair<double, double> spanAlong(const SplitPlane& p, int axis, const Bounds& box)
{
    const int u = (axis + 1) % 3, w = (axis + 2) % 3;
    double lo = std::numeric_limits<double>::max(), hi = -lo;
    const double na = p.normal[std::size_t(axis)];
    for (int corner = 0; corner < 4; ++corner) {
        const double cu = (corner & 1) ? box.max[std::size_t(u)] : box.min[std::size_t(u)];
        const double cw = (corner & 2) ? box.max[std::size_t(w)] : box.min[std::size_t(w)];
        const double along = p.point[std::size_t(axis)] - (p.normal[std::size_t(u)] * (cu - p.point[std::size_t(u)]) +
                                                            p.normal[std::size_t(w)] * (cw - p.point[std::size_t(w)])) /
                                                               na;
        lo = std::min(lo, along);
        hi = std::max(hi, along);
    }
    return {lo, hi};
}

// Room for a piece along each axis (the printer, less the margin and a peg), with X and
// Y swapped when pieces lie turned.
std::array<double, 3> roomFor(const std::array<double, 3>& volume, const KeyOptions& options, bool turned)
{
    auto room = usable(volume, options);
    if (turned)
        std::swap(room[0], room[1]);
    return room;
}

}  // namespace

FitPlan planFitCuts(const Document& doc, ObjectId id, const std::array<double, 3>& volume, const KeyOptions& options,
                    bool angled)
{
    FitPlan plan;
    const SmartPlan counts = planSmartSplit(doc, id, volume, options);
    plan.pieces = counts.pieces;
    if (counts.pieces <= 1)
        return plan;
    const Bounds box = doc.worldBounds(id);
    const auto size = box.size();
    // Turned when the counts came from swapping X and Y.
    {
        const auto room = usable(volume, options);
        const auto need = [](double extent, double limit) {
            return extent <= limit + kFitMargin - 1e-6 ? 1 : int(std::ceil(extent / limit - 1e-9));
        };
        plan.turned = need(size[0], room[1]) * need(size[1], room[0]) < need(size[0], room[0]) * need(size[1], room[1]);
    }
    const auto room = roomFor(volume, options, plan.turned);

    // The model's surface once, for measuring many cuts quickly.
    const auto mesh = doc.mesh(id);
    const std::vector<Vec3> pts = worldPoints(doc, id, *mesh);
    const auto areaOf = [&](const SplitPlane& p) {
        Vec3 u, v;
        basis(unit(p.normal), u, v);
        const auto loops = loops2d(sliceMesh(pts, *mesh, p), p.point, u, v);
        double area = 0;
        for (std::size_t i = 0; i < loops.size(); ++i) {
            int depth = 0;
            for (std::size_t j = 0; j < loops.size(); ++j)
                if (j != i && insideLoop(loops[j], loops[i].front()))
                    ++depth;
            area += (depth % 2 ? -1.0 : 1.0) * std::abs(signedArea(loops[i]));
        }
        return std::max(area, 0.0);
    };

    for (int axis = 0; axis < 3; ++axis) {
        const int k = counts.cutsAcross[std::size_t(axis)];
        if (k <= 0)
            continue;
        const double limit = room[std::size_t(axis)];
        if (!(limit > 2.0))
            throw std::invalid_argument("The printer is too small for these pieces (and their pegs)");
        const double lo0 = box.min[std::size_t(axis)], hi0 = box.max[std::size_t(axis)];
        const double extent = hi0 - lo0;
        double before = lo0;  // where the piece before this cut starts (its lowest point)
        for (int i = 0; i < k; ++i) {
            const int left = k - i;  // slabs after this cut's piece
            const double ideal = before + (hi0 - before) / double(left + 1);
            const double from = std::max(hi0 - double(left) * limit, before + 1.0);
            const double to = std::min(before + limit, hi0 - 1.0);
            const auto cost = [&](const FitCut& c, double& area) {
                const SplitPlane p = planeOf(c, box);
                const auto [lo, hi] = spanAlong(p, axis, box);
                // Must leave this piece and the rest able to fit.
                if (hi - before > limit + 1e-6 || hi0 - lo > double(left) * limit + 1e-6 || lo < before + 1.0 ||
                    hi > hi0 - 1.0)
                    return std::numeric_limits<double>::max();
                area = areaOf(p);
                if (area <= 0.0)
                    return std::numeric_limits<double>::max();  // misses the model
                // Nearer the even spacing breaks ties; a lean has to earn its place.
                return area * (1.0 + 0.2 * std::abs(c.position - ideal) / std::max(limit, 1.0) +
                               0.05 * std::abs(c.tilt) / 30.0) +
                       0.01 * std::abs(c.position - ideal);
            };
            FitCut best{axis, std::clamp(ideal, std::min(from, to), std::max(from, to)), (axis + 1) % 3, 0.0};
            double bestCost = std::numeric_limits<double>::max(), area = 0.0;
            const int samples = to > from ? 14 : 1;
            for (int s = -1; s < samples; ++s) {  // the even spacing itself, then along the range
                FitCut c = best;
                c.position = s < 0 || samples == 1 ? best.position : from + (to - from) * s / (samples - 1);
                const double v = cost(c, area);
                if (v < bestCost) {
                    bestCost = v;
                    best = c;
                }
            }
            if (angled) {
                // Leaning cuts, near the best level place.
                const FitCut level = best;
                const double step = samples > 1 ? (to - from) / (samples - 1) : 0.0;
                for (int tiltAxis : {(axis + 1) % 3, (axis + 2) % 3})
                    for (double tilt : {-30.0, -20.0, -10.0, 10.0, 20.0, 30.0})
                        for (int shift = -2; shift <= 2; ++shift) {
                            FitCut c{axis, level.position + shift * step, tiltAxis, tilt};
                            const double v = cost(c, area);
                            if (v < bestCost) {
                                bestCost = v;
                                best = c;
                            }
                        }
            }
            plan.cuts.push_back(best);
            before = spanAlong(planeOf(best, box), axis, box).first;
            (void)extent;
        }
    }
    return plan;
}

std::vector<std::string> fitProblems(const Document& doc, ObjectId id, const FitPlan& plan,
                                     const std::array<double, 3>& volume, const KeyOptions& options)
{
    std::vector<std::string> problems;
    const Bounds box = doc.worldBounds(id);
    const auto room = roomFor(volume, options, plan.turned);
    const char* names[3] = {"X", "Y", "Z (height)"};
    const auto mm = [](double v) { return std::to_string(int(std::lround(v))) + " mm"; };
    for (int axis = 0; axis < 3; ++axis) {
        struct Span {
            double lo, hi;
            std::size_t index;
        };
        std::vector<Span> spans;
        for (std::size_t i = 0; i < plan.cuts.size(); ++i) {
            if (plan.cuts[i].axis != axis)
                continue;
            const auto [lo, hi] = spanAlong(planeOf(plan.cuts[i], box), axis, box);
            spans.push_back({lo, hi, i});
        }
        const double lo0 = box.min[std::size_t(axis)], hi0 = box.max[std::size_t(axis)];
        const double limit = room[std::size_t(axis)];
        if (spans.empty()) {
            if (hi0 - lo0 > limit + kFitMargin + 1e-6)
                problems.push_back(std::string("Too long along ") + names[axis] + ": " + mm(hi0 - lo0) +
                                   " with no cut across it (the printer takes " + mm(limit) + ")");
            continue;
        }
        std::sort(spans.begin(), spans.end(),
                  [](const Span& a, const Span& b) { return a.lo + a.hi < b.lo + b.hi; });
        for (std::size_t i = 0; i < spans.size(); ++i) {
            const std::string name = "Cut " + std::to_string(spans[i].index + 1);
            if (spans[i].hi <= lo0 || spans[i].lo >= hi0)
                problems.push_back(name + " misses the model");
            if (i > 0 && spans[i].lo < spans[i - 1].hi)
                problems.push_back(name + " and cut " + std::to_string(spans[i - 1].index + 1) +
                                   " cross inside the model");
        }
        for (std::size_t i = 0; i <= spans.size(); ++i) {
            const double from = i == 0 ? lo0 : spans[i - 1].lo;
            const double to = i == spans.size() ? hi0 : spans[i].hi;
            if (to - from > limit + 1e-6) {
                const std::string where = i == 0               ? "before cut " + std::to_string(spans[0].index + 1)
                                          : i == spans.size() ? "after cut " + std::to_string(spans[i - 1].index + 1)
                                                               : "between cuts " + std::to_string(spans[i - 1].index + 1) +
                                                                     " and " + std::to_string(spans[i].index + 1);
                problems.push_back("The piece " + where + " could be " + mm(to - from) + " along " + names[axis] +
                                   ": the printer takes " + mm(limit) + " (with room for the pegs)");
            }
        }
    }
    return problems;
}

KeyedSplit splitAlong(CommandStack& stack, Document& doc, ObjectId id, const std::vector<FitCut>& cuts,
                      const KeyOptions& options)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!doc.isTopLevel(id))
        throw std::invalid_argument("Only a shape on the plate can be split");
    KeyedSplit out;
    out.pieces = {id};
    if (cuts.empty())
        return out;
    const Bounds box = doc.worldBounds(id);
    std::vector<SplitPlane> planes;
    for (const FitCut& c : cuts)
        planes.push_back(planeOf(c, box));
    stack.beginMacro("Split " + body->name + " to fit the printer");
    try {
        for (std::size_t k = 0; k < planes.size(); ++k) {
            const SplitPlane& plane = planes[k];
            // Pegs keep clear of the other cuts (those still to come, and those made).
            std::vector<SplitPlane> others;
            for (std::size_t j = 0; j < planes.size(); ++j)
                if (j != k)
                    others.push_back(planes[j]);
            std::vector<ObjectId> next;
            for (ObjectId piece : out.pieces) {
                // A cut only splits the pieces it passes through.
                if (sectionArea(doc, piece, plane) <= 1e-6) {
                    next.push_back(piece);
                    continue;
                }
                const KeyedSplit made = splitWithKeys(stack, doc, piece, plane, options, others);
                out.pins.insert(out.pins.end(), made.pins.begin(), made.pins.end());
                out.keys += made.keys;
                next.insert(next.end(), made.pieces.begin(), made.pieces.end());
            }
            out.pieces = std::move(next);
        }
    } catch (...) {
        stack.abortMacro();  // undo what was done of it
        throw;
    }
    stack.endMacro();
    return out;
}

}  // namespace forge
