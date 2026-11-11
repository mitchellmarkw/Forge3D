// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/view/Gizmo.h"

#include <QVector4D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace forge::view {

namespace {

constexpr int kRingSegments = 72;
constexpr float kRibbonHalfWidthPixels = 1.6f;
constexpr float kHotRibbonHalfWidthPixels = 2.6f;

const std::array<QVector3D, 3> kAxisColor{QVector3D(0.90f, 0.22f, 0.22f), QVector3D(0.25f, 0.72f, 0.28f),
                                          QVector3D(0.22f, 0.45f, 0.95f)};
const QVector3D kHotColor(1.0f, 0.82f, 0.10f);
const std::array<QVector3D, 3> kPlateAxis{QVector3D(1, 0, 0), QVector3D(0, 1, 0), QVector3D(0, 0, 1)};

float distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    double t = len2 > 0.0 ? QPointF::dotProduct(p - a, ab) / len2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const QPointF closest = a + ab * t;
    return static_cast<float>(std::hypot(p.x() - closest.x(), p.y() - closest.y()));
}

void pushVertex(std::vector<float>& out, const QVector3D& p, const QVector3D& c)
{
    out.insert(out.end(), {p.x(), p.y(), p.z(), c.x(), c.y(), c.z()});
}

void pushTriangle(std::vector<float>& out, const QVector3D& a, const QVector3D& b, const QVector3D& c,
                  const QVector3D& color)
{
    pushVertex(out, a, color);
    pushVertex(out, b, color);
    pushVertex(out, c, color);
}

}  // namespace

Gizmo::Gizmo(const OrbitCamera& camera, QSizeF viewport, const GizmoTarget& target, GizmoMode mode)
    : m_camera(camera), m_viewport(viewport), m_target(target), m_mode(mode)
{
    const float w = static_cast<float>(std::max(1.0, viewport.width()));
    const float h = static_cast<float>(std::max(1.0, viewport.height()));
    m_viewProj = camera.projectionMatrix(w / h) * camera.viewMatrix();
    m_eye = camera.eyePosition();
    const float depth = std::max(1e-3f, QVector3D::dotProduct(target.centre - m_eye, camera.forward()));
    m_worldPerPixel = 2.0f * depth * std::tan(qDegreesToRadians(camera.fieldOfViewDegrees()) * 0.5f) / h;
}

std::optional<QPointF> Gizmo::project(const QVector3D& p) const
{
    const QVector4D clip = m_viewProj * QVector4D(p, 1.0f);
    if (clip.w() <= 1e-6f)
        return std::nullopt;
    const float nx = clip.x() / clip.w();
    const float ny = clip.y() / clip.w();
    return QPointF((nx + 1.0) * 0.5 * m_viewport.width(), (1.0 - ny) * 0.5 * m_viewport.height());
}

QVector3D Gizmo::direction(const GizmoHandle& handle) const
{
    if (handle.axis < 0 || handle.axis > 2)
        return {};
    if (handle.kind == GizmoHandle::Kind::Face)
        return m_target.bodyAxes.at(static_cast<std::size_t>(handle.axis)) * static_cast<float>(handle.side);
    return kPlateAxis.at(static_cast<std::size_t>(handle.axis));
}

QVector3D Gizmo::facePosition(const GizmoHandle& handle) const
{
    const auto i = static_cast<std::size_t>(handle.axis);
    const float extent = (i == 0 ? m_target.halfExtents.x() : i == 1 ? m_target.halfExtents.y()
                                                                     : m_target.halfExtents.z());
    return m_target.centre + direction(handle) * (extent + kFaceGapPixels * m_worldPerPixel);
}

std::vector<GizmoHandle> Gizmo::faceHandles() const
{
    std::vector<GizmoHandle> handles;
    if (!m_target.resizable)
        return handles;
    for (int axis = 0; axis < 3; ++axis)
        for (int side : {+1, -1})
            handles.push_back({GizmoHandle::Kind::Face, axis, side});
    return handles;
}

QVector3D Gizmo::ringPoint(int axis, float angle) const
{
    // Two unit vectors spanning the plane perpendicular to the axis.
    const QVector3D& u = kPlateAxis.at(static_cast<std::size_t>((axis + 1) % 3));
    const QVector3D& v = kPlateAxis.at(static_cast<std::size_t>((axis + 2) % 3));
    const float r = kRingPixels * m_worldPerPixel;
    return m_target.centre + (u * std::cos(angle) + v * std::sin(angle)) * r;
}

GizmoHandle Gizmo::hitTest(const QPointF& mouse) const
{
    GizmoHandle best;
    float bestDistance = kHitPixels;
    const auto consider = [&](const GizmoHandle& handle, float distance) {
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = handle;
        }
    };

    const auto centre = project(m_target.centre);
    if (!centre)
        return best;

    switch (m_mode) {
    case GizmoMode::Move:
        for (int axis = 0; axis < 3; ++axis) {
            const auto tip = project(m_target.centre + kPlateAxis.at(axis) * kArrowPixels * m_worldPerPixel);
            if (!tip || std::hypot(tip->x() - centre->x(), tip->y() - centre->y()) < 12.0)
                continue;  // axis points at the viewer: too short on screen to grab reliably
            consider({GizmoHandle::Kind::Arrow, axis, 0}, distanceToSegment(mouse, *centre, *tip));
        }
        break;
    case GizmoMode::Rotate:
        for (int axis = 0; axis < 3; ++axis) {
            std::optional<QPointF> prev = project(ringPoint(axis, 0.0f));
            for (int s = 1; s <= kRingSegments; ++s) {
                const auto next = project(ringPoint(axis, 2.0f * float(M_PI) * s / kRingSegments));
                if (prev && next)
                    consider({GizmoHandle::Kind::Ring, axis, 0}, distanceToSegment(mouse, *prev, *next));
                prev = next;
            }
        }
        break;
    case GizmoMode::Resize:
        for (const GizmoHandle& h : faceHandles()) {
            if (const auto p = project(facePosition(h)))
                consider(h, std::hypot(mouse.x() - p->x(), mouse.y() - p->y()) - 3.0f);  // handles are ~7 px
        }
        break;
    }
    return best;
}

std::vector<float> Gizmo::triangles(const GizmoHandle& hot, const GizmoHandle& active) const
{
    std::vector<float> out;
    const float px = m_worldPerPixel;
    const QVector3D camRight = QVector3D::crossProduct(m_camera.forward(), QVector3D(0, 0, 1)).normalized();
    const QVector3D camUp = QVector3D::crossProduct(camRight, m_camera.forward()).normalized();

    const auto colorFor = [&](const GizmoHandle& h) {
        if (h == hot || h == active)
            return kHotColor;
        const QVector3D base = kAxisColor.at(static_cast<std::size_t>(h.axis));
        // While dragging one handle, fade the others.
        return active.valid() ? base * 0.35f + QVector3D(0.6f, 0.6f, 0.6f) * 0.65f : base;
    };
    // A flat strip from a to b, turned to face the camera, `half` pixels each side.
    const auto ribbon = [&](const QVector3D& a, const QVector3D& b, float half, const QVector3D& color) {
        const QVector3D d = b - a;
        QVector3D side = QVector3D::crossProduct(d, (a - m_eye)).normalized() * (half * px);
        if (side.isNull())
            side = camRight * (half * px);
        pushTriangle(out, a - side, a + side, b + side, color);
        pushTriangle(out, a - side, b + side, b - side, color);
    };

    switch (m_mode) {
    case GizmoMode::Move:
        for (int axis = 0; axis < 3; ++axis) {
            const GizmoHandle h{GizmoHandle::Kind::Arrow, axis, 0};
            const QVector3D dir = kPlateAxis.at(axis);
            const QVector3D start = m_target.centre + dir * (10.0f * px);
            const QVector3D shaftEnd = m_target.centre + dir * ((kArrowPixels - 18.0f) * px);
            const QVector3D tip = m_target.centre + dir * (kArrowPixels * px);
            const QVector3D color = colorFor(h);
            ribbon(start, shaftEnd, h == hot || h == active ? kHotRibbonHalfWidthPixels : kRibbonHalfWidthPixels,
                   color);
            // Flat arrowhead facing the camera.
            QVector3D side = QVector3D::crossProduct(dir, shaftEnd - m_eye).normalized() * (7.0f * px);
            if (side.isNull())
                side = camRight * (7.0f * px);
            pushTriangle(out, shaftEnd - side, shaftEnd + side, tip, color);
        }
        break;
    case GizmoMode::Rotate:
        for (int axis = 0; axis < 3; ++axis) {
            const GizmoHandle h{GizmoHandle::Kind::Ring, axis, 0};
            const QVector3D color = colorFor(h);
            const float half = h == hot || h == active ? kHotRibbonHalfWidthPixels : kRibbonHalfWidthPixels;
            for (int s = 0; s < kRingSegments; ++s) {
                ribbon(ringPoint(axis, 2.0f * float(M_PI) * s / kRingSegments),
                       ringPoint(axis, 2.0f * float(M_PI) * (s + 1) / kRingSegments), half, color);
            }
        }
        break;
    case GizmoMode::Resize:
        for (const GizmoHandle& h : faceHandles()) {
            const QVector3D p = facePosition(h);
            const float half = (h == hot || h == active ? 7.5f : 6.0f) * px;
            const QVector3D r = camRight * half, u = camUp * half;
            const QVector3D color = colorFor(h);
            pushTriangle(out, p - r - u, p + r - u, p + r + u, color);
            pushTriangle(out, p - r - u, p + r + u, p - r + u, color);
            // A thin line back to the face shows which face the handle belongs to.
            const auto i = static_cast<std::size_t>(h.axis);
            const float extent = i == 0 ? m_target.halfExtents.x()
                                        : i == 1 ? m_target.halfExtents.y() : m_target.halfExtents.z();
            ribbon(m_target.centre + direction(h) * extent, p, 0.8f, color);
        }
        break;
    }
    return out;
}

// ---- Drag maths -----------------------------------------------------------------

std::optional<float> closestOnLine(const QVector3D& rayOrigin, const QVector3D& rayDir,
                                   const QVector3D& linePoint, const QVector3D& lineDir)
{
    const QVector3D l = lineDir.normalized();
    const QVector3D r = rayDir.normalized();
    const QVector3D w0 = linePoint - rayOrigin;
    const float b = QVector3D::dotProduct(l, r);
    const float denom = 1.0f - b * b;
    if (denom < 1e-6f)
        return std::nullopt;  // parallel
    const float d = QVector3D::dotProduct(l, w0);
    const float e = QVector3D::dotProduct(r, w0);
    return (b * e - d) / denom;
}

std::optional<QVector3D> intersectPlane(const QVector3D& rayOrigin, const QVector3D& rayDir,
                                        const QVector3D& planePoint, const QVector3D& planeNormal)
{
    const float denom = QVector3D::dotProduct(rayDir, planeNormal);
    if (std::abs(denom) < 1e-6f)
        return std::nullopt;
    const float t = QVector3D::dotProduct(planePoint - rayOrigin, planeNormal) / denom;
    if (t < 0.0f)
        return std::nullopt;
    return rayOrigin + rayDir * t;
}

float screenAngle(const QPointF& centre, const QPointF& from, const QPointF& to)
{
    // Screen y grows downward; flip it so anticlockwise is positive.
    const double a0 = std::atan2(-(from.y() - centre.y()), from.x() - centre.x());
    const double a1 = std::atan2(-(to.y() - centre.y()), to.x() - centre.x());
    double d = (a1 - a0) * 180.0 / M_PI;
    while (d > 180.0)
        d -= 360.0;
    while (d < -180.0)
        d += 360.0;
    return static_cast<float>(d);
}

double snapTo(double value, double step)
{
    if (!(step > 0.0))
        return value;
    const double r = std::round(value / step) * step;
    return r == 0.0 ? 0.0 : r;
}

}  // namespace forge::view
