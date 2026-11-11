// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
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
#include <unordered_map>
#include <vector>

namespace forge::app {

// One thing to draw: a mesh in its own coordinates, plus where it sits.
struct DisplayItem {
    std::uint64_t id = 0;
    std::shared_ptr<const forge::Mesh> mesh;  // a new pointer = new geometry, re-uploaded
    QMatrix4x4 model;                          // placement on the plate
    QVector3D color{0.28f, 0.55f, 0.85f};
};

// The 3D view: draws the build plate and the scene, and handles navigation.
// Mouse scheme (3D Builder style):
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

    QString glVersionString() const { return m_glVersion; }

signals:
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
    void syncGpuMeshes();
    void upload(GpuMesh& gpu, const std::shared_ptr<const forge::Mesh>& mesh);
    void releaseGL();

    view::OrbitCamera m_camera;

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

    // Interaction
    DragMode m_drag = DragMode::None;
    QPointF m_lastMouse;

    QString m_glVersion;
    bool m_glReady = false;
    bool m_firstFrameDone = false;
};

}  // namespace forge::app
