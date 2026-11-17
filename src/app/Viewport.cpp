// SPDX-License-Identifier: GPL-3.0-or-later
#include "Viewport.h"

#include "Theme.h"

#include "ViewCube.h"

#include <QFontMetricsF>
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
layout(location = 2) in float aShade;
uniform mat4 uMvp;
uniform mat4 uModelView;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;
out vec3 vNormal;
out vec3 vViewPos;
out vec3 vWorldPos;
out float vShade;
void main() {
    vNormal = uNormalMatrix * aNormal;
    vShade = aShade;
    vWorldPos = (uModel * vec4(aPosition, 1.0)).xyz;
    vViewPos = (uModelView * vec4(aPosition, 1.0)).xyz;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

const char* kMeshFragmentShader = R"(#version 330 core
in vec3 vNormal;
in vec3 vViewPos;
in vec3 vWorldPos;
in float vShade;  // sculpting's mask: 1 = masked, drawn dark
uniform vec4 uClip;         // section view: plane normal (plate) and offset
uniform float uClipOn;      // 0 or 1
uniform vec3 uClipNormalView;  // the plane's normal as the camera sees it, towards the cut-away side
uniform vec3 uColor;
uniform float uWarn;      // 1: the shape doesn't fit the build plate
uniform float uSelected;  // 0 or 1
uniform float uAlpha;     // 1 = solid, less = see-through (ghost)
out vec4 fragColor;
void main() {
    if (uClipOn > 0.5 && dot(uClip.xyz, vWorldPos) > uClip.w) discard;
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;               // light the inside of open meshes too
    vec3 toEye = normalize(-vViewPos);
    if (uClipOn > 0.5 && !gl_FrontFacing) {
        // The inside of a solid seen through the cut: drawn as the flat cut face.
        vec3 flatN = normalize(uClipNormalView);
        float lit = 0.70 + 0.25 * max(dot(flatN, normalize(vec3(-0.5, 0.7, 0.6))), 0.0);
        fragColor = vec4(mix(uColor, vec3(0.12, 0.13, 0.16), 0.35) * lit, uAlpha);
        return;
    }
    vec3 key = normalize(vec3(-0.5, 0.7, 0.6)); // light fixed to the camera, upper left
    float diffuse = max(dot(n, key), 0.0);
    float head = max(dot(n, toEye), 0.0);
    float spec = pow(max(dot(n, normalize(key + toEye)), 0.0), 48.0) * 0.20;
    vec3 tinted = mix(uColor, vec3(0.93, 0.33, 0.16), uWarn * 0.75);
    vec3 base = mix(tinted, vec3(0.16, 0.16, 0.20), clamp(vShade, 0.0, 1.0) * 0.8);
    vec3 color = base * (0.28 + 0.62 * diffuse + 0.15 * head) + vec3(spec);
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

constexpr float kMinorStep = 10.0f;
constexpr float kMajorStep = 50.0f;
QVector3D rgb(const QColor& c)
{
    return {float(c.redF()), float(c.greenF()), float(c.blueF())};
}

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
    shade.destroy();
    indices.destroy();
    indexCount = 0;
    source.reset();
}

void Viewport::setItems(std::vector<DisplayItem> items)
{
    m_items = std::move(items);
    m_itemsDirty = true;
    noteSectionShapes();
    checkFit();
    update();
}

void Viewport::themeChanged()
{
    m_plateDirty = true;
    update();
}

void Viewport::setLabels(std::vector<Label> labels)
{
    m_labels = std::move(labels);
    update();
}

void Viewport::setLeftDrag(LeftDrag what)
{
    m_leftNav = what;
    setCursor(restingCursor());
}

Qt::CursorShape Viewport::restingCursor() const
{
    switch (m_leftNav) {
    case LeftDrag::Orbit: return Qt::SizeAllCursor;
    case LeftDrag::Pan: return Qt::OpenHandCursor;
    case LeftDrag::Zoom: return Qt::SizeVerCursor;
    case LeftDrag::Tool: break;
    }
    return Qt::ArrowCursor;
}

void Viewport::setPlateSize(float x, float y)
{
    m_plateX = std::clamp(x, 20.0f, 2000.0f);
    m_plateY = std::clamp(y, 20.0f, 2000.0f);
    m_plateDirty = true;
    checkFit();
    update();
}

void Viewport::setPlateHeight(float z)
{
    m_plateZ = std::clamp(z, 10.0f, 5000.0f);
    m_plateDirty = true;
    checkFit();
    update();
}

void Viewport::setFitWarning(bool on)
{
    m_fitWarning = on;
    checkFit();
    update();
}

void Viewport::checkFit()
{
    std::set<std::uint64_t> off;
    if (m_fitWarning) {
        const float halfX = m_plateX * 0.5f + 0.01f, halfY = m_plateY * 0.5f + 0.01f, top = m_plateZ + 0.01f;
        const auto outside = [&](const QVector3D& p) {
            return std::abs(p.x()) > halfX || std::abs(p.y()) > halfY || p.z() > top;
        };
        for (const DisplayItem& item : m_items) {
            if (!item.mesh || item.ghost || item.alpha < 1.0f || item.mesh->empty())
                continue;
            const Bounds local = item.mesh->bounds();
            if (!local.valid)
                continue;
            // Its box fits: so does the shape. Otherwise look at its points (a turned
            // shape's box reaches further than the shape does).
            bool boxOut = false;
            for (int corner = 0; corner < 8 && !boxOut; ++corner)
                boxOut = outside(item.model.map(QVector3D((corner & 1) ? local.max[0] : local.min[0],
                                                          (corner & 2) ? local.max[1] : local.min[1],
                                                          (corner & 4) ? local.max[2] : local.min[2])));
            if (!boxOut)
                continue;
            bool out = false;
            const auto& pos = item.mesh->positions;
            for (std::size_t i = 0; i + 2 < pos.size() && !out; i += 3)
                out = outside(item.model.map(QVector3D(pos[i], pos[i + 1], pos[i + 2])));
            if (out)
                off.insert(item.id);
        }
    }
    if (off != m_offPlate) {
        const bool countChanged = off.size() != m_offPlate.size();
        m_offPlate = std::move(off);
        if (countChanged)
            emit offPlateChanged(int(m_offPlate.size()));
    }
}

void Viewport::setSection(const std::optional<Section>& section)
{
    m_section = section;
    noteSectionShapes();
    update();
}

void Viewport::noteSectionShapes()
{
    // The selected shapes are the ones cut; with nothing selected, the last ones stay cut.
    std::set<std::uint64_t> selected;
    bool anyLeft = false;
    for (const DisplayItem& item : m_items) {
        if (!item.mesh || item.ghost || item.alpha < 1.0f)
            continue;
        if (item.selected)
            selected.insert(item.id);
        anyLeft = anyLeft || m_sectionIds.count(item.id) > 0;
    }
    if (!selected.empty())
        m_sectionIds = std::move(selected);
    else if (!anyLeft)
        m_sectionIds.clear();
}

Bounds Viewport::sceneBounds(bool cutOnly) const
{
    Bounds all;
    const bool some = cutOnly && std::any_of(m_items.begin(), m_items.end(), [this](const DisplayItem& item) {
        return item.mesh && !item.ghost && item.alpha >= 1.0f && m_sectionIds.count(item.id) > 0;
    });
    for (const DisplayItem& item : m_items) {
        if (!item.mesh || item.ghost || item.alpha < 1.0f)
            continue;
        if (some && !m_sectionIds.count(item.id))
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
    return all;
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
        m_camera.fitSphere(QVector3D(0, 0, 0), std::max(m_plateX, m_plateY) * 0.5f);
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

    const float halfX = m_plateX * 0.5f, halfY = m_plateY * 0.5f;
    const QVector3D minor = rgb(theme::look().gridMinor);
    const QVector3D major = rgb(theme::look().gridMajor);
    const QVector3D edge = rgb(theme::look().plateEdge);
    const QVector3D xAxis(0.85f, 0.25f, 0.25f);
    const QVector3D yAxis(0.25f, 0.65f, 0.30f);

    // Lines every 10 mm counted from the middle (every 50 mm darker), then the plate's edge.
    for (int k = 1; static_cast<float>(k) * kMinorStep < halfX - 1e-3f; ++k) {
        const float v = static_cast<float>(k) * kMinorStep;
        const QVector3D& c = k % static_cast<int>(kMajorStep / kMinorStep) == 0 ? major : minor;
        addLine(v, -halfY, v, halfY, c);
        addLine(-v, -halfY, -v, halfY, c);
    }
    for (int k = 1; static_cast<float>(k) * kMinorStep < halfY - 1e-3f; ++k) {
        const float v = static_cast<float>(k) * kMinorStep;
        const QVector3D& c = k % static_cast<int>(kMajorStep / kMinorStep) == 0 ? major : minor;
        addLine(-halfX, v, halfX, v, c);
        addLine(-halfX, -v, halfX, -v, c);
    }
    addLine(-halfX, -halfY, halfX, -halfY, edge);
    addLine(halfX, -halfY, halfX, halfY, edge);
    addLine(halfX, halfY, -halfX, halfY, edge);
    addLine(-halfX, halfY, -halfX, -halfY, edge);
    // The build height: a faint box standing on the plate.
    {
        const QVector3D faint = rgb(theme::look().buildBox);
        const auto add3 = [&vertices, &faint](float x0, float y0, float z0, float x1, float y1, float z1) {
            vertices.insert(vertices.end(), {x0, y0, z0, faint.x(), faint.y(), faint.z()});
            vertices.insert(vertices.end(), {x1, y1, z1, faint.x(), faint.y(), faint.z()});
        };
        const float z = m_plateZ;
        add3(-halfX, -halfY, z, halfX, -halfY, z);
        add3(halfX, -halfY, z, halfX, halfY, z);
        add3(halfX, halfY, z, -halfX, halfY, z);
        add3(-halfX, halfY, z, -halfX, -halfY, z);
        for (float sx : {-1.0f, 1.0f})
            for (float sy : {-1.0f, 1.0f})
                add3(sx * halfX, sy * halfY, 0.0f, sx * halfX, sy * halfY, z);
    }
    addLine(-halfX, 0.0f, 0.0f, 0.0f, major);
    addLine(0.0f, -halfY, 0.0f, 0.0f, major);
    addLine(0.0f, 0.0f, halfX, 0.0f, xAxis);  // +X in red
    addLine(0.0f, 0.0f, 0.0f, halfY, yAxis);  // +Y in green

    m_plateVertexCount = static_cast<int>(vertices.size() / 6);

    if (!m_plateVao.isCreated())
        m_plateVao.create();
    QOpenGLVertexArrayObject::Binder bind(&m_plateVao);
    if (!m_plateBuffer.isCreated())
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

    // The mask (sculpting), or none: always a buffer, so the attribute is always there.
    if (mesh->shade.size() == mesh->vertexCount()) {
        fill(gpu.shade, mesh->shade.data(), mesh->shade.size() * sizeof(float));
    } else {
        const std::vector<float> none(mesh->vertexCount(), 0.0f);
        fill(gpu.shade, none.data(), none.size() * sizeof(float));
    }
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

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
        // The mask over the same points (zeros if it was cleared).
        const std::size_t count = last - range.first;
        gpu.shade.bind();
        if (mesh.shade.size() == mesh.vertexCount()) {
            gpu.shade.write(int(range.first * sizeof(float)), mesh.shade.data() + range.first,
                            int(count * sizeof(float)));
        } else {
            const std::vector<float> none(count, 0.0f);
            gpu.shade.write(int(range.first * sizeof(float)), none.data(), int(count * sizeof(float)));
        }
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
    {
        const QColor bg = theme::look().view;
        glClearColor(float(bg.redF()), float(bg.greenF()), float(bg.blueF()), 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_glReady)
        return;

    syncGpuMeshes();

    const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
    const QMatrix4x4 view = m_camera.viewMatrix();
    const QMatrix4x4 viewProj = m_camera.projectionMatrix(aspect) * view;

    // Build plate
    if (m_plateDirty) {
        m_plateDirty = false;
        buildPlateGeometry();
    }
    m_lineShader->bind();
    m_lineShader->setUniformValue("uMvp", viewProj);
    {
        QOpenGLVertexArrayObject::Binder bind(&m_plateVao);
        glDrawArrays(GL_LINES, 0, m_plateVertexCount);
    }
    m_lineShader->release();

    // Bodies: solid ones first, then see-through parts on top of them.
    m_meshShader->bind();
    m_meshShader->setUniformValue("uClipOn", m_section ? 1.0f : 0.0f);
    const bool cutSome = m_section && m_section->selectedOnly;
    if (m_section) {
        m_meshShader->setUniformValue("uClip", QVector4D(m_section->normal, m_section->offset));
        m_meshShader->setUniformValue("uClipNormalView", view.mapVector(m_section->normal));
    }
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
            m_meshShader->setUniformValue("uModel", item.model);
            if (cutSome)
                m_meshShader->setUniformValue("uClipOn", m_sectionIds.count(item.id) ? 1.0f : 0.0f);
            m_meshShader->setUniformValue("uNormalMatrix", modelView.normalMatrix());
            m_meshShader->setUniformValue("uColor", item.color);
            m_meshShader->setUniformValue("uWarn", pass == Pass::Solid && m_offPlate.count(item.id) ? 1.0f : 0.0f);
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

    if (!m_labels.empty()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont font = painter.font();
        font.setBold(true);
        painter.setFont(font);
        const QFontMetricsF metrics(font);
        for (const Label& label : m_labels) {
            const auto a = projectToScreen(label.from), b = projectToScreen(label.to);
            if (!a || !b)
                continue;
            painter.setPen(QPen(label.colour, 2.0));
            painter.drawLine(*a, *b);
            painter.setBrush(label.colour);
            painter.drawEllipse(*a, 3.0, 3.0);
            painter.drawEllipse(*b, 3.0, 3.0);
            const QPointF mid = (*a + *b) / 2.0;
            QRectF box(0, 0, metrics.horizontalAdvance(label.text) + 12, metrics.height() + 4);
            box.moveCenter(mid);
            painter.setPen(QPen(label.colour, 1.2));
            painter.setBrush(theme::look().labelFill);
            painter.drawRoundedRect(box, 4, 4);
            painter.setPen(theme::look().labelText);
            painter.drawText(box, Qt::AlignCenter, label.text);
        }
    }

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
        if (m_leftNav != LeftDrag::Tool) {
            // The left button works the view (View tab: Orbit / Pan / Zoom).
            m_drag = m_leftNav == LeftDrag::Orbit ? DragMode::Orbit
                     : m_leftNav == LeftDrag::Pan ? DragMode::Pan
                                                  : DragMode::Zoom;
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
    case DragMode::Zoom:
        // Up the screen zooms in: about a wheel notch every 25 pixels.
        m_camera.zoom(static_cast<float>(-delta.y()) / 25.0f);
        update();
        break;
    case DragMode::None: {
        const auto piece = viewCubePieceAt(event->position());
        const auto button = viewCubeButtonAt(event->position());
        if (piece != m_cubeHover || button != m_cubeButtonHover) {
            m_cubeHover = piece;
            m_cubeButtonHover = button;
            setCursor(piece || button ? Qt::PointingHandCursor : restingCursor());
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
    if (m_drag != DragMode::None && !(event->buttons() & (Qt::RightButton | Qt::MiddleButton | Qt::LeftButton)))
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
