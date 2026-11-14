// SPDX-License-Identifier: GPL-3.0-or-later
#include "Viewport.h"

#include "ViewCube.h"

#include <QPainter>
#include <QVariantAnimation>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <QtDebug>

#include <algorithm>
#include <cmath>
#include <numbers>
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
uniform float uSelected;  // 0 or 1
uniform float uAlpha;     // 1 = solid, less = see-through (ghost)
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
    // Selection: lighten the body and add a bright cyan glow toward its silhouette.
    // Cyan is not in the body palette, so it reads as "selected" on any colour.
    float rim = pow(1.0 - max(dot(n, toEye), 0.0), 1.5);
    vec3 glow = vec3(0.25, 0.85, 1.0);
    vec3 selectedColor = mix(mix(color, vec3(1.0), 0.30), glow, clamp(0.25 + 0.9 * rim, 0.0, 1.0));
    color = mix(color, selectedColor, uSelected);
    fragColor = vec4(color, uAlpha);
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
    setMouseTracking(true);           // hover highlight on gizmo handles
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
    m_gizmoVao.destroy();
    m_gizmoBuffer.destroy();
    m_ringVao.destroy();
    m_ringBuffer.destroy();
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
        if (!item.mesh || item.ghost || item.alpha < 1.0f)
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

void Viewport::setViewCubeVisible(bool on)
{
    m_viewCube = on;
    m_cubeHover.reset();
    update();
}

QRectF Viewport::viewCubeArea() const
{
    constexpr double kSize = 110.0, kMargin = 8.0;
    return QRectF(width() - kSize - kMargin, kMargin, kSize, kSize);
}

std::optional<int> Viewport::viewCubePieceAt(const QPointF& pixel) const
{
    if (!m_viewCube || !viewCubeArea().contains(pixel))
        return std::nullopt;
    return ViewCube::pieceAt(ViewCube::pieces(m_camera.viewMatrix(), viewCubeArea()), pixel);
}

QRectF Viewport::viewCubeButton(int which) const
{
    const QRectF cube = viewCubeArea();
    constexpr double kButton = 22.0;
    const double gap = 6.0;
    const double left = cube.center().x() - 1.5 * kButton - gap;
    return QRectF(left + which * (kButton + gap), cube.bottom() + 2.0, kButton, kButton);
}

std::optional<int> Viewport::viewCubeButtonAt(const QPointF& pixel) const
{
    if (!m_viewCube)
        return std::nullopt;
    for (int i = 0; i < 3; ++i)
        if (viewCubeButton(i).contains(pixel))
            return i;
    return std::nullopt;
}

void Viewport::turnTo(const QVector3D& lookFrom)
{
    const auto [yaw, pitch] = ViewCube::anglesFor(lookFrom);
    turnToAngles(yaw, pitch);
}

void Viewport::turnToAngles(float yaw, float pitch)
{
    const float fromYaw = m_camera.yawDegrees(), fromPitch = m_camera.pitchDegrees();
    // The short way round.
    float turn = std::fmod(yaw - fromYaw, 360.0f);
    if (turn > 180.0f)
        turn -= 360.0f;
    if (turn < -180.0f)
        turn += 360.0f;
    if (!m_turn) {
        m_turn = new QVariantAnimation(this);
        m_turn->setDuration(350);
        m_turn->setEasingCurve(QEasingCurve::InOutCubic);
    }
    m_turn->stop();
    m_turn->disconnect(this);
    m_turn->setStartValue(0.0);
    m_turn->setEndValue(1.0);
    connect(m_turn, &QVariantAnimation::valueChanged, this, [this, fromYaw, fromPitch, turn, pitch](const QVariant& v) {
        const float t = v.toFloat();
        m_camera.setAngles(fromYaw + turn * t, fromPitch + (pitch - fromPitch) * t);
        update();
    });
    m_turn->start();
}

void Viewport::resetView()
{
    m_camera.reset();
    fitView();
}

// ---- Gizmo ----------------------------------------------------------------------

void Viewport::setGizmo(const std::optional<view::GizmoTarget>& target, view::GizmoMode mode)
{
    m_gizmoTarget = target;
    m_gizmoMode = mode;
    if (!target)
        m_hover = {};
    update();
}

void Viewport::setActiveHandle(const view::GizmoHandle& handle)
{
    m_active = handle;
    update();
}

std::optional<view::Gizmo> Viewport::makeGizmo() const
{
    if (!m_gizmoTarget)
        return std::nullopt;
    return view::Gizmo(m_camera, QSizeF(width(), height()), *m_gizmoTarget, m_gizmoMode);
}

std::optional<QPointF> Viewport::projectToScreen(const QVector3D& point) const
{
    const view::Gizmo g(m_camera, QSizeF(width(), height()), view::GizmoTarget{}, view::GizmoMode::Move);
    return g.project(point);
}

void Viewport::updateHover(const QPointF& pixel)
{
    view::GizmoHandle hover;
    if (const auto gizmo = makeGizmo())
        hover = gizmo->hitTest(pixel);
    if (hover != m_hover) {
        m_hover = hover;
        setCursor(hover.valid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void Viewport::drawGizmo(const QMatrix4x4& viewProj)
{
    const auto gizmo = makeGizmo();
    if (!gizmo)
        return;
    const std::vector<float> tris = gizmo->triangles(m_active.valid() ? m_active : m_hover, m_active);
    if (tris.empty())
        return;

    if (!m_gizmoVao.isCreated()) {
        m_gizmoVao.create();
        QOpenGLVertexArrayObject::Binder bind(&m_gizmoVao);
        m_gizmoBuffer.create();
        m_gizmoBuffer.setUsagePattern(QOpenGLBuffer::StreamDraw);
        m_gizmoBuffer.bind();
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              reinterpret_cast<const void*>(3 * sizeof(float)));
    }
    QOpenGLVertexArrayObject::Binder bind(&m_gizmoVao);
    m_gizmoBuffer.bind();
    m_gizmoBuffer.allocate(tris.data(), static_cast<int>(tris.size() * sizeof(float)));

    // Always on top, two-sided.
    glDisable(GL_DEPTH_TEST);
    m_lineShader->bind();
    m_lineShader->setUniformValue("uMvp", viewProj);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(tris.size() / 6));
    m_lineShader->release();
    glEnable(GL_DEPTH_TEST);
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
    // Meshes that grew while sculpting: all of it again.
    for (std::uint64_t id : m_grown) {
        const auto found = m_gpu.find(id);
        if (found != m_gpu.end() && found->second->source) {
            const auto source = found->second->source;
            upload(*found->second, source);
        }
        m_movedPoints.erase(id);
    }
    m_grown.clear();
    // Points moved by sculpting: just their stretch of the buffers.
    for (const auto& [id, range] : m_movedPoints) {
        const auto found = m_gpu.find(id);
        if (found == m_gpu.end() || !found->second->source || range.first >= range.second)
            continue;
        GpuMesh& gpu = *found->second;
        const auto& mesh = *gpu.source;
        const std::size_t last = std::min<std::size_t>(range.second, mesh.vertexCount());
        if (range.first >= last)
            continue;
        const int offset = int(range.first * 3 * sizeof(float));
        const int bytes = int((last - range.first) * 3 * sizeof(float));
        gpu.positions.bind();
        gpu.positions.write(offset, mesh.positions.data() + 3 * range.first, bytes);
        gpu.normals.bind();
        gpu.normals.write(offset, mesh.normals.data() + 3 * range.first, bytes);
    }
    m_movedPoints.clear();
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

    // Bodies: solid ones first, then see-through parts on top of them.
    m_meshShader->bind();
    enum class Pass { Solid, Translucent, Ghost };
    const auto drawItems = [&](Pass pass) {
        for (const DisplayItem& item : m_items) {
            const Pass mine = item.ghost ? Pass::Ghost : item.alpha < 1.0f ? Pass::Translucent : Pass::Solid;
            if (mine != pass)
                continue;
            const auto found = m_gpu.find(item.id);
            if (found == m_gpu.end() || found->second->indexCount == 0)
                continue;
            const QMatrix4x4 modelView = view * item.model;
            m_meshShader->setUniformValue("uMvp", viewProj * item.model);
            m_meshShader->setUniformValue("uModelView", modelView);
            m_meshShader->setUniformValue("uNormalMatrix", modelView.normalMatrix());
            m_meshShader->setUniformValue("uColor", item.color);
            m_meshShader->setUniformValue("uSelected", item.selected ? 1.0f : 0.0f);
            m_meshShader->setUniformValue("uAlpha", pass == Pass::Ghost ? 0.45f : item.alpha);
            QOpenGLVertexArrayObject::Binder bind(found->second->vao.get());
            glDrawElements(GL_TRIANGLES, found->second->indexCount, GL_UNSIGNED_INT, nullptr);
        }
    };
    drawItems(Pass::Solid);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // See-through items: hidden behind solids, but don't hide each other.
    glDepthMask(GL_FALSE);
    drawItems(Pass::Translucent);
    glDepthMask(GL_TRUE);
    // Ghosts are drawn over everything so a part hidden inside a result can be seen.
    glDisable(GL_DEPTH_TEST);
    drawItems(Pass::Ghost);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_meshShader->release();

    drawGizmo(viewProj);
    drawBrushRing(viewProj);

    if (m_viewCube) {
        QPainter painter(this);
        ViewCube::paint(painter, view, viewCubeArea(), m_cubeHover);
        // Turn left, home, turn right.
        painter.setRenderHint(QPainter::Antialiasing);
        const QString labels[3] = {QStringLiteral("\u21BA"), QStringLiteral("\u2302"), QStringLiteral("\u21BB")};
        for (int i = 0; i < 3; ++i) {
            const QRectF r = viewCubeButton(i);
            const bool hot = m_cubeButtonHover && *m_cubeButtonHover == i;
            painter.setPen(QPen(QColor(0x5a, 0x63, 0x6e), 1.0));
            painter.setBrush(hot ? QColor(0x2b, 0x78, 0xd0, 150) : QColor(0xe9, 0xee, 0xf5));
            painter.drawEllipse(r);
            QFont font = painter.font();
            font.setPixelSize(15);
            painter.setFont(font);
            painter.setPen(QColor(0x2b, 0x2f, 0x33));
            painter.drawText(r, Qt::AlignCenter, labels[i]);
        }
    }

    if (!m_firstFrameDone) {
        m_firstFrameDone = true;
        emit firstFrameRendered();
    }
}

void Viewport::meshPointsChanged(std::uint64_t id, std::uint32_t first, std::uint32_t last)
{
    if (first >= last)
        return;
    auto [it, added] = m_movedPoints.try_emplace(id, first, last);
    if (!added) {
        it->second.first = std::min(it->second.first, first);
        it->second.second = std::max(it->second.second, last);
    }
    update();
}

void Viewport::meshGrew(std::uint64_t id)
{
    m_grown.insert(id);
    update();
}

void Viewport::setBrushRing(const std::optional<BrushRing>& ring)
{
    m_brushRing = ring;
    update();
}

void Viewport::drawBrushRing(const QMatrix4x4& viewProj)
{
    if (!m_brushRing)
        return;
    // A circle in the plane facing the normal, lifted a touch off the surface.
    const QVector3D n = m_brushRing->normal.normalized();
    QVector3D u = QVector3D::crossProduct(n, std::abs(n.z()) < 0.9f ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0));
    u.normalize();
    const QVector3D v = QVector3D::crossProduct(n, u);
    const QVector3D c = m_brushRing->centre + n * (0.02f * m_brushRing->radius);
    constexpr int kSegments = 64;
    std::vector<float> vertices;
    vertices.reserve(kSegments * 6);
    for (int i = 0; i < kSegments; ++i) {
        const float a = 2.0f * std::numbers::pi_v<float> * float(i) / kSegments;
        const QVector3D p = c + (u * std::cos(a) + v * std::sin(a)) * m_brushRing->radius;
        vertices.insert(vertices.end(), {p.x(), p.y(), p.z(), 0.95f, 0.55f, 0.10f});
    }
    if (!m_ringVao.isCreated()) {
        m_ringVao.create();
        QOpenGLVertexArrayObject::Binder bind(&m_ringVao);
        m_ringBuffer.create();
        m_ringBuffer.bind();
        m_ringBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
        m_ringBuffer.allocate(int(kSegments * 6 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              reinterpret_cast<const void*>(3 * sizeof(float)));
    }
    QOpenGLVertexArrayObject::Binder bind(&m_ringVao);
    m_ringBuffer.bind();
    m_ringBuffer.write(0, vertices.data(), int(vertices.size() * sizeof(float)));
    m_lineShader->bind();
    m_lineShader->setUniformValue("uMvp", viewProj);
    glDisable(GL_DEPTH_TEST);  // always visible, even where the surface bulges over it
    glDrawArrays(GL_LINE_LOOP, 0, kSegments);
    glEnable(GL_DEPTH_TEST);
    m_lineShader->release();
}

// ---- Navigation -----------------------------------------------------------------

void Viewport::mousePressEvent(QMouseEvent* event)
{
    m_lastMouse = event->position();
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (event->button() == Qt::LeftButton) {
        if (const auto button = viewCubeButtonAt(event->position())) {
            if (*button == 1)
                turnToAngles(-60.0f, 25.0f);  // the starting view
            else
                turnToAngles(m_camera.yawDegrees() + (*button == 0 ? -90.0f : 90.0f), m_camera.pitchDegrees());
            return;
        }
        if (const auto piece = viewCubePieceAt(event->position())) {
            // A click on the view cube turns the view; nothing else happens.
            const auto all = ViewCube::pieces(m_camera.viewMatrix(), viewCubeArea());
            turnTo(all[std::size_t(*piece)].lookFrom);
            return;
        }
        m_leftPressed = true;
        m_leftDragging = false;
        m_leftPressAt = event->position();
        QVector3D origin, direction;
        m_camera.rayThroughPixel(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()),
                                 static_cast<float>(width()), static_cast<float>(height()), origin, direction);
        view::GizmoHandle handle;
        if (const auto gizmo = makeGizmo())
            handle = gizmo->hitTest(event->position());
        emit leftPressed(origin, direction, handle, event->position(), event->modifiers());
        return;
    }
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

    if (m_leftPressed) {
        if (!m_leftDragging && (event->position() - m_leftPressAt).manhattanLength() > 3.0)
            m_leftDragging = true;
        if (m_leftDragging) {
            QVector3D origin, direction;
            m_camera.rayThroughPixel(static_cast<float>(event->position().x()),
                                     static_cast<float>(event->position().y()), static_cast<float>(width()),
                                     static_cast<float>(height()), origin, direction);
            emit leftDragged(origin, direction, event->position(), event->modifiers());
        }
        return;
    }

    switch (m_drag) {
    case DragMode::Orbit:
        m_camera.orbit(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        update();
        break;
    case DragMode::Pan:
        m_camera.pan(static_cast<float>(delta.x()), static_cast<float>(delta.y()), static_cast<float>(height()));
        update();
        break;
    case DragMode::None: {
        const auto piece = viewCubePieceAt(event->position());
        const auto button = viewCubeButtonAt(event->position());
        if (piece != m_cubeHover || button != m_cubeButtonHover) {
            m_cubeHover = piece;
            m_cubeButtonHover = button;
            setCursor(piece || button ? Qt::PointingHandCursor : Qt::ArrowCursor);
            setToolTip(button ? (*button == 1 ? tr("Back to the starting view")
                                              : *button == 0 ? tr("Turn the view 90° left") : tr("Turn the view 90° right"))
                              : piece ? tr("Look from this side (or edge, or corner)") : QString());
            update();
        }
        if (piece || button)
            break;  // over the cube: nothing in the scene under it
        updateHover(event->position());
        QVector3D origin, direction;
        m_camera.rayThroughPixel(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()),
                                 static_cast<float>(width()), static_cast<float>(height()), origin, direction);
        emit hovered(origin, direction, event->position());
        QOpenGLWidget::mouseMoveEvent(event);
        break;
    }
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_leftPressed) {
        m_leftPressed = false;
        const bool wasDrag = m_leftDragging;
        m_leftDragging = false;
        emit leftReleased(wasDrag);
        if (!wasDrag) {
            QVector3D origin, direction;
            m_camera.rayThroughPixel(static_cast<float>(event->position().x()),
                                     static_cast<float>(event->position().y()), static_cast<float>(width()),
                                     static_cast<float>(height()), origin, direction);
            emit clicked(origin, direction, event->modifiers());
        }
        updateHover(event->position());
        return;
    }
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
