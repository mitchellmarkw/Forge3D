// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/view/OrbitCamera.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using forge::view::OrbitCamera;

TEST_CASE("orbiting keeps the distance to the target", "[camera]")
{
    OrbitCamera cam;
    const float before = (cam.eyePosition() - cam.target()).length();
    cam.orbit(150.0f, -80.0f);
    const float after = (cam.eyePosition() - cam.target()).length();
    CHECK_THAT(after, WithinAbs(before, 1e-3));
}

TEST_CASE("pitch is clamped so the view never flips over the pole", "[camera]")
{
    OrbitCamera cam;
    cam.orbit(0.0f, 10000.0f);
    CHECK(cam.pitchDegrees() <= OrbitCamera::kMaxPitch);
    cam.orbit(0.0f, -20000.0f);
    CHECK(cam.pitchDegrees() >= -OrbitCamera::kMaxPitch);
}

TEST_CASE("zoom moves closer and stays within limits", "[camera]")
{
    OrbitCamera cam;
    const float start = cam.distance();
    cam.zoom(1.0f);
    CHECK(cam.distance() < start);
    cam.zoom(-2.0f);
    CHECK(cam.distance() > start);

    cam.zoom(1000.0f);
    CHECK(cam.distance() == OrbitCamera::kMinDistance);
    cam.zoom(-1000.0f);
    CHECK(cam.distance() == OrbitCamera::kMaxDistance);
}

TEST_CASE("panning moves the target in the view plane", "[camera]")
{
    OrbitCamera cam;
    const QVector3D fwd = cam.forward();
    const QVector3D before = cam.target();
    cam.pan(40.0f, 25.0f, 800.0f);
    const QVector3D moved = cam.target() - before;
    CHECK(moved.length() > 0.0f);
    CHECK_THAT(QVector3D::dotProduct(moved, fwd), WithinAbs(0.0, 1e-4));
    CHECK_THAT(cam.distance(), WithinAbs(200.0, 1e-4));
}

TEST_CASE("fitSphere frames the whole sphere", "[camera]")
{
    OrbitCamera cam;
    cam.fitSphere(QVector3D(5.0f, 5.0f, 10.0f), 50.0f);
    CHECK(cam.target() == QVector3D(5.0f, 5.0f, 10.0f));

    // A point on the sphere's edge, perpendicular to the view, must land inside the frustum.
    const QMatrix4x4 mvp = cam.projectionMatrix(1.0f) * cam.viewMatrix();
    const QVector3D right = QVector3D::crossProduct(cam.forward(), QVector3D(0, 0, 1)).normalized();
    const QVector3D clip = mvp.map(cam.target() + right * 50.0f);
    CHECK(std::abs(clip.x()) < 1.0f);
}

TEST_CASE("the view matrix puts the target straight ahead", "[camera]")
{
    OrbitCamera cam;
    cam.orbit(33.0f, 12.0f);
    const QVector3D inView = cam.viewMatrix().map(cam.target());
    CHECK_THAT(inView.x(), WithinAbs(0.0, 1e-3));
    CHECK_THAT(inView.y(), WithinAbs(0.0, 1e-3));
    CHECK_THAT(inView.z(), WithinAbs(-cam.distance(), 1e-2));
}
