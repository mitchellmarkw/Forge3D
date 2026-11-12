// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "BuildInfo.h"
#include "EmbossPanel.h"
#include "Log.h"
#include "PropertiesPanel.h"
#include "SplitPanel.h"
#include "Viewport.h"

#include "forge/core/Emboss.h"
#include "forge/core/Export.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Import.h"
#include "forge/core/Picking.h"
#include "forge/core/Primitives.h"
#include "forge/core/Tessellator.h"
#include "forge/core/Text.h"
#include "forge/core/Transform.h"

#include <QAction>
#include <QActionGroup>
#include <QHeaderView>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QInputDialog>
#include <QProcess>
#include <QKeySequence>
#include <QLabel>
#include <QTreeWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QUrl>
#include <QtDebug>

#include <Standard_Version.hxx>

#include <array>
#include <exception>
#include <filesystem>
#include <variant>

namespace forge::app {

namespace {

// Distinct, print-friendly colours; each body keeps its colour (picked by id).
const std::array<QVector3D, 6> kPalette{
    QVector3D(0.28f, 0.55f, 0.85f),  // blue
    QVector3D(0.93f, 0.55f, 0.20f),  // orange
    QVector3D(0.35f, 0.70f, 0.40f),  // green
    QVector3D(0.62f, 0.45f, 0.80f),  // purple
    QVector3D(0.20f, 0.68f, 0.70f),  // teal
    QVector3D(0.88f, 0.42f, 0.52f),  // rose
};

// Snapping steps (hold Alt while dragging to turn snapping off).
constexpr double kMoveStep = 1.0;   // mm
constexpr double kTurnStep = 15.0;  // degrees
constexpr double kSizeStep = 1.0;   // mm

const QString kFileFilter = QObject::tr("Forge3D model (*.f3d)");
const QString kLastFolderKey = QStringLiteral("files/lastFolder");

// The same transform the core uses for bounds and picking, so what you see is
// exactly what you click.
QMatrix4x4 toQMatrix(const Transform& t)
{
    const auto f = [&t](int r, int c) { return static_cast<float>(t.m[r][c]); };
    return QMatrix4x4(f(0, 0), f(0, 1), f(0, 2), f(0, 3),  //
                      f(1, 0), f(1, 1), f(1, 2), f(1, 3),  //
                      f(2, 0), f(2, 1), f(2, 2), f(2, 3),  //
                      0.0f, 0.0f, 0.0f, 1.0f);
}

double footprintWidth(const PrimitiveSpec& spec)
{
    return 2.0 * halfExtents(spec)[0];
}

std::filesystem::path toPath(const QString& path)
{
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

// Preferences live in HKEY_CURRENT_USER\Software\Forge3D on Windows. (No organisation
// name is set on the app, so its data folder stays %LOCALAPPDATA%\Forge3D.)
QString startFolder()
{
    const QSettings settings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                             QStringLiteral("Forge3D"));
    const QString remembered = settings.value(kLastFolderKey).toString();
    if (!remembered.isEmpty() && QDir(remembered).exists())
        return remembered;
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void rememberFolderOf(const QString& filePath)
{
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                       QStringLiteral("Forge3D"));
    settings.setValue(kLastFolderKey, QFileInfo(filePath).absolutePath());
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    resize(1400, 860);
    setAcceptDrops(true);  // drop STL / 3MF / OBJ (import) or .f3d (open) files on the window

    m_viewport = new Viewport(this);
    setCentralWidget(m_viewport);
    connect(m_viewport, &Viewport::firstFrameRendered, this, &MainWindow::firstFrameRendered);
    connect(m_viewport, &Viewport::clicked, this, &MainWindow::onViewportClicked);
    connect(m_viewport, &Viewport::leftPressed, this, &MainWindow::onLeftPressed);
    connect(m_viewport, &Viewport::leftDragged, this, &MainWindow::onLeftDragged);
    connect(m_viewport, &Viewport::leftReleased, this, &MainWindow::onLeftReleased);

    createActions();
    createMenus();
    createToolBar();
    createDocks();
    createStatusBar();

    m_doc.addListener([this](const Document::Change& change) {
        if (change.kind == Document::ChangeKind::Removed)  // e.g. undo of an Add
            std::erase(m_selection, change.id);
        scheduleSceneRefresh();
    });
    m_stack.setListener([this] { refreshCommandState(); });

    refreshScene();
    refreshCommandState();
    m_viewport->resetView();
}

MainWindow::~MainWindow()
{
    m_stack.setListener({});
}

// ---- Setup ----------------------------------------------------------------------

void MainWindow::createActions()
{
    m_new = new QAction(tr("&New"), this);
    m_new->setShortcut(QKeySequence::New);
    connect(m_new, &QAction::triggered, this, &MainWindow::newDocument);

    m_open = new QAction(tr("&Open..."), this);
    m_open->setShortcut(QKeySequence::Open);
    connect(m_open, &QAction::triggered, this, &MainWindow::openDocument);

    m_save = new QAction(tr("&Save"), this);
    m_save->setShortcut(QKeySequence::Save);
    connect(m_save, &QAction::triggered, this, &MainWindow::save);

    m_import = new QAction(tr("&Import..."), this);
    m_import->setShortcut(Qt::CTRL | Qt::Key_I);
    m_import->setToolTip(tr("Import (Ctrl+I): add shapes from STL, 3MF or OBJ files, repaired for printing. "
                            "You can also drag files onto the window"));
    connect(m_import, &QAction::triggered, this, [this] {
        QSettings settings;
        const QString folder = settings.value(QStringLiteral("import/folder"), startFolder()).toString();
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, tr("Import shapes"), folder,
            tr("3D models (*.stl *.3mf *.obj);;STL (*.stl);;3MF (*.3mf);;OBJ (*.obj)"));
        if (paths.isEmpty())
            return;
        settings.setValue(QStringLiteral("import/folder"), QFileInfo(paths.front()).absolutePath());
        importFiles(paths);
    });

    m_export = new QAction(tr("&Export..."), this);
    m_export->setShortcut(Qt::CTRL | Qt::Key_E);
    m_export->setToolTip(tr("Export (Ctrl+E): save the selected shapes (or everything) as STL, 3MF or OBJ for "
                            "printing or other programs"));
    connect(m_export, &QAction::triggered, this, &MainWindow::exportShapes);
    m_sendToSlicer = new QAction(tr("Send to Slicer"), this);
    m_sendToSlicer->setToolTip(tr("Send to Slicer: open the selected shapes (or everything) in your slicer, "
                                  "ready to print"));
    connect(m_sendToSlicer, &QAction::triggered, this, &MainWindow::sendToSlicer);
    m_chooseSlicer = new QAction(tr("Choose Slicer..."), this);
    m_chooseSlicer->setToolTip(tr("Pick which slicer Send to Slicer opens"));
    connect(m_chooseSlicer, &QAction::triggered, this, [this] { chooseSlicer(); });

    m_saveAs = new QAction(tr("Save &As..."), this);
    m_saveAs->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_saveAs, &QAction::triggered, this, &MainWindow::saveAs);

    m_quit = new QAction(tr("E&xit"), this);
    m_quit->setShortcut(QKeySequence::Quit);
    connect(m_quit, &QAction::triggered, this, &QWidget::close);

    m_undo = new QAction(tr("&Undo"), this);
    m_undo->setShortcut(QKeySequence::Undo);
    connect(m_undo, &QAction::triggered, this, [this] {
        const QString label = QString::fromStdString(m_stack.undoLabel());
        m_stack.undo();
        qInfo().noquote() << "Undo:" << label;
    });

    m_redo = new QAction(tr("&Redo"), this);
    m_redo->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(m_redo, &QAction::triggered, this, [this] {
        const QString label = QString::fromStdString(m_stack.redoLabel());
        m_stack.redo();
        qInfo().noquote() << "Redo:" << label;
    });

    m_delete = new QAction(tr("&Delete"), this);
    m_delete->setShortcut(QKeySequence::Delete);
    m_delete->setToolTip(tr("Delete the selected bodies (Del). Deleting a combined shape deletes its parts too."));
    connect(m_delete, &QAction::triggered, this, &MainWindow::deleteSelected);

    m_duplicate = new QAction(tr("Duplicate"), this);
    m_duplicate->setShortcut(Qt::CTRL | Qt::Key_D);
    m_duplicate->setToolTip(tr("Duplicate (Ctrl+D): copies of the selected shapes, placed to the right of everything on the plate"));
    connect(m_duplicate, &QAction::triggered, this, &MainWindow::duplicateSelected);
    m_copy = new QAction(tr("&Copy"), this);
    m_copy->setShortcut(QKeySequence::Copy);
    connect(m_copy, &QAction::triggered, this, &MainWindow::copySelected);
    m_cut = new QAction(tr("Cu&t"), this);
    m_cut->setShortcut(QKeySequence::Cut);
    connect(m_cut, &QAction::triggered, this, &MainWindow::cutSelected);
    m_paste = new QAction(tr("&Paste"), this);
    m_paste->setShortcut(QKeySequence::Paste);
    m_paste->setToolTip(tr("Paste (Ctrl+V): copies of what you copied, to the right of everything on the plate"));
    connect(m_paste, &QAction::triggered, this, &MainWindow::paste);

    m_group = new QAction(tr("Group"), this);
    m_group->setShortcut(Qt::CTRL | Qt::Key_G);
    m_group->setToolTip(tr("Group (Ctrl+G): the selected shapes move, turn and resize as one, without merging. "
                           "Ungroup with Separate"));
    connect(m_group, &QAction::triggered, this, &MainWindow::groupSelected);

    m_mirrorMenu = new QMenu(tr("Mirror"), this);
    m_mirrorMenu->menuAction()->setToolTip(tr("Mirror: flip the selected shapes in place"));
    m_mirrorMenu->setToolTipsVisible(true);
    const auto addMirror = [this](const QString& text, const QString& tip, int axis) {
        QAction* a = m_mirrorMenu->addAction(text);
        a->setToolTip(tip);
        connect(a, &QAction::triggered, this, [this, axis] { mirrorSelected(axis); });
    };
    addMirror(tr("Left / right (X)"), tr("Flip across X: what was on the left ends up on the right"), 0);
    addMirror(tr("Front / back (Y)"), tr("Flip across Y: what was at the front ends up at the back"), 1);
    addMirror(tr("Upside down (Z)"), tr("Flip across Z: the top ends up at the bottom, still on the plate"), 2);

    m_deselect = new QAction(tr("Select &None"), this);
    m_deselect->setShortcut(Qt::Key_Escape);
    connect(m_deselect, &QAction::triggered, this, [this] {
        if (m_dragState.kind != Drag::Kind::None)
            cancelDrag();  // Esc during a drag puts the body back
        else if (m_splitMode.active)
            endSplit();  // Esc leaves the Split tool without cutting
        else if (m_embossMode.active)
            endEmboss();
        else
            setSelection(std::vector<ObjectId>{});
    });

    auto* modes = new QActionGroup(this);
    m_modeMove = new QAction(tr("Move"), modes);
    m_modeMove->setShortcut(Qt::Key_W);
    m_modeMove->setToolTip(tr("Move tool (W): drag the arrows to move along X, Y or Z"));
    m_modeRotate = new QAction(tr("Rotate"), modes);
    m_modeRotate->setShortcut(Qt::Key_E);
    m_modeRotate->setToolTip(tr("Rotate tool (E): drag a ring to turn around X, Y or Z"));
    m_modeResize = new QAction(tr("Resize"), modes);
    m_modeResize->setShortcut(Qt::Key_R);
    m_modeResize->setToolTip(tr("Resize tool (R): drag a face handle; the opposite face stays put"));
    for (QAction* a : {m_modeMove, m_modeRotate, m_modeResize})
        a->setCheckable(true);
    m_modeMove->setChecked(true);
    connect(m_modeMove, &QAction::triggered, this, [this] { setGizmoMode(view::GizmoMode::Move); });
    connect(m_modeRotate, &QAction::triggered, this, [this] { setGizmoMode(view::GizmoMode::Rotate); });
    connect(m_modeResize, &QAction::triggered, this, [this] { setGizmoMode(view::GizmoMode::Resize); });

    m_snapAction = new QAction(tr("Snap"), this);
    m_snapAction->setCheckable(true);
    m_snapAction->setChecked(true);
    m_snapAction->setToolTip(tr("Snap drags to 1 mm steps and 15° turns. Hold Alt while dragging to "
                                "move freely."));
    connect(m_snapAction, &QAction::toggled, this, [this](bool on) { m_snap = on; });

    m_uniformAction = new QAction(tr("Uniform"), this);
    m_uniformAction->setCheckable(true);
    m_uniformAction->setShortcut(Qt::Key_U);
    m_uniformAction->setToolTip(tr("Uniform scale (U): resizing changes every size by the same factor, "
                                   "keeping the shape's proportions"));
    connect(m_uniformAction, &QAction::toggled, this, [this](bool on) {
        m_uniform = on;
        m_properties->setUniform(on);
        qInfo() << "Uniform scale" << (on ? "on" : "off");
    });

    // Add Shape: one entry per primitive, added beside what is already on the plate.
    m_addMenu = new QMenu(tr("Add Shape"), this);
    m_addMenu->setToolTipsVisible(true);
    m_addMenu->menuAction()->setToolTip(tr("Add a shape beside the existing bodies. Change its size in Properties "
                                           "or with the Resize handles (R)"));
    const std::array<QString, kPrimitiveKindCount> tips{
        tr("A block with flat faces"),
        tr("A round column; untick Round in Properties for an oval one"),
        tr("A ball"),
        tr("A cone with a point on top; give it a top radius to cut the tip off flat"),
        tr("A ring, like a doughnut, lying flat"),
        tr("A pipe: a cylinder with a round hole through it"),
        tr("A ramp: tall at its -X end, sloping down to the plate at +X"),
        tr("A pyramid on a rectangular base; give it a top width and depth for a flat top, or just one for a "
           "roof-like ridge"),
        tr("3D letters lying flat: type the words, pick a font in Properties"),
    };
    for (std::size_t kind = 0; kind < kPrimitiveKindCount; ++kind) {
        const PrimitiveSpec spec = defaultSpec(kind);
        QAction* a = m_addMenu->addAction(QString::fromStdString(typeName(spec)));
        a->setToolTip(tips[kind]);
        a->setObjectName(QStringLiteral("Add") + QString::fromStdString(typeName(spec)));
        connect(a, &QAction::triggered, this, [this, spec] { addBody(spec); });
        if (kind == 1)
            m_addMenu->addSeparator();  // the familiar two first
    }

    m_merge = new QAction(tr("Merge"), this);
    m_merge->setToolTip(tr("Merge: join the selected shapes into one (select two or more; Ctrl+click adds)"));
    connect(m_merge, &QAction::triggered, this, [this] { combineSelected(BooleanOp::Merge); });
    m_subtract = new QAction(tr("Subtract"), this);
    m_subtract->setToolTip(tr("Subtract: keep the FIRST shape you selected and cut the others out of it"));
    connect(m_subtract, &QAction::triggered, this, [this] { combineSelected(BooleanOp::Subtract); });
    m_intersect = new QAction(tr("Intersect"), this);
    m_intersect->setToolTip(tr("Intersect: keep only the space the selected shapes share"));
    connect(m_intersect, &QAction::triggered, this, [this] { combineSelected(BooleanOp::Intersect); });
    m_separate = new QAction(tr("Separate"), this);
    m_separate->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_G);
    m_separate->setToolTip(tr("Separate: break a combined shape back into its parts, where they are"));
    connect(m_separate, &QAction::triggered, this, &MainWindow::separateSelected);

    m_split = new QAction(tr("Split"), this);
    m_split->setToolTip(tr("Split: cut the selected shape in two with a plane, keeping one or both halves"));
    connect(m_split, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Split: select one shape on the plate first."), 6000);
            return;
        }
        if (const QString open = openMeshIn({id}); !open.isEmpty()) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("'%1' has gaps in its surface (it isn't watertight), so it can't be split.")
                                         .arg(open));
            return;
        }
        startSplit(id, false);
    });

    m_emboss = new QAction(tr("Emboss"), this);
    m_emboss->setToolTip(tr("Emboss: put raised or engraved words on the selected shape; they follow its surface"));
    connect(m_emboss, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Emboss: select one shape on the plate first."), 6000);
            return;
        }
        if (const QString open = openMeshIn({id}); !open.isEmpty()) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("'%1' has gaps in its surface (it isn't watertight), so it can't be "
                                        "embossed.").arg(open));
            return;
        }
        startEmboss(id, false);
    });

    // Align Shapes: the first shape selected stays put; the others move to it.
    m_alignMenu = new QMenu(tr("Align Shapes"), this);
    m_alignMenu->setToolTipsVisible(true);
    m_alignMenu->menuAction()->setToolTip(
        tr("Align Shapes: select shape 1, Ctrl+click shape 2, then pick how shape 2 lines up with shape 1"));
    const auto addAlign = [this](AlignMode mode, const QString& tip) {
        QAction* a = m_alignMenu->addAction(QString::fromStdString(toString(mode)));
        a->setToolTip(tip);
        connect(a, &QAction::triggered, this, [this, mode] { alignSelected(mode); });
    };
    addAlign(AlignMode::MeetAtFace, tr("Slide shape 2 until its facing side touches shape 1"));
    m_alignMenu->addSeparator();
    addAlign(AlignMode::FaceCentreXYZ, tr("Put shape 2's centre on the middle of shape 1's facing face"));
    addAlign(AlignMode::FaceCentreX, tr("Along X: put shape 2's centre on shape 1's facing side (halfway in)"));
    addAlign(AlignMode::FaceCentreY, tr("Along Y: put shape 2's centre on shape 1's facing side (halfway in)"));
    addAlign(AlignMode::FaceCentreZ, tr("Along Z: put shape 2's centre on shape 1's top or bottom (halfway in)"));
    m_alignMenu->addSeparator();
    addAlign(AlignMode::CentreXYZ, tr("Put shape 2's centre on shape 1's centre"));
    addAlign(AlignMode::CentreX, tr("Line up the centres along X"));
    addAlign(AlignMode::CentreY, tr("Line up the centres along Y"));
    addAlign(AlignMode::CentreZ, tr("Line up the centres along Z (height)"));

    m_clear = new QAction(tr("Clear Plate"), this);
    m_clear->setToolTip(tr("Delete every body (one Undo brings them all back)"));
    connect(m_clear, &QAction::triggered, this, &MainWindow::clearPlate);

    m_fit = new QAction(tr("&Fit model"), this);
    m_fit->setShortcut(Qt::Key_F);
    connect(m_fit, &QAction::triggered, m_viewport, &Viewport::fitView);

    m_resetView = new QAction(tr("&Reset view"), this);
    m_resetView->setShortcut(Qt::Key_Home);
    connect(m_resetView, &QAction::triggered, m_viewport, &Viewport::resetView);
}

void MainWindow::createMenus()
{
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_new);
    file->addAction(m_open);
    file->addSeparator();
    file->addAction(m_save);
    file->addAction(m_saveAs);
    file->addSeparator();
    file->addAction(m_import);
    file->addAction(m_export);
    file->addAction(m_sendToSlicer);
    file->addAction(m_chooseSlicer);
    file->addSeparator();
    file->addAction(m_quit);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_undo);
    edit->addAction(m_redo);
    edit->addSeparator();
    edit->addAction(m_cut);
    edit->addAction(m_copy);
    edit->addAction(m_paste);
    edit->addAction(m_duplicate);
    edit->addAction(m_delete);
    edit->addAction(m_deselect);
    edit->addSeparator();
    edit->addAction(m_modeMove);
    edit->addAction(m_modeRotate);
    edit->addAction(m_modeResize);
    edit->addAction(m_snapAction);
    edit->addAction(m_uniformAction);
    edit->addSeparator();
    edit->addAction(m_merge);
    edit->addAction(m_subtract);
    edit->addAction(m_intersect);
    edit->addAction(m_separate);
    edit->addAction(m_split);
    edit->addAction(m_emboss);
    edit->addMenu(m_alignMenu);
    edit->addMenu(m_mirrorMenu);
    edit->addAction(m_group);

    QMenu* add = menuBar()->addMenu(tr("&Add"));
    add->addActions(m_addMenu->actions());

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_fit);
    view->addAction(m_resetView);
    view->addSeparator();
    m_viewMenu = view;  // the panels' show/hide entries are added in createDocks()

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("Open &log folder"), this, [] {
        const QString path = logFilePath();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    help->addSeparator();
    help->addAction(tr("&About Forge3D"), this, &MainWindow::showAbout);
}

void MainWindow::createToolBar()
{
    QToolBar* bar = addToolBar(tr("Build"));
    bar->setObjectName(QStringLiteral("BuildToolBar"));
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    bar->addAction(m_open);
    bar->addAction(m_save);
    bar->addAction(m_import);
    bar->addAction(m_export);
    bar->addAction(m_sendToSlicer);
    bar->addSeparator();
    bar->addAction(m_undo);
    bar->addAction(m_redo);
    bar->addSeparator();
    {
        auto* addButton = new QToolButton(bar);
        addButton->setDefaultAction(m_addMenu->menuAction());
        addButton->setPopupMode(QToolButton::InstantPopup);
        addButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        addButton->setObjectName(QStringLiteral("AddShapeButton"));
        bar->addWidget(addButton);
    }
    bar->addSeparator();
    bar->addAction(m_modeMove);
    bar->addAction(m_modeRotate);
    bar->addAction(m_modeResize);
    bar->addAction(m_snapAction);
    bar->addAction(m_uniformAction);
    bar->addSeparator();
    bar->addAction(m_merge);
    bar->addAction(m_subtract);
    bar->addAction(m_intersect);
    bar->addAction(m_separate);
    bar->addAction(m_split);
    bar->addAction(m_emboss);
    bar->addSeparator();
    {
        auto* alignButton = new QToolButton(bar);
        alignButton->setDefaultAction(m_alignMenu->menuAction());
        alignButton->setPopupMode(QToolButton::InstantPopup);
        alignButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        alignButton->setObjectName(QStringLiteral("AlignShapesButton"));
        bar->addWidget(alignButton);
        auto* mirrorButton = new QToolButton(bar);
        mirrorButton->setDefaultAction(m_mirrorMenu->menuAction());
        mirrorButton->setPopupMode(QToolButton::InstantPopup);
        mirrorButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        bar->addWidget(mirrorButton);
    }
    bar->addAction(m_group);
    bar->addAction(m_duplicate);
    bar->addSeparator();
    bar->addAction(m_delete);
    bar->addAction(m_clear);
}

void MainWindow::createDocks()
{
    // Objects tree (left): combined shapes hold their parts.
    m_objects = new QTreeWidget;
    m_objects->setHeaderHidden(true);
    m_objects->setSelectionMode(QAbstractItemView::ExtendedSelection);  // Ctrl+click, Shift+click
    m_objects->setToolTip(tr("Every body. Ctrl+click to select several. Open a combined shape to see "
                             "and edit its parts."));
    connect(m_objects, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelectionChanged);
    connect(m_objects, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
        if (!m_syncingList)
            m_expanded.insert(item->data(0, Qt::UserRole).value<ObjectId>());
    });
    connect(m_objects, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* item) {
        if (!m_syncingList)
            m_expanded.erase(item->data(0, Qt::UserRole).value<ObjectId>());
    });
    auto* objectsDock = new QDockWidget(tr("Objects"), this);
    objectsDock->setObjectName(QStringLiteral("ObjectsDock"));
    objectsDock->setWidget(m_objects);
    objectsDock->setMinimumWidth(200);
    addDockWidget(Qt::LeftDockWidgetArea, objectsDock);

    // Properties (right)
    m_properties = new PropertiesPanel;
    auto* propsDock = new QDockWidget(tr("Properties"), this);
    propsDock->setObjectName(QStringLiteral("PropertiesDock"));
    m_splitPanel = new SplitPanel;
    // Properties and the Split panel share the dock; only one is shown. (Hidden
    // widgets don't count towards the dock's size, unlike pages of a stacked widget.)
    m_propsStack = new QWidget;
    auto* stackLayout = new QVBoxLayout(m_propsStack);
    stackLayout->setContentsMargins(0, 0, 0, 0);
    stackLayout->addWidget(m_properties);
    stackLayout->addWidget(m_splitPanel);
    m_splitPanel->hide();
    m_embossPanel = new EmbossPanel;
    stackLayout->addWidget(m_embossPanel);
    m_embossPanel->hide();
    // Long hint texts wrap instead of widening the dock.
    m_propsStack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    propsDock->setWidget(m_propsStack);
    m_propsDock = propsDock;
    connect(m_splitPanel, &SplitPanel::planeChanged, this, &MainWindow::updateSplitPlane);
    connect(m_splitPanel, &SplitPanel::applyRequested, this, &MainWindow::applySplit);
    connect(m_splitPanel, &SplitPanel::cancelRequested, this, &MainWindow::endSplit);
    m_embossTimer = new QTimer(this);
    m_embossTimer->setSingleShot(true);
    connect(m_embossTimer, &QTimer::timeout, this, &MainWindow::updateEmbossPreview);
    connect(m_embossPanel, &EmbossPanel::changed, this, [this] { m_embossTimer->start(0); });
    connect(m_embossPanel, &EmbossPanel::typing, this, [this] { m_embossTimer->start(400); });
    connect(m_embossPanel, &EmbossPanel::applyRequested, this, &MainWindow::applyEmboss);
    connect(m_embossPanel, &EmbossPanel::cancelRequested, this, &MainWindow::endEmboss);
    propsDock->setMinimumWidth(320);
    addDockWidget(Qt::RightDockWidgetArea, propsDock);

    connect(m_properties, &PropertiesPanel::renameRequested, this, [this](ObjectId id, const QString& name) {
        run(std::make_unique<RenameBodyCommand>(id, name.toStdString()));
    });
    connect(m_properties, &PropertiesPanel::specRequested, this, [this](ObjectId id, const PrimitiveSpec& spec) {
        run(std::make_unique<SetSpecCommand>(id, spec));
    });
    connect(m_properties, &PropertiesPanel::uniformChanged, m_uniformAction, &QAction::setChecked);
    connect(m_properties, &PropertiesPanel::separateRequested, this, &MainWindow::separateSelected);
    connect(m_properties, &PropertiesPanel::editCutRequested, this, [this] { startSplit(selectedOne(), true); });
    connect(m_properties, &PropertiesPanel::editEmbossRequested, this, [this] { startEmboss(selectedOne(), true); });
    connect(m_properties, &PropertiesPanel::keepRoundChanged, this, [this](bool on) { m_keepRound = on; });
    connect(m_properties, &PropertiesPanel::sitOnPlateRequested, this, [this](ObjectId id) {
        try {
            const Placement resting = sitOnPlate(m_doc, id);
            if (!(resting == m_doc.find(id)->placement))
                run(std::make_unique<SetPlacementCommand>(id, resting, /*mergeable=*/false));
        } catch (const std::exception& e) {
            qWarning() << "Sit on plate failed:" << e.what();
        }
    });
    connect(m_properties, &PropertiesPanel::placementRequested, this,
            [this](ObjectId id, const Placement& placement) {
                Placement wanted = placement;
                const Body* body = m_doc.find(id);
                const bool turned = body && (placement.rotationX != body->placement.rotationX ||
                                             placement.rotationY != body->placement.rotationY ||
                                             placement.rotationZ != body->placement.rotationZ);
                if (turned && body->isBoolean()) {
                    // A combined shape turns about the middle of what you see.
                    Body moved = *body;
                    moved.placement.x = placement.x;
                    moved.placement.y = placement.y;
                    moved.placement.z = placement.z;
                    wanted = withRotation(moved, {placement.rotationX, placement.rotationY, placement.rotationZ},
                                          m_doc.visibleCentre(id));
                }
                run(std::make_unique<SetPlacementCommand>(id, wanted, /*mergeable=*/false));
            });

    // Let the user bring closed panels back.
    m_viewMenu->addAction(objectsDock->toggleViewAction());
    m_viewMenu->addAction(propsDock->toggleViewAction());
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Left-click: select (Ctrl: add)   |   Left-drag: move on the plate or drag a handle   |   "
                                "Right-drag: orbit   |   Middle-drag: pan   |   Wheel: zoom   |   W / E / R: "
                                "move / rotate / resize   |   F: fit"));

    m_modelInfo = new QLabel(this);
    m_glInfo = new QLabel(this);
    statusBar()->addPermanentWidget(m_modelInfo);
    statusBar()->addPermanentWidget(m_glInfo);

    connect(m_viewport, &Viewport::glInfoReady, this, [this](const QString& summary) {
        m_glInfo->setText(m_viewport->glVersionString());
        m_glInfo->setToolTip(summary);
    });
}

// ---- Selection ------------------------------------------------------------------

ObjectId MainWindow::selectedOne() const
{
    return m_selection.size() == 1 ? m_selection.front() : kInvalidId;
}

bool MainWindow::isSelected(ObjectId id) const
{
    return std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
}

void MainWindow::setSelection(ObjectId id)
{
    setSelection(id == kInvalidId ? std::vector<ObjectId>{} : std::vector<ObjectId>{id});
}

void MainWindow::setSelection(std::vector<ObjectId> ids)
{
    std::erase_if(ids, [this](ObjectId id) { return !m_doc.find(id); });
    if (ids == m_selection)
        return;
    if (m_splitMode.active)
        endSplit();  // picking something else leaves the Split tool
    if (m_embossMode.active)
        endEmboss();
    const ObjectId previousOne = selectedOne();
    m_selection = std::move(ids);
    if (const Body* body = m_doc.find(selectedOne()); body && body->id != previousOne) {
        // A newly selected shape starts in "Round" mode only if it is round.
        m_keepRound = body->isBoolean() || isRound(body->spec);
        m_properties->setKeepRound(m_keepRound);
    }
    // Show a selected part's result open in the tree.
    for (ObjectId id : m_selection)
        for (ObjectId p = m_doc.parentOf(id); p != kInvalidId; p = m_doc.parentOf(p))
            m_expanded.insert(p);
    refreshScene();  // highlight, tree, properties
}

void MainWindow::toggleSelection(ObjectId id)
{
    std::vector<ObjectId> next = m_selection;
    if (isSelected(id))
        std::erase(next, id);
    else
        next.push_back(id);
    setSelection(std::move(next));
}

void MainWindow::onViewportClicked(const QVector3D& origin, const QVector3D& direction,
                                   Qt::KeyboardModifiers modifiers)
{
    if (m_pressOnHandle) {  // a click on a handle keeps the current selection
        m_pressOnHandle = false;
        return;
    }
    if (m_splitMode.active)
        return;  // while splitting, clicks don't change what is selected (Cancel or Esc leaves)
    if (m_embossMode.active) {
        placeEmboss(origin, direction);  // a click on the shape moves the words there
        return;
    }
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {direction.x(), direction.y(), direction.z()};
    try {
        const auto hit = pick(m_doc, ray);
        if (modifiers.testFlag(Qt::ControlModifier)) {
            if (hit)
                toggleSelection(hit->id);  // Ctrl+click adds or removes
        } else {
            setSelection(hit ? hit->id : kInvalidId);  // clicking empty space clears the selection
        }
    } catch (const std::exception& e) {
        qWarning() << "Picking failed:" << e.what();
    }
}

// ---- Dragging ------------------------------------------------------------------

std::optional<view::GizmoTarget> MainWindow::gizmoTargetFor(ObjectId id) const
{
    const Body* body = m_doc.find(id);
    if (!body)
        return std::nullopt;
    const Transform t = m_doc.worldTransform(id);
    const Vec3 c = t.apply(m_doc.visibleCentre(id));  // combined shapes: the middle of what you see
    const auto axis = [&t](double x, double y, double z) {
        const Vec3 d = t.applyDirection({x, y, z});
        return QVector3D(static_cast<float>(d[0]), static_cast<float>(d[1]), static_cast<float>(d[2]));
    };
    view::GizmoTarget target;
    target.centre = QVector3D(static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]));
    target.bodyAxes = {axis(1, 0, 0), axis(0, 1, 0), axis(0, 0, 1)};
    if (body->isBoolean()) {
        // The box around what you see; resizing scales everything inside.
        const Bounds b = m_doc.localBounds(id);
        const auto size = b.size();
        target.halfExtents = QVector3D(size[0] / 2.0f, size[1] / 2.0f, size[2] / 2.0f);
        target.resizable = b.valid;
    } else {
        const Vec3 h = halfExtents(body->spec);
        target.halfExtents = QVector3D(float(h[0]), float(h[1]), float(h[2]));
        target.round = isRoundFromAbove(body->spec);
    }
    return target;
}

void MainWindow::setGizmoMode(view::GizmoMode mode)
{
    m_gizmoMode = mode;
    refreshScene();
}

void MainWindow::preview(const Body& state)
{
    // Live preview straight on the document; the undo step is recorded on release.
    try {
        m_doc.setSpec(state.id, state.spec);
        m_doc.setPlacement(state.id, state.placement);
    } catch (const std::exception& e) {
        qWarning() << "Drag preview rejected:" << e.what();
    }
}

void MainWindow::previewAll(const std::vector<Body>& states)
{
    try {
        for (const Body& b : states) {
            m_doc.setSpec(b.id, b.spec);
            if (b.boolean)
                m_doc.setBoolean(b.id, *b.boolean);
            m_doc.setPlacement(b.id, b.placement);
        }
    } catch (const std::exception& e) {
        qWarning() << "Resize preview rejected:" << e.what();
    }
}

void MainWindow::onLeftPressed(const QVector3D& origin, const QVector3D& direction,
                               const view::GizmoHandle& handle, const QPointF& pixel, Qt::KeyboardModifiers modifiers)
{
    m_dragState = Drag{};
    m_pressOnHandle = false;
    if (m_splitMode.active) {
        // The Split tool is open: shapes stay put, but the plane can be dragged.
        if (grabSplitPlane(origin, direction))
            m_pressOnHandle = true;
        return;
    }
    if (m_embossMode.active)
        return;  // shapes stay put while the words are placed

    if (handle.valid() && multiSelection()) {
        // Several shapes: move or turn them together about their shared middle.
        const auto frame = selectionGizmo();
        if (!frame)
            return;
        m_pressOnHandle = true;
        m_dragState.multi = true;
        m_dragState.handle = handle;
        m_dragState.frame = *frame;
        const Vec3 mid{frame->centre.x(), frame->centre.y(), frame->centre.z()};
        for (ObjectId id : m_selection) {
            m_dragState.beforeAll.push_back(*m_doc.find(id));
            m_dragState.turnCentres.push_back(m_doc.worldTransform(id).inverse().apply(mid));
        }
        const view::Gizmo gizmo(m_viewport->camera(), QSizeF(m_viewport->width(), m_viewport->height()),
                                m_dragState.frame, m_gizmoMode);
        if (handle.kind == view::GizmoHandle::Kind::Arrow) {
            const auto s = view::closestOnLine(origin, direction, frame->centre, gizmo.direction(handle));
            if (!s)
                return;
            m_dragState.kind = Drag::Kind::Arrow;
            m_dragState.startParam = *s;
        } else if (handle.kind == view::GizmoHandle::Kind::Ring) {
            const auto centre = m_viewport->projectToScreen(frame->centre);
            if (!centre)
                return;
            m_dragState.kind = Drag::Kind::Ring;
            m_dragState.centrePixel = *centre;
            m_dragState.lastPixel = pixel;
        } else {
            return;
        }
        m_viewport->setActiveHandle(handle);
        return;
    }

    const Body* selected = m_doc.find(selectedOne());
    const auto frame = gizmoTargetFor(selectedOne());
    if (handle.valid() && selected && frame) {
        m_pressOnHandle = true;
        m_dragState.before = *selected;
        m_dragState.handle = handle;
        m_dragState.frame = *frame;
        if (const ObjectId parent = m_doc.parentOf(selected->id); parent != kInvalidId)
            m_dragState.parentWorld = m_doc.worldTransform(parent);
        if (handle.kind == view::GizmoHandle::Kind::Face && selected->isBoolean()) {
            for (ObjectId id : m_doc.withDescendants(selected->id))
                m_dragState.beforeAll.push_back(*m_doc.find(id));
            m_dragState.snapshot = std::make_shared<Document>();
            m_dragState.snapshot->replaceAll(m_doc.bodies());
            m_dragState.localBounds = m_doc.localBounds(selected->id);
        }
        const view::Gizmo gizmo(m_viewport->camera(), QSizeF(m_viewport->width(), m_viewport->height()),
                                m_dragState.frame, m_gizmoMode);
        const QVector3D dir = gizmo.direction(handle);
        switch (handle.kind) {
        case view::GizmoHandle::Kind::Arrow:
        case view::GizmoHandle::Kind::Face: {
            const auto s = view::closestOnLine(origin, direction, m_dragState.frame.centre, dir);
            if (!s)
                return;  // looking straight down that axis: nothing sensible to drag
            m_dragState.kind = handle.kind == view::GizmoHandle::Kind::Arrow ? Drag::Kind::Arrow : Drag::Kind::Face;
            m_dragState.startParam = *s;
            break;
        }
        case view::GizmoHandle::Kind::Ring: {
            const auto centre = m_viewport->projectToScreen(m_dragState.frame.centre);
            if (!centre)
                return;
            m_dragState.kind = Drag::Kind::Ring;
            m_dragState.centrePixel = *centre;
            m_dragState.lastPixel = pixel;
            m_dragState.turnCentre = m_doc.visibleCentre(m_dragState.before.id);
            break;
        }
        case view::GizmoHandle::Kind::None:
            return;
        }
        m_viewport->setActiveHandle(handle);
        return;
    }

    // Ctrl+click adds to the selection (handled on release); it never drags.
    if (modifiers.testFlag(Qt::ControlModifier))
        return;

    // Not on a handle: grab whatever body is under the cursor and slide it on the plate.
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {direction.x(), direction.y(), direction.z()};
    const auto hit = pick(m_doc, ray);
    if (!hit)
        return;  // empty space: the click (on release) clears the selection
    if (multiSelection() && isSelected(hit->id)) {
        // Grabbed one of several selected shapes: slide them all. (A plain click,
        // without dragging, still selects just that one.)
        m_dragState.kind = Drag::Kind::Plate;
        m_dragState.multi = true;
        for (ObjectId id : m_selection)
            m_dragState.beforeAll.push_back(*m_doc.find(id));
        m_dragState.grabPoint = QVector3D(float(hit->point[0]), float(hit->point[1]), float(hit->point[2]));
        return;
    }
    setSelection(hit->id);
    m_dragState.kind = Drag::Kind::Plate;
    m_dragState.before = *m_doc.find(hit->id);
    m_dragState.grabPoint = QVector3D(float(hit->point[0]), float(hit->point[1]), float(hit->point[2]));
}

void MainWindow::onLeftDragged(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel,
                               Qt::KeyboardModifiers modifiers)
{
    Drag& d = m_dragState;
    if (d.kind == Drag::Kind::None)
        return;
    const bool snap = m_snap && !modifiers.testFlag(Qt::AltModifier);
    const auto step = [snap](double value, double size) { return snap ? view::snapTo(value, size) : value; };
    const QString freeHint = snap ? tr("   (hold Alt to move freely)") : QString();

    if (d.multi) {
        std::vector<Body> next = d.beforeAll;
        QString status;
        if (d.kind == Drag::Kind::Plate) {
            const auto hit = view::intersectPlane(origin, direction, d.grabPoint, QVector3D(0, 0, 1));
            if (!hit)
                return;
            const double dx = step(hit->x() - d.grabPoint.x(), kMoveStep);
            const double dy = step(hit->y() - d.grabPoint.y(), kMoveStep);
            for (Body& b : next)
                b.placement = movedOnPlate(b, {dx, dy, 0.0});
            status = tr("Move  X %1 mm   Y %2 mm").arg(dx, 0, 'f', 1).arg(dy, 0, 'f', 1);
        } else if (d.kind == Drag::Kind::Arrow) {
            const QVector3D dir(d.handle.axis == 0, d.handle.axis == 1, d.handle.axis == 2);
            const auto s = view::closestOnLine(origin, direction, d.frame.centre, dir);
            if (!s)
                return;
            const double delta = step(*s - d.startParam, kMoveStep);
            Vec3 move{0.0, 0.0, 0.0};
            move[static_cast<std::size_t>(d.handle.axis)] = delta;
            for (Body& b : next)
                b.placement = movedOnPlate(b, move);
            status = tr("Move  %1 %2 mm").arg(QChar("XYZ"[d.handle.axis])).arg(delta, 0, 'f', 1);
        } else if (d.kind == Drag::Kind::Ring) {
            d.turned += view::screenAngle(d.centrePixel, d.lastPixel, pixel);
            d.lastPixel = pixel;
            const QVector3D axis(d.handle.axis == 0, d.handle.axis == 1, d.handle.axis == 2);
            const float facing = QVector3D::dotProduct(axis, m_viewport->camera().eyePosition() - d.frame.centre);
            const double degrees = step(d.turned * (facing >= 0.0f ? 1.0 : -1.0), kTurnStep);
            for (std::size_t i = 0; i < next.size(); ++i)
                next[i].placement = rotatedAboutPlateAxis(d.beforeAll[i], d.handle.axis, degrees, d.turnCentres[i]);
            status = tr("Rotate around %1  %2°").arg(QChar("XYZ"[d.handle.axis])).arg(degrees, 0, 'f', 1);
        } else {
            return;
        }
        d.moved = true;
        previewAll(next);
        statusBar()->showMessage(tr("%1 shapes:  ").arg(next.size()) + status + freeHint);
        return;
    }

    Body next = d.before;
    QString status;
    switch (d.kind) {
    case Drag::Kind::Plate: {
        const auto hit = view::intersectPlane(origin, direction, d.grabPoint, QVector3D(0, 0, 1));
        if (!hit)
            return;
        const double dx = step(hit->x() - d.grabPoint.x(), kMoveStep);
        const double dy = step(hit->y() - d.grabPoint.y(), kMoveStep);
        next.placement.x += dx;
        next.placement.y += dy;
        status = tr("Move  X %1 mm   Y %2 mm").arg(dx, 0, 'f', 1).arg(dy, 0, 'f', 1);
        break;
    }
    case Drag::Kind::Arrow: {
        const QVector3D dir(d.handle.axis == 0, d.handle.axis == 1, d.handle.axis == 2);
        const auto s = view::closestOnLine(origin, direction, d.frame.centre, dir);
        if (!s)
            return;
        const double delta = step(*s - d.startParam, kMoveStep);
        Vec3 move{0.0, 0.0, 0.0};
        move[static_cast<std::size_t>(d.handle.axis)] = delta;
        next.placement = movedOnPlate(d.before, move, d.parentWorld);  // a part moves inside its shape
        status = tr("Move  %1 %2 mm").arg(QChar("XYZ"[d.handle.axis])).arg(delta, 0, 'f', 1);
        break;
    }
    case Drag::Kind::SplitPlane: {
        const auto s = view::closestOnLine(origin, direction, d.grabPoint, d.axis);
        if (!s)
            return;
        const double offset = d.startOffset + step(*s - d.startParam, kMoveStep);
        m_splitPanel->setOffset(offset);
        statusBar()->showMessage(tr("Cut position %1 mm from the middle").arg(offset, 0, 'f', 1) + freeHint);
        d.moved = true;
        return;  // the panel redraws the plane; no shape changes
    }
    case Drag::Kind::Ring: {
        d.turned += view::screenAngle(d.centrePixel, d.lastPixel, pixel);
        d.lastPixel = pixel;
        const QVector3D axis(d.handle.axis == 0, d.handle.axis == 1, d.handle.axis == 2);
        // Anticlockwise on screen is a positive turn when the axis points at the viewer.
        const float facing = QVector3D::dotProduct(axis, m_viewport->camera().eyePosition() - d.frame.centre);
        const double degrees = step(d.turned * (facing >= 0.0f ? 1.0 : -1.0), kTurnStep);
        next.placement = rotatedAboutPlateAxis(d.before, d.handle.axis, degrees, d.turnCentre, d.parentWorld);
        status = tr("Rotate around %1  %2°").arg(QChar("XYZ"[d.handle.axis])).arg(degrees, 0, 'f', 1);
        break;
    }
    case Drag::Kind::Face: {
        const QVector3D dir = d.frame.bodyAxes.at(static_cast<std::size_t>(d.handle.axis)) *
                              static_cast<float>(d.handle.side);
        const auto s = view::closestOnLine(origin, direction, d.frame.centre, dir);
        if (!s)
            return;
        if (d.before.isBoolean()) {
            // A combined shape: stretch everything inside, keeping the opposite side still.
            const std::size_t axis = static_cast<std::size_t>(d.handle.axis);
            const auto size = d.localBounds.size();
            const double was = size[axis];
            double value = was + (*s - d.startParam);
            if (snap)
                value = view::snapTo(value, kSizeStep);
            value = std::max(value, kSizeStep);
            const double factor = value / was;
            const auto c = d.localBounds.center();
            Vec3 anchor{c[0], c[1], c[2]};
            anchor[axis] -= d.handle.side * was / 2.0;
            Vec3 factors{1.0, 1.0, 1.0};
            factors[axis] = factor;
            QString note;
            std::vector<Body> after;
            try {
                after = scaledTree(*d.snapshot, d.before.id, m_uniform ? Vec3{factor, factor, factor} : factors, anchor);
            } catch (const NotScalable& e) {
                // Can't stretch one way only: scale evenly instead, and say why.
                after = scaledTree(*d.snapshot, d.before.id, {factor, factor, factor}, anchor);
                note = tr("   (scaled evenly: %1)").arg(QString::fromUtf8(e.what()));
            } catch (const std::exception& e) {
                statusBar()->showMessage(QString::fromUtf8(e.what()));
                return;
            }
            d.moved = true;
            previewAll(after);
            statusBar()->showMessage(tr("Resize  %1 %2 mm   (x%3)")
                                         .arg(QChar("XYZ"[axis]))
                                         .arg(value, 0, 'f', 1)
                                         .arg(factor, 0, 'f', 2) +
                                     (m_uniform ? tr("   (uniform)") : QString()) + note + freeHint);
            return;
        }
        double value = dimension(d.before.spec, d.handle.axis) + (*s - d.startParam);
        if (snap)
            value = std::max(kSizeStep, view::snapTo(value, kSizeStep));
        ResizeOptions options;
        options.uniform = m_uniform;
        options.keepRound = m_keepRound;
        next = resizedFace(d.before, d.handle.axis, d.handle.side, value, options);

        QString what;
        const bool linked = m_keepRound || m_uniform;
        const QString axisName = d.handle.axis == 0 ? QStringLiteral(" X") : QStringLiteral(" Y");
        if (d.handle.axis == 2)
            what = std::holds_alternative<SphereSpec>(next.spec) ? (linked ? tr("Diameter") : tr("Height"))
                   : std::holds_alternative<TorusSpec>(next.spec) ? tr("Thickness")
                                                                   : tr("Height");
        else if (std::holds_alternative<CylinderSpec>(next.spec) || std::holds_alternative<SphereSpec>(next.spec))
            what = linked ? tr("Radius") : tr("Radius") + axisName;
        else if (isRoundFromAbove(next.spec))
            what = linked ? tr("Outer radius") : tr("Outer radius") + axisName;
        else
            what = d.handle.axis == 0 ? tr("Width") : tr("Depth");
        status = tr("Resize  %1 %2 mm").arg(what).arg(dimension(next.spec, d.handle.axis), 0, 'f', 1);
        if (m_uniform) {
            const double factor = dimension(next.spec, d.handle.axis) / dimension(d.before.spec, d.handle.axis);
            status += tr("   (uniform x%1)").arg(factor, 0, 'f', 2);
        }
        break;
    }
    case Drag::Kind::None:
        return;
    }

    d.moved = true;
    preview(next);
    statusBar()->showMessage(status + freeHint);
}

void MainWindow::onLeftReleased(bool wasDrag)
{
    Drag d = m_dragState;
    m_dragState = Drag{};
    m_viewport->setActiveHandle({});
    if (d.kind == Drag::Kind::None || !wasDrag || !d.moved)
        return;
    statusBar()->clearMessage();
    if (d.kind == Drag::Kind::SplitPlane)
        return;  // the plane moved; nothing is cut until Split is clicked

    if (!d.beforeAll.empty()) {
        // Resized a combined shape: one undo step for every body that changed.
        std::vector<Body> before, after;
        for (const Body& b : d.beforeAll) {
            const Body* now = m_doc.find(b.id);
            if (now && !(*now == b)) {
                before.push_back(b);
                after.push_back(*now);
            }
        }
        if (before.empty())
            return;
        const char* label = !d.multi ? "Resize" : d.kind == Drag::Kind::Ring ? "Rotate" : "Move";
        if (!run(std::make_unique<EditBodiesCommand>(before, after, label)))
            previewAll(d.beforeAll);
        return;
    }

    const Body* now = m_doc.find(d.before.id);
    if (!now || *now == d.before)
        return;  // dragged back to where it started
    const QString label = d.kind == Drag::Kind::Ring   ? tr("Rotate")
                          : d.kind == Drag::Kind::Face ? tr("Resize")
                                                       : tr("Move");
    if (!run(std::make_unique<EditBodyCommand>(d.before, *now, label.toStdString())))
        preview(d.before);
}

void MainWindow::cancelDrag()
{
    const Drag d = m_dragState;
    m_dragState = Drag{};
    m_viewport->setActiveHandle({});
    if (d.kind == Drag::Kind::SplitPlane)
        m_splitPanel->setOffset(d.startOffset);
    else if (!d.beforeAll.empty() && d.moved)
        previewAll(d.beforeAll);
    else if (d.kind != Drag::Kind::None && d.moved)
        preview(d.before);
    statusBar()->showMessage(tr("Drag cancelled"), 2000);
    qInfo() << "Drag cancelled";
}

// ---- Editing --------------------------------------------------------------------

bool MainWindow::run(std::unique_ptr<Command> command)
{
    const std::string label = command->label();
    try {
        m_stack.execute(std::move(command));
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        return true;
    } catch (const std::exception& e) {
        qWarning().noquote() << "Command" << QString::fromStdString(label) << "failed:" << e.what();
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("%1 did not work:\n%2").arg(QString::fromStdString(label),
                                                           QString::fromUtf8(e.what())));
        refreshScene();  // put the panel back to the real values
        return false;
    }
}

void MainWindow::addBody(const PrimitiveSpec& spec)
{
    Body body;
    body.id = m_doc.allocateId();
    body.spec = spec;
    if (const auto* t = std::get_if<TextSpec>(&spec)) {
        // Letters 10 mm tall in their natural proportions, in the default font.
        TextSpec text = *t;
        try {
            text.font = text::defaultFont();
            body.spec = text::sizedForLetters(text, 10.0);
        } catch (const std::exception& e) {
            qWarning() << "Text could not be laid out:" << e.what();
        }
    }
    body.name = m_doc.uniqueName(typeName(spec));
    body.placement = suggestPlacement(m_doc, footprintWidth(body.spec));
    const ObjectId id = body.id;
    if (run(std::make_unique<AddBodyCommand>(std::move(body))))
        setSelection(id);  // like 3D Builder: a new object is ready to edit
}

void MainWindow::deleteSelected()
{
    std::vector<ObjectId> targets;
    for (ObjectId id : m_selection) {
        if (!m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("'%1' is part of a combined shape. Delete the whole shape, or Separate "
                                        "it first.").arg(QString::fromStdString(m_doc.find(id)->name)), 6000);
            return;
        }
        targets.push_back(id);
    }
    if (targets.empty())
        return;

    // A combined shape goes with all its parts: remove the result first (which frees
    // its parts), then the parts. Undo puts it all back in reverse.
    m_stack.beginMacro(targets.size() == 1 ? "Delete " + m_doc.find(targets.front())->name : "Delete");
    try {
        for (ObjectId id : targets)
            for (ObjectId part : m_doc.withDescendants(id))
                m_stack.execute(std::make_unique<RemoveBodyCommand>(part));
    } catch (const std::exception& e) {
        qWarning() << "Delete stopped early:" << e.what();
    }
    m_stack.endMacro();
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
}

void MainWindow::combineSelected(BooleanOp op)
{
    const QString name = QString::fromStdString(toString(op));
    if (m_selection.size() < 2) {
        statusBar()->showMessage(tr("%1 needs two or more shapes: Ctrl+click to select them.").arg(name), 6000);
        return;
    }
    for (ObjectId id : m_selection) {
        if (!m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Only shapes on the plate can be combined; '%1' is a part of another "
                                        "shape.").arg(QString::fromStdString(m_doc.find(id)->name)), 6000);
            return;
        }
    }

    if (const QString open = openMeshIn(m_selection); !open.isEmpty()) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("'%1' has gaps in its surface (it isn't watertight), so it can't be combined. "
                                    "It can still be moved, resized and exported.").arg(open));
        return;
    }

    // Check the result first: an empty result (e.g. Intersect of shapes that don't
    // touch) would vanish from the plate, which looks like a bug.
    BooleanSpec spec;
    spec.op = op;
    spec.operands = m_selection;
    Document::Preview preview;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        preview = m_doc.previewResult(spec);
        if (preview.empty) {
            QMessageBox::information(this, tr("Forge3D"),
                                     op == BooleanOp::Intersect
                                         ? tr("These shapes don't overlap, so Intersect would leave nothing.")
                                         : tr("%1 would leave nothing: the first shape is completely "
                                              "covered by the others.").arg(name));
            return;
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("%1 did not work:\n%2").arg(name, QString::fromUtf8(e.what())));
        return;
    }

    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName(toString(op));
    // Turn around the middle of what you will see (cut-away parts don't count).
    // Rotation always uses the visible centre; this keeps the stored pivot the same.
    const auto centre = preview.bounds.center();
    spec.pivot = {centre[0], centre[1], centre[2]};
    result.boolean = spec;
    const ObjectId id = result.id;
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::alignSelected(AlignMode mode)
{
    const QString name = QString::fromStdString(toString(mode));
    if (m_selection.size() < 2) {
        statusBar()->showMessage(tr("%1: select shape 1, then Ctrl+click the shape to move.").arg(name), 6000);
        return;
    }
    for (ObjectId id : m_selection) {
        if (!m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Only shapes on the plate can be aligned; '%1' is a part of another "
                                        "shape.").arg(QString::fromStdString(m_doc.find(id)->name)), 6000);
            return;
        }
    }

    // Work out every move first, against shape 1 where it is now.
    const ObjectId fixed = m_selection.front();
    std::vector<std::pair<ObjectId, Placement>> moves;
    try {
        for (std::size_t i = 1; i < m_selection.size(); ++i) {
            const ObjectId id = m_selection[i];
            const Placement p = alignedPlacement(m_doc, fixed, id, mode);
            if (!(p == m_doc.find(id)->placement))
                moves.emplace_back(id, p);
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("%1 did not work:\n%2").arg(name, QString::fromUtf8(e.what())));
        return;
    }
    if (moves.empty()) {
        statusBar()->showMessage(tr("%1: already lined up.").arg(name), 4000);
        return;
    }

    // One Undo step for the lot.
    m_stack.beginMacro(name.toStdString());
    try {
        for (const auto& [id, p] : moves)
            m_stack.execute(std::make_unique<SetPlacementCommand>(id, p, /*mergeable=*/false));
    } catch (const std::exception& e) {
        qWarning() << "Align stopped early:" << e.what();
    }
    m_stack.endMacro();
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    statusBar()->showMessage(tr("%1: moved %2 to '%3'.")
                                 .arg(name)
                                 .arg(moves.size() == 1
                                          ? QStringLiteral("'%1'").arg(QString::fromStdString(m_doc.find(moves.front().first)->name))
                                          : tr("%1 shapes").arg(moves.size()))
                                 .arg(QString::fromStdString(m_doc.find(fixed)->name)),
                             5000);
}

// ---- Arrange: duplicate, clipboard, mirror, group -----------------------------------

bool MainWindow::multiSelection() const
{
    return m_selection.size() >= 2 &&
           std::all_of(m_selection.begin(), m_selection.end(), [this](ObjectId id) { return m_doc.isTopLevel(id); });
}

std::optional<view::GizmoTarget> MainWindow::selectionGizmo() const
{
    if (!multiSelection())
        return gizmoTargetFor(selectedOne());
    Bounds all;
    for (ObjectId id : m_selection) {
        const Bounds b = m_doc.worldBounds(id);
        if (b.valid) {
            all.expand(b.min[0], b.min[1], b.min[2]);
            all.expand(b.max[0], b.max[1], b.max[2]);
        }
    }
    if (!all.valid)
        return std::nullopt;
    const auto c = all.center();
    view::GizmoTarget target;
    target.centre = QVector3D(c[0], c[1], c[2]);
    target.bodyAxes = {QVector3D(1, 0, 0), QVector3D(0, 1, 0), QVector3D(0, 0, 1)};
    target.resizable = false;  // Group them to resize together
    return target;
}

void MainWindow::duplicateSelected()
{
    if (m_selection.empty() || !std::all_of(m_selection.begin(), m_selection.end(),
                                             [this](ObjectId id) { return m_doc.isTopLevel(id); }))
        return;
    std::vector<BodyTree> trees;
    for (ObjectId id : m_selection)
        trees.push_back(captureTree(m_doc, id));
    try {
        // Copies go to the right of everything, so they never land inside another shape.
        const auto made =
            addCopies(m_stack, m_doc, trees, clearOfPlate(m_doc, leftEdge(m_doc, m_selection)), "Duplicate");
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        setSelection(made);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Duplicate did not work:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::copySelected()
{
    if (m_selection.empty())
        return;
    m_clipboard.clear();
    for (ObjectId id : m_selection)
        if (m_doc.isTopLevel(id))
            m_clipboard.push_back(captureTree(m_doc, id));
    m_clipboardLeft = leftEdge(m_doc, m_selection);
    m_pasteCount = 1;
    statusBar()->showMessage(tr("Copied %n shape(s). Ctrl+V pastes.", nullptr, int(m_clipboard.size())), 4000);
    refreshScene();  // Paste becomes available
}

void MainWindow::cutSelected()
{
    copySelected();
    m_pasteCount = 0;  // the originals are gone: the first paste puts them back where they were
    deleteSelected();
}

void MainWindow::paste()
{
    if (m_clipboard.empty())
        return;
    try {
        // Right after Cut: back where they were. Otherwise: clear of everything on the plate.
        const Vec3 offset = m_pasteCount == 0 ? Vec3{0.0, 0.0, 0.0} : clearOfPlate(m_doc, m_clipboardLeft);
        const auto made = addCopies(m_stack, m_doc, m_clipboard, offset, "Paste");
        ++m_pasteCount;
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        setSelection(made);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Paste did not work:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::mirrorSelected(int axis)
{
    if (m_selection.empty())
        return;
    try {
        const auto made = mirrorBodies(m_stack, m_doc, m_selection, axis);
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        setSelection(made);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Mirror did not work:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::groupSelected()
{
    if (m_selection.size() < 2) {
        statusBar()->showMessage(tr("Group: select two or more shapes (Ctrl+click)."), 5000);
        return;
    }
    try {
        const ObjectId id = groupBodies(m_stack, m_doc, m_selection);
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        setSelection(id);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Group did not work:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

// ---- Split tool -------------------------------------------------------------------

namespace {

constexpr std::uint64_t kSplitPlaneItem = ~std::uint64_t{0} - 1;  // ids no body will reach
constexpr std::uint64_t kSplitShaftItem = ~std::uint64_t{0} - 2;
constexpr std::uint64_t kSplitHeadItem = ~std::uint64_t{0} - 3;

// A 2 x 2 square in the XY plane, centred, facing both ways.
std::shared_ptr<const Mesh> splitPlaneMesh()
{
    static const std::shared_ptr<const Mesh> mesh = [] {
        Mesh m;
        const float c[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (float side : {1.0f, -1.0f})
            for (const auto& p : c) {
                m.positions.insert(m.positions.end(), {p[0], p[1], 0.0f});
                m.normals.insert(m.normals.end(), {0.0f, 0.0f, side});
            }
        m.indices = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6};
        return std::make_shared<const Mesh>(std::move(m));
    }();
    return mesh;
}

// The plane's arrow, 1 unit long along +Z: a thin shaft and a cone tip.
std::shared_ptr<const Mesh> splitArrowMesh(bool head)
{
    static const std::shared_ptr<const Mesh> shaft =
        std::make_shared<const Mesh>(tessellate(primitives::makeCylinder(0.025, 0.75)));
    static const std::shared_ptr<const Mesh> tip = [] {
        TessellationSettings s;
        Mesh m = tessellate(primitives::makeCone(0.08, 0.0, 0.25), s);
        for (std::size_t i = 2; i < m.positions.size(); i += 3)
            m.positions[i] += 0.75f;  // sit on top of the shaft
        return std::make_shared<const Mesh>(std::move(m));
    }();
    return head ? tip : shaft;
}

// Places a unit-sized model at `point`, turned so its +Z faces `normal`, scaled by s.
QMatrix4x4 planeFrame(const Vec3& point, const Vec3& normal, float s)
{
    QVector3D n{static_cast<float>(normal[0]), static_cast<float>(normal[1]), static_cast<float>(normal[2])};
    n.normalize();
    const QVector3D helper = std::abs(n.z()) < 0.9f ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0);
    const QVector3D u = QVector3D::crossProduct(helper, n).normalized();
    const QVector3D v = QVector3D::crossProduct(n, u);
    QMatrix4x4 m(u.x(), v.x(), n.x(), float(point[0]),  //
                 u.y(), v.y(), n.y(), float(point[1]),  //
                 u.z(), v.z(), n.z(), float(point[2]),  //
                 0.0f, 0.0f, 0.0f, 1.0f);
    m.scale(s);
    return m;
}

}  // namespace

void MainWindow::startSplit(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || !m_doc.isTopLevel(target))
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();

    SplitMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    double tiltX = 0.0, tiltY = 0.0, offset = 0.0;
    try {
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Split)
                return;
            mode.measured = body->boolean->operands.front();
            mode.centre = m_doc.worldTransform(mode.measured).apply(m_doc.visibleCentre(mode.measured));
            tiltsOfPlane(splitPlaneOf(m_doc, target), mode.centre, tiltX, tiltY, offset);
        } else {
            mode.measured = target;
            mode.centre = m_doc.worldTransform(target).apply(m_doc.visibleCentre(target));
        }
    } catch (const std::exception& e) {
        qWarning() << "Split could not start:" << e.what();
        return;
    }
    m_splitMode = mode;
    const QString name = QString::fromStdString(body->name);
    m_splitPanel->start(editing ? tr("Move the cut of '%1'").arg(name) : tr("Split '%1'").arg(name), editing,
                        tiltX, tiltY, offset);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_splitPanel->show();
    statusBar()->showMessage(tr("Split: drag the yellow plane (or use the panel on the right), then click %1. "
                                "Esc cancels.")
                                 .arg(editing ? tr("Apply") : tr("Split")));
    updateSplitPlane();
    refreshCommandState();
}

void MainWindow::endSplit()
{
    if (!m_splitMode.active)
        return;
    m_splitMode = SplitMode{};
    m_splitPanel->hide();
    m_properties->show();
    // Swapping the panels back makes Qt re-measure the dock and it can come back much
    // wider; keep the width it had.
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

SplitPlane MainWindow::currentSplitPlane() const
{
    return planeFromTilts(m_splitMode.centre, m_splitPanel->tiltX(), m_splitPanel->tiltY(), m_splitPanel->offset());
}

void MainWindow::updateSplitPlane()
{
    if (!m_splitMode.active)
        return;
    const SplitPlane plane = currentSplitPlane();
    m_splitPanel->setSideNames(QString::fromStdString(sideName(plane.normal)),
                               QString::fromStdString(sideName({-plane.normal[0], -plane.normal[1], -plane.normal[2]})));
    // The plane can slide from one end of the shape to the other.
    const Bounds b = m_doc.worldBounds(m_splitMode.measured);
    double lo = 0.0, hi = 0.0;
    for (int corner = 0; corner < 8; ++corner) {
        const Vec3 p{(corner & 1) ? b.max[0] : b.min[0], (corner & 2) ? b.max[1] : b.min[1],
                     (corner & 4) ? b.max[2] : b.min[2]};
        double d = 0.0;
        for (int i = 0; i < 3; ++i)
            d += (p[i] - m_splitMode.centre[i]) * plane.normal[i];
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    m_splitPanel->setOffsetRange(lo, hi);
    refreshScene();
}

float MainWindow::splitPlaneHalfSize() const
{
    const auto size = m_doc.worldBounds(m_splitMode.measured).size();
    return std::max({size[0], size[1], size[2], 2.0f}) * 0.65f;
}

bool MainWindow::grabSplitPlane(const QVector3D& origin, const QVector3D& direction)
{
    const SplitPlane plane = currentSplitPlane();
    const QVector3D p(float(plane.point[0]), float(plane.point[1]), float(plane.point[2]));
    QVector3D n(float(plane.normal[0]), float(plane.normal[1]), float(plane.normal[2]));
    n.normalize();
    const float s = splitPlaneHalfSize();

    // On the yellow square?
    bool hit = false;
    QVector3D grab = p;
    if (const auto at = view::intersectPlane(origin, direction, p, n)) {
        const QMatrix4x4 frame = planeFrame(plane.point, plane.normal, 1.0f);
        const QVector3D u = frame.column(0).toVector3D(), v = frame.column(1).toVector3D();
        const QVector3D d = *at - p;
        if (std::abs(QVector3D::dotProduct(d, u)) <= s && std::abs(QVector3D::dotProduct(d, v)) <= s) {
            hit = true;
            grab = *at;
        }
    }
    // Or close to the arrow (useful when the plane is seen edge-on)?
    if (!hit) {
        const float length = s * 0.6f;
        const QVector3D dir = direction.normalized();
        for (int i = 0; i <= 20 && !hit; ++i) {
            const QVector3D q = p + n * (length * float(i) / 20.0f);
            const QVector3D toQ = q - origin;
            const float along = QVector3D::dotProduct(toQ, dir);
            if (along > 0.0f && (toQ - dir * along).length() < s * 0.08f) {
                hit = true;
                grab = q;
            }
        }
    }
    if (!hit)
        return false;
    const auto startParam = view::closestOnLine(origin, direction, grab, n);
    if (!startParam) {
        statusBar()->showMessage(tr("Looking straight along the arrow: orbit a little to drag the plane."), 4000);
        return true;  // still a press on the plane: don't change the selection
    }
    m_dragState.kind = Drag::Kind::SplitPlane;
    m_dragState.grabPoint = grab;
    m_dragState.axis = n;
    m_dragState.startParam = *startParam;
    m_dragState.startOffset = m_splitPanel->offset();
    return true;
}

void MainWindow::addSplitItems(std::vector<DisplayItem>& items) const
{
    if (!m_splitMode.active)
        return;
    const SplitPlane plane = currentSplitPlane();
    const float s = splitPlaneHalfSize();

    DisplayItem quad;
    quad.id = kSplitPlaneItem;
    quad.mesh = splitPlaneMesh();
    quad.model = planeFrame(plane.point, plane.normal, s);
    quad.color = QVector3D(1.0f, 0.82f, 0.1f);
    quad.alpha = 0.4f;
    items.push_back(quad);
    for (const bool head : {false, true}) {
        DisplayItem arrow;
        arrow.id = head ? kSplitHeadItem : kSplitShaftItem;
        arrow.mesh = splitArrowMesh(head);
        arrow.model = planeFrame(plane.point, plane.normal, s * 0.6f);
        arrow.color = QVector3D(1.0f, 0.75f, 0.0f);
        items.push_back(arrow);
    }
}

void MainWindow::applySplit()
{
    if (!m_splitMode.active)
        return;
    const SplitPlane plane = currentSplitPlane();
    if (m_splitMode.editing) {
        const Body* now = m_doc.find(m_splitMode.target);
        if (!now)
            return endSplit();
        Body after;
        try {
            after = withSplitPlane(m_doc, m_splitMode.target, plane);
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Forge3D"), QString::fromUtf8(e.what()));
            return;  // stay in the tool so the plane can be fixed
        }
        const Body before = *now;
        const ObjectId id = before.id;
        endSplit();
        run(std::make_unique<EditBodyCommand>(before, after, "Move cut"));
        setSelection(id);
        return;
    }
    std::vector<ObjectId> made;
    try {
        made = splitBody(m_stack, m_doc, m_splitMode.target, plane, m_splitPanel->keep());
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Split did not work:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    endSplit();
    // Select the arrow-side half on its own, so the move and rotate handles show
    // straight away (as after Merge); click the other half to work on it.
    setSelection(made.front());
}

bool MainWindow::toolOpen() const
{
    return m_splitMode.active || m_embossMode.active;
}

void MainWindow::startEmboss(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || !m_doc.isTopLevel(target))
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    if (m_splitMode.active)
        endSplit();

    EmbossMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    EmbossText words = m_lastWords;
    try {
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Emboss || !body->boolean->emboss)
                return;
            words = *body->boolean->emboss;
            mode.spot = embossSpot(m_doc, target);
        } else {
            // Letters to suit the shape: about a fifth of its middle size.
            auto size = m_doc.worldBounds(target).size();
            std::sort(size.begin(), size.end());
            words.letterHeight = std::clamp(std::round(double(size[1]) * 0.2 * 2.0) / 2.0, 2.0, 20.0);
            // Start on the side facing you, in the middle.
            const QVector3D eye = m_viewport->camera().eyePosition();
            const auto c = m_doc.worldTransform(target).apply(m_doc.visibleCentre(target));
            const Vec3 from{eye.x(), eye.y(), eye.z()};
            mode.spot = surfaceUnder(m_doc, target, from, {c[0] - from[0], c[1] - from[1], c[2] - from[2]},
                                     words.letterHeight / 2.0);
        }
    } catch (const std::exception& e) {
        qWarning() << "Emboss could not start:" << e.what();
        if (editing)
            return;
    }
    m_embossMode = mode;
    const QString name = QString::fromStdString(body->name);
    m_embossPanel->start(editing ? tr("Change the words on '%1'").arg(name) : tr("Emboss '%1'").arg(name),
                         editing, words);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_embossPanel->show();
    statusBar()->showMessage(tr("Emboss: type the words in the panel on the right and click the shape where they "
                                "go, then click %1. Esc cancels.")
                                 .arg(editing ? tr("Apply") : tr("Emboss")));
    updateEmbossPreview();
    refreshCommandState();
}

void MainWindow::endEmboss()
{
    if (!m_embossMode.active)
        return;
    m_embossTimer->stop();
    m_embossMode = EmbossMode{};
    m_embossPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

std::optional<BooleanSpec> MainWindow::currentEmbossSpec() const
{
    if (!m_embossMode.active || !m_embossMode.spot)
        return std::nullopt;
    const EmbossText words = m_embossPanel->words();
    if (m_embossMode.editing)
        return withEmboss(m_doc, m_embossMode.target, *m_embossMode.spot, words).boolean;
    return embossSpec(m_doc, m_embossMode.target, *m_embossMode.spot, words);
}

void MainWindow::updateEmbossPreview()
{
    if (!m_embossMode.active)
        return;
    m_embossMode.preview.reset();
    bool ok = false;
    try {
        const auto spec = currentEmbossSpec();
        if (!spec) {
            m_embossPanel->setStatus(tr("Click on the shape where the words should go."), false);
        } else {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            m_embossMode.preview = m_doc.previewMesh(*spec);
            m_embossPanel->setStatus(tr("This is how it will look. Click elsewhere on the shape to move the words."),
                                     false);
            ok = true;
        }
    } catch (const std::exception& e) {
        m_embossPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    m_embossPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::placeEmboss(const QVector3D& origin, const QVector3D& direction)
{
    try {
        const auto spot = surfaceUnder(m_doc, m_embossMode.target, {origin.x(), origin.y(), origin.z()},
                                       {direction.x(), direction.y(), direction.z()},
                                       m_embossPanel->words().letterHeight / 2.0);
        if (!spot) {
            statusBar()->showMessage(tr("Click on the shape itself to put the words there."), 4000);
            return;
        }
        m_embossMode.spot = spot;
    } catch (const std::exception& e) {
        qWarning() << "Placing the words failed:" << e.what();
        return;
    }
    updateEmbossPreview();
}

void MainWindow::applyEmboss()
{
    if (!m_embossMode.active)
        return;
    std::optional<BooleanSpec> spec;
    try {
        spec = currentEmbossSpec();
        if (!spec)
            return;
        m_doc.previewMesh(*spec);  // fails here (with its message) rather than in the command
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Emboss did not work:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    m_lastWords = *spec->emboss;
    const ObjectId target = m_embossMode.target;
    if (m_embossMode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endEmboss();
        const Body before = *now;
        Body after = before;
        after.boolean = *spec;
        endEmboss();
        run(std::make_unique<EditBodyCommand>(before, after, "Change words"));
        setSelection(target);
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Emboss");
    result.boolean = *spec;
    const ObjectId id = result.id;
    endEmboss();
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::separateSelected()
{
    const Body* body = m_doc.find(selectedOne());
    if (!body || !body->isBoolean()) {
        statusBar()->showMessage(tr("Select one combined shape (Merge, Subtract or Intersect) to separate it."), 6000);
        return;
    }
    const std::vector<ObjectId> parts = body->boolean->operands;
    if (run(std::make_unique<SeparateCommand>(body->id)))
        setSelection(parts);
}

void MainWindow::clearPlate()
{
    if (m_doc.empty())
        return;
    const std::vector<ObjectId> ids = m_doc.topLevelIds();  // copy first: deleting changes the list

    m_stack.beginMacro("Clear Plate");
    try {
        for (ObjectId id : ids)
            for (ObjectId part : m_doc.withDescendants(id))
                m_stack.execute(std::make_unique<RemoveBodyCommand>(part));
    } catch (const std::exception& e) {
        qWarning() << "Clear Plate stopped early:" << e.what();
    }
    m_stack.endMacro();
    qInfo() << "Command: Clear Plate";
}

void MainWindow::loadDemoScene()
{
    addBody(BoxSpec{30.0, 20.0, 15.0});
    addBody(CylinderSpec{10.0, 25.0});
    addBody(BoxSpec{30.0, 20.0, 22.5});
    addBody(CylinderSpec{10.0, 25.0});
    setSelection(m_doc.topLevelIds().at(1));
    refreshScene();  // don't wait for the queued refresh: fitView needs the new items
    m_viewport->fitView();
}

// ---- Files ----------------------------------------------------------------------

void MainWindow::newDocument()
{
    if (!confirmDiscardChanges())
        return;
    m_doc.replaceAll({});
    m_stack.clear();
    m_selection.clear();
    m_expanded.clear();
    m_filePath.clear();
    refreshCommandState();
    m_viewport->resetView();
    qInfo() << "New document";
}

void MainWindow::openDocument()
{
    if (!confirmDiscardChanges())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open model"), startFolder(), kFileFilter);
    if (!path.isEmpty())
        openFile(path);
}

bool MainWindow::openFile(const QString& path)
{
    try {
        std::vector<Body> bodies = file::load(toPath(path));
        m_doc.replaceAll(std::move(bodies));
    } catch (const std::exception& e) {
        qWarning().noquote() << "Open failed:" << path << "-" << e.what();
        QMessageBox::critical(this, tr("Could not open %1").arg(QFileInfo(path).fileName()),
                              QString::fromUtf8(e.what()));
        return false;
    }
    m_stack.clear();  // the history belonged to the previous document
    m_selection.clear();
    m_expanded.clear();
    m_filePath = QFileInfo(path).absoluteFilePath();
    rememberFolderOf(path);
    refreshCommandState();
    refreshScene();
    m_viewport->fitView();
    qInfo().noquote() << "Opened" << m_filePath << "-" << m_doc.bodyCount() << "bodies";
    return true;
}

bool MainWindow::save()
{
    return m_filePath.isEmpty() ? saveAs() : saveTo(m_filePath);
}

// ---- Import -----------------------------------------------------------------------

QString MainWindow::openMeshIn(const std::vector<ObjectId>& ids) const
{
    for (ObjectId id : ids)
        for (ObjectId part : m_doc.withDescendants(id))
            if (const Body* b = m_doc.find(part); b && !b->isBoolean())
                if (const auto* mesh = std::get_if<MeshSpec>(&b->spec); mesh && !mesh->data->closed)
                    return QString::fromStdString(b->name);
    return {};
}

void MainWindow::importFiles(const QStringList& paths)
{
    std::vector<ObjectId> made;
    QStringList notes;
    std::size_t biggest = 0;
    for (const QString& path : paths) {
        const QString file = QFileInfo(path).fileName();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        try {
            const auto askUnits = [this, &file](double largest) {
                QApplication::restoreOverrideCursor();
                const QStringList choices{
                    tr("Metres: x1000, making it %1 mm across").arg(largest * 1000.0, 0, 'f', 1),
                    tr("Inches: x25.4, making it %1 mm across").arg(largest * 25.4, 0, 'f', 1),
                    tr("Centimetres: x10, making it %1 mm across").arg(largest * 10.0, 0, 'f', 1),
                    tr("Keep it: %1 mm across").arg(largest, 0, 'g', 3)};
                const int suggested = largest * 1000.0 <= 2000.0 ? 0 : 1;
                bool ok = false;
                const QString picked = QInputDialog::getItem(
                    this, tr("Import %1").arg(file),
                    tr("This model is only %1 mm across.\nSTL and OBJ files don't record their units: it was "
                       "probably\nsaved in metres (Blender does this) or inches.\n\nWhat units was it made in?")
                        .arg(largest, 0, 'g', 3),
                    choices, suggested, false, &ok);
                QApplication::setOverrideCursor(Qt::WaitCursor);
                if (!ok)
                    return 1.0;
                const int i = static_cast<int>(choices.indexOf(picked));
                const double scale = i == 0 ? 1000.0 : i == 1 ? 25.4 : i == 2 ? 10.0 : 1.0;
                qInfo().noquote() << "Import units: x" << scale;
                return scale;
            };
            const ImportResult result = importFile(m_stack, m_doc, toPath(path), askUnits);
            QApplication::restoreOverrideCursor();
            qInfo().noquote() << "Imported" << path << "-" << result.bodies.size() << "shape(s)";
            for (std::size_t i = 0; i < result.bodies.size(); ++i) {
                made.push_back(result.bodies[i]);
                const RepairReport& r = result.reports[i];
                biggest = std::max(biggest, r.trianglesOut);
                qInfo().noquote() << "  " << QString::fromStdString(m_doc.find(result.bodies[i])->name) << ":"
                                  << r.trianglesOut << "triangles." << QString::fromStdString(r.summary());
                if (r.changedAnything() || r.openEdges > 0 || r.nonManifoldEdges > 0)
                    notes << QStringLiteral("%1: %2").arg(QString::fromStdString(m_doc.find(result.bodies[i])->name),
                                                          QString::fromStdString(r.summary()));
            }
        } catch (const std::exception& e) {
            QApplication::restoreOverrideCursor();
            qWarning() << "Import failed:" << path << e.what();
            QMessageBox::warning(this, tr("Forge3D"),
                                 tr("Could not import %1:\n%2").arg(file, QString::fromUtf8(e.what())));
        }
    }
    if (made.empty())
        return;
    setSelection(made);
    m_viewport->fitView();
    QString status = made.size() == 1 ? tr("Imported 1 shape") : tr("Imported %1 shapes").arg(made.size());
    status += notes.isEmpty() ? tr(" (no repairs needed)") : QString();
    if (biggest > 200000)
        status += tr(". It has many triangles: combining it with other shapes can take a while");
    statusBar()->showMessage(status, 8000);
    if (!notes.isEmpty())
        QMessageBox::information(this, tr("Forge3D"), tr("Imported, with repairs:\n\n%1").arg(notes.join(QStringLiteral("\n\n"))));
}

namespace {
bool droppable(const QString& path)
{
    const QString s = path.toLower();
    return s.endsWith(QLatin1String(".stl")) || s.endsWith(QLatin1String(".3mf")) ||
           s.endsWith(QLatin1String(".obj")) || s.endsWith(QLatin1String(".f3d"));
}
}  // namespace

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls())
        if (url.isLocalFile() && droppable(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    QStringList models;
    QString document;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile() || !droppable(url.toLocalFile()))
            continue;
        const QString path = url.toLocalFile();
        if (path.toLower().endsWith(QLatin1String(".f3d")))
            document = path;
        else
            models << path;
    }
    event->acceptProposedAction();
    if (!document.isEmpty() && confirmDiscardChanges())
        openFile(document);
    if (!models.isEmpty())
        importFiles(models);
}

// ---- Export / print -------------------------------------------------------------

std::vector<ObjectId> MainWindow::exportTargets() const
{
    // The selected shapes (a selected part counts as the shape it is in), else everything.
    std::vector<ObjectId> ids;
    for (ObjectId id : m_selection) {
        ObjectId top = id;
        while (m_doc.parentOf(top) != kInvalidId)
            top = m_doc.parentOf(top);
        if (std::find(ids.begin(), ids.end(), top) == ids.end())
            ids.push_back(top);
    }
    return ids.empty() ? m_doc.topLevelIds() : ids;
}

namespace {

// Builds the triangles (can take a moment for big shapes) with a busy cursor.
std::vector<ExportMesh> buildExportMeshes(const Document& doc, const std::vector<ObjectId>& ids)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        auto meshes = exportMeshes(doc, ids);
        QApplication::restoreOverrideCursor();
        return meshes;
    } catch (...) {
        QApplication::restoreOverrideCursor();
        throw;
    }
}

QString leakWarning(const std::vector<ExportMesh>& meshes)
{
    QStringList open;
    for (const ExportMesh& m : meshes)
        if (m.openEdges > 0)
            open << QString::fromStdString(m.name);
    if (open.isEmpty())
        return {};
    return QObject::tr("These shapes have small gaps in their surface: %1. Most slicers still print them, "
                       "but check the preview.")
        .arg(open.join(QStringLiteral(", ")));
}

}  // namespace

void MainWindow::exportShapes()
{
    const std::vector<ObjectId> ids = exportTargets();
    if (ids.empty())
        return;
    QSettings settings;
    const QString folder = settings.value(QStringLiteral("export/folder"), startFolder()).toString();
    const QString base = m_filePath.isEmpty() ? (ids.size() == 1 ? QString::fromStdString(m_doc.find(ids[0])->name)
                                                                 : tr("Model"))
                                              : QFileInfo(m_filePath).completeBaseName();
    const QString filters = tr("3MF - best for slicers (*.3mf);;STL (*.stl);;OBJ (*.obj)");
    QString filter = settings.value(QStringLiteral("export/filter")).toString();
    const QString ext = filter.contains(QStringLiteral("*.stl")) ? QStringLiteral(".stl")
                        : filter.contains(QStringLiteral("*.obj")) ? QStringLiteral(".obj")
                                                                     : QStringLiteral(".3mf");
    const QString what = ids.size() == 1 ? tr("Export '%1'").arg(QString::fromStdString(m_doc.find(ids[0])->name))
                                         : tr("Export %1 shapes").arg(ids.size());
    QString path = QFileDialog::getSaveFileName(this, what, QDir(folder).filePath(base + ext), filters, &filter);
    if (path.isEmpty())
        return;
    // Add the extension of the chosen type if the name has none we know.
    const QString lower = path.toLower();
    if (!lower.endsWith(QLatin1String(".stl")) && !lower.endsWith(QLatin1String(".3mf")) &&
        !lower.endsWith(QLatin1String(".obj")))
        path += filter.contains(QStringLiteral("*.stl")) ? QStringLiteral(".stl")
                : filter.contains(QStringLiteral("*.obj")) ? QStringLiteral(".obj")
                                                             : QStringLiteral(".3mf");
    settings.setValue(QStringLiteral("export/folder"), QFileInfo(path).absolutePath());
    settings.setValue(QStringLiteral("export/filter"), filter);

    try {
        const auto meshes = buildExportMeshes(m_doc, ids);
        writeMeshes(meshes, toPath(path), formatForPath(toPath(path)));
        std::size_t triangles = 0;
        for (const auto& m : meshes)
            triangles += m.triangleCount();
        qInfo().noquote() << "Exported" << ids.size() << "shape(s)," << triangles << "triangles to" << path;
        statusBar()->showMessage(tr("Exported %1 to %2 (%3 triangles)")
                                     .arg(ids.size() == 1 ? tr("1 shape") : tr("%1 shapes").arg(ids.size()))
                                     .arg(QDir::toNativeSeparators(path))
                                     .arg(triangles),
                                 8000);
        if (const QString warning = leakWarning(meshes); !warning.isEmpty())
            QMessageBox::information(this, tr("Forge3D"), warning);
    } catch (const std::exception& e) {
        qWarning() << "Export failed:" << e.what();
        QMessageBox::warning(this, tr("Forge3D"), tr("Export did not work:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

namespace {

struct SlicerChoice {
    QString label;
    QString path;  // empty: the default app for .3mf files
};

// Slicers installed in their usual places on Windows.
std::vector<SlicerChoice> findSlicers()
{
    std::vector<SlicerChoice> found;
    const QStringList roots{qEnvironmentVariable("ProgramFiles", QStringLiteral("C:/Program Files")),
                            qEnvironmentVariable("ProgramFiles(x86)"),
                            QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath(QStringLiteral("Programs"))};
    struct Known {
        const char* name;
        const char* folderPattern;  // a folder under Program Files (may contain *)
        const char* exe;
    };
    const Known known[] = {
        {"PrusaSlicer", "Prusa3D/PrusaSlicer", "prusa-slicer.exe"},
        {"Bambu Studio", "Bambu Studio", "bambu-studio.exe"},
        {"OrcaSlicer", "OrcaSlicer", "orca-slicer.exe"},
        {"UltiMaker Cura", "UltiMaker Cura*", "UltiMaker-Cura.exe"},
        {"Ultimaker Cura", "Ultimaker Cura*", "Ultimaker-Cura.exe"},
        {"Creality Print", "Creality Print*", "CrealityPrint.exe"},
        {"SuperSlicer", "SuperSlicer", "superslicer.exe"},
    };
    for (const QString& root : roots) {
        if (root.isEmpty() || !QDir(root).exists())
            continue;
        for (const Known& k : known) {
            const QString pattern = QString::fromLatin1(k.folderPattern);
            const int slash = pattern.lastIndexOf(QLatin1Char('/'));
            const QDir parent(QDir(root).filePath(slash < 0 ? QString() : pattern.left(slash)));
            const QString leaf = slash < 0 ? pattern : pattern.mid(slash + 1);
            for (const QString& folder : parent.entryList({leaf}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
                const QString exe = QDir(parent.filePath(folder)).filePath(QString::fromLatin1(k.exe));
                if (QFileInfo::exists(exe))
                    found.push_back({QString::fromLatin1(k.name) + QStringLiteral(" (") + folder + QLatin1Char(')'),
                                     QDir::cleanPath(exe)});
            }
        }
    }
    return found;
}

}  // namespace

bool MainWindow::chooseSlicer()
{
    std::vector<SlicerChoice> choices = findSlicers();
    choices.push_back({tr("The program Windows opens .3mf files with"), QString()});
    const QString browse = tr("Browse for a slicer program...");
    QStringList labels;
    for (const auto& c : choices)
        labels << c.label;
    labels << browse;

    QSettings settings;
    const QString current = settings.value(QStringLiteral("slicer/path")).toString();
    int start = 0;
    for (int i = 0; i < static_cast<int>(choices.size()); ++i)
        if (choices[static_cast<std::size_t>(i)].path == current && settings.contains(QStringLiteral("slicer/path")))
            start = i;
    bool ok = false;
    const QString picked = QInputDialog::getItem(this, tr("Choose Slicer"),
                                                 tr("Send to Slicer opens your shapes in:"), labels, start, false, &ok);
    if (!ok)
        return false;
    QString path;
    if (picked == browse) {
        path = QFileDialog::getOpenFileName(this, tr("Choose your slicer program"),
                                            qEnvironmentVariable("ProgramFiles", QStringLiteral("C:/Program Files")),
                                            tr("Programs (*.exe);;All files (*)"));
        if (path.isEmpty())
            return false;
    } else {
        path = choices[static_cast<std::size_t>(labels.indexOf(picked))].path;
    }
    settings.setValue(QStringLiteral("slicer/path"), path);
    qInfo().noquote() << "Slicer:" << (path.isEmpty() ? QStringLiteral("default app for .3mf") : path);
    statusBar()->showMessage(tr("Send to Slicer will use: %1").arg(path.isEmpty() ? picked : QDir::toNativeSeparators(path)),
                             6000);
    return true;
}

void MainWindow::sendToSlicer()
{
    const std::vector<ObjectId> ids = exportTargets();
    if (ids.empty())
        return;
    QSettings settings;
    if (!settings.contains(QStringLiteral("slicer/path")) && !chooseSlicer())
        return;
    QString slicer = settings.value(QStringLiteral("slicer/path")).toString();
    if (!slicer.isEmpty() && !QFileInfo::exists(slicer)) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("The slicer chosen before can't be found:\n%1\nPlease choose it again.")
                                     .arg(QDir::toNativeSeparators(slicer)));
        if (!chooseSlicer())
            return;
        slicer = settings.value(QStringLiteral("slicer/path")).toString();
    }

    // A 3MF in a temporary folder (slicers read 3MF best: names, millimetres).
    const QString folder = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(
        QStringLiteral("Forge3D"));
    QDir().mkpath(folder);
    QString base = m_filePath.isEmpty() ? tr("Forge3D model") : QFileInfo(m_filePath).completeBaseName();
    const QString path = QDir(folder).filePath(base + QStringLiteral(".3mf"));
    try {
        const auto meshes = buildExportMeshes(m_doc, ids);
        writeMeshes(meshes, toPath(path), ExportFormat::ThreeMf);
        if (const QString warning = leakWarning(meshes); !warning.isEmpty())
            statusBar()->showMessage(warning, 10000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Could not prepare the file for the slicer:\n%1")
                                                      .arg(QString::fromUtf8(e.what())));
        return;
    }

    const bool started = slicer.isEmpty() ? QDesktopServices::openUrl(QUrl::fromLocalFile(path))
                                          : QProcess::startDetached(slicer, {QDir::toNativeSeparators(path)});
    qInfo().noquote() << "Send to slicer:" << path << (started ? "opened" : "FAILED to open");
    if (!started)
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("Could not start the slicer. The file is ready at:\n%1\n\nUse File > Choose Slicer to "
                                "pick another program.")
                                 .arg(QDir::toNativeSeparators(path)));
    else
        statusBar()->showMessage(tr("Sent %1 to the slicer").arg(ids.size() == 1 ? tr("1 shape")
                                                                                 : tr("%1 shapes").arg(ids.size())),
                                 6000);
}

bool MainWindow::saveAs()
{
    const QString suggested = m_filePath.isEmpty()
                                  ? QDir(startFolder()).filePath(tr("Untitled.f3d"))
                                  : m_filePath;
    QString path = QFileDialog::getSaveFileName(this, tr("Save model as"), suggested, kFileFilter);
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QString::fromLatin1(file::kExtension), Qt::CaseInsensitive))
        path += QString::fromLatin1(file::kExtension);
    return saveTo(path);
}

bool MainWindow::saveTo(const QString& path)
{
    try {
        const std::string createdBy = std::string(build::kAppName) + " " + build::kVersion;
        file::save(m_doc, toPath(path), createdBy);
    } catch (const std::exception& e) {
        qWarning().noquote() << "Save failed:" << path << "-" << e.what();
        QMessageBox::critical(this, tr("Could not save %1").arg(QFileInfo(path).fileName()),
                              QString::fromUtf8(e.what()));
        return false;
    }
    m_filePath = QFileInfo(path).absoluteFilePath();
    rememberFolderOf(path);
    m_stack.setClean();  // also refreshes the title
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(m_filePath)), 4000);
    qInfo().noquote() << "Saved" << m_filePath << "-" << m_doc.bodyCount() << "bodies";
    return true;
}

bool MainWindow::confirmDiscardChanges()
{
    if (m_stack.isClean())
        return true;
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    const auto answer = QMessageBox::question(
        this, tr("Forge3D"), tr("Save changes to %1 before closing it?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (confirmDiscardChanges())
        event->accept();
    else
        event->ignore();
}

// ---- Keeping the UI in step ----------------------------------------------------

void MainWindow::scheduleSceneRefresh()
{
    // A macro can change many bodies at once; redraw once, after they all land.
    if (m_refreshQueued)
        return;
    m_refreshQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_refreshQueued = false;
        refreshScene();
    });
}

void MainWindow::refreshScene()
{
    std::erase_if(m_selection, [this](ObjectId id) { return !m_doc.find(id); });

    std::vector<DisplayItem> items;
    std::size_t triangles = 0;
    const auto colourOf = [](ObjectId id) {
        return kPalette.at(static_cast<std::size_t>((id - 1) % kPalette.size()));
    };
    try {
        // Everything on the plate...
        if (m_embossMode.active && !m_doc.find(m_embossMode.target))
            endEmboss();  // e.g. Undo removed the shape
        for (ObjectId id : m_doc.topLevelIds()) {
            DisplayItem item;
            item.id = id;
            item.mesh = m_doc.mesh(id);  // rebuilt only if the shape changed
            item.model = toQMatrix(m_doc.worldTransform(id));
            if (m_embossMode.active && id == m_embossMode.target && m_embossMode.preview) {
                // The shape with the words on, as it would be. A new result's coordinates
                // are the plate's; an edited one keeps its placement.
                item.mesh = m_embossMode.preview;
                if (!m_embossMode.editing)
                    item.model = QMatrix4x4();
            }
            item.color = colourOf(id);
            item.selected = isSelected(id);
            triangles += item.mesh->triangleCount();
            items.push_back(std::move(item));
        }
        // ...plus selected parts of combined shapes, see-through, where they are.
        for (ObjectId id : m_selection) {
            if (m_doc.isTopLevel(id))
                continue;
            DisplayItem item;
            item.id = id;
            item.mesh = m_doc.mesh(id);
            item.model = toQMatrix(m_doc.worldTransform(id));
            item.color = colourOf(id);
            item.selected = true;
            item.ghost = true;
            items.push_back(std::move(item));
        }
    } catch (const std::exception& e) {
        qCritical() << "Could not build geometry for display:" << e.what();
        statusBar()->showMessage(tr("A shape could not be built: %1").arg(QString::fromUtf8(e.what())), 8000);
    }
    if (m_splitMode.active && !m_doc.find(m_splitMode.target))
        endSplit();  // e.g. Undo removed the shape being cut
    addSplitItems(items);
    m_viewport->setItems(std::move(items));

    const Body* selected = m_doc.find(selectedOne());
    // "Round" can't be ticked for an oval cylinder. Undo, redo or a typed radius can turn
    // the selected cylinder oval without a new selection, so check on every refresh.
    // (A round cylinder keeps the user's choice: they may untick Round before dragging.)
    if (selected) {
        if (!selected->isBoolean() && !isRound(selected->spec) && m_keepRound) {
            m_keepRound = false;
            m_properties->setKeepRound(false);
        }
    }
    m_viewport->setGizmo(toolOpen() ? std::nullopt : selectionGizmo(), m_gizmoMode);

    // Properties: what the selected body is made of, or what it belongs to.
    QString context;
    if (selected) {
        if (selected->isBoolean()) {
            QStringList names;
            for (ObjectId part : selected->boolean->operands)
                names << QString::fromStdString(m_doc.find(part)->name);
            const QString joiner = selected->boolean->op == BooleanOp::Subtract ? QStringLiteral(" − ")
                                   : selected->boolean->op == BooleanOp::Merge  ? QStringLiteral(" + ")
                                                                                : QStringLiteral(" ∩ ");
            switch (selected->boolean->op) {
            case BooleanOp::Split: context = tr("Made from: %1, cut by a plane").arg(names.join(QString())); break;
            case BooleanOp::Mirror: context = tr("Made from: %1, mirrored").arg(names.join(QString())); break;
            case BooleanOp::Group: context = tr("A group of: %1").arg(names.join(QStringLiteral(", "))); break;
            case BooleanOp::Emboss:
                context = tr("Made from: %1, with the words \"%2\" %3")
                              .arg(names.join(QString()),
                                   QString::fromStdString(selected->boolean->emboss ? selected->boolean->emboss->text
                                                                                    : std::string()),
                                   selected->boolean->emboss && !selected->boolean->emboss->raised ? tr("engraved")
                                                                                                    : tr("raised"));
                break;
            default: context = tr("Made from: %1").arg(names.join(joiner)); break;
            }
        } else if (const auto* mesh = std::get_if<MeshSpec>(&selected->spec);
                   mesh && m_doc.isTopLevel(selected->id)) {
            context = tr("Imported: %1 triangles.").arg(mesh->data->indices.size() / 3) +
                      (mesh->data->closed ? QString() : tr(" Not watertight: it can't be combined or split."));
        } else if (const ObjectId owner = m_doc.parentOf(selected->id); owner != kInvalidId) {
            context = tr("Part of '%1'. Its position is measured inside that shape.")
                          .arg(QString::fromStdString(m_doc.find(owner)->name));
        }
    } else if (m_selection.size() > 1) {
        context = tr("%1 shapes selected. Use Merge, Subtract or Intersect to combine them; for Subtract, "
                     "the first one you picked ('%2') is kept.\n\nAlign Shapes moves the others to line up "
                     "with '%2', which stays put.")
                      .arg(m_selection.size())
                      .arg(QString::fromStdString(m_doc.find(m_selection.front())->name));
    }
    m_properties->setBody(selected, context, selected && !m_doc.isTopLevel(selected->id));
    refreshObjectList();

    const auto count = static_cast<int>(m_doc.topLevelIds().size());
    m_modelInfo->setText(count == 0 ? tr("Empty plate - click Add Shape")
                                    : (count == 1 ? tr("1 shape") : tr("%1 shapes").arg(count)) +
                                          tr("  |  %1 triangles").arg(triangles));
    const bool anySelected = !m_selection.empty();
    const bool allTopLevel = std::all_of(m_selection.begin(), m_selection.end(),
                                         [this](ObjectId id) { return m_doc.isTopLevel(id); });
    m_delete->setEnabled(anySelected && allTopLevel);
    m_deselect->setEnabled(anySelected);
    for (QAction* a : {m_merge, m_subtract, m_intersect})
        a->setEnabled(m_selection.size() >= 2 && allTopLevel);
    m_separate->setEnabled(selected && selected->isBoolean() && m_doc.isTopLevel(selected->id));
    m_alignMenu->menuAction()->setEnabled(m_selection.size() >= 2 && allTopLevel);
    for (QAction* a : {m_duplicate, m_copy, m_cut, m_mirrorMenu->menuAction()})
        a->setEnabled(anySelected && allTopLevel && !toolOpen());
    m_group->setEnabled(m_selection.size() >= 2 && allTopLevel);
    m_paste->setEnabled(!m_clipboard.empty() && !toolOpen());
    m_export->setEnabled(!m_doc.empty());
    m_sendToSlicer->setEnabled(!m_doc.empty());
    m_split->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_emboss->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_clear->setEnabled(!m_doc.empty());
}

void MainWindow::addTreeItem(QTreeWidgetItem* parent, ObjectId id)
{
    const Body* body = m_doc.find(id);
    if (!body)
        return;
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_objects);
    item->setText(0, QString::fromStdString(body->name));
    item->setData(0, Qt::UserRole, QVariant::fromValue<ObjectId>(id));
    item->setToolTip(0, QString::fromStdString(typeName(*body)));
    if (body->isBoolean()) {
        QFont bold = item->font(0);
        bold.setBold(true);
        item->setFont(0, bold);
        for (ObjectId part : body->boolean->operands)
            addTreeItem(item, part);
        item->setExpanded(m_expanded.count(id) > 0);
    }
    item->setSelected(isSelected(id));
}

void MainWindow::refreshObjectList()
{
    m_syncingList = true;
    m_objects->clear();
    for (ObjectId id : m_doc.topLevelIds())
        addTreeItem(nullptr, id);
    m_syncingList = false;
}

void MainWindow::onTreeSelectionChanged()
{
    if (m_syncingList)
        return;
    std::set<ObjectId> picked;
    for (QTreeWidgetItem* item : m_objects->selectedItems())
        picked.insert(item->data(0, Qt::UserRole).value<ObjectId>());
    // Keep the order things were picked in (it decides what Subtract keeps).
    std::vector<ObjectId> next;
    for (ObjectId id : m_selection)
        if (picked.erase(id))
            next.push_back(id);
    for (QTreeWidgetItem* item : m_objects->selectedItems()) {
        const ObjectId id = item->data(0, Qt::UserRole).value<ObjectId>();
        if (picked.erase(id))
            next.push_back(id);
    }
    // Refresh later: rebuilding the tree inside its own selection signal is unsafe.
    QTimer::singleShot(0, this, [this, next] { setSelection(next); });
}

void MainWindow::refreshCommandState()
{
    m_undo->setEnabled(m_stack.canUndo());
    m_redo->setEnabled(m_stack.canRedo());
    m_undo->setText(m_stack.canUndo() ? tr("&Undo %1").arg(QString::fromStdString(m_stack.undoLabel()))
                                      : tr("&Undo"));
    m_redo->setText(m_stack.canRedo() ? tr("&Redo %1").arg(QString::fromStdString(m_stack.redoLabel()))
                                      : tr("&Redo"));

    // "[*]" is where Qt shows the unsaved-changes star.
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    setWindowTitle(QStringLiteral("%1[*] - %2 %3")
                       .arg(name, QString::fromLatin1(build::kAppName), QString::fromLatin1(build::kVersion)));
    setWindowModified(!m_stack.isClean());
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About Forge3D"),
        tr("<h3>Forge3D %1</h3>"
           "<p>Parametric 3D modeling with sculpting.</p>"
           "<p>Qt %2 &middot; OpenCASCADE %3 &middot; %4</p>"
           "<p>Copyright &copy; 2026 the Forge3D contributors.<br>"
           "This program is free software: you can redistribute it and/or modify it under the terms "
           "of the GNU General Public License, version 3 or any later version. It comes with "
           "ABSOLUTELY NO WARRANTY.</p>")
            .arg(QString::fromLatin1(build::kVersion), QString::fromLatin1(qVersion()),
                 QString::fromLatin1(OCC_VERSION_COMPLETE), m_viewport->glVersionString()));
}

}  // namespace forge::app
