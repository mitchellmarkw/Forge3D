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

namespace {

// The other two axes, turning the right way round `axis` (so u × v = axis).
int uOf(int axis) { return (axis + 1) % 3; }
int vOf(int axis) { return (axis + 2) % 3; }

Vec3 along(int axis, double s)
{
    Vec3 d{0, 0, 0};
    d[std::size_t(axis)] = s;
    return d;
}

// Triangles filed on a grid square to `axis`, to find quickly whether a straight line
// along it meets the part.
class Shadow {
public:
    Shadow(const Mesh& m, int axis) : m_mesh(m), m_axis(axis), m_u(uOf(axis)), m_v(vOf(axis))
    {
        const Bounds b = m.bounds();
        m_u0 = b.min[std::size_t(m_u)];
        m_v0 = b.min[std::size_t(m_v)];
        const double su = std::max(1e-6, double(b.size()[std::size_t(m_u)]));
        const double sv = std::max(1e-6, double(b.size()[std::size_t(m_v)]));
        const double n = std::max<double>(1.0, double(m.triangleCount()));
        m_cell = std::max({std::sqrt(su * sv / n) * 1.5, std::max(su, sv) / 400.0, 1e-4});
        m_nu = std::max(1, int(std::ceil(su / m_cell)) + 1);
        m_nv = std::max(1, int(std::ceil(sv / m_cell)) + 1);
        m_cells.assign(std::size_t(m_nu) * std::size_t(m_nv), {});
        for (std::size_t t = 0; t < m.triangleCount(); ++t) {
            double lu = 1e300, hu = -1e300, lv = 1e300, hv = -1e300;
            for (int k = 0; k < 3; ++k) {
                const Vec3 p = vertex(m, m.indices[3 * t + std::size_t(k)]);
                lu = std::min(lu, p[std::size_t(m_u)]);
                hu = std::max(hu, p[std::size_t(m_u)]);
                lv = std::min(lv, p[std::size_t(m_v)]);
                hv = std::max(hv, p[std::size_t(m_v)]);
            }
            for (int j = cellV(lv); j <= cellV(hv); ++j)
                for (int i = cellU(lu); i <= cellU(hu); ++i)
                    m_cells[std::size_t(j) * std::size_t(m_nu) + std::size_t(i)].push_back(std::uint32_t(t));
        }
    }

    // Whether the line from p along the axis (dir +1) or against it (-1) meets a triangle
    // other than `skip`.
    bool blocked(const Vec3& p, int dir, std::uint32_t skip) const
    {
        const std::size_t u = std::size_t(m_u), v = std::size_t(m_v), w = std::size_t(m_axis);
        const int i = cellU(p[u]), j = cellV(p[v]);
        for (std::uint32_t t : m_cells[std::size_t(j) * std::size_t(m_nu) + std::size_t(i)]) {
            if (t == skip)
                continue;
            const Vec3 a = vertex(m_mesh, m_mesh.indices[3 * t]), b = vertex(m_mesh, m_mesh.indices[3 * t + 1]),
                       c = vertex(m_mesh, m_mesh.indices[3 * t + 2]);
            const double d = (b[u] - a[u]) * (c[v] - a[v]) - (c[u] - a[u]) * (b[v] - a[v]);
            if (std::abs(d) < 1e-14)
                continue;
            const double s = ((p[u] - a[u]) * (c[v] - a[v]) - (c[u] - a[u]) * (p[v] - a[v])) / d;
            const double r = ((b[u] - a[u]) * (p[v] - a[v]) - (p[u] - a[u]) * (b[v] - a[v])) / d;
            if (s < 0.0 || r < 0.0 || s + r > 1.0)
                continue;
            const double at = a[w] + s * (b[w] - a[w]) + r * (c[w] - a[w]);
            if (dir * (at - p[w]) > 1e-3)
                return true;
        }
        return false;
    }

private:
    const Mesh& m_mesh;
    int m_axis, m_u, m_v;
    double m_u0 = 0, m_v0 = 0, m_cell = 1;
    int m_nu = 1, m_nv = 1;
    std::vector<std::vector<std::uint32_t>> m_cells;
    int cellU(double x) const { return std::clamp(int((x - m_u0) / m_cell), 0, m_nu - 1); }
    int cellV(double x) const { return std::clamp(int((x - m_v0) / m_cell), 0, m_nv - 1); }
};

struct Face {
    Vec3 centre, normal;
    double area;
};

Face faceOf(const Mesh& m, std::size_t t)
{
    const Vec3 a = vertex(m, m.indices[3 * t]), b = vertex(m, m.indices[3 * t + 1]), c = vertex(m, m.indices[3 * t + 2]);
    const Vec3 n = cross(sub(b, a), sub(c, a));
    const double l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    return {mul(add(add(a, b), c), 1.0 / 3.0), l > 1e-14 ? mul(n, 1.0 / l) : Vec3{0, 0, 0}, l / 2.0};
}

// Whether a piece pulled off along `axis` (dir ±1) comes away from this triangle freely.
bool freeAlong(const Shadow& shadow, const Face& f, int axis, int dir, std::uint32_t t)
{
    if (f.normal[std::size_t(axis)] * dir < -0.05)
        return false;  // facing away: the piece is behind it
    return !shadow.blocked(add(f.centre, mul(f.normal, 0.02)), dir, t);
}

int sideOf(double x, double at) { return x >= at ? 1 : -1; }

// Whether this half of the first cut (side ±1) is cut again by the second.
bool splitHalf(const MouldFinish& f, int side) { return f.second && (f.secondIn == 0 || f.secondIn == side); }

double undercutArea(const Mesh& part, int axis, double parting, const Shadow& shadow)
{
    double area = 0.0;
    for (std::size_t t = 0; t < part.triangleCount(); ++t) {
        const Face f = faceOf(part, t);
        if (!freeAlong(shadow, f, axis, sideOf(f.centre[std::size_t(axis)], parting), std::uint32_t(t)))
            area += f.area;
    }
    return area;
}

}  // namespace

void check(const MouldFinish& f)
{
    if (!std::isfinite(finite(f.parting)) || !std::isfinite(finite(f.parting2)))
        throw std::invalid_argument("The mould's cut isn't at a number.");
    if (f.axis < 0 || f.axis > 2 || f.axis2 < 0 || f.axis2 > 2)
        throw std::invalid_argument("The mould's cut goes an unknown way.");
    if (f.second && f.axis2 == f.axis)
        throw std::invalid_argument("The second cut must go another way from the first.");
    if (f.secondIn < -1 || f.secondIn > 1 || f.side2 < -1 || f.side2 > 1)
        throw std::invalid_argument("The mould's pieces are mixed up.");
    if (f.second ? (splitHalf(f, f.top ? 1 : -1) != (f.side2 != 0)) : f.side2 != 0)
        throw std::invalid_argument("The mould's pieces are mixed up.");
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

std::string sideName(int axis, bool positive)
{
    switch (axis) {
    case 0: return positive ? "right" : "left";
    case 1: return positive ? "back" : "front";
    default: return positive ? "top" : "bottom";
    }
}

std::string pieceName(const MouldFinish& f)
{
    std::string s = sideName(f.axis, f.top);
    if (f.side2 != 0)
        s += ", " + sideName(f.axis2, f.side2 > 0);
    return s;
}

std::vector<MouldFinish> pieces(const MouldFinish& f)
{
    std::vector<MouldFinish> out;
    for (bool top : {true, false}) {
        MouldFinish p = f;
        p.top = top;
        if (splitHalf(f, top ? 1 : -1)) {
            for (int s : {1, -1}) {
                p.side2 = s;
                out.push_back(p);
            }
        } else {
            p.side2 = 0;
            out.push_back(p);
        }
    }
    return out;
}

bool sameMould(const MouldFinish& a, const MouldFinish& b)
{
    MouldFinish x = a, y = b;
    x.top = y.top = false;
    x.side2 = y.side2 = 0;
    return x == y;
}

std::string describe(const MouldFinish& f)
{
    std::string s = pieceName(f) + " piece of " + std::to_string(pieces(f).size());
    s += ", " + number(f.wall) + " mm walls";
    if (f.keys)
        s += ", keyed";
    return s;
}

double sectionArea(const Mesh& part, double level, int axis)
{
    // Each triangle the cut crosses gives a piece of the outline, turned so the outside
    // is on its right (seen from the way `axis` points); the pieces' signed areas add up to
    // the outline's.
    const std::size_t u = std::size_t(uOf(axis)), v = std::size_t(vOf(axis)), w = std::size_t(axis);
    double twice = 0.0;
    for (std::size_t t = 0; t + 2 < part.indices.size(); t += 3) {
        const Vec3 p[3] = {vertex(part, part.indices[t]), vertex(part, part.indices[t + 1]),
                           vertex(part, part.indices[t + 2])};
        double d[3];
        for (int k = 0; k < 3; ++k)
            d[k] = p[k][w] - level == 0.0 ? 1e-12 : p[k][w] - level;
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
        // The outline runs anticlockwise round the inside: along axis × n.
        const double tu = -n[v], tv = n[u];
        Vec3 a = ends[0], b = ends[1];
        if ((b[u] - a[u]) * tu + (b[v] - a[v]) * tv < 0)
            std::swap(a, b);
        twice += a[u] * b[v] - b[u] * a[v];
    }
    return std::max(0.0, twice / 2.0);
}

double widest(const Mesh& part, int axis)
{
    const Bounds b = part.bounds();
    const std::size_t w = std::size_t(axis);
    const double lo = b.min[w], hi = b.max[w], h = hi - lo;
    if (!(h > 0))
        return lo;
    constexpr int kLevels = 80;
    std::vector<std::pair<double, double>> levels;  // place, area
    double best = 0.0;
    for (int i = 0; i <= kLevels; ++i) {
        const double z = lo + h * (0.04 + 0.92 * i / kLevels);
        levels.push_back({z, sectionArea(part, z, axis)});
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

std::vector<Vec3> highSpots(const Mesh& part, double above, const Vec3& spout, double ventSize, int most)
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
            if (!std::isfinite(top[c]) || top[c] < above + ventSize)
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

Undercuts undercuts(const Mesh& part, const MouldFinish& f)
{
    Undercuts out;
    const Shadow first(part, f.axis);
    std::optional<Shadow> second;
    if (f.second)
        second.emplace(part, f.axis2);
    for (std::size_t t = 0; t < part.triangleCount(); ++t) {
        const Face face = faceOf(part, t);
        out.total += face.area;
        const int side = sideOf(face.centre[std::size_t(f.axis)], f.parting);
        bool free = freeAlong(first, face, f.axis, side, std::uint32_t(t));
        // A piece cut off by the second cut too can come away along that one instead.
        if (!free && splitHalf(f, side))
            free = freeAlong(*second, face, f.axis2, sideOf(face.centre[std::size_t(f.axis2)], f.parting2),
                             std::uint32_t(t));
        if (!free) {
            out.triangles.push_back(std::uint32_t(t));
            out.area += face.area;
        }
    }
    return out;
}

Mesh undercutMesh(const Mesh& part, const Undercuts& u)
{
    Mesh m;
    for (std::uint32_t t : u.triangles)
        for (int k = 0; k < 3; ++k) {
            put(m, vertex(part, part.indices[3 * std::size_t(t) + std::size_t(k)]));
            m.indices.push_back(std::uint32_t(m.indices.size()));
        }
    return m;
}

MouldFinish bestSplit(const Mesh& part, const MouldFinish& f)
{
    const Bounds b = part.bounds();
    struct Try {
        int axis;
        double at, undercut, outline;
    };
    std::vector<Try> tries;
    double least = 1e300;
    for (int axis : {2, 0, 1}) {
        const Shadow shadow(part, axis);
        const std::size_t w = std::size_t(axis);
        const double lo = b.min[w], hi = b.max[w];
        std::vector<double> places{widest(part, axis)};
        for (int i = 2; i <= 8; ++i)
            places.push_back(std::round((lo + (hi - lo) * i / 10.0) * 10.0) / 10.0);
        for (double at : places) {
            if (!(at > lo + 0.05 && at < hi - 0.05))
                continue;
            const Try t{axis, at, undercutArea(part, axis, at, shadow), sectionArea(part, at, axis)};
            least = std::min(least, t.undercut);
            tries.push_back(t);
        }
    }
    if (tries.empty())
        return f;
    // About as little undercut (within 1 mm² or 2% of the least): the widest outline.
    const Try* pick = nullptr;
    for (const Try& t : tries)
        if (t.undercut <= least + std::max(1.0, least * 0.02) && (!pick || t.outline > pick->outline))
            pick = &t;
    MouldFinish out = f;
    out.axis = pick->axis;
    out.parting = pick->at;
    if (out.second && out.axis2 == out.axis) {
        out.axis2 = out.axis == 2 ? 0 : 2;
        const std::size_t w2 = std::size_t(out.axis2);
        out.parting2 = std::round((b.min[w2] + b.max[w2]) / 2.0 * 10.0) / 10.0;
    }
    return out;
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

std::vector<Key> keyList(const Bounds& part, const MouldFinish& f)
{
    std::vector<Key> out;
    if (!f.keys || keyRadius(f) <= 0.0)
        return out;
    const double in = f.wall / 2.0;
    const auto edge = [&](int axis, int side) {
        const std::size_t k = std::size_t(axis);
        return side > 0 ? double(part.max[k]) + in : double(part.min[k]) - in;
    };
    // The first cut: one in each corner of its face.
    {
        const int u = uOf(f.axis), v = vOf(f.axis);
        for (int su : {-1, 1})
            for (int sv : {-1, 1}) {
                Key k;
                k.axis = f.axis;
                k.cut = 1;
                k.point[std::size_t(f.axis)] = f.parting;
                k.point[std::size_t(u)] = edge(u, su);
                k.point[std::size_t(v)] = edge(v, sv);
                if (f.second)
                    k.side = sideOf(k.point[std::size_t(f.axis2)], f.parting2);
                out.push_back(k);
            }
    }
    // The second cut: in each half it goes through, two at the far corners (away from
    // the first cut).
    if (f.second) {
        const int third = 3 - f.axis - f.axis2;
        for (int side : {1, -1}) {
            if (!splitHalf(f, side))
                continue;
            for (int st : {-1, 1}) {
                Key k;
                k.axis = f.axis2;
                k.cut = 2;
                k.side = side;
                k.point[std::size_t(f.axis2)] = f.parting2;
                k.point[std::size_t(f.axis)] = edge(f.axis, side);
                k.point[std::size_t(third)] = edge(third, st);
                out.push_back(k);
            }
        }
    }
    return out;
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
        if (f.axis != 2 || v[2] > f.parting)  // a level cut: vents only from the top half
            append(m, frustum({v[0], v[1], v[2] - std::min(0.5, rv / 4.0)}, {v[0], v[1], topZ}, rv, rv, 16));
    return m;
}

namespace {

Mesh keyShape(const Key& k, double r, double fit, bool hole)
{
    const Vec3 d = along(k.axis, 1.0);
    if (hole)
        return frustum(sub(k.point, d), add(k.point, mul(d, r + fit)), r + fit, 0.6 * r + fit);
    return frustum(sub(k.point, mul(d, 0.5)), add(k.point, mul(d, r)), r, 0.6 * r);
}

}  // namespace

Mesh keys(const Bounds& part, const MouldFinish& f)
{
    Mesh m;
    const double r = keyRadius(f);
    for (const Key& k : keyList(part, f))
        append(m, keyShape(k, r, f.keyFit, false));
    return m;
}

Mesh partingSheets(const Bounds& part, const MouldFinish& f)
{
    const Bounds outer = box(part, f.wall + 2.0);
    Mesh m;
    Bounds b = outer;
    const std::size_t a = std::size_t(f.axis);
    b.min[a] = float(f.parting - 0.05);
    b.max[a] = float(f.parting + 0.05);
    append(m, boxMesh(b));
    if (f.second) {
        for (int side : {1, -1}) {
            if (!splitHalf(f, side))
                continue;
            Bounds c = outer;
            const std::size_t a2 = std::size_t(f.axis2);
            c.min[a2] = float(f.parting2 - 0.05);
            c.max[a2] = float(f.parting2 + 0.05);
            if (side > 0)
                c.min[a] = float(f.parting);
            else
                c.max[a] = float(f.parting);
            append(m, boxMesh(c));
        }
    }
    return m;
}

MeshSolid apply(const MeshSolid& part, const MouldFinish& f)
{
    check(f);
    const Bounds pb = meshkernel::bounds(part);
    if (!pb.valid)
        throw std::invalid_argument("The part is empty.");
    const auto inside = [&](int axis, double at) {
        const std::size_t k = std::size_t(axis);
        return at > pb.min[k] + 0.01 && at < pb.max[k] - 0.01;
    };
    if (!inside(f.axis, f.parting))
        throw std::invalid_argument("The cut must go through the part (between " + number(pb.min[std::size_t(f.axis)]) +
                                    " and " + number(pb.max[std::size_t(f.axis)]) + " mm).");
    if (f.second && !inside(f.axis2, f.parting2))
        throw std::invalid_argument("The second cut must go through the part (between " +
                                    number(pb.min[std::size_t(f.axis2)]) + " and " +
                                    number(pb.max[std::size_t(f.axis2)]) + " mm).");
    if (f.axis == 2 && !(f.spout[2] > f.parting))
        throw std::invalid_argument("The spout must meet the part above the parting face.");
    const std::vector<MeshSolid> tools{part, solid(channels(pb, f))};
    MeshSolid piece = meshkernel::subtracted(solid(boxMesh(box(pb, f.wall))), tools);
    const int side = f.top ? 1 : -1;
    piece = meshkernel::trimmed(piece, along(f.axis, f.parting), along(f.axis, side));
    if (f.side2 != 0)
        piece = meshkernel::trimmed(piece, along(f.axis2, f.parting2), along(f.axis2, f.side2));
    // The keys on this piece's cut faces: pegs where it's on the low side, holes on the high.
    const double r = keyRadius(f);
    Mesh pegs, holes;
    for (const Key& k : keyList(pb, f)) {
        const bool mine = k.cut == 1 ? (f.side2 == 0 || k.side == 0 || k.side == f.side2)
                                     : (f.side2 != 0 && k.side == side);
        if (!mine)
            continue;
        const int mySide = k.cut == 1 ? side : f.side2;
        append(mySide < 0 ? pegs : holes, keyShape(k, r, f.keyFit, mySide > 0));
    }
    if (!holes.empty())
        piece = meshkernel::subtracted(piece, {solid(holes)});
    if (!pegs.empty())
        piece = meshkernel::merged({piece, solid(pegs)});
    return piece;
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
    MouldFinish settings = f;
    settings.top = true;
    settings.side2 = 0;
    if (settings.second && settings.secondIn != -1)
        settings.side2 = 1;
    mould::check(settings);
    // On the plate, the step's coordinates are the plate's. Every piece worked out first:
    // refuse before changing anything.
    std::vector<BooleanSpec> specs;
    std::vector<Document::Preview> shapes;
    double lowest = 0.0;
    for (const MouldFinish& piece : mould::pieces(settings)) {
        BooleanSpec spec;
        spec.op = BooleanOp::Mould;
        spec.operands = {id};
        spec.mould = piece;
        const Document::Preview shape = doc.previewResult(spec);
        if (shape.empty)
            throw std::invalid_argument("The " + mould::pieceName(piece) + " piece of the mould round '" + body->name +
                                        "' came out empty.");
        lowest = std::min(lowest, double(shape.bounds.min[2]));
        specs.push_back(std::move(spec));
        shapes.push_back(shape);
    }
    const std::string base = body->name;
    // All lifted together, if need be, so the lowest stands on the plate.
    const double lift = -lowest;
    std::vector<ObjectId> made;
    stack.beginMacro("Make a mould of " + base);
    try {
        std::vector<Body> results;
        for (std::size_t i = 0; i < specs.size(); ++i) {
            BooleanSpec spec = specs[i];
            // The last piece takes the shape itself; the others copies of it.
            if (i + 1 < specs.size()) {
                const std::vector<Body> copies = duplicateTree(doc, id);
                for (const Body& c : copies)
                    stack.execute(std::make_unique<AddBodyCommand>(c));
                spec.operands = {copies.back().id};
            }
            const auto c = shapes[i].bounds.center();
            spec.pivot = {c[0], c[1], c[2]};
            Body r;
            r.id = doc.allocateId();
            r.name = base + " mould (" + mould::pieceName(*spec.mould) + ")";
            r.boolean = std::move(spec);
            r.placement.z = lift;
            made.push_back(r.id);
            results.push_back(std::move(r));
        }
        for (Body& r : results)
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
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
    const MouldFinish old = *b.boolean->mould;
    if (f.axis != old.axis || f.second != old.second ||
        (old.second && (f.axis2 != old.axis2 || f.secondIn != old.secondIn)))
        throw std::invalid_argument("A mould's number of pieces and the way its cuts go can't be changed: undo it "
                                    "and make it again.");
    const auto settingsFor = [&f](const MouldFinish& piece) {
        MouldFinish out = f;
        out.top = piece.top;
        out.side2 = piece.side2;
        return out;
    };
    mould::check(settingsFor(old));
    // Thicker or thinner walls: an upright piece moved so its underside stays where it was
    // (on the plate).
    const auto keepBase = [&](Body& piece) {
        if (piece.placement.rotationX == 0.0 && piece.placement.rotationY == 0.0)
            piece.placement.z += f.wall - old.wall;
    };
    std::vector<Body> out;
    Body next = b;
    next.boolean->mould = settingsFor(old);
    keepBase(next);
    out.push_back(next);
    // The other pieces of the mould, if they're still on the plate with the same settings:
    // changed alike.
    for (const Body& other : doc.bodies()) {
        if (other.id == result || !doc.isTopLevel(other.id) || !other.isBoolean() ||
            other.boolean->op != BooleanOp::Mould || !other.boolean->mould ||
            !mould::sameMould(*other.boolean->mould, old))
            continue;
        const MouldFinish& theirs = *other.boolean->mould;
        if (theirs.top == old.top && theirs.side2 == old.side2)
            continue;  // another mould just like it
        if (std::any_of(out.begin(), out.end(), [&](const Body& o) {
                return o.boolean->mould->top == theirs.top && o.boolean->mould->side2 == theirs.side2;
            }))
            continue;
        Body o = other;
        o.boolean->mould = settingsFor(theirs);
        keepBase(o);
        out.push_back(o);
    }
    return out;
}

}  // namespace forge
