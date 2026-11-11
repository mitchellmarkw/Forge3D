// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "BuildInfo.h"
#include "Log.h"
#include "PropertiesPanel.h"
#include "Viewport.h"

#include "forge/core/FileFormat.h"
#include "forge/core/Picking.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
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

const QString kFileFilter = QObject::tr("Forge3D model (*.f3d)");
const QString kLastFolderKey = QStringLiteral("files/lastFolder");

QMatrix4x4 modelMatrix(const Placement& p)
{
    QMatrix4x4 m;
    m.translate(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
    m.rotate(static_cast<float>(p.rotationZ), 0.0f, 0.0f, 1.0f);
    return m;
}

double footprintWidth(const PrimitiveSpec& spec)
{
    if (const auto* box = std::get_if<BoxSpec>(&spec))
        return box->width;
    return 2.0 * std::get<CylinderSpec>(spec).radius;
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

    createActions();
    createMenus();
    createToolBar();
    createDocks();
    createStatusBar();

    m_doc.addListener([this](const Document::Change& change) {
        if (change.kind == Document::ChangeKind::Removed && change.id == m_selected)
            m_selected = kInvalidId;  // e.g. undo of an Add
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
    m_delete->setToolTip(tr("Delete the selected body (Del)"));
    connect(m_delete, &QAction::triggered, this, &MainWindow::deleteSelected);

    m_deselect = new QAction(tr("Select &None"), this);
    m_deselect->setShortcut(Qt::Key_Escape);
    connect(m_deselect, &QAction::triggered, this, [this] { setSelection(kInvalidId); });

    m_addBox = new QAction(tr("Add Box"), this);
    m_addBox->setToolTip(tr("Add a 30 x 20 x 15 mm box beside the existing bodies"));
    connect(m_addBox, &QAction::triggered, this, [this] { addBody(BoxSpec{30.0, 20.0, 15.0}); });

    m_addCylinder = new QAction(tr("Add Cylinder"), this);
    m_addCylinder->setToolTip(tr("Add a cylinder, 10 mm radius and 25 mm tall"));
    connect(m_addCylinder, &QAction::triggered, this, [this] { addBody(CylinderSpec{10.0, 25.0}); });

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
    bar->addAction(m_delete);
    bar->addAction(m_clear);
}

void MainWindow::createDocks()
{
    // Objects list (left)
    m_objects = new QListWidget;
    m_objects->setSelectionMode(QAbstractItemView::SingleSelection);
    m_objects->setToolTip(tr("Every body on the plate. Click one to select it."));
    connect(m_objects, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (m_syncingList)
            return;
        setSelection(item ? item->data(Qt::UserRole).value<ObjectId>() : kInvalidId);
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
    statusBar()->showMessage(tr("Left-click: select   |   Right-drag: orbit   |   Middle-drag or "
                                "Shift+right-drag: pan   |   Wheel: zoom   |   F: fit   |   Home: reset view"));

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

void MainWindow::setSelection(ObjectId id)
{
    if (id != kInvalidId && !m_doc.find(id))
        id = kInvalidId;
    if (id == m_selected)
        return;
    m_selected = id;
    refreshScene();  // highlight, list, properties
}

void MainWindow::onViewportClicked(const QVector3D& origin, const QVector3D& direction)
{
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {direction.x(), direction.y(), direction.z()};
    try {
        const auto hit = pick(m_doc, ray);
        setSelection(hit ? hit->id : kInvalidId);  // clicking empty space clears the selection
    } catch (const std::exception& e) {
        qWarning() << "Picking failed:" << e.what();
    }
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
    if (m_selected == kInvalidId)
        return;
    run(std::make_unique<RemoveBodyCommand>(m_selected));
}

void MainWindow::clearPlate()
{
    if (m_doc.empty())
        return;
    std::vector<ObjectId> ids;  // copy first: deleting changes the list
    for (const Body& b : m_doc.bodies())
        ids.push_back(b.id);

    m_stack.beginMacro("Clear Plate");
    try {
        for (ObjectId id : ids)
            m_stack.execute(std::make_unique<RemoveBodyCommand>(id));
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
    setSelection(m_doc.bodies()[1].id);
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
    m_selected = kInvalidId;
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
    m_selected = kInvalidId;
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
    if (m_selected != kInvalidId && !m_doc.find(m_selected))
        m_selected = kInvalidId;

    std::vector<DisplayItem> items;
    items.reserve(m_doc.bodyCount());
    std::size_t triangles = 0;
    try {
        for (const Body& body : m_doc.bodies()) {
            DisplayItem item;
            item.id = body.id;
            item.mesh = m_doc.mesh(body.id);  // tessellates only if the shape changed
            item.model = modelMatrix(body.placement);
            item.color = kPalette.at(static_cast<std::size_t>((body.id - 1) % kPalette.size()));
            item.selected = body.id == m_selected;
            triangles += item.mesh->triangleCount();
            items.push_back(std::move(item));
        }
    } catch (const std::exception& e) {
        qCritical() << "Could not build geometry for display:" << e.what();
    }
    m_viewport->setItems(std::move(items));
    m_properties->setBody(m_doc.find(m_selected));
    refreshObjectList();

    const auto count = static_cast<int>(m_doc.bodyCount());
    m_modelInfo->setText(count == 0 ? tr("Empty plate - click Add Box or Add Cylinder")
                                    : (count == 1 ? tr("1 body") : tr("%1 bodies").arg(count)) +
                                          tr("  |  %1 triangles").arg(triangles));
    m_delete->setEnabled(m_selected != kInvalidId);
    m_deselect->setEnabled(m_selected != kInvalidId);
    m_clear->setEnabled(!m_doc.empty());
}

void MainWindow::refreshObjectList()
{
    m_syncingList = true;
    m_objects->clear();
    for (const Body& body : m_doc.bodies()) {
        auto* item = new QListWidgetItem(QString::fromStdString(body.name), m_objects);
        item->setData(Qt::UserRole, QVariant::fromValue<ObjectId>(body.id));
        item->setToolTip(QString::fromStdString(typeName(body.spec)));
        if (body.id == m_selected)
            m_objects->setCurrentItem(item);
    }
    if (m_selected == kInvalidId)
        m_objects->setCurrentItem(nullptr);
    m_syncingList = false;
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
