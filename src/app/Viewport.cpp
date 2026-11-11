// SPDX-License-Identifier: GPL-3.0-or-later
#include "Viewport.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <QtDebug>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace forge::app {

namespace {

// ---- Shaders (GLSL 3.30 core) ---------------------------------------------------

const char* kMeshVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat4 uModelView;
uniform mat3 uNormalMatrix;
out vec3 vNormal;
out vec3 vViewPos;
void main() {
    vNormal = uNormalMatrix * aNormal;
    vViewPos = (uModelView * vec4(aPosition, 1.0)).xyz;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

const char* kMeshFragmentShader = R"(#version 330 core
in vec3 vNormal;
in vec3 vViewPos;
uniform vec3 uColor;
out vec4 fragColor;
void main() {
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;               // light the inside of open meshes too
    vec3 toEye = normalize(-vViewPos);
    vec3 key = normalize(vec3(-0.5, 0.7, 0.6)); // light fixed to the camera, upper left
    float diffuse = max(dot(n, key), 0.0);
    float head = max(dot(n, toEye), 0.0);
    float spec = pow(max(dot(n, normalize(key + toEye)), 0.0), 48.0) * 0.20;
    vec3 color = uColor * (0.28 + 0.62 * diffuse + 0.15 * head) + vec3(spec);
    fragColor = vec4(color, 1.0);
}
)";

const char* kLineVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

const char* kLineFragmentShader = R"(#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)";

// ---- Look -----------------------------------------------------------------------

constexpr float kPlateSize = 220.0f;  // mm; a common printer bed. Configurable later.
constexpr float kMinorStep = 10.0f;
constexpr float kMajorStep = 50.0f;
const QVector3D kBackground(0.93f, 0.94f, 0.96f);

std::unique_ptr<QOpenGLShaderProgram> buildProgram(const char* vs, const char* fs, const char* name)
{
    auto program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, vs) ||
        !program->addShaderFromSourceCode(QOpenGLShader::Fragment, fs) || !program->link()) {
        qCritical() << "Shader program" << name << "failed:" << program->log();
        return nullptr;
    }
    return program;
}

}  // namespace

Viewport::Viewport(QWidget* parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);  // receive key presses (F, Home)
    setMinimumSize(320, 240);
}

Viewport::~Viewport()
{
    makeCurrent();
    releaseGL();
    doneCurrent();
}

void Viewport::releaseGL()
{
    for (auto& [id, gpu] : m_gpu)
        gpu->destroy();
    m_gpu.clear();
    m_itemsDirty = true;  // re-upload if a new context arrives
    m_plateVao.destroy();
    m_plateBuffer.destroy();
    m_meshShader.reset();
    m_lineShader.reset();
    m_glReady = false;
}

void Viewport::GpuMesh::destroy()
{
    if (vao)
        vao->destroy();
    positions.destroy();
    normals.destroy();
    indices.destroy();
    indexCount = 0;
    source.reset();
}

void Viewport::setItems(std::vector<DisplayItem> items)
{
    m_items = std::move(items);
    m_itemsDirty = true;
    update();
}

void Viewport::fitView()
{
    Bounds all;
    for (const DisplayItem& item : m_items) {
        if (!item.mesh)
            continue;
        const Bounds local = item.mesh->bounds();
        if (!local.valid)
            continue;
        for (int corner = 0; corner < 8; ++corner) {
            const QVector3D p = item.model.map(QVector3D((corner & 1) ? local.max[0] : local.min[0],
                                                         (corner & 2) ? local.max[1] : local.min[1],
                                                         (corner & 4) ? local.max[2] : local.min[2]));
            all.expand(p.x(), p.y(), p.z());
        }
    }
    if (all.valid) {
        const auto c = all.center();
        m_camera.fitSphere(QVector3D(c[0], c[1], c[2]), all.radius());
    } else {
        m_camera.fitSphere(QVector3D(0, 0, 0), kPlateSize * 0.5f);
    }
    update();
}

void Viewport::resetView()
{
    m_camera.reset();
    fitView();
}

// ---- OpenGL lifecycle -----------------------------------------------------------

void Viewport::initializeGL()
{
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
        makeCurrent();
        releaseGL();
        doneCurrent();
    });

    initializeOpenGLFunctions();

    const QSurfaceFormat fmt = context()->format();
    m_glVersion = QStringLiteral("OpenGL %1.%2").arg(fmt.majorVersion()).arg(fmt.minorVersion());
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const QString summary = QStringLiteral("%1 - %2 (%3)")
                                .arg(m_glVersion, QString::fromLatin1(renderer ? renderer : "?"),
                                     QString::fromLatin1(vendor ? vendor : "?"));
    qInfo().noquote() << "Viewport context:" << summary;

    if (fmt.majorVersion() < 3 || (fmt.majorVersion() == 3 && fmt.minorVersion() < 3)) {
        qCritical() << "OpenGL 3.3 or newer is required; got" << m_glVersion;
        emit glInfoReady(summary + QStringLiteral(" - too old, needs OpenGL 3.3"));
        return;
    }

    m_meshShader = buildProgram(kMeshVertexShader, kMeshFragmentShader, "mesh");
    m_lineShader = buildProgram(kLineVertexShader, kLineFragmentShader, "line");
    if (!m_meshShader || !m_lineShader) {
        emit glInfoReady(summary + QStringLiteral(" - shader error, see log"));
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    buildPlateGeometry();
    m_itemsDirty = true;
    m_glReady = true;
    emit glInfoReady(summary);
}

void Viewport::buildPlateGeometry()
{
    // Interleaved: x y z r g b
    std::vector<float> vertices;
    const auto addLine = [&vertices](float x0, float y0, float x1, float y1, const QVector3D& color) {
        for (const auto& p : {QVector3D(x0, y0, 0.0f), QVector3D(x1, y1, 0.0f)}) {
            vertices.insert(vertices.end(), {p.x(), p.y(), p.z(), color.x(), color.y(), color.z()});
        }
    };

    const float half = kPlateSize * 0.5f;
    const QVector3D minor(0.80f, 0.81f, 0.84f);
    const QVector3D major(0.62f, 0.64f, 0.68f);
    const QVector3D xAxis(0.85f, 0.25f, 0.25f);
    const QVector3D yAxis(0.25f, 0.65f, 0.30f);

    const int steps = static_cast<int>(kPlateSize / kMinorStep);
    for (int i = 0; i <= steps; ++i) {
        const float v = -half + static_cast<float>(i) * kMinorStep;
        if (qFuzzyIsNull(v))
            continue;  // the axes are drawn separately
        const bool isMajor = qFuzzyIsNull(std::fmod(std::abs(v), kMajorStep));
        const QVector3D& c = isMajor ? major : minor;
        addLine(v, -half, v, half, c);
        addLine(-half, v, half, v, c);
    }
    addLine(-half, 0.0f, 0.0f, 0.0f, major);
    addLine(0.0f, -half, 0.0f, 0.0f, major);
    addLine(0.0f, 0.0f, half, 0.0f, xAxis);  // +X in red
    addLine(0.0f, 0.0f, 0.0f, half, yAxis);  // +Y in green

    m_plateVertexCount = static_cast<int>(vertices.size() / 6);

    m_plateVao.create();
    QOpenGLVertexArrayObject::Binder bind(&m_plateVao);
    m_plateBuffer.create();
    m_plateBuffer.bind();
    m_plateBuffer.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    m_plateBuffer.release();
}

void Viewport::upload(GpuMesh& gpu, const std::shared_ptr<const forge::Mesh>& mesh)
{
    if (!gpu.vao) {
        gpu.vao = std::make_unique<QOpenGLVertexArrayObject>();
        gpu.vao->create();
    }
    QOpenGLVertexArrayObject::Binder bind(gpu.vao.get());

    const auto fill = [](QOpenGLBuffer& buffer, const void* source, std::size_t bytes) {
        if (!buffer.isCreated())
            buffer.create();
        buffer.bind();
        buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        buffer.allocate(source, static_cast<int>(bytes));
    };

    fill(gpu.positions, mesh->positions.data(), mesh->positions.size() * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    fill(gpu.normals, mesh->normals.data(), mesh->normals.size() * sizeof(float));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // The index buffer binding is stored in the VAO, so bind it while the VAO is bound.
    fill(gpu.indices, mesh->indices.data(), mesh->indices.size() * sizeof(std::uint32_t));

    gpu.indexCount = static_cast<int>(mesh->indices.size());
    gpu.source = mesh;
}

void Viewport::syncGpuMeshes()
{
    if (!m_itemsDirty)
        return;
    m_itemsDirty = false;

    // Drop GPU copies of items that are gone.
    for (auto it = m_gpu.begin(); it != m_gpu.end();) {
        const bool stillShown = std::any_of(m_items.begin(), m_items.end(),
                                            [&](const DisplayItem& item) { return item.id == it->first; });
        if (stillShown) {
            ++it;
        } else {
            it->second->destroy();
            it = m_gpu.erase(it);
        }
    }

    // Upload new or changed meshes; unchanged ones keep their buffers.
    for (const DisplayItem& item : m_items) {
        if (!item.mesh)
            continue;
        auto& slot = m_gpu[item.id];
        if (!slot)
            slot = std::make_unique<GpuMesh>();
        if (slot->source != item.mesh)
            upload(*slot, item.mesh);
    }
}

void Viewport::paintGL()
{
    glClearColor(kBackground.x(), kBackground.y(), kBackground.z(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_glReady)
        return;

    syncGpuMeshes();

    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    const QMatrix4x4 view = m_camera.viewMatrix();
    const QMatrix4x4 viewProj = m_camera.projectionMatrix(aspect) * view;

    // Build plate
    m_lineShader->bind();
    m_lineShader->setUniformValue("uMvp", viewProj);
    {
        QOpenGLVertexArrayObject::Binder bind(&m_plateVao);
        glDrawArrays(GL_LINES, 0, m_plateVertexCount);
    }
    m_lineShader->release();

    // Bodies
    m_meshShader->bind();
    for (const DisplayItem& item : m_items) {
        const auto found = m_gpu.find(item.id);
        if (found == m_gpu.end() || found->second->indexCount == 0)
            continue;
        const QMatrix4x4 modelView = view * item.model;
        m_meshShader->setUniformValue("uMvp", viewProj * item.model);
        m_meshShader->setUniformValue("uModelView", modelView);
        m_meshShader->setUniformValue("uNormalMatrix", modelView.normalMatrix());
        m_meshShader->setUniformValue("uColor", item.color);
        QOpenGLVertexArrayObject::Binder bind(found->second->vao.get());
        glDrawElements(GL_TRIANGLES, found->second->indexCount, GL_UNSIGNED_INT, nullptr);
    }
    m_meshShader->release();

    if (!m_firstFrameDone) {
        m_firstFrameDone = true;
        emit firstFrameRendered();
    }
}

// ---- Navigation -----------------------------------------------------------------

void Viewport::mousePressEvent(QMouseEvent* event)
{
    m_lastMouse = event->position();
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::RightButton && shift))
        m_drag = DragMode::Pan;
    else if (event->button() == Qt::RightButton)
        m_drag = DragMode::Orbit;
    else
        QOpenGLWidget::mousePressEvent(event);
}

void Viewport::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF delta = event->position() - m_lastMouse;
    m_lastMouse = event->position();
    switch (m_drag) {
    case DragMode::Orbit:
        m_camera.orbit(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        update();
        break;
    case DragMode::Pan:
        m_camera.pan(static_cast<float>(delta.x()), static_cast<float>(delta.y()),
                     static_cast<float>(height()));
        update();
        break;
    case DragMode::None:
        QOpenGLWidget::mouseMoveEvent(event);
        break;
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_drag != DragMode::None && !(event->buttons() & (Qt::RightButton | Qt::MiddleButton)))
        m_drag = DragMode::None;
    QOpenGLWidget::mouseReleaseEvent(event);
}

void Viewport::wheelEvent(QWheelEvent* event)
{
    // One notch of a normal mouse wheel = 120 units.
    m_camera.zoom(static_cast<float>(event->angleDelta().y()) / 120.0f);
    update();
    event->accept();
}

void Viewport::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_F:
        fitView();
        break;
    case Qt::Key_Home:
        resetView();
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
    }
}

}  // namespace forge::app
