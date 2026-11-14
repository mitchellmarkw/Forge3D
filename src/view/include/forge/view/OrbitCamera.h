// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace forge::view {

// A turntable camera around a target point, with Z pointing up (like a build plate).
// All mouse amounts are in device-independent pixels.
class OrbitCamera {
public:
    OrbitCamera();

    // Interaction
    void orbit(float dxPixels, float dyPixels);
    void pan(float dxPixels, float dyPixels, float viewportHeightPixels);
    void zoom(float wheelSteps);  // positive = closer
    void fitSphere(const QVector3D& center, float radius);
    void reset();
    // Turns to look from a direction: yaw about +Z, pitch above the plate (degrees;
    // pitch kept within +-kMaxPitch).
    void setAngles(float yawDegrees, float pitchDegrees);

    // Matrices
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix(float aspect) const;
    QVector3D eyePosition() const;
    QVector3D forward() const;  // unit vector from eye towards target

    // The ray under a viewport pixel (device-independent pixels, origin top-left),
    // for picking. Returns the start point (on the near plane) and a unit direction.
    void rayThroughPixel(float px, float py, float viewportWidth, float viewportHeight,
                         QVector3D& origin, QVector3D& direction) const;

    // State (exposed for tests and for saving the view later)
    QVector3D target() const { return m_target; }
    float distance() const { return m_distance; }
    float yawDegrees() const { return m_yaw; }
    float pitchDegrees() const { return m_pitch; }
    float fieldOfViewDegrees() const { return m_fovY; }

    static constexpr float kMinDistance = 1.0f;     // mm
    static constexpr float kMaxDistance = 20000.0f; // mm
    static constexpr float kMaxPitch = 89.0f;

private:
    QVector3D m_target;
    float m_distance;
    float m_yaw;    // degrees, rotation about +Z
    float m_pitch;  // degrees, elevation above the XY plane
    float m_fovY;   // degrees
};

}  // namespace forge::view
