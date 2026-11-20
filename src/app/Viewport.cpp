// SPDX-License-Identifier: GPL-3.0-or-later
#include "Viewport.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QOpenGLTexture>

#include "forge/core/Layout.h"

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

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
uniform float uWire;      // 1: drawing triangle edges (no lighting)
uniform float uMaskOn;    // 1: painting a mask: two plain colours, evenly lit
uniform vec3 uUnmasked;
uniform vec3 uMasked;
uniform vec3 uOverhang;   // uMaskOn 2 (Print Check): a third colour, for shade 0.5
out vec4 fragColor;
void main() {
    if (uClipOn > 0.5 && dot(uClip.xyz, vWorldPos) > uClip.w) discard;
    if (uWire > 0.5) {
        vec3 warnLine = uWarn > 0.75 ? vec3(0.93, 0.33, 0.16) : vec3(0.96, 0.72, 0.18);
        vec3 line = mix(uColor * 0.85, warnLine, uWarn > 0.0 ? 0.75 : 0.0);
        fragColor = vec4(mix(line, vec3(0.25, 0.85, 1.0), uSelected * 0.8), 1.0);
        return;
    }
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
    if (uMaskOn > 0.5) {
        // Mostly flat light, so a shadow never looks like mask (just enough to see the form).
        float s = clamp(vShade, 0.0, 1.0);
        vec3 paint = mix(uUnmasked, uMasked, s);
        if (uMaskOn > 1.5)
            paint = s > 0.75 ? uMasked : (s > 0.25 ? uOverhang : uUnmasked);
        float light = 0.80 + 0.12 * diffuse + 0.08 * max(dot(n, toEye), 0.0);
        fragColor = vec4(paint * light, uAlpha);
        return;
    }
    float head = max(dot(n, toEye), 0.0);
    float spec = pow(max(dot(n, normalize(key + toEye)), 0.0), 48.0) * 0.20;
    // 1: too big for the printer (orange-red); 0.5: only off the plate (amber).
    vec3 warnColour = uWarn > 0.75 ? vec3(0.93, 0.33, 0.16) : vec3(0.96, 0.72, 0.18);
    vec3 tinted = mix(uColor, warnColour, uWarn > 0.0 ? 0.75 : 0.0);
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

const char* kPictureVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aUv;
uniform mat4 uMvp;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

const char* kPictureFragmentShader = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uPicture;
uniform float uOpacity;
out vec4 fragColor;
void main() {
    vec4 c = texture(uPicture, vUv);
    fragColor = vec4(c.rgb, c.a * uOpacity);
}
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
    m_pictureTextures.clear();
    m_pictureShader.reset();
    m_pictureVao.destroy();
    m_pictureBuffer.destroy();
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

void Viewport::setPlateCount(int count)
{
    count = std::clamp(count, 1, 50);
    if (count == m_plateCount)
        return;
    m_plateCount = count;
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
    std::map<std::uint64_t, int> off;  // 2: too big for the printer; 1: off the plate
    if (m_fitWarning) {
        const float halfX = m_plateX * 0.5f + 0.01f, halfY = m_plateY * 0.5f + 0.01f, top = m_plateZ + 0.01f;
        float plateMiddle = 0.0f;  // of the plate the shape is meant to be on (the nearest)
        const auto outside = [&](const QVector3D& p) {
            return std::abs(p.x() - plateMiddle) > halfX || std::abs(p.y()) > halfY || p.z() > top;
        };
        const float pitch = m_plateX + float(forge::kPlateGap);
        for (const DisplayItem& item : m_items) {
            if (!item.mesh || item.ghost || item.helper || item.alpha < 1.0f || item.mesh->empty())
                continue;
            const Bounds local = item.mesh->bounds();
            if (!local.valid)
                continue;
            {
                const QVector3D middle = item.model.map(QVector3D((local.min[0] + local.max[0]) / 2,
                                                                  (local.min[1] + local.max[1]) / 2, 0.0f));
                const int k = std::clamp(int(std::lround(middle.x() / pitch)), 0, m_plateCount - 1);
                plateMiddle = float(k) * pitch;
            }
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
            QVector3D lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
            const auto& pos = item.mesh->positions;
            for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
                const QVector3D p = item.model.map(QVector3D(pos[i], pos[i + 1], pos[i + 2]));
                out = out || outside(p);
                lo = QVector3D(std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z()));
                hi = QVector3D(std::max(hi.x(), p.x()), std::max(hi.y(), p.y()), std::max(hi.z(), p.z()));
            }
            if (!out)
                continue;
            // Too big for the printer, or just not on the plate (it would fit if moved,
            // turned round on the plate if need be).
            const QVector3D s = hi - lo;
            const float x = m_plateX + 0.02f, y = m_plateY + 0.02f;
            const bool wouldFit = ((s.x() <= x && s.y() <= y) || (s.y() <= x && s.x() <= y)) &&
                                  s.z() - std::max(0.0f, -lo.z()) <= m_plateZ + 0.01f;
            off[item.id] = wouldFit ? 1 : 2;
        }
    }
    if (off != m_offPlate) {
        m_offPlate = std::move(off);
        int big = 0, away = 0;
        for (const auto& [id, kind] : m_offPlate)
            (kind == 2 ? big : away)++;
        emit offPlateChanged(big, away);
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
        if (!item.mesh || item.ghost || item.helper || item.alpha < 1.0f)
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
        if (!item.mesh || item.ghost || item.helper || item.alpha < 1.0f)
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
        if (!item.mesh || (!item.fitted && (item.ghost || item.helper || item.alpha < 1.0f)))
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
    m_pictureShader = buildProgram(kPictureVertexShader, kPictureFragmentShader, "picture");
    if (!m_meshShader || !m_lineShader) {
        emit glInfoReady(summary + QStringLiteral(" - shader error, see log"));
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    // Wireframe needs glPolygonMode, which OpenGL ES lacks (Qt's common functions leave it out).
    m_gl33 = context()->isOpenGLES() ? nullptr
                                     : QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(context());

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
    // The other plates (Lay Out): their edges and 50 mm lines.
    for (int k = 1; k < m_plateCount; ++k) {
        const float cx = float(k) * (m_plateX + float(forge::kPlateGap));
        for (int j = 1; static_cast<float>(j) * kMajorStep < halfX - 1e-3f; ++j) {
            const float v = static_cast<float>(j) * kMajorStep;
            addLine(cx + v, -halfY, cx + v, halfY, minor);
            addLine(cx - v, -halfY, cx - v, halfY, minor);
        }
        for (int j = 1; static_cast<float>(j) * kMajorStep < halfY - 1e-3f; ++j) {
            const float v = static_cast<float>(j) * kMajorStep;
            addLine(cx - halfX, v, cx + halfX, v, minor);
            addLine(cx - halfX, -v, cx + halfX, -v, minor);
        }
        addLine(cx - halfX, -halfY, cx + halfX, -halfY, edge);
        addLine(cx + halfX, -halfY, cx + halfX, halfY, edge);
        addLine(cx + halfX, halfY, cx - halfX, halfY, edge);
        addLine(cx - halfX, halfY, cx - halfX, -halfY, edge);
        addLine(cx - halfX, 0.0f, cx + halfX, 0.0f, major);
        addLine(cx, -halfY, cx, halfY, major);
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

void Viewport::upload(GpuMesh& gpu, const std::shared_ptr<const forge::Mesh>& mesh, bool headroom)
{
    if (!gpu.vao) {
        gpu.vao = std::make_unique<QOpenGLVertexArrayObject>();
        gpu.vao->create();
    }
    QOpenGLVertexArrayObject::Binder bind(gpu.vao.get());

    // Sculpting: room for a quarter more (detail added under the brush), so new points
    // and triangles can be sent on their own.
    const double grow = headroom ? 1.25 : 1.0;
    gpu.pointRoom = std::size_t(double(mesh->vertexCount()) * grow) + (headroom ? 4096 : 0);
    gpu.triangleRoom = std::size_t(double(mesh->triangleCount()) * grow) + (headroom ? 8192 : 0);
    const auto fill = [headroom](QOpenGLBuffer& buffer, const void* source, std::size_t bytes, std::size_t room) {
        if (!buffer.isCreated())
            buffer.create();
        buffer.bind();
        buffer.setUsagePattern(headroom ? QOpenGLBuffer::DynamicDraw : QOpenGLBuffer::StaticDraw);
        if (room <= bytes) {
            buffer.allocate(source, static_cast<int>(bytes));
        } else {
            buffer.allocate(static_cast<int>(room));
            if (bytes > 0)
                buffer.write(0, source, static_cast<int>(bytes));
        }
    };
    const std::size_t pointBytes = gpu.pointRoom * 3 * sizeof(float);

    fill(gpu.positions, mesh->positions.data(), mesh->positions.size() * sizeof(float), pointBytes);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    fill(gpu.normals, mesh->normals.data(), mesh->normals.size() * sizeof(float), pointBytes);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // The mask (sculpting), or none: always a buffer, so the attribute is always there.
    if (mesh->shade.size() == mesh->vertexCount()) {
        fill(gpu.shade, mesh->shade.data(), mesh->shade.size() * sizeof(float), gpu.pointRoom * sizeof(float));
    } else {
        const std::vector<float> none(std::max(mesh->vertexCount(), gpu.pointRoom), 0.0f);
        fill(gpu.shade, none.data(), none.size() * sizeof(float), gpu.pointRoom * sizeof(float));
    }
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);

    // The index buffer binding is stored in the VAO, so bind it while the VAO is bound.
    fill(gpu.indices, mesh->indices.data(), mesh->indices.size() * sizeof(std::uint32_t),
         gpu.triangleRoom * 3 * sizeof(std::uint32_t));

    gpu.indexCount = static_cast<int>(mesh->indices.size());
    gpu.source = mesh;
}

void Viewport::syncGpuMeshes()
{
    // Meshes changed throughout while sculpting: all of it again (with room to grow).
    for (std::uint64_t id : m_grown) {
        const auto found = m_gpu.find(id);
        if (found != m_gpu.end() && found->second->source) {
            const auto source = found->second->source;
            upload(*found->second, source, true);
        }
        m_changes.erase(id);
    }
    m_grown.clear();
    // Parts changed by sculpting: just those runs of the buffers.
    const auto merged = [](std::vector<std::pair<std::uint32_t, std::uint32_t>> runs, std::size_t end) {
        std::sort(runs.begin(), runs.end());
        std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
        for (auto [a, b] : runs) {
            b = std::uint32_t(std::min<std::size_t>(b, end));
            if (a >= b)
                continue;
            if (!out.empty() && a <= out.back().second)
                out.back().second = std::max(out.back().second, b);
            else
                out.push_back({a, b});
        }
        return out;
    };
    for (auto& [id, changes] : m_changes) {
        const auto found = m_gpu.find(id);
        if (found == m_gpu.end() || !found->second->source)
            continue;
        GpuMesh& gpu = *found->second;
        const auto source = gpu.source;
        const auto& mesh = *source;
        if (mesh.vertexCount() > gpu.pointRoom || mesh.triangleCount() > gpu.triangleRoom) {
            upload(gpu, source, true);  // outgrew its room: all again, with more
            continue;
        }
        const bool masked = mesh.shade.size() == mesh.vertexCount();
        for (const auto& [first, last] : merged(changes.points, mesh.vertexCount())) {
            const int offset = int(first * 3 * sizeof(float));
            const int bytes = int((last - first) * 3 * sizeof(float));
            gpu.positions.bind();
            gpu.positions.write(offset, mesh.positions.data() + 3 * std::size_t(first), bytes);
            gpu.normals.bind();
            gpu.normals.write(offset, mesh.normals.data() + 3 * std::size_t(first), bytes);
            // The mask over the same points (zeros if it was cleared).
            const std::size_t count = last - first;
            gpu.shade.bind();
            if (masked) {
                gpu.shade.write(int(first * sizeof(float)), mesh.shade.data() + first, int(count * sizeof(float)));
            } else {
                const std::vector<float> none(count, 0.0f);
                gpu.shade.write(int(first * sizeof(float)), none.data(), int(count * sizeof(float)));
            }
        }
        const auto triangles = merged(changes.triangles, mesh.triangleCount());
        if (!triangles.empty()) {
            QOpenGLVertexArrayObject::Binder bind(gpu.vao.get());
            gpu.indices.bind();
            for (const auto& [first, last] : triangles)
                gpu.indices.write(int(std::size_t(first) * 3 * sizeof(std::uint32_t)),
                                  mesh.indices.data() + 3 * std::size_t(first),
                                  int(std::size_t(last - first) * 3 * sizeof(std::uint32_t)));
        }
        gpu.indexCount = int(mesh.indices.size());
    }
    m_changes.clear();
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

void Viewport::setLook(Look look)
{
    if (m_look == look)
        return;
    m_look = look;
    update();
}

void Viewport::paintGL()
{
    {
        const QColor bg = theme::look().view;
        glClearColor(float(bg.redF()), float(bg.greenF()), float(bg.blueF()), 1.0f);
    }
    // QPainter (labels, the view cube) leaves its own state behind at the end of each
    // frame, depth testing off among it: set what the 3D drawing needs every time.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
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
    m_meshShader->setUniformValue("uWire", 0.0f);
    const bool cutSome = m_section && m_section->selectedOnly;
    if (m_section) {
        m_meshShader->setUniformValue("uClip", QVector4D(m_section->normal, m_section->offset));
        m_meshShader->setUniformValue("uClipNormalView", view.mapVector(m_section->normal));
    }
    enum class Pass { Solid, Translucent, Ghost };
    const auto drawItems = [&](Pass pass) {
        for (const DisplayItem& item : m_items) {
            const bool seeThrough = m_look == Look::SeeThrough && !item.ghost && item.alpha >= 1.0f;
            const Pass mine = item.ghost                               ? Pass::Ghost
                              : item.alpha < 1.0f || seeThrough ? Pass::Translucent
                                                                       : Pass::Solid;
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
            m_meshShader->setUniformValue("uMaskOn", pass == Pass::Ghost ? 0.0f
                                                     : item.checkColours ? 2.0f
                                                     : item.maskColours  ? 1.0f
                                                                         : 0.0f);
            m_meshShader->setUniformValue("uOverhang", item.overhang);
            m_meshShader->setUniformValue("uUnmasked", item.unmasked);
            m_meshShader->setUniformValue("uMasked", item.masked);
            {
                const auto warn = m_offPlate.find(item.id);
                m_meshShader->setUniformValue("uWarn", pass == Pass::Ghost || item.alpha < 1.0f || warn == m_offPlate.end() ? 0.0f
                                                       : warn->second == 2                    ? 1.0f
                                                                                              : 0.5f);
            }
            m_meshShader->setUniformValue("uSelected", item.selected ? 1.0f : 0.0f);
            m_meshShader->setUniformValue("uAlpha", pass == Pass::Ghost ? 0.45f : seeThrough ? 0.42f : item.alpha);
            QOpenGLVertexArrayObject::Binder bind(found->second->vao.get());
            const bool wire = m_look == Look::Wireframe && pass == Pass::Solid && m_gl33;
            if (wire) {
                m_meshShader->setUniformValue("uWire", 1.0f);
                m_gl33->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glDrawElements(GL_TRIANGLES, found->second->indexCount, GL_UNSIGNED_INT, nullptr);
                m_gl33->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                m_meshShader->setUniformValue("uWire", 0.0f);
            } else if (seeThrough) {
                // The far side first, then the near side over it, so the shape reads as a shape.
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                glFrontFace(GL_CW);
                glDrawElements(GL_TRIANGLES, found->second->indexCount, GL_UNSIGNED_INT, nullptr);
                glFrontFace(GL_CCW);
                glDrawElements(GL_TRIANGLES, found->second->indexCount, GL_UNSIGNED_INT, nullptr);
                glDisable(GL_CULL_FACE);
            } else {
                glDrawElements(GL_TRIANGLES, found->second->indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }
    };
    drawItems(Pass::Solid);
    m_meshShader->release();
    drawPictures(viewProj);
    m_meshShader->bind();
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
    m_changes[id].points.push_back({first, last});
    update();
}

void Viewport::meshChanged(std::uint64_t id, const MeshChanges& changes)
{
    if (changes.everything) {
        meshGrew(id);
        return;
    }
    if (changes.points.empty() && changes.triangles.empty())
        return;
    MeshChanges& waiting = m_changes[id];
    waiting.points.insert(waiting.points.end(), changes.points.begin(), changes.points.end());
    waiting.triangles.insert(waiting.triangles.end(), changes.triangles.begin(), changes.triangles.end());
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

QImage Viewport::readPicture(const QByteArray& file)
{
    QByteArray copy = file;  // QBuffer wants something it may write to
    QBuffer buffer(&copy);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);  // a phone photo taken sideways shows the right way up
    return reader.read();
}

void Viewport::setPictures(std::vector<Picture> pictures)
{
    m_pictures = std::move(pictures);
    update();
}

void Viewport::drawPictures(const QMatrix4x4& viewProj)
{
    if (!m_pictureShader)
        return;
    // Let go of pictures no longer shown.
    for (auto it = m_pictureTextures.begin(); it != m_pictureTextures.end();) {
        const bool used = std::any_of(m_pictures.begin(), m_pictures.end(),
                                      [&it](const Picture& p) { return p.file.get() == it->first; });
        it = used ? std::next(it) : m_pictureTextures.erase(it);
    }
    if (m_pictures.empty())
        return;
    const QVector3D eye = m_camera.eyePosition();
    if (!m_pictureVao.isCreated()) {
        m_pictureVao.create();
        QOpenGLVertexArrayObject::Binder bind(&m_pictureVao);
        m_pictureBuffer.create();
        m_pictureBuffer.bind();
        m_pictureBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
        m_pictureBuffer.allocate(int(4 * 5 * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<const void*>(3 * sizeof(float)));
    }
    // See-through, hidden where the model is in front, not hiding what's drawn after.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    m_pictureShader->bind();
    m_pictureShader->setUniformValue("uMvp", viewProj);
    m_pictureShader->setUniformValue("uPicture", 0);
    std::vector<QVector3D> outlines;
    for (const Picture& p : m_pictures) {
        if (!p.file || p.file->empty())
            continue;
        // Seen from its own side only (from behind, it would be back to front).
        if (QVector3D::dotProduct(eye - p.corners[0], p.toward) <= 0.0f)
            continue;
        // Fading out as it turns edge-on, where it would only be a confusing sliver.
        const float facing = -QVector3D::dotProduct(m_camera.forward(), p.toward);
        const float fade = std::clamp((facing - 0.1f) / 0.25f, 0.0f, 1.0f);
        if (fade <= 0.0f)
            continue;
        auto& texture = m_pictureTextures[p.file.get()];
        if (!texture) {
            QImage image = readPicture(QByteArray::fromRawData(p.file->data(), qsizetype(p.file->size())));
            if (image.isNull()) {  // a file Qt can't read: a plain grey card instead
                image = QImage(2, 2, QImage::Format_RGBA8888);
                image.fill(QColor(200, 200, 200));
            }
            // Older graphics chips take up to 4096 pixels a side (some 8192).
            if (image.width() > 4096 || image.height() > 4096)
                image = image.scaled(4096, 4096, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            texture = std::make_shared<QOpenGLTexture>(image.convertToFormat(QImage::Format_RGBA8888).mirrored(),
                                                       QOpenGLTexture::GenerateMipMaps);
            texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            texture->setMagnificationFilter(QOpenGLTexture::Linear);
            texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        }
        // Texture rows run bottom up (the image was flipped to suit).
        const float uv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
        float vertices[4 * 5];
        const int order[4] = {0, 1, 3, 2};  // a strip: TL, TR, BL, BR
        for (int k = 0; k < 4; ++k) {
            const int i = order[k];
            vertices[k * 5 + 0] = p.corners[i].x();
            vertices[k * 5 + 1] = p.corners[i].y();
            vertices[k * 5 + 2] = p.corners[i].z();
            vertices[k * 5 + 3] = uv[i][0];
            vertices[k * 5 + 4] = uv[i][1];
        }
        QOpenGLVertexArrayObject::Binder bind(&m_pictureVao);
        m_pictureBuffer.bind();
        m_pictureBuffer.write(0, vertices, int(sizeof vertices));
        m_pictureShader->setUniformValue("uOpacity", std::clamp(p.opacity, 0.0f, 1.0f) * fade);
        texture->bind(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        texture->release(0);
        if (p.chosen)
            for (int i = 0; i < 4; ++i)
                outlines.insert(outlines.end(), {p.corners[i], p.corners[(i + 1) % 4]});
    }
    m_pictureShader->release();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (!outlines.empty()) {
        // The chosen picture's edge, in the selection blue, over everything.
        std::vector<float> vertices;
        for (const QVector3D& v : outlines)
            vertices.insert(vertices.end(), {v.x(), v.y(), v.z(), 0.17f, 0.47f, 0.82f});
        QOpenGLVertexArrayObject vao;
        vao.create();
        QOpenGLVertexArrayObject::Binder bind(&vao);
        QOpenGLBuffer buffer(QOpenGLBuffer::VertexBuffer);
        buffer.create();
        buffer.bind();
        buffer.allocate(vertices.data(), int(vertices.size() * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<const void*>(3 * sizeof(float)));
        m_lineShader->bind();
        m_lineShader->setUniformValue("uMvp", viewProj);
        glDisable(GL_DEPTH_TEST);
        glDrawArrays(GL_LINES, 0, GLsizei(outlines.size()));
        glEnable(GL_DEPTH_TEST);
        m_lineShader->release();
        buffer.destroy();
        vao.destroy();
    }
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
