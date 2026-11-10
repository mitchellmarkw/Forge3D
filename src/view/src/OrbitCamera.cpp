// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/view/OrbitCamera.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace forge::view {

namespace {
constexpr float kOrbitDegreesPerPixel = 0.4f;
constexpr float kZoomFactorPerStep = 0.85f;
const QVector3D kWorldUp(0.0f, 0.0f, 1.0f);
}  // namespace

OrbitCamera::OrbitCamera()
{
    reset();
}

void OrbitCamera::reset()
{
    m_target = QVector3D(0.0f, 0.0f, 0.0f);
    m_distance = 200.0f;
    m_yaw = -60.0f;
    m_pitch = 25.0f;
    m_fovY = 35.0f;
}

QVector3D OrbitCamera::eyePosition() const
{
    const float yaw = qDegreesToRadians(m_yaw);
    const float pitch = qDegreesToRadians(m_pitch);
    const QVector3D offset(std::cos(pitch) * std::cos(yaw),
                           std::cos(pitch) * std::sin(yaw),
                           std::sin(pitch));
    return m_target + offset * m_distance;
}

QVector3D OrbitCamera::forward() const
{
    return (m_target - eyePosition()).normalized();
}

void OrbitCamera::orbit(float dxPixels, float dyPixels)
{
    m_yaw -= dxPixels * kOrbitDegreesPerPixel;
    m_yaw = std::fmod(m_yaw, 360.0f);
    m_pitch = std::clamp(m_pitch + dyPixels * kOrbitDegreesPerPixel, -kMaxPitch, kMaxPitch);
}

void OrbitCamera::pan(float dxPixels, float dyPixels, float viewportHeightPixels)
{
    if (viewportHeightPixels <= 0.0f)
        return;
    // World units covered by one pixel at the target's depth.
    const float worldPerPixel =
        2.0f * m_distance * std::tan(qDegreesToRadians(m_fovY) * 0.5f) / viewportHeightPixels;
    const QVector3D fwd = forward();
    const QVector3D right = QVector3D::crossProduct(fwd, kWorldUp).normalized();
    const QVector3D up = QVector3D::crossProduct(right, fwd).normalized();
    // Dragging right moves the model right, so the camera target moves left.
    m_target += (-right * dxPixels + up * dyPixels) * worldPerPixel;
}

void OrbitCamera::zoom(float wheelSteps)
{
    m_distance *= std::pow(kZoomFactorPerStep, wheelSteps);
    m_distance = std::clamp(m_distance, kMinDistance, kMaxDistance);
}

void OrbitCamera::fitSphere(const QVector3D& center, float radius)
{
    m_target = center;
    const float halfFov = qDegreesToRadians(m_fovY) * 0.5f;
    const float needed = std::max(radius, 1.0f) / std::sin(halfFov) * 1.15f;
    m_distance = std::clamp(needed, kMinDistance, kMaxDistance);
}

QMatrix4x4 OrbitCamera::viewMatrix() const
{
    QMatrix4x4 view;
    view.lookAt(eyePosition(), m_target, kWorldUp);
    return view;
}

QMatrix4x4 OrbitCamera::projectionMatrix(float aspect) const
{
    QMatrix4x4 proj;
    const float nearPlane = std::max(0.1f, m_distance * 0.01f);
    const float farPlane = m_distance * 50.0f + 1000.0f;
    proj.perspective(m_fovY, aspect > 0.0f ? aspect : 1.0f, nearPlane, farPlane);
    return proj;
}

}  // namespace forge::view
