// SPDX-License-Identifier: GPL-3.0-or-later
#include "SceneryWindow.h"

#include "BuildInfo.h"
#include "BuildingPanel.h"
#include "RockPanel.h"
#include "TerrainPanel.h"
#include "TreePanel.h"
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
#include <tuple>
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
    m_objects->setToolTip(tr("Click a shape to pick it; double-click a building, rock, terrain or tree to change it"));
    dock->setWidget(m_objects);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    connect(m_objects, &QListWidget::currentRowChanged, this, [this](int row) {
        if (designing())
            return;
        m_selected = row >= 0 && row < int(m_rows.size()) ? m_rows[std::size_t(row)] : kInvalidId;
        refreshScene();
        refreshCommands();
    });
    connect(m_objects, &QListWidget::itemDoubleClicked, this, [this] {
        if (!designing() && m_editBuilding->isEnabled())
            changeSelected();
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

    // The rock designer's panel, in the same place.
    m_rockDock = new QDockWidget(tr("Rock"), this);
    m_rockDock->setObjectName(QStringLiteral("rock"));
    m_rockDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    auto* rockScroll = new QScrollArea;
    rockScroll->setWidgetResizable(true);
    rockScroll->setFrameShape(QFrame::NoFrame);
    m_rockPanel = new RockPanel;
    rockScroll->setWidget(m_rockPanel);
    rockScroll->setMinimumWidth(330);
    m_rockDock->setWidget(rockScroll);
    addDockWidget(Qt::RightDockWidgetArea, m_rockDock);
    m_rockDock->hide();
    m_rockTimer = new QTimer(this);
    m_rockTimer->setSingleShot(true);
    m_rockTimer->setInterval(500);
    connect(m_rockTimer, &QTimer::timeout, this, [this] { requestRockDetail(); });
    connect(m_rockPanel, &RockPanel::changed, this, [this] {
        updateRockPreview();
        m_rockTimer->start();
    });
    connect(m_rockPanel, &RockPanel::applyRequested, this, [this] { applyRock(); });
    connect(m_rockPanel, &RockPanel::cancelRequested, this, [this] { endRock(); });

    // The tree designer's panel, in the same place.
    m_treeDock = new QDockWidget(tr("Tree"), this);
    m_treeDock->setObjectName(QStringLiteral("tree"));
    m_treeDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    auto* treeScroll = new QScrollArea;
    treeScroll->setWidgetResizable(true);
    treeScroll->setFrameShape(QFrame::NoFrame);
    m_treePanel = new TreePanel;
    treeScroll->setWidget(m_treePanel);
    treeScroll->setMinimumWidth(330);
    m_treeDock->setWidget(treeScroll);
    addDockWidget(Qt::RightDockWidgetArea, m_treeDock);
    m_treeDock->hide();
    m_treeTimer = new QTimer(this);
    m_treeTimer->setSingleShot(true);
    m_treeTimer->setInterval(500);
    connect(m_treeTimer, &QTimer::timeout, this, [this] { requestTreeDetail(); });
    connect(m_treePanel, &TreePanel::changed, this, [this] {
        updateTreePreview();
        m_treeTimer->start();
    });
    connect(m_treePanel, &TreePanel::applyRequested, this, [this] { applyTree(); });
    connect(m_treePanel, &TreePanel::cancelRequested, this, [this] { endTree(); });

    // The terrain designer's panel, in the same place.
    m_terrainDock = new QDockWidget(tr("Terrain"), this);
    m_terrainDock->setObjectName(QStringLiteral("terrain"));
    m_terrainDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    auto* terrainScroll = new QScrollArea;
    terrainScroll->setWidgetResizable(true);
    terrainScroll->setFrameShape(QFrame::NoFrame);
    m_terrainPanel = new TerrainPanel;
    terrainScroll->setWidget(m_terrainPanel);
    terrainScroll->setMinimumWidth(340);
    m_terrainDock->setWidget(terrainScroll);
    addDockWidget(Qt::RightDockWidgetArea, m_terrainDock);
    m_terrainDock->hide();
    m_terrainTimer = new QTimer(this);
    m_terrainTimer->setSingleShot(true);
    m_terrainTimer->setInterval(500);
    connect(m_terrainTimer, &QTimer::timeout, this, [this] { requestTerrainDetail(); });
    connect(m_terrainPanel, &TerrainPanel::changed, this, [this] {
        updateTerrainPreview(true);
        m_terrainTimer->start();
    });
    connect(m_terrainPanel, &TerrainPanel::placeRequested, this, [this](int kind) {
        m_terrain.placing = kind;
        m_terrain.pathStart.reset();
        statusBar()->showMessage(kind < 0                                    ? QString()
                                 : kind == int(TerrainFeatureKind::Path) ? tr("Click on the ground where the path starts.")
                                                                         : tr("Click on the ground where it goes."));
    });
    connect(m_terrainPanel, &TerrainPanel::selectionChanged, this, [this] { updateTerrainPreview(false); });
    connect(m_terrainPanel, &TerrainPanel::applyRequested, this, [this] { applyTerrain(); });
    connect(m_terrainPanel, &TerrainPanel::cancelRequested, this, [this] { endTerrain(); });
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
        if (m_rock.active) {
            endRock();
            return;
        }
        if (m_tree.active) {
            endTree();
            return;
        }
        if (m_terrain.active) {
            if (m_terrain.placing >= 0) {  // stop placing first
                m_terrain.placing = -1;
                m_terrain.pathStart.reset();
                m_terrainPanel->setPlacing(-1);
                statusBar()->clearMessage();
            } else {
                endTerrain();
            }
            return;
        }
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
                                "build to: New Building, New Rock, New Terrain and New Tree make scenery at it."),
                             12000);
    if (preferences().value(QStringLiteral("updates/checkAtStart"), true).toBool())
        checkForUpdates(true);
}

SceneryWindow::~SceneryWindow()
{
    if (m_detail.valid())
        m_detail.wait();
    if (m_rockDetail.valid())
        m_rockDetail.wait();
    if (m_terrainDetail.valid())
        m_terrainDetail.wait();
    if (m_treeDetail.valid())
        m_treeDetail.wait();
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
    m_newRock = new QPushButton(tr("New Rock"));
    m_newRock->setToolTip(tr("Design a rock: a boulder, a crag, a slab or a pile of rocks, from real sizes at the "
                             "model scale, sunk into the ground"));
    m_newTerrain = new QPushButton(tr("New Terrain"));
    m_newTerrain->setToolTip(tr("Design a terrain piece: a base with rolling hills, craters, crags and paths, from "
                                "real sizes at the model scale"));
    buildings->addWidget(m_newBuilding);
    buildings->addWidget(m_newRock);
    buildings->addStretch(1);
    row->addLayout(buildings);
    auto* changing = new QVBoxLayout;
    changing->setSpacing(4);
    m_editBuilding = new QPushButton(tr("Change"));
    m_editBuilding->setToolTip(tr("Change the building, rock, terrain or tree picked in Objects (or double-click it there)"));
    m_newTree = new QPushButton(tr("New Tree"));
    m_newTree->setToolTip(tr("Design a tree: broadleaf, conifer, dead or palm, from real sizes at the model scale"));
    changing->addWidget(m_newTerrain);
    changing->addWidget(m_newTree);
    changing->addStretch(1);
    row->addLayout(changing);
    auto* editing = new QVBoxLayout;
    editing->setSpacing(4);
    editing->addWidget(m_editBuilding);
    editing->addStretch(1);
    row->addLayout(editing);
    connect(m_newBuilding, &QPushButton::clicked, this, [this] { startBuilding(kInvalidId); });
    connect(m_newRock, &QPushButton::clicked, this, [this] { startRock(kInvalidId); });
    connect(m_newTerrain, &QPushButton::clicked, this, [this] { startTerrain(kInvalidId); });
    connect(m_newTree, &QPushButton::clicked, this, [this] { startTree(kInvalidId); });
    connect(m_editBuilding, &QPushButton::clicked, this, [this] { changeSelected(); });

    auto* coming = new QLabel(tr("Double-click a shape in Objects to change it."));
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
            if ((m_build.active && (id == m_build.editing || id == m_build.roof)) ||
                (m_rock.active && id == m_rock.editing) || (m_terrain.active && id == m_terrain.editing) ||
                (m_tree.active && id == m_tree.editing)) {
                if (m_build.active)
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
            item.selected = !designing() && id == m_selected;
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
        if (m_terrain.active) {
            QMatrix4x4 at;
            at.translate(float(m_terrain.x), float(m_terrain.y), float(m_terrain.z));
            for (const auto& [m, id, colour, alpha] :
                 {std::tuple{m_terrain.mesh, 0xB0000005ull, QVector3D(0.49f, 0.42f, 0.31f), 1.0f},
                  std::tuple{m_terrain.marker, 0xB0000006ull, QVector3D(1.0f, 0.6f, 0.1f), 0.35f}}) {
                if (!m || m->empty())
                    continue;
                app::DisplayItem item;
                item.id = id;
                item.mesh = m;
                item.model = at;
                item.color = colour;
                item.alpha = alpha;
                item.helper = alpha < 1.0f;
                items.push_back(std::move(item));
                if (alpha >= 1.0f)
                    triangles += m->triangleCount();
            }
        }
        if (m_tree.active && m_tree.mesh && !m_tree.mesh->empty()) {
            app::DisplayItem item;
            item.id = 0xB0000007ull;
            item.mesh = m_tree.mesh;
            item.model.translate(float(m_tree.x), float(m_tree.y), float(m_tree.z));
            const TreeSpec spec = m_treePanel->spec();
            item.color = spec.leaves && spec.kind != TreeKind::Dead ? QVector3D(0.33f, 0.44f, 0.25f)
                                                                     : QVector3D(0.43f, 0.35f, 0.26f);
            items.push_back(std::move(item));
            triangles += m_tree.mesh->triangleCount();
        }
        if (m_rock.active && m_rock.mesh && !m_rock.mesh->empty()) {
            app::DisplayItem item;
            item.id = 0xB0000004ull;
            item.mesh = m_rock.mesh;
            item.model.translate(float(m_rock.x), float(m_rock.y), float(m_rock.z));
            item.color = QVector3D(0.54f, 0.53f, 0.50f);
            items.push_back(std::move(item));
            triangles += m_rock.mesh->triangleCount();
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
    endRock();
    endTerrain();
    endTree();
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
        endRock();
        endTerrain();
        endTree();
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
    m_newBuilding->setEnabled(!designing());
    m_newRock->setEnabled(!designing());
    m_newTerrain->setEnabled(!designing());
    m_newTree->setEnabled(!designing());
    m_editBuilding->setEnabled(!designing() && (building || (b && (b->rock || b->terrain || b->tree))));
}

void SceneryWindow::changeSelected()
{
    const Body* b = m_doc.find(m_selected);
    if (b && b->rock)
        startRock(m_selected);
    else if (b && b->terrain)
        startTerrain(m_selected);
    else if (b && b->tree)
        startTree(m_selected);
    else
        startBuilding(m_selected);
}

void SceneryWindow::startBuilding(ObjectId editing)
{
    if (designing())
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
    if (m_terrain.active) {
        terrainClicked(origin, direction);
        return;
    }
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
    if (m_terrain.active) {
        terrainPressed(origin, direction);
        return;
    }
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
    if (m_terrain.active) {
        terrainDragged(origin, direction);
        return;
    }
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
    if (m_terrain.active) {
        if (m_terrain.dragging >= 0) {
            m_terrain.dragging = -1;
            m_terrainTimer->start();
        }
        return;
    }
    if (!m_build.active || m_build.dragging < 0)
        return;
    m_build.dragging = -1;
    m_detailTimer->start();
}

void SceneryWindow::deleteSelected()
{
    if (m_terrain.active) {
        // The picked crater, crag or path.
        TerrainSpec spec = m_terrainPanel->spec();
        const int i = m_terrainPanel->selectedFeature();
        if (i >= 0 && i < int(spec.features.size())) {
            spec.features.erase(spec.features.begin() + i);
            m_terrainPanel->setSpec(spec);
            m_terrainPanel->selectFeature(-1);
            updateTerrainPreview(true);
            m_terrainTimer->start();
        }
        return;
    }
    if (m_rock.active || m_tree.active)
        return;
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
    endBuilding();
    endRock();
    endTerrain();
    endTree();
    if (askToSave())
        event->accept();
    else
        event->ignore();
}

// ---- The rock designer ------------------------------------------------------------------

void SceneryWindow::startRock(ObjectId editing)
{
    if (designing())
        return;
    RockMode mode;
    mode.active = true;
    RockSpec spec;
    if (editing != kInvalidId) {
        const Body* b = m_doc.find(editing);
        if (!b || !b->rock)
            return;
        mode.editing = b->id;
        spec = *b->rock;
        mode.x = b->placement.x - spec.offset[0];
        mode.y = b->placement.y - spec.offset[1];
        mode.z = b->placement.z - spec.offset[2];
    } else {
        spec = rock::defaults(RockKind::Boulder);
        spec.scale = m_scale.ratio;
        if (!m_doc.empty()) {  // beside what's there
            Bounds all;
            for (ObjectId id : m_doc.topLevelIds()) {
                const Bounds b = m_doc.worldBounds(id);
                all.expand(b.min[0], b.min[1], b.min[2]);
                all.expand(b.max[0], b.max[1], b.max[2]);
            }
            mode.x = all.max[0] + spec.length / spec.scale / 2 + 10.0;
            mode.y = all.center()[1];
        }
    }
    m_rock = mode;
    m_rockPanel->start(spec, editing != kInvalidId);
    m_rockDock->show();
    if (auto* scroll = qobject_cast<QScrollArea*>(m_rockDock->widget()))
        scroll->verticalScrollBar()->setValue(0);
    m_viewport->setFocus();
    statusBar()->showMessage(tr("Rock: set it up on the right; New Shape for another like it. Esc cancels."));
    updateRockPreview();
    if (editing == kInvalidId)
        m_viewport->fitView();
    requestRockDetail();
    refreshCommands();
}

void SceneryWindow::endRock()
{
    if (!m_rock.active)
        return;
    m_rockTimer->stop();
    m_rock = RockMode{};
    m_rockDock->hide();
    statusBar()->clearMessage();
    refreshScene();
}

void SceneryWindow::applyRock()
{
    if (!m_rock.active)
        return;
    const RockSpec spec = m_rockPanel->spec();
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (m_rock.editing != kInvalidId) {
            rock::update(m_stack, m_doc, m_rock.editing, spec);
            m_selected = m_rock.editing;
        } else {
            const QString name = tr(rock::title(spec.kind)).section(QStringLiteral(" ("), 0, 0);  // "Crag"
            m_selected = rock::add(m_stack, m_doc, spec, name.toStdString(), m_rock.x, m_rock.y);
        }
    } catch (const std::exception& e) {
        m_rockPanel->setStatus(tr("It couldn't be made: %1").arg(QString::fromUtf8(e.what())), true);
        return;
    }
    endRock();
    statusBar()->showMessage(tr("Rock made. Double-click it in Objects to change it."), 8000);
}

// The rock as it's set up: coarsely now, in full a moment later.
void SceneryWindow::updateRockPreview()
{
    if (!m_rock.active)
        return;
    const RockSpec spec = m_rockPanel->spec();
    ++m_rock.generation;
    try {
        const Mesh made = rock::make(spec, true);
        m_rock.mesh = std::make_shared<const Mesh>(
            meshkernel::displayMesh(meshkernel::fromTriangles(made.positions, made.indices)));
        m_rockPanel->setStatus({}, false);
    } catch (const std::exception& e) {
        m_rockPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    refreshScene();
}

void SceneryWindow::requestRockDetail()
{
    if (!m_rock.active)
        return;
    const RockSpec spec = m_rockPanel->spec();
    const int generation = m_rock.generation;
    QPointer<SceneryWindow> self(this);
    if (m_rockDetail.valid())
        m_rockDetail.wait();  // one at a time
    m_rockDetail = std::async(std::launch::async, [self, spec, generation] {
        std::shared_ptr<const Mesh> mesh;
        QString problem;
        try {
            const Mesh made = rock::make(spec);
            mesh = std::make_shared<const Mesh>(
                meshkernel::displayMesh(meshkernel::fromTriangles(made.positions, made.indices)));
        } catch (const std::exception& e) {
            problem = QString::fromUtf8(e.what());
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, mesh, problem] {
                if (!self || !self->m_rock.active || self->m_rock.generation != generation)
                    return;
                if (!problem.isEmpty()) {
                    self->m_rockPanel->setStatus(problem, true);
                    return;
                }
                self->m_rock.mesh = mesh;
                self->refreshScene();
            },
            Qt::QueuedConnection);
    });
}

// ---- The tree designer ------------------------------------------------------------------

void SceneryWindow::startTree(ObjectId editing)
{
    if (designing())
        return;
    TreeMode mode;
    mode.active = true;
    TreeSpec spec;
    if (editing != kInvalidId) {
        const Body* b = m_doc.find(editing);
        if (!b || !b->tree)
            return;
        mode.editing = b->id;
        spec = *b->tree;
        mode.x = b->placement.x - spec.offset[0];
        mode.y = b->placement.y - spec.offset[1];
        mode.z = b->placement.z - spec.offset[2];
    } else {
        spec = tree::defaults(TreeKind::Broadleaf);
        spec.scale = m_scale.ratio;
        if (!m_doc.empty()) {  // beside what's there
            Bounds all;
            for (ObjectId id : m_doc.topLevelIds()) {
                const Bounds b = m_doc.worldBounds(id);
                all.expand(b.min[0], b.min[1], b.min[2]);
                all.expand(b.max[0], b.max[1], b.max[2]);
            }
            mode.x = all.max[0] + spec.crownWidth / spec.scale / 2 + 10.0;
            mode.y = all.center()[1];
        }
    }
    m_tree = mode;
    m_treePanel->setPrinter(m_viewport->plateHeight(), std::min(m_viewport->plateSize().width(), m_viewport->plateSize().height()));
    m_treePanel->start(spec, editing != kInvalidId);
    m_treeDock->show();
    if (auto* scroll = qobject_cast<QScrollArea*>(m_treeDock->widget()))
        scroll->verticalScrollBar()->setValue(0);
    m_viewport->setFocus();
    statusBar()->showMessage(tr("Tree: set it up on the right; New Shape for another like it. Esc cancels."));
    updateTreePreview();
    if (editing == kInvalidId)
        m_viewport->fitView();
    requestTreeDetail();
    refreshCommands();
}

void SceneryWindow::endTree()
{
    if (!m_tree.active)
        return;
    m_treeTimer->stop();
    m_tree = TreeMode{};
    m_treeDock->hide();
    statusBar()->clearMessage();
    refreshScene();
}

void SceneryWindow::applyTree()
{
    if (!m_tree.active)
        return;
    const TreeSpec spec = m_treePanel->spec();
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (m_tree.editing != kInvalidId) {
            tree::update(m_stack, m_doc, m_tree.editing, spec);
            m_selected = m_tree.editing;
        } else {
            const QString name = tr(tree::title(spec.kind)).section(QStringLiteral(" ("), 0, 0);  // "Conifer"
            m_selected = tree::add(m_stack, m_doc, spec, name.toStdString(), m_tree.x, m_tree.y);
        }
    } catch (const std::exception& e) {
        m_treePanel->setStatus(tr("It couldn't be made: %1").arg(QString::fromUtf8(e.what())), true);
        return;
    }
    endTree();
    statusBar()->showMessage(tr("Tree made. Double-click it in Objects to change it."), 8000);
}

// The tree as it's set up: coarsely now, in full a moment later.
void SceneryWindow::updateTreePreview()
{
    if (!m_tree.active)
        return;
    const TreeSpec spec = m_treePanel->spec();
    ++m_tree.generation;
    try {
        const Mesh made = tree::make(spec, true);
        m_tree.mesh = std::make_shared<const Mesh>(
            meshkernel::displayMesh(meshkernel::fromTriangles(made.positions, made.indices)));
        m_treePanel->setStatus({}, false);
    } catch (const std::exception& e) {
        m_treePanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    refreshScene();
}

void SceneryWindow::requestTreeDetail()
{
    if (!m_tree.active)
        return;
    const TreeSpec spec = m_treePanel->spec();
    const int generation = m_tree.generation;
    QPointer<SceneryWindow> self(this);
    if (m_treeDetail.valid())
        m_treeDetail.wait();  // one at a time
    m_treeDetail = std::async(std::launch::async, [self, spec, generation] {
        std::shared_ptr<const Mesh> mesh;
        QString problem;
        try {
            const Mesh made = tree::make(spec);
            mesh = std::make_shared<const Mesh>(
                meshkernel::displayMesh(meshkernel::fromTriangles(made.positions, made.indices)));
        } catch (const std::exception& e) {
            problem = QString::fromUtf8(e.what());
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, mesh, problem] {
                if (!self || !self->m_tree.active || self->m_tree.generation != generation)
                    return;
                if (!problem.isEmpty()) {
                    self->m_treePanel->setStatus(problem, true);
                    return;
                }
                self->m_tree.mesh = mesh;
                self->refreshScene();
            },
            Qt::QueuedConnection);
    });
}

// ---- The terrain designer ---------------------------------------------------------------

void SceneryWindow::startTerrain(ObjectId editing)
{
    if (designing())
        return;
    TerrainMode mode;
    mode.active = true;
    TerrainSpec spec;
    if (editing != kInvalidId) {
        const Body* b = m_doc.find(editing);
        if (!b || !b->terrain)
            return;
        mode.editing = b->id;
        spec = *b->terrain;
        mode.x = b->placement.x - spec.offset[0];
        mode.y = b->placement.y - spec.offset[1];
        mode.z = b->placement.z - spec.offset[2];
    } else {
        spec = terrain::defaults();
        spec.scale = m_scale.ratio;
        // Most of the printer's plate, whatever the scale.
        const QSizeF plate = m_viewport->plateSize();
        spec.width = std::max(100.0, std::floor(plate.width() * 0.8 * spec.scale / 100.0) * 100.0);
        spec.depth = std::max(100.0, std::floor(std::min(plate.height() * 0.8, plate.width() * 0.6) * spec.scale / 100.0) * 100.0);
        spec.hillSize = std::round(spec.width * 0.7 / 100.0) * 100.0;
        spec.hillHeight = std::round(spec.depth * 0.18 / 100.0) * 100.0;
        if (!m_doc.empty()) {  // beside what's there
            Bounds all;
            for (ObjectId id : m_doc.topLevelIds()) {
                const Bounds b = m_doc.worldBounds(id);
                all.expand(b.min[0], b.min[1], b.min[2]);
                all.expand(b.max[0], b.max[1], b.max[2]);
            }
            mode.x = all.max[0] + spec.width / spec.scale / 2 + 10.0;
            mode.y = all.center()[1];
        }
    }
    m_terrain = mode;
    m_terrainPanel->start(spec, editing != kInvalidId);
    m_terrainDock->show();
    if (auto* scroll = qobject_cast<QScrollArea*>(m_terrainDock->widget()))
        scroll->verticalScrollBar()->setValue(0);
    m_viewport->setFocus();
    statusBar()->showMessage(tr("Terrain: set it up on the right; Add Crater, Add Crag or Add Path, then click on the "
                                "ground, or Scatter. Esc cancels."));
    updateTerrainPreview(true);
    if (editing == kInvalidId)
        m_viewport->fitView();
    requestTerrainDetail();
    refreshCommands();
}

void SceneryWindow::endTerrain()
{
    if (!m_terrain.active)
        return;
    m_terrainTimer->stop();
    m_terrain = TerrainMode{};
    m_terrainDock->hide();
    statusBar()->clearMessage();
    refreshScene();
}

void SceneryWindow::applyTerrain()
{
    if (!m_terrain.active)
        return;
    const TerrainSpec spec = m_terrainPanel->spec();
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (m_terrain.editing != kInvalidId) {
            terrain::update(m_stack, m_doc, m_terrain.editing, spec);
            m_selected = m_terrain.editing;
        } else {
            m_selected = terrain::add(m_stack, m_doc, spec, tr("Terrain").toStdString(), m_terrain.x, m_terrain.y);
        }
    } catch (const std::exception& e) {
        m_terrainPanel->setStatus(tr("It couldn't be made: %1").arg(QString::fromUtf8(e.what())), true);
        return;
    }
    endTerrain();
    statusBar()->showMessage(tr("Terrain made. Double-click it in Objects to change it."), 8000);
}

// The terrain as it's set up: coarsely now (and the picked one's marker), in full a moment later.
void SceneryWindow::updateTerrainPreview(bool quick)
{
    if (!m_terrain.active)
        return;
    const TerrainSpec spec = m_terrainPanel->spec();
    try {
        if (quick) {
            ++m_terrain.generation;
            const Mesh made = terrain::make(spec, true);
            m_terrain.mesh = std::make_shared<const Mesh>(
                meshkernel::displayMesh(meshkernel::fromTriangles(made.positions, made.indices)));
        }
        const int picked = m_terrainPanel->selectedFeature();
        if (picked >= 0 && picked < int(spec.features.size())) {
            const Mesh m = terrain::marker(spec, spec.features[std::size_t(picked)]);
            m_terrain.marker =
                std::make_shared<const Mesh>(meshkernel::displayMesh(meshkernel::fromTriangles(m.positions, m.indices)));
        } else {
            m_terrain.marker = nullptr;
        }
        m_terrainPanel->setStatus({}, false);
    } catch (const std::exception& e) {
        m_terrainPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    refreshScene();
}

void SceneryWindow::requestTerrainDetail()
{
    if (!m_terrain.active)
        return;
    const TerrainSpec spec = m_terrainPanel->spec();
    const int generation = m_terrain.generation;
    QPointer<SceneryWindow> self(this);
    if (m_terrainDetail.valid())
        m_terrainDetail.wait();  // one at a time
    m_terrainDetail = std::async(std::launch::async, [self, spec, generation] {
        std::shared_ptr<const Mesh> mesh;
        QString problem;
        try {
            const Mesh made = terrain::make(spec);
            mesh = std::make_shared<const Mesh>(
                meshkernel::displayMesh(meshkernel::fromTriangles(made.positions, made.indices)));
        } catch (const std::exception& e) {
            problem = QString::fromUtf8(e.what());
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, mesh, problem] {
                if (!self || !self->m_terrain.active || self->m_terrain.generation != generation)
                    return;
                if (!problem.isEmpty()) {
                    self->m_terrainPanel->setStatus(problem, true);
                    return;
                }
                self->m_terrain.mesh = mesh;
                self->refreshScene();
            },
            Qt::QueuedConnection);
    });
}

std::optional<std::array<double, 2>> SceneryWindow::terrainSpot(const QVector3D& origin,
                                                                const QVector3D& direction) const
{
    return terrain::hitAt(m_terrainPanel->spec(),
                          {origin.x() - m_terrain.x, origin.y() - m_terrain.y, origin.z() - m_terrain.z},
                          {direction.x(), direction.y(), direction.z()});
}

void SceneryWindow::terrainClicked(const QVector3D& origin, const QVector3D& direction)
{
    const auto spot = terrainSpot(origin, direction);
    TerrainSpec spec = m_terrainPanel->spec();
    if (m_terrain.placing >= 0) {
        if (!spot) {
            statusBar()->showMessage(tr("Click on the ground."), 4000);
            return;
        }
        const auto kind = TerrainFeatureKind(m_terrain.placing);
        if (kind == TerrainFeatureKind::Path && !m_terrain.pathStart) {
            m_terrain.pathStart = spot;
            statusBar()->showMessage(tr("Now click where the path ends."));
            return;
        }
        TerrainFeature f = kind == TerrainFeatureKind::Path
                               ? terrain::feature(spec, kind, (*m_terrain.pathStart)[0], (*m_terrain.pathStart)[1],
                                                  (*spot)[0], (*spot)[1])
                               : terrain::feature(spec, kind, (*spot)[0], (*spot)[1]);
        spec.features.push_back(f);
        m_terrain.placing = -1;
        m_terrain.pathStart.reset();
        m_terrainPanel->setPlacing(-1);
        m_terrainPanel->setSpec(spec);
        m_terrainPanel->selectFeature(int(spec.features.size()) - 1);
        statusBar()->clearMessage();
        updateTerrainPreview(true);
        m_terrainTimer->start();
        return;
    }
    const auto picked = spot ? terrain::pickedFeature(spec, (*spot)[0], (*spot)[1]) : std::nullopt;
    m_terrainPanel->selectFeature(picked ? int(*picked) : -1);
    updateTerrainPreview(false);
}

void SceneryWindow::terrainPressed(const QVector3D& origin, const QVector3D& direction)
{
    if (m_terrain.placing >= 0)
        return;
    const auto spot = terrainSpot(origin, direction);
    if (!spot)
        return;
    const TerrainSpec spec = m_terrainPanel->spec();
    const auto picked = terrain::pickedFeature(spec, (*spot)[0], (*spot)[1]);
    if (!picked)
        return;
    const TerrainFeature& f = spec.features[*picked];
    m_terrain.dragging = int(*picked);
    m_terrain.dragEnd = 0;
    m_terrain.grabX = (*spot)[0] - f.x;
    m_terrain.grabY = (*spot)[1] - f.y;
    if (f.kind == TerrainFeatureKind::Path) {
        const double toStart = std::hypot((*spot)[0] - f.x, (*spot)[1] - f.y);
        const double toEnd = std::hypot((*spot)[0] - f.x2, (*spot)[1] - f.y2);
        const double length = std::hypot(f.x2 - f.x, f.y2 - f.y);
        const double near = std::min(f.size * 1.5, length / 4);
        if (toStart < near && toStart <= toEnd)
            m_terrain.dragEnd = 1;
        else if (toEnd < near)
            m_terrain.dragEnd = 2;
        if (m_terrain.dragEnd == 2) {
            m_terrain.grabX = (*spot)[0] - f.x2;
            m_terrain.grabY = (*spot)[1] - f.y2;
        }
    }
    m_terrainPanel->selectFeature(int(*picked));
    updateTerrainPreview(false);
}

void SceneryWindow::terrainDragged(const QVector3D& origin, const QVector3D& direction)
{
    if (m_terrain.dragging < 0)
        return;
    TerrainSpec spec = m_terrainPanel->spec();
    if (m_terrain.dragging >= int(spec.features.size()))
        return;
    const auto spot = terrainSpot(origin, direction);
    if (!spot)
        return;
    TerrainFeature& f = spec.features[std::size_t(m_terrain.dragging)];
    const double nx = (*spot)[0] - m_terrain.grabX, ny = (*spot)[1] - m_terrain.grabY;
    if (m_terrain.dragEnd == 2) {
        f.x2 = nx, f.y2 = ny;
    } else if (m_terrain.dragEnd == 1) {
        f.x = nx, f.y = ny;
    } else {
        f.x2 += nx - f.x, f.y2 += ny - f.y;
        f.x = nx, f.y = ny;
    }
    f.scattered = false;  // moved by hand: it stays
    m_terrainPanel->setSpec(spec);
    m_terrainPanel->selectFeature(m_terrain.dragging);
    updateTerrainPreview(true);
}

}  // namespace forge::scenery
