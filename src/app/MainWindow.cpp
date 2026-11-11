// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "BuildInfo.h"
#include "Log.h"
#include "PropertiesPanel.h"
#include "Viewport.h"

#include "forge/core/FileFormat.h"
#include "forge/core/Picking.h"
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
    if (const auto* box = std::get_if<BoxSpec>(&spec))
        return box->width;
    return 2.0 * std::get<CylinderSpec>(spec).radiusX;
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

    m_deselect = new QAction(tr("Select &None"), this);
    m_deselect->setShortcut(Qt::Key_Escape);
    connect(m_deselect, &QAction::triggered, this, [this] {
        if (m_dragState.kind != Drag::Kind::None)
            cancelDrag();  // Esc during a drag puts the body back
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

    m_addBox = new QAction(tr("Add Box"), this);
    m_addBox->setToolTip(tr("Add a 30 x 20 x 15 mm box beside the existing bodies"));
    connect(m_addBox, &QAction::triggered, this, [this] { addBody(BoxSpec{30.0, 20.0, 15.0}); });

    m_addCylinder = new QAction(tr("Add Cylinder"), this);
    m_addCylinder->setToolTip(tr("Add a cylinder, 10 mm radius and 25 mm tall"));
    connect(m_addCylinder, &QAction::triggered, this, [this] { addBody(CylinderSpec{10.0, 25.0}); });

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
    m_separate->setToolTip(tr("Separate: break a combined shape back into its parts, where they are"));
    connect(m_separate, &QAction::triggered, this, &MainWindow::separateSelected);

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
    file->addAction(m_quit);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_undo);
    edit->addAction(m_redo);
    edit->addSeparator();
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
    bar->addSeparator();
    bar->addAction(m_undo);
    bar->addAction(m_redo);
    bar->addSeparator();
    bar->addAction(m_addBox);
    bar->addAction(m_addCylinder);
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
    propsDock->setWidget(m_properties);
    propsDock->setMinimumWidth(280);
    addDockWidget(Qt::RightDockWidgetArea, propsDock);

    connect(m_properties, &PropertiesPanel::renameRequested, this, [this](ObjectId id, const QString& name) {
        run(std::make_unique<RenameBodyCommand>(id, name.toStdString()));
    });
    connect(m_properties, &PropertiesPanel::specRequested, this, [this](ObjectId id, const PrimitiveSpec& spec) {
        run(std::make_unique<SetSpecCommand>(id, spec));
    });
    connect(m_properties, &PropertiesPanel::uniformChanged, m_uniformAction, &QAction::setChecked);
    connect(m_properties, &PropertiesPanel::separateRequested, this, &MainWindow::separateSelected);
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
                run(std::make_unique<SetPlacementCommand>(id, placement, /*mergeable=*/false));
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
    const ObjectId previousOne = selectedOne();
    m_selection = std::move(ids);
    if (const Body* body = m_doc.find(selectedOne()); body && body->id != previousOne) {
        // A newly selected cylinder starts in "Round" mode only if it is round.
        const auto* cyl = std::get_if<CylinderSpec>(&body->spec);
        m_keepRound = !cyl || cyl->isRound();
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
    if (!body || !m_doc.isTopLevel(id))
        return std::nullopt;  // parts inside a result are edited in Properties for now
    const Transform t = m_doc.worldTransform(id);
    const Vec3 c = t.apply(pivotOf(*body));
    const auto axis = [&t](double x, double y, double z) {
        const Vec3 d = t.applyDirection({x, y, z});
        return QVector3D(static_cast<float>(d[0]), static_cast<float>(d[1]), static_cast<float>(d[2]));
    };
    view::GizmoTarget target;
    target.centre = QVector3D(static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]));
    target.bodyAxes = {axis(1, 0, 0), axis(0, 1, 0), axis(0, 0, 1)};
    if (body->isBoolean()) {
        target.resizable = false;  // resize a combined shape through its parts
    } else if (const auto* box = std::get_if<BoxSpec>(&body->spec)) {
        target.halfExtents = QVector3D(float(box->width / 2), float(box->depth / 2), float(box->height / 2));
    } else {
        const auto& cyl = std::get<CylinderSpec>(body->spec);
        target.halfExtents = QVector3D(float(cyl.radiusX), float(cyl.radiusY), float(cyl.height / 2));
        target.round = true;
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

void MainWindow::onLeftPressed(const QVector3D& origin, const QVector3D& direction,
                               const view::GizmoHandle& handle, const QPointF& pixel, Qt::KeyboardModifiers modifiers)
{
    m_dragState = Drag{};
    m_pressOnHandle = false;

    const Body* selected = m_doc.find(selectedOne());
    const auto frame = gizmoTargetFor(selectedOne());
    if (handle.valid() && selected && frame) {
        m_pressOnHandle = true;
        m_dragState.before = *selected;
        m_dragState.handle = handle;
        m_dragState.frame = *frame;
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
        double* coord = d.handle.axis == 0 ? &next.placement.x : d.handle.axis == 1 ? &next.placement.y
                                                                                  : &next.placement.z;
        *coord += delta;
        status = tr("Move  %1 %2 mm").arg(QChar("XYZ"[d.handle.axis])).arg(delta, 0, 'f', 1);
        break;
    }
    case Drag::Kind::Ring: {
        d.turned += view::screenAngle(d.centrePixel, d.lastPixel, pixel);
        d.lastPixel = pixel;
        const QVector3D axis(d.handle.axis == 0, d.handle.axis == 1, d.handle.axis == 2);
        // Anticlockwise on screen is a positive turn when the axis points at the viewer.
        const float facing = QVector3D::dotProduct(axis, m_viewport->camera().eyePosition() - d.frame.centre);
        const double degrees = step(d.turned * (facing >= 0.0f ? 1.0 : -1.0), kTurnStep);
        next.placement = rotatedAboutPlateAxis(d.before, d.handle.axis, degrees);
        status = tr("Rotate around %1  %2°").arg(QChar("XYZ"[d.handle.axis])).arg(degrees, 0, 'f', 1);
        break;
    }
    case Drag::Kind::Face: {
        const QVector3D dir = d.frame.bodyAxes.at(static_cast<std::size_t>(d.handle.axis)) *
                              static_cast<float>(d.handle.side);
        const auto s = view::closestOnLine(origin, direction, d.frame.centre, dir);
        if (!s)
            return;
        double value = dimension(d.before.spec, d.handle.axis) + (*s - d.startParam);
        if (snap)
            value = std::max(kSizeStep, view::snapTo(value, kSizeStep));
        ResizeOptions options;
        options.uniform = m_uniform;
        options.keepRound = m_keepRound;
        next = resizedFace(d.before, d.handle.axis, d.handle.side, value, options);

        const bool cylinder = std::holds_alternative<CylinderSpec>(next.spec);
        QString what;
        if (d.handle.axis == 2)
            what = tr("Height");
        else if (cylinder)
            what = (m_keepRound || m_uniform) ? tr("Radius") : (d.handle.axis == 0 ? tr("Radius X") : tr("Radius Y"));
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
    if (d.kind != Drag::Kind::None && d.moved)
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
    body.name = m_doc.uniqueName(typeName(spec));
    body.placement = suggestPlacement(m_doc, footprintWidth(spec));
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

    // Check the result first: an empty result (e.g. Intersect of shapes that don't
    // touch) would vanish from the plate, which looks like a bug.
    try {
        if (isEmptySolid(m_doc.previewCombine(op, m_selection))) {
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
    BooleanSpec spec;
    spec.op = op;
    spec.operands = m_selection;
    // Turn around the middle of everything being combined.
    Bounds all;
    for (ObjectId id : m_selection) {
        const Bounds b = m_doc.worldBounds(id);
        if (b.valid) {
            all.expand(b.min[0], b.min[1], b.min[2]);
            all.expand(b.max[0], b.max[1], b.max[2]);
        }
    }
    const auto centre = all.center();
    spec.pivot = {centre[0], centre[1], centre[2]};
    result.boolean = spec;
    const ObjectId id = result.id;
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
        for (ObjectId id : m_doc.topLevelIds()) {
            DisplayItem item;
            item.id = id;
            item.mesh = m_doc.mesh(id);  // rebuilt only if the shape changed
            item.model = toQMatrix(m_doc.worldTransform(id));
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
    m_viewport->setItems(std::move(items));

    const Body* selected = m_doc.find(selectedOne());
    // "Round" can't be ticked for an oval cylinder. Undo, redo or a typed radius can turn
    // the selected cylinder oval without a new selection, so check on every refresh.
    // (A round cylinder keeps the user's choice: they may untick Round before dragging.)
    if (selected) {
        if (const auto* cyl = std::get_if<CylinderSpec>(&selected->spec);
            !selected->isBoolean() && cyl && !cyl->isRound() && m_keepRound) {
            m_keepRound = false;
            m_properties->setKeepRound(false);
        }
    }
    m_viewport->setGizmo(gizmoTargetFor(selectedOne()), m_gizmoMode);

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
            context = tr("Made from: %1").arg(names.join(joiner));
        } else if (const ObjectId owner = m_doc.parentOf(selected->id); owner != kInvalidId) {
            context = tr("Part of '%1'. Its position is measured inside that shape.")
                          .arg(QString::fromStdString(m_doc.find(owner)->name));
        }
    } else if (m_selection.size() > 1) {
        context = tr("%1 shapes selected. Use Merge, Subtract or Intersect to combine them; for Subtract, "
                     "the first one you picked ('%2') is kept.")
                      .arg(m_selection.size())
                      .arg(QString::fromStdString(m_doc.find(m_selection.front())->name));
    }
    m_properties->setBody(selected, context, selected && !m_doc.isTopLevel(selected->id));
    refreshObjectList();

    const auto count = static_cast<int>(m_doc.topLevelIds().size());
    m_modelInfo->setText(count == 0 ? tr("Empty plate - click Add Box or Add Cylinder")
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
