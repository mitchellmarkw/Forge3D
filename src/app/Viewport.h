// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/view/Gizmo.h"
#include "forge/view/OrbitCamera.h"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <map>
#include <set>
#include <optional>
#include <unordered_map>
#include <vector>

namespace forge::app {

// One thing to draw: a mesh in its own coordinates, plus where it sits.
struct DisplayItem {
    std::uint64_t id = 0;
    std::shared_ptr<const forge::Mesh> mesh;  // a new pointer = new geometry, re-uploaded
    QMatrix4x4 model;                          // placement on the plate
    QVector3D color{0.28f, 0.55f, 0.85f};
    bool selected = false;
    bool ghost = false;  // drawn see-through on top (a part inside a combined shape)
    // Below 1: drawn see-through after the solids, still hidden behind them (the
    // Split tool's plane). Not counted by Fit.
    float alpha = 1.0f;
};

// The 3D view: draws the build plate and the scene, and handles navigation.
// Mouse scheme (3D Builder style):
//   left-click          select (signal clicked; the window decides what was hit)
//   left-drag           move a body on the plate, or drag a gizmo handle
//                       (signals leftPressed / leftDragged / leftReleased)
//   right-drag          orbit
//   middle-drag         pan   (Shift + right-drag also pans, for touchpads)
//   wheel               zoom
//   F                   fit the model in view
//   Home                reset the view
class Viewport : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT

public:
    explicit Viewport(QWidget* parent = nullptr);
    ~Viewport() override;

    // Replaces what is drawn. GPU buffers are kept for meshes that did not change.
    void setItems(std::vector<DisplayItem> items);

    void fitView();
    void resetView();

    // The gizmo around the selected body (empty = none shown).
    void setGizmo(const std::optional<view::GizmoTarget>& target, view::GizmoMode mode);
    // The handle being dragged, drawn highlighted (none = not dragging).
    void setActiveHandle(const view::GizmoHandle& handle);

    // Sculpting: points [first, last) of item `id`'s mesh moved in place (same mesh, same
    // triangles); only they are sent to the graphics card again.
    void meshPointsChanged(std::uint64_t id, std::uint32_t first, std::uint32_t last);
    // Sculpting: item `id`'s mesh gained points and triangles (same mesh object): sent
    // to the graphics card again in full.
    void meshGrew(std::uint64_t id);
    // A ring showing the brush on the surface (none: hidden).
    struct BrushRing {
        QVector3D centre;
        QVector3D normal;
        float radius = 1.0f;
    };
    void setBrushRing(const std::optional<BrushRing>& ring);

    const view::OrbitCamera& camera() const { return m_camera; }
    std::optional<QPointF> projectToScreen(const QVector3D& point) const;

    QString glVersionString() const { return m_glVersion; }

signals:
    // A left click (not a drag). origin/direction: the ray under the cursor, in plate mm.
    void clicked(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers);
    // Left button pressed. `handle` is the gizmo part under the cursor, if any.
    void leftPressed(const QVector3D& origin, const QVector3D& direction, const view::GizmoHandle& handle,
                     const QPointF& pixel, Qt::KeyboardModifiers modifiers);
    // The mouse moved with the left button held, past a small dead zone.
    void leftDragged(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel,
                     Qt::KeyboardModifiers modifiers);
    void leftReleased(bool wasDrag);
    // The mouse moved with no button held (for tools that highlight what's under it).
    void hovered(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel);
    void glInfoReady(const QString& summary);
    void firstFrameRendered();

protected:
    void initializeGL() override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class DragMode { None, Orbit, Pan };

    // GPU copy of one mesh.
    struct GpuMesh {
        std::shared_ptr<const forge::Mesh> source;
        std::unique_ptr<QOpenGLVertexArrayObject> vao;
        QOpenGLBuffer positions{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer normals{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer indices{QOpenGLBuffer::IndexBuffer};
        int indexCount = 0;
        void destroy();
    };

    void buildPlateGeometry();
    void drawGizmo(const QMatrix4x4& viewProj);
    std::optional<view::Gizmo> makeGizmo() const;
    void updateHover(const QPointF& pixel);
    void syncGpuMeshes();
    void upload(GpuMesh& gpu, const std::shared_ptr<const forge::Mesh>& mesh);
    void releaseGL();
    void drawBrushRing(const QMatrix4x4& viewProj);

    view::OrbitCamera m_camera;

    // Sculpting
    std::map<std::uint64_t, std::pair<std::uint32_t, std::uint32_t>> m_movedPoints;
    std::set<std::uint64_t> m_grown;
    std::optional<BrushRing> m_brushRing;
    QOpenGLVertexArrayObject m_ringVao;
    QOpenGLBuffer m_ringBuffer{QOpenGLBuffer::VertexBuffer};

    // Scene
    std::vector<DisplayItem> m_items;
    bool m_itemsDirty = false;
    std::unordered_map<std::uint64_t, std::unique_ptr<GpuMesh>> m_gpu;
    std::unique_ptr<QOpenGLShaderProgram> m_meshShader;

    // Build plate grid
    int m_plateVertexCount = 0;
    QOpenGLVertexArrayObject m_plateVao;
    QOpenGLBuffer m_plateBuffer{QOpenGLBuffer::VertexBuffer};
    std::unique_ptr<QOpenGLShaderProgram> m_lineShader;

    // Gizmo overlay
    std::optional<view::GizmoTarget> m_gizmoTarget;
    view::GizmoMode m_gizmoMode = view::GizmoMode::Move;
    view::GizmoHandle m_hover;
    view::GizmoHandle m_active;
    QOpenGLVertexArrayObject m_gizmoVao;
    QOpenGLBuffer m_gizmoBuffer{QOpenGLBuffer::VertexBuffer};

    // Interaction
    DragMode m_drag = DragMode::None;
    QPointF m_lastMouse;
    QPointF m_leftPressAt;
    bool m_leftPressed = false;
    bool m_leftDragging = false;

    QString m_glVersion;
    bool m_glReady = false;
    bool m_firstFrameDone = false;
};

}  // namespace forge::app
