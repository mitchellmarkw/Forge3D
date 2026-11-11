// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/view/Gizmo.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace forge::view;
using Catch::Matchers::WithinAbs;

namespace {
const QSizeF kViewport(1000, 700);

GizmoTarget boxAt(QVector3D centre)
{
    GizmoTarget t;
    t.centre = centre;
    t.halfExtents = QVector3D(20, 15, 10);
    return t;
}
}  // namespace

TEST_CASE("the gizmo keeps the same size on screen at any zoom", "[gizmo-view]")
{
    OrbitCamera cam;
    cam.fitSphere(QVector3D(0, 0, 10), 30);
    const float nearSize = [&] {
        Gizmo g(cam, kViewport, boxAt({0, 0, 10}), GizmoMode::Move);
        const auto a = g.project({0, 0, 10});
        const auto b = g.project(QVector3D(0, 0, 10) + QVector3D(1, 0, 0) * Gizmo::kArrowPixels * g.worldPerPixel());
        return std::hypot(b->x() - a->x(), b->y() - a->y());
    }();
    cam.zoom(-6.0f);  // much further away
    const float farSize = [&] {
        Gizmo g(cam, kViewport, boxAt({0, 0, 10}), GizmoMode::Move);
        const auto a = g.project({0, 0, 10});
        const auto b = g.project(QVector3D(0, 0, 10) + QVector3D(1, 0, 0) * Gizmo::kArrowPixels * g.worldPerPixel());
        return std::hypot(b->x() - a->x(), b->y() - a->y());
    }();
    CHECK_THAT(nearSize, WithinAbs(farSize, 3.0));
}

TEST_CASE("clicking near an arrow grabs that arrow", "[gizmo-view]")
{
    OrbitCamera cam;
    Gizmo g(cam, kViewport, boxAt({0, 0, 10}), GizmoMode::Move);
    for (int axis = 0; axis < 3; ++axis) {
        QVector3D dir(0, 0, 0);
        dir[axis] = 1.0f;
        const auto mid = g.project(QVector3D(0, 0, 10) + dir * (0.6f * Gizmo::kArrowPixels * g.worldPerPixel()));
        REQUIRE(mid);
        const GizmoHandle h = g.hitTest(*mid + QPointF(2, 1));
        CHECK(h.kind == GizmoHandle::Kind::Arrow);
        CHECK(h.axis == axis);
    }
    CHECK_FALSE(g.hitTest(QPointF(5, 5)).valid());  // far corner of the screen
}

TEST_CASE("clicking on a ring grabs that ring", "[gizmo-view]")
{
    OrbitCamera cam;
    Gizmo g(cam, kViewport, boxAt({0, 0, 10}), GizmoMode::Rotate);
    // A point on the Z ring (in the XY plane).
    const auto p = g.project(QVector3D(0, 0, 10) + QVector3D(0.7071f, -0.7071f, 0) * Gizmo::kRingPixels * g.worldPerPixel());
    REQUIRE(p);
    const GizmoHandle h = g.hitTest(*p);
    CHECK(h.kind == GizmoHandle::Kind::Ring);
    CHECK(h.axis == 2);
}

TEST_CASE("resize mode offers a handle on each face", "[gizmo-view]")
{
    OrbitCamera cam;
    Gizmo g(cam, kViewport, boxAt({5, 0, 10}), GizmoMode::Resize);
    REQUIRE(g.faceHandles().size() == 6);
    const GizmoHandle top{GizmoHandle::Kind::Face, 2, +1};
    const QVector3D pos = g.facePosition(top);
    CHECK(pos.z() > 20.0f);  // above the top face (10 + 10) by a gap
    const auto screen = g.project(pos);
    REQUIRE(screen);
    CHECK(g.hitTest(*screen) == top);
    CHECK(g.direction(top) == QVector3D(0, 0, 1));
    CHECK(g.direction({GizmoHandle::Kind::Face, 0, -1}) == QVector3D(-1, 0, 0));
}

TEST_CASE("each mode draws something, highlighted when hot", "[gizmo-view]")
{
    OrbitCamera cam;
    for (GizmoMode mode : {GizmoMode::Move, GizmoMode::Rotate, GizmoMode::Resize}) {
        Gizmo g(cam, kViewport, boxAt({0, 0, 10}), mode);
        const auto tris = g.triangles({});
        CHECK(!tris.empty());
        CHECK(tris.size() % 18 == 0);  // whole triangles of x y z r g b
    }
}

TEST_CASE("closestOnLine follows the mouse along an axis", "[gizmo-view]")
{
    // Line: the X axis. Ray: straight down at x = 7.
    const auto s = closestOnLine({7, 3, 100}, {0, 0, -1}, {0, 0, 0}, {1, 0, 0});
    REQUIRE(s);
    CHECK_THAT(*s, WithinAbs(7.0, 1e-4));
    CHECK_FALSE(closestOnLine({0, 0, 5}, {1, 0, 0}, {0, 0, 0}, {1, 0, 0}));  // parallel
}

TEST_CASE("intersectPlane finds the plate point under a ray", "[gizmo-view]")
{
    const auto p = intersectPlane({10, 20, 50}, QVector3D(0, 1, -1).normalized(), {0, 0, 0}, {0, 0, 1});
    REQUIRE(p);
    CHECK_THAT(p->x(), WithinAbs(10, 1e-4));
    CHECK_THAT(p->y(), WithinAbs(70, 1e-3));
    CHECK_FALSE(intersectPlane({0, 0, 50}, {0, 0, 1}, {0, 0, 0}, {0, 0, 1}));  // pointing away
    CHECK_FALSE(intersectPlane({0, 0, 50}, {1, 0, 0}, {0, 0, 0}, {0, 0, 1}));  // parallel
}

TEST_CASE("screenAngle measures anticlockwise as positive", "[gizmo-view]")
{
    const QPointF c(100, 100);
    CHECK_THAT(screenAngle(c, {150, 100}, {100, 50}), WithinAbs(90.0, 1e-4));   // right -> up
    CHECK_THAT(screenAngle(c, {150, 100}, {100, 150}), WithinAbs(-90.0, 1e-4)); // right -> down
    CHECK_THAT(screenAngle(c, {50, 101}, {50, 99}), WithinAbs(-2.29, 0.05));    // wraps across 180
}

TEST_CASE("snapTo rounds to the step", "[gizmo-view]")
{
    CHECK(snapTo(7.4, 1.0) == 7.0);
    CHECK(snapTo(7.6, 1.0) == 8.0);
    CHECK(snapTo(-22.0, 15.0) == -15.0);
    CHECK(snapTo(3.3, 0.0) == 3.3);
}
