// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Mould.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace forge {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    return l > 1e-12 ? mul(a, 1.0 / l) : Vec3{0, 0, 1};
}

Vec3 vertex(const Mesh& m, std::uint32_t i)
{
    return {m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]};
}

void put(Mesh& m, const Vec3& p)
{
    for (std::size_t k = 0; k < 3; ++k)
        m.positions.push_back(float(p[k]));
}

void append(Mesh& to, const Mesh& from)
{
    const auto first = std::uint32_t(to.positions.size() / 3);
    to.positions.insert(to.positions.end(), from.positions.begin(), from.positions.end());
    for (std::uint32_t i : from.indices)
        to.indices.push_back(first + i);
}

// A closed round rod from `a` (radius ra) to `b` (radius rb), facing out.
Mesh frustum(const Vec3& a, const Vec3& b, double ra, double rb, int sides = 32)
{
    const Vec3 d = unit(sub(b, a));
    const Vec3 u = unit(cross(d, std::abs(d[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}));
    const Vec3 v = cross(d, u);  // u × v = d
    Mesh m;
    for (int end = 0; end < 2; ++end)
        for (int k = 0; k < sides; ++k) {
            const double t = 2.0 * std::numbers::pi * k / sides;
            const double r = end == 0 ? ra : rb;
            put(m, add(end == 0 ? a : b, add(mul(u, r * std::cos(t)), mul(v, r * std::sin(t)))));
        }
    put(m, a);
    put(m, b);
    const auto n = std::uint32_t(sides);
    const std::uint32_t ca = 2 * n, cb = ca + 1;
    for (std::uint32_t k = 0; k < n; ++k) {
        const std::uint32_t a0 = k, a1 = (k + 1) % n, b0 = a0 + n, b1 = a1 + n;
        m.indices.insert(m.indices.end(), {a0, a1, b1, a0, b1, b0, ca, a1, a0, cb, b0, b1});
    }
    return m;
}

MeshSolid solid(const Mesh& m) { return meshkernel::fromTriangles(m.positions, m.indices); }

// Where a straight-down line at (x, y) first meets the part (its highest crossing).
std::optional<double> topAt(const Mesh& part, double x, double y)
{
    std::optional<double> best;
    for (std::size_t t = 0; t + 2 < part.indices.size(); t += 3) {
        const Vec3 a = vertex(part, part.indices[t]), b = vertex(part, part.indices[t + 1]),
                   c = vertex(part, part.indices[t + 2]);
        const double d = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
        if (std::abs(d) < 1e-12)
            continue;
        const double s = ((x - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (y - a[1])) / d;
        const double r = ((b[0] - a[0]) * (y - a[1]) - (x - a[0]) * (b[1] - a[1])) / d;
        if (s < -1e-9 || r < -1e-9 || s + r > 1.0 + 1e-9)
            continue;
        const double z = a[2] + s * (b[2] - a[2]) + r * (c[2] - a[2]);
        if (!best || z > *best)
            best = z;
    }
    return best;
}

double finite(double v) { return std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN(); }

std::string number(double v)
{
    std::ostringstream s;
    s.precision(3);
    s << v;
    return s.str();
}

}  // namespace

namespace mould {

void check(const MouldFinish& f)
{
    if (!std::isfinite(finite(f.parting)))
        throw std::invalid_argument("The mould's parting height isn't a number.");
    if (!(f.wall >= 2.0 && f.wall <= 50.0))
        throw std::invalid_argument("The mould's walls must be from 2 to 50 mm thick.");
    if (!(f.keySize >= 1.0 && f.keySize <= 30.0))
        throw std::invalid_argument("The keys must be from 1 to 30 mm across.");
    if (!(f.keyFit >= 0.0 && f.keyFit <= 1.0))
        throw std::invalid_argument("The keys' fit must be from 0 to 1 mm.");
    if (!(f.spoutSize >= 1.0 && f.spoutSize <= 30.0))
        throw std::invalid_argument("The spout must be from 1 to 30 mm across.");
    if (!(f.ventSize >= 0.5 && f.ventSize <= 10.0))
        throw std::invalid_argument("The air vents must be from 0.5 to 10 mm across.");
    if (f.vents.size() > 20)
        throw std::invalid_argument("A mould can have at most 20 air vents.");
    for (double v : f.spout)
        if (!std::isfinite(v))
            throw std::invalid_argument("The spout's place isn't a number.");
    for (const auto& p : f.vents)
        for (double v : p)
            if (!std::isfinite(v))
                throw std::invalid_argument("An air vent's place isn't a number.");
}

std::string describe(const MouldFinish& f)
{
    std::string s = f.top ? "top half" : "bottom half";
    s += ", " + number(f.wall) + " mm walls";
    if (f.top) {
        s += ", spout \xC3\x98" + number(f.spoutSize);
        if (!f.vents.empty())
            s += ", " + std::to_string(f.vents.size()) + (f.vents.size() == 1 ? " air vent" : " air vents");
    }
    if (f.keys)
        s += ", keyed";
    return s;
}

double sectionArea(const Mesh& part, double z)
{
    // Each triangle the level crosses gives a piece of the outline, turned so the outside
    // is on its right (seen from above); the pieces' signed areas add up to the outline's.
    double twice = 0.0;
    for (std::size_t t = 0; t + 2 < part.indices.size(); t += 3) {
        const Vec3 p[3] = {vertex(part, part.indices[t]), vertex(part, part.indices[t + 1]),
                           vertex(part, part.indices[t + 2])};
        double d[3];
        for (int k = 0; k < 3; ++k)
            d[k] = p[k][2] - z == 0.0 ? 1e-12 : p[k][2] - z;
        Vec3 ends[2];
        int found = 0;
        for (int k = 0; k < 3 && found < 2; ++k) {
            const int l = (k + 1) % 3;
            if ((d[k] > 0) == (d[l] > 0))
                continue;
            const double s = d[k] / (d[k] - d[l]);
            ends[found++] = add(p[k], mul(sub(p[l], p[k]), s));
        }
        if (found < 2)
            continue;
        const Vec3 n = cross(sub(p[1], p[0]), sub(p[2], p[0]));
        // The outline runs anticlockwise round the inside: along up × n.
        const Vec3 along{-n[1], n[0], 0.0};
        Vec3 a = ends[0], b = ends[1];
        if ((b[0] - a[0]) * along[0] + (b[1] - a[1]) * along[1] < 0)
            std::swap(a, b);
        twice += a[0] * b[1] - b[0] * a[1];
    }
    return std::max(0.0, twice / 2.0);
}

double widest(const Mesh& part)
{
    const Bounds b = part.bounds();
    const double lo = b.min[2], hi = b.max[2], h = hi - lo;
    if (!(h > 0))
        return lo;
    constexpr int kLevels = 80;
    std::vector<std::pair<double, double>> levels;  // height, area
    double best = 0.0;
    for (int i = 0; i <= kLevels; ++i) {
        const double z = lo + h * (0.04 + 0.92 * i / kLevels);
        levels.push_back({z, sectionArea(part, z)});
        best = std::max(best, levels.back().second);
    }
    // Of the widest levels (within 2%), the one nearest the middle.
    const double middle = lo + h / 2.0;
    double at = middle;
    bool any = false;
    for (const auto& [z, a] : levels)
        if (a >= best * 0.98 && (!any || std::abs(z - middle) < std::abs(at - middle))) {
            at = z;
            any = true;
        }
    return std::round(at * 10.0) / 10.0;
}

Vec3 highest(const Mesh& part)
{
    if (part.positions.empty())
        throw std::invalid_argument("The part has no surface.");
    double top = -std::numeric_limits<double>::infinity();
    std::size_t at = 0;
    for (std::size_t i = 0; i < part.vertexCount(); ++i)
        if (part.positions[3 * i + 2] > top) {
            top = part.positions[3 * i + 2];
            at = i;
        }
    // A flat top: the spout in the middle of it.
    double sx = 0, sy = 0;
    int count = 0;
    for (std::size_t i = 0; i < part.vertexCount(); ++i)
        if (part.positions[3 * i + 2] > top - 0.3) {
            sx += part.positions[3 * i];
            sy += part.positions[3 * i + 1];
            ++count;
        }
    if (count > 1) {
        const double x = sx / count, y = sy / count;
        if (const auto z = topAt(part, x, y); z && *z > top - 0.5)
            return {x, y, *z};
    }
    return vertex(part, std::uint32_t(at));
}

std::vector<Vec3> highSpots(const Mesh& part, double parting, const Vec3& spout, double ventSize, int most)
{
    const Bounds b = part.bounds();
    const double sx = b.size()[0], sy = b.size()[1];
    const double cell = std::max({ventSize * 1.5, std::max(sx, sy) / 80.0, 0.25});
    const int nx = std::max(1, int(std::ceil(sx / cell)) + 1), ny = std::max(1, int(std::ceil(sy / cell)) + 1);
    std::vector<double> top(std::size_t(nx) * std::size_t(ny), -std::numeric_limits<double>::infinity());
    std::vector<std::uint32_t> which(top.size(), 0);
    for (std::size_t i = 0; i < part.vertexCount(); ++i) {
        const int cx = std::clamp(int((part.positions[3 * i] - b.min[0]) / cell), 0, nx - 1);
        const int cy = std::clamp(int((part.positions[3 * i + 1] - b.min[1]) / cell), 0, ny - 1);
        const std::size_t c = std::size_t(cy) * std::size_t(nx) + std::size_t(cx);
        if (part.positions[3 * i + 2] > top[c]) {
            top[c] = part.positions[3 * i + 2];
            which[c] = std::uint32_t(i);
        }
    }
    const double gap = std::max(8.0, ventSize * 5.0);
    const int reach = std::max(1, int(std::ceil(gap / cell)));
    std::vector<Vec3> found;
    for (int y = 0; y < ny; ++y)
        for (int x = 0; x < nx; ++x) {
            const std::size_t c = std::size_t(y) * std::size_t(nx) + std::size_t(x);
            if (!std::isfinite(top[c]) || top[c] < parting + ventSize)
                continue;
            bool peak = true;
            for (int dy = -reach; dy <= reach && peak; ++dy)
                for (int dx = -reach; dx <= reach; ++dx) {
                    const int ox = x + dx, oy = y + dy;
                    if ((dx == 0 && dy == 0) || ox < 0 || oy < 0 || ox >= nx || oy >= ny || dx * dx + dy * dy > reach * reach)
                        continue;
                    const std::size_t o = std::size_t(oy) * std::size_t(nx) + std::size_t(ox);
                    // Level ties go to the first cell.
                    if (top[o] > top[c] || (top[o] == top[c] && o < c)) {
                        peak = false;
                        break;
                    }
                }
            if (!peak)
                continue;
            const Vec3 p = vertex(part, which[c]);
            if (std::hypot(p[0] - spout[0], p[1] - spout[1]) < gap)
                continue;
            found.push_back(p);
        }
    std::sort(found.begin(), found.end(), [](const Vec3& a, const Vec3& b) { return a[2] > b[2]; });
    std::vector<Vec3> out;
    for (const Vec3& p : found) {
        if (int(out.size()) >= most)
            break;
        bool apart = true;
        for (const Vec3& q : out)
            if (std::hypot(p[0] - q[0], p[1] - q[1]) < gap)
                apart = false;
        if (apart)
            out.push_back(p);
    }
    return out;
}

MouldFinish suggested(const Mesh& part)
{
    MouldFinish f;
    f.parting = widest(part);
    f.spout = highest(part);
    for (const Vec3& v : highSpots(part, f.parting, f.spout, f.ventSize))
        f.vents.push_back(v);
    return f;
}

Bounds box(const Bounds& part, double wall)
{
    Bounds b = part;
    for (std::size_t k = 0; k < 3; ++k) {
        b.min[k] = float(part.min[k] - wall);
        b.max[k] = float(part.max[k] + wall);
    }
    b.valid = true;
    return b;
}

double keyRadius(const MouldFinish& f)
{
    const double r = std::min(f.keySize / 2.0, 0.45 * f.wall - f.keyFit);
    return r >= 0.75 ? r : 0.0;
}

std::vector<Vec3> keyPoints(const Bounds& part, const MouldFinish& f)
{
    if (!f.keys || keyRadius(f) <= 0.0)
        return {};
    const double in = f.wall / 2.0;
    const double x0 = part.min[0] - in, x1 = part.max[0] + in, y0 = part.min[1] - in, y1 = part.max[1] + in;
    return {{x0, y0, f.parting}, {x1, y0, f.parting}, {x1, y1, f.parting}, {x0, y1, f.parting}};
}

Mesh boxMesh(const Bounds& b)
{
    Mesh m;
    for (int c = 0; c < 8; ++c)
        put(m, {(c & 1) ? b.max[0] : b.min[0], (c & 2) ? b.max[1] : b.min[1], (c & 4) ? b.max[2] : b.min[2]});
    const std::uint32_t faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
    for (const auto& q : faces)
        m.indices.insert(m.indices.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
    return m;
}

Mesh channels(const Bounds& part, const MouldFinish& f)
{
    const Bounds b = box(part, f.wall);
    const double topZ = double(b.max[2]) + 1.0;
    Mesh m;
    const double rs = f.spoutSize / 2.0;
    const Vec3 s{f.spout[0], f.spout[1], f.spout[2]};
    append(m, frustum({s[0], s[1], s[2] - std::min(0.5, rs / 4.0)}, {s[0], s[1], topZ}, rs, rs));
    // A funnel at the top to pour into.
    const double funnel = std::min(f.wall * 0.5, std::max(0.5, double(b.max[2]) - s[2] - 1.0));
    append(m, frustum({s[0], s[1], double(b.max[2]) - funnel}, {s[0], s[1], topZ}, rs, rs * (1.0 + (funnel + 1.0) / funnel)));
    const double rv = f.ventSize / 2.0;
    for (const auto& v : f.vents)
        if (v[2] > f.parting)
            append(m, frustum({v[0], v[1], v[2] - std::min(0.5, rv / 4.0)}, {v[0], v[1], topZ}, rv, rv, 16));
    return m;
}

Mesh keys(const Bounds& part, const MouldFinish& f, bool holes)
{
    Mesh m;
    const double r = keyRadius(f);
    for (const Vec3& p : keyPoints(part, f)) {
        if (holes)
            append(m, frustum({p[0], p[1], p[2] - 1.0}, {p[0], p[1], p[2] + r + f.keyFit}, r + f.keyFit,
                              0.6 * r + f.keyFit));
        else
            append(m, frustum({p[0], p[1], p[2] - 0.5}, {p[0], p[1], p[2] + r}, r, 0.6 * r));
    }
    return m;
}

Mesh partingSheet(const Bounds& part, const MouldFinish& f)
{
    Bounds b = box(part, f.wall + 2.0);
    b.min[2] = float(f.parting - 0.05);
    b.max[2] = float(f.parting + 0.05);
    return boxMesh(b);
}

MeshSolid apply(const MeshSolid& part, const MouldFinish& f)
{
    check(f);
    const Bounds pb = meshkernel::bounds(part);
    if (!pb.valid)
        throw std::invalid_argument("The part is empty.");
    if (!(f.parting > pb.min[2] + 0.01 && f.parting < pb.max[2] - 0.01))
        throw std::invalid_argument("The parting face must cross the part (between " + number(pb.min[2]) +
                                    " and " + number(pb.max[2]) + " mm high).");
    if (f.top && !(f.spout[2] > f.parting))
        throw std::invalid_argument("The spout must meet the part above the parting face.");
    std::vector<MeshSolid> tools{part};
    if (f.top)
        tools.push_back(solid(channels(pb, f)));
    MeshSolid block = meshkernel::subtracted(solid(boxMesh(box(pb, f.wall))), tools);
    MeshSolid half = meshkernel::trimmed(block, {0.0, 0.0, f.parting}, {0.0, 0.0, f.top ? 1.0 : -1.0});
    const Mesh k = keys(pb, f, f.top);
    if (!k.empty())
        half = f.top ? meshkernel::subtracted(half, {solid(k)}) : meshkernel::merged({half, solid(k)});
    return half;
}

}  // namespace mould

namespace {

const Body& mouldStep(const Document& doc, ObjectId result)
{
    const Body* b = doc.find(result);
    if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::Mould || !b->boolean->mould)
        throw std::invalid_argument("That isn't a mould.");
    return *b;
}

Mesh moved(Mesh m, const Transform& t)
{
    for (std::size_t i = 0; i < m.vertexCount(); ++i) {
        const Vec3 p = t.apply(vertex(m, std::uint32_t(i)));
        for (std::size_t k = 0; k < 3; ++k)
            m.positions[3 * i + k] = float(p[k]);
    }
    m.normals.clear();
    return m;
}

}  // namespace

Mesh mouldPart(const Document& doc, ObjectId id)
{
    const Body* b = doc.find(id);
    if (!b)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (b->isBoolean() && b->boolean->op == BooleanOp::Mould) {
        const ObjectId part = b->boolean->operands.front();
        return moved(*doc.mesh(part), doc.worldTransform(id).inverse() * doc.worldTransform(part));
    }
    return moved(*doc.mesh(id), doc.worldTransform(id));
}

const MouldFinish& mouldOf(const Document& doc, ObjectId result) { return *mouldStep(doc, result).boolean->mould; }

std::vector<ObjectId> makeMould(CommandStack& stack, Document& doc, ObjectId id, const MouldFinish& f)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!doc.isTopLevel(id))
        throw std::invalid_argument("Only a shape on the plate can be moulded.");
    mould::check(f);
    // On the plate, the step's coordinates are the plate's.
    const auto specFor = [&](bool top) {
        BooleanSpec spec;
        spec.op = BooleanOp::Mould;
        spec.operands = {id};
        MouldFinish half = f;
        half.top = top;
        spec.mould = half;
        return spec;
    };
    BooleanSpec topSpec = specFor(true), bottomSpec = specFor(false);
    // Both halves worked out first: refuse before changing anything.
    const Document::Preview topShape = doc.previewResult(topSpec), bottomShape = doc.previewResult(bottomSpec);
    if (topShape.empty || bottomShape.empty)
        throw std::invalid_argument("The mould round '" + body->name + "' came out empty.");
    const std::string base = body->name;
    // Both lifted together, if need be, so the bottom half stands on the plate.
    const double lift = std::max(0.0, -double(bottomShape.bounds.min[2]));
    const auto makeResult = [&](BooleanSpec spec, const Document::Preview& preview, const std::string& side) {
        const auto c = preview.bounds.center();
        spec.pivot = {c[0], c[1], c[2]};
        Body r;
        r.id = doc.allocateId();
        r.name = base + " mould (" + side + ")";
        r.boolean = std::move(spec);
        r.placement.z = lift;
        return r;
    };
    std::vector<ObjectId> made;
    stack.beginMacro("Make a mould of " + base);
    try {
        const std::vector<Body> copies = duplicateTree(doc, id);
        for (const Body& c : copies)
            stack.execute(std::make_unique<AddBodyCommand>(c));
        topSpec.operands = {copies.back().id};
        Body top = makeResult(topSpec, topShape, "top");
        Body bottom = makeResult(bottomSpec, bottomShape, "bottom");
        made = {top.id, bottom.id};
        stack.execute(std::make_unique<CombineCommand>(std::move(top)));
        stack.execute(std::make_unique<CombineCommand>(std::move(bottom)));
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

std::vector<Body> withMould(const Document& doc, ObjectId result, const MouldFinish& f)
{
    const Body& b = mouldStep(doc, result);
    mould::check(f);
    const MouldFinish old = *b.boolean->mould;
    // Thicker or thinner walls: an upright half moved so its underside stays where it was
    // (on the plate).
    const auto keepBase = [&](Body& half) {
        if (half.placement.rotationX == 0.0 && half.placement.rotationY == 0.0)
            half.placement.z += f.wall - old.wall;
    };
    std::vector<Body> out;
    Body next = b;
    MouldFinish mine = f;
    mine.top = old.top;
    next.boolean->mould = mine;
    keepBase(next);
    out.push_back(next);
    // Its other half, if it's still on the plate with the same settings: changed alike.
    MouldFinish pair = old;
    pair.top = !old.top;
    for (const Body& other : doc.bodies()) {
        if (other.id == result || !doc.isTopLevel(other.id) || !other.isBoolean() ||
            other.boolean->op != BooleanOp::Mould || !other.boolean->mould || !(*other.boolean->mould == pair))
            continue;
        Body o = other;
        MouldFinish theirs = f;
        theirs.top = pair.top;
        o.boolean->mould = theirs;
        keepBase(o);
        out.push_back(o);
        break;
    }
    return out;
}

}  // namespace forge
