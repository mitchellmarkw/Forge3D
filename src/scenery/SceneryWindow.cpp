// SPDX-License-Identifier: GPL-3.0-or-later
#include "SceneryWindow.h"

#include "BuildInfo.h"
#include "Theme.h"
#include "Viewport.h"
#include "forge/core/Export.h"
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
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <filesystem>

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
    m_objects->setSelectionMode(QAbstractItemView::NoSelection);
    dock->setWidget(m_objects);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

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

SceneryWindow::~SceneryWindow() = default;

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

    auto* coming = new QLabel(tr("<b>Coming next:</b> a building designer (brick, stone, logs; doors and windows), "
                                 "trees, rocks and boulders, and terrain."));
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
    m_objects->clear();
    try {
        for (ObjectId id : m_doc.topLevelIds()) {
            const auto mesh = m_doc.mesh(id);
            if (!mesh)
                continue;
            app::DisplayItem item;
            item.id = id;
            item.mesh = mesh;
            item.model = toQMatrix(m_doc.worldTransform(id));
            const std::uint32_t c = m_doc.colourOf(id);
            item.color = QVector3D(float((c >> 16) & 0xFF) / 255.0f, float((c >> 8) & 0xFF) / 255.0f,
                                   float(c & 0xFF) / 255.0f);
            items.push_back(std::move(item));
            triangles += mesh->triangleCount();
            m_objects->addItem(QString::fromStdString(m_doc.find(id)->name));
        }
    } catch (const std::exception& e) {
        statusBar()->showMessage(tr("Something couldn't be shown: %1").arg(QString::fromUtf8(e.what())), 8000);
    }
    m_viewport->setItems(std::move(items));
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

void SceneryWindow::closeEvent(QCloseEvent* event)
{
    if (askToSave())
        event->accept();
    else
        event->ignore();
}

}  // namespace forge::scenery
