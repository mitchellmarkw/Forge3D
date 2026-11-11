// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "BuildInfo.h"
#include "Log.h"
#include "Viewport.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QtDebug>

#include <Standard_Version.hxx>

#include <array>
#include <exception>
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

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    resize(1280, 800);

    m_viewport = new Viewport(this);
    setCentralWidget(m_viewport);
    connect(m_viewport, &Viewport::firstFrameRendered, this, &MainWindow::firstFrameRendered);

    createActions();
    createMenus();
    createToolBar();
    createStatusBar();

    m_doc.addListener([this](const Document::Change&) { scheduleSceneRefresh(); });
    m_stack.setListener([this] { refreshCommandState(); });

    refreshScene();
    refreshCommandState();
    m_viewport->resetView();
}

MainWindow::~MainWindow()
{
    m_stack.setListener({});  // the stack outlives nothing, but be explicit
}

// ---- Setup ----------------------------------------------------------------------

void MainWindow::createActions()
{
    m_new = new QAction(tr("&New"), this);
    m_new->setShortcut(QKeySequence::New);
    connect(m_new, &QAction::triggered, this, &MainWindow::newDocument);

    m_quit = new QAction(tr("E&xit"), this);
    m_quit->setShortcut(QKeySequence::Quit);
    connect(m_quit, &QAction::triggered, this, &QWidget::close);

    m_undo = new QAction(tr("&Undo"), this);
    m_undo->setShortcut(QKeySequence::Undo);  // Ctrl+Z
    connect(m_undo, &QAction::triggered, this, [this] { m_stack.undo(); });

    m_redo = new QAction(tr("&Redo"), this);
    m_redo->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(m_redo, &QAction::triggered, this, [this] { m_stack.redo(); });

    m_addBox = new QAction(tr("Add Box"), this);
    m_addBox->setToolTip(tr("Add a 30 x 20 x 15 mm box beside the existing bodies"));
    connect(m_addBox, &QAction::triggered, this, &MainWindow::addBox);

    m_addCylinder = new QAction(tr("Add Cylinder"), this);
    m_addCylinder->setToolTip(tr("Add a cylinder, 10 mm radius and 25 mm tall"));
    connect(m_addCylinder, &QAction::triggered, this, &MainWindow::addCylinder);

    m_taller = new QAction(tr("Make Last Taller"), this);
    m_taller->setToolTip(tr("Make the newest body 50% taller (rebuilds its geometry)"));
    connect(m_taller, &QAction::triggered, this, &MainWindow::makeLastTaller);

    m_deleteLast = new QAction(tr("Delete Last"), this);
    m_deleteLast->setToolTip(tr("Delete the newest body"));
    connect(m_deleteLast, &QAction::triggered, this, &MainWindow::deleteLast);

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
    file->addSeparator();
    file->addAction(m_quit);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(m_undo);
    edit->addAction(m_redo);

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_fit);
    view->addAction(m_resetView);

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
    bar->addAction(m_undo);
    bar->addAction(m_redo);
    bar->addSeparator();
    bar->addAction(m_addBox);
    bar->addAction(m_addCylinder);
    bar->addAction(m_taller);
    bar->addSeparator();
    bar->addAction(m_deleteLast);
    bar->addAction(m_clear);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(
        tr("Right-drag: orbit   |   Middle-drag or Shift+right-drag: pan   |   Wheel: zoom   |   "
           "F: fit   |   Home: reset view"));

    m_modelInfo = new QLabel(this);
    m_glInfo = new QLabel(this);
    statusBar()->addPermanentWidget(m_modelInfo);
    statusBar()->addPermanentWidget(m_glInfo);

    connect(m_viewport, &Viewport::glInfoReady, this, [this](const QString& summary) {
        m_glInfo->setText(m_viewport->glVersionString());
        m_glInfo->setToolTip(summary);
    });
}

void MainWindow::loadDemoScene()
{
    addBox();
    addCylinder();
    addBox();
    makeLastTaller();
    addCylinder();
    refreshScene();  // don't wait for the queued refresh: fitView needs the new items
    m_viewport->fitView();
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
        return false;
    }
}

void MainWindow::addBox()
{
    Body body;
    body.id = m_doc.allocateId();
    body.spec = BoxSpec{30.0, 20.0, 15.0};
    body.name = m_doc.uniqueName("Box");
    body.placement = suggestPlacement(m_doc, footprintWidth(body.spec));
    run(std::make_unique<AddBodyCommand>(std::move(body)));
}

void MainWindow::addCylinder()
{
    Body body;
    body.id = m_doc.allocateId();
    body.spec = CylinderSpec{10.0, 25.0};
    body.name = m_doc.uniqueName("Cylinder");
    body.placement = suggestPlacement(m_doc, footprintWidth(body.spec));
    run(std::make_unique<AddBodyCommand>(std::move(body)));
}

void MainWindow::makeLastTaller()
{
    if (m_doc.empty())
        return;
    const Body& last = m_doc.bodies().back();
    PrimitiveSpec taller = last.spec;
    std::visit([](auto& p) { p.height *= 1.5; }, taller);
    run(std::make_unique<SetSpecCommand>(last.id, taller));
}

void MainWindow::deleteLast()
{
    if (m_doc.empty())
        return;
    run(std::make_unique<RemoveBodyCommand>(m_doc.bodies().back().id));
}

void MainWindow::clearPlate()
{
    if (m_doc.empty())
        return;
    // Copy the ids first: deleting changes the list we would be walking.
    std::vector<ObjectId> ids;
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
}

void MainWindow::newDocument()
{
    if (!confirmDiscardChanges())
        return;
    while (!m_doc.empty())
        m_doc.removeBody(m_doc.bodies().back().id);
    m_stack.clear();
    m_viewport->resetView();
    qInfo() << "New document";
}

bool MainWindow::confirmDiscardChanges()
{
    if (m_stack.isClean() || m_doc.empty())
        return true;
    // Saving arrives in slice 3; until then, the only choice is to discard.
    const auto answer = QMessageBox::question(
        this, tr("Forge3D"), tr("Discard the current model?"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
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
            triangles += item.mesh->triangleCount();
            items.push_back(std::move(item));
        }
    } catch (const std::exception& e) {
        qCritical() << "Could not build geometry for display:" << e.what();
    }
    m_viewport->setItems(std::move(items));

    const auto count = static_cast<int>(m_doc.bodyCount());
    m_modelInfo->setText(count == 0 ? tr("Empty plate - click Add Box or Add Cylinder")
                                    : (count == 1 ? tr("1 body") : tr("%1 bodies").arg(count)) +
                                          tr("  |  %1 triangles").arg(triangles));

    const bool any = !m_doc.empty();
    m_deleteLast->setEnabled(any);
    m_taller->setEnabled(any);
    m_clear->setEnabled(any);
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
    setWindowTitle(QStringLiteral("Untitled[*] - %1 %2")
                       .arg(QString::fromLatin1(build::kAppName), QString::fromLatin1(build::kVersion)));
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
