// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Align.h"
#include "forge/core/Arrange.h"
#include "forge/core/PrintSplit.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/Edges.h"
#include "forge/core/Emboss.h"
#include "forge/core/Join.h"
#include "forge/core/Measure.h"
#include "forge/core/Sculpt.h"
#include "forge/core/Sketch.h"
#include "forge/core/Split.h"
#include "forge/core/Transform.h"
#include "forge/core/Scale.h"
#include "forge/core/Tolerance.h"
#include "forge/core/PrintCheck.h"
#include "forge/view/Gizmo.h"

#include "SketchPanel.h"
#include "Viewport.h"
#include "forge/core/Picking.h"

#include <QMainWindow>
#include <QPointer>
#include <QPointF>
#include <QString>
#include <QVector3D>

#include <map>
#include <unordered_map>
#include <functional>
#include <memory>

#include <QLockFile>
#include <set>
#include <vector>
#include <optional>

class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QDialog;
class QNetworkAccessManager;
class QTreeWidget;
class QTreeWidgetItem;
class QMenu;
class QActionGroup;
class QWidget;
class QDockWidget;
class QStackedWidget;
class QTabWidget;
class QToolBar;

namespace forge::app {

class PropertiesPanel;
class FitPanel;
class SplitPanel;
class EmbossPanel;
class Tour;
class MeasurePanel;
class SectionPanel;
class SculptPanel;
class EdgesPanel;
class FollowPanel;
class HolePanel;
class PatternPanel;
class ShellPanel;
class ThreadPanel;
class SocketPanel;
class HingePanel;
class PosePanel;
class PrintCheckPanel;
class MannequinPanel;
class HollowPanel;
class StampPanel;
class PanelLinePanel;
class JoinPanel;
class MaskPanel;
class ReferencePanel;
class ScaleCalculator;
class ToleranceDialog;
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
    // Custom shapes (the shape library).
    QString customShapesFolder() const;
    void saveCustomShape();
    void addCustomShape(const QString& file);
    void fillCustomMenu();
    void manageCustomShapes();
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
    void splitToFit(ObjectId target);  // the smart split, after saying what it will do
    QAction* m_splitToFit = nullptr;
    QMenu* m_layOutMenu = nullptr;  // Lay Out: flattest side down, or as they are
    // Lays out the selected shapes on the plates (all the shapes if none are selected).
    void layOutShapes(bool turnOver);
    KeyOptions savedKeyOptions() const;
    void saveKeyOptions(const KeyOptions& options);
    std::array<double, 3> printerVolume() const;
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
    void choosePicture();
    // Lay on Face: the next click on a shape turns it onto that face.
    void layFlatClick(const QVector3D& origin, const QVector3D& direction);
    void sitSelectedOnPlate();
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
    void sketchOnSection();                // the section view's cut
    QAction* m_sketchOnSection = nullptr;
    // Light / dark.
    void setIconNamed(QAction* action, const QString& name);
    void chooseTheme(int choice);  // theme::Choice
    void styleEmptyHint();
    std::vector<std::pair<QAction*, QString>> m_namedIcons;
    void sketchOnPlane(int which, bool atSection = false);         // front / side / top plane through the middle
    bool sketchTargetOk(ObjectId id);      // a shape that can be sketched on (else says why)
    void sketchFaceClick(const QVector3D& origin, const QVector3D& direction);
    void openSketch(const sketch::Sketch& sketch, std::vector<std::vector<QPointF>> reference, const QString& title,
                    bool onFace, const SketchPanel::Options& options);
    void startSketchEdit(ObjectId id);     // a pad, or the Merge / Subtract step holding one
    void finishSketch();
    // Loft and Sweep: keep what's drawn in the stage open; go to another outline (step
    // -1 below, +1 above) or between the outline and the path.
    void storeSketchStage();
    void loftStep(int step);
    void sweepStage(bool path);
    void endSketch();
    void updateSketchStatus();
    void lockForSketch(bool on);  // every other command off while sketching
    void relockForSketch();       // again, after commands were updated
    // The sketched tool (pad, revolve, hole) a body or step is: the body itself, or a
    // Merge / Subtract step's second part.
    ObjectId sketchedToolOf(ObjectId id) const;
    // Adds `tool` and a Merge / Subtract of it with `target` (top-level), as one undo step.
    bool addWithStep(ObjectId target, Body tool, BooleanOp op, bool throughAll, const std::string& label,
                     const std::string& resultName, const Bounds& toolBox);

    // Holes (slice 8): click spots on a flat face; settings in the Hole panel.
    void holeClicked();
    void startHole(ObjectId target, bool editing);
    void holeClick(const QVector3D& origin, const QVector3D& direction);
    HoleSpec currentHole() const;
    void updateHolePreview();
    void showHoleSpots();
    void applyHole();
    void endHole();
    // Follow (slice 10): a groove / ridge along a path drawn on the surface.
    void startFollow(ObjectId target, bool editing);
    void followClick(const QVector3D& origin, const QVector3D& direction);
    void openFollowSketch(bool profile);  // draw the path (or the cross-section)
    FollowFinish currentFollow() const;
    void updateFollowPreview();
    void applyFollow();
    void endFollow();
    // The sketch view is open for a tool (hole spots, a path, a cross-section).
    bool sketchingForTool() const { return m_holeMode.positioning || m_followMode.sketching != 0; }
    // Pattern (slice 11): linked copies of a shape or its last feature.
    void startPattern(ObjectId target, bool editing);
    void updatePatternPreview();
    void applyPattern();
    void endPattern();
    // Shell (slice 8): hollow out, faces clicked to leave open.
    void startShell(ObjectId target, bool editing);
    void shellClick(const QVector3D& origin, const QVector3D& direction);
    std::optional<BooleanSpec> currentShellSpec() const;
    void updateShellPreview();
    void rebuildShellHighlights();
    void applyShell();
    void endShell();

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
    // The objects the selection belongs to (each once), for Colour.
    std::vector<ObjectId> selectedObjects() const;
    void colourSelected(std::optional<std::uint32_t> colour);
    void chooseOtherColour();
    void fillColourMenu(QMenu* menu);
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
    void showQuickStart();
    void showFigureTools();
    QDialog* m_figureTools = nullptr;
    // quietly: at start (a note in the status bar if there's a newer one, nothing
    // otherwise); else from the Help menu, with a message either way.
    void checkForUpdates(bool quietly);
    void updatesAtStart();
    void startTour();
    // The ribbon button for an action on ribbon tab `tab` (nothing if it has none there).
    QWidget* ribbonButtonFor(QAction* action, int tab) const;

    Document m_doc;
    CommandStack m_stack{m_doc};
    std::vector<ObjectId> m_selection;
    QString m_filePath;  // empty = never saved
    bool m_refreshQueued = false;
    bool m_syncingList = false;

    struct Drag {
        enum class Kind { None, Plate, Arrow, Ring, Face, SplitPlane, FitPlane };
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
    QLabel* m_fitInfo = nullptr;

    // Backup copies while working, offered back after a crash.
    void startAutosave();
    void autosaveNow();
    void removeRecovery();
    void offerRecovery();
    bool recoverFrom(const QString& backup);
    QTimer* m_autosaveTimer = nullptr;
    QString m_recoveryPath;                     // this session's backup (.f3d; a .json beside it says what of)
    std::unique_ptr<QLockFile> m_recoveryLock;  // held while this session lives
    quint64 m_docSerial = 0, m_autosavedSerial = 0;
    std::function<void()> m_applyBackupSettings;
    QLabel* m_glInfo = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_recentMenu = nullptr;
    QAction* m_plateSizeAction = nullptr;
    QAction* m_sectionAction = nullptr;
    std::array<QAction*, 3> m_lookActions{};  // Solid, See-Through, Wireframe
    QAction* m_navOrbit = nullptr;
    QAction* m_navPan = nullptr;
    QAction* m_navZoom = nullptr;
    void noteRecentFile(const QString& path);  // empty: just rebuild the menu
    void forgetRecentFile(const QString& path);
    void choosePlateSize();
    void applyPlateSize();

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
    QMenu* m_customMenu = nullptr;  // Add Shape → Custom: the saved custom shapes
    QAction* m_saveCustomShape = nullptr;
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
    QMenu* m_colourMenu = nullptr;  // Colour: swatches, Automatic, More Colours
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
    // Split to Fit, with its cuts shown before they're made (slice 42).
    struct FitMode {
        bool active = false;
        ObjectId target = kInvalidId;
        FitPlan plan;
        int selected = -1;
        Bounds box;  // the model's, when the plan was made: the cuts are placed by it
    };
    FitMode m_fitMode;
    FitPanel* m_fitPanel = nullptr;
    void startFit(ObjectId target);
    void endFit();
    void updateFit();
    void planFit(bool angled);
    void applyFit();
    void addFitItems(std::vector<DisplayItem>& items) const;
    bool grabFitPlane(const QVector3D& origin, const QVector3D& direction);
    QAction* m_emboss = nullptr;
    QAction* m_layFlat = nullptr;
    QLabel* m_emptyHint = nullptr;  // over the view while the plate is empty
    QPointer<QDialog> m_quickStart;
    QPointer<Tour> m_tour;
    QNetworkAccessManager* m_network = nullptr;
    QAction* m_checkAtStart = nullptr;
    QAction* m_sitOnPlate = nullptr;
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
    EmbossPicture m_lastPicture;     // likewise the picture
    bool m_lastUsedPicture = false;  // the last Emboss was a picture
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
        ObjectId pad = kInvalidId;     // the pad (or revolve) being changed
        ObjectId step = kInvalidId;    // the Merge / Subtract step holding it
        Transform frame;               // sketch -> plate coordinates
        int plane = -1;                // a middle plane (0 front, 1 side, 2 top), or -1
        std::vector<std::vector<QPointF>> reference;  // the face's outlines, shown in grey
        LoftSpec loft;                 // Make: Loft: the outlines so far
        int outline = 0;               // the one open
        SweepSpec sweep;               // Make: Sweep: the outline and the path
        bool pathStage = false;        // the path is open (not the outline)
    };
    SketchMode m_sketchMode;
    QMenu* m_sketchMenu = nullptr;
    QTabWidget* m_ribbonTabs = nullptr;     // where to sketch: a face, or a middle plane

    QAction* m_holeAction = nullptr;
    HolePanel* m_holePanel = nullptr;
    QTimer* m_holeTimer = nullptr;
    struct HoleMode {
        bool active = false;
        bool editing = false;
        bool placed = false;           // the face is chosen (frame set)
        ObjectId target = kInvalidId;  // the shape drilled; editing: the Subtract step
        ObjectId tool = kInvalidId;    // editing: the hole body
        Transform frame;               // the face: sketch -> plate coordinates
        TopoDS_Shape shape;            // what is drilled, in plate coordinates
        std::shared_ptr<edges::FaceFinder> finder;
        double size = 1.0;             // the shape's diagonal
        sketch::Sketch spots;          // loose points: the holes
        bool positioning = false;      // the spots are open in the sketch view
        std::shared_ptr<const Mesh> preview;  // the drilled shape (plate coordinates)
    };
    HoleMode m_holeMode;
    QAction* m_shellAction = nullptr;
    ShellPanel* m_shellPanel = nullptr;

    // Sculpting: a step whose strokes are kept and played again on its part. The
    // sculpting happens in the part's own coordinates (toPlate shows it on the plate).
    struct SculptMode {
        bool active = false;
        bool editing = false;         // adding strokes to a Sculpt step
        ObjectId target = kInvalidId;  // the part sculpted, or (editing) the Sculpt step
        ObjectId operand = kInvalidId; // the part the strokes are played on
        ObjectId object = kInvalidId;  // the shape on the plate it shows as
        Transform toPlate;
        Transform fromPlate;
        double size = 0.0;  // the shape's biggest side
        Mesh start;         // the part's surface, divided finely: before any stroke
        std::shared_ptr<sculpt::Sculptor> sculptor;
        std::shared_ptr<const Mesh> display;  // the sculptor's mesh, drawn
        std::vector<SculptStroke> strokes;    // all of them, as kept in the step
        std::vector<SculptPattern> patterns;  // the Pattern brush's, numbered from the strokes
        std::size_t kept = 0;                 // how many the step had already
        // The mesh before each recent stroke, and how many strokes there were then.
        std::vector<std::pair<Mesh, std::size_t>> undo;
        std::uint64_t topology = 0;  // the sculptor's topologyVersion last shown
        bool stroking = false;
        int recording = -1;  // the stroke the dabs go in (-1: none yet this press)
        std::optional<Vec3> lastDab;
        Vec3 middle{0.0, 0.0, 0.0};  // of the starting surface, for the mirror planes
        bool grabbing = false;       // a Grab stroke: pulled across a plane facing the view
        bool snaking = false;        // a Snake hook stroke: likewise, from tip to tip
        Vec3 grabFrom{0.0, 0.0, 0.0};
        Vec3 snakeShift{0.0, 0.0, 0.0};  // from the pressed point to the surface's nearest
        Vec3 grabFacing{0.0, 0.0, 1.0};
        int dabs = 0;
        double dabMs = 0.0;
    };
    SculptMode m_sculptMode;

    // Measuring: things picked on the shapes, and what they measure.
    struct MeasureMode {
        bool active = false;
        std::map<ObjectId, std::shared_ptr<measure::Shape>> shapes;  // solids, ready to pick on
        struct Thing {
            measure::Target target;
            ObjectId object = kInvalidId;
            int index = -1;  // the edge or face number (-1: a point)
            std::shared_ptr<const Mesh> mesh;  // how it's shown picked
        };
        std::optional<Thing> hover;
        std::vector<Thing> picks;  // one or two
        std::shared_ptr<const Mesh> hoverMesh, pickedMesh, lineMesh, centreMesh;
    };
    MeasureMode m_measureMode;
    std::vector<Viewport::Label> m_measureLabels;  // the measurement being shown
    std::vector<Viewport::Label> m_keptLabels;     // kept on screen (until the model changes)
    void showLabels();
    std::optional<PickHit> pickShown(const Ray& ray) const;  // not through a section view's hidden side
    void chooseBackups();
    MeasurePanel* m_measurePanel = nullptr;
    SectionPanel* m_sectionPanel = nullptr;
    QDockWidget* m_sectionDock = nullptr;
    void applySection(bool newRange);
    QAction* m_measureAction = nullptr;
    void startMeasure();
    void endMeasure();
    std::optional<MeasureMode::Thing> measureThingAt(const QVector3D& origin, const QVector3D& direction,
                                                     const QPointF& pixel);
    void measureHover(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel);
    void measureClick(const QVector3D& origin, const QVector3D& direction);
    void showMeasurements();
    SculptPanel* m_sculptPanel = nullptr;
    QAction* m_sculptAction = nullptr;
    QAction* m_simplifyAction = nullptr;
    void simplifySelected();
    QAction* m_smoothAction = nullptr;
    void smoothSelected();
    QAction* m_extrudeDownAction = nullptr;
    void extrudeDownSelected();
    // Adds a step on the selected object (at its rollback bar if it's rolled back).
    // Fills in the spec's operand and pivot; false if it didn't work.
    bool addStepOnSelected(BooleanSpec spec, const std::string& name);
    QAction* m_sculptSmaller = nullptr;
    QAction* m_sculptBigger = nullptr;
    void startSculpt(ObjectId target, bool editing);
    void remeshSculpt();
    std::optional<sculpt::Hit> sculptHit(const QVector3D& origin, const QVector3D& direction) const;
    void sculptHover(const QVector3D& origin, const QVector3D& direction);
    void showBrushRing(const sculpt::Hit& hit, double radius);
    SculptStroke newStroke(int brush, bool invert) const;
    void beginStroke(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers);
    void sculptAt(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers);
    void undoStroke();
    void wholeMask(int brush);  // Clear or Invert mask, kept as a stroke
    void evenOutSculpt();
    // A picture file as grey pixels (an SVG drawn on white); null, with a message, if not.
    std::shared_ptr<const PictureData> loadPictureFile(const QString& path);       // Remesh: even triangles of the panel's size, kept as a stroke
    // Shows what a dab changed: points [first, last), or everything if it added triangles.
    void sculptShown(std::uint32_t first, std::uint32_t last);
    void finishSculpt();
    void endSculpt();
    QTimer* m_shellTimer = nullptr;
    struct ShellMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;
        TopoDS_Shape shape;  // what is picked on, in plate coordinates
        edges::ShapeEdges found;
        std::shared_ptr<edges::FaceFinder> finder;
        struct Pick {
            int face = -1;
            FaceRef ref;
        };
        std::vector<Pick> picks;
        std::shared_ptr<const Mesh> preview;  // result coordinates (like Edges)
        std::shared_ptr<const Mesh> pickedLines;
    };
    ShellMode m_shellMode;

    // Thread (slice 26): a screw thread cut into the round face clicked.
    QAction* m_threadAction = nullptr;
    ThreadPanel* m_threadPanel = nullptr;
    QTimer* m_threadTimer = nullptr;
    struct ThreadMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;
        TopoDS_Shape shape;  // what is picked on, in plate coordinates
        edges::ShapeEdges found;
        std::shared_ptr<edges::FaceFinder> finder;
        int face = -1;  // the round face picked
        FaceRef ref;
        std::shared_ptr<const Mesh> preview;  // result coordinates
        std::shared_ptr<const Mesh> pickedLines;
    };
    ThreadMode m_threadMode;
    void startThread(ObjectId target, bool editing);
    void threadClick(const QVector3D& origin, const QVector3D& direction);
    // Shows the face picked; `suggest`: sets the standard size that fits it.
    void threadPicked(bool suggest);
    std::optional<BooleanSpec> currentThreadSpec() const;
    void updateThreadPreview();
    void applyThread();
    void endThread();

    QAction* m_maskAction = nullptr;
    MaskPanel* m_maskPanel = nullptr;
    struct MaskMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;  // the shape being split, or the Split by Mask step being changed
        ObjectId object = kInvalidId;  // the shape on the plate it's shown as
        ObjectId part = kInvalidId;    // whose surface is painted
        ObjectId hidden = kInvalidId;  // the other piece, out of the way while the mask changes
        Transform toPlate;             // the part's coordinates to the plate
        std::shared_ptr<Mesh> shown;   // the part's surface, the paint as its shade
        std::vector<MaskDab> dabs;     // plate coordinates, in order
        std::vector<std::size_t> strokeStarts;
        bool stroking = false;
        bool erasing = false;
        std::optional<Vec3> lastDab;   // plate coordinates
        double cell = 1.0;             // the shown points filed in a grid (part coordinates)
        std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> grid;
    };
    MaskMode m_maskMode;

    ScaleCalculator* m_scaleCalculator = nullptr;  // Tools → Scale Calculator, made when first opened
    ToleranceDialog* m_toleranceDialog = nullptr;  // Tools → Tolerance Test Print
    void addToleranceTest(const tolerance::Settings& settings);
    void rescaleDesign();
    void makeKit();
    void showPartsList();

    // Reference Images
    QAction* m_referenceAction = nullptr;
    QAction* m_showPicturesAction = nullptr;  // all pictures on or off (the view only)
    ReferencePanel* m_referencePanel = nullptr;
    struct ReferenceMode {
        bool active = false;
        int picking = 0;  // sizing from two points: 0 no, 1 first point, 2 second
        std::array<double, 2> first{0, 0};  // the first point's pixel
        Vec3 firstAt{0, 0, 0};              // and where it is, plate mm
        bool dragging = false;
        RefView dragged = RefView::Front;
        Vec3 grabbed{0, 0, 0};  // where the picture was grabbed, plate mm
        double startAcross = 0, startUp = 0;
    };
    ReferenceMode m_referenceMode;
    void startReferences(std::optional<RefView> view = std::nullopt);
    void endReferences();

    // The figure guide (a mannequin to build to)
    QAction* m_mannequinAction = nullptr;
    MannequinPanel* m_mannequinPanel = nullptr;
    bool m_mannequinOpen = false;                    // its panel is showing
    std::shared_ptr<const Mesh> m_mannequinMesh;     // as drawn (none: none, or hidden)
    void startMannequin();
    void endMannequin();
    // Pictures to the view, and the chosen one to the panel.
    void refreshReferences();
    void referenceEdited(const QString& what, bool dragging);
    void addReferencePicture();
    void removeReferencePicture();
    void referenceClick(const QVector3D& origin, const QVector3D& direction);
    bool grabReference(const QVector3D& origin, const QVector3D& direction);
    void dragReference(const QVector3D& origin, const QVector3D& direction);
    // A picture under a click outside the panel (active ones only): opens the panel on it.
    bool referenceClickedOutside(const QVector3D& origin, const QVector3D& direction);
    void startMask(ObjectId target, bool editing);
    std::optional<emboss::SurfacePoint> maskHit(const QVector3D& origin, const QVector3D& direction) const;
    void maskAt(const QVector3D& origin, const QVector3D& direction);
    void maskHover(const QVector3D& origin, const QVector3D& direction);
    // Paints one dab onto the shown surface (and records it).
    void maskDab(const Vec3& centre, double radius, bool erase);
    // The shade worked out again from all the dabs.
    void repaintMask();
    void undoMaskStroke();
    void applyMask();
    void endMask();

    QAction* m_joinAction = nullptr;
    JoinPanel* m_joinPanel = nullptr;
    struct JoinMode {
        bool active = false;
        int picking = 0;  // the next click picks: 0 the part that moves, 1 where it goes, 2 neither
        ObjectId moving = kInvalidId, fixed = kInvalidId;
        std::optional<join::Feature> movingFeature, fixedFeature;
        std::optional<Placement> placed;  // where the moving part would go
    };
    JoinMode m_joinMode;
    void startJoin();
    void joinClick(const QVector3D& origin, const QVector3D& direction);
    void updateJoinPreview();
    void applyJoin();
    void endJoin();

    QAction* m_socketAction = nullptr;
    SocketPanel* m_socketPanel = nullptr;
    QTimer* m_socketTimer = nullptr;
    struct SocketMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;  // the shape getting the socket, or the Socket step being changed
        std::optional<emboss::SurfacePoint> spot;  // the opening (plate coordinates)
        std::shared_ptr<const Mesh> preview;      // the result as it would be (result coordinates)
        std::shared_ptr<const Mesh> cut;          // a big shape: just what's cut away (plate coordinates)
        ObjectId other = kInvalidId;  // a magnet's matching pocket goes in this part (on the plate)
        bool pickingOther = false;
    };
    SocketMode m_socketMode;
    void startSocket(ObjectId target, bool editing);
    void socketClick(const QVector3D& origin, const QVector3D& direction);
    std::optional<BooleanSpec> currentSocketSpec() const;
    void updateSocketPreview();
    void applySocket();
    void endSocket();

    // Hollow (for resin): walls, drain holes and plugs
    QAction* m_hollowAction = nullptr;
    HollowPanel* m_hollowPanel = nullptr;
    struct HollowMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;    // the part to hollow, or the Hollow step being changed
        std::vector<DrainHole> holes;    // plate coordinates
        std::shared_ptr<const Mesh> cut;  // the holes' cutters, plate coordinates (shown red)
    };
    HollowMode m_hollowMode;
    void startHollow(ObjectId target, bool editing);
    void endHollow();
    void hollowClick(const QVector3D& origin, const QVector3D& direction);
    void updateHollowPreview();
    void applyHollow();

    // Surface stamps: rivets, bolts, vents, scales, stitches... clicked or dragged in rows
    QAction* m_stampAction = nullptr;
    StampPanel* m_stampPanel = nullptr;
    struct StampMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;      // the part to stamp, or the Stamp step being changed
        std::vector<StampSpot> spots;      // plate coordinates
        std::vector<std::size_t> groups;   // where each click or row starts in `spots` (Remove Last)
        std::optional<StampSpot> rowStart;  // dragging a row from here
        std::vector<StampSpot> row;         // the row being dragged
        std::optional<MeshSolid> custom;    // a custom stamp's shape (any size; fitted when stamped)
        QString customName;
        std::shared_ptr<const Mesh> shown;  // the stamps, plate coordinates
    };
    StampMode m_stampMode;
    QStringList stampLibrary() const;
    void startStamp(ObjectId target, bool editing);
    void endStamp();
    std::optional<StampSpot> stampSpotAt(const QVector3D& origin, const QVector3D& direction) const;
    void stampClick(const QVector3D& origin, const QVector3D& direction);
    void stampDrag(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers);

    // Straight lines for stamp rows and panel lines: holding Shift keeps a line level
    // (its height on the plate doesn't change), Ctrl keeps it upright (straight up and
    // down as seen from where you're looking). Either way the line stays in a flat
    // plane through where it started, following the surface across it.
    struct StraightPlane {
        Vec3 point{0, 0, 0};
        Vec3 normal{0, 0, 1};
    };
    std::optional<StraightPlane> straightPlane(const std::array<double, 3>& from, Qt::KeyboardModifiers modifiers,
                                               const QVector3D& direction) const;
    // `q` (with the way out there, `n`) brought into the plane and back onto `target`'s
    // surface, keeping in the plane.
    std::optional<emboss::SurfacePoint> onSurfaceInPlane(ObjectId target, const Vec3& q, const Vec3& n,
                                                         const StraightPlane& plane, double radius) const;
    void loadStampShape();
    void updateStampPreview();
    void applyStamp();

    // Panel lines: grooves along lines clicked on the surface
    QAction* m_panelLineAction = nullptr;
    PanelLinePanel* m_panelLinePanel = nullptr;
    struct PanelLineMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;
        std::vector<PanelLine> lines;                  // finished, plate coordinates
        std::vector<PanelPoint> anchors;               // the line being drawn: the points clicked
        std::vector<std::vector<PanelPoint>> pieces;   // and the surface between each and the next
        std::vector<std::optional<StraightPlane>> planes;  // each piece's plane, if drawn straight
        std::shared_ptr<const Mesh> shown;             // the grooves, plate coordinates (red)
    };
    PanelLineMode m_panelLineMode;
    void startPanelLines(ObjectId target, bool editing);
    void endPanelLines();
    void panelLineClick(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers);
    std::optional<PanelPoint> panelPointAt(const QVector3D& origin, const QVector3D& direction) const;
    // The surface from a to b, as points close together (not a; ending on b).
    std::vector<PanelPoint> panelPiece(const PanelPoint& a, const PanelPoint& b,
                                       const std::optional<StraightPlane>& plane = std::nullopt) const;
    PanelLine currentPanelLine(bool closing) const;
    void endPanelLine(bool closing);
    void updatePanelLinePreview();
    void applyPanelLines();
    void addMatchingPart();

    // Pose Test
    QAction* m_poseAction = nullptr;
    PosePanel* m_posePanel = nullptr;
    QTimer* m_poseTimer = nullptr;  // checks for parts hitting once the pose stops changing
    struct PoseMode {
        bool active = false;
        std::vector<pose::Joint> joints;
        pose::Rig rig;
        std::map<ObjectId, std::array<double, 3>> angles;  // by the step that makes the joint
        std::map<ObjectId, Transform> moved;               // each moved part, on top of where it is
        int joint = -1;                                    // chosen (index into joints)
        std::set<ObjectId> hitParts;
        std::shared_ptr<const Mesh> hitMesh;  // where parts overlap (plate coordinates)
        int generation = 0;                   // the pose's version: late answers are dropped
        bool busy = false;                    // finding a range
        int savedPose = -1;                   // the saved pose being shown
        // Dragging a part
        bool dragging = false;
        Vec3 grabbed{0, 0, 0};
        Vec3 dragPivot{0, 0, 0}, dragAxis{0, 0, 1};
        std::array<double, 3> startAngles{0, 0, 0};
        Transform startTurn;
        Vec3 dragViewAxis{0, 0, 1};  // a ball: turned round the line of sight, in its own frame
    };
    PoseMode m_poseMode;
    void startPose();
    void endPose();
    // The joints again (after an undo, say), keeping the angles that still apply.
    void rebuildPose(ObjectId hold = kInvalidId);
    // After the angles change: where the parts go, the panel, and (soon) the hit check.
    void posed(bool checkSoon = true);
    void choosePoseJoint(int index);
    void checkPoseHits();
    void findPoseRange();
    void poseClick(const QVector3D& origin, const QVector3D& direction);
    bool poseGrab(const QVector3D& origin, const QVector3D& direction);
    void poseDrag(const QVector3D& origin, const QVector3D& direction);
    std::vector<pose::PosedPart> posedParts() const;
    void refreshSavedPoses();

    // Print Check: thin walls and overhangs, shown as colours on every part.
    QAction* m_printCheckAction = nullptr;
    PrintCheckPanel* m_printCheckPanel = nullptr;
    struct PrintCheckMode {
        bool active = false;
        struct Part {
            std::shared_ptr<const Mesh> mesh;                 // what was measured
            std::shared_ptr<const std::vector<float>> thick;  // printcheck::faceThickness of it
            std::shared_ptr<const Mesh> shown;                // coloured, for the view
            printcheck::Report report;
        };
        std::map<ObjectId, Part> parts;
        int generation = 0;  // the model's version: late answers are dropped
        int measuring = 0;   // parts being measured
    };
    PrintCheckMode m_printCheckMode;
    void startPrintCheck();
    void endPrintCheck();
    // Measures the parts that changed (in the background), then colours them all.
    void updatePrintCheck();
    void colourPrintCheck();

    // Quick Hinge
    QAction* m_hingeAction = nullptr;
    HingePanel* m_hingePanel = nullptr;
    QTimer* m_hingeTimer = nullptr;
    struct HingeMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;  // the part getting the fork, or the Hinge step being changed
        std::optional<emboss::SurfacePoint> spot;  // on the fork's face (plate coordinates)
        ObjectId other = kInvalidId;  // the part the other half is made from (top level), if any
        bool pickingOther = false;
        std::shared_ptr<const Mesh> preview;  // the result as it would be (result coordinates)
        std::shared_ptr<const Mesh> cut;      // a big shape: just what's cut away (plate coordinates)
        std::shared_ptr<const Mesh> ghost;    // the tab, link, peg and pins (plate coordinates)
    };
    HingeMode m_hingeMode;
    void startHinge(ObjectId target, bool editing);
    void hingeClick(const QVector3D& origin, const QVector3D& direction);
    std::optional<BooleanSpec> currentHingeSpec() const;
    void updateHingePreview();
    void applyHinge();
    void endHinge();
    void addHingePieces();
    // The shapes to print with a joint (tab, link, peg, pins), in place. The pins are as
    // long as the part is wide there.
    // How far a printed pin reaches each way from the spot, to the part's surface ({0, 0}: unknown).
    std::array<double, 2> hingePinSpan(const HingeFinish& f, const emboss::SurfacePoint& spot, ObjectId forkPart) const;
    std::vector<Body> hingePieceBodies(const HingeFinish& f, const emboss::SurfacePoint& spot, bool withTab,
                                       ObjectId forkPart) const;

    QAction* m_followAction = nullptr;
    FollowPanel* m_followPanel = nullptr;
    QTimer* m_followTimer = nullptr;
    struct FollowMode {
        bool active = false;
        bool editing = false;
        bool placed = false;           // the path's spot is chosen
        int sketching = 0;             // the sketch view is open: 1 the path, 2 the cross-section
        ObjectId target = kInvalidId;  // the shape (editing: the Follow step)
        TopoDS_Shape shape;            // what the path goes on, in plate coordinates
        std::shared_ptr<edges::FaceFinder> finder;
        FollowFinish finish;           // plate coordinates (the panel's settings applied on use)
        std::shared_ptr<const Mesh> preview;  // result coordinates (like Edges)
        std::shared_ptr<const Mesh> pathLines;
    };
    FollowMode m_followMode;

    QAction* m_patternAction = nullptr;
    PatternPanel* m_patternPanel = nullptr;
    QTimer* m_patternTimer = nullptr;
    struct PatternMode {
        bool active = false;
        bool editing = false;
        ObjectId target = kInvalidId;  // the shape (editing: the Pattern step)
        std::shared_ptr<const Mesh> preview;  // result coordinates (like Edges)
    };
    PatternMode m_patternMode;
};

}  // namespace forge::app
