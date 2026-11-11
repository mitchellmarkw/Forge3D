// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/view/OrbitCamera.h"

#include <QOpenGLBuffer>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>

#include <memory>

namespace forge::app {

// The 3D view: draws the build plate and the current model, and handles
// navigation. Mouse scheme (3D Builder style):
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

    void setMesh(const forge::Mesh& mesh);
    void fitView();
    void resetView();

    QString glVersionString() const { return m_glVersion; }

signals:
    void glInfoReady(const QString& summary);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class DragMode { None, Orbit, Pan };

    void buildPlateGeometry();
    void uploadMeshIfNeeded();
    void releaseGL();

    view::OrbitCamera m_camera;

    // Model
    forge::Mesh m_mesh;
    bool m_meshDirty = false;
    int m_meshIndexCount = 0;
    QOpenGLVertexArrayObject m_meshVao;
    QOpenGLBuffer m_meshPositions{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_meshNormals{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_meshIndices{QOpenGLBuffer::IndexBuffer};
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
};

}  // namespace forge::app
