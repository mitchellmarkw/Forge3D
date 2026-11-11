// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/view/OrbitCamera.h"

#include <QPointF>
#include <QSizeF>
#include <QVector3D>

#include <array>
#include <optional>
#include <vector>

namespace forge::view {

enum class GizmoMode { Move, Rotate, Resize };

// One grabbable part of the gizmo.
struct GizmoHandle {
    enum class Kind { None, Arrow, Ring, Face };
    Kind kind = Kind::None;
    int axis = -1;  // 0 = X, 1 = Y, 2 = Z (plate axes for arrows and rings, body axes for faces)
    int side = 0;   // faces only: +1 or -1

    bool valid() const { return kind != Kind::None; }
    bool operator==(const GizmoHandle&) const = default;
};

// What the gizmo is attached to, in plate coordinates (mm).
struct GizmoTarget {
    QVector3D centre;                        // the body's centre (its pivot)
    std::array<QVector3D, 3> bodyAxes{QVector3D(1, 0, 0), QVector3D(0, 1, 0), QVector3D(0, 0, 1)};
    QVector3D halfExtents{10, 10, 10};       // centre to each face, along bodyAxes
    bool round = false;                      // cylinder: X and Y face handles both set the radius
    bool resizable = true;                   // false: Resize mode shows no handles (combine results)
};

// The gizmo for one frame: its size on screen stays constant however far the
// camera is. Pure maths (no OpenGL), so it can be unit-tested.
class Gizmo {
public:
    static constexpr float kArrowPixels = 95.0f;  // arrow length
    static constexpr float kRingPixels = 80.0f;   // ring radius
    static constexpr float kFaceGapPixels = 16.0f; // face handles sit this far outside the face
    static constexpr float kHitPixels = 9.0f;     // how close the mouse must be to grab

    Gizmo(const OrbitCamera& camera, QSizeF viewport, const GizmoTarget& target, GizmoMode mode);

    GizmoMode mode() const { return m_mode; }
    const GizmoTarget& target() const { return m_target; }

    // Plate-space size of one screen pixel at the gizmo's centre.
    float worldPerPixel() const { return m_worldPerPixel; }
    // Plate point -> viewport pixel (origin top-left). Empty if behind the camera.
    std::optional<QPointF> project(const QVector3D& p) const;

    // The handle under the mouse, or Kind::None.
    GizmoHandle hitTest(const QPointF& mouse) const;

    // The direction a handle moves along (unit): plate axis for arrows and rings
    // (the ring's axis), the body axis times side for faces.
    QVector3D direction(const GizmoHandle& handle) const;
    // Where a face handle is drawn.
    QVector3D facePosition(const GizmoHandle& handle) const;
    // Every face handle shown for the target.
    std::vector<GizmoHandle> faceHandles() const;

    // Triangles to draw, 6 floats per vertex (x y z r g b). `hot` is drawn
    // highlighted; while dragging, pass the dragged handle as `active` to dim the rest.
    std::vector<float> triangles(const GizmoHandle& hot, const GizmoHandle& active = {}) const;

private:
    QVector3D ringPoint(int axis, float angle) const;

    const OrbitCamera& m_camera;
    QSizeF m_viewport;
    GizmoTarget m_target;
    GizmoMode m_mode;
    QMatrix4x4 m_viewProj;
    QVector3D m_eye;
    float m_worldPerPixel = 1.0f;
};

// ---- Drag maths -----------------------------------------------------------------

// Position along a line (point + s * unit direction) closest to a ray.
// Empty when the ray runs (almost) parallel to the line.
std::optional<float> closestOnLine(const QVector3D& rayOrigin, const QVector3D& rayDir,
                                   const QVector3D& linePoint, const QVector3D& lineDir);

// Where a ray meets a plane; empty if parallel or behind the ray's start.
std::optional<QVector3D> intersectPlane(const QVector3D& rayOrigin, const QVector3D& rayDir,
                                        const QVector3D& planePoint, const QVector3D& planeNormal);

// The angle (degrees, -180..180) swept from `from` to `to` around `centre`, in
// viewport pixels; positive = anticlockwise as seen on screen.
float screenAngle(const QPointF& centre, const QPointF& from, const QPointF& to);

// Rounds value to the nearest multiple of step (step <= 0: unchanged).
double snapTo(double value, double step);

}  // namespace forge::view
