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
#include <QColor>
#include <QImage>
#include <QSizeF>
#include <QString>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cstdint>
#include <string>
#include <memory>
#include <map>
#include <set>
#include <optional>
#include <unordered_map>
#include <vector>

class QVariantAnimation;
class QOpenGLTexture;

class QOpenGLFunctions_3_3_Core;

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
    // A tool's helper (a cutting plane, its arrow): not a shape, so never warned about
    // as too big or off the plate, and not counted by Fit.
    bool helper = false;
    // Counted by Fit all the same (the figure guide).
    bool fitted = false;
    // Painting a mask: the mesh's shade drawn as two plain colours, evenly lit (no
    // shadows to mistake for the mask).
    bool maskColours = false;
    QVector3D unmasked{1.0f, 0.82f, 0.10f};
    QVector3D masked{0.08f, 0.08f, 0.10f};
    // Print Check: the shade drawn as three plain colours (0 = unmasked, 0.5 = overhang,
    // 1 = masked), evenly lit. The mesh gives each triangle its own corners.
    bool checkColours = false;
    QVector3D overhang{0.16f, 0.47f, 1.0f};
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
    // The view cube in the top-right corner (shown by default).
    void setViewCubeVisible(bool on);
    bool viewCubeVisible() const { return m_viewCube; }
    // Turns the view smoothly to look from a direction (from the model towards the eye).
    void turnTo(const QVector3D& lookFrom);

    // The gizmo around the selected body (empty = none shown).
    void setGizmo(const std::optional<view::GizmoTarget>& target, view::GizmoMode mode);
    // The handle being dragged, drawn highlighted (none = not dragging).
    void setActiveHandle(const view::GizmoHandle& handle);

    // Sculpting: parts of item `id`'s mesh changed in place (same mesh object): runs of
    // points [first, last) (positions, normals, mask) and of triangles, which may run
    // past what the card has (new ones). Only they are sent, at the next frame, unless
    // `everything`, or the mesh has outgrown the room kept for it on the card.
    struct MeshChanges {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> points;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> triangles;
        bool everything = false;
    };
    void meshChanged(std::uint64_t id, const MeshChanges& changes);
    // Sculpting: points [first, last) of item `id`'s mesh moved in place.
    void meshPointsChanged(std::uint64_t id, std::uint32_t first, std::uint32_t last);
    // Sculpting: item `id`'s mesh changed throughout: sent again in full.
    void meshGrew(std::uint64_t id);
    // A ring showing the brush on the surface (none: hidden).
    struct BrushRing {
        QVector3D centre;
        QVector3D normal;
        float radius = 1.0f;
    };
    void setBrushRing(const std::optional<BrushRing>& ring);

    // What dragging with the left button does: the tool in use (select, move, handles:
    // the usual), or turning, sliding or zooming the view (as the right / middle button
    // and wheel always do).
    enum class LeftDrag { Tool, Orbit, Pan, Zoom };
    void setLeftDrag(LeftDrag what);
    LeftDrag leftDrag() const { return m_leftNav; }

    // The build plate's size (the grid drawn), mm: across (X) and front to back (Y).
    void setPlateSize(float x, float y);
    QSizeF plateSize() const { return {m_plateX, m_plateY}; }
    // How tall the printer can build (a faint box over the plate), mm.
    void setPlateHeight(float z);
    // How many build plates to draw: plate 1 in the middle, the others beside it along
    // +X (Lay Out puts pieces on them). A shape wholly on any of them isn't "off the plate".
    void setPlateCount(int count);
    int plateCount() const { return m_plateCount; }
    float plateHeight() const { return m_plateZ; }
    // Shapes reaching past the plate's edge or above the build height are drawn in a
    // warning colour (on by default): orange-red when too big for the printer, amber
    // when they'd fit if moved onto the plate. offPlateChanged says how many of each.
    void setFitWarning(bool on);

    // Section view: everything on the side of the plane that `normal` points to is left
    // undrawn (where dot(normal, point) > offset, plate mm), and the insides showing
    // through the cut are drawn flat, like a cut face. None: everything is drawn.
    struct Section {
        QVector3D normal{0, 0, 1};
        float offset = 0.0f;
        // Cut only the selected shapes (the ones selected last, when nothing is now);
        // the rest are drawn whole.
        bool selectedOnly = false;
    };
    void setSection(const std::optional<Section>& section);
    const std::optional<Section>& section() const { return m_section; }
    const std::set<std::uint64_t>& sectionShapes() const { return m_sectionIds; }  // for selectedOnly

    // Measurements drawn over the view: a line between two points with words at its middle.
    struct Label {
        QVector3D from, to;
        QString text;
        QColor colour{30, 140, 60};
    };
    void setLabels(std::vector<Label> labels);
    // A lasso being drawn or waiting to be used: its points (plate mm), joined in order,
    // closed back to the first when `closed`. Empty: none.
    void setLasso(std::vector<QVector3D> points, bool closed);
    void themeChanged();  // light / dark: redraw in the new colours
    // How shapes are drawn: solid, see-through (half clear, so what's inside and behind
    // shows), or as their triangles' edges. Parts shown inside combined shapes, the
    // Split plane and other helpers are drawn as before.
    enum class Look { Solid, SeeThrough, Wireframe };
    void setLook(Look look);
    Look look() const { return m_look; }
    // The box round what's shown (ghosts and see-through helpers aside), plate mm.
    // cutOnly: just the shapes a selected-only section cuts (all, if there are none).
    Bounds sceneBounds(bool cutOnly = false) const;

    // Reference pictures, drawn behind the model, each seen from its own side only.
    // Not shapes: never counted by Fit, never warned about.
    struct Picture {
        std::shared_ptr<const std::string> file;  // the picture file (PNG, JPEG...)
        std::array<QVector3D, 4> corners;          // plate mm: top-left, top-right, bottom-right, bottom-left
        QVector3D toward;                          // its front faces this way
        float opacity = 0.6f;
        bool chosen = false;  // outlined (the one being changed)
    };
    void setPictures(std::vector<Picture> pictures);
    // Reads a picture file the way the view shows it (turned as its camera says).
    static QImage readPicture(const QByteArray& file);

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
    void offPlateChanged(int tooBig, int offPlate);
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
    enum class DragMode { None, Orbit, Pan, Zoom };

    // GPU copy of one mesh.
    struct GpuMesh {
        std::shared_ptr<const forge::Mesh> source;
        std::unique_ptr<QOpenGLVertexArrayObject> vao;
        QOpenGLBuffer positions{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer normals{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer shade{QOpenGLBuffer::VertexBuffer};  // one float per point: the mask
        QOpenGLBuffer paint{QOpenGLBuffer::VertexBuffer};  // four floats per point: painted colour, how much
        QOpenGLBuffer indices{QOpenGLBuffer::IndexBuffer};
        int indexCount = 0;
        // Room on the card, in points and triangles (more than the mesh has, for a mesh
        // being sculpted, so detail can be added without sending it all again).
        std::size_t pointRoom = 0, triangleRoom = 0;
        void destroy();
    };

    void buildPlateGeometry();
    void drawGizmo(const QMatrix4x4& viewProj);
    std::optional<view::Gizmo> makeGizmo() const;
    void updateHover(const QPointF& pixel);
    void syncGpuMeshes();
    // `headroom`: keep room for the mesh to grow (sculpting).
    void upload(GpuMesh& gpu, const std::shared_ptr<const forge::Mesh>& mesh, bool headroom = false);
    void releaseGL();
    void drawBrushRing(const QMatrix4x4& viewProj);
    void drawPictures(const QMatrix4x4& viewProj);
    QRectF viewCubeArea() const;
    std::optional<int> viewCubePieceAt(const QPointF& pixel) const;
    // The three buttons under the cube: 0 turn left, 1 home view, 2 turn right.
    QRectF viewCubeButton(int which) const;
    std::optional<int> viewCubeButtonAt(const QPointF& pixel) const;
    void turnToAngles(float yaw, float pitch);

    view::OrbitCamera m_camera;

    // The view cube
    bool m_viewCube = true;
    Look m_look = Look::Solid;
    QOpenGLFunctions_3_3_Core* m_gl33 = nullptr;  // for glPolygonMode (Wireframe); none on OpenGL ES
    std::optional<Section> m_section;
    std::vector<Label> m_labels;
    std::vector<QVector3D> m_lasso;
    bool m_lassoClosed = false;
    std::set<std::uint64_t> m_sectionIds;  // the shapes a selected-only section cuts
    void noteSectionShapes();
    std::optional<int> m_cubeHover;
    std::optional<int> m_cubeButtonHover;
    QVariantAnimation* m_turn = nullptr;

    // Sculpting
    std::map<std::uint64_t, MeshChanges> m_changes;  // sculpting, waiting for the next frame
    std::set<std::uint64_t> m_grown;
    std::optional<BrushRing> m_brushRing;
    QOpenGLVertexArrayObject m_ringVao;
    QOpenGLBuffer m_ringBuffer{QOpenGLBuffer::VertexBuffer};

    // Reference pictures
    std::vector<Picture> m_pictures;
    std::map<const std::string*, std::shared_ptr<QOpenGLTexture>> m_pictureTextures;  // by picture file
    std::unique_ptr<QOpenGLShaderProgram> m_pictureShader;
    QOpenGLVertexArrayObject m_pictureVao;
    QOpenGLBuffer m_pictureBuffer{QOpenGLBuffer::VertexBuffer};

    // Scene
    std::vector<DisplayItem> m_items;
    bool m_itemsDirty = false;
    std::unordered_map<std::uint64_t, std::unique_ptr<GpuMesh>> m_gpu;
    std::unique_ptr<QOpenGLShaderProgram> m_meshShader;

    // Build plate grid
    int m_plateVertexCount = 0;
    float m_plateX = 220.0f, m_plateY = 220.0f;
    bool m_plateDirty = false;
    float m_plateZ = 250.0f;
    int m_plateCount = 1;
    bool m_fitWarning = true;
    std::map<std::uint64_t, int> m_offPlate;
    void checkFit();
    LeftDrag m_leftNav = LeftDrag::Tool;
    Qt::CursorShape restingCursor() const;
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
