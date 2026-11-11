// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Transform.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
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

Vec3 eulerXYZ(const Transform& t)
{
    constexpr double toDeg = 180.0 / std::numbers::pi;
    const auto& m = t.m;
    // R = Rz * Ry * Rx  =>  m[2][0] = -sin(y)
    const double sy = std::clamp(-m[2][0], -1.0, 1.0);
    const double y = std::asin(sy);
    double x = 0.0, z = 0.0;
    if (std::abs(sy) < 1.0 - 1e-9) {
        x = std::atan2(m[2][1], m[2][2]);
        z = std::atan2(m[1][0], m[0][0]);
    } else {
        // Gimbal lock: only X + Z (or X - Z) is defined. Put it all in X.
        x = std::atan2(-m[1][2], m[1][1]);
        z = 0.0;
    }
    // Tidy values like -0.0 and 359.9999999 so the panel shows clean numbers.
    const auto tidy = [](double deg) {
        const double r = std::round(deg * 1e6) / 1e6;
        return r == 0.0 ? 0.0 : r;
    };
    return {tidy(x * toDeg), tidy(y * toDeg), tidy(z * toDeg)};
}

Placement rotatedAboutPlateAxis(const Body& body, int axis, double degrees)
{
    if (axis < 0 || axis > 2)
        throw std::invalid_argument("axis must be 0, 1 or 2");
    const Placement& p = body.placement;
    const Transform current = Transform::rotationXYZ(p.rotationX, p.rotationY, p.rotationZ);
    const Transform turn = Transform::rotationXYZ(axis == 0 ? degrees : 0.0, axis == 1 ? degrees : 0.0,
                                                  axis == 2 ? degrees : 0.0);
    const Vec3 angles = eulerXYZ(turn * current);  // turn about the fixed plate axis
    Placement result = p;                          // centre stays: position unchanged
    result.rotationX = angles[0];
    result.rotationY = angles[1];
    result.rotationZ = angles[2];
    return result;
}

double dimension(const PrimitiveSpec& spec, int axis)
{
    if (const auto* box = std::get_if<BoxSpec>(&spec))
        return axis == 0 ? box->width : axis == 1 ? box->depth : box->height;
    const auto& cyl = std::get<CylinderSpec>(spec);
    return axis == 2 ? cyl.height : cyl.radius;
}

namespace {

// The point (body coordinates) that must not move when a face is dragged.
Vec3 anchorFor(const PrimitiveSpec& spec, int axis, int side)
{
    if (axis == 2) {
        const double h = dimension(spec, 2);
        return side > 0 ? Vec3{0.0, 0.0, 0.0} : Vec3{0.0, 0.0, h};  // the other end
    }
    if (std::holds_alternative<CylinderSpec>(spec))
        return pivotOf(spec);  // radius grows evenly around the centre line
    const double half = dimension(spec, axis) / 2.0;
    const double h = dimension(spec, 2);
    Vec3 a{0.0, 0.0, h / 2.0};
    a[static_cast<std::size_t>(axis)] = -side * half;  // centre of the opposite face
    return a;
}

}  // namespace

Body resizedFace(const Body& body, int axis, int side, double newValue, double minValue)
{
    if (axis < 0 || axis > 2 || (side != 1 && side != -1))
        throw std::invalid_argument("resizedFace: axis must be 0..2 and side +1 or -1");

    Body result = body;
    const double value = std::max(newValue, minValue);
    if (auto* box = std::get_if<BoxSpec>(&result.spec)) {
        (axis == 0 ? box->width : axis == 1 ? box->depth : box->height) = value;
    } else {
        auto& cyl = std::get<CylinderSpec>(result.spec);
        (axis == 2 ? cyl.height : cyl.radius) = value;
    }

    // Keep the anchor fixed in the plate:
    //   world(q) = p + c + R (q - c)   for body point q, pivot c, position p.
    // Solve p' + c' + R (a' - c') = p + c + R (a - c).
    const Placement& pl = body.placement;
    const Transform R = Transform::rotationXYZ(pl.rotationX, pl.rotationY, pl.rotationZ);
    const Vec3 c = pivotOf(body.spec), c2 = pivotOf(result.spec);
    const Vec3 a = anchorFor(body.spec, axis, side), a2 = anchorFor(result.spec, axis, side);
    const Vec3 before = R.applyDirection({a[0] - c[0], a[1] - c[1], a[2] - c[2]});
    const Vec3 after = R.applyDirection({a2[0] - c2[0], a2[1] - c2[1], a2[2] - c2[2]});
    result.placement.x = pl.x + c[0] + before[0] - c2[0] - after[0];
    result.placement.y = pl.y + c[1] + before[1] - c2[1] - after[1];
    result.placement.z = pl.z + c[2] + before[2] - c2[2] - after[2];
    return result;
}

}  // namespace forge
