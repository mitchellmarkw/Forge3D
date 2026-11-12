// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Align.h"
#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"
#include "forge/core/Emboss.h"
#include "forge/core/Sketch.h"
#include "forge/core/Split.h"
#include "forge/core/Transform.h"
#include "forge/core/Scale.h"
#include "forge/view/Gizmo.h"

#include <QMainWindow>
#include <QPointF>
#include <QString>
#include <QVector3D>

#include <memory>
#include <set>
#include <vector>
#include <optional>

class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QMenu;
class QActionGroup;
class QWidget;
class QDockWidget;
class QStackedWidget;
class QToolBar;

namespace forge::app {

class PropertiesPanel;
class SplitPanel;
class EmbossPanel;
class EdgesPanel;
class SketchPanel;
class SketchView;
struct DisplayItem;
class Viewport;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Adds a few bodies through the normal commands. For screenshots and demos.
    void loadDemoScene();
    // Opens a file without asking about unsaved changes (start-up argument).
    bool openFile(const QString& path);

signals:
    void firstFrameRendered();

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createDocks();
    void createStatusBar();

    // Runs a command through the undo stack; shows a message if it fails.
    bool run(std::unique_ptr<Command> command);

    // Selection (not part of the undo history, like in most CAD tools). Order matters:
    // for Subtract, the first selected body is the one kept.
    void setSelection(ObjectId id);
    void setSelection(std::vector<ObjectId> ids);
    void toggleSelection(ObjectId id);
    ObjectId selectedOne() const;  // the single selected body, or kInvalidId
    bool isSelected(ObjectId id) const;
    void onViewportClicked(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers);

    // Dragging (gizmo handles, or a body across the plate)
    void onLeftPressed(const QVector3D& origin, const QVector3D& direction, const view::GizmoHandle& handle,
                       const QPointF& pixel, Qt::KeyboardModifiers modifiers);
    void onLeftDragged(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel,
                       Qt::KeyboardModifiers modifiers);
    void onLeftReleased(bool wasDrag);
    void cancelDrag();
    void preview(const Body& state);
    std::optional<view::GizmoTarget> gizmoTargetFor(ObjectId id) const;
    void setGizmoMode(view::GizmoMode mode);

    // Editing
    void addBody(const PrimitiveSpec& spec);
    void deleteSelected();
    void duplicateSelected();
    void copySelected();
    void cutSelected();
    void paste();
    void mirrorSelected(int axis);
    void groupSelected();
    // Handles for the selection: one shape's, or a shared set (move / turn only) at
    // the middle of several shapes on the plate.
    std::optional<view::GizmoTarget> selectionGizmo() const;
    bool multiSelection() const;  // two or more shapes, all on the plate
    void combineSelected(BooleanOp op);
    void alignSelected(AlignMode mode);
    void separateSelected();

    // ---- Split tool ----
    // target: a shape on the plate to cut, or (editing) a Split result whose cut moves.
    void startSplit(ObjectId target, bool editing);
    void endSplit();
    void applySplit();
    void updateSplitPlane();  // after the panel changed: redraw, update side names and range
    SplitPlane currentSplitPlane() const;
    void addSplitItems(std::vector<DisplayItem>& items) const;
    void previewAll(const std::vector<Body>& states);  // like preview(), for several bodies
    float splitPlaneHalfSize() const;  // half the drawn plane's width (mm)
    // Starts dragging the Split plane if the ray hits it or its arrow.
    bool grabSplitPlane(const QVector3D& origin, const QVector3D& direction);
    void clearPlate();

    // ---- Emboss tool ----
    // target: a shape on the plate to put words on, or (editing) an Emboss result.
    void startEmboss(ObjectId target, bool editing);
    void endEmboss();
    void applyEmboss();
    // Puts the words where the ray meets the shape.
    void placeEmboss(const QVector3D& origin, const QVector3D& direction);
    void updateEmbossPreview();
    std::optional<BooleanSpec> currentEmbossSpec() const;
    bool toolOpen() const;  // Split, Emboss or Edges

    // ---- Edges tool (rounding / fillet / chamfer / bevel) ----
    // target: a solid shape on the plate, or (editing) an Edges result.
    void startEdges(ObjectId target, bool editing);
    void endEdges();
    void applyEdges();
    void updateEdgesPreview();
    void edgesHover(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel);
    void edgesClick(const QVector3D& origin, const QVector3D& direction);
    void edgesKindChanged();
    void pickAllEdges();
    std::optional<EdgeFinish> currentEdgeFinish() const;
    std::optional<BooleanSpec> currentEdgesSpec() const;
    void rebuildEdgeHighlights();

    // Sketches (slice 7): drawn flat in the sketch view, then pushed out (Pad) or cut in
    // (Pocket). On the plate, or on a flat face of the selected shape.
    void createSketchBar();
    void sketchClicked();                  // the Sketch button
    void sketchFaceClick(const QVector3D& origin, const QVector3D& direction);
    void openSketch(const sketch::Sketch& sketch, std::vector<std::vector<QPointF>> reference, const QString& title,
                    bool pocketAllowed, bool pocket, double length, bool otherWay, bool bothWays, bool throughAll);
    void startSketchEdit(ObjectId id);     // a pad, or the Merge / Subtract step holding one
    void finishSketch();
    void endSketch();
    void updateSketchStatus();
    void lockForSketch(bool on);  // every other command off while sketching
    void relockForSketch();       // again, after commands were updated
    // The pad a step or body is: the body itself, or a Merge / Subtract step's added shape.
    ObjectId padOf(ObjectId id) const;

    // Files
    void newDocument();
    void openDocument();
    bool save();
    bool saveAs();
    // ---- Export / print ----
    std::vector<ObjectId> exportTargets() const;  // the selected shapes on the plate, or all of them
    void exportShapes();
    void importFiles(const QStringList& paths);
    // A shape with an open (not watertight) imported mesh in it can't be combined or
    // split; says so and returns its name, or returns empty.
    QString openMeshIn(const std::vector<ObjectId>& ids) const;
    void sendToSlicer();
    bool chooseSlicer();  // asks which slicer to use; false if cancelled
    bool saveTo(const QString& path);
    bool confirmDiscardChanges();

    // Keeping the UI in step with the document
    void scheduleSceneRefresh();
    void refreshScene();
    void refreshObjectList();
    // Objects tree: each object, opened, lists its steps oldest first (tools under the
    // step that used them) with the rollback bar after the last step shown.
    void addObjectRow(QTreeWidgetItem* parent, ObjectId id);
    void addStepRows(QTreeWidgetItem* objectRow, ObjectId top);
    void showTreeMenu(const QPoint& pos);
    void editStep(ObjectId id);
    void rollTo(ObjectId top, std::size_t shown);
    void setStepOff(ObjectId id, bool off);
    bool eventFilter(QObject* watched, QEvent* event) override;
    // Where a tool's preview is drawn: a new step on the plate is in plate coordinates;
    // a new step inside a history is in its holder's; an edited step in its own.
    QMatrix4x4 previewFrame(ObjectId target, bool editing) const;
    // True if any selected object is rolled back (combining it is blocked).
    bool rolledBackSelected() const;
    void onTreeSelectionChanged();
    void refreshCommandState();
    void showAbout();

    Document m_doc;
    CommandStack m_stack{m_doc};
    std::vector<ObjectId> m_selection;
    QString m_filePath;  // empty = never saved
    bool m_refreshQueued = false;
    bool m_syncingList = false;

    struct Drag {
        enum class Kind { None, Plate, Arrow, Ring, Face, SplitPlane };
        Kind kind = Kind::None;
        Body before;                  // the body when the drag started
        view::GizmoHandle handle;
        view::GizmoTarget frame;      // gizmo frame when the drag started
        QVector3D grabPoint;          // Plate: where the body was grabbed
        float startParam = 0.0f;      // Arrow / Face: position along the handle's line
        QPointF lastPixel;            // Ring: for accumulating the turn
        QPointF centrePixel;
        double turned = 0.0;          // Ring: degrees so far (unsnapped)
        double startOffset = 0.0;     // SplitPlane: the plane's offset when grabbed
        Transform parentWorld;        // the dragged body's parent on the plate (identity if on the plate)
        // Resizing a combined shape changes its parts: their states when the drag
        // started, a copy of the document then (to compute from), and its size then.
        std::vector<Body> beforeAll;
        bool multi = false;               // moving / turning several shapes together
        std::vector<Vec3> turnCentres;    // multi: the shared middle, in each shape's own coordinates
        std::shared_ptr<Document> snapshot;
        Bounds localBounds;
        QVector3D axis;               // SplitPlane: the plane's direction
        Vec3 turnCentre{};            // Ring: the point it turns about (body coordinates)
        bool moved = false;
    };
    Drag m_dragState;
    bool m_pressOnHandle = false;  // a click on a gizmo handle must not change the selection
    view::GizmoMode m_gizmoMode = view::GizmoMode::Move;
    bool m_snap = true;
    bool m_uniform = false;    // resize keeps proportions
    bool m_keepRound = true;   // cylinder side handles change both radii

    Viewport* m_viewport = nullptr;
    PropertiesPanel* m_properties = nullptr;
    QTreeWidget* m_objects = nullptr;
    std::set<std::pair<int, ObjectId>> m_expanded;  // tree rows left open: (row kind, id)
    ObjectId m_barDrag = kInvalidId;                // the object whose rollback bar is being dragged
    std::size_t m_barDragShown = 0;
    QLabel* m_modelInfo = nullptr;
    QLabel* m_glInfo = nullptr;
    QMenu* m_viewMenu = nullptr;

    QAction* m_new = nullptr;
    QAction* m_open = nullptr;
    QAction* m_save = nullptr;
    QAction* m_saveAs = nullptr;
    QAction* m_quit = nullptr;
    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_deselect = nullptr;
    QMenu* m_addMenu = nullptr;  // Add Shape (toolbar dropdown and Add menu)
    QAction* m_clear = nullptr;
    QAction* m_merge = nullptr;
    QAction* m_subtract = nullptr;
    QAction* m_intersect = nullptr;
    QAction* m_separate = nullptr;
    QAction* m_split = nullptr;
    QAction* m_duplicate = nullptr;
    QAction* m_export = nullptr;
    QAction* m_import = nullptr;
    QAction* m_sendToSlicer = nullptr;
    QAction* m_chooseSlicer = nullptr;
    QAction* m_copy = nullptr;
    QAction* m_cut = nullptr;
    QAction* m_paste = nullptr;
    QAction* m_group = nullptr;
    QMenu* m_mirrorMenu = nullptr;
    std::vector<BodyTree> m_clipboard;  // what Copy / Cut captured
    double m_clipboardLeft = 0.0;       // left edge of what was copied, on the plate
    int m_pasteCount = 0;               // pastes since Copy / Cut (0 right after Cut)
    QWidget* m_propsStack = nullptr;
    QDockWidget* m_propsDock = nullptr;
    int m_propsDockWidth = 300;  // holds Properties and the Split tool's panel (one shown)
    SplitPanel* m_splitPanel = nullptr;
    struct SplitMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;   // the shape being cut, or the result being edited
        ObjectId measured = kInvalidId; // whose size and middle place the plane (the shape, or the result's part)
        Vec3 centre{};                  // middle of `measured` on the plate
    };
    SplitMode m_splitMode;
    QAction* m_emboss = nullptr;
    EmbossPanel* m_embossPanel = nullptr;
    struct EmbossMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;  // the shape being embossed, or the Emboss result being edited
        std::optional<emboss::SurfacePoint> spot;  // where the words go (plate coordinates)
        std::shared_ptr<const Mesh> preview;      // the result as it would be (result coordinates)
        bool wordsOnly = false;                   // big shape: preview draws just the words over it
    };
    EmbossMode m_embossMode;
    EmbossText m_lastWords;          // the words used last, offered next time
    QTimer* m_embossTimer = nullptr;  // previews a moment after typing stops
    QAction* m_edgesAction = nullptr;
    EdgesPanel* m_edgesPanel = nullptr;
    struct EdgesMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;
        TopoDS_Shape shape;  // what is picked on, in plate coordinates
        edges::ShapeEdges found;
        std::shared_ptr<edges::FaceFinder> finder;
        double size = 1.0;  // of the shape (its diagonal), for tolerances
        struct Pick {
            int edge = -1;
            EdgeRef ref;
        };
        std::vector<Pick> picks;
        int hover = -1;  // the edge under the mouse, or -1
        std::shared_ptr<const Mesh> preview;  // the result as it would be (result coordinates)
        std::shared_ptr<const Mesh> pickedLines, hoverLine;
    };
    EdgesMode m_edgesMode;
    QTimer* m_edgesTimer = nullptr;
    QMenu* m_alignMenu = nullptr;  // Align Shapes (toolbar dropdown and Edit submenu)
    QAction* m_fit = nullptr;
    QAction* m_resetView = nullptr;
    QAction* m_modeMove = nullptr;
    QAction* m_modeRotate = nullptr;
    QAction* m_modeResize = nullptr;
    QAction* m_snapAction = nullptr;
    QAction* m_uniformAction = nullptr;

    QStackedWidget* m_central = nullptr;  // the 3D view, or the sketch view while sketching
    SketchView* m_sketchView = nullptr;
    SketchPanel* m_sketchPanel = nullptr;
    QToolBar* m_sketchBar = nullptr;
    QAction* m_sketchAction = nullptr;
    std::vector<QAction*> m_sketchToolActions;  // Select, Line, Rectangle, Circle, Arc, Slot
    std::vector<QAction*> m_lockedActions;      // switched off while sketching (to switch back on)
    struct SketchMode {
        bool picking = false;  // waiting for a click on the face to sketch on
        bool active = false;   // the sketch view is open
        ObjectId target = kInvalidId;  // the shape sketched on (kInvalidId: the plate)
        ObjectId pad = kInvalidId;     // the pad being changed
        ObjectId step = kInvalidId;    // the Merge / Subtract step holding it
        Transform frame;               // sketch -> plate coordinates
    };
    SketchMode m_sketchMode;
};

}  // namespace forge::app
