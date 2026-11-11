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

Vec3 halfExtents(const PrimitiveSpec& spec)
{
    return std::visit(
        [](const auto& p) -> Vec3 {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec> || std::is_same_v<T, WedgeSpec>)
                return {p.width / 2.0, p.depth / 2.0, p.height / 2.0};
            else if constexpr (std::is_same_v<T, CylinderSpec>)
                return {p.radiusX, p.radiusY, p.height / 2.0};
            else if constexpr (std::is_same_v<T, SphereSpec>)
                return {p.radius, p.radius, p.radius};
            else if constexpr (std::is_same_v<T, ConeSpec>) {
                const double r = std::max(p.radiusBottom, p.radiusTop);
                return {r, r, p.height / 2.0};
            } else if constexpr (std::is_same_v<T, TorusSpec>) {
                const double r = p.majorRadius + p.minorRadius;
                return {r, r, p.minorRadius};
            } else {
                static_assert(std::is_same_v<T, TubeSpec>, "halfExtents: unhandled primitive");
                return {p.outerRadius, p.outerRadius, p.height / 2.0};
            }
        },
        spec);
}

Vec3 pivotOf(const PrimitiveSpec& spec)
{
    return {0.0, 0.0, halfExtents(spec)[2]};
}

Vec3 pivotOf(const Body& body)
{
    return body.isBoolean() ? body.boolean->pivot : pivotOf(body.spec);
}

Transform placementTransform(const Placement& p, const Vec3& c)
{
    return Transform::translation({p.x, p.y, p.z}) * Transform::translation(c) *
           Transform::rotationXYZ(p.rotationX, p.rotationY, p.rotationZ) *
           Transform::translation({-c[0], -c[1], -c[2]});
}

Transform placementTransform(const Placement& p, const PrimitiveSpec& spec)
{
    return placementTransform(p, pivotOf(spec));
}

Placement placementFromTransform(const Transform& t, const Vec3& c)
{
    // t = T(p) T(c) R T(-c)  =>  translation part = p + c - R c
    const Vec3 angles = eulerXYZ(t);
    const Vec3 rc = t.applyDirection(c);
    Placement p;
    p.x = t.m[0][3] - c[0] + rc[0];
    p.y = t.m[1][3] - c[1] + rc[1];
    p.z = t.m[2][3] - c[2] + rc[2];
    p.rotationX = angles[0];
    p.rotationY = angles[1];
    p.rotationZ = angles[2];
    return p;
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

Placement rotatedAboutPlateAxis(const Body& body, int axis, double degrees, const Vec3& centre)
{
    if (axis < 0 || axis > 2)
        throw std::invalid_argument("axis must be 0, 1 or 2");
    const Vec3 pivot = pivotOf(body);
    const Transform current = placementTransform(body.placement, pivot);
    const Vec3 w = current.apply(centre);  // where the centre is now
    const Transform turn = Transform::rotationXYZ(axis == 0 ? degrees : 0.0, axis == 1 ? degrees : 0.0,
                                                  axis == 2 ? degrees : 0.0);
    // Turn about the fixed plate axis through w.
    const Transform next =
        Transform::translation(w) * turn * Transform::translation({-w[0], -w[1], -w[2]}) * current;
    return placementFromTransform(next, pivot);
}

Placement withRotation(const Body& body, const Vec3& anglesDeg, const Vec3& centre)
{
    const Vec3 pivot = pivotOf(body);
    const Vec3 w = placementTransform(body.placement, pivot).apply(centre);
    // New rotation, with `centre` mapped back onto w.
    const Transform next = Transform::translation(w) *
                           Transform::rotationXYZ(anglesDeg[0], anglesDeg[1], anglesDeg[2]) *
                           Transform::translation({-centre[0], -centre[1], -centre[2]});
    Placement p = placementFromTransform(next, pivot);
    // Keep the angles exactly as typed (eulerXYZ may pick an equivalent set).
    p.rotationX = anglesDeg[0];
    p.rotationY = anglesDeg[1];
    p.rotationZ = anglesDeg[2];
    return p;
}

double dimension(const PrimitiveSpec& spec, int axis)
{
    if (axis < 0 || axis > 2)
        throw std::invalid_argument("axis must be 0, 1 or 2");
    const Vec3 h = halfExtents(spec);
    if (axis == 2)
        return 2.0 * h[2];  // full height
    // Sides: full width / depth for boxes and wedges, the radius for round shapes.
    return isRoundFromAbove(spec) ? h[static_cast<std::size_t>(axis)] : 2.0 * h[static_cast<std::size_t>(axis)];
}

namespace {

// The point (body coordinates) that must not move when a face is dragged.
Vec3 anchorFor(const PrimitiveSpec& spec, int axis, int side)
{
    if (axis == 2)
        return side > 0 ? Vec3{0.0, 0.0, 0.0} : Vec3{0.0, 0.0, dimension(spec, 2)};  // the other end
    if (isRoundFromAbove(spec))
        return {0.0, 0.0, 0.0};  // centre line, at the base
    Vec3 a{0.0, 0.0, 0.0};       // centre of the opposite side, at base level
    a[static_cast<std::size_t>(axis)] = -side * dimension(spec, axis) / 2.0;
    return a;
}

void scaleAll(PrimitiveSpec& spec, double factor)
{
    spec = scaled(spec, factor);
}

double smallestDimension(const PrimitiveSpec& spec)
{
    return std::min({dimension(spec, 0), dimension(spec, 1), dimension(spec, 2)});
}

// Sets dimension(axis) to value, keeping the shape valid.
void resizeOne(PrimitiveSpec& spec, int axis, double value, const ResizeOptions& options)
{
    const double minV = options.minValue;
    std::visit(
        [&](auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec> || std::is_same_v<T, WedgeSpec>) {
                (axis == 0 ? p.width : axis == 1 ? p.depth : p.height) = value;
            } else if constexpr (std::is_same_v<T, CylinderSpec>) {
                if (axis == 2) {
                    p.height = value;
                } else if (options.keepRound) {
                    p.radiusX = value;
                    p.radiusY = value;
                } else {
                    (axis == 0 ? p.radiusX : p.radiusY) = value;
                }
            } else if constexpr (std::is_same_v<T, SphereSpec>) {
                p.radius = axis == 2 ? value / 2.0 : value;  // stays a sphere
            } else if constexpr (std::is_same_v<T, ConeSpec>) {
                if (axis == 2) {
                    p.height = value;
                } else {
                    // The wider end gets the new radius; the other keeps its proportion.
                    const double factor = value / std::max(p.radiusBottom, p.radiusTop);
                    p.radiusBottom *= factor;
                    p.radiusTop *= factor;
                }
            } else if constexpr (std::is_same_v<T, TorusSpec>) {
                if (axis == 2) {
                    // Thicker ring; the ring grows if needed to keep its hole.
                    p.minorRadius = value / 2.0;
                    p.majorRadius = std::max(p.majorRadius, p.minorRadius + minV);
                } else {
                    // Whole ring wider, same proportions.
                    const double factor = value / (p.majorRadius + p.minorRadius);
                    p.majorRadius *= factor;
                    p.minorRadius *= factor;
                }
            } else {
                static_assert(std::is_same_v<T, TubeSpec>, "resizeOne: unhandled primitive");
                if (axis == 2) {
                    p.height = value;
                } else {
                    // Same wall thickness; never thinner than the wall plus a hole.
                    const double wall = p.outerRadius - p.innerRadius;
                    p.outerRadius = std::max(value, wall + minV);
                    p.innerRadius = p.outerRadius - wall;
                }
            }
        },
        spec);
}

}  // namespace

Body resizedFace(const Body& body, int axis, int side, double newValue, const ResizeOptions& options)
{
    if (axis < 0 || axis > 2 || (side != 1 && side != -1))
        throw std::invalid_argument("resizedFace: axis must be 0..2 and side +1 or -1");

    Body result = body;
    const double value = std::max(newValue, options.minValue);
    if (options.uniform) {
        // The same factor everywhere, but never shrink any size below the minimum.
        double factor = value / dimension(body.spec, axis);
        factor = std::max(factor, options.minValue / smallestDimension(body.spec));
        scaleAll(result.spec, factor);
    } else {
        resizeOne(result.spec, axis, value, options);
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
