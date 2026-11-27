// SPDX-License-Identifier: GPL-3.0-or-later
#include "SceneryWindow.h"

#include "BuildInfo.h"
#include "BuildingPanel.h"
#include "Theme.h"
#include "Viewport.h"
#include "forge/core/Commands.h"
#include "forge/core/Export.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Updates.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QPointer>
#include <QUrl>
#include <QVBoxLayout>

#include <filesystem>
#include <thread>

namespace forge::scenery {

namespace {

// Scenery's own settings.
QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                     QStringLiteral("Forge3D Scenery"));
}
// Forge3D's, for what the two share: the printer's plate.
QSettings forgePreferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

std::filesystem::path toPath(const QString& s)
{
#ifdef _WIN32
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::path(s.toStdString());
#endif
}

QMatrix4x4 toQMatrix(const Transform& t)
{
    const auto f = [&t](int r, int c) { return static_cast<float>(t.m[r][c]); };
    return QMatrix4x4(f(0, 0), f(0, 1), f(0, 2), f(0, 3),  //
                      f(1, 0), f(1, 1), f(1, 2), f(1, 3),  //
                      f(2, 0), f(2, 1), f(2, 2), f(2, 3),  //
                      0.0f, 0.0f, 0.0f, 1.0f);
}

QString versionText()
{
    const QString label = QString::fromLatin1(build::kReleaseLabel);
    return label.isEmpty() ? QString::fromLatin1(build::kVersion)
                           : QStringLiteral("%1 %2").arg(QString::fromLatin1(build::kVersion), label);
}

}  // namespace

SceneryWindow::SceneryWindow()
{
    setWindowIcon(QIcon(QStringLiteral(":/scenery/scenery.png")));
    resize(1400, 860);

    // The ribbon above the view.
    auto* central = new QWidget;
    auto* column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(buildRibbon());
    m_viewport = new app::Viewport;
    column->addWidget(m_viewport, 1);
    setCentralWidget(central);

    // The shapes in the model.
    auto* dock = new QDockWidget(tr("Objects"), this);
    dock->setObjectName(QStringLiteral("objects"));
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_objects = new QListWidget;
    m_objects->setSelectionMode(QAbstractItemView::SingleSelection);
    m_objects->setToolTip(tr("Click a shape to pick it; double-click a building to change it"));
    dock->setWidget(m_objects);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    connect(m_objects, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_build.active)
            return;
        m_selected = row >= 0 && row < int(m_rows.size()) ? m_rows[std::size_t(row)] : kInvalidId;
        refreshScene();
        refreshCommands();
    });
    connect(m_objects, &QListWidget::itemDoubleClicked, this, [this] {
        if (!m_build.active && m_editBuilding->isEnabled())
            startBuilding(m_selected);
    });

    // The building designer's panel, on the right while it's open.
    m_panelDock = new QDockWidget(tr("Building"), this);
    m_panelDock->setObjectName(QStringLiteral("building"));
    m_panelDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_buildingPanel = new BuildingPanel;
    scroll->setWidget(m_buildingPanel);
    scroll->setMinimumWidth(330);
    m_panelDock->setWidget(scroll);
    addDockWidget(Qt::RightDockWidgetArea, m_panelDock);
    m_panelDock->hide();
    m_detailTimer = new QTimer(this);
    m_detailTimer->setSingleShot(true);
    m_detailTimer->setInterval(500);
    connect(m_detailTimer, &QTimer::timeout, this, [this] { requestDetail(); });
    connect(m_buildingPanel, &BuildingPanel::changed, this, [this] {
        updateBuildingPreview(true);
        m_detailTimer->start();
    });
    connect(m_buildingPanel, &BuildingPanel::placeRequested, this, [this](int kind) {
        m_build.placing = kind;
        statusBar()->showMessage(kind < 0 ? QString()
                                          : tr("Click on a wall where the %1 goes.")
                                                .arg(kind == int(OpeningKind::Door) ? tr("door") : tr("window")));
    });
    connect(m_buildingPanel, &BuildingPanel::selectionChanged, this, [this] {
        updateBuildingPreview(true);  // the marker
        m_detailTimer->start();
    });
    connect(m_buildingPanel, &BuildingPanel::applyRequested, this, [this] { applyBuilding(); });
    connect(m_buildingPanel, &BuildingPanel::cancelRequested, this, [this] { endBuilding(); });
    connect(m_viewport, &app::Viewport::clicked, this,
            [this](const QVector3D& o, const QVector3D& d, Qt::KeyboardModifiers) { onViewClicked(o, d); });
    connect(m_viewport, &app::Viewport::leftPressed, this,
            [this](const QVector3D& o, const QVector3D& d, const view::GizmoHandle&, const QPointF&,
                   Qt::KeyboardModifiers) { onViewPressed(o, d); });
    connect(m_viewport, &app::Viewport::leftDragged, this,
            [this](const QVector3D& o, const QVector3D& d, const QPointF&, Qt::KeyboardModifiers) { onViewDragged(o, d); });
    connect(m_viewport, &app::Viewport::leftReleased, this, [this](bool) { onViewReleased(); });
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] {
        if (!m_build.active)
            return;
        if (m_build.placing >= 0) {  // stop placing first
            m_build.placing = -1;
            m_buildingPanel->setPlacing(-1);
            statusBar()->clearMessage();
        } else {
            endBuilding();
        }
    });

    m_status = new QLabel;
    statusBar()->addPermanentWidget(m_status);

    // The printer's plate, as set in Forge3D.
    const QSettings forge = forgePreferences();
    m_viewport->setPlateSize(forge.value(QStringLiteral("plate/x"), 220.0).toFloat(),
                             forge.value(QStringLiteral("plate/y"), 220.0).toFloat());
    m_viewport->setPlateHeight(forge.value(QStringLiteral("plate/z"), 250.0).toFloat());

    buildMenus();
    if (const auto saved = modelscale::parse(preferences().value(QStringLiteral("scale"), QStringLiteral("1:35"))
                                                 .toString()
                                                 .toStdString()))
        useScale(*saved);
    else
        useScale({35});
    m_stack.setListener([this] {
        refreshScene();
        refreshTitle();
    });
    refreshScene();
    refreshTitle();
    statusBar()->showMessage(tr("Welcome to Forge3D Scenery. Open a model, or set the scale the scenery tools will "
                                "build to. Buildings, trees, rocks and terrain are on their way."),
                             12000);
    if (preferences().value(QStringLiteral("updates/checkAtStart"), true).toBool())
        checkForUpdates(true);
}

SceneryWindow::~SceneryWindow()
{
    if (m_detail.valid())
        m_detail.wait();
}

QWidget* SceneryWindow::buildRibbon()
{
    auto* tabs = new QTabWidget;
    tabs->setDocumentMode(true);
    tabs->setMaximumHeight(118);

    auto* page = new QWidget;
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(10, 6, 10, 6);

    // The scale, its label resting on its list.
    auto* scaleColumn = new QVBoxLayout;
    scaleColumn->setSpacing(0);
    scaleColumn->addWidget(new QLabel(tr("Model scale:")));
    m_scaleBox = new QComboBox;
    m_scaleBox->setEditable(true);
    m_scaleBox->setInsertPolicy(QComboBox::NoInsert);
    m_scaleBox->setMinimumWidth(150);
    for (const auto& n : modelscale::common())
        m_scaleBox->addItem(QString::fromStdString(n.name));
    m_scaleBox->lineEdit()->setPlaceholderText(QStringLiteral("1:35"));
    m_scaleBox->setToolTip(tr("The scale the scenery tools build to: a real 194 mm brick is 5.5 mm at 1:35. Pick "
                              "one, or type your own (1:76, 1:87.1)"));
    scaleColumn->addWidget(m_scaleBox);
    scaleColumn->addStretch(1);
    row->addLayout(scaleColumn);
    connect(m_scaleBox, &QComboBox::activated, this, [this](int i) {
        if (i >= 0 && i < int(modelscale::common().size()))
            useScale(modelscale::common()[std::size_t(i)].scale);
    });
    connect(m_scaleBox->lineEdit(), &QLineEdit::editingFinished, this, [this] {
        if (const auto s = modelscale::parse(m_scaleBox->currentText().toStdString()))
            useScale(*s);
        else
            useScale(m_scale);  // back to what it was
    });

    // Buildings.
    row->addSpacing(18);
    auto* buildings = new QVBoxLayout;
    buildings->setSpacing(4);
    m_newBuilding = new QPushButton(tr("New Building"));
    m_newBuilding->setToolTip(tr("Design a building: brick, stone blocks, logs or wood planks, from real sizes at the "
                                 "model scale, with doors, windows and a roof"));
    m_editBuilding = new QPushButton(tr("Change Building"));
    m_editBuilding->setToolTip(tr("Change the building picked in Objects (or double-click it there)"));
    buildings->addWidget(m_newBuilding);
    buildings->addWidget(m_editBuilding);
    buildings->addStretch(1);
    row->addLayout(buildings);
    connect(m_newBuilding, &QPushButton::clicked, this, [this] { startBuilding(kInvalidId); });
    connect(m_editBuilding, &QPushButton::clicked, this, [this] { startBuilding(m_selected); });

    auto* coming = new QLabel(tr("<b>Coming next:</b> rocks and boulders, terrain, and trees."));
    coming->setWordWrap(true);
    coming->setStyleSheet(QStringLiteral("color: palette(mid);"));
    row->addSpacing(24);
    row->addWidget(coming, 1);
    tabs->addTab(page, tr("Scenery"));
    return tabs;
}

void SceneryWindow::buildMenus()
{
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New"), QKeySequence::New, this, [this] { newModel(); });
    file->addAction(tr("&Open..."), QKeySequence::Open, this, [this] { openModel(); });
    file->addAction(tr("&Save"), QKeySequence::Save, this, [this] { saveModel(); });
    file->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, [this] { saveModelAs(); });
    file->addSeparator();
    file->addAction(tr("&Export for Printing..."), QKeySequence(Qt::CTRL | Qt::Key_E), this,
                    [this] { exportModel(); });
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    QAction* undo = edit->addAction(tr("&Undo"), QKeySequence::Undo, this, [this] { m_stack.undo(); });
    QAction* redo = edit->addAction(tr("&Redo"), QKeySequence::Redo, this, [this] { m_stack.redo(); });
    edit->addSeparator();
    edit->addAction(tr("&Delete"), QKeySequence::Delete, this, [this] { deleteSelected(); });
    connect(edit, &QMenu::aboutToShow, this, [this, undo, redo] {
        undo->setEnabled(m_stack.canUndo());
        redo->setEnabled(m_stack.canRedo());
    });

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(tr("&Fit All"), QKeySequence(Qt::Key_F), this, [this] { m_viewport->fitView(); });
    view->addAction(tr("&Home View"), QKeySequence(Qt::Key_Home), this, [this] { m_viewport->resetView(); });
    QMenu* theme = view->addMenu(tr("&Theme"));
    const auto setTheme = [this](app::theme::Choice c) {
        app::theme::save(c);
        app::theme::apply(*qApp, c);
        m_viewport->themeChanged();
    };
    theme->addAction(tr("Like &Windows"), this, [setTheme] { setTheme(app::theme::Choice::System); });
    theme->addAction(tr("&Light"), this, [setTheme] { setTheme(app::theme::Choice::Light); });
    theme->addAction(tr("&Dark"), this, [setTheme] { setTheme(app::theme::Choice::Dark); });

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("Check for &Updates..."), this, [this] { checkForUpdates(false); });
    QAction* atStart = help->addAction(tr("Check for Updates at &Start"));
    atStart->setCheckable(true);
    atStart->setChecked(preferences().value(QStringLiteral("updates/checkAtStart"), true).toBool());
    connect(atStart, &QAction::toggled, this,
            [](bool on) { preferences().setValue(QStringLiteral("updates/checkAtStart"), on); });
    help->addSeparator();
    help->addAction(tr("&About Forge3D Scenery"), this, [this] { showAbout(); });
}

void SceneryWindow::useScale(const modelscale::Scale& s)
{
    m_scale = s;
    preferences().setValue(QStringLiteral("scale"), QString::fromStdString(modelscale::format(s)));
    // Show it by its usual name if it has one.
    QString shown = QString::fromStdString(modelscale::format(s));
    for (const auto& n : modelscale::common())
        if (n.scale == s)
            shown = QString::fromStdString(n.name);
    const QSignalBlocker block(m_scaleBox);
    const int i = m_scaleBox->findText(shown);
    if (i >= 0)
        m_scaleBox->setCurrentIndex(i);
    else
        m_scaleBox->setEditText(shown);
    refreshTitle();
}

void SceneryWindow::refreshScene()
{
    std::vector<app::DisplayItem> items;
    std::size_t triangles = 0;
    if (m_selected != kInvalidId && !m_doc.find(m_selected))
        m_selected = kInvalidId;
    const QSignalBlocker block(m_objects);
    m_objects->clear();
    m_rows.clear();
    try {
        for (ObjectId id : m_doc.topLevelIds()) {
            const auto mesh = m_doc.mesh(id);
            if (!mesh)
                continue;
            m_rows.push_back(id);
            m_objects->addItem(QString::fromStdString(m_doc.find(id)->name));
            if (id == m_selected)
                m_objects->setCurrentRow(m_objects->count() - 1);
            if (m_build.active && (id == m_build.editing || id == m_build.roof)) {
                triangles += mesh->triangleCount();
                continue;  // shown as it's being changed instead
            }
            app::DisplayItem item;
            item.id = id;
            item.mesh = mesh;
            item.model = toQMatrix(m_doc.worldTransform(id));
            const std::uint32_t c = m_doc.colourOf(id);
            item.color = QVector3D(float((c >> 16) & 0xFF) / 255.0f, float((c >> 8) & 0xFF) / 255.0f,
                                   float(c & 0xFF) / 255.0f);
            item.selected = !m_build.active && id == m_selected;
            items.push_back(std::move(item));
            triangles += mesh->triangleCount();
        }
        if (m_build.active) {
            QMatrix4x4 at;
            at.translate(float(m_build.x), float(m_build.y), float(m_build.z));
            const auto add = [&](const std::shared_ptr<const Mesh>& m, std::uint64_t id, const QVector3D& colour,
                                 float alpha) {
                if (!m || m->empty())
                    return;
                app::DisplayItem item;
                item.id = id;
                item.mesh = m;
                item.model = at;
                item.color = colour;
                item.alpha = alpha;
                item.helper = alpha < 1.0f;
                items.push_back(std::move(item));
            };
            add(m_build.walls, 0xB0000001ull, QVector3D(0.72f, 0.42f, 0.32f), 1.0f);
            add(m_build.roofMesh, 0xB0000002ull, QVector3D(0.45f, 0.38f, 0.36f), 1.0f);
            add(m_build.marker, 0xB0000003ull, QVector3D(1.0f, 0.6f, 0.1f), 0.6f);
        }
    } catch (const std::exception& e) {
        statusBar()->showMessage(tr("Something couldn't be shown: %1").arg(QString::fromUtf8(e.what())), 8000);
    }
    m_viewport->setItems(std::move(items));
    refreshCommands();
    const int shapes = m_objects->count();
    m_status->setText((shapes == 1 ? tr("1 shape") : tr("%1 shapes").arg(shapes)) + QStringLiteral("  |  ") +
                      tr("%1 triangles").arg(triangles));
}

void SceneryWindow::refreshTitle()
{
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    setWindowTitle(QStringLiteral("%1%2 - %3 - Forge3D Scenery %4")
                       .arg(name, modified() ? QStringLiteral("*") : QString(),
                            QString::fromStdString(modelscale::format(m_scale)), versionText()));
}

bool SceneryWindow::modified() const
{
    return !m_stack.isClean();
}

bool SceneryWindow::askToSave()
{
    if (!modified())
        return true;
    const auto answer = QMessageBox::question(this, tr("Forge3D Scenery"), tr("Save the changes to this model?"),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel)
        return false;
    return answer == QMessageBox::Discard || saveModel();
}

void SceneryWindow::newModel()
{
    if (!askToSave())
        return;
    endBuilding();
    m_stack.clear();
    m_doc.replaceAll({});
    m_filePath.clear();
    m_stack.setClean();
    refreshScene();
    refreshTitle();
}

void SceneryWindow::openModel()
{
    if (!askToSave())
        return;
    QSettings s = preferences();
    const QString start = s.value(QStringLiteral("folder"),
                                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                              .toString();
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Open a model"), start, tr("Forge3D models (*.f3d);;All files (*)"));
    if (path.isEmpty())
        return;
    s.setValue(QStringLiteral("folder"), QFileInfo(path).absolutePath());
    openFile(path);
}

void SceneryWindow::openFile(const QString& path)
{
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        std::vector<Body> bodies = file::load(toPath(path));
        endBuilding();
        m_stack.clear();
        m_doc.replaceAll(std::move(bodies));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D Scenery"),
                             tr("%1 couldn't be opened:\n%2").arg(QFileInfo(path).fileName(), QString::fromUtf8(e.what())));
        return;
    }
    m_filePath = path;
    m_stack.setClean();
    qInfo().noquote() << "Opened" << path << "-" << m_doc.topLevelIds().size() << "shapes";
    refreshScene();
    refreshTitle();
    m_viewport->fitView();
}

bool SceneryWindow::saveModel()
{
    return m_filePath.isEmpty() ? saveModelAs() : saveTo(m_filePath);
}

bool SceneryWindow::saveModelAs()
{
    QSettings s = preferences();
    const QString start = s.value(QStringLiteral("folder"),
                                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                              .toString();
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save the model"),
        m_filePath.isEmpty() ? QDir(start).filePath(tr("Scenery.f3d")) : m_filePath, tr("Forge3D models (*.f3d)"));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QLatin1String(".f3d"), Qt::CaseInsensitive))
        path += QStringLiteral(".f3d");
    s.setValue(QStringLiteral("folder"), QFileInfo(path).absolutePath());
    return saveTo(path);
}

bool SceneryWindow::saveTo(const QString& path)
{
    try {
        file::save(m_doc, toPath(path), std::string(build::kAppName) + " " + build::kVersion);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D Scenery"),
                             tr("The model couldn't be saved:\n%1").arg(QString::fromUtf8(e.what())));
        return false;
    }
    m_filePath = path;
    m_stack.setClean();
    refreshTitle();
    statusBar()->showMessage(tr("Saved %1. It opens in Forge3D too.").arg(QFileInfo(path).fileName()), 6000);
    return true;
}

void SceneryWindow::exportModel()
{
    const std::vector<ObjectId> ids = m_doc.topLevelIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("There's nothing to export yet."), 5000);
        return;
    }
    QSettings s = preferences();
    const QString start = s.value(QStringLiteral("exportFolder"),
                                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                              .toString();
    const QString base = m_filePath.isEmpty() ? tr("Scenery") : QFileInfo(m_filePath).completeBaseName();
    QString filter;
    QString path = QFileDialog::getSaveFileName(this, tr("Export for printing"), QDir(start).filePath(base + ".3mf"),
                                                tr("3MF - best for slicers (*.3mf);;STL (*.stl);;OBJ (*.obj)"),
                                                &filter);
    if (path.isEmpty())
        return;
    const QString lower = path.toLower();
    if (!lower.endsWith(QLatin1String(".stl")) && !lower.endsWith(QLatin1String(".3mf")) &&
        !lower.endsWith(QLatin1String(".obj")))
        path += filter.contains(QStringLiteral("*.stl")) ? QStringLiteral(".stl")
                : filter.contains(QStringLiteral("*.obj")) ? QStringLiteral(".obj")
                                                             : QStringLiteral(".3mf");
    s.setValue(QStringLiteral("exportFolder"), QFileInfo(path).absolutePath());
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        writeMeshes(exportMeshes(m_doc, ids), toPath(path), formatForPath(toPath(path)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D Scenery"), tr("It couldn't be exported:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    statusBar()->showMessage(tr("Exported %1.").arg(QFileInfo(path).fileName()), 6000);
}

void SceneryWindow::checkForUpdates(bool quietly)
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QString::fromLatin1(updates::kReleasesApi)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Forge3D-Scenery/%1").arg(QString::fromLatin1(build::kVersion)));
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, quietly] {
        reply->deleteLater();
        std::optional<updates::Release> newer;
        QString problem;
        if (reply->error() != QNetworkReply::NoError) {
            problem = reply->errorString();
        } else {
            try {
                newer = updates::newerRelease(reply->readAll().toStdString(), build::kVersion, updates::kSceneryTags,
                                              build::kAppName);
            } catch (const std::exception& e) {
                problem = QString::fromUtf8(e.what());
            }
        }
        if (!problem.isEmpty()) {
            qWarning().noquote() << "Update check failed:" << problem;
            if (!quietly)
                QMessageBox::warning(this, tr("Check for Updates"),
                                     tr("Couldn't check for updates (%1).").arg(problem.toHtmlEscaped()));
            return;
        }
        if (!newer) {
            qInfo() << "Update check: up to date";
            if (!quietly)
                QMessageBox::information(this, tr("Check for Updates"),
                                         tr("You have the newest Forge3D Scenery (%1).").arg(versionText()));
            return;
        }
        const QString name = QString::fromStdString(newer->name), url = QString::fromStdString(newer->url);
        qInfo().noquote() << "Update check: newer release" << name << url;
        if (quietly) {
            auto* note = new QLabel(tr("<a href=\"%1\">%2 is out: download</a>").arg(url, name.toHtmlEscaped()));
            note->setOpenExternalLinks(true);
            statusBar()->addPermanentWidget(note);
            return;
        }
        if (QMessageBox::question(this, tr("Check for Updates"),
                                  tr("%1 is out (you have %2). Download it?").arg(name, versionText())) ==
            QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl(url));
    });
}

void SceneryWindow::showAbout()
{
    QMessageBox::about(this, tr("About Forge3D Scenery"),
                       tr("<b>Forge3D Scenery %1</b><p>Scenery for models and dioramas: buildings, trees, rocks and "
                          "terrain, to your scale. The companion to Forge3D: the same engine, and the same .f3d "
                          "files, so a model opens in either.</p><p>Free software under the GNU GPL, version 3 or "
                          "later.</p>")
                           .arg(versionText()));
}

// ---- The building designer --------------------------------------------------------------

void SceneryWindow::refreshCommands()
{
    const Body* b = m_doc.find(m_selected);
    const bool building = b && (b->building || (b->roofOf != kInvalidId && m_doc.find(b->roofOf)));
    m_newBuilding->setEnabled(!m_build.active);
    m_editBuilding->setEnabled(!m_build.active && building);
}

void SceneryWindow::startBuilding(ObjectId editing)
{
    if (m_build.active)
        return;
    BuildMode mode;
    mode.active = true;
    BuildingSpec spec;
    if (editing != kInvalidId) {
        const Body* b = m_doc.find(editing);
        if (b && b->roofOf != kInvalidId)  // the roof picked: its building
            b = m_doc.find(b->roofOf);
        if (!b || !b->building)
            return;
        mode.editing = b->id;
        spec = *b->building;
        mode.x = b->placement.x - spec.offset[0];
        mode.y = b->placement.y - spec.offset[1];
        mode.z = b->placement.z - spec.offset[2];
        for (ObjectId id : m_doc.topLevelIds())
            if (const Body* r = m_doc.find(id); r && r->roofOf == b->id)
                mode.roof = id;
    } else {
        spec = building::defaults(WallMaterial::Brick);
        spec.scale = m_scale.ratio;
        // On the plate's middle, or beside what's there.
        if (!m_doc.empty()) {
            Bounds all;
            for (ObjectId id : m_doc.topLevelIds()) {
                const Bounds b = m_doc.worldBounds(id);
                all.expand(b.min[0], b.min[1], b.min[2]);
                all.expand(b.max[0], b.max[1], b.max[2]);
            }
            mode.x = all.max[0] + spec.width / spec.scale / 2 + spec.overhang / spec.scale + 10.0;
            mode.y = all.center()[1];
        }
    }
    m_build = mode;
    m_buildingPanel->start(spec, editing != kInvalidId);
    m_panelDock->show();
    if (auto* scroll = qobject_cast<QScrollArea*>(m_panelDock->widget()))
        scroll->verticalScrollBar()->setValue(0);
    m_viewport->setFocus();
    statusBar()->showMessage(tr("Building: set it up on the right; Add Door or Add Window, then click on a wall. Esc "
                                "cancels."));
    updateBuildingPreview(true);
    if (editing == kInvalidId)
        m_viewport->fitView();
    requestDetail();
    refreshCommands();
}

void SceneryWindow::endBuilding()
{
    if (!m_build.active)
        return;
    m_detailTimer->stop();
    m_build = BuildMode{};
    m_panelDock->hide();
    statusBar()->clearMessage();
    refreshScene();
}

void SceneryWindow::applyBuilding()
{
    if (!m_build.active)
        return;
    const BuildingSpec spec = m_buildingPanel->spec();
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (m_build.editing != kInvalidId) {
            building::update(m_stack, m_doc, m_build.editing, spec);
            m_selected = m_build.editing;
        } else {
            m_selected = building::add(m_stack, m_doc, spec, tr("Building").toStdString(), m_build.x, m_build.y);
        }
    } catch (const std::exception& e) {
        m_buildingPanel->setStatus(tr("It couldn't be made: %1").arg(QString::fromUtf8(e.what())), true);
        return;
    }
    endBuilding();
    statusBar()->showMessage(tr("Building made. Double-click it in Objects to change it."), 8000);
}

// The building as it's set up: quickly (plain walls) now, in full a moment later.
void SceneryWindow::updateBuildingPreview(bool quick)
{
    if (!m_build.active)
        return;
    const BuildingSpec spec = m_buildingPanel->spec();
    ++m_build.generation;
    try {
        if (quick) {
            const building::Made made = building::make(spec, true);
            m_build.walls = std::make_shared<const Mesh>(
                meshkernel::displayMesh(meshkernel::fromTriangles(made.walls.positions, made.walls.indices)));
            m_build.roofMesh =
                made.roof.empty() ? nullptr
                                  : std::make_shared<const Mesh>(meshkernel::displayMesh(
                                        meshkernel::fromTriangles(made.roof.positions, made.roof.indices)));
        }
        const int picked = m_buildingPanel->selectedOpening();
        m_build.marker = picked >= 0 && picked < int(spec.openings.size())
                             ? std::make_shared<const Mesh>(meshkernel::displayMesh(meshkernel::fromTriangles(
                                   building::openingMarker(spec, spec.openings[std::size_t(picked)]).positions,
                                   building::openingMarker(spec, spec.openings[std::size_t(picked)]).indices)))
                             : nullptr;
        m_buildingPanel->setStatus({}, false);
    } catch (const std::exception& e) {
        m_buildingPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    refreshScene();
}

void SceneryWindow::requestDetail()
{
    if (!m_build.active)
        return;
    const BuildingSpec spec = m_buildingPanel->spec();
    const int generation = m_build.generation;
    QPointer<SceneryWindow> self(this);
    if (m_detail.valid())
        m_detail.wait();  // one at a time
    m_detail = std::async(std::launch::async, [self, spec, generation] {
        std::shared_ptr<const Mesh> walls, roof;
        QStringList notes;
        QString problem;
        try {
            const building::Made made = building::make(spec);
            walls = std::make_shared<const Mesh>(
                meshkernel::displayMesh(meshkernel::fromTriangles(made.walls.positions, made.walls.indices)));
            if (!made.roof.empty())
                roof = std::make_shared<const Mesh>(
                    meshkernel::displayMesh(meshkernel::fromTriangles(made.roof.positions, made.roof.indices)));
            for (const auto& n : made.notes)
                notes << QString::fromStdString(n);
        } catch (const std::exception& e) {
            problem = QString::fromUtf8(e.what());
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, walls, roof, notes, problem] {
                if (!self || !self->m_build.active || self->m_build.generation != generation)
                    return;  // changed since: a newer one is on its way
                if (!problem.isEmpty()) {
                    self->m_buildingPanel->setStatus(problem, true);
                    return;
                }
                self->m_build.walls = walls;
                self->m_build.roofMesh = roof;
                self->m_buildingPanel->setNotes(notes);
                self->refreshScene();
            },
            Qt::QueuedConnection);
    });
}

std::optional<building::Spot> SceneryWindow::spotFromRay(const QVector3D& origin, const QVector3D& direction) const
{
    const BuildingSpec spec = m_buildingPanel->spec();
    return building::spotAt(spec, {origin.x() - m_build.x, origin.y() - m_build.y, origin.z() - m_build.z},
                            {direction.x(), direction.y(), direction.z()});
}

void SceneryWindow::onViewClicked(const QVector3D& origin, const QVector3D& direction)
{
    if (!m_build.active) {
        return;
    }
    const auto spot = spotFromRay(origin, direction);
    BuildingSpec spec = m_buildingPanel->spec();
    if (m_build.placing >= 0) {
        if (!spot) {
            statusBar()->showMessage(tr("That's not on a wall: click on the outside of one."), 5000);
            return;
        }
        const BuildingOpening o = building::openingAt(spec, OpeningKind(m_build.placing), *spot);
        if (building::clashes(spec, o)) {
            statusBar()->showMessage(tr("That's on top of another door or window: click somewhere clear."), 5000);
            return;
        }
        spec.openings.push_back(o);
        m_build.placing = -1;
        m_buildingPanel->setPlacing(-1);
        m_buildingPanel->setSpec(spec);
        m_buildingPanel->selectOpening(int(spec.openings.size()) - 1);
        statusBar()->showMessage(tr("Placed. Drag it to move it; set its size on the right."), 6000);
        updateBuildingPreview(true);
        m_detailTimer->start();
        return;
    }
    const auto picked = spot ? building::pickedOpening(spec, *spot) : std::nullopt;
    m_buildingPanel->selectOpening(picked ? int(*picked) : -1);
    updateBuildingPreview(false);
}

void SceneryWindow::onViewPressed(const QVector3D& origin, const QVector3D& direction)
{
    if (!m_build.active || m_build.placing >= 0)
        return;
    const auto spot = spotFromRay(origin, direction);
    if (!spot)
        return;
    const BuildingSpec spec = m_buildingPanel->spec();
    const auto picked = building::pickedOpening(spec, *spot);
    if (!picked)
        return;
    const BuildingOpening& o = spec.openings[*picked];
    m_build.dragging = int(*picked);
    m_build.grabAlong = spot->along - o.along;
    m_build.grabUp = spot->up - o.bottom;
    m_buildingPanel->selectOpening(int(*picked));
}

void SceneryWindow::onViewDragged(const QVector3D& origin, const QVector3D& direction)
{
    if (!m_build.active || m_build.dragging < 0)
        return;
    BuildingSpec spec = m_buildingPanel->spec();
    if (m_build.dragging >= int(spec.openings.size()))
        return;
    const auto spot = spotFromRay(origin, direction);
    if (!spot)
        return;
    BuildingOpening& o = spec.openings[std::size_t(m_build.dragging)];
    o.wall = spot->wall;  // onto another wall, if it's dragged round a corner
    o.along = spot->along - m_build.grabAlong;
    if (o.kind == OpeningKind::Window)
        o.bottom = spot->up - m_build.grabUp;
    o = building::keptIn(spec, o);
    if (building::clashes(spec, o, std::size_t(m_build.dragging)))
        return;  // kept where it was: it would be on top of another
    m_buildingPanel->setSpec(spec);
    m_buildingPanel->selectOpening(m_build.dragging);
    updateBuildingPreview(true);
}

void SceneryWindow::onViewReleased()
{
    if (!m_build.active || m_build.dragging < 0)
        return;
    m_build.dragging = -1;
    m_detailTimer->start();
}

void SceneryWindow::deleteSelected()
{
    if (m_build.active) {
        // The picked door or window.
        BuildingSpec spec = m_buildingPanel->spec();
        const int i = m_buildingPanel->selectedOpening();
        if (i >= 0 && i < int(spec.openings.size())) {
            spec.openings.erase(spec.openings.begin() + i);
            m_buildingPanel->setSpec(spec);
            m_buildingPanel->selectOpening(-1);
            updateBuildingPreview(true);
            m_detailTimer->start();
        }
        return;
    }
    if (m_selected == kInvalidId)
        return;
    m_stack.execute(std::make_unique<RemoveBodyCommand>(m_selected));
    m_selected = kInvalidId;
}

void SceneryWindow::closeEvent(QCloseEvent* event)
{
    if (m_build.active)
        endBuilding();
    if (askToSave())
        event->accept();
    else
        event->ignore();
}

}  // namespace forge::scenery
