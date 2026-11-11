// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Transform.h"

#include <cmath>
#include <numbers>
#include <type_traits>
#include <variant>

namespace forge {

Transform Transform::translation(const Vec3& t)
{
    Transform r;
    r.m[0][3] = t[0];
    r.m[1][3] = t[1];
    r.m[2][3] = t[2];
    return r;
}

Transform Transform::rotationXYZ(double degX, double degY, double degZ)
{
    constexpr double toRad = std::numbers::pi / 180.0;
    const double cx = std::cos(degX * toRad), sx = std::sin(degX * toRad);
    const double cy = std::cos(degY * toRad), sy = std::sin(degY * toRad);
    const double cz = std::cos(degZ * toRad), sz = std::sin(degZ * toRad);

    Transform rx, ry, rz;
    rx.m = {{{1, 0, 0, 0}, {0, cx, -sx, 0}, {0, sx, cx, 0}}};
    ry.m = {{{cy, 0, sy, 0}, {0, 1, 0, 0}, {-sy, 0, cy, 0}}};
    rz.m = {{{cz, -sz, 0, 0}, {sz, cz, 0, 0}, {0, 0, 1, 0}}};
    return rz * ry * rx;
}

Vec3 Transform::apply(const Vec3& p) const
{
    Vec3 r{};
    for (int i = 0; i < 3; ++i)
        r[i] = m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3];
    return r;
}

Vec3 Transform::applyDirection(const Vec3& d) const
{
    Vec3 r{};
    for (int i = 0; i < 3; ++i)
        r[i] = m[i][0] * d[0] + m[i][1] * d[1] + m[i][2] * d[2];
    return r;
}

Transform Transform::inverse() const
{
    // For p' = R p + t:  p = R^T p' - R^T t
    Transform r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r.m[i][j] = m[j][i];
    for (int i = 0; i < 3; ++i)
        r.m[i][3] = -(r.m[i][0] * m[0][3] + r.m[i][1] * m[1][3] + r.m[i][2] * m[2][3]);
    return r;
}

Transform Transform::operator*(const Transform& rhs) const
{
    Transform r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            double v = m[i][0] * rhs.m[0][j] + m[i][1] * rhs.m[1][j] + m[i][2] * rhs.m[2][j];
            if (j == 3)
                v += m[i][3];
            r.m[i][j] = v;
        }
    }
    return r;
}

Vec3 pivotOf(const PrimitiveSpec& spec)
{
    return std::visit([](const auto& p) { return Vec3{0.0, 0.0, p.height / 2.0}; }, spec);
}

Transform placementTransform(const Placement& p, const PrimitiveSpec& spec)
{
    const Vec3 c = pivotOf(spec);
    return Transform::translation({p.x, p.y, p.z}) * Transform::translation(c) *
           Transform::rotationXYZ(p.rotationX, p.rotationY, p.rotationZ) *
           Transform::translation({-c[0], -c[1], -c[2]});
}

}  // namespace forge
