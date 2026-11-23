// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Balance.h"

#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>

namespace forge::balance {

namespace {

using P2 = std::array<double, 2>;

double crossZ(const P2& o, const P2& a, const P2& b)
{
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
}

// Round a set of points (seen from above), anticlockwise.
std::vector<P2> hull(std::vector<P2> pts)
{
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    if (pts.size() < 3)
        return pts;
    std::vector<P2> h(2 * pts.size());
    std::size_t k = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && crossZ(h[k - 2], h[k - 1], pts[i]) <= 0)
            --k;
        h[k++] = pts[i];
    }
    for (std::size_t i = pts.size() - 1, t = k + 1; i-- > 0;) {
        while (k >= t && crossZ(h[k - 2], h[k - 1], pts[i]) <= 0)
            --k;
        h[k++] = pts[i];
    }
    h.resize(k - 1);
    return h;
}

double area(const std::vector<P2>& poly)
{
    double a = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const P2& p = poly[i];
        const P2& q = poly[(i + 1) % poly.size()];
        a += p[0] * q[1] - q[0] * p[1];
    }
    return std::abs(a) / 2.0;
}

// From p to the segment a-b: the nearest point.
P2 nearestOn(const P2& p, const P2& a, const P2& b)
{
    const double dx = b[0] - a[0], dy = b[1] - a[1];
    const double l2 = dx * dx + dy * dy;
    const double t = l2 > 0 ? std::clamp(((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2, 0.0, 1.0) : 0.0;
    return {a[0] + t * dx, a[1] + t * dy};
}

}  // namespace

std::pair<double, Vec3> volumeAndCentre(const Mesh& m)
{
    // Tetrahedra from the origin to each triangle.
    double v = 0.0, cx = 0.0, cy = 0.0, cz = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &m.positions[3 * m.indices[t]];
        const float* b = &m.positions[3 * m.indices[t + 1]];
        const float* c = &m.positions[3 * m.indices[t + 2]];
        const double d = double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) -
                         double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
                         double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0]);
        v += d;
        cx += d * (double(a[0]) + b[0] + c[0]);
        cy += d * (double(a[1]) + b[1] + c[1]);
        cz += d * (double(a[2]) + b[2] + c[2]);
    }
    if (std::abs(v) < 1e-12)
        return {0.0, {0, 0, 0}};
    return {v / 6.0, {cx / (4.0 * v), cy / (4.0 * v), cz / (4.0 * v)}};
}

Balance of(const std::vector<pose::PosedPart>& parts, double touching)
{
    Balance out;
    double ground = std::numeric_limits<double>::infinity();
    std::vector<Mesh> posed(parts.size());
    double wx = 0, wy = 0, wz = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        Mesh& m = posed[i];
        meshkernel::triangles(parts[i].solid, m.positions, m.indices);
        for (std::size_t v = 0; v < m.vertexCount(); ++v) {
            const Vec3 p = parts[i].moved.apply({m.positions[3 * v], m.positions[3 * v + 1], m.positions[3 * v + 2]});
            for (std::size_t k = 0; k < 3; ++k)
                m.positions[3 * v + k] = float(p[k]);
            ground = std::min(ground, p[2]);
        }
        const auto [vol, c] = volumeAndCentre(m);
        if (vol <= 0.0)
            continue;
        out.weight += vol;
        wx += vol * c[0];
        wy += vol * c[1];
        wz += vol * c[2];
    }
    if (!(out.weight > 0.0) || !std::isfinite(ground))
        throw std::invalid_argument("There's nothing solid to stand up.");
    out.centre = {wx / out.weight, wy / out.weight, wz / out.weight};
    out.ground = ground;

    // Where it touches the ground: each patch of each part (a one-piece figure's two
    // feet are two), points less than about a millimetre apart counting as one patch.
    std::vector<P2> all;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        // Points on the ground, joined into patches along the surface's edges.
        const Mesh& m = posed[i];
        std::vector<int> link(m.vertexCount(), -1);  // union-find over the points on the ground
        const auto on = [&](std::uint32_t v) { return double(m.positions[3 * v + 2]) <= ground + touching; };
        std::function<int(int)> rootOf = [&](int v) {
            while (link[std::size_t(v)] != v) {
                link[std::size_t(v)] = link[std::size_t(link[std::size_t(v)])];
                v = link[std::size_t(v)];
            }
            return v;
        };
        bool any = false;
        for (std::uint32_t v = 0; v < m.vertexCount(); ++v)
            if (on(v)) {
                link[v] = int(v);
                any = true;
            }
        if (!any)
            continue;
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = m.indices[t + std::size_t(k)], b = m.indices[t + std::size_t((k + 1) % 3)];
                if (on(a) && on(b)) {
                    const int ra = rootOf(int(a)), rb = rootOf(int(b));
                    if (ra != rb)
                        link[std::size_t(ra)] = rb;
                }
            }
        std::map<int, std::vector<P2>> byRoot;
        for (std::uint32_t v = 0; v < m.vertexCount(); ++v)
            if (link[v] >= 0)
                byRoot[rootOf(int(v))].push_back({m.positions[3 * v], m.positions[3 * v + 1]});
        std::vector<std::vector<P2>> groups;
        for (auto& [root, list] : byRoot)
            groups.push_back(std::move(list));
        for (const auto& group : groups) {
            Foot f;
            f.part = parts[i].id;
            f.outline = hull(group);
            f.area = f.outline.size() >= 3 ? area(f.outline) : 0.0;
            // The middle of the outline (or of the points, for a line or a point).
            if (f.outline.size() >= 3 && f.area > 1e-9) {
                double a = 0, cx = 0, cy = 0;
                for (std::size_t k = 0; k < f.outline.size(); ++k) {
                    const P2& p = f.outline[k];
                    const P2& q = f.outline[(k + 1) % f.outline.size()];
                    const double c = p[0] * q[1] - q[0] * p[1];
                    a += c;
                    cx += (p[0] + q[0]) * c;
                    cy += (p[1] + q[1]) * c;
                }
                f.centre = {cx / (3.0 * a), cy / (3.0 * a), ground};
            } else {
                double sx = 0, sy = 0;
                for (const P2& p : group) {
                    sx += p[0];
                    sy += p[1];
                }
                f.centre = {sx / double(group.size()), sy / double(group.size()), ground};
            }
            all.insert(all.end(), f.outline.begin(), f.outline.end());
            out.feet.push_back(std::move(f));
        }
    }
    out.support = hull(all);

    // The centre's foot against the support's edge.
    const P2 c{out.centre[0], out.centre[1]};
    const auto& s = out.support;
    double nearest = std::numeric_limits<double>::infinity();
    P2 at = c;
    if (s.size() == 1) {
        at = s[0];
        nearest = std::hypot(c[0] - at[0], c[1] - at[1]);
    } else {
        for (std::size_t k = 0; k < s.size(); ++k) {
            const P2 q = nearestOn(c, s[k], s[(k + 1) % s.size()]);
            const double d = std::hypot(c[0] - q[0], c[1] - q[1]);
            if (d < nearest) {
                nearest = d;
                at = q;
            }
        }
    }
    bool inside = s.size() >= 3;
    for (std::size_t k = 0; inside && k < s.size(); ++k)
        if (crossZ(s[k], s[(k + 1) % s.size()], c) < 0.0)
            inside = false;
    out.margin = inside ? nearest : -nearest;
    if (nearest > 1e-9)
        out.toward = {(at[0] - c[0]) / nearest * (inside ? 1.0 : 1.0), (at[1] - c[1]) / nearest};
    const double height = std::max(out.centre[2] - ground, 1e-6);
    out.lean = inside ? std::atan2(nearest, height) * 180.0 / 3.14159265358979 : 0.0;
    return out;
}

std::vector<PegSpot> pegSpots(const std::vector<pose::PosedPart>& parts, const Balance& b)
{
    std::map<ObjectId, const pose::PosedPart*> byId;
    for (const auto& p : parts)
        byId[p.id] = &p;
    std::vector<PegSpot> out;
    for (const Foot& f : b.feet) {
        const auto found = byId.find(f.part);
        if (found == byId.end() || f.area < 1.0)
            continue;  // touching at a point or along a line: no room for a hole
        const Transform back = found->second->moved.inverse();
        PegSpot s;
        s.part = f.part;
        s.point = back.apply(f.centre);
        const Vec3 tip = back.apply({f.centre[0], f.centre[1], f.centre[2] - 1.0});
        s.normal = {tip[0] - s.point[0], tip[1] - s.point[1], tip[2] - s.point[2]};
        out.push_back(s);
    }
    return out;
}

}  // namespace forge::balance
