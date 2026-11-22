// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include "BuildInfo.h"
#include "EdgesPanel.h"
#include "EmbossPanel.h"
#include "Log.h"
#include "PropertiesPanel.h"
#include "FollowPanel.h"
#include "HolePanel.h"
#include "PatternPanel.h"
#include "ShellPanel.h"
#include "MeasurePanel.h"

#include <BRepClass3d_SolidClassifier.hxx>
#include <gp_Pnt.hxx>
#include "SectionPanel.h"
#include "forge/core/Layout.h"
#include "forge/core/ShapeLibrary.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/Hinge.h"
#include "forge/core/Socket.h"
#include "forge/core/PrintSplit.h"
#include "Theme.h"
#include "JoinPanel.h"
#include "HingePanel.h"
#include "PosePanel.h"
#include "PrintCheckPanel.h"
#include "MannequinPanel.h"
#include "HollowPanel.h"
#include "MouldPanel.h"
#include "PortPanel.h"
#include "HandPanel.h"
#include "PoseFigurePanel.h"
#include "PaintPanel.h"
#include "JointStandardsDialog.h"
#include "CutPartsPanel.h"
#include "Standards.h"
#include "StampPanel.h"
#include "PanelLinePanel.h"
#include "forge/core/PanelLines.h"
#include "forge/core/Stamp.h"
#include "Fits.h"
#include "forge/core/Hollow.h"
#include "forge/core/Mould.h"
#include "forge/core/Port.h"
#include "forge/core/Hand.h"
#include "MaskPanel.h"
#include "ReferencePanel.h"
#include "ScaleCalculator.h"
#include "RescaleDialog.h"
#include "KitDialog.h"
#include "PartsListDialog.h"
#include "forge/core/Kit.h"
#include "ToleranceDialog.h"
#include "SocketPanel.h"
#include "ThreadPanel.h"
#include "forge/core/Thread.h"
#include "forge/core/Picking.h"
#include "SketchPanel.h"
#include "SketchView.h"
#include "SculptPanel.h"
#include "CustomShapesDialog.h"
#include "FitPanel.h"
#include "SplitPanel.h"
#include "Tour.h"
#include "Viewport.h"

#include "forge/core/Edges.h"
#include "forge/core/Emboss.h"
#include "forge/core/Export.h"
#include "forge/core/History.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Import.h"
#include "forge/core/Picking.h"
#include "forge/core/Primitives.h"
#include "forge/core/SketchShape.h"
#include "forge/core/Shell.h"
#include "forge/core/Follow.h"
#include "forge/core/Pattern.h"
#include "forge/core/Picture.h"
#include "forge/core/Updates.h"
#include "forge/core/Sculpt.h"
#include "forge/core/Tessellator.h"
#include "forge/core/Tessellator.h"
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
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QGridLayout>
#include <QWidgetAction>
#include <QDialog>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QLockFile>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QFileDialog>
#include <QRandomGenerator>
#include <QImageReader>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QInputDialog>
#include <QLineEdit>
#include <QThreadPool>
#include <QProcess>
#include <QKeySequence>
#include <QLabel>
#include <QTreeWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QScrollArea>
#include <QScreen>
#include <QElapsedTimer>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextBrowser>
#include <QTextDocument>
#include <QCursor>
#include <QDialogButtonBox>
#include <QFile>
#include <QGuiApplication>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QFrame>
#include <QTabWidget>
#include <Standard_Failure.hxx>
#include <gp_Trsf.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QUrl>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <cstring>
#include <QtDebug>

#include <Standard_Version.hxx>

#include <array>
#include <exception>
#include <filesystem>
#include <limits>
#include <numbers>
#include <variant>

namespace forge::app {

namespace {

// Distinct, print-friendly colours; each body keeps its colour (picked by id).
// Colours offered by Colour: the usual filament colours.
struct Swatch {
    std::uint32_t rgb;
    const char* name;
};
constexpr std::array<Swatch, 14> kSwatches{{
    {0xF2F2F2, QT_TRANSLATE_NOOP("MainWindow", "White")},
    {0xB5B8BC, QT_TRANSLATE_NOOP("MainWindow", "Light grey")},
    {0x55585D, QT_TRANSLATE_NOOP("MainWindow", "Dark grey")},
    {0x222326, QT_TRANSLATE_NOOP("MainWindow", "Black")},
    {0xD7322D, QT_TRANSLATE_NOOP("MainWindow", "Red")},
    {0xED8C33, QT_TRANSLATE_NOOP("MainWindow", "Orange")},
    {0xF2C230, QT_TRANSLATE_NOOP("MainWindow", "Yellow")},
    {0x3FA458, QT_TRANSLATE_NOOP("MainWindow", "Green")},
    {0x33ADB3, QT_TRANSLATE_NOOP("MainWindow", "Teal")},
    {0x2B78D0, QT_TRANSLATE_NOOP("MainWindow", "Blue")},
    {0x8A56C2, QT_TRANSLATE_NOOP("MainWindow", "Purple")},
    {0xE8779E, QT_TRANSLATE_NOOP("MainWindow", "Pink")},
    {0x8A5A3C, QT_TRANSLATE_NOOP("MainWindow", "Brown")},
    {0xC9A43A, QT_TRANSLATE_NOOP("MainWindow", "Gold")},
}};

// Snapping steps (hold Alt while dragging to turn snapping off).
constexpr double kMoveStep = 1.0;   // mm
constexpr double kTurnStep = 15.0;  // degrees
constexpr double kSizeStep = 1.0;   // mm

const QString kFileFilter = QObject::tr("Forge3D model (*.f3d)");
const QString kLastFolderKey = QStringLiteral("files/lastFolder");

// Objects tree rows: what each row is, and (steps, bar) which object it belongs to.
constexpr int kRowKindRole = Qt::UserRole + 1;
constexpr int kOwnerRole = Qt::UserRole + 2;
constexpr int kStepIndexRole = Qt::UserRole + 3;
constexpr int kObjectRow = 0;
constexpr int kStepRow = 1;
constexpr int kBarRow = 2;

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
// name is set on the app, so its data folder stays %LOCALAPPDATA%\Forge3D; a plain
// Preferences() would then have nowhere to keep anything on Windows, so always use this.)
class Preferences : public QSettings {
public:
    Preferences() : QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"))
    {
    }
};

QString startFolder()
{
    const Preferences settings;
    const QString remembered = settings.value(kLastFolderKey).toString();
    if (!remembered.isEmpty() && QDir(remembered).exists())
        return remembered;
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

const char* const kRecentKey = "file/recent";
constexpr int kRecentCount = 6;

void rememberFolderOf(const QString& filePath)
{
    Preferences settings;
    settings.setValue(kLastFolderKey, QFileInfo(filePath).absolutePath());
}

}  // namespace

namespace {

// A ribbon page that scrolls sideways when the window is too narrow for its buttons:
// as tall as its buttons, plus the scroll bar only while one is needed.
// An icon drawn from its SVG at every size it's shown (menus, ribbon, high-DPI screens).
// Drawn here rather than by Qt's SVG icon plugin, so nothing extra has to be deployed.
QIcon ribbonIcon(const QString& name)
{
    QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = file.readAll();
    // On a dark window the dark lines turn light.
    if (QApplication::palette().color(QPalette::Window).lightness() < 128)
        data.replace("#3a3f45", "#d6dade");
    QSvgRenderer svg(data);
    QIcon icon;
    if (!svg.isValid())
        return icon;
    for (int size : {16, 20, 24, 32, 40, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        svg.render(&painter);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

// "0.25.0 Beta": the version, with its label if it has one.
QString versionText()
{
    const QString label = QString::fromLatin1(build::kReleaseLabel);
    return QString::fromLatin1(build::kVersion) + (label.isEmpty() ? QString() : QStringLiteral(" ") + label);
}

class RibbonScroll : public QScrollArea {
public:
    explicit RibbonScroll(QWidget* page)
    {
        setWidget(page);
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
        fit();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QScrollArea::resizeEvent(event);
        fit();
    }

private:
    void fit()
    {
        const QSize content = widget()->sizeHint();
        const bool scrolls = content.width() > width();
        const int h = content.height() + (scrolls ? horizontalScrollBar()->sizeHint().height() : 0);
        if (height() != h)
            setFixedHeight(h);
    }
};

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    // 1400 x 860, or less on a smaller screen.
    QSize start(1400, 860);
    if (const QScreen* screen = QGuiApplication::primaryScreen())
        start = start.boundedTo(screen->availableGeometry().size() * 0.95);
    resize(start);
    setAcceptDrops(true);  // drop STL / 3MF / OBJ (import) or .f3d (open) files on the window

    m_viewport = new Viewport(this);
    m_sketchView = new SketchView(this);
    m_central = new QStackedWidget(this);
    m_central->addWidget(m_viewport);
    m_central->addWidget(m_sketchView);
    setCentralWidget(m_central);
    // A first step for an empty plate, over the top of the view.
    {
        auto* overlay = new QVBoxLayout(m_viewport);
        m_emptyHint = new QLabel(tr("Start by adding a shape: <b>Build → Add Shape</b> (or the <b>Add</b> menu).<br>"
                                    "Or <b>Import</b> an STL, 3MF or OBJ file, or drop one here."));
        m_emptyHint->setAlignment(Qt::AlignCenter);
        m_emptyHint->setTextFormat(Qt::RichText);
        styleEmptyHint();
        m_emptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);
        overlay->addSpacing(40);
        overlay->addWidget(m_emptyHint, 0, Qt::AlignHCenter | Qt::AlignTop);
        overlay->addStretch(1);
    }
    connect(m_viewport, &Viewport::firstFrameRendered, this, &MainWindow::firstFrameRendered);
    // The first time Forge3D runs, the tour starts by itself (once).
    // After the tour (or straight away, once it's been seen): whether to look for updates.
    connect(this, &MainWindow::firstFrameRendered, this, [this] {
        QTimer::singleShot(300, this, &MainWindow::offerRecovery);
        if (!Preferences().value(QStringLiteral("tour/seen"), false).toBool())
            QTimer::singleShot(600, this, &MainWindow::startTour);
        else
            QTimer::singleShot(1500, this, &MainWindow::updatesAtStart);
    });
    connect(m_viewport, &Viewport::clicked, this, &MainWindow::onViewportClicked);
    connect(m_viewport, &Viewport::hovered, this, [this](const QVector3D& o, const QVector3D& d, const QPointF& px) {
        if (m_edgesMode.active)
            edgesHover(o, d, px);
        if (m_sculptMode.active)
            sculptHover(o, d);
        if (m_maskMode.active)
            maskHover(o, d);
        if (m_paintMode.active)
            paintHover(o, d);
        if (m_measureMode.active)
            measureHover(o, d, px);
        if (m_referenceMode.active && m_referenceMode.picking == 2) {
            // A line from the first point to the mouse, while picking the second.
            const ReferenceImage* image = reference::find(m_doc.references(), m_referencePanel->direction());
            const auto h = image ? reference::hit(*image, {o.x(), o.y(), o.z()}, {d.x(), d.y(), d.z()}) : std::nullopt;
            const Vec3& a = m_referenceMode.firstAt;
            const QVector3D from{float(a[0]), float(a[1]), float(a[2])};
            if (h) {
                const Vec3 b = reference::pointAt(*image, h->px, h->py);
                m_viewport->setLabels({{from, QVector3D(float(b[0]), float(b[1]), float(b[2])), tr("2")}});
            } else {
                m_viewport->setLabels({{from, from, tr("1")}});
            }
        }
    });
    connect(m_viewport, &Viewport::leftPressed, this, &MainWindow::onLeftPressed);
    connect(m_viewport, &Viewport::leftDragged, this, &MainWindow::onLeftDragged);
    connect(m_viewport, &Viewport::leftReleased, this, &MainWindow::onLeftReleased);

    setWindowIcon(ribbonIcon(QStringLiteral("forge3d")));
    createActions();
    createMenus();
    createToolBar();
    createSketchBar();
    createDocks();
    createStatusBar();

    m_doc.addListener([this](const Document::Change& change) {
        ++m_docSerial;
        if (m_poseMode.active)  // an undo while posing: the joints may have changed
            QTimer::singleShot(0, this, [this] {
                if (m_poseMode.active)
                    rebuildPose();
            });
        if (!m_keptLabels.empty()) {
            // The shapes changed: the kept numbers may no longer be true.
            m_keptLabels.clear();
            showLabels();
            statusBar()->showMessage(tr("The kept measurements were taken off the view: the model changed."), 8000);
        }
        if (change.kind == Document::ChangeKind::Removed)  // e.g. undo of an Add
            std::erase(m_selection, change.id);
        if (m_measureMode.active) {
            // The shapes changed (an undo, say): what was picked may be gone.
            m_measureMode.shapes.clear();
            m_measureMode.picks.clear();
            m_measureMode.hover.reset();
            QTimer::singleShot(0, this, &MainWindow::showMeasurements);
        }
        if (m_printCheckMode.active)  // an undo while checking: check the parts again
            QTimer::singleShot(0, this, [this] {
                if (m_printCheckMode.active)
                    updatePrintCheck();
            });
        scheduleSceneRefresh();
    });
    m_stack.setListener([this] { refreshCommandState(); });

    refreshScene();
    refreshCommandState();
    m_viewport->resetView();
    startAutosave();
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
    m_import->setToolTip(tr("Import (Ctrl+I): add shapes from STL, 3MF, OBJ, PLY (scans) or glTF / GLB files, "
                            "repaired for printing. "
                            "You can also drag files onto the window"));
    connect(m_import, &QAction::triggered, this, [this] {
        Preferences settings;
        const QString folder = settings.value(QStringLiteral("import/folder"), startFolder()).toString();
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, tr("Import shapes"), folder,
            tr("3D models (*.stl *.3mf *.obj *.ply *.glb *.gltf);;STL (*.stl);;3MF (*.3mf);;OBJ (*.obj);;"
               "PLY - scans (*.ply);;glTF / GLB (*.glb *.gltf)"));
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
        if (m_sculptMode.active) {
            undoStroke();
            return;
        }
        if (m_maskMode.active) {
            undoMaskStroke();
            return;
        }
        if (m_paintMode.active) {
            undoPaintStroke();
            return;
        }
        if (m_referenceMode.active)
            m_referencePanel->setStatus({}, false);
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

    m_colourMenu = new QMenu(tr("Colour"), this);
    m_colourMenu->menuAction()->setToolTip(
        tr("Colour: choose a colour for the selected shapes. It's saved with your model and written to 3MF "
           "files, for printers with more than one colour"));
    setIconNamed(m_colourMenu->menuAction(), QStringLiteral("colour"));
    fillColourMenu(m_colourMenu);

    m_layFlat = new QAction(tr("Lay on Face"), this);
    m_layFlat->setCheckable(true);
    m_layFlat->setToolTip(tr("Lay on Face: click a face of a shape, and the shape turns to lie on that face on "
                             "the plate (good for printing). Esc cancels."));
    connect(m_layFlat, &QAction::toggled, this, [this](bool on) {
        if (on)
            statusBar()->showMessage(tr("Lay on Face: click the face the shape should lie on. Esc cancels."));
        else
            statusBar()->clearMessage();
        refreshScene();  // the handles go while picking a face
    });
    m_sitOnPlate = new QAction(tr("Sit on Plate"), this);
    m_sitOnPlate->setToolTip(tr("Sit on Plate: move the selected shapes straight down (or up) to rest on the plate"));
    connect(m_sitOnPlate, &QAction::triggered, this, &MainWindow::sitSelectedOnPlate);

    m_deselect = new QAction(tr("Select &None"), this);
    m_deselect->setShortcut(Qt::Key_Escape);
    connect(m_deselect, &QAction::triggered, this, [this] {
        if (m_dragState.kind != Drag::Kind::None)
            cancelDrag();  // Esc during a drag puts the body back
        else if (m_layFlat->isChecked())
            m_layFlat->setChecked(false);
        else if (m_sculptMode.active)
            endSculpt();
        else if (m_measureMode.active)
            endMeasure();
        else if (m_splitMode.active)
            endSplit();  // Esc leaves the Split tool without cutting
        else if (m_fitMode.active)
            endFit();
        else if (m_embossMode.active)
            endEmboss();
        else if (m_edgesMode.active)
            endEdges();
        else if (m_holeMode.active)
            endHole();
        else if (m_shellMode.active)
            endShell();
        else if (m_threadMode.active)
            endThread();
        else if (m_socketMode.active && m_socketMode.pickingOther) {
            m_socketMode.pickingOther = false;
            const Body* other = m_doc.find(m_socketMode.other);
            m_socketPanel->setOtherPart(other ? QString::fromStdString(other->name) : QString(), false);
        } else if (m_socketMode.active)
            endSocket();
        else if (m_hollowMode.active)
            endHollow();
        else if (m_mouldMode.active)
            endMould();
        else if (m_portMode.active)
            endPort();
        else if (m_handMode.active)
            endHand();
        else if (m_poseFigureMode.active)
            endPoseFigure();
        else if (m_paintMode.active)
            endPaint();
        else if (m_cutPartsMode.active)
            endCutParts();
        else if (m_stampMode.active)
            endStamp();
        else if (m_panelLineMode.active)
            endPanelLines();
        else if (m_hingeMode.active && m_hingeMode.pickingOther) {
            m_hingeMode.pickingOther = false;
            const Body* other = m_doc.find(m_hingeMode.other);
            m_hingePanel->setOtherPart(other ? QString::fromStdString(other->name) : QString(), false);
        } else if (m_hingeMode.active)
            endHinge();
        else if (m_poseMode.active)
            endPose();
        else if (m_printCheckMode.active)
            endPrintCheck();
        else if (m_joinMode.active)
            endJoin();
        else if (m_maskMode.active)
            endMask();
        else if (m_referenceMode.active && m_referenceMode.picking) {
            m_referenceMode.picking = 0;
            m_referencePanel->setPicking(0);
            m_referencePanel->setStatus(tr("Stopped picking."), false);
            m_viewport->setLabels({});
        } else if (m_referenceMode.active)
            endReferences();
        else if (m_mannequinOpen)
            endMannequin();
        else if (m_followMode.active)
            endFollow();
        else if (m_patternMode.active)
            endPattern();
        else if (m_sketchMode.picking) {
            m_sketchMode.picking = false;
            statusBar()->clearMessage();
            refreshScene();
        } else
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
        setIconNamed(a, QStringLiteral("shape-") + QString::fromStdString(typeName(spec)).toLower());
        connect(a, &QAction::triggered, this, [this, spec] { addBody(spec); });
        if (kind == 1)
            m_addMenu->addSeparator();  // the familiar two first
    }
    m_addMenu->addSeparator();
    m_customMenu = m_addMenu->addMenu(tr("Custom"));
    m_customMenu->setToolTipsVisible(true);
    m_customMenu->menuAction()->setToolTip(
        tr("Shapes you've saved with Save Custom Shape, each at the size it was saved"));
    m_customMenu->menuAction()->setObjectName(QStringLiteral("AddCustom"));
    setIconNamed(m_customMenu->menuAction(), QStringLiteral("shape-custom"));
    connect(m_customMenu, &QMenu::aboutToShow, this, &MainWindow::fillCustomMenu);
    fillCustomMenu();  // so it's never an empty menu
    m_saveCustomShape = new QAction(tr("Save Custom Shape..."), this);
    m_saveCustomShape->setToolTip(tr("Save Custom Shape: keep the selected shapes in your library, to add again "
                                     "at exactly this size from Add Shape → Custom"));
    m_saveCustomShape->setStatusTip(m_saveCustomShape->toolTip());
    connect(m_saveCustomShape, &QAction::triggered, this, &MainWindow::saveCustomShape);

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

    m_splitToFit = new QAction(tr("Split to Fit"), this);
    m_splitToFit->setToolTip(tr("Split to Fit: cut the selected shape into pieces that fit your printer (Build "
                                "Plate Size), keyed with alignment pegs so they line up when glued"));
    connect(m_splitToFit, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Split to Fit: select one shape on the plate first."), 6000);
            return;
        }
        splitToFit(id);
    });
    m_layOutMenu = new QMenu(tr("Lay Out"), this);
    m_layOutMenu->menuAction()->setToolTip(
        tr("Lay Out: put the selected shapes (or all of them) side by side on the build plate, ready to print, "
           "using more plates beside it if they don't all fit"));
    setIconNamed(m_layOutMenu->menuAction(), QStringLiteral("layout"));
    m_layOutMenu->setToolTipsVisible(true);
    {
        QAction* flat = m_layOutMenu->addAction(tr("Flattest Side Down"));
        flat->setToolTip(tr("Turn each shape onto the side with the most of it touching the plate, then lay them out "
                            "(best for pieces of a split)"));
        connect(flat, &QAction::triggered, this, [this] { QTimer::singleShot(0, this, [this] { layOutShapes(true); }); });
        QAction* asIs = m_layOutMenu->addAction(tr("As They Stand"));
        asIs->setToolTip(tr("Lay them out without turning them over"));
        connect(asIs, &QAction::triggered, this, [this] { QTimer::singleShot(0, this, [this] { layOutShapes(false); }); });
    }
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

    m_sculptAction = new QAction(tr("Sculpt"), this);
    m_sculptAction->setToolTip(tr("Sculpt: push, pull and smooth the selected shape with a brush, like "
                                  "modelling clay"));
    connect(m_sculptAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Sculpt: select one shape on the plate first."), 6000);
            return;
        }
        startSculpt(history::tip(m_doc, id), false);
    });
    m_cutPartsAction = new QAction(tr("Cut into Parts"), this);
    m_cutPartsAction->setToolTip(tr("Cut into Parts: cut a figure sculpted in one piece at its joints (neck, shoulders, "
                                    "elbows, hips, knees...) and put ball joints or hinges in the cuts"));
    m_cutPartsAction->setStatusTip(m_cutPartsAction->toolTip());
    connect(m_cutPartsAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Cut into Parts: select one figure on the plate first."), 6000);
            return;
        }
        startCutParts(id);
    });
    m_paintAction = new QAction(tr("Paint"), this);
    m_paintAction->setToolTip(tr("Paint: brush colours onto the selected shape (skin, clothes, eyes), for colour "
                                 "printing; exported with the colours in 3MF files"));
    m_paintAction->setStatusTip(m_paintAction->toolTip());
    connect(m_paintAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Paint: select one shape on the plate first."), 6000);
            return;
        }
        startPaint(history::tip(m_doc, id), false);
    });
    m_measureAction = new QAction(tr("Measure"), this);
    m_measureAction->setToolTip(tr("Measure: click faces, edges, corners or points to see sizes, distances and "
                                   "angles"));
    connect(m_measureAction, &QAction::triggered, this, &MainWindow::startMeasure);
    m_printCheckAction = new QAction(tr("Print Check"), this);
    m_printCheckAction->setToolTip(tr("Print Check: colour the places that are too thin to print well, and the "
                                      "faces that hang over the air the way the parts sit (they'll need "
                                      "supports). Nothing is changed."));
    m_printCheckAction->setStatusTip(m_printCheckAction->toolTip());
    connect(m_printCheckAction, &QAction::triggered, this, &MainWindow::startPrintCheck);
    m_simplifyAction = new QAction(tr("Simplify"), this);
    m_simplifyAction->setToolTip(tr("Simplify: fewer triangles for the selected shape, keeping its look (for big "
                                    "imported or sculpted meshes). Set how many to keep in Properties."));
    connect(m_simplifyAction, &QAction::triggered, this, &MainWindow::simplifySelected);
    m_smoothAction = new QAction(tr("Smooth"), this);
    m_smoothAction->setToolTip(tr("Smooth: round off the whole surface of the selected shape, edges and corners "
                                  "included. Set how strongly in Properties."));
    connect(m_smoothAction, &QAction::triggered, this, &MainWindow::smoothSelected);
    m_extrudeDownAction = new QAction(tr("Extrude Down"), this);
    m_extrudeDownAction->setToolTip(tr("Extrude Down: stretch the underside of the selected shape straight down to "
                                       "the plate, filling everything below it (for printing without supports)"));
    connect(m_extrudeDownAction, &QAction::triggered, this, &MainWindow::extrudeDownSelected);
    m_sculptSmaller = new QAction(this);
    m_sculptSmaller->setShortcut(Qt::Key_BracketLeft);
    connect(m_sculptSmaller, &QAction::triggered, this, [this] {
        if (m_sculptMode.active)
            m_sculptPanel->setSize(m_sculptPanel->size() / 1.2);
        if (m_maskMode.active)
            m_maskPanel->setBrushSize(m_maskPanel->brushSize() / 1.2);
        if (m_paintMode.active)
            m_paintPanel->setBrushSize(m_paintPanel->brushSize() / 1.2);
    });
    m_sculptBigger = new QAction(this);
    m_sculptBigger->setShortcut(Qt::Key_BracketRight);
    connect(m_sculptBigger, &QAction::triggered, this, [this] {
        if (m_sculptMode.active)
            m_sculptPanel->setSize(m_sculptPanel->size() * 1.2);
        if (m_maskMode.active)
            m_maskPanel->setBrushSize(m_maskPanel->brushSize() * 1.2);
        if (m_paintMode.active)
            m_paintPanel->setBrushSize(m_paintPanel->brushSize() * 1.2);
    });
    addAction(m_sculptSmaller);
    addAction(m_sculptBigger);

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
        startEmboss(history::tip(m_doc, id), false);  // at the rollback bar, if it's rolled back
    });

    m_edgesAction = new QAction(tr("Edges"), this);
    m_edgesAction->setToolTip(tr("Edges: round, fillet, chamfer or bevel the edges of the selected shape"));
    connect(m_edgesAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Edges: select one shape on the plate first."), 6000);
            return;
        }
        const ObjectId target = history::tip(m_doc, id);  // at the rollback bar, if it's rolled back
        if (m_doc.usesMeshEngine(target)) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("Rounding and chamfering work on solid shapes made in Forge3D, not on "
                                        "imported meshes or shapes made with them."));
            return;
        }
        startEdges(target, false);
    });

    m_sketchAction = new QAction(tr("Sketch"), this);
    m_sketchAction->setToolTip(tr("Sketch: draw an outline, then push it out (Pad) or cut it in (Pocket). With "
                                  "nothing selected it goes on the plate; with a shape selected, click the flat "
                                  "face to draw on"));
    connect(m_sketchAction, &QAction::triggered, this, &MainWindow::sketchClicked);
    m_sketchMenu = new QMenu(tr("Sketch on"), this);
    {
        QAction* onFace = m_sketchMenu->addAction(tr("A face (click it), or the plate"));
        onFace->setToolTip(m_sketchAction->toolTip());
        connect(onFace, &QAction::triggered, this, &MainWindow::sketchClicked);
        m_sketchMenu->addSeparator();
        const QString names[] = {tr("Front plane through the middle"), tr("Side plane through the middle"),
                                 tr("Top plane through the middle")};
        for (int which = 0; which < 3; ++which) {
            QAction* a = m_sketchMenu->addAction(names[which]);
            a->setToolTip(tr("Sketch on a plane through the middle of the selected shape (or the plate's middle "
                             "with nothing selected). Its cut through the shape is shown to snap to: draw a "
                             "groove's outline at a round part's edge and spin it round the middle line."));
            connect(a, &QAction::triggered, this, [this, which] { sketchOnPlane(which); });
        }
        m_sketchMenu->addSeparator();
        m_sketchOnSection = m_sketchMenu->addAction(tr("The section view's cut"));
        m_sketchOnSection->setToolTip(tr("Sketch on the flat cut that Section View shows (turn Section View on "
                                         "first, View tab). The cut through the selected shape is shown to snap "
                                         "to. With a shape selected you can also click Sketch and then the cut "
                                         "face itself."));
        m_sketchOnSection->setEnabled(false);
        connect(m_sketchOnSection, &QAction::triggered, this, &MainWindow::sketchOnSection);
        m_sketchMenu->setToolTipsVisible(true);
    }

    m_holeAction = new QAction(tr("Hole"), this);
    m_holeAction->setToolTip(tr("Hole: drill holes (plain, counterbored, countersunk; standard metric sizes) into a "
                                "flat face of the selected shape"));
    connect(m_holeAction, &QAction::triggered, this, &MainWindow::holeClicked);
    m_followAction = new QAction(tr("Follow"), this);
    m_followAction->setToolTip(tr("Follow: draw a path on the selected shape's surface and cut a groove (or raise a "
                                  "ridge) along it, round, square, V or your own shape across"));
    connect(m_followAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Follow: select one shape on the plate first."), 6000);
            return;
        }
        const ObjectId target = history::tip(m_doc, id);
        if (m_doc.usesMeshEngine(target)) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("Follow works on solid shapes made in Forge3D, not on imported meshes or "
                                        "shapes made with them."));
            return;
        }
        startFollow(target, false);
    });
    m_threadAction = new QAction(tr("Thread"), this);
    m_threadAction->setToolTip(tr("Thread: cut a screw thread into a round face of the selected shape: a peg (a "
                                  "bolt) or a hole (a nut), in standard metric sizes"));
    connect(m_threadAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Thread: select one shape on the plate first."), 6000);
            return;
        }
        const ObjectId target = history::tip(m_doc, id);
        if (m_doc.usesMeshEngine(target)) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("Threads go on solid shapes made in Forge3D, not on imported meshes or "
                                        "shapes made with them (sculpted, embossed or already threaded). Add the "
                                        "thread before those steps."));
            return;
        }
        startThread(target, false);
    });
    m_poseAction = new QAction(tr("Pose Test"), this);
    m_poseAction->setToolTip(tr("Pose Test: turn the joints made with Quick Hinge and Quick Socket to see how far "
                                "they go before parts hit, and save poses by name. The parts aren't changed."));
    m_poseAction->setStatusTip(m_poseAction->toolTip());
    connect(m_poseAction, &QAction::triggered, this, &MainWindow::startPose);
    m_hingeAction = new QAction(tr("Quick Hinge"), this);
    m_hingeAction->setToolTip(tr("Quick Hinge: a hinge (single or double) or a swivel at the spot you click on "
                                 "the selected shape: a fork and a tab that turns in it on a pin, sized to fit "
                                 "once printed"));
    m_hingeAction->setStatusTip(m_hingeAction->toolTip());
    connect(m_hingeAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Quick Hinge: select one shape on the plate first."), 6000);
            return;
        }
        startHinge(history::tip(m_doc, id), false);
    });
    m_socketAction = new QAction(tr("Quick Socket"), this);
    m_socketAction->setToolTip(tr("Quick Socket: cut a socket for a ball joint or a peg into the selected shape, "
                                  "at the spot you click, sized to fit once printed"));
    connect(m_socketAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Quick Socket: select one shape on the plate first."), 6000);
            return;
        }
        startSocket(history::tip(m_doc, id), false);
    });
    m_patternAction = new QAction(tr("Pattern"), this);
    m_patternAction->setToolTip(tr("Pattern: linked copies in a row, a grid or round a circle, of the selected shape "
                                   "or of the hole / pocket / groove its last step made"));
    connect(m_patternAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Pattern: select one shape on the plate first."), 6000);
            return;
        }
        startPattern(history::tip(m_doc, id), false);
    });
    m_panelLineAction = new QAction(tr("Panel Lines"), this);
    m_panelLineAction->setToolTip(tr("Panel Lines: click points along the selected shape's surface to cut a thin "
                                     "groove that follows it (armour plates, vehicle panels, scale models)"));
    m_panelLineAction->setStatusTip(m_panelLineAction->toolTip());
    connect(m_panelLineAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Panel Lines: select one shape on the plate first."), 6000);
            return;
        }
        startPanelLines(history::tip(m_doc, id), false);
    });
    m_stampAction = new QAction(tr("Stamp"), this);
    m_stampAction->setToolTip(tr("Stamp: small details on the selected shape's surface (rivets, bolts, screws, "
                                 "vents, scales, stitches, or your own custom shapes), raised or cut in; click "
                                 "for one, drag for a row"));
    m_stampAction->setStatusTip(m_stampAction->toolTip());
    connect(m_stampAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Stamp: select one shape on the plate first."), 6000);
            return;
        }
        startStamp(history::tip(m_doc, id), false);
    });
    m_poseFigureAction = new QAction(tr("Pose Figure"), this);
    m_poseFigureAction->setToolTip(tr("Pose Figure: put a simple skeleton in a figure sculpted in one piece (or an "
                                      "imported one) and bend it at its joints, the surface following"));
    m_poseFigureAction->setStatusTip(m_poseFigureAction->toolTip());
    connect(m_poseFigureAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Pose Figure: select one figure on the plate first."), 6000);
            return;
        }
        startPoseFigure(history::tip(m_doc, id), false);
    });
    m_hollowAction = new QAction(tr("Hollow"), this);
    m_hollowAction->setToolTip(tr("Hollow: for resin printing, take the inside out of the selected shape (any shape, "
                                  "sculpts too), leaving walls of a set thickness, with drain holes and plugs"));
    m_hollowAction->setStatusTip(m_hollowAction->toolTip());
    connect(m_hollowAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Hollow: select one shape on the plate first."), 6000);
            return;
        }
        startHollow(history::tip(m_doc, id), false);
    });
    m_handAction = new QAction(tr("Make Hand"), this);
    m_handAction->setToolTip(tr("Make Hand: fists, gripping, open, relaxed and pointing hands at your figure's scale, "
                                "with the socket for the wrist peg"));
    m_handAction->setStatusTip(m_handAction->toolTip());
    connect(m_handAction, &QAction::triggered, this, &MainWindow::startHand);
    m_portAction = new QAction(tr("Hand Ports"), this);
    m_portAction->setToolTip(tr("Hand Ports: a wrist peg on an arm for swappable hands, the socket for it in each "
                                "hand, and grip holes through hands sized to weapons' handles"));
    m_portAction->setStatusTip(m_portAction->toolTip());
    connect(m_portAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Hand Ports: select one shape on the plate first (an arm, a hand or an "
                                        "accessory)."),
                                     6000);
            return;
        }
        startPort(id, false);
    });
    m_mouldAction = new QAction(tr("Make Mould"), this);
    m_mouldAction->setToolTip(tr("Make Mould: a two-part mould box round the selected shape, for casting resin "
                                 "copies of a printed master, with registration keys, a pour spout and air vents"));
    m_mouldAction->setStatusTip(m_mouldAction->toolTip());
    connect(m_mouldAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Make Mould: select one shape on the plate first."), 6000);
            return;
        }
        startMould(id, false);
    });
    m_shellAction = new QAction(tr("Shell"), this);
    m_shellAction->setToolTip(tr("Shell: hollow the selected shape out to walls of a set thickness, leaving the "
                                 "faces you click open"));
    connect(m_shellAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Shell: select one shape on the plate first."), 6000);
            return;
        }
        const ObjectId target = history::tip(m_doc, id);
        if (m_doc.usesMeshEngine(target)) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("Shell works on solid shapes made in Forge3D, not on imported meshes or "
                                        "shapes made with them."));
            return;
        }
        startShell(target, false);
    });

    m_maskAction = new QAction(tr("Split by Mask"), this);
    m_maskAction->setToolTip(tr("Split by Mask: paint over part of the selected shape, and it's cut off along the "
                                "edge of the paint; both pieces are closed with the same smooth cap"));
    connect(m_maskAction, &QAction::triggered, this, [this] {
        const ObjectId id = selectedOne();
        if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
            statusBar()->showMessage(tr("Split by Mask: select one shape on the plate first."), 6000);
            return;
        }
        startMask(id, false);
    });
    m_joinAction = new QAction(tr("Join Parts"), this);
    m_joinAction->setToolTip(tr("Join Parts: move one part so it joins another: a peg into its socket, a ball "
                                "into its cup, a face flat against a face. Click the place on each part."));
    connect(m_joinAction, &QAction::triggered, this, &MainWindow::startJoin);

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
    m_fit->setToolTip(tr("Fit: show the whole model (F)"));
    m_resetView->setToolTip(tr("Reset View: back to the starting view (Home)"));

    // The left mouse button working the view. One at a time, or none (the usual:
    // the left button selects and moves, the right button turns the view).
    auto* nav = new QActionGroup(this);
    nav->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    const auto navAction = [this, nav](const QString& name, const QString& tip, Viewport::LeftDrag what) {
        auto* a = new QAction(name, this);
        a->setCheckable(true);
        a->setToolTip(tip);
        a->setStatusTip(tip);
        nav->addAction(a);
        connect(a, &QAction::toggled, this, [this, a, what, name](bool on) {
            if (on) {
                m_viewport->setLeftDrag(what);
                statusBar()->showMessage(tr("%1 is on: drag with the left button in the view. Click %1 again "
                                            "to go back to selecting and moving shapes.")
                                             .arg(name));
            } else if (m_viewport->leftDrag() == what) {
                m_viewport->setLeftDrag(Viewport::LeftDrag::Tool);
                statusBar()->clearMessage();
            }
        });
        return a;
    };
    m_navOrbit = navAction(tr("Orbit"),
                           tr("Orbit: while this is on, dragging with the left button turns the view (as the "
                              "right button always does). Click again to turn it off."),
                           Viewport::LeftDrag::Orbit);
    m_navPan = navAction(tr("Pan"),
                         tr("Pan: while this is on, dragging with the left button slides the view (as the middle "
                            "button always does). Click again to turn it off."),
                         Viewport::LeftDrag::Pan);
    m_navZoom = navAction(tr("Zoom"),
                          tr("Zoom: while this is on, dragging up and down with the left button zooms (as the "
                             "wheel always does). Click again to turn it off."),
                          Viewport::LeftDrag::Zoom);

    m_sectionAction = new QAction(tr("&Section View"), this);
    m_sectionAction->setCheckable(true);
    m_sectionAction->setToolTip(tr("Section View: cut the view open to look inside the model (the model itself "
                                   "isn't changed)"));
    m_sectionAction->setStatusTip(m_sectionAction->toolTip());

    m_referenceAction = new QAction(tr("Reference &Images..."), this);
    m_referenceAction->setToolTip(tr("Reference Images: put a photo or drawing behind the model for each side, "
                                     "sized to a length you know, to build to. Never exported or printed."));
    m_referenceAction->setStatusTip(m_referenceAction->toolTip());
    connect(m_referenceAction, &QAction::triggered, this, [this] { startReferences(); });
    m_mannequinAction = new QAction(tr("&Figure Guide..."), this);
    m_mannequinAction->setToolTip(tr("Figure Guide: a see-through mannequin to build a figure to, at a figure "
                                     "line's height and in proportion (heads tall), posed how you like. Never "
                                     "printed."));
    m_mannequinAction->setStatusTip(m_mannequinAction->toolTip());
    connect(m_mannequinAction, &QAction::triggered, this, &MainWindow::startMannequin);
    m_showPicturesAction = new QAction(tr("Show &Pictures"), this);
    m_showPicturesAction->setCheckable(true);
    m_showPicturesAction->setChecked(Preferences().value(QStringLiteral("reference/showAll"), true).toBool());
    m_showPicturesAction->setToolTip(tr("Show or hide all the reference pictures (they stay in the file)"));
    m_showPicturesAction->setStatusTip(m_showPicturesAction->toolTip());
    connect(m_showPicturesAction, &QAction::toggled, this, [this](bool on) {
        Preferences().setValue(QStringLiteral("reference/showAll"), on);
        refreshReferences();
    });

    {
        // How shapes are drawn (remembered).
        auto* looks = new QActionGroup(this);
        const struct {
            Viewport::Look look;
            QString name, icon, tip;
        } choices[] = {
            {Viewport::Look::Solid, tr("S&olid"), QStringLiteral("look-solid"),
             tr("Solid: shapes are drawn solid, as they will print")},
            {Viewport::Look::SeeThrough, tr("See-&Through"), QStringLiteral("look-see-through"),
             tr("See-Through: shapes are half clear, so you can see into them and what's behind them")},
            {Viewport::Look::Wireframe, tr("&Wireframe"), QStringLiteral("look-wireframe"),
             tr("Wireframe: shapes are drawn as the edges of their triangles")},
        };
        const int saved = std::clamp(Preferences().value(QStringLiteral("view/look"), 0).toInt(), 0, 2);
        for (const auto& c : choices) {
            auto* a = new QAction(c.name, this);
            a->setCheckable(true);
            a->setToolTip(c.tip);
            a->setStatusTip(c.tip);
            setIconNamed(a, c.icon);
            looks->addAction(a);
            m_lookActions[std::size_t(c.look)] = a;
            connect(a, &QAction::triggered, this, [this, look = c.look] {
                m_viewport->setLook(look);
                Preferences().setValue(QStringLiteral("view/look"), int(look));
            });
        }
        m_lookActions[std::size_t(saved)]->setChecked(true);
        m_viewport->setLook(Viewport::Look(saved));
    }

    m_plateSizeAction = new QAction(tr("Build &Plate Size..."), this);
    m_plateSizeAction->setToolTip(tr("Plate Size: set the grid to the size of your printer's bed"));
    m_plateSizeAction->setStatusTip(m_plateSizeAction->toolTip());
    connect(m_plateSizeAction, &QAction::triggered, this, &MainWindow::choosePlateSize);
}

void MainWindow::createMenus()
{
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_new);
    file->addAction(m_open);
    m_recentMenu = file->addMenu(tr("Open &Recent"));
    m_recentMenu->setToolTipsVisible(true);
    noteRecentFile({});
    file->addSeparator();
    file->addAction(m_save);
    file->addAction(m_saveAs);
    file->addAction(m_saveCustomShape);
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
    edit->addAction(m_splitToFit);
    edit->addMenu(m_layOutMenu);
    edit->addAction(m_emboss);
    edit->addAction(m_edgesAction);
    edit->addAction(m_sketchAction);
    edit->addMenu(m_sketchMenu);
    edit->addAction(m_holeAction);
    edit->addAction(m_shellAction);
    edit->addAction(m_followAction);
    edit->addAction(m_patternAction);
    edit->addMenu(m_alignMenu);
    edit->addMenu(m_mirrorMenu);
    edit->addAction(m_group);

    edit->addSeparator();
    edit->addAction(m_plateSizeAction);
    {
        QAction* backups = edit->addAction(tr("&Backup Copies..."));
        backups->setToolTip(tr("How often a backup copy of unsaved work is made, or none at all"));
        backups->setStatusTip(backups->toolTip());
        connect(backups, &QAction::triggered, this, &MainWindow::chooseBackups);
    }

    QMenu* add = menuBar()->addMenu(tr("&Add"));
    add->addActions(m_addMenu->actions());

    QMenu* view = menuBar()->addMenu(tr("&View"));
    view->addAction(m_fit);
    view->addAction(m_resetView);
    view->addSeparator();
    view->addAction(m_navOrbit);
    view->addAction(m_navPan);
    view->addAction(m_navZoom);
    auto* viewCube = view->addAction(tr("View &Cube"));
    viewCube->setCheckable(true);
    viewCube->setToolTip(tr("The cube in the top-right corner of the view: it shows which side faces you, and "
                            "clicking a side, edge or corner turns the view to it"));
    viewCube->setChecked(Preferences().value(QStringLiteral("view/cube"), true).toBool());
    m_viewport->setViewCubeVisible(viewCube->isChecked());
    connect(viewCube, &QAction::toggled, this, [this](bool on) {
        m_viewport->setViewCubeVisible(on);
        Preferences().setValue(QStringLiteral("view/cube"), on);
    });
    view->addSeparator();
    for (QAction* a : m_lookActions)
        view->addAction(a);
    view->addSeparator();
    view->addAction(m_plateSizeAction);
    applyPlateSize();
    {
        QMenu* themes = view->addMenu(tr("&Theme"));
        auto* group = new QActionGroup(themes);
        const std::pair<theme::Choice, QString> choices[] = {
            {theme::Choice::System, tr("Follow &Windows")},
            {theme::Choice::Light, tr("&Light")},
            {theme::Choice::Dark, tr("&Dark")},
        };
        for (const auto& [choice, name] : choices) {
            QAction* a = themes->addAction(name);
            a->setCheckable(true);
            a->setChecked(theme::saved() == choice);
            group->addAction(a);
            connect(a, &QAction::triggered, this, [this, choice = choice] { chooseTheme(int(choice)); });
        }
    }
    view->addSeparator();
    m_viewMenu = view;  // the panels' show/hide entries are added in createDocks()

    QMenu* tools = menuBar()->addMenu(tr("&Tools"));
    QAction* scaleCalculator = tools->addAction(tr("&Scale Calculator..."), this, [this] {
        if (!m_scaleCalculator)
            m_scaleCalculator = new ScaleCalculator(this);
        m_scaleCalculator->show();
        m_scaleCalculator->raise();
        m_scaleCalculator->activateWindow();
    });
    scaleCalculator->setStatusTip(tr("Work out a size at a scale: a real size in any unit, at 1:X, on the model"));
    QAction* toleranceTest = tools->addAction(tr("&Tolerance Test Print..."), this, [this] {
        if (!m_toleranceDialog) {
            m_toleranceDialog = new ToleranceDialog(this);
            connect(m_toleranceDialog, &ToleranceDialog::addRequested, this, &MainWindow::addToleranceTest);
        }
        m_toleranceDialog->refreshStandard();
        m_toleranceDialog->show();
        m_toleranceDialog->raise();
        m_toleranceDialog->activateWindow();
    });
    toleranceTest->setStatusTip(tr("Print holes, cups and pin holes at stepped fits once, and use the fit that worked "
                                   "best from then on"));
    QAction* jointStandards = tools->addAction(tr("&Joint Standards..."), this, [this] {
        if (!m_standardsDialog) {
            m_standardsDialog = new JointStandardsDialog(this);
            connect(m_standardsDialog, &JointStandardsDialog::changed, this, [this] {
                refreshStandardRows();
                if (m_toleranceDialog)
                    m_toleranceDialog->refreshStandard();
            });
        }
        m_standardsDialog->reload();
        m_standardsDialog->show();
        m_standardsDialog->raise();
        m_standardsDialog->activateWindow();
    });
    jointStandards->setStatusTip(tr("A figure line's joint sizes, kept by name, for Quick Socket, Quick Hinge and "
                                    "the Tolerance Test Print to start from"));
    tools->addSeparator();
    QAction* rescale = tools->addAction(tr("&Rescale Design..."), this, &MainWindow::rescaleDesign);
    rescale->setStatusTip(tr("Move the design to another scale (1:12 to 1:18, say), keeping the joints at their real "
                             "sizes"));
    QAction* kitLayout = tools->addAction(tr("&Kit Layout..."), this, &MainWindow::makeKit);
    kitLayout->setStatusTip(tr("Lay small parts out on a frame like a model kit's sprue, gated and numbered"));
    tools->addSeparator();
    QAction* partsList = tools->addAction(tr("&Parts List..."), this, &MainWindow::showPartsList);
    partsList->setStatusTip(tr("Every part's size, volume, print weight and material cost, as a table to save or "
                               "print"));

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("Open &log folder"), this, [] {
        const QString path = logFilePath();
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    help->addAction(tr("&Quick Start Guide"), this, &MainWindow::showQuickStart);
    help->addAction(tr("&Figure, Toy and Kit Tools"), this, &MainWindow::showFigureTools);
    help->addAction(tr("Take the &Tour"), this, &MainWindow::startTour);
    help->addAction(tr("Check for &Updates..."), this, [this] { checkForUpdates(false); });
    m_checkAtStart = help->addAction(tr("Check for Updates at &Start"));
    m_checkAtStart->setCheckable(true);
    m_checkAtStart->setChecked(Preferences().value(QStringLiteral("updates/atStart"), false).toBool());
    m_checkAtStart->setToolTip(tr("Look for a newer Forge3D each time it starts (it only asks GitHub for the "
                                  "latest version number)"));
    connect(m_checkAtStart, &QAction::toggled, this,
            [](bool on) { Preferences().setValue(QStringLiteral("updates/atStart"), on); });
    help->addSeparator();
    help->addAction(tr("&About Forge3D"), this, &MainWindow::showAbout);
}

void MainWindow::createToolBar()
{
    // The ribbon: tabs of grouped buttons (Build / Design / Sculpt / Print), in a fixed
    // bar across the top. Each button runs a window action, so the menus, shortcuts and
    // enabling all stay as they are.
    // Pictures for the buttons (and the menus): line drawings from icons.qrc.
    const std::pair<QAction*, const char*> pictures[] = {
        {m_open, "open"},
        {m_save, "save"},
        {m_import, "import"},
        {m_undo, "undo"},
        {m_redo, "redo"},
        {m_addMenu->menuAction(), "add-shape"},
        {m_saveCustomShape, "save-custom"},
        {m_modeMove, "move"},
        {m_modeRotate, "rotate"},
        {m_modeResize, "resize"},
        {m_snapAction, "snap"},
        {m_uniformAction, "uniform"},
        {m_merge, "merge"},
        {m_subtract, "subtract"},
        {m_intersect, "intersect"},
        {m_separate, "separate"},
        {m_alignMenu->menuAction(), "align"},
        {m_joinAction, "join"},
        {m_maskAction, "split-mask"},
        {m_mirrorMenu->menuAction(), "mirror"},
        {m_patternAction, "pattern"},
        {m_group, "group"},
        {m_duplicate, "duplicate"},
        {m_delete, "delete"},
        {m_clear, "clear-plate"},
        {m_sketchAction, "sketch"},
        {m_holeAction, "hole"},
        {m_shellAction, "shell"},
        {m_followAction, "follow"},
        {m_edgesAction, "edges"},
        {m_split, "split"},
        {m_splitToFit, "split-fit"},
        {m_emboss, "emboss"},
        {m_sculptAction, "sculpt"},
        {m_simplifyAction, "simplify"},
        {m_smoothAction, "smooth"},
        {m_extrudeDownAction, "extrude-down"},
        {m_measureAction, "measure"},
        {m_printCheckAction, "print-check"},
        {m_threadAction, "thread"},
        {m_socketAction, "socket"},
        {m_hingeAction, "hinge"},
        {m_poseAction, "pose"},
        {m_layFlat, "lay-on-face"},
        {m_sitOnPlate, "sit-on-plate"},
        {m_export, "export"},
        {m_sendToSlicer, "send-to-slicer"},
        {m_navOrbit, "orbit"},
        {m_navPan, "pan"},
        {m_navZoom, "zoom"},
        {m_fit, "fit"},
        {m_resetView, "reset-view"},
        {m_sectionAction, "section"},
        {m_referenceAction, "reference"},
        {m_mannequinAction, "mannequin"},
        {m_hollowAction, "hollow"},
        {m_mouldAction, "mould"},
        {m_portAction, "ports"},
        {m_handAction, "hand"},
        {m_poseFigureAction, "pose-figure"},
        {m_paintAction, "paint"},
        {m_cutPartsAction, "cut-parts"},
        {m_stampAction, "stamp"},
        {m_panelLineAction, "panel-lines"},
        {m_showPicturesAction, "reference"},
        {m_plateSizeAction, "plate"},
    };
    for (const auto& [action, name] : pictures)
        setIconNamed(action, QString::fromLatin1(name));

    QToolBar* bar = addToolBar(tr("Ribbon"));
    bar->setObjectName(QStringLiteral("Ribbon"));
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->toggleViewAction()->setEnabled(false);
    auto* tabs = new QTabWidget(bar);
    tabs->setObjectName(QStringLiteral("RibbonTabs"));
    tabs->setDocumentMode(true);
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bar->addWidget(tabs);

    struct Page {
        QWidget* widget;
        QHBoxLayout* row;
    };
    const auto page = [tabs](const QString& title, const QString& tip) {
        auto* w = new QWidget;
        auto* row = new QHBoxLayout(w);
        row->setContentsMargins(2, 2, 2, 0);
        row->setSpacing(2);
        row->setSizeConstraint(QLayout::SetMinimumSize);
        // In a narrow window the buttons keep their size and the row scrolls sideways
        // (a scroll bar appears under it), rather than squashing the names.
        auto* scroll = new RibbonScroll(w);
        const int index = tabs->addTab(scroll, title);
        tabs->setTabToolTip(index, tip);
        return Page{w, row};
    };
    // A group: buttons in a row, its name underneath, a line after it.
    const auto group = [](Page& p, const QString& name, const std::vector<QWidget*>& buttons) {
        auto* box = new QWidget;
        auto* column = new QVBoxLayout(box);
        column->setContentsMargins(1, 0, 1, 0);
        column->setSpacing(0);
        auto* row = new QHBoxLayout;
        row->setSpacing(1);
        for (QWidget* b : buttons)
            row->addWidget(b);
        column->addLayout(row);
        auto* label = new QLabel(name);
        label->setAlignment(Qt::AlignCenter);
        QFont small = label->font();
        small.setPointSizeF(small.pointSizeF() * 0.85);
        label->setFont(small);
        label->setStyleSheet(QStringLiteral("color: palette(mid);"));
        column->addWidget(label);
        p.row->addWidget(box);
        auto* line = new QFrame;
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);
        p.row->addWidget(line);
    };
    const auto button = [](QAction* action) {
        auto* b = new QToolButton;
        b->setDefaultAction(action);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIconSize(QSize(32, 32));
        b->setAutoRaise(true);
        return static_cast<QWidget*>(b);
    };
    // Undo / Redo keep a short name in the ribbon (so it doesn't change width); what
    // they'd undo shows as the tip.
    const auto shortButton = [this, button](QAction* action, const QString& name) {
        auto* b = static_cast<QToolButton*>(button(action));
        const auto shorten = [b, action, name] {
            b->setText(name);
            b->setToolTip(QString(action->text()).remove(QLatin1Char('&')));
        };
        shorten();
        connect(action, &QAction::changed, b, shorten);
        return static_cast<QWidget*>(b);
    };
    const auto menuButton = [](QMenu* menu, const QString& name = {}) {
        auto* b = new QToolButton;
        b->setDefaultAction(menu->menuAction());
        if (!name.isEmpty())
            b->setText(name);
        b->setPopupMode(QToolButton::InstantPopup);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIconSize(QSize(32, 32));
        b->setAutoRaise(true);
        return static_cast<QWidget*>(b);
    };

    // Check and Undo are on every tab.
    const auto common = [&](Page& p) {
        group(p, tr("Check"), {button(m_measureAction), button(m_printCheckAction)});
        group(p, tr("Undo"), {shortButton(m_undo, tr("Undo")), shortButton(m_redo, tr("Redo"))});
        p.row->addStretch(1);
    };
    const auto named = [&](QAction* action, const QString& name) {
        auto* b = static_cast<QToolButton*>(button(action));
        // (A tool button takes its action's text again whenever the action changes.)
        const auto keep = [b, name] { b->setText(name); };
        keep();
        connect(action, &QAction::changed, b, keep);
        return static_cast<QWidget*>(b);
    };

    Page build = page(tr("Build"), tr("Shapes: open, add, move, turn and resize them"));
    group(build, tr("File"), {button(m_open), button(m_save), button(m_import)});
    {
        QWidget* add = menuButton(m_addMenu);
        add->setObjectName(QStringLiteral("AddShapeButton"));
        group(build, tr("Add"), {add});
    }
    group(build, tr("Handles"),
          {button(m_modeMove), button(m_modeRotate), button(m_modeResize), button(m_snapAction),
           button(m_uniformAction)});
    group(build, tr("Look"), {menuButton(m_colourMenu)});
    group(build, tr("Remove"), {button(m_delete), button(m_clear)});
    common(build);

    Page design = page(tr("Design"), tr("Combine and arrange shapes; sketches, words and cuts"));
    group(design, tr("Combine"), {button(m_merge), button(m_subtract), button(m_intersect), button(m_separate)});
    {
        QWidget* align = menuButton(m_alignMenu, tr("Align"));
        align->setObjectName(QStringLiteral("AlignShapesButton"));
        group(design, tr("Arrange"),
              {align, button(m_joinAction), menuButton(m_mirrorMenu), button(m_group), button(m_duplicate)});
    }
    {
        // Sketch: click for a face (or the plate); the arrow offers the middle planes.
        auto* sketchButton = new QToolButton;
        sketchButton->setDefaultAction(m_sketchAction);
        sketchButton->setMenu(m_sketchMenu);
        sketchButton->setPopupMode(QToolButton::MenuButtonPopup);
        sketchButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        sketchButton->setIconSize(QSize(32, 32));
        sketchButton->setAutoRaise(true);
        group(design, tr("Sketch"), {sketchButton});
    }
    group(design, tr("Change"), {button(m_split), button(m_maskAction), button(m_emboss)});
    group(design, tr("Place"), {button(m_layFlat), button(m_sitOnPlate), button(m_extrudeDownAction)});
    common(design);

    Page features = page(tr("Features"), tr("Holes, threads, sockets, shells, grooves, patterns and edges"));
    group(features, tr("Holes and Joints"), {button(m_holeAction), button(m_threadAction), button(m_socketAction), button(m_hingeAction),
                                       button(m_portAction), button(m_poseAction)});
    group(features, tr("Surface"),
          {button(m_shellAction), button(m_hollowAction), button(m_followAction), button(m_panelLineAction),
           button(m_stampAction), button(m_edgesAction)});
    group(features, tr("Repeat"), {button(m_patternAction)});
    common(features);

    Page sculpt = page(tr("Sculpt"), tr("Sculpting brushes"));
    group(sculpt, tr("Brushes"), {button(m_sculptAction), button(m_paintAction)});
    group(sculpt, tr("Whole Shape"), {button(m_smoothAction), button(m_simplifyAction), button(m_poseFigureAction),
                                      button(m_cutPartsAction), button(m_handAction)});
    common(sculpt);

    Page viewPage = page(tr("View"), tr("Turn, slide and zoom the view; look inside; the build plate"));
    group(viewPage, tr("Workspace"),
          {button(m_navOrbit), button(m_navPan), button(m_navZoom), named(m_fit, tr("Fit")),
           named(m_resetView, tr("Reset View"))});
    group(viewPage, tr("Look"), {button(m_lookActions[0]), button(m_lookActions[1]), button(m_lookActions[2])});
    group(viewPage, tr("Objects"), {named(m_sectionAction, tr("Section View"))});
    group(viewPage, tr("Pictures"),
          {named(m_referenceAction, tr("Reference Images")), named(m_showPicturesAction, tr("Show Pictures")),
           named(m_mannequinAction, tr("Figure Guide"))});
    group(viewPage, tr("Plate"), {named(m_plateSizeAction, tr("Plate Size"))});
    common(viewPage);

    Page print = page(tr("Print/Save"), tr("Save it, or get it to the printer"));
    group(print, tr("Prepare"), {button(m_splitToFit), menuButton(m_layOutMenu), button(m_mouldAction)});
    group(print, tr("Out"), {button(m_save), button(m_export), button(m_sendToSlicer)});
    group(print, tr("Library"), {button(m_saveCustomShape)});
    common(print);

    // Keep the tab you were on.
    // Kept by name ("ribbon/tabName"), so a new tab doesn't move you. Before 0.55.1 it was
    // kept by place (Build, Design, Sculpt, View, Print/Save): Features now follows Design.
    Preferences settings;
    int start = 0;
    const QString lastTab = settings.value(QStringLiteral("ribbon/tabName")).toString();
    bool found = false;
    for (int i = 0; i < tabs->count() && !lastTab.isEmpty(); ++i)
        if (tabs->tabText(i) == lastTab) {
            start = i;
            found = true;
        }
    if (!found && settings.contains(QStringLiteral("ribbon/tab"))) {
        const int old = settings.value(QStringLiteral("ribbon/tab"), 0).toInt();
        start = old >= 2 ? old + 1 : old;
    }
    tabs->setCurrentIndex(std::clamp(start, 0, tabs->count() - 1));
    connect(tabs, &QTabWidget::currentChanged, this, [tabs](int index) {
        Preferences().setValue(QStringLiteral("ribbon/tabName"), tabs->tabText(index));
    });
    m_ribbonTabs = tabs;
}

void MainWindow::createDocks()
{
    // Objects tree (left): combined shapes hold their parts.
    m_objects = new QTreeWidget;
    m_objects->setHeaderHidden(true);
    m_objects->setColumnCount(2);
    m_objects->header()->setStretchLastSection(true);
    m_objects->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_objects->setSelectionMode(QAbstractItemView::ExtendedSelection);  // Ctrl+click, Shift+click
    m_objects->setContextMenuPolicy(Qt::CustomContextMenu);
    m_objects->setToolTip(tr("Every object. Open one to see how it was made, step by step: double-click a "
                             "step to change it, right-click for more."));
    m_objects->viewport()->installEventFilter(this);
    connect(m_objects, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelectionChanged);
    connect(m_objects, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeMenu);
    // Double-clicks are handled in eventFilter: the first click can rebuild the list
    // (selecting), and Qt then misses the double-click on the new row.
    const auto keyOf = [](QTreeWidgetItem* item) {
        return std::pair{item->data(0, kRowKindRole).toInt(), item->data(0, Qt::UserRole).value<ObjectId>()};
    };
    connect(m_objects, &QTreeWidget::itemExpanded, this, [this, keyOf](QTreeWidgetItem* item) {
        if (!m_syncingList)
            m_expanded.insert(keyOf(item));
    });
    connect(m_objects, &QTreeWidget::itemCollapsed, this, [this, keyOf](QTreeWidgetItem* item) {
        if (!m_syncingList)
            m_expanded.erase(keyOf(item));
    });
    auto* objectsDock = new QDockWidget(tr("Objects"), this);
    objectsDock->setObjectName(QStringLiteral("ObjectsDock"));
    objectsDock->setWidget(m_objects);
    objectsDock->setMinimumWidth(270);  // room for the step descriptions
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
    m_fitPanel = new FitPanel;
    stackLayout->addWidget(m_fitPanel);
    m_fitPanel->hide();
    m_embossPanel = new EmbossPanel;
    stackLayout->addWidget(m_embossPanel);
    m_embossPanel->hide();
    m_edgesPanel = new EdgesPanel;
    stackLayout->addWidget(m_edgesPanel);
    m_edgesPanel->hide();
    m_sketchPanel = new SketchPanel;
    stackLayout->addWidget(m_sketchPanel);
    m_sketchPanel->hide();
    connect(m_sketchPanel, &SketchPanel::finishRequested, this, &MainWindow::finishSketch);
    connect(m_sketchPanel, &SketchPanel::outlineRequested, this, &MainWindow::loftStep);
    connect(m_sketchPanel, &SketchPanel::pathRequested, this, &MainWindow::sweepStage);
    connect(m_sketchPanel, &SketchPanel::cancelRequested, this, &MainWindow::endSketch);
    connect(m_sketchView, &SketchView::changed, this, &MainWindow::updateSketchStatus);
    connect(m_sketchView, &SketchView::message, this, [this](const QString& text, bool problem) {
        m_sketchPanel->setMessage(text, problem);
    });
    connect(m_sketchView, &SketchView::toolChanged, this, [this](SketchView::Tool tool) {
        const auto i = std::size_t(tool);
        if (i < m_sketchToolActions.size())
            m_sketchToolActions[i]->setChecked(true);
    });
    // Long hint texts wrap instead of widening the dock.
    m_propsStack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    // A tall panel (Properties of a big shape, Emboss with a picture) scrolls rather
    // than making the window taller than a laptop screen.
    auto* propsScroll = new QScrollArea;
    propsScroll->setWidget(m_propsStack);
    propsScroll->setWidgetResizable(true);
    propsScroll->setFrameShape(QFrame::NoFrame);
    propsScroll->setMinimumWidth(300);
    propsDock->setWidget(propsScroll);
    m_propsDock = propsDock;
    connect(m_splitPanel, &SplitPanel::planeChanged, this, &MainWindow::updateSplitPlane);
    connect(m_splitPanel, &SplitPanel::applyRequested, this, &MainWindow::applySplit);
    connect(m_splitPanel, &SplitPanel::cancelRequested, this, &MainWindow::endSplit);
    connect(m_splitPanel, &SplitPanel::keysChanged, this,
            [this] { saveKeyOptions(m_splitPanel->keySettings()); });
    connect(m_splitPanel, &SplitPanel::smartRequested, this, [this] {
        const ObjectId target = m_splitMode.target;
        endSplit();
        // Its questions open once this click is done with (the panel was just put away).
        QTimer::singleShot(0, this, [this, target] { splitToFit(target); });
    });
    connect(m_fitPanel, &FitPanel::cutSelected, this, [this](int index) {
        m_fitMode.selected = index;
        refreshScene();
    });
    connect(m_fitPanel, &FitPanel::cutEdited, this, [this](int index, FitCut cut) {
        if (!m_fitMode.active || index < 0 || index >= int(m_fitMode.plan.cuts.size()))
            return;
        m_fitMode.plan.cuts[std::size_t(index)] = cut;
        m_fitMode.selected = index;
        updateFit();
    });
    connect(m_fitPanel, &FitPanel::angledChanged, this, [this](bool angled) {
        Preferences().setValue(QStringLiteral("fit/angled"), angled);
        planFit(angled);
    });
    connect(m_fitPanel, &FitPanel::layOutChanged, this,
            [](bool on) { Preferences().setValue(QStringLiteral("fit/layOut"), on); });
    connect(m_fitPanel, &FitPanel::replanRequested, this, [this] { planFit(m_fitPanel->angled()); });
    connect(m_fitPanel, &FitPanel::applyRequested, this, [this] {
        // After the click is over: the panel is put away and a question may open.
        QTimer::singleShot(0, this, &MainWindow::applyFit);
    });
    connect(m_fitPanel, &FitPanel::cancelRequested, this, &MainWindow::endFit);
    m_embossTimer = new QTimer(this);
    m_embossTimer->setSingleShot(true);
    connect(m_embossTimer, &QTimer::timeout, this, &MainWindow::updateEmbossPreview);
    connect(m_embossPanel, &EmbossPanel::changed, this, [this] { m_embossTimer->start(0); });
    connect(m_embossPanel, &EmbossPanel::typing, this, [this] { m_embossTimer->start(400); });
    connect(m_embossPanel, &EmbossPanel::applyRequested, this, &MainWindow::applyEmboss);
    connect(m_embossPanel, &EmbossPanel::cancelRequested, this, &MainWindow::endEmboss);
    connect(m_embossPanel, &EmbossPanel::pictureRequested, this, &MainWindow::choosePicture);
    connect(m_embossPanel, &EmbossPanel::centerRequested, this, [this](int how) {
        if (!m_embossMode.active)
            return;
        if (!m_embossMode.spot) {
            statusBar()->showMessage(tr("Click on the shape first, to say which face."), 5000);
            return;
        }
        std::optional<emboss::SurfacePoint> moved;
        try {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            const emboss::Center c = how == 0   ? emboss::Center::Both
                                     : how == 1 ? emboss::Center::TopBottom
                                                : emboss::Center::LeftRight;
            moved = m_embossPanel->usingPicture()
                        ? centeredSpot(m_doc, m_embossMode.target, *m_embossMode.spot, m_embossPanel->picture(), c)
                        : centeredSpot(m_doc, m_embossMode.target, *m_embossMode.spot, m_embossPanel->words(), c);
            QApplication::restoreOverrideCursor();
        } catch (const std::exception& e) {
            QApplication::restoreOverrideCursor();
            qWarning() << "Centering the words failed:" << e.what();
        }
        if (!moved) {
            statusBar()->showMessage(tr("The middle of that face isn't on the face (it may be hollow there)."), 6000);
            return;
        }
        m_embossMode.spot = moved;
        updateEmbossPreview();
    });
    m_edgesTimer = new QTimer(this);
    m_edgesTimer->setSingleShot(true);
    connect(m_edgesTimer, &QTimer::timeout, this, &MainWindow::updateEdgesPreview);
    connect(m_edgesPanel, &EdgesPanel::kindChanged, this, &MainWindow::edgesKindChanged);
    connect(m_edgesPanel, &EdgesPanel::sizeChanged, this, [this] { m_edgesTimer->start(0); });
    connect(m_edgesPanel, &EdgesPanel::allRequested, this, &MainWindow::pickAllEdges);
    connect(m_edgesPanel, &EdgesPanel::clearRequested, this, [this] {
        m_edgesMode.picks.clear();
        rebuildEdgeHighlights();
        m_edgesTimer->start(0);
    });
    connect(m_edgesPanel, &EdgesPanel::applyRequested, this, &MainWindow::applyEdges);
    connect(m_edgesPanel, &EdgesPanel::cancelRequested, this, &MainWindow::endEdges);

    m_holePanel = new HolePanel;
    stackLayout->addWidget(m_holePanel);
    m_holePanel->hide();
    m_holeTimer = new QTimer(this);
    m_holeTimer->setSingleShot(true);
    m_holeTimer->setInterval(150);
    connect(m_holeTimer, &QTimer::timeout, this, &MainWindow::updateHolePreview);
    connect(m_holePanel, &HolePanel::changed, this, [this] { m_holeTimer->start(); });
    connect(m_holePanel, &HolePanel::spotEdited, this, [this](int index, double x, double y) {
        const auto spots = sketch::loosePoints(m_holeMode.spots);
        if (index < 0 || std::size_t(index) >= spots.size())
            return;
        m_holeMode.spots.points[std::size_t(spots[std::size_t(index)])] = {x, y};
        // Typed: rules that held it elsewhere would fight, so they go.
        std::vector<int> rules;
        for (std::size_t i = 0; i < m_holeMode.spots.constraints.size(); ++i) {
            const auto& c = m_holeMode.spots.constraints[i];
            if (c.a == spots[std::size_t(index)] || c.b == spots[std::size_t(index)])
                rules.push_back(int(i));
        }
        sketch::removeConstraints(m_holeMode.spots, rules);
        m_holeTimer->start();
    });
    connect(m_holePanel, &HolePanel::removeSpotRequested, this, [this](int index) {
        const auto spots = sketch::loosePoints(m_holeMode.spots);
        if (index < 0 || std::size_t(index) >= spots.size())
            return;
        sketch::removePoints(m_holeMode.spots, {spots[std::size_t(index)]});
        showHoleSpots();
        m_holeTimer->start(0);
    });
    connect(m_holePanel, &HolePanel::sketchRequested, this, [this] {
        if (!m_holeMode.active || !m_holeMode.placed)
            return;
        std::vector<std::vector<QPointF>> reference;
        try {
            for (const auto& line : sketch::outlinesOnPlane(m_holeMode.shape, m_holeMode.frame)) {
                std::vector<QPointF> q;
                for (const auto& p : line)
                    q.emplace_back(p.x, p.y);
                reference.push_back(std::move(q));
            }
        } catch (const std::exception& e) {
            qWarning() << "Finding the face's outline failed:" << e.what();
        }
        m_holeMode.positioning = true;
        m_sketchMode = SketchMode{};
        m_sketchMode.frame = m_holeMode.frame;
        openSketch(m_holeMode.spots, std::move(reference), tr("Place the holes exactly"), false,
                   SketchPanel::Options{});
    });
    connect(m_holePanel, &HolePanel::applyRequested, this, &MainWindow::applyHole);
    connect(m_holePanel, &HolePanel::cancelRequested, this, &MainWindow::endHole);

    m_shellPanel = new ShellPanel;
    stackLayout->addWidget(m_shellPanel);
    m_shellPanel->hide();
    m_sculptPanel = new SculptPanel;
    stackLayout->addWidget(m_sculptPanel);
    m_sculptPanel->hide();
    m_threadPanel = new ThreadPanel;
    stackLayout->addWidget(m_threadPanel);
    m_threadPanel->hide();
    m_socketPanel = new SocketPanel;
    stackLayout->addWidget(m_socketPanel);
    m_socketPanel->hide();
    m_hingePanel = new HingePanel;
    stackLayout->addWidget(m_hingePanel);
    m_hingePanel->hide();
    m_posePanel = new PosePanel;
    stackLayout->addWidget(m_posePanel);
    m_posePanel->hide();
    m_printCheckPanel = new PrintCheckPanel;
    stackLayout->addWidget(m_printCheckPanel);
    m_printCheckPanel->hide();
    connect(m_printCheckPanel, &PrintCheckPanel::settingsChanged, this, &MainWindow::colourPrintCheck);
    connect(m_printCheckPanel, &PrintCheckPanel::doneRequested, this, &MainWindow::endPrintCheck);
    m_poseTimer = new QTimer(this);
    m_poseTimer->setSingleShot(true);
    m_poseTimer->setInterval(200);
    connect(m_poseTimer, &QTimer::timeout, this, &MainWindow::checkPoseHits);
    connect(m_posePanel, &PosePanel::jointChosen, this, &MainWindow::choosePoseJoint);
    connect(m_posePanel, &PosePanel::anglesEdited, this, [this](bool dragging) {
        auto& mode = m_poseMode;
        if (!mode.active || mode.joint < 0)
            return;
        const pose::Joint& j = mode.joints[std::size_t(mode.joint)];
        auto a = m_posePanel->angles();
        if (j.clicks > 0 && !dragging) {
            a[0] = pose::nearestClick(j, a[0]);
            m_posePanel->showJoint(mode.joint, j.kind, a);
        }
        mode.angles[j.step] = a;
        mode.savedPose = -1;
        posed();
        if (dragging)
            m_poseTimer->start();  // checked when it stops moving
    });
    connect(m_posePanel, &PosePanel::rangeRequested, this, &MainWindow::findPoseRange);
    connect(m_posePanel, &PosePanel::makeRoomRequested, this, &MainWindow::makePoseRoom);
    connect(m_posePanel, &PosePanel::straightenRequested, this, [this] {
        auto& mode = m_poseMode;
        if (!mode.active || mode.joint < 0)
            return;
        mode.angles.erase(mode.joints[std::size_t(mode.joint)].step);
        choosePoseJoint(mode.joint);
        posed();
    });
    connect(m_posePanel, &PosePanel::buildPoseRequested, this, [this] {
        if (!m_poseMode.active)
            return;
        m_poseMode.angles.clear();
        m_poseMode.savedPose = -1;
        refreshSavedPoses();
        choosePoseJoint(m_poseMode.joint);
        posed();
    });
    connect(m_posePanel, &PosePanel::holdRequested, this, [this] {
        const ObjectId id = selectedOne();
        if (!m_poseMode.active || id == kInvalidId || !m_doc.isTopLevel(id)) {
            m_posePanel->setStatus(tr("Click the part to hold still first."), true);
            return;
        }
        // Held from somewhere else, the same pose is a different one: start straight.
        m_poseMode.angles.clear();
        rebuildPose(id);
    });
    connect(m_posePanel, &PosePanel::poseChosen, this, [this](int index) {
        if (!m_poseMode.active || index < 0 || index >= int(m_doc.poses().size()))
            return;
        const SavedPose& p = m_doc.poses()[std::size_t(index)];
        m_poseMode.angles = p.angles;
        m_poseMode.savedPose = index;
        rebuildPose(p.hold);
    });
    connect(m_posePanel, &PosePanel::savePoseRequested, this, [this] {
        if (!m_poseMode.active)
            return;
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("Save Pose"), tr("Name for this pose:"), QLineEdit::Normal,
                                  tr("Pose %1").arg(m_doc.poses().size() + 1), &ok)
                .trimmed();
        if (!ok || name.isEmpty())
            return;
        auto poses = m_doc.poses();
        SavedPose p{name.toStdString(), m_poseMode.rig.hold, m_poseMode.angles};
        const auto same = std::find_if(poses.begin(), poses.end(), [&p](const SavedPose& q) { return q.name == p.name; });
        if (same != poses.end()) {
            if (QMessageBox::question(this, tr("Save Pose"), tr("There's already a pose called \"%1\". Replace it?").arg(name)) !=
                QMessageBox::Yes)
                return;
            *same = p;
        } else {
            poses.push_back(p);
        }
        m_poseMode.savedPose = int(std::distance(poses.begin(), std::find_if(poses.begin(), poses.end(),
                                                [&p](const SavedPose& q) { return q.name == p.name; })));
        m_stack.execute(std::make_unique<SetPosesCommand>(std::move(poses), "Save Pose"));
        m_posePanel->setStatus(tr("Saved \"%1\" (in the file).").arg(name), false);
    });
    connect(m_posePanel, &PosePanel::deletePoseRequested, this, [this] {
        const int index = m_poseMode.savedPose >= 0 ? m_poseMode.savedPose : -1;
        auto poses = m_doc.poses();
        if (!m_poseMode.active || index < 0 || index >= int(poses.size())) {
            m_posePanel->setStatus(tr("Choose a saved pose to delete."), true);
            return;
        }
        poses.erase(poses.begin() + index);
        m_poseMode.savedPose = -1;
        m_stack.execute(std::make_unique<SetPosesCommand>(std::move(poses), "Delete Pose"));
    });
    connect(m_posePanel, &PosePanel::doneRequested, this, &MainWindow::endPose);
    m_doc.addPoseListener([this] {
        if (m_poseMode.active)
            refreshSavedPoses();
    });
    m_joinPanel = new JoinPanel;
    stackLayout->addWidget(m_joinPanel);
    m_joinPanel->hide();
    m_maskPanel = new MaskPanel;
    stackLayout->addWidget(m_maskPanel);
    m_maskPanel->hide();
    m_referencePanel = new ReferencePanel;
    stackLayout->addWidget(m_referencePanel);
    m_referencePanel->hide();
    m_mannequinPanel = new MannequinPanel;
    stackLayout->addWidget(m_mannequinPanel);
    m_mannequinPanel->hide();
    connect(m_mannequinPanel, &MannequinPanel::addRequested, this, [this] {
        Mannequin m = m_mannequinPanel->fresh();
        // Beside what's on the plate, so it doesn't stand inside the model.
        const Bounds all = m_doc.worldBounds();
        if (all.valid)
            m.x = all.max[0] + 0.25 * m.height;
        if (run(std::make_unique<SetMannequinCommand>(m, "Add Figure Guide"))) {
            refreshScene();
            m_viewport->fitView();  // it's usually taller than the model: show all of it
        }
    });
    connect(m_mannequinPanel, &MannequinPanel::removeRequested, this,
            [this] { run(std::make_unique<SetMannequinCommand>(std::nullopt, "Remove Figure Guide")); });
    connect(m_mannequinPanel, &MannequinPanel::changed, this, [this](const Mannequin& m, const QString& label) {
        try {
            m_stack.execute(std::make_unique<SetMannequinCommand>(m, label.toStdString(), /*mergeable=*/true));
            m_mannequinPanel->setStatus({}, false);
        } catch (const std::exception& e) {
            m_mannequinPanel->setStatus(QString::fromUtf8(e.what()), true);
        }
    });
    connect(m_mannequinPanel, &MannequinPanel::doneRequested, this, &MainWindow::endMannequin);
    m_doc.addMannequinListener([this] {
        const auto& m = m_doc.mannequin();
        m_mannequinMesh = m && m->shown ? std::make_shared<const Mesh>(mannequin::mesh(*m)) : nullptr;
        if (m_mannequinOpen)
            m_mannequinPanel->setMannequin(m);
        scheduleSceneRefresh();
    });
    connect(m_referencePanel, &ReferencePanel::directionChanged, this, [this] {
        if (m_referenceMode.picking) {
            m_referenceMode.picking = 0;
            m_referencePanel->setPicking(0);
            m_viewport->setLabels({});
        }
        m_referencePanel->setStatus({}, false);
        refreshReferences();
    });
    connect(m_referencePanel, &ReferencePanel::addRequested, this, &MainWindow::addReferencePicture);
    connect(m_referencePanel, &ReferencePanel::removeRequested, this, &MainWindow::removeReferencePicture);
    connect(m_referencePanel, &ReferencePanel::lookRequested, this, [this] {
        const auto f = reference::frameOf(m_referencePanel->direction());
        m_viewport->turnTo(QVector3D(float(f.toward[0]), float(f.toward[1]), float(f.toward[2])));
    });
    connect(m_referencePanel, &ReferencePanel::edited, this, &MainWindow::referenceEdited);
    connect(m_referencePanel, &ReferencePanel::pickRequested, this, [this] {
        if (m_referenceMode.picking) {
            m_referenceMode.picking = 0;
            m_referencePanel->setPicking(0);
            m_referencePanel->setStatus({}, false);
            m_viewport->setLabels({});
            return;
        }
        const ReferenceImage* image = reference::find(m_doc.references(), m_referencePanel->direction());
        if (!image)
            return;
        if (!image->shown || !m_showPicturesAction->isChecked()) {
            m_referencePanel->setStatus(tr("Show the picture first, to click on it."), true);
            return;
        }
        m_referenceMode.picking = 1;
        m_referencePanel->setPicking(1);
    });
    connect(m_referencePanel, &ReferencePanel::doneRequested, this, &MainWindow::endReferences);
    m_doc.addReferenceListener([this] { refreshReferences(); });
    m_measurePanel = new MeasurePanel;
    stackLayout->addWidget(m_measurePanel);
    m_measurePanel->hide();
    connect(m_measurePanel, &MeasurePanel::clearRequested, this, [this] {
        m_measureMode.picks.clear();
        showMeasurements();
    });
    connect(m_measurePanel, &MeasurePanel::doneRequested, this, &MainWindow::endMeasure);
    connect(m_measurePanel, &MeasurePanel::keepRequested, this, [this] {
        if (m_measureLabels.empty()) {
            statusBar()->showMessage(tr("Pick two things first: then their distance can be kept on screen."), 6000);
            return;
        }
        m_keptLabels.insert(m_keptLabels.end(), m_measureLabels.begin(), m_measureLabels.end());
        // Ready for the next one.
        m_measureMode.picks.clear();
        m_measureMode.hover.reset();
        showMeasurements();
        statusBar()->showMessage(tr("Kept. Measure something else, or Done."), 6000);
    });
    connect(m_measurePanel, &MeasurePanel::removeKeptRequested, this, [this] {
        m_keptLabels.clear();
        showLabels();
    });
    connect(m_sculptPanel, &SculptPanel::detailChanged, this, &MainWindow::remeshSculpt);
    connect(m_sculptPanel, &SculptPanel::addDetailChanged, this, [this] {
        if (m_sculptMode.sculptor)
            m_sculptMode.sculptor->setDetail(m_sculptPanel->addDetail() ? m_sculptPanel->finestDetail() : 0.0);
    });
    connect(m_sculptPanel, &SculptPanel::undoRequested, this, &MainWindow::undoStroke);
    connect(m_sculptPanel, &SculptPanel::remeshRequested, this, &MainWindow::evenOutSculpt);
    connect(m_sculptPanel, &SculptPanel::ownPatternRequested, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Choose a picture to use as a pattern"), startFolder(),
            tr("Pictures (*.png *.jpg *.jpeg *.bmp *.gif *.svg);;All files (*)"));
        if (path.isEmpty())
            return;
        rememberFolderOf(path);
        const auto image = loadPictureFile(path);
        if (!image)
            return;
        SculptPattern own;
        own.name = QFileInfo(path).fileName().toStdString();
        own.builtIn = false;
        own.image = image;
        m_sculptPanel->addOwnPattern(own);
    });
    connect(m_sculptPanel, &SculptPanel::clearMaskRequested, this,
            [this] { wholeMask(int(sculpt::Brush::MaskClear)); });
    connect(m_sculptPanel, &SculptPanel::invertMaskRequested, this,
            [this] { wholeMask(int(sculpt::Brush::MaskInvert)); });
    connect(m_sculptPanel, &SculptPanel::doneRequested, this, &MainWindow::finishSculpt);
    connect(m_sculptPanel, &SculptPanel::cancelRequested, this, &MainWindow::endSculpt);
    m_shellTimer = new QTimer(this);
    m_shellTimer->setSingleShot(true);
    m_shellTimer->setInterval(150);
    connect(m_shellTimer, &QTimer::timeout, this, &MainWindow::updateShellPreview);
    connect(m_shellPanel, &ShellPanel::changed, this, [this] { m_shellTimer->start(); });
    connect(m_shellPanel, &ShellPanel::clearRequested, this, [this] {
        m_shellMode.picks.clear();
        rebuildShellHighlights();
        m_shellTimer->start(0);
    });
    m_threadTimer = new QTimer(this);
    m_threadTimer->setSingleShot(true);
    m_threadTimer->setInterval(250);
    connect(m_threadTimer, &QTimer::timeout, this, &MainWindow::updateThreadPreview);
    connect(m_threadPanel, &ThreadPanel::changed, this, [this] { m_threadTimer->start(); });
    connect(m_threadPanel, &ThreadPanel::applyRequested, this, &MainWindow::applyThread);
    connect(m_threadPanel, &ThreadPanel::cancelRequested, this, &MainWindow::endThread);
    m_panelLinePanel = new PanelLinePanel;
    stackLayout->addWidget(m_panelLinePanel);
    m_panelLinePanel->hide();
    connect(m_panelLinePanel, &PanelLinePanel::changed, this, [this] {
        // A wider groove follows the surface in bigger steps: the line being drawn is
        // laid again between its clicks.
        auto& mode = m_panelLineMode;
        for (std::size_t i = 0; i < mode.pieces.size(); ++i)
            mode.pieces[i] = panelPiece(mode.anchors[i], mode.anchors[i + 1], mode.planes[i]);
        updatePanelLinePreview();
    });
    connect(m_panelLinePanel, &PanelLinePanel::endLineRequested, this, [this] { endPanelLine(false); });
    connect(m_panelLinePanel, &PanelLinePanel::closeLoopRequested, this, [this] { endPanelLine(true); });
    connect(m_panelLinePanel, &PanelLinePanel::undoPointRequested, this, [this] {
        auto& mode = m_panelLineMode;
        if (!mode.anchors.empty())
            mode.anchors.pop_back();
        if (!mode.pieces.empty()) {
            mode.pieces.pop_back();
            mode.planes.pop_back();
        }
        updatePanelLinePreview();
    });
    connect(m_panelLinePanel, &PanelLinePanel::removeLineRequested, this, [this] {
        auto& mode = m_panelLineMode;
        if (!mode.anchors.empty()) {
            mode.anchors.clear();
            mode.pieces.clear();
            mode.planes.clear();
        } else if (!mode.lines.empty()) {
            mode.lines.pop_back();
        }
        updatePanelLinePreview();
    });
    connect(m_panelLinePanel, &PanelLinePanel::clearRequested, this, [this] {
        m_panelLineMode.lines.clear();
        m_panelLineMode.anchors.clear();
        m_panelLineMode.pieces.clear();
        m_panelLineMode.planes.clear();
        updatePanelLinePreview();
    });
    connect(m_panelLinePanel, &PanelLinePanel::applyRequested, this, &MainWindow::applyPanelLines);
    connect(m_panelLinePanel, &PanelLinePanel::cancelRequested, this, &MainWindow::endPanelLines);
    m_stampPanel = new StampPanel;
    stackLayout->addWidget(m_stampPanel);
    m_stampPanel->hide();
    connect(m_stampPanel, &StampPanel::changed, this, &MainWindow::updateStampPreview);
    connect(m_stampPanel, &StampPanel::kindChanged, this, &MainWindow::loadStampShape);
    connect(m_stampPanel, &StampPanel::removeLastRequested, this, [this] {
        auto& mode = m_stampMode;
        if (!mode.groups.empty()) {
            mode.spots.resize(mode.groups.back());
            mode.groups.pop_back();
        } else if (!mode.spots.empty()) {
            mode.spots.pop_back();  // changing a step: one at a time
        }
        updateStampPreview();
    });
    connect(m_stampPanel, &StampPanel::clearRequested, this, [this] {
        m_stampMode.spots.clear();
        m_stampMode.groups.clear();
        updateStampPreview();
    });
    connect(m_stampPanel, &StampPanel::applyRequested, this, &MainWindow::applyStamp);
    connect(m_stampPanel, &StampPanel::cancelRequested, this, &MainWindow::endStamp);
    m_cutPartsPanel = new CutPartsPanel;
    stackLayout->addWidget(m_cutPartsPanel);
    m_cutPartsPanel->hide();
    connect(m_cutPartsPanel, &CutPartsPanel::changed, this, [this] { updateCutPartsPreview(false); });
    connect(m_cutPartsPanel, &CutPartsPanel::fitAgainRequested, this, [this] {
        if (!m_cutPartsMode.active)
            return;
        m_cutPartsMode.points = m_cutPartsMode.fitted;
        updateCutPartsPreview(true);
    });
    connect(m_cutPartsPanel, &CutPartsPanel::applyRequested, this, &MainWindow::applyCutParts);
    connect(m_cutPartsPanel, &CutPartsPanel::cancelRequested, this, &MainWindow::endCutParts);
    m_paintPanel = new PaintPanel;
    stackLayout->addWidget(m_paintPanel);
    m_paintPanel->hide();
    connect(m_paintPanel, &PaintPanel::fillRequested, this, [this] {
        auto& mode = m_paintMode;
        if (!mode.active || !mode.shown)
            return;
        // One dab big enough for the whole surface, as a stroke of its own.
        const Bounds b = mode.shown->bounds();
        const auto c = b.center();
        mode.strokeStarts.push_back(mode.dabs.size());
        const QColor colour = m_paintPanel->colour();
        mode.dabs.push_back({mode.toPlate.apply(Vec3{c[0], c[1], c[2]}), double(b.radius()) * 1.5 + 1.0,
                             std::uint32_t(colour.rgb() & 0xFFFFFFu), false});
        repaintPaint();
    });
    connect(m_paintPanel, &PaintPanel::undoStrokeRequested, this, &MainWindow::undoPaintStroke);
    connect(m_paintPanel, &PaintPanel::clearRequested, this, [this] {
        if (!m_paintMode.active)
            return;
        m_paintMode.dabs.clear();
        m_paintMode.strokeStarts.clear();
        repaintPaint();
    });
    connect(m_paintPanel, &PaintPanel::brushSizeChanged, this, [this] {
        if (m_paintMode.active)
            m_viewport->setBrushRing(std::nullopt);  // shown again at the new size when the mouse moves
    });
    connect(m_paintPanel, &PaintPanel::applyRequested, this, &MainWindow::applyPaint);
    connect(m_paintPanel, &PaintPanel::cancelRequested, this, &MainWindow::endPaint);
    connect(m_maskPanel, &MaskPanel::paintColourChosen, this, [this](std::uint32_t colour) {
        auto& mode = m_maskMode;
        if (!mode.active)
            return;
        const ObjectId object = history::objectOf(m_doc, mode.part);
        const auto painted = paint::moved(paint::objectPaint(m_doc, object), m_doc.worldTransform(object));
        mode.dabs = paint::maskFor(painted, colour);
        mode.strokeStarts.clear();
        if (!mode.dabs.empty())
            mode.strokeStarts.push_back(0);
        repaintMask();
    });
    m_poseFigurePanel = new PoseFigurePanel;
    stackLayout->addWidget(m_poseFigurePanel);
    m_poseFigurePanel->hide();
    connect(m_poseFigurePanel, &PoseFigurePanel::stageChanged, this, [this](bool) { updatePoseFigurePreview(); });
    connect(m_poseFigurePanel, &PoseFigurePanel::fitAgainRequested, this, [this] {
        auto& mode = m_poseFigureMode;
        if (!mode.active)
            return;
        mode.finish.points = mode.fitted.points;
        mode.finish.blend = mode.fitted.blend;
        if (!mode.posedYet)
            mode.finish.angles = rig::restAngles(mode.finish.points);
        updatePoseFigurePreview();
    });
    connect(m_poseFigurePanel, &PoseFigurePanel::optionsChanged, this, [this] {
        auto& mode = m_poseFigureMode;
        if (!mode.active)
            return;
        mode.finish.blend = m_poseFigurePanel->blend();
        mode.finish.feetFlat = m_poseFigurePanel->feetFlat();
        mode.finish.onGround = m_poseFigurePanel->onGround();
        updatePoseFigurePreview();
    });
    connect(m_poseFigurePanel, &PoseFigurePanel::presetChosen, this, [this](int index) {
        auto& mode = m_poseFigureMode;
        if (!mode.active)
            return;
        const rig::Angles rest = rig::restAngles(mode.finish.points);
        if (index == PoseFigurePanel::kAsMade) {
            mode.finish.angles = rest;
            mode.posedYet = false;
        } else if (index <= mannequin::kPosePresetCount) {
            mode.finish.angles = rig::fromMannequin(mannequin::preset(mannequin::PosePreset(index - 1)));
            mode.posedYet = true;
        } else if (m_doc.mannequin()) {
            mode.finish.angles = rig::fromMannequin(m_doc.mannequin()->pose);
            mode.posedYet = true;
        }
        // Feet flat: the ankles keep the angle they were made at.
        for (rig::Joint j : {rig::Joint::LeftAnkle, rig::Joint::RightAnkle})
            mode.finish.angles[std::size_t(j)] = rest[std::size_t(j)];
        updatePoseFigurePreview();
    });
    connect(m_poseFigurePanel, &PoseFigurePanel::angleChanged, this, [this](int joint, int which, double degrees) {
        auto& mode = m_poseFigureMode;
        if (!mode.active || joint < 0 || joint >= kRigJointCount)
            return;
        mode.finish.angles[std::size_t(joint)][std::size_t(which)] = degrees;
        mode.posedYet = true;
        updatePoseFigurePreview();
    });
    connect(m_poseFigurePanel, &PoseFigurePanel::jointChosen, this, [this] { updatePoseFigurePreview(); });
    connect(m_poseFigurePanel, &PoseFigurePanel::applyRequested, this, &MainWindow::applyPoseFigure);
    connect(m_poseFigurePanel, &PoseFigurePanel::cancelRequested, this, &MainWindow::endPoseFigure);
    m_hollowPanel = new HollowPanel;
    stackLayout->addWidget(m_hollowPanel);
    m_hollowPanel->hide();
    connect(m_hollowPanel, &HollowPanel::changed, this, &MainWindow::updateHollowPreview);
    m_handPanel = new HandPanel;
    stackLayout->addWidget(m_handPanel);
    m_handPanel->hide();
    m_handTimer = new QTimer(this);
    m_handTimer->setSingleShot(true);
    m_handTimer->setInterval(200);
    connect(m_handTimer, &QTimer::timeout, this, &MainWindow::updateHandPreview);
    connect(m_handPanel, &HandPanel::changed, this, [this] { m_handTimer->start(); });
    connect(m_handPanel, &HandPanel::applyRequested, this, &MainWindow::applyHand);
    connect(m_handPanel, &HandPanel::cancelRequested, this, &MainWindow::endHand);
    connect(m_handPanel->standardRow(), &StandardRow::chosen, this, [this](int index) {
        if (index >= 0 && index < int(m_standardPorts.size()) && m_standardPorts[std::size_t(index)].socket)
            m_handPanel->setWrist(*m_standardPorts[std::size_t(index)].socket);
    });
    connect(m_handPanel->standardRow(), &StandardRow::saveRequested, this, [this] {
        if (const auto w = m_handPanel->wrist())
            saveToStandard(false, &*w);
    });
    m_portPanel = new PortPanel;
    stackLayout->addWidget(m_portPanel);
    m_portPanel->hide();
    connect(m_portPanel, &PortPanel::changed, this, &MainWindow::updatePortPreview);
    connect(m_portPanel, &PortPanel::straightRequested, this, [this] {
        m_portMode.exit.reset();
        updatePortPreview();
    });
    connect(m_portPanel, &PortPanel::applyRequested, this, &MainWindow::applyPort);
    connect(m_portPanel, &PortPanel::cancelRequested, this, &MainWindow::endPort);
    connect(m_portPanel->standardRow(), &StandardRow::chosen, this, [this](int index) {
        if (index < 0 || index >= int(m_standardPorts.size()) || !m_standardPorts[std::size_t(index)].socket)
            return;
        m_portPanel->setFinish(port::fromSocket(*m_standardPorts[std::size_t(index)].socket, m_portPanel->finish()));
    });
    connect(m_portPanel->standardRow(), &StandardRow::saveRequested, this, [this] {
        const SocketFinish joint = port::jointFor(m_portPanel->finish(), m_portPanel->resin());
        saveToStandard(false, &joint);
    });
    m_mouldPanel = new MouldPanel;
    stackLayout->addWidget(m_mouldPanel);
    m_mouldPanel->hide();
    connect(m_mouldPanel, &MouldPanel::changed, this, &MainWindow::updateMouldPreview);
    connect(m_mouldPanel, &MouldPanel::widestRequested, this, [this] {
        if (m_mouldMode.active)
            m_mouldPanel->setParting(mould::widest(m_mouldMode.part, m_mouldPanel->settings().axis));
    });
    connect(m_mouldPanel, &MouldPanel::directionChanged, this, [this] {
        if (!m_mouldMode.active)
            return;
        m_mouldPanel->setParting(mould::widest(m_mouldMode.part, m_mouldPanel->settings().axis));
        updateMouldPreview();
    });
    connect(m_mouldPanel, &MouldPanel::bestRequested, this, [this] {
        if (!m_mouldMode.active)
            return;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const MouldFinish best = mould::bestSplit(m_mouldMode.part, mouldSettings());
        QApplication::restoreOverrideCursor();
        m_mouldPanel->setSplit(best.axis, best.parting);
    });
    connect(m_mouldPanel, &MouldPanel::placeVentsRequested, this, [this] {
        auto& mode = m_mouldMode;
        if (!mode.active)
            return;
        const MouldFinish f = mouldSettings();
        mode.vents.clear();
        const double above = f.axis == 2 ? f.parting : -std::numeric_limits<double>::infinity();
        for (const Vec3& v : mould::highSpots(mode.part, above, mode.spout, f.ventSize))
            mode.vents.push_back(v);
        updateMouldPreview();
    });
    connect(m_mouldPanel, &MouldPanel::removeLastRequested, this, [this] {
        if (!m_mouldMode.vents.empty())
            m_mouldMode.vents.pop_back();
        updateMouldPreview();
    });
    connect(m_mouldPanel, &MouldPanel::clearRequested, this, [this] {
        m_mouldMode.vents.clear();
        updateMouldPreview();
    });
    connect(m_mouldPanel, &MouldPanel::applyRequested, this, &MainWindow::applyMould);
    connect(m_mouldPanel, &MouldPanel::cancelRequested, this, &MainWindow::endMould);
    connect(m_hollowPanel, &HollowPanel::placeRequested, this, [this] {
        auto& mode = m_hollowMode;
        if (!mode.active)
            return;
        try {
            const Body* step = m_doc.find(mode.target);
            const ObjectId part = mode.editing && step && step->isBoolean() ? step->boolean->operands.front()
                                                                            : mode.target;
            Mesh plate = *m_doc.mesh(part);
            const Transform t = m_doc.worldTransform(part);
            for (std::size_t i = 0; i < plate.vertexCount(); ++i) {
                const Vec3 p = t.apply(Vec3{plate.positions[3 * i], plate.positions[3 * i + 1], plate.positions[3 * i + 2]});
                for (int k = 0; k < 3; ++k)
                    plate.positions[3 * i + std::size_t(k)] = float(p[std::size_t(k)]);
            }
            mode.holes = hollow::lowSpots(plate, m_hollowPanel->holeSize(), 2);
        } catch (const std::exception& e) {
            m_hollowPanel->setStatus(QString::fromUtf8(e.what()), true);
            return;
        }
        updateHollowPreview();
    });
    connect(m_hollowPanel, &HollowPanel::removeLastRequested, this, [this] {
        if (!m_hollowMode.holes.empty())
            m_hollowMode.holes.pop_back();
        updateHollowPreview();
    });
    connect(m_hollowPanel, &HollowPanel::clearRequested, this, [this] {
        m_hollowMode.holes.clear();
        updateHollowPreview();
    });
    connect(m_hollowPanel, &HollowPanel::applyRequested, this, &MainWindow::applyHollow);
    connect(m_hollowPanel, &HollowPanel::cancelRequested, this, &MainWindow::endHollow);
    m_socketTimer = new QTimer(this);
    m_socketTimer->setSingleShot(true);
    m_socketTimer->setInterval(250);
    connect(m_socketTimer, &QTimer::timeout, this, &MainWindow::updateSocketPreview);
    connect(m_socketPanel, &SocketPanel::changed, this, [this] { m_socketTimer->start(); });
    connect(m_socketPanel->standardRow(), &StandardRow::chosen, this, [this](int i) {
        if (i < 0 || i >= int(m_standardSockets.size()) || !m_standardSockets[std::size_t(i)].socket)
            return;
        m_socketPanel->setFinish(standards::applied(*m_standardSockets[std::size_t(i)].socket, m_socketPanel->finish()));
        m_socketTimer->start();
    });
    connect(m_socketPanel->standardRow(), &StandardRow::saveRequested, this, [this] { saveToStandard(false); });
    connect(m_socketPanel, &SocketPanel::applyRequested, this, &MainWindow::applySocket);
    connect(m_socketPanel, &SocketPanel::cancelRequested, this, &MainWindow::endSocket);
    connect(m_socketPanel, &SocketPanel::addMatchingRequested, this, &MainWindow::addMatchingPart);
    connect(m_socketPanel, &SocketPanel::pickOtherRequested, this, [this] {
        if (!m_socketMode.active)
            return;
        m_socketMode.pickingOther = true;
        m_socketPanel->setOtherPart({}, true);
        statusBar()->showMessage(tr("Click the part against this face. Esc stops."), 6000);
    });
    m_hingeTimer = new QTimer(this);
    m_hingeTimer->setSingleShot(true);
    m_hingeTimer->setInterval(250);
    connect(m_hingeTimer, &QTimer::timeout, this, &MainWindow::updateHingePreview);
    connect(m_hingePanel, &HingePanel::changed, this, [this] { m_hingeTimer->start(); });
    connect(m_hingePanel->standardRow(), &StandardRow::chosen, this, [this](int i) {
        if (i < 0 || i >= int(m_standardHinges.size()) || !m_standardHinges[std::size_t(i)].hinge)
            return;
        m_hingePanel->setFinish(standards::applied(*m_standardHinges[std::size_t(i)].hinge, m_hingePanel->finish()));
        m_hingeTimer->start();
    });
    connect(m_hingePanel->standardRow(), &StandardRow::saveRequested, this, [this] { saveToStandard(true); });
    connect(m_hingePanel, &HingePanel::applyRequested, this, &MainWindow::applyHinge);
    connect(m_hingePanel, &HingePanel::cancelRequested, this, &MainWindow::endHinge);
    connect(m_hingePanel, &HingePanel::addPiecesRequested, this, &MainWindow::addHingePieces);
    connect(m_hingePanel, &HingePanel::pickOtherRequested, this, [this] {
        if (!m_hingeMode.active)
            return;
        m_hingeMode.pickingOther = true;
        m_hingePanel->setOtherPart({}, true);
        statusBar()->showMessage(tr("Quick Hinge: click the other part. Esc stops."), 6000);
    });
    connect(m_hingePanel, &HingePanel::centerRequested, this, [this](int how) {
        if (!m_hingeMode.active)
            return;
        if (!m_hingeMode.spot) {
            statusBar()->showMessage(tr("Click on the shape first, to say which face."), 5000);
            return;
        }
        std::optional<emboss::SurfacePoint> moved;
        try {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            const emboss::Center c = how == 0   ? emboss::Center::Both
                                     : how == 1 ? emboss::Center::TopBottom
                                                : emboss::Center::LeftRight;
            const HingeFinish f = m_hingePanel->finish();
            moved = centeredSpot(m_doc, m_hingeMode.target, *m_hingeMode.spot, f.turn,
                                 std::max(0.25, std::max(f.width, f.barrel) / 2.0), c);
        } catch (const std::exception& e) {
            qWarning() << "Centering the hinge failed:" << e.what();
        }
        if (!moved) {
            statusBar()->showMessage(tr("The middle of that face isn't on the face (it may be hollow there)."), 6000);
            return;
        }
        m_hingeMode.spot = moved;
        updateHingePreview();
    });
    connect(m_socketPanel, &SocketPanel::centerRequested, this, [this](int how) {
        if (!m_socketMode.active)
            return;
        if (!m_socketMode.spot) {
            statusBar()->showMessage(tr("Click on the shape first, to say which face."), 5000);
            return;
        }
        std::optional<emboss::SurfacePoint> moved;
        try {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            const emboss::Center c = how == 0   ? emboss::Center::Both
                                     : how == 1 ? emboss::Center::TopBottom
                                                : emboss::Center::LeftRight;
            const SocketFinish f = m_socketPanel->finish();
            moved = centeredSpot(m_doc, m_socketMode.target, *m_socketMode.spot, f.turn,
                                 std::max(0.25, f.size / 2.0), c);
        } catch (const std::exception& e) {
            qWarning() << "Centering the socket failed:" << e.what();
        }
        if (!moved) {
            statusBar()->showMessage(tr("The middle of that face isn't on the face (it may be hollow there)."), 6000);
            return;
        }
        m_socketMode.spot = moved;
        updateSocketPreview();
    });
    connect(m_maskPanel, &MaskPanel::coloursChanged, this, [this] { refreshScene(); });
    connect(m_sculptPanel, &SculptPanel::brushChanged, this, [this] {
        if (m_sculptMode.active)
            refreshScene();
    });
    connect(m_maskPanel, &MaskPanel::undoStrokeRequested, this, &MainWindow::undoMaskStroke);
    connect(m_maskPanel, &MaskPanel::clearRequested, this, [this] {
        if (!m_maskMode.active)
            return;
        m_maskMode.dabs.clear();
        m_maskMode.strokeStarts.clear();
        repaintMask();
    });
    connect(m_maskPanel, &MaskPanel::applyRequested, this, &MainWindow::applyMask);
    connect(m_maskPanel, &MaskPanel::cancelRequested, this, &MainWindow::endMask);
    connect(m_joinPanel, &JoinPanel::changed, this, &MainWindow::updateJoinPreview);
    connect(m_joinPanel, &JoinPanel::applyRequested, this, &MainWindow::applyJoin);
    connect(m_joinPanel, &JoinPanel::cancelRequested, this, &MainWindow::endJoin);
    connect(m_joinPanel, &JoinPanel::pickAgainRequested, this, [this](int which) {
        if (!m_joinMode.active)
            return;
        m_joinMode.picking = which;
        m_joinPanel->setPicked(0, m_joinMode.movingFeature ? m_joinPanel->property("movingText").toString() : QString(),
                               which == 0);
        m_joinPanel->setPicked(1, m_joinMode.fixedFeature ? m_joinPanel->property("fixedText").toString() : QString(),
                               which == 1);
        statusBar()->showMessage(which == 0 ? tr("Join Parts: click the part that moves, where it joins.")
                                            : tr("Join Parts: click where it goes on the other part."));
    });
    connect(m_shellPanel, &ShellPanel::applyRequested, this, &MainWindow::applyShell);
    connect(m_shellPanel, &ShellPanel::cancelRequested, this, &MainWindow::endShell);

    m_patternPanel = new PatternPanel;
    stackLayout->addWidget(m_patternPanel);
    m_patternPanel->hide();
    m_patternTimer = new QTimer(this);
    m_patternTimer->setSingleShot(true);
    m_patternTimer->setInterval(200);
    connect(m_patternTimer, &QTimer::timeout, this, &MainWindow::updatePatternPreview);
    connect(m_patternPanel, &PatternPanel::changed, this, [this] { m_patternTimer->start(); });
    connect(m_patternPanel, &PatternPanel::repeatChanged, this, [this](bool feature) {
        // Copies of a hole sit closer together than copies of the whole shape.
        PatternFinish f = startingPattern(m_doc, m_patternMode.target, feature);
        const PatternFinish now = m_patternPanel->finish();
        f.kind = now.kind;
        m_patternPanel->setNumbers(f);
        m_patternTimer->start(0);
    });
    connect(m_patternPanel, &PatternPanel::applyRequested, this, &MainWindow::applyPattern);
    connect(m_patternPanel, &PatternPanel::cancelRequested, this, &MainWindow::endPattern);

    m_followPanel = new FollowPanel;
    stackLayout->addWidget(m_followPanel);
    m_followPanel->hide();
    m_followTimer = new QTimer(this);
    m_followTimer->setSingleShot(true);
    m_followTimer->setInterval(200);
    connect(m_followTimer, &QTimer::timeout, this, &MainWindow::updateFollowPreview);
    connect(m_followPanel, &FollowPanel::changed, this, [this] { m_followTimer->start(); });
    connect(m_followPanel, &FollowPanel::pathRequested, this, [this] { openFollowSketch(false); });
    connect(m_followPanel, &FollowPanel::profileRequested, this, [this] { openFollowSketch(true); });
    connect(m_followPanel, &FollowPanel::applyRequested, this, &MainWindow::applyFollow);
    connect(m_followPanel, &FollowPanel::cancelRequested, this, &MainWindow::endFollow);
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
    connect(m_properties, &PropertiesPanel::editEdgesRequested, this, [this] { startEdges(selectedOne(), true); });
    connect(m_properties, &PropertiesPanel::throughAllRequested, this, [this](ObjectId id, bool on) {
        const Body* body = m_doc.find(id);
        if (!body || !body->isBoolean() || body->boolean->throughAll == on)
            return;
        Body after = *body;
        after.boolean->throughAll = on;
        run(std::make_unique<EditBodyCommand>(*body, after, on ? "Through all" : "Not through all"));
    });
    connect(m_properties, &PropertiesPanel::hardwareRequested, this, [this](ObjectId id, bool on) {
        const Body* body = m_doc.find(id);
        if (!body || body->hardware == on)
            return;
        Body after = *body;
        after.hardware = on;
        run(std::make_unique<EditBodyCommand>(*body, after, on ? "Mark as joint part" : "Not a joint part"));
    });
    connect(m_properties, &PropertiesPanel::keepRoundChanged, this, [this](bool on) { m_keepRound = on; });
    connect(m_properties, &PropertiesPanel::smoothStrengthRequested, this, [this](ObjectId id, int strength) {
        const Body* body = m_doc.find(id);
        if (!body || !body->isBoolean() || !body->boolean->smooth || body->boolean->smooth->strength == strength)
            return;
        Body after = *body;
        after.boolean->smooth->strength = strength;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        run(std::make_unique<EditBodyCommand>(*body, after, "Smooth"));
        QApplication::restoreOverrideCursor();
    });
    connect(m_properties, &PropertiesPanel::simplifyKeepRequested, this, [this](ObjectId id, double keep) {
        const Body* body = m_doc.find(id);
        if (!body || !body->isBoolean() || !body->boolean->simplify ||
            std::abs(body->boolean->simplify->keep - keep) < 1e-9)
            return;
        Body after = *body;
        after.boolean->simplify->keep = keep;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        run(std::make_unique<EditBodyCommand>(*body, after, "Simplify"));
        QApplication::restoreOverrideCursor();
    });
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

    // Section view (left, under Objects): shown only while the view is cut open.
    m_sectionPanel = new SectionPanel;
    m_sectionDock = new QDockWidget(tr("Section View"), this);
    m_sectionDock->setObjectName(QStringLiteral("SectionDock"));
    m_sectionDock->setWidget(m_sectionPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_sectionDock);
    m_sectionDock->hide();
    connect(m_sectionDock, &QDockWidget::visibilityChanged, this, [this](bool shown) {
        if (shown)
            resizeDocks({m_sectionDock}, {m_sectionPanel->sizeHint().height() + 30}, Qt::Vertical);  // all of it showing
        applySection(true);
    });
    connect(m_sectionPanel, &SectionPanel::axisChanged, this, [this] { applySection(true); });
    connect(m_sectionPanel, &SectionPanel::changed, this, [this] { applySection(false); });
    connect(m_sectionAction, &QAction::toggled, m_sectionDock, &QDockWidget::setVisible);
    connect(m_sectionDock, &QDockWidget::visibilityChanged, m_sectionAction, &QAction::setChecked);
    connect(m_sectionDock, &QDockWidget::visibilityChanged, m_sketchOnSection, &QAction::setEnabled);
    m_viewMenu->addAction(m_sectionAction);
    m_viewMenu->addAction(m_referenceAction);
    m_viewMenu->addAction(m_mannequinAction);
    m_viewMenu->addAction(m_showPicturesAction);
    m_viewMenu->addSeparator();
    // Let the user bring closed panels back.
    m_viewMenu->addAction(objectsDock->toggleViewAction());
    m_viewMenu->addAction(propsDock->toggleViewAction());
}

void MainWindow::applySection(bool newRange)
{
    if (!m_sectionDock || !m_sectionPanel)
        return;
    if (!m_sectionDock->isVisible()) {
        m_viewport->setSection(std::nullopt);
        return;
    }
    const int axis = m_sectionPanel->axis();
    if (newRange) {
        // (Tell the view first which shapes are cut, so the reach is theirs.)
        Viewport::Section which;
        which.selectedOnly = m_sectionPanel->selectedOnly();
        m_viewport->setSection(which);
        const Bounds box = m_viewport->sceneBounds(m_sectionPanel->selectedOnly());
        // setRange comes back here (changed) to draw it.
        if (box.valid)
            m_sectionPanel->setRange(box.min[std::size_t(axis)], box.max[std::size_t(axis)], true);
        else
            m_sectionPanel->setRange(-50.0, 50.0, true);
        return;
    }
    // Hidden by default: the right (X), the front (Y), the top (Z): the sides the view
    // usually looks from.
    const bool hideHigh = (axis != 1) != m_sectionPanel->otherSide();
    Viewport::Section cut;
    cut.normal = QVector3D(axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f) *
                 (hideHigh ? 1.0f : -1.0f);
    cut.offset = float(m_sectionPanel->position()) * (hideHigh ? 1.0f : -1.0f);
    cut.selectedOnly = m_sectionPanel->selectedOnly();
    m_viewport->setSection(cut);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Left-click: select (Ctrl: add)   |   Left-drag: move on the plate or drag a handle   |   "
                                "Right-drag: orbit   |   Middle-drag: pan   |   Wheel: zoom   |   W / E / R: "
                                "move / rotate / resize   |   F: fit"));

    m_modelInfo = new QLabel(this);
    m_glInfo = new QLabel(this);
    m_fitInfo = new QLabel(this);
    m_fitInfo->setStyleSheet(QStringLiteral("color: #c0431a; font-weight: bold;"));
    m_fitInfo->setToolTip(tr("Orange-red: too big for the printer (Split to Fit cuts it into pieces that fit). "
                             "Amber: it would fit, but isn't on the plate: move it on to print it. "
                             "Edit > Build Plate Size sets the printer's size."));
    m_fitInfo->hide();
    statusBar()->addPermanentWidget(m_fitInfo);
    connect(m_viewport, &Viewport::offPlateChanged, this, [this](int big, int away) {
        QStringList parts;
        if (big > 0)
            parts << (big == 1 ? tr("1 shape too big for the printer") : tr("%1 shapes too big for the printer").arg(big));
        if (away > 0)
            parts << (away == 1 ? tr("1 shape off the plate") : tr("%1 shapes off the plate").arg(away));
        m_fitInfo->setVisible(!parts.isEmpty());
        m_fitInfo->setText(parts.join(QStringLiteral(" \u00B7 ")));
        m_fitInfo->setStyleSheet(big > 0 ? QStringLiteral("color: #c0431a; font-weight: bold;")
                                         : QStringLiteral("color: #b07a00; font-weight: bold;"));
    });
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
    if (m_edgesMode.active)
        endEdges();
    if (m_holeMode.active && !m_holeMode.positioning)
        endHole();
    if (m_shellMode.active)
        endShell();
    if (m_followMode.active && m_followMode.sketching == 0)
        endFollow();
    if (m_patternMode.active)
        endPattern();
    if (m_sketchMode.picking) {
        m_sketchMode.picking = false;  // picking something else leaves the face picking
        statusBar()->clearMessage();
    }
    const ObjectId previousOne = selectedOne();
    m_selection = std::move(ids);
    if (const Body* body = m_doc.find(selectedOne()); body && body->id != previousOne) {
        // A newly selected shape starts in "Round" mode only if it is round.
        m_keepRound = body->isBoolean() || isRound(body->spec);
        m_properties->setKeepRound(m_keepRound);
    }
    // Show a selected step or tool open in the tree: its object, and the steps above it.
    for (ObjectId id : m_selection)
        for (ObjectId p = m_doc.parentOf(id); p != kInvalidId; p = m_doc.parentOf(p)) {
            m_expanded.insert({kObjectRow, p});
            m_expanded.insert({kStepRow, p});
        }
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
    if (m_layFlat->isChecked()) {
        layFlatClick(origin, direction);
        return;
    }
    if (m_edgesMode.active) {
        edgesClick(origin, direction);
        return;
    }
    if (m_measureMode.active) {
        measureClick(origin, direction);
        return;
    }
    if (m_sketchMode.picking) {
        sketchFaceClick(origin, direction);
        return;
    }
    if (m_holeMode.active) {
        holeClick(origin, direction);
        return;
    }
    if (m_shellMode.active) {
        shellClick(origin, direction);
        return;
    }
    if (m_threadMode.active) {
        threadClick(origin, direction);
        return;
    }
    if (m_socketMode.active) {
        socketClick(origin, direction);
        return;
    }
    if (m_hollowMode.active) {
        hollowClick(origin, direction);
        return;
    }
    if (m_mouldMode.active) {
        mouldClick(origin, direction);
        return;
    }
    if (m_portMode.active) {
        portClick(origin, direction);
        return;
    }
    if (m_stampMode.active) {
        stampClick(origin, direction);
        return;
    }
    if (m_panelLineMode.active) {
        panelLineClick(origin, direction, modifiers);
        return;
    }
    if (m_hingeMode.active) {
        hingeClick(origin, direction);
        return;
    }
    if (m_joinMode.active) {
        joinClick(origin, direction);
        return;
    }
    if (m_maskMode.active || m_paintMode.active)
        return;  // painting is done by pressing and dragging
    if (m_referenceMode.active) {
        referenceClick(origin, direction);
        return;
    }
    if (m_poseMode.active) {
        poseClick(origin, direction);
        return;
    }
    if (m_poseFigureMode.active || m_cutPartsMode.active)
        return;  // balls are picked by pressing on them
    if (m_followMode.active) {
        followClick(origin, direction);
        return;
    }
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {direction.x(), direction.y(), direction.z()};
    try {
        const auto hit = pickShown(ray);
        if (!hit && !modifiers.testFlag(Qt::ControlModifier) && referenceClickedOutside(origin, direction))
            return;
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
    if (body->isBoolean() || std::holds_alternative<PadSpec>(body->spec)) {
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
    if (m_measureMode.active)
        return;  // measuring: clicks pick, nothing moves
    if (m_sculptMode.active) {
        beginStroke(origin, direction, modifiers);
        return;
    }
    if (m_paintMode.active) {
        m_paintMode.stroking = true;
        m_paintMode.erasing = modifiers.testFlag(Qt::ControlModifier);
        m_paintMode.lastDab.reset();
        m_paintMode.strokeStarts.push_back(m_paintMode.dabs.size());
        paintAt(origin, direction);
        return;
    }
    if (m_maskMode.active) {
        m_maskMode.stroking = true;
        m_maskMode.erasing = modifiers.testFlag(Qt::ControlModifier);
        m_maskMode.lastDab.reset();
        m_maskMode.strokeStarts.push_back(m_maskMode.dabs.size());
        maskAt(origin, direction);
        return;
    }
    if (m_poseMode.active) {
        poseGrab(origin, direction);  // a part on a joint turns as it's dragged
        return;
    }
    if (m_poseFigureMode.active) {
        poseFigureGrab(origin, direction);  // a ball of the skeleton
        return;
    }
    if (m_cutPartsMode.active) {
        cutPartsGrab(origin, direction);
        return;
    }
    if (m_stampMode.active) {
        // Dragging from the shape stamps a row.
        m_stampMode.rowStart = stampSpotAt(origin, direction);
        m_stampMode.row.clear();
        return;
    }
    if (m_referenceMode.active) {
        // Pictures can be dragged; shapes stay put.
        if (!m_referenceMode.picking)
            grabReference(origin, direction);
        return;
    }
    if (m_splitMode.active) {
        // The Split tool is open: shapes stay put, but the plane can be dragged.
        if (grabSplitPlane(origin, direction))
            m_pressOnHandle = true;
        return;
    }
    if (m_fitMode.active) {
        // Split to Fit: the planned cuts can be dragged.
        if (grabFitPlane(origin, direction))
            m_pressOnHandle = true;
        return;
    }
    if (m_embossMode.active || m_edgesMode.active || m_holeMode.active || m_shellMode.active || m_followMode.active ||
        m_patternMode.active || m_threadMode.active || m_socketMode.active || m_joinMode.active ||
        m_maskMode.active || m_hollowMode.active || m_panelLineMode.active || m_poseFigureMode.active ||
        m_paintMode.active || m_cutPartsMode.active || m_mouldMode.active ||
        m_portMode.active || m_handMode.active)
        return;  // shapes stay put while the tool is open

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
        // Combined shapes and pads resize by scaling what's inside their box.
        if (handle.kind == view::GizmoHandle::Kind::Face &&
            (selected->isBoolean() || std::holds_alternative<PadSpec>(selected->spec))) {
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
    const auto hit = pickShown(ray);
    if (!hit) {
        // An active reference picture can be dragged like a shape (a plain click opens
        // its panel, on release).
        if (grabReference(origin, direction))
            statusBar()->showMessage(tr("Moving the %1 picture. (Untick Active in Reference Images to click "
                                        "straight through it.)")
                                         .arg(tr(reference::title(m_referenceMode.dragged))),
                                     6000);
        return;  // empty space: the click (on release) clears the selection
    }
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
    if (m_sculptMode.active) {
        if (m_sculptMode.stroking)
            sculptAt(origin, direction, modifiers);
        return;
    }
    if (m_paintMode.active) {
        if (m_paintMode.stroking)
            paintAt(origin, direction);
        paintHover(origin, direction);
        return;
    }
    if (m_maskMode.active) {
        if (m_maskMode.stroking)
            maskAt(origin, direction);
        maskHover(origin, direction);
        return;
    }
    if (m_referenceMode.active || m_referenceMode.dragging) {
        if (m_referenceMode.dragging)
            dragReference(origin, direction);
        return;
    }
    if (m_poseMode.active) {
        poseDrag(origin, direction);
        return;
    }
    if (m_poseFigureMode.active) {
        poseFigureDrag(origin, direction);
        return;
    }
    if (m_cutPartsMode.active) {
        cutPartsDrag(origin, direction);
        return;
    }
    if (m_stampMode.active) {
        stampDrag(origin, direction, modifiers);
        return;
    }
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
    case Drag::Kind::FitPlane: {
        const auto s = view::closestOnLine(origin, direction, d.grabPoint, d.axis);
        if (!s || m_fitMode.selected < 0 || m_fitMode.selected >= int(m_fitMode.plan.cuts.size()))
            return;
        FitCut& cut = m_fitMode.plan.cuts[std::size_t(m_fitMode.selected)];
        cut.position = d.startOffset + step(*s - d.startParam, kMoveStep);
        statusBar()->showMessage(tr("Cut %1 at %2 mm").arg(m_fitMode.selected + 1).arg(cut.position, 0, 'f', 1) +
                                 freeHint);
        d.moved = true;
        m_fitPanel->setCuts(m_fitMode.plan.cuts, m_fitMode.selected);
        updateFit();
        return;
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
        if (!d.beforeAll.empty()) {
            // A combined shape (or a pad): stretch everything inside, keeping the opposite side still.
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
    if (m_stampMode.active) {
        auto& mode = m_stampMode;
        if (wasDrag && !mode.row.empty()) {
            mode.groups.push_back(mode.spots.size());
            mode.spots.insert(mode.spots.end(), mode.row.begin(), mode.row.end());
        }
        mode.rowStart.reset();
        mode.row.clear();
        updateStampPreview();
        return;
    }
    if (m_poseMode.active) {
        m_poseMode.dragging = false;
        return;
    }
    if (m_poseFigureMode.active) {
        poseFigureRelease();
        return;
    }
    if (m_cutPartsMode.active) {
        if (m_cutPartsMode.picked >= 0) {
            m_cutPartsMode.picked = -1;
            updateCutPartsPreview(true);
        }
        return;
    }
    if (m_referenceMode.active || m_referenceMode.dragging) {
        m_referenceMode.dragging = false;
        return;
    }
    if (m_paintMode.active) {
        m_paintMode.stroking = false;
        m_paintMode.lastDab.reset();
        // A press that painted nothing isn't a stroke.
        if (!m_paintMode.strokeStarts.empty() && m_paintMode.strokeStarts.back() == m_paintMode.dabs.size())
            m_paintMode.strokeStarts.pop_back();
        m_paintPanel->setStrokes(int(m_paintMode.strokeStarts.size()));
        return;
    }
    if (m_maskMode.active) {
        m_maskMode.stroking = false;
        m_maskMode.lastDab.reset();
        // A press that painted nothing isn't a stroke.
        if (!m_maskMode.strokeStarts.empty() && m_maskMode.strokeStarts.back() == m_maskMode.dabs.size())
            m_maskMode.strokeStarts.pop_back();
        m_maskPanel->setStrokes(int(m_maskMode.strokeStarts.size()));
        m_maskPanel->setApplyEnabled(!m_maskMode.dabs.empty());
        return;
    }
    if (m_sculptMode.active) {
        m_sculptMode.stroking = false;
        m_sculptMode.lastDab.reset();
        if (m_sculptMode.grabbing && m_sculptMode.sculptor)
            m_sculptMode.sculptor->endGrab();
        if (m_sculptMode.snaking && m_sculptMode.sculptor)
            m_sculptMode.sculptor->endSnake();
        if (m_sculptMode.sculptor) {
            m_sculptMode.sculptor->endPattern();
            m_sculptMode.sculptor->settle();  // the moved triangles refiled for finding the surface
        }
        m_sculptMode.grabbing = false;
        m_sculptMode.snaking = false;
        return;
    }
    Drag d = m_dragState;
    m_dragState = Drag{};
    m_viewport->setActiveHandle({});
    if (d.kind == Drag::Kind::None || !wasDrag || !d.moved)
        return;
    statusBar()->clearMessage();
    if (d.kind == Drag::Kind::SplitPlane || d.kind == Drag::Kind::FitPlane)
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
    else if (d.kind == Drag::Kind::FitPlane && m_fitMode.selected >= 0 &&
             m_fitMode.selected < int(m_fitMode.plan.cuts.size())) {
        m_fitMode.plan.cuts[std::size_t(m_fitMode.selected)].position = d.startOffset;
        m_fitPanel->setCuts(m_fitMode.plan.cuts, m_fitMode.selected);
        updateFit();
    } else if (!d.beforeAll.empty() && d.moved)
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

// ---- Custom shapes ------------------------------------------------------------------

QString MainWindow::customShapesFolder() const
{
    const QString chosen = Preferences().value(QStringLiteral("library/folder")).toString();
    if (!chosen.isEmpty())
        return chosen;
    // In Documents, where it's easy to find, back up or sync.
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
           QStringLiteral("/Forge3D/Custom Shapes");
}

void MainWindow::fillCustomMenu()
{
    m_customMenu->clear();
    const auto entries = library::list(toPath(customShapesFolder()));
    for (const auto& entry : entries) {
        const QString name = QString::fromStdString(entry.name);
#ifdef _WIN32
        const QString file = QString::fromStdWString(entry.file.wstring());
#else
        const QString file = QString::fromStdString(entry.file.string());
#endif
        QAction* a = m_customMenu->addAction(name);
        a->setToolTip(tr("Add '%1' at the size it was saved").arg(name));
        connect(a, &QAction::triggered, this, [this, file] { addCustomShape(file); });
    }
    if (entries.empty()) {
        QAction* none = m_customMenu->addAction(tr("None saved yet"));
        none->setEnabled(false);
        none->setToolTip(tr("Select a shape, then Print/Save → Save Custom Shape"));
    }
    m_customMenu->addSeparator();
    QAction* manage = m_customMenu->addAction(tr("Manage Custom Shapes..."));
    manage->setToolTip(tr("Rename or delete custom shapes, or choose the folder they're kept in"));
    connect(manage, &QAction::triggered, this, &MainWindow::manageCustomShapes);
}

void MainWindow::saveCustomShape()
{
    if (m_selection.empty() || !std::all_of(m_selection.begin(), m_selection.end(),
                                             [this](ObjectId id) { return m_doc.isTopLevel(id); })) {
        statusBar()->showMessage(tr("Save Custom Shape: select the shapes to save first."), 6000);
        return;
    }
    const QString suggested = m_selection.size() == 1 ? QString::fromStdString(m_doc.find(m_selection.front())->name)
                                                      : QString();
    const QString folder = customShapesFolder();
    QString name;
    for (;;) {
        bool ok = false;
        name = QInputDialog::getText(this, tr("Save Custom Shape"),
                                     m_selection.size() == 1
                                         ? tr("Name for this custom shape (it's added at this size from Add "
                                              "Shape → Custom):")
                                         : tr("Name for these %1 shapes, saved together (they're added as they "
                                              "sit now, from Add Shape → Custom):").arg(m_selection.size()),
                                     QLineEdit::Normal, suggested, &ok)
                   .trimmed();
        if (!ok || name.isEmpty())
            return;
        std::string fileName;
        try {
            fileName = library::fileNameFor(name.toStdString());
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Save Custom Shape"), QString::fromUtf8(e.what()));
            continue;
        }
        if (!QFileInfo::exists(folder + QLatin1Char('/') + QString::fromStdString(fileName)))
            break;
        const auto answer = QMessageBox::question(
            this, tr("Save Custom Shape"),
            tr("There's already a custom shape called '%1'. Replace it?\n\nShapes already added from it aren't "
               "changed.").arg(name),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
        if (answer == QMessageBox::Yes)
            break;
        if (answer == QMessageBox::Cancel)
            return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        const std::string createdBy = std::string(build::kAppName) + " " + build::kVersion;
        library::save(m_doc, m_selection, toPath(folder), name.toStdString(), createdBy);
        QApplication::restoreOverrideCursor();
        qInfo().noquote() << "Saved custom shape" << name << "in" << folder;
        statusBar()->showMessage(tr("Saved '%1'. Add it from Add Shape → Custom.").arg(name), 6000);
    } catch (const std::exception& e) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Save Custom Shape"),
                             tr("The custom shape couldn't be saved:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::addCustomShape(const QString& file)
{
    if (toolOpen()) {
        statusBar()->showMessage(tr("Finish what you're doing first (Done or Cancel), then add the shape."), 6000);
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        const auto made = library::add(m_stack, m_doc, toPath(file));
        QApplication::restoreOverrideCursor();
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        setSelection(made);
    } catch (const std::exception& e) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("The custom shape couldn't be added:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::manageCustomShapes()
{
    CustomShapesDialog dialog(customShapesFolder(), this);
    connect(&dialog, &CustomShapesDialog::folderChanged, this,
            [](const QString& folder) { Preferences().setValue(QStringLiteral("library/folder"), folder); });
    connect(&dialog, &CustomShapesDialog::addRequested, this, [this](const QString& file) { addCustomShape(file); });
    dialog.exec();
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
constexpr std::uint64_t kEmbossWordsItem = ~std::uint64_t{0} - 4;
constexpr std::uint64_t kEdgesPickedItem = ~std::uint64_t{0} - 5;
constexpr std::uint64_t kEdgesHoverItem = ~std::uint64_t{0} - 6;
constexpr std::uint64_t kMeasurePickedItem = ~std::uint64_t{0} - 7;
constexpr std::uint64_t kMeasureHoverItem = ~std::uint64_t{0} - 8;
constexpr std::uint64_t kMeasureLineItem = ~std::uint64_t{0} - 9;
constexpr std::uint64_t kMeasureCentreItem = ~std::uint64_t{0} - 10;
constexpr std::uint64_t kMannequinItem = ~std::uint64_t{0} - 20;
constexpr std::uint64_t kMouldItem = ~std::uint64_t{0} - 30;  // to - 34: the Make Mould helpers; - 35: Hand Ports

// Thin six-sided tubes along lines: how picked edges are drawn.
std::shared_ptr<const Mesh> tubes(const std::vector<const std::vector<Vec3>*>& lines, float radius)
{
    auto mesh = std::make_shared<Mesh>();
    constexpr int sides = 6;
    for (const auto* line : lines) {
        for (std::size_t i = 0; i + 1 < line->size(); ++i) {
            const QVector3D a((*line)[i][0], (*line)[i][1], (*line)[i][2]);
            const QVector3D b((*line)[i + 1][0], (*line)[i + 1][1], (*line)[i + 1][2]);
            const QVector3D along = (b - a).normalized();
            if (along.isNull())
                continue;
            QVector3D side = QVector3D::crossProduct(along, QVector3D(0, 0, 1));
            if (side.length() < 0.1f)
                side = QVector3D::crossProduct(along, QVector3D(1, 0, 0));
            side.normalize();
            const QVector3D other = QVector3D::crossProduct(along, side);
            const auto base = std::uint32_t(mesh->positions.size() / 3);
            for (const QVector3D& end : {a, b})
                for (int k = 0; k < sides; ++k) {
                    const float t = float(k) * 2.0f * float(M_PI) / sides;
                    const QVector3D n = side * std::cos(t) + other * std::sin(t);
                    const QVector3D p = end + n * radius;
                    mesh->positions.insert(mesh->positions.end(), {p.x(), p.y(), p.z()});
                    mesh->normals.insert(mesh->normals.end(), {n.x(), n.y(), n.z()});
                }
            for (int k = 0; k < sides; ++k) {
                const std::uint32_t a0 = base + std::uint32_t(k), a1 = base + std::uint32_t((k + 1) % sides);
                const std::uint32_t b0 = a0 + sides, b1 = a1 + sides;
                mesh->indices.insert(mesh->indices.end(), {a0, a1, b1, a0, b1, b0});
            }
        }
    }
    return mesh;
}
// Shapes bigger than this get a words-only preview: re-cutting them on every change is slow.
constexpr std::size_t kQuickEmbossTriangles = 150'000;

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
    m_splitPanel->setKeyOptions(savedKeyOptions());
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
    quad.helper = true;
    items.push_back(quad);
    for (const bool head : {false, true}) {
        DisplayItem arrow;
        arrow.id = head ? kSplitHeadItem : kSplitShaftItem;
        arrow.mesh = splitArrowMesh(head);
        arrow.model = planeFrame(plane.point, plane.normal, s * 0.6f);
        arrow.color = QVector3D(1.0f, 0.75f, 0.0f);
        arrow.helper = true;
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
    const KeyOptions keys = m_splitPanel->keyOptions();
    int keyed = 0;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (m_splitPanel->keep() == SplitKeep::Both && keys.keys) {
            const KeyedSplit result = splitWithKeys(m_stack, m_doc, m_splitMode.target, plane, keys);
            made = result.pieces;
            keyed = result.keys;
        } else {
            made = splitBody(m_stack, m_doc, m_splitMode.target, plane, m_splitPanel->keep());
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Split did not work:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    const bool wantedKeys = m_splitPanel->keep() == SplitKeep::Both && keys.keys;
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel()) << "-" << keyed << "keys";
    endSplit();
    setSelection(made.front());
    if (!wantedKeys)
        return;
    if (keyed > 0) {
        statusBar()->showMessage(tr("Split, with %1 alignment %2.")
                                     .arg(keyed)
                                     .arg(keys.loosePins ? (keyed == 1 ? tr("pin") : tr("pins"))
                                                         : (keyed == 1 ? tr("peg") : tr("pegs"))),
                                 10000);
        return;
    }
    // Asked for pegs and got none: say so plainly, and why. (Once this click is done
    // with: the panel it came from has just been put away.)
    QTimer::singleShot(0, this, [this, keys] {
        const double smallest = std::max(2.0, keys.diameter * 0.5);
        const double across = smallest + 2 * (keys.clearance + 1.2);
        const double behind = (keys.loosePins ? keys.length / 2 : keys.length) + 0.5 + 1.2;
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("No room for alignment pegs"));
        box.setText(tr("The shape was split, but without alignment pegs: the cut is too small or thin for them."));
        box.setInformativeText(
            tr("Even the smallest peg (%1 mm) needs about %2 mm of solid across the cut, and %3 mm of solid behind "
               "it for its hole. Try a smaller diameter or a shorter peg in Split, cut somewhere thicker, or line "
               "the pieces up by eye.")
                .arg(smallest, 0, 'g', 3)
                .arg(across, 0, 'g', 3)
                .arg(behind, 0, 'g', 3));
        QPushButton* undo = box.addButton(tr("Undo the Split"), QMessageBox::DestructiveRole);
        box.addButton(tr("Keep It"), QMessageBox::AcceptRole);
        box.exec();
        if (box.clickedButton() == undo)
            m_stack.undo();
    });
}

KeyOptions MainWindow::savedKeyOptions() const
{
    const Preferences s;
    KeyOptions o;
    o.keys = s.value(QStringLiteral("split/keys"), true).toBool();
    o.loosePins = s.value(QStringLiteral("split/pins"), false).toBool();
    o.diameter = s.value(QStringLiteral("split/diameter"), 5.0).toDouble();
    o.length = s.value(QStringLiteral("split/length"), 6.0).toDouble();
    o.taper = s.value(QStringLiteral("split/taper"), 2.0).toDouble();
    o.clearance = s.value(QStringLiteral("split/clearance"), 0.2).toDouble();
    return o;
}

void MainWindow::saveKeyOptions(const KeyOptions& o)
{
    Preferences s;
    s.setValue(QStringLiteral("split/keys"), o.keys);
    s.setValue(QStringLiteral("split/pins"), o.loosePins);
    s.setValue(QStringLiteral("split/diameter"), o.diameter);
    s.setValue(QStringLiteral("split/length"), o.length);
    s.setValue(QStringLiteral("split/taper"), o.taper);
    s.setValue(QStringLiteral("split/clearance"), o.clearance);
}

std::array<double, 3> MainWindow::printerVolume() const
{
    return {m_viewport->plateSize().width(), m_viewport->plateSize().height(), m_viewport->plateHeight()};
}

void MainWindow::splitToFit(ObjectId target)
{
    const Body* body = m_doc.find(target);
    if (!body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    const QString name = QString::fromStdString(body->name);
    const auto volume = printerVolume();
    const KeyOptions keys = savedKeyOptions();
    const QString printer = tr("%1 \u00D7 %2 \u00D7 %3 mm").arg(volume[0]).arg(volume[1]).arg(volume[2]);
    SmartPlan plan;
    try {
        plan = planSmartSplit(m_doc, target, volume, keys);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Split to Fit"), QString::fromUtf8(e.what()));
        return;
    }
    if (plan.pieces <= 1) {
        QMessageBox::information(this, tr("Split to Fit"),
                                 tr("'%1' already fits your printer (%2): there's nothing to split.\n\n"
                                    "(Edit > Build Plate Size sets the printer's size.)")
                                     .arg(name, printer));
        return;
    }
    startFit(target);
}

void MainWindow::layOutShapes(bool turnOver)
{
    if (toolOpen())
        return;
    std::vector<ObjectId> ids;
    for (ObjectId id : m_selection)
        if (m_doc.find(id) && m_doc.isTopLevel(id))
            ids.push_back(id);
    if (ids.empty())
        ids = m_doc.topLevelIds();
    if (ids.empty())
        return;
    LayoutOptions options;
    options.plate = printerVolume();
    options.turnOver = turnOver;
    LayoutResult result;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        result = layOut(m_stack, m_doc, ids, options);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Lay Out"), tr("Lay Out did not work:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel()) << "-" << result.plates << "plates";
    refreshCommandState();
    QTimer::singleShot(50, this, [this] { m_viewport->fitView(); });  // once the plates are drawn
    QString message = result.plates <= 1 ? tr("Laid out on the plate.")
                                         : tr("Laid out on %1 plates, side by side. Export can save one file per plate.")
                                               .arg(result.plates);
    if (!result.tooBig.empty())
        message += QStringLiteral(" ") + tr("%n shape(s) too big for the printer were left where they are (Split to "
                                            "Fit can cut them up).",
                                            nullptr, int(result.tooBig.size()));
    statusBar()->showMessage(message, 15000);
}

void MainWindow::startFit(ObjectId target)
{
    const Body* body = m_doc.find(target);
    if (!body || !m_doc.isTopLevel(target))
        return;
    if (m_splitMode.active)
        endSplit();
    m_fitMode = FitMode{};
    m_fitMode.active = true;
    m_fitMode.target = target;
    const bool angled = Preferences().value(QStringLiteral("fit/angled"), false).toBool();
    m_fitPanel->start(tr("Split '%1' to fit the printer").arg(QString::fromStdString(body->name)), QString(), angled,
                      Preferences().value(QStringLiteral("fit/layOut"), true).toBool());
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_fitPanel->show();
    planFit(angled);
    if (!m_fitMode.active)
        return;  // couldn't plan
    statusBar()->showMessage(tr("Split to Fit: these are the cuts it would make. Drag a plane (or use the panel) to "
                                "move a cut, then click Split. Esc cancels."));
    refreshCommandState();
}

void MainWindow::planFit(bool angled)
{
    if (!m_fitMode.active)
        return;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        m_fitMode.box = m_doc.worldBounds(m_fitMode.target);
        m_fitMode.plan = planFitCuts(m_doc, m_fitMode.target, printerVolume(), savedKeyOptions(), angled);
    } catch (const std::exception& e) {
        const QString why = QString::fromUtf8(e.what());
        endFit();
        QTimer::singleShot(0, this, [this, why] { QMessageBox::warning(this, tr("Split to Fit"), why); });
        return;
    }
    m_fitMode.selected = m_fitMode.plan.cuts.empty() ? -1 : 0;
    m_fitPanel->setCuts(m_fitMode.plan.cuts, m_fitMode.selected);
    updateFit();
}

void MainWindow::updateFit()
{
    if (!m_fitMode.active)
        return;
    const auto volume = printerVolume();
    const KeyOptions keys = savedKeyOptions();
    const auto size = m_fitMode.box.size();
    QString summary = tr("%1 × %2 × %3 mm; your printer takes %4 × %5 × %6 mm. ")
                          .arg(std::round(size[0] * 10) / 10)
                          .arg(std::round(size[1] * 10) / 10)
                          .arg(std::round(size[2] * 10) / 10)
                          .arg(volume[0])
                          .arg(volume[1])
                          .arg(volume[2]);
    summary += tr("%n cut(s), making about %1 pieces.", nullptr, int(m_fitMode.plan.cuts.size()))
                   .arg(m_fitMode.plan.pieces);
    if (m_fitMode.plan.turned)
        summary += tr(" The pieces are meant to lie turned a quarter on the plate.");
    if (!keys.keys)
        summary += tr(" The cuts won't be keyed (alignment pegs are off in Split).");
    m_fitPanel->setSummary(summary);
    QStringList problems;
    try {
        for (const std::string& p : fitProblems(m_doc, m_fitMode.target, m_fitMode.plan, volume, keys))
            problems << QString::fromStdString(p);
    } catch (const std::exception& e) {
        problems << QString::fromUtf8(e.what());
    }
    m_fitPanel->setProblems(problems);
    refreshScene();
}

void MainWindow::endFit()
{
    if (!m_fitMode.active)
        return;
    if (m_dragState.kind == Drag::Kind::FitPlane)
        m_dragState = Drag{};
    m_fitMode = FitMode{};
    m_fitPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::applyFit()
{
    if (!m_fitMode.active)
        return;
    const ObjectId target = m_fitMode.target;
    const std::vector<FitCut> cuts = m_fitMode.plan.cuts;
    const KeyOptions keys = savedKeyOptions();
    const bool thenLayOut = m_fitPanel->layOut();
    std::vector<Vec3> cutDirections;
    for (const FitCut& c : cuts)
        cutDirections.push_back(planeOf(c, m_fitMode.box).normal);
    std::vector<std::string> problems;
    try {
        problems = fitProblems(m_doc, target, m_fitMode.plan, printerVolume(), keys);
    } catch (const std::exception&) {
    }
    if (!problems.empty()) {
        QStringList lines;
        for (const std::string& p : problems)
            lines << QString::fromStdString(p);
        if (QMessageBox::question(this, tr("Split to Fit"),
                                  tr("These cuts may not make pieces that fit:\n\n%1\n\nSplit anyway?")
                                      .arg(lines.join(QStringLiteral("\n"))),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }
    endFit();
    KeyedSplit made;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        const std::string name = m_doc.find(target)->name;
        m_stack.beginMacro("Split " + name + " to fit the printer");
        try {
            made = splitAlong(m_stack, m_doc, target, cuts, keys);
            if (thenLayOut) {
                LayoutOptions options;
                options.plate = printerVolume();
                options.favour = cutDirections;  // pegs up and holes down: they print round
                std::vector<ObjectId> all = made.pieces;
                all.insert(all.end(), made.pins.begin(), made.pins.end());
                layOut(m_stack, m_doc, all, options);
            }
        } catch (...) {
            m_stack.abortMacro();  // undo what was done of it
            throw;
        }
        m_stack.endMacro();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Split to Fit"), tr("Split to Fit did not work:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel()) << "-" << made.pieces.size()
                      << "pieces," << made.keys << "keys";
    std::vector<ObjectId> all = made.pieces;
    all.insert(all.end(), made.pins.begin(), made.pins.end());
    setSelection(all);
    if (thenLayOut)
        QTimer::singleShot(50, this, [this] { m_viewport->fitView(); });  // once the plates are drawn
    statusBar()->showMessage((thenLayOut ? tr("Split into %1 pieces (%2 alignment %3), laid out on the plates.")
                                         : tr("Split into %1 pieces (%2 alignment %3). Move one onto the plate to "
                                              "print it."))
                                 .arg(made.pieces.size())
                                 .arg(made.keys)
                                 .arg(made.keys == 1 ? tr("key") : tr("keys")),
                             20000);
}

namespace {
constexpr std::uint64_t kFitItemBase = ~std::uint64_t{0} - 100;  // ids no body will reach
}

void MainWindow::addFitItems(std::vector<DisplayItem>& items) const
{
    if (!m_fitMode.active)
        return;
    const auto size = m_fitMode.box.size();
    const float s = std::max({size[0], size[1], size[2], 2.0f}) * 0.6f;
    for (std::size_t i = 0; i < m_fitMode.plan.cuts.size(); ++i) {
        SplitPlane plane;
        try {
            plane = planeOf(m_fitMode.plan.cuts[i], m_fitMode.box);
        } catch (const std::exception&) {
            continue;
        }
        const bool picked = int(i) == m_fitMode.selected;
        DisplayItem quad;
        quad.id = kFitItemBase - i * 3;
        quad.mesh = splitPlaneMesh();
        quad.model = planeFrame(plane.point, plane.normal, s);
        quad.color = picked ? QVector3D(1.0f, 0.82f, 0.1f) : QVector3D(0.35f, 0.65f, 1.0f);
        quad.alpha = picked ? 0.45f : 0.28f;
        quad.helper = true;
        items.push_back(quad);
        if (!picked)
            continue;
        for (const bool head : {false, true}) {
            DisplayItem arrow;
            arrow.id = kFitItemBase - i * 3 - (head ? 1 : 2);
            arrow.mesh = splitArrowMesh(head);
            arrow.model = planeFrame(plane.point, plane.normal, s * 0.6f);
            arrow.color = QVector3D(1.0f, 0.75f, 0.0f);
            arrow.helper = true;
            items.push_back(arrow);
        }
    }
}

bool MainWindow::grabFitPlane(const QVector3D& origin, const QVector3D& direction)
{
    const auto size = m_fitMode.box.size();
    const float s = std::max({size[0], size[1], size[2], 2.0f}) * 0.6f;
    // The nearest planned plane under the mouse (the picked one first if it's hit).
    int best = -1;
    float bestDistance = std::numeric_limits<float>::max();
    QVector3D grab;
    for (std::size_t i = 0; i < m_fitMode.plan.cuts.size(); ++i) {
        SplitPlane plane;
        try {
            plane = planeOf(m_fitMode.plan.cuts[i], m_fitMode.box);
        } catch (const std::exception&) {
            continue;
        }
        const QVector3D p(float(plane.point[0]), float(plane.point[1]), float(plane.point[2]));
        QVector3D n(float(plane.normal[0]), float(plane.normal[1]), float(plane.normal[2]));
        n.normalize();
        const auto at = view::intersectPlane(origin, direction, p, n);
        if (!at)
            continue;
        const QMatrix4x4 frame = planeFrame(plane.point, plane.normal, 1.0f);
        const QVector3D u = frame.column(0).toVector3D(), v = frame.column(1).toVector3D();
        const QVector3D d = *at - p;
        if (std::abs(QVector3D::dotProduct(d, u)) > s || std::abs(QVector3D::dotProduct(d, v)) > s)
            continue;
        float distance = (*at - origin).length();
        if (int(i) == m_fitMode.selected)
            distance *= 0.5f;  // the picked plane wins where planes overlap
        if (distance < bestDistance) {
            bestDistance = distance;
            best = int(i);
            grab = *at;
        }
    }
    if (best < 0)
        return false;
    m_fitMode.selected = best;
    m_fitPanel->setCuts(m_fitMode.plan.cuts, best);
    refreshScene();
    // Dragged straight along its axis: the cut's position is where it crosses the middle.
    const FitCut& cut = m_fitMode.plan.cuts[std::size_t(best)];
    QVector3D axis(cut.axis == 0, cut.axis == 1, cut.axis == 2);
    const auto startParam = view::closestOnLine(origin, direction, grab, axis);
    if (!startParam) {
        statusBar()->showMessage(tr("Looking straight along the cut's axis: orbit a little to drag it."), 4000);
        return true;
    }
    m_dragState.kind = Drag::Kind::FitPlane;
    m_dragState.grabPoint = grab;
    m_dragState.axis = axis;
    m_dragState.startParam = *startParam;
    m_dragState.startOffset = cut.position;
    return true;
}

void MainWindow::startEdges(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    if (m_splitMode.active)
        endSplit();
    if (m_embossMode.active)
        endEmboss();
    EdgesMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    EdgeFinish finish;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        mode.shape = pickableShape(m_doc, target);
        mode.found = edges::analyse(mode.shape);
        mode.finder = std::make_shared<edges::FaceFinder>(mode.shape);
        const auto size = m_doc.worldBounds(target).size();
        mode.size = std::sqrt(double(size[0]) * size[0] + double(size[1]) * size[1] + double(size[2]) * size[2]);
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Edges)
                return;
            finish = edgesInPlate(m_doc, target);
            // The picks as the result finds them now (the shape under them may have changed
            // size); each re-made from its edge as it is now, so it's stored fresh on Apply.
            const std::vector<int> found = pickedEdges(m_doc, target);
            for (std::size_t i = 0; i < found.size(); ++i) {
                if (found[i] < 0)
                    continue;
                const auto& faces = mode.found.edges[std::size_t(found[i])].faces;
                const Vec3 n = finish.edges[i].faceNormal;
                // Keep the face it was picked from: the one facing the same way.
                int face = -1;
                if (finish.edges[i].located) {
                    const auto& normals = mode.found.edges[std::size_t(found[i])].normals;
                    const auto dot = [&n](const Vec3& a) { return a[0] * n[0] + a[1] * n[1] + a[2] * n[2]; };
                    face = dot(normals[1]) > dot(normals[0]) ? faces[1] : faces[0];
                }
                mode.picks.push_back({found[i], edges::refFor(mode.shape, mode.found, found[i], face)});
            }
            if (found.size() != finish.edges.size() ||
                std::count(found.begin(), found.end(), -1) > 0)
                qInfo() << "Some picked edges could not be found again";
        } else {
            // A size to suit the shape: about a twentieth of its smallest side.
            auto sides = m_doc.worldBounds(target).size();
            std::sort(sides.begin(), sides.end());
            finish.size = std::clamp(std::round(double(sides[0]) * 0.1 * 2.0) / 2.0, 0.5, 5.0);
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_edgesMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_edgesPanel->start(editing ? tr("Change the edges of '%1'").arg(name) : tr("Edges of '%1'").arg(name), editing,
                        finish.kind, finish.size, finish.angle);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_edgesPanel->show();
    statusBar()->showMessage(tr("Edges: click edges (or faces) to pick them, then click Apply. Esc cancels."));
    rebuildEdgeHighlights();
    updateEdgesPreview();
    refreshCommandState();
}

void MainWindow::endEdges()
{
    if (!m_edgesMode.active)
        return;
    m_edgesTimer->stop();
    m_edgesMode = EdgesMode{};
    m_edgesPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

std::optional<EdgeFinish> MainWindow::currentEdgeFinish() const
{
    if (!m_edgesMode.active || m_edgesMode.picks.empty())
        return std::nullopt;
    EdgeFinish f;
    f.kind = m_edgesPanel->kind();
    f.size = m_edgesPanel->size();
    f.angle = m_edgesPanel->angle();
    for (const auto& pick : m_edgesMode.picks)
        f.edges.push_back(pick.ref);
    return f;
}

std::optional<BooleanSpec> MainWindow::currentEdgesSpec() const
{
    const auto f = currentEdgeFinish();
    if (!f)
        return std::nullopt;
    if (m_edgesMode.editing)
        return withEdges(m_doc, m_edgesMode.target, *f).boolean;
    return edgesSpec(m_doc, m_edgesMode.target, *f);
}

void MainWindow::updateEdgesPreview()
{
    if (!m_edgesMode.active)
        return;
    m_edgesMode.preview.reset();
    m_edgesPanel->setPickedCount(int(m_edgesMode.picks.size()));
    bool ok = false;
    try {
        const auto spec = currentEdgesSpec();
        if (!spec) {
            m_edgesPanel->setStatus(tr("Click the edges to change."), false);
        } else {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            m_edgesMode.preview = m_doc.previewMesh(*spec);
            m_edgesPanel->setStatus(tr("This is how it will look."), false);
            ok = true;
        }
    } catch (const std::exception& e) {
        m_edgesPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    m_edgesPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::rebuildEdgeHighlights()
{
    const float radius = std::max(0.02f, m_viewport->camera().distance() * 0.0022f);
    std::vector<const std::vector<Vec3>*> picked;
    for (const auto& pick : m_edgesMode.picks)
        picked.push_back(&m_edgesMode.found.edges[std::size_t(pick.edge)].polyline);
    m_edgesMode.pickedLines = picked.empty() ? nullptr : tubes(picked, radius);
    const int h = m_edgesMode.hover;
    m_edgesMode.hoverLine =
        h >= 0 ? tubes({&m_edgesMode.found.edges[std::size_t(h)].polyline}, radius * 1.4f) : nullptr;
}

void MainWindow::edgesHover(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel)
{
    // The suitable edge drawn nearest the mouse (within a few pixels), if not hidden.
    const EdgeFinishKind kind = m_edgesPanel->kind();
    const QVector3D dir = direction.normalized();
    std::optional<double> hitDistance;
    if (const auto hit = m_edgesMode.finder->under({origin.x(), origin.y(), origin.z()}, {dir.x(), dir.y(), dir.z()}))
        hitDistance = hit->distance;
    const double slack = std::max(m_edgesMode.size * 0.01, 0.2);
    int best = -1;
    double bestPx = 9.0;
    for (std::size_t i = 0; i < m_edgesMode.found.edges.size(); ++i) {
        const auto& e = m_edgesMode.found.edges[i];
        if (!edges::suits(kind, e.bend))
            continue;
        for (std::size_t k = 0; k + 1 < e.polyline.size(); ++k) {
            const QVector3D a(e.polyline[k][0], e.polyline[k][1], e.polyline[k][2]);
            const QVector3D b(e.polyline[k + 1][0], e.polyline[k + 1][1], e.polyline[k + 1][2]);
            const auto pa = m_viewport->projectToScreen(a), pb = m_viewport->projectToScreen(b);
            if (!pa || !pb)
                continue;
            const QPointF ab = *pb - *pa, ap = pixel - *pa;
            const double l2 = QPointF::dotProduct(ab, ab);
            const double t = l2 > 0 ? std::clamp(QPointF::dotProduct(ap, ab) / l2, 0.0, 1.0) : 0.0;
            const QPointF d = ap - ab * t;
            const double px = std::hypot(d.x(), d.y());
            if (px >= bestPx)
                continue;
            // Hidden behind the shape?
            const QVector3D p = a + (b - a) * float(t);
            const double along = QVector3D::dotProduct(p - origin, dir);
            if (hitDistance && along > *hitDistance + slack)
                continue;
            bestPx = px;
            best = int(i);
        }
    }
    if (best != m_edgesMode.hover) {
        m_edgesMode.hover = best;
        rebuildEdgeHighlights();
        refreshScene();
    }
}

void MainWindow::edgesClick(const QVector3D& origin, const QVector3D& direction)
{
    const QVector3D dir = direction.normalized();
    const auto hit = m_edgesMode.finder->under({origin.x(), origin.y(), origin.z()}, {dir.x(), dir.y(), dir.z()});
    const EdgeFinishKind kind = m_edgesPanel->kind();
    auto& picks = m_edgesMode.picks;
    const auto pickedAt = [&picks](int e) {
        return std::find_if(picks.begin(), picks.end(), [e](const auto& p) { return p.edge == e; });
    };
    const auto pick = [&](int e, int face) {
        picks.push_back({e, edges::refFor(m_edgesMode.shape, m_edgesMode.found, e, face)});
    };
    try {
        if (const int e = m_edgesMode.hover; e >= 0) {
            // An edge: picked from the face clicked next to it, if that's one of its faces.
            if (const auto it = pickedAt(e); it != picks.end()) {
                picks.erase(it);
            } else {
                const auto& faces = m_edgesMode.found.edges[std::size_t(e)].faces;
                const int face = hit && (hit->face == faces[0] || hit->face == faces[1]) ? hit->face : -1;
                pick(e, face);
            }
        } else if (hit) {
            // A face: all its edges this kind can go on (or, if all are picked, none).
            std::vector<int> suitable;
            for (int e : m_edgesMode.found.faceEdges[std::size_t(hit->face)])
                if (edges::suits(kind, m_edgesMode.found.edges[std::size_t(e)].bend))
                    suitable.push_back(e);
            if (suitable.empty()) {
                statusBar()->showMessage(tr("That face has no edges this can go on."), 4000);
                return;
            }
            const bool allPicked = std::all_of(suitable.begin(), suitable.end(),
                                               [&](int e) { return pickedAt(e) != picks.end(); });
            for (int e : suitable) {
                const auto it = pickedAt(e);
                if (allPicked)
                    picks.erase(it);
                else if (it == picks.end())
                    pick(e, hit->face);
            }
        } else {
            return;
        }
    } catch (const std::exception& e) {
        qWarning() << "Picking an edge failed:" << e.what();
        return;
    }
    rebuildEdgeHighlights();
    m_edgesTimer->start(0);
}

void MainWindow::edgesKindChanged()
{
    // Drop picks this kind can't go on (inside corners when rounding, say).
    const EdgeFinishKind kind = m_edgesPanel->kind();
    auto& picks = m_edgesMode.picks;
    const auto before = picks.size();
    picks.erase(std::remove_if(picks.begin(), picks.end(),
                               [&](const auto& p) {
                                   return !edges::suits(kind, m_edgesMode.found.edges[std::size_t(p.edge)].bend);
                               }),
                picks.end());
    if (picks.size() != before)
        statusBar()->showMessage(before - picks.size() == 1
                                     ? tr("1 picked edge dropped: it doesn't suit this kind.")
                                     : tr("%1 picked edges dropped: they don't suit this kind.")
                                           .arg(int(before - picks.size())),
                                 5000);
    m_edgesMode.hover = -1;
    rebuildEdgeHighlights();
    m_edgesTimer->start(0);
}

void MainWindow::pickAllEdges()
{
    const EdgeFinishKind kind = m_edgesPanel->kind();
    try {
        for (std::size_t i = 0; i < m_edgesMode.found.edges.size(); ++i) {
            if (!edges::suits(kind, m_edgesMode.found.edges[i].bend))
                continue;
            if (std::any_of(m_edgesMode.picks.begin(), m_edgesMode.picks.end(),
                            [i](const auto& p) { return p.edge == int(i); }))
                continue;
            m_edgesMode.picks.push_back({int(i), edges::refFor(m_edgesMode.shape, m_edgesMode.found, int(i), -1)});
        }
    } catch (const std::exception& e) {
        qWarning() << "Picking all edges failed:" << e.what();
    }
    if (m_edgesMode.picks.empty())
        statusBar()->showMessage(tr("This shape has no edges this kind can go on."), 5000);
    rebuildEdgeHighlights();
    m_edgesTimer->start(0);
}

void MainWindow::applyEdges()
{
    if (!m_edgesMode.active)
        return;
    std::optional<BooleanSpec> spec;
    try {
        spec = currentEdgesSpec();
        if (!spec)
            return;
        m_doc.previewMesh(*spec);  // fails here (with its message) rather than in the command
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("The edges could not be changed:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    const ObjectId target = m_edgesMode.target;
    if (m_edgesMode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endEdges();
        const Body before = *now;
        Body after = before;
        after.boolean = *spec;
        endEdges();
        run(std::make_unique<EditBodyCommand>(before, after, "Change edges"));
        setSelection(target);
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    static const char* names[] = {"Rounded", "Filleted", "Chamfered", "Bevelled"};
    result.name = m_doc.uniqueName(names[int(spec->edges->kind)]);
    result.boolean = *spec;
    const ObjectId id = result.id;
    endEdges();
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

bool MainWindow::toolOpen() const
{
    return m_splitMode.active || m_fitMode.active || m_embossMode.active || m_edgesMode.active || m_sketchMode.picking ||
           m_sketchMode.active || m_holeMode.active || m_shellMode.active || m_followMode.active ||
           m_patternMode.active || m_sculptMode.active || m_measureMode.active || m_threadMode.active ||
           m_socketMode.active || m_joinMode.active || m_maskMode.active || m_referenceMode.active ||
           m_hingeMode.active || m_poseMode.active || m_printCheckMode.active || m_mannequinOpen ||
           m_hollowMode.active || m_stampMode.active || m_panelLineMode.active || m_poseFigureMode.active ||
           m_paintMode.active || m_cutPartsMode.active || m_mouldMode.active || m_portMode.active ||
           m_handMode.active;
}

void MainWindow::startEmboss(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    if (m_splitMode.active)
        endSplit();

    EmbossMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    try {
        const ObjectId surface = editing && body->isBoolean() ? body->boolean->operands.front() : target;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        mode.wordsOnly = meshkernel::triangleCount(m_doc.meshSolid(surface)) > kQuickEmbossTriangles;
        QApplication::restoreOverrideCursor();
    } catch (const std::exception&) {
        QApplication::restoreOverrideCursor();
    }
    EmbossText words = m_lastWords;
    EmbossPicture picture = m_lastPicture;
    bool usePicture = m_lastUsedPicture;
    try {
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Emboss ||
                (!body->boolean->emboss && !body->boolean->picture))
                return;
            usePicture = body->boolean->picture.has_value();
            if (usePicture)
                picture = *body->boolean->picture;
            else
                words = *body->boolean->emboss;
            mode.spot = embossSpot(m_doc, target);
        } else {
            // Letters to suit the shape: about a fifth of its middle size; a picture about
            // half of it.
            auto size = m_doc.worldBounds(target).size();
            std::sort(size.begin(), size.end());
            words.letterHeight = std::clamp(std::round(double(size[1]) * 0.2 * 2.0) / 2.0, 2.0, 20.0);
            picture.width = std::clamp(std::round(double(size[1]) * 0.5), 5.0, 200.0);
            // Start on the side facing you, in the middle.
            const QVector3D eye = m_viewport->camera().eyePosition();
            const auto c = m_doc.worldTransform(target).apply(m_doc.visibleCentre(target));
            const Vec3 from{eye.x(), eye.y(), eye.z()};
            mode.spot = surfaceUnder(m_doc, target, from, {c[0] - from[0], c[1] - from[1], c[2] - from[2]},
                                     words.letterHeight / 2.0);
            // ...in the middle of that face.
            if (mode.spot)
                if (const auto middle = usePicture
                                            ? centeredSpot(m_doc, target, *mode.spot, picture, emboss::Center::Both)
                                            : centeredSpot(m_doc, target, *mode.spot, words, emboss::Center::Both))
                    mode.spot = middle;
        }
    } catch (const std::exception& e) {
        qWarning() << "Emboss could not start:" << e.what();
        if (editing)
            return;
    }
    m_embossMode = mode;
    const QString name = QString::fromStdString(body->name);
    m_embossPanel->start(editing ? (usePicture ? tr("Change the picture on '%1'") : tr("Change the words on '%1'"))
                                         .arg(name)
                                 : tr("Emboss '%1'").arg(name),
                         editing, words, picture, usePicture);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_embossPanel->show();
    statusBar()->showMessage(tr("Emboss: type the words (or choose a picture) in the panel on the right and click "
                                "the shape where they go, then click %1. Esc cancels.")
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
    if (m_embossPanel->usingPicture()) {
        const EmbossPicture picture = m_embossPanel->picture();
        if (!picture.image)
            throw GeometryError(tr("Choose a picture (Choose a picture...).").toStdString());
        if (m_embossMode.editing)
            return withEmboss(m_doc, m_embossMode.target, *m_embossMode.spot, picture).boolean;
        return embossSpec(m_doc, m_embossMode.target, *m_embossMode.spot, picture);
    }
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
        if (m_embossPanel->usingPicture() && !m_embossPanel->picture().image) {
            m_embossPanel->setStatus(tr("Choose a picture: a logo, or a photo (Choose a picture...)."), false);
            m_embossPanel->setApplyEnabled(false);
            refreshScene();
            return;
        }
        const auto spec = currentEmbossSpec();
        if (!spec) {
            m_embossPanel->setStatus(m_embossPanel->usingPicture() ? tr("Click on the shape where the picture should go.")
                                                                   : tr("Click on the shape where the words should go."),
                                     false);
        } else {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            if (m_embossMode.wordsOnly) {
                m_embossMode.preview = m_doc.previewWords(*spec);
                m_embossPanel->setStatus(tr("A big shape: what you emboss is shown in orange and is cut in or joined "
                                            "on when you click %1. Click elsewhere on the shape to move it.")
                                             .arg(m_embossMode.editing ? tr("Apply") : tr("Emboss")),
                                         false);
            } else {
                m_embossMode.preview = m_doc.previewMesh(*spec);
                m_embossPanel->setStatus(
                    tr("This is how it will look. Click elsewhere on the shape to move it."), false);
            }
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
                                       m_embossPanel->usingPicture()
                                           ? std::max(0.5, m_embossPanel->picture().width / 8.0)
                                           : m_embossPanel->words().letterHeight / 2.0);
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
    m_lastUsedPicture = spec->picture.has_value();
    if (spec->picture)
        m_lastPicture = *spec->picture;
    else
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
        run(std::make_unique<EditBodyCommand>(before, after, spec->picture ? "Change picture" : "Change words"));
        setSelection(target);
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Emboss");
    result.boolean = *spec;
    const ObjectId id = result.id;
    endEmboss();
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::layFlatClick(const QVector3D& origin, const QVector3D& direction)
{
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {direction.x(), direction.y(), direction.z()};
    try {
        const auto hit = pickShown(ray);
        if (!hit) {
            statusBar()->showMessage(tr("Lay on Face: click a face of a shape (Esc cancels)."));
            return;
        }
        ObjectId id = hit->id;
        while (m_doc.parentOf(id) != kInvalidId)
            id = m_doc.parentOf(id);
        const auto face = faceUnder(m_doc, id, ray.origin, ray.direction);
        if (!face)
            return;
        const Body before = *m_doc.find(id);
        Body after = before;
        after.placement = laidOnFace(m_doc, id, face->second);
        m_layFlat->setChecked(false);
        if (!(after.placement == before.placement))
            run(std::make_unique<EditBodyCommand>(before, after, "Lay on Face"));
        setSelection(id);
    } catch (const std::exception& e) {
        qWarning() << "Lay on Face failed:" << e.what();
        m_layFlat->setChecked(false);
    }
}

void MainWindow::sitSelectedOnPlate()
{
    std::vector<ObjectId> ids;
    for (ObjectId id : m_selection)
        if (m_doc.isTopLevel(id))
            ids.push_back(id);
    if (ids.empty()) {
        statusBar()->showMessage(tr("Select the shapes to sit on the plate first."), 5000);
        return;
    }
    m_stack.beginMacro("Sit on Plate");
    try {
        for (ObjectId id : ids) {
            const Placement resting = sitOnPlate(m_doc, id);
            if (!(resting == m_doc.find(id)->placement))
                m_stack.execute(std::make_unique<SetPlacementCommand>(id, resting, /*mergeable=*/false));
        }
    } catch (const std::exception& e) {
        qWarning() << "Sit on plate failed:" << e.what();
    }
    m_stack.endMacro();
}

// ---- Sculpt -----------------------------------------------------------------------

namespace {
int detailIndex(SculptPanel::Detail detail)
{
    return detail == SculptPanel::Detail::High ? 2 : detail == SculptPanel::Detail::Low ? 0 : 1;
}

// Stored as floats, so a stroke played again lands exactly where it went live.
Vec3 asStored(const Vec3& p)
{
    return {double(float(p[0])), double(float(p[1])), double(float(p[2]))};
}
}  // namespace

void MainWindow::startSculpt(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (toolOpen() || !body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    SculptMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    mode.object = history::objectOf(m_doc, target);
    if (editing) {
        if (!body->isBoolean() || body->boolean->op != BooleanOp::Sculpt || !body->boolean->sculpt)
            return;
        mode.operand = body->boolean->operands.front();
        mode.strokes = body->boolean->sculpt->strokes;
        mode.patterns = body->boolean->sculpt->patterns;
        mode.kept = mode.strokes.size();
    } else {
        mode.operand = target;
    }
    try {
        // Strokes are kept in the part's own coordinates, so they stay on it when it moves.
        mode.toPlate = m_doc.worldTransform(mode.operand);
        mode.fromPlate = mode.toPlate.inverse();
        const Bounds b = m_doc.worldBounds(target);
        mode.size = std::max({b.size()[0], b.size()[1], b.size()[2]});
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_sculptMode = std::move(mode);
    const QString name = QString::fromStdString(m_doc.find(m_sculptMode.object)->name);
    m_sculptPanel->start(editing ? tr("Change the sculpting of '%1'").arg(name) : tr("Sculpt '%1'").arg(name),
                         std::clamp(std::round(m_sculptMode.size * 0.08 * 2) / 2, 0.5, 50.0));
    if (editing)
        m_sculptPanel->setDetail(body->boolean->sculpt->detail);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_sculptPanel->show();
    remeshSculpt();
    if (!m_sculptMode.sculptor) {
        endSculpt();
        return;
    }
    m_sculptPanel->setDetailEnabled(!editing);
    statusBar()->showMessage(editing ? tr("Sculpt: add strokes to this step. Done keeps them; Esc leaves it as it was.")
                                     : tr("Sculpt: drag over the shape to push it out (Ctrl: in, Shift: smooth). Done "
                                          "keeps it; Esc cancels."));
    refreshCommandState();
}

void MainWindow::remeshSculpt()
{
    if (!m_sculptMode.active || m_sculptMode.strokes.size() > m_sculptMode.kept)
        return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QElapsedTimer timer;
    timer.start();
    try {
        const int detail = detailIndex(m_sculptPanel->detail());
        // The step's surface, divided finely, in its own coordinates; with the strokes it
        // already has, when changing one.
        m_sculptMode.undo.clear();
        m_sculptMode.start = m_doc.sculptStart(m_sculptMode.operand, detail);
        const Bounds b = m_sculptMode.start.bounds();
        m_sculptMode.middle = {b.center()[0], b.center()[1], b.center()[2]};
        // The finest added detail: half the starting triangles' edges; Remesh starts at their size.
        const double edge = sculpt::edgeForTriangles(m_sculptMode.start, m_sculptMode.start.triangleCount());
        m_sculptPanel->setRemeshSize(std::max(0.01, std::round(edge * 100.0) / 100.0));
        m_sculptPanel->setFinestDetail(std::max(0.01, std::round(edge * 0.5 * 100.0) / 100.0));
        m_sculptMode.sculptor = std::make_shared<sculpt::Sculptor>(m_sculptMode.start);
        for (const SculptStroke& stroke : m_sculptMode.strokes)
            sculpt::replay(*m_sculptMode.sculptor, stroke, m_sculptMode.start, m_sculptMode.patterns);
        if (!m_sculptMode.strokes.empty())
            m_sculptPanel->setFinestDetail(m_sculptMode.strokes.back().finest > 0.0 ? m_sculptMode.strokes.back().finest
                                                                                     : m_sculptPanel->finestDetail());
        m_sculptMode.sculptor->setDetail(m_sculptPanel->addDetail() ? m_sculptPanel->finestDetail() : 0.0);
        m_sculptMode.topology = m_sculptMode.sculptor->topologyVersion();
        m_sculptMode.display =
            std::shared_ptr<const Mesh>(m_sculptMode.sculptor, &m_sculptMode.sculptor->mesh());
        m_sculptPanel->setStatus(tr("%1 triangles. Drag over the shape to sculpt.")
                                     .arg(QLocale().toString(qulonglong(m_sculptMode.sculptor->mesh().triangleCount()))),
                                 false);
        qInfo().noquote() << "Sculpt: mesh of" << m_sculptMode.sculptor->mesh().triangleCount() << "triangles in"
                          << timer.elapsed() << "ms";
    } catch (const std::exception& e) {
        m_sculptMode.sculptor.reset();
        m_sculptMode.display.reset();
        QApplication::restoreOverrideCursor();
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        refreshScene();
        return;
    }
    QApplication::restoreOverrideCursor();
    refreshScene();
}

std::optional<sculpt::Hit> MainWindow::sculptHit(const QVector3D& origin, const QVector3D& direction) const
{
    if (!m_sculptMode.sculptor)
        return std::nullopt;
    // Sculpting happens in the part's own coordinates.
    return m_sculptMode.sculptor->hit(m_sculptMode.fromPlate.apply({origin.x(), origin.y(), origin.z()}),
                                      m_sculptMode.fromPlate.applyDirection({direction.x(), direction.y(), direction.z()}));
}

void MainWindow::showBrushRing(const sculpt::Hit& hit, double radius)
{
    const Vec3 p = m_sculptMode.toPlate.apply(hit.point);
    const Vec3 n = m_sculptMode.toPlate.applyDirection(hit.normal);
    m_viewport->setBrushRing(Viewport::BrushRing{QVector3D(float(p[0]), float(p[1]), float(p[2])),
                                                 QVector3D(float(n[0]), float(n[1]), float(n[2])), float(radius)});
}

void MainWindow::sculptHover(const QVector3D& origin, const QVector3D& direction)
{
    const auto hit = sculptHit(origin, direction);
    if (!hit) {
        m_viewport->setBrushRing(std::nullopt);
        return;
    }
    showBrushRing(*hit, m_sculptPanel->size());
}

SculptStroke MainWindow::newStroke(int brush, bool invert) const
{
    SculptStroke s;
    s.brush = brush;
    s.invert = invert;
    s.radius = m_sculptPanel->size();
    s.strength = m_sculptPanel->strength();
    s.mirrorX = m_sculptPanel->mirrorX();
    s.mirrorY = m_sculptPanel->mirrorY();
    s.mirrorZ = m_sculptPanel->mirrorZ();
    s.finest = m_sculptPanel->addDetail() ? m_sculptPanel->finestDetail() : 0.0;
    return s;
}

void MainWindow::beginStroke(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers)
{
    if (!m_sculptMode.sculptor)
        return;
    const auto hit = sculptHit(origin, direction);
    if (!hit)
        return;  // pressed off the shape: nothing (the view turns with the right button)
    // Before the stroke, for Undo stroke (the last 8; whole meshes, as strokes can add
    // triangles), and how many strokes were kept then.
    m_sculptMode.undo.push_back({m_sculptMode.sculptor->mesh(), m_sculptMode.strokes.size()});
    if (m_sculptMode.undo.size() > 8)
        m_sculptMode.undo.erase(m_sculptMode.undo.begin());
    m_sculptMode.stroking = true;
    m_sculptMode.lastDab.reset();
    m_sculptMode.recording = -1;
    m_sculptPanel->setDetailEnabled(false);
    m_sculptPanel->setUndoEnabled(true);
    if (m_sculptPanel->brush() == int(sculpt::Brush::SnakeHook) && !modifiers.testFlag(Qt::ShiftModifier)) {
        // Snake hook: the tip follows the mouse across planes facing the view.
        SculptStroke stroke = newStroke(int(sculpt::Brush::SnakeHook), false);
        const Vec3 from = asStored(hit->point);
        stroke.dabs.push_back({float(from[0]), float(from[1]), float(from[2])});
        m_sculptMode.sculptor->setDetail(stroke.finest);
        m_sculptMode.strokes.push_back(stroke);
        m_sculptMode.snaking = true;
        m_sculptMode.grabFacing = m_sculptMode.fromPlate.applyDirection({direction.x(), direction.y(), direction.z()});
        // As replay does it: from the nearest point of the surface, the tips moved to match.
        const Vec3 start = m_sculptMode.sculptor->onSurface(from);
        m_sculptMode.snakeShift = {start[0] - from[0], start[1] - from[1], start[2] - from[2]};
        m_sculptMode.sculptor->beginSnake(start, stroke.radius, stroke.strength, m_sculptMode.sculptor->triangleAt(start),
                                          sculpt::symmetryFor(stroke, m_sculptMode.start));
        return;
    }
    if (m_sculptPanel->brush() == int(sculpt::Brush::Grab) && !modifiers.testFlag(Qt::ShiftModifier)) {
        // Grab: the points under the brush follow the mouse across a plane facing the view.
        SculptStroke stroke = newStroke(int(sculpt::Brush::Grab), false);
        const Vec3 from = asStored(hit->point);
        stroke.dabs.push_back({float(from[0]), float(from[1]), float(from[2])});
        m_sculptMode.sculptor->setDetail(stroke.finest);
        m_sculptMode.strokes.push_back(stroke);
        m_sculptMode.grabbing = true;
        m_sculptMode.grabFrom = from;
        m_sculptMode.grabFacing = m_sculptMode.fromPlate.applyDirection({direction.x(), direction.y(), direction.z()});
        const Vec3 start = m_sculptMode.sculptor->onSurface(from);  // as replay does it
        m_sculptMode.sculptor->beginGrab(start, stroke.radius, m_sculptMode.sculptor->triangleAt(start),
                                         sculpt::symmetryFor(stroke, m_sculptMode.start));
        return;
    }
    sculptAt(origin, direction, modifiers);
}

void MainWindow::sculptAt(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers)
{
    if (m_sculptMode.grabbing) {
        // Where the ray crosses the plane through the grab point, facing the view.
        const Vec3& n = m_sculptMode.grabFacing;
        const Vec3 o = m_sculptMode.fromPlate.apply({origin.x(), origin.y(), origin.z()});
        const Vec3 d = m_sculptMode.fromPlate.applyDirection({direction.x(), direction.y(), direction.z()});
        const double along = n[0] * d[0] + n[1] * d[1] + n[2] * d[2];
        if (std::abs(along) < 1e-9)
            return;
        const Vec3& f = m_sculptMode.grabFrom;
        const double t = (n[0] * (f[0] - o[0]) + n[1] * (f[1] - o[1]) + n[2] * (f[2] - o[2])) / along;
        const Vec3 offset = asStored({o[0] + d[0] * t - f[0], o[1] + d[1] * t - f[1], o[2] + d[2] * t - f[2]});
        const auto [first, last] = m_sculptMode.sculptor->moveGrab(offset);
        m_sculptMode.strokes.back().grab = {float(offset[0]), float(offset[1]), float(offset[2])};
        sculptShown(first, last);
        m_sculptMode.dabs++;
        return;
    }
    if (m_sculptMode.snaking) {
        // The next tip: where the ray crosses a plane through the last one, facing the view.
        const Vec3& n = m_sculptMode.grabFacing;
        const Vec3 o = m_sculptMode.fromPlate.apply({origin.x(), origin.y(), origin.z()});
        const Vec3 d = m_sculptMode.fromPlate.applyDirection({direction.x(), direction.y(), direction.z()});
        const double along = n[0] * d[0] + n[1] * d[1] + n[2] * d[2];
        if (std::abs(along) < 1e-9)
            return;
        SculptStroke& stroke = m_sculptMode.strokes.back();
        const Vec3 f{stroke.dabs.back()[0], stroke.dabs.back()[1], stroke.dabs.back()[2]};
        const double t = (n[0] * (f[0] - o[0]) + n[1] * (f[1] - o[1]) + n[2] * (f[2] - o[2])) / along;
        const Vec3 to = asStored({o[0] + d[0] * t, o[1] + d[1] * t, o[2] + d[2] * t});
        if (std::hypot(to[0] - f[0], to[1] - f[1], to[2] - f[2]) < stroke.radius * 0.15)
            return;  // steps about a seventh of the brush, so the pull is even
        stroke.dabs.push_back({float(to[0]), float(to[1]), float(to[2])});
        QElapsedTimer timer;
        timer.start();
        const Vec3& shift = m_sculptMode.snakeShift;
        const auto [first, last] =
            m_sculptMode.sculptor->moveSnake({to[0] + shift[0], to[1] + shift[1], to[2] + shift[2]});
        m_sculptMode.dabs++;
        m_sculptMode.dabMs += timer.nsecsElapsed() / 1e6;
        sculptShown(first, last);
        return;
    }
    const auto hit = sculptHit(origin, direction);
    if (!hit)
        return;
    const double radius = m_sculptPanel->size();
    // Dabs about a fifth of the brush apart, so a stroke is even however fast it goes.
    if (m_sculptMode.lastDab) {
        const Vec3& last = *m_sculptMode.lastDab;
        const double moved = std::hypot(hit->point[0] - last[0], hit->point[1] - last[1], hit->point[2] - last[2]);
        if (moved < radius * 0.2)
            return;
    }
    int brush = modifiers.testFlag(Qt::ShiftModifier) ? int(sculpt::Brush::Smooth) : m_sculptPanel->brush();
    if (brush == int(sculpt::Brush::Grab) || brush == int(sculpt::Brush::SnakeHook) || brush < 0 ||
        (brush > int(sculpt::Brush::Mask) && brush != int(sculpt::Brush::Pattern)))
        brush = int(sculpt::Brush::Smooth);
    const bool invert = modifiers.testFlag(Qt::ControlModifier);
    // A stroke is kept as dabs of one brush; holding Shift or Ctrl part way starts another.
    if (m_sculptMode.recording < 0 || m_sculptMode.strokes.back().brush != brush ||
        m_sculptMode.strokes.back().invert != invert) {
        SculptStroke next = newStroke(brush, invert);
        m_sculptMode.sculptor->endPattern();
        if (brush == int(sculpt::Brush::Pattern)) {
            // The pattern goes in the step's list (once), and is pressed in until release.
            const SculptPattern chosen = m_sculptPanel->pattern();
            auto found = std::find(m_sculptMode.patterns.begin(), m_sculptMode.patterns.end(), chosen);
            if (found == m_sculptMode.patterns.end())
                found = m_sculptMode.patterns.insert(m_sculptMode.patterns.end(), chosen);
            next.pattern = int(found - m_sculptMode.patterns.begin());
            next.tile = m_sculptPanel->patternSize();
            next.turn = m_sculptPanel->patternTurn();
            next.once = m_sculptPanel->patternOnce();
            try {
                const auto image = sculpt::patternImage(chosen);
                if (next.once)
                    next.radius = sculpt::stampRadius(*image, next.tile);  // reaches the picture's corners
                m_sculptMode.sculptor->beginPattern(image, next.tile, sculpt::patternDepth(next), next.turn,
                                                    next.once);
            } catch (const std::exception& e) {
                statusBar()->showMessage(QString::fromUtf8(e.what()), 6000);
                return;
            }
        }
        m_sculptMode.strokes.push_back(next);
        m_sculptMode.recording = int(m_sculptMode.strokes.size()) - 1;
        m_sculptMode.sculptor->setDetail(m_sculptMode.strokes.back().finest);
    }
    SculptStroke& stroke = m_sculptMode.strokes.back();
    if (stroke.once && !stroke.dabs.empty())
        return;  // a stamp: pressed once, where the click was
    const Vec3 centre = asStored(hit->point);
    stroke.dabs.push_back({float(centre[0]), float(centre[1]), float(centre[2])});
    sculpt::Dab dab;
    dab.centre = m_sculptMode.sculptor->onSurface(centre);  // as replay does it
    dab.radius = stroke.radius;
    dab.strength = stroke.strength;
    dab.brush = sculpt::Brush(stroke.brush);
    dab.invert = stroke.invert;
    QElapsedTimer timer;
    timer.start();
    // Started from the triangle nearest the dab, as when the stroke is played again.
    const auto [first, last] = m_sculptMode.sculptor->apply(dab, m_sculptMode.sculptor->triangleAt(dab.centre),
                                                            sculpt::symmetryFor(stroke, m_sculptMode.start));
    m_sculptMode.lastDab = hit->point;
    m_sculptMode.dabs++;
    m_sculptMode.dabMs += timer.nsecsElapsed() / 1e6;
    sculptShown(first, last);
    showBrushRing(*hit, radius);
}

void MainWindow::sculptShown(std::uint32_t, std::uint32_t)
{
    // Only what the sculptor changed goes to the graphics card (it keeps count).
    const auto changes = m_sculptMode.sculptor->takeChanges();
    Viewport::MeshChanges shown;
    shown.points = changes.points;
    shown.triangles = changes.triangles;
    shown.everything = changes.everything;
    m_viewport->meshChanged(m_sculptMode.object, shown);
    const auto version = m_sculptMode.sculptor->topologyVersion();
    if (version != m_sculptMode.topology) {
        m_sculptMode.topology = version;
        m_sculptPanel->setStatus(tr("%1 triangles.").arg(QLocale().toString(
                                     qulonglong(m_sculptMode.sculptor->mesh().triangleCount()))),
                                 false);
    }
}

void MainWindow::evenOutSculpt()
{
    if (!m_sculptMode.active || !m_sculptMode.sculptor || m_sculptMode.stroking)
        return;
    SculptStroke stroke = newStroke(int(sculpt::Brush::Remesh), false);
    stroke.radius = m_sculptPanel->remeshSize();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QElapsedTimer timer;
    timer.start();
    m_sculptMode.undo.push_back({m_sculptMode.sculptor->mesh(), m_sculptMode.strokes.size()});
    if (m_sculptMode.undo.size() > 8)
        m_sculptMode.undo.erase(m_sculptMode.undo.begin());
    try {
        m_sculptMode.sculptor->remesh(stroke.radius);
    } catch (const std::exception& e) {
        m_sculptMode.undo.pop_back();
        QApplication::restoreOverrideCursor();
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_sculptMode.strokes.push_back(stroke);
    QApplication::restoreOverrideCursor();
    qInfo().noquote() << "Sculpt: remeshed to" << m_sculptMode.sculptor->mesh().triangleCount() << "triangles in"
                      << timer.elapsed() << "ms";
    sculptShown(0, 0);
    m_sculptPanel->setDetailEnabled(false);
    m_sculptPanel->setUndoEnabled(true);
}

void MainWindow::wholeMask(int brush)
{
    if (!m_sculptMode.active || !m_sculptMode.sculptor || m_sculptMode.stroking)
        return;
    if (brush == int(sculpt::Brush::MaskClear) && m_sculptMode.sculptor->mesh().shade.empty())
        return;  // no mask to clear
    m_sculptMode.undo.push_back({m_sculptMode.sculptor->mesh(), m_sculptMode.strokes.size()});
    if (m_sculptMode.undo.size() > 8)
        m_sculptMode.undo.erase(m_sculptMode.undo.begin());
    m_sculptMode.strokes.push_back(newStroke(brush, false));
    const auto [first, last] = brush == int(sculpt::Brush::MaskClear) ? m_sculptMode.sculptor->clearMask()
                                                                      : m_sculptMode.sculptor->invertMask();
    sculptShown(first, last);
    m_sculptPanel->setDetailEnabled(false);
    m_sculptPanel->setUndoEnabled(true);
}

void MainWindow::undoStroke()
{
    if (!m_sculptMode.active || m_sculptMode.undo.empty())
        return;
    auto& [mesh, strokes] = m_sculptMode.undo.back();
    m_sculptMode.sculptor->setMesh(std::move(mesh));
    m_sculptMode.strokes.resize(strokes);
    m_sculptMode.undo.pop_back();
    sculptShown(0, 0);
    m_sculptPanel->setUndoEnabled(!m_sculptMode.undo.empty());
    m_sculptPanel->setDetailEnabled(!m_sculptMode.editing && m_sculptMode.strokes.empty());
}

void MainWindow::finishSculpt()
{
    if (!m_sculptMode.active || !m_sculptMode.sculptor)
        return;
    // Strokes that did nothing (a Grab that never moved) are left out.
    const std::size_t erased = std::erase_if(m_sculptMode.strokes, [](const SculptStroke& s) {
        if (s.brush == int(sculpt::Brush::MaskClear) || s.brush == int(sculpt::Brush::MaskInvert) ||
            s.brush == int(sculpt::Brush::Remesh))
            return false;
        return s.dabs.empty() || (s.brush == int(sculpt::Brush::Grab) && s.grab == std::array<float, 3>{0, 0, 0}) ||
               (s.brush == int(sculpt::Brush::SnakeHook) && s.dabs.size() < 2);
    });
    if (m_sculptMode.strokes.size() <= m_sculptMode.kept) {
        endSculpt();  // nothing new sculpted: the shape stays as it was
        return;
    }
    SculptFinish finish;
    finish.detail = detailIndex(m_sculptPanel->detail());
    finish.strokes = m_sculptMode.strokes;
    // Only the patterns the strokes use, numbered afresh.
    {
        std::vector<int> renumber(m_sculptMode.patterns.size(), -1);
        for (SculptStroke& st : finish.strokes)
            if (st.pattern >= 0 && st.pattern < int(renumber.size())) {
                if (renumber[std::size_t(st.pattern)] < 0) {
                    renumber[std::size_t(st.pattern)] = int(finish.patterns.size());
                    finish.patterns.push_back(m_sculptMode.patterns[std::size_t(st.pattern)]);
                }
                st.pattern = renumber[std::size_t(st.pattern)];
            }
    }
    // What the strokes made is kept, so the step isn't worked out again from the start
    // (with none left out above, it is exactly what playing them again gives).
    if (erased == 0)
        m_doc.rememberSculpt(m_sculptMode.start, finish, m_sculptMode.sculptor->mesh());
    qInfo().noquote() << "Sculpt: done," << m_sculptMode.dabs << "dabs, average"
                      << (m_sculptMode.dabs ? m_sculptMode.dabMs / m_sculptMode.dabs : 0.0) << "ms each";
    const ObjectId target = m_sculptMode.target;
    const bool editing = m_sculptMode.editing;
    endSculpt();
    if (editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return;
        Body after = *now;
        after.boolean->sculpt = finish;
        run(std::make_unique<EditBodyCommand>(*now, after, "Change sculpting"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    BooleanSpec spec;
    spec.op = BooleanOp::Sculpt;
    spec.operands = {target};
    spec.sculpt = finish;
    {
        const ObjectId holder = m_doc.parentOf(target);
        const Transform toStep = holder == kInvalidId ? Transform{} : m_doc.worldTransform(holder).inverse();
        const auto c = m_doc.worldBounds(target).center();
        const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
        spec.pivot = {centre[0], centre[1], centre[2]};
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Sculpted");
    result.boolean = spec;
    const ObjectId id = result.id;
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

// ---- Measure ----------------------------------------------------------------------

namespace {
QString mm(double v)
{
    return QString::number(v, 'f', 2);
}

QString aboutText(const measure::About& a)
{
    using K = measure::About::Kind;
    switch (a.kind) {
    case K::Point:
        return MainWindow::tr("Point at X %1, Y %2, Z %3 mm").arg(mm(a.point[0]), mm(a.point[1]), mm(a.point[2]));
    case K::Line: return MainWindow::tr("Straight edge, <b>%1 mm</b> long").arg(mm(a.length));
    case K::Circle:
        return MainWindow::tr("Round edge: <b>Ø %1 mm</b> (radius %2), %3 mm round")
            .arg(mm(2 * a.radius), mm(a.radius), mm(a.length));
    case K::Arc:
        return MainWindow::tr("Curved edge (part of a circle): <b>radius %1 mm</b>, %2 mm long")
            .arg(mm(a.radius), mm(a.length));
    case K::Curve: return MainWindow::tr("Curved edge, <b>%1 mm</b> long").arg(mm(a.length));
    case K::Flat: return MainWindow::tr("Flat face: <b>%1 mm²</b>").arg(mm(a.area));
    case K::Cylinder:
        return MainWindow::tr("Round face (a cylinder or hole): <b>Ø %1 mm</b> (radius %2), %3 mm²")
            .arg(mm(2 * a.radius), mm(a.radius), mm(a.area));
    case K::Cone:
        return MainWindow::tr("Cone-shaped face: <b>%1°</b> each side of its middle, %2 mm²")
            .arg(QString::number(a.halfAngle, 'f', 1), mm(a.area));
    case K::Sphere:
        return MainWindow::tr("Ball-shaped face: <b>Ø %1 mm</b> (radius %2), %3 mm²")
            .arg(mm(2 * a.radius), mm(a.radius), mm(a.area));
    case K::Torus:
        return MainWindow::tr("Ring-shaped face: tube <b>Ø %1 mm</b>, ring radius %2 mm, %3 mm²")
            .arg(mm(2 * a.radius), mm(a.bigRadius), mm(a.area));
    case K::Surface: return MainWindow::tr("Curved face: <b>%1 mm²</b>").arg(mm(a.area));
    }
    return {};
}

// A small cross, to show a point picked.
std::shared_ptr<const Mesh> crossAt(const Vec3& p, float size, float radius)
{
    std::vector<std::vector<Vec3>> arms;
    for (int axis = 0; axis < 3; ++axis) {
        Vec3 a = p, b = p;
        a[axis] -= size;
        b[axis] += size;
        arms.push_back({a, b});
    }
    std::vector<const std::vector<Vec3>*> lines;
    for (const auto& arm : arms)
        lines.push_back(&arm);
    return tubes(lines, radius);
}

// Several meshes as one.
std::shared_ptr<const Mesh> joined(const std::vector<std::shared_ptr<const Mesh>>& meshes)
{
    auto out = std::make_shared<Mesh>();
    for (const auto& m : meshes) {
        if (!m)
            continue;
        const auto base = std::uint32_t(out->positions.size() / 3);
        out->positions.insert(out->positions.end(), m->positions.begin(), m->positions.end());
        out->normals.insert(out->normals.end(), m->normals.begin(), m->normals.end());
        for (std::uint32_t i : m->indices)
            out->indices.push_back(base + i);
    }
    return out;
}
}  // namespace

void MainWindow::startMeasure()
{
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    m_measureMode = MeasureMode{};
    m_measureMode.active = true;
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_measurePanel->show();
    showMeasurements();
    statusBar()->showMessage(tr("Measure: click a face, an edge or a corner (Shift: an exact point). Esc closes."));
    refreshScene();
    refreshCommandState();
}

void MainWindow::endMeasure()
{
    if (!m_measureMode.active)
        return;
    m_measureMode = MeasureMode{};
    m_measureLabels.clear();
    showLabels();
    m_measurePanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

std::optional<MainWindow::MeasureMode::Thing> MainWindow::measureThingAt(const QVector3D& origin,
                                                                         const QVector3D& direction,
                                                                         const QPointF& pixel)
{
    const QVector3D dir = direction.normalized();
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {dir.x(), dir.y(), dir.z()};
    const auto hit = pickShown(ray);
    if (!hit)
        return std::nullopt;
    MeasureMode::Thing thing;
    thing.object = hit->id;
    thing.target = {measure::Target::Kind::Point, {}, hit->point};
    const float size = std::max(0.05f, m_viewport->camera().distance() * 0.012f);
    const float radius = std::max(0.01f, m_viewport->camera().distance() * 0.0018f);
    const auto asPoint = [&](const Vec3& p) {
        thing.target = {measure::Target::Kind::Point, {}, p};
        thing.index = -1;
        thing.mesh = crossAt(p, size, radius);
        return thing;
    };
    if (QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier) || m_doc.usesMeshEngine(hit->id))
        return asPoint(hit->point);  // an exact point (meshes have no faces or edges to pick)

    // The solid, ready for picking on (worked out once while measuring).
    std::shared_ptr<measure::Shape> shape;
    if (const auto found = m_measureMode.shapes.find(hit->id); found != m_measureMode.shapes.end()) {
        shape = found->second;
    } else {
        try {
            shape = std::make_shared<measure::Shape>(pickableShape(m_doc, hit->id));
        } catch (const std::exception& e) {
            qWarning() << "Measure: no faces to pick:" << e.what();
        }
        m_measureMode.shapes[hit->id] = shape;
    }
    if (!shape)
        return asPoint(hit->point);
    const auto onTop = shape->faceUnder({origin.x(), origin.y(), origin.z()}, {dir.x(), dir.y(), dir.z()});
    const double slack = std::max(shape->size() * 0.01, 0.2);
    const auto visible = [&](const QVector3D& p) {
        return !onTop || QVector3D::dotProduct(p - origin, dir) <= onTop->distance + slack;
    };
    // A corner within a few pixels.
    double bestPx = 10.0;
    std::optional<Vec3> corner;
    for (const Vec3& c : shape->corners()) {
        const QVector3D p{float(c[0]), float(c[1]), float(c[2])};
        const auto s = m_viewport->projectToScreen(p);
        if (!s)
            continue;
        const double px = std::hypot(s->x() - pixel.x(), s->y() - pixel.y());
        if (px < bestPx && visible(p)) {
            bestPx = px;
            corner = c;
        }
    }
    if (corner)
        return asPoint(*corner);
    // An edge within a few pixels.
    int bestEdge = -1;
    bestPx = 8.0;
    Vec3 onEdge{0, 0, 0};
    const auto& lines = shape->edgeLines();
    for (std::size_t i = 0; i < lines.size(); ++i)
        for (std::size_t k = 0; k + 1 < lines[i].size(); ++k) {
            const QVector3D a(lines[i][k][0], lines[i][k][1], lines[i][k][2]);
            const QVector3D b(lines[i][k + 1][0], lines[i][k + 1][1], lines[i][k + 1][2]);
            const auto pa = m_viewport->projectToScreen(a), pb = m_viewport->projectToScreen(b);
            if (!pa || !pb)
                continue;
            const QPointF ab = *pb - *pa, ap = pixel - *pa;
            const double l2 = QPointF::dotProduct(ab, ab);
            const double t = l2 > 0 ? std::clamp(QPointF::dotProduct(ap, ab) / l2, 0.0, 1.0) : 0.0;
            const QPointF d = ap - ab * t;
            const double px = std::hypot(d.x(), d.y());
            const QVector3D p = a + (b - a) * float(t);
            if (px >= bestPx || !visible(p))
                continue;
            bestPx = px;
            bestEdge = int(i);
            onEdge = {p.x(), p.y(), p.z()};
        }
    if (bestEdge >= 0) {
        thing.target = shape->edge(bestEdge, onEdge);
        thing.index = bestEdge;
        thing.mesh = tubes({&lines[std::size_t(bestEdge)]}, radius * 1.4f);
        return thing;
    }
    if (!onTop)
        return asPoint(hit->point);
    thing.target = shape->face(onTop->face, onTop->point);
    thing.index = onTop->face;
    // The same face as last time: keep its mesh (big faces are slow to divide again).
    if (m_measureMode.hover && m_measureMode.hover->object == thing.object &&
        m_measureMode.hover->target.kind == measure::Target::Kind::Face && m_measureMode.hover->index == thing.index)
        thing.mesh = m_measureMode.hover->mesh;
    else
        thing.mesh = std::make_shared<const Mesh>(shape->faceMesh(onTop->face));
    return thing;
}

void MainWindow::measureHover(const QVector3D& origin, const QVector3D& direction, const QPointF& pixel)
{
    std::optional<MeasureMode::Thing> thing;
    try {
        thing = measureThingAt(origin, direction, pixel);
    } catch (const std::exception& e) {
        qWarning() << "Measure:" << e.what();
    }
    const bool same = thing && m_measureMode.hover && thing->object == m_measureMode.hover->object &&
                      thing->target.kind == m_measureMode.hover->target.kind &&
                      thing->index == m_measureMode.hover->index &&
                      (thing->index >= 0 || thing->target.point == m_measureMode.hover->target.point);
    if (same || (!thing && !m_measureMode.hover))
        return;
    m_measureMode.hover = thing;
    m_measureMode.hoverMesh = thing ? thing->mesh : nullptr;
    QString text;
    if (thing) {
        try {
            QTextDocument plain;
            plain.setHtml(aboutText(measure::describe(thing->target)));
            text = plain.toPlainText();
        } catch (const std::exception&) {
        }
    }
    m_measurePanel->setHover(text);
    refreshScene();
}

void MainWindow::measureClick(const QVector3D& origin, const QVector3D& direction)
{
    // What the mouse is over (worked out on hover; again here if it moved without one).
    std::optional<MeasureMode::Thing> thing = m_measureMode.hover;
    if (!thing) {
        const auto px = m_viewport->mapFromGlobal(QCursor::pos());
        try {
            thing = measureThingAt(origin, direction, QPointF(px));
        } catch (const std::exception& e) {
            qWarning() << "Measure:" << e.what();
        }
    }
    if (!thing) {
        statusBar()->showMessage(tr("Measure: click on a shape. Esc closes."), 4000);
        return;
    }
    if (m_measureMode.picks.size() >= 2)
        m_measureMode.picks.clear();  // a third click starts again
    m_measureMode.picks.push_back(*thing);
    showMeasurements();
}

void MainWindow::showMeasurements()
{
    auto& mode = m_measureMode;
    if (!mode.active)
        return;
    mode.lineMesh.reset();
    mode.centreMesh.reset();
    m_measureLabels.clear();
    std::vector<std::shared_ptr<const Mesh>> picked;
    for (const auto& p : mode.picks)
        picked.push_back(p.mesh);
    mode.pickedMesh = picked.empty() ? nullptr : joined(picked);
    if (!mode.hover)
        mode.hoverMesh.reset();
    if (mode.picks.empty()) {
        m_measurePanel->setResult(tr("Nothing picked yet."));
        refreshScene();
        return;
    }
    QString html;
    try {
        const measure::About first = measure::describe(mode.picks[0].target);
        if (mode.picks.size() == 1) {
            html = tr("<b>1.</b> %1<br><br>Click a second thing to measure between them.").arg(aboutText(first));
        } else {
            const measure::About second = measure::describe(mode.picks[1].target);
            const measure::Between b = measure::between(mode.picks[0].target, mode.picks[1].target);
            html = tr("<b>1.</b> %1<br><b>2.</b> %2<hr>").arg(aboutText(first), aboutText(second));
            const Vec3 d{std::abs(b.to[0] - b.from[0]), std::abs(b.to[1] - b.from[1]), std::abs(b.to[2] - b.from[2])};
            html += b.distance < 1e-6 ? tr("They <b>touch</b>.")
                                      : tr("<b>Distance: %1 mm</b> at the nearest (green)<br>"
                                           "&nbsp;&nbsp;X %2, Y %3, Z %4 mm")
                                            .arg(mm(b.distance), mm(d[0]), mm(d[1]), mm(d[2]));
            if (b.parallel) {
                html += tr("<br><b>Parallel</b>");
                if (b.apart)
                    html += tr(", <b>%1 mm</b> apart").arg(mm(*b.apart));
            } else if (b.angle) {
                html += tr("<br><b>Angle: %1°</b>").arg(QString::number(*b.angle, 'f', 1));
            }
            if (b.centres)
                html += tr("<br><b>Centre to centre: %1 mm</b> (blue)").arg(mm(*b.centres));
            if (b.centreToFace)
                html += tr("<br><b>Centre to the face: %1 mm</b> (blue)").arg(mm(*b.centreToFace));
            const float radius = std::max(0.01f, m_viewport->camera().distance() * 0.0015f);
            const auto vec = [](const Vec3& v) { return QVector3D(float(v[0]), float(v[1]), float(v[2])); };
            if (b.distance > 1e-6) {
                const std::vector<Vec3> line{b.from, b.to};
                mode.lineMesh = tubes({&line}, radius);
                m_measureLabels.push_back({vec(b.from), vec(b.to), tr("%1 mm").arg(mm(b.distance)), QColor(30, 140, 60)});
            }
            if ((b.centres && *b.centres > 1e-6) || (b.centreToFace && *b.centreToFace > 1e-6)) {
                const std::vector<Vec3> line{b.centreFrom, b.centreTo};
                mode.centreMesh = tubes({&line}, radius);
                m_measureLabels.push_back({vec(b.centreFrom), vec(b.centreTo),
                                           tr("%1 mm").arg(mm(b.centres ? *b.centres : *b.centreToFace)),
                                           QColor(40, 110, 200)});
            }
        }
    } catch (const std::exception& e) {
        html = tr("Could not measure that: %1").arg(QString::fromUtf8(e.what()).toHtmlEscaped());
    }
    m_measurePanel->setResult(html);
    showLabels();
    refreshScene();
}

void MainWindow::showLabels()
{
    std::vector<Viewport::Label> all = m_keptLabels;
    if (m_measureMode.active)
        all.insert(all.end(), m_measureLabels.begin(), m_measureLabels.end());
    m_viewport->setLabels(std::move(all));
}

std::optional<PickHit> MainWindow::pickShown(const Ray& ray) const
{
    const auto& section = m_viewport->section();
    if (!section)
        return pick(m_doc, ray);
    PickClip clip;
    clip.normal = {section->normal.x(), section->normal.y(), section->normal.z()};
    clip.offset = section->offset;
    if (section->selectedOnly) {
        for (std::uint64_t id : m_viewport->sectionShapes())
            clip.only.push_back(ObjectId(id));
        if (clip.only.empty())
            return pick(m_doc, ray);  // nothing is cut
    }
    return pick(m_doc, ray, &clip);
}

void MainWindow::simplifySelected()
{
    const ObjectId id = selectedOne();
    if (toolOpen() || id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
        statusBar()->showMessage(tr("Simplify: select one shape on the plate first."), 6000);
        return;
    }
    const ObjectId target = history::tip(m_doc, id);
    if (const QString open = openMeshIn({target}); !open.isEmpty()) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("'%1' has gaps in its surface (it isn't watertight), so it can't be simplified.")
                                     .arg(open));
        return;
    }
    try {
        // A light shape (a plain box: 12 triangles) would only lose its shape.
        const std::size_t triangles = m_doc.mesh(target)->triangleCount();
        if (triangles < 2000) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("'%1' has only %2 triangles, so simplifying it wouldn't help. Simplify is "
                                        "for big meshes: imported scans and models, or sculpted shapes.")
                                         .arg(QString::fromStdString(m_doc.find(id)->name))
                                         .arg(triangles));
            return;
        }
    } catch (const std::exception&) {
    }
    BooleanSpec spec;
    spec.op = BooleanOp::Simplify;
    spec.operands = {target};
    spec.simplify = SimplifyFinish{};
    {
        const ObjectId holder = m_doc.parentOf(target);
        const Transform toStep = holder == kInvalidId ? Transform{} : m_doc.worldTransform(holder).inverse();
        const auto c = m_doc.worldBounds(target).center();
        const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
        spec.pivot = {centre[0], centre[1], centre[2]};
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Simplified");
    result.boolean = spec;
    const ObjectId newId = result.id;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool done = false;
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        done = run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target)));
        if (done)
            setSelection(object);
    } else {
        done = run(std::make_unique<CombineCommand>(std::move(result)));
        if (done)
            setSelection(newId);
    }
    QApplication::restoreOverrideCursor();
    if (done)
        statusBar()->showMessage(tr("Simplified to half the triangles. Change how many to keep in Properties."),
                                 8000);
}

bool MainWindow::addStepOnSelected(BooleanSpec spec, const std::string& name)
{
    const ObjectId id = selectedOne();
    const ObjectId target = history::tip(m_doc, id);
    spec.operands = {target};
    {
        const ObjectId holder = m_doc.parentOf(target);
        const Transform toStep = holder == kInvalidId ? Transform{} : m_doc.worldTransform(holder).inverse();
        const auto c = m_doc.worldBounds(target).center();
        const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
        spec.pivot = {centre[0], centre[1], centre[2]};
        if (spec.op == BooleanOp::ExtrudeDown) {
            // The plate, and which way is up, as the step sees them.
            spec.planePoint = toStep.apply(Vec3{0.0, 0.0, 0.0});
            spec.planeNormal = toStep.applyDirection(Vec3{0.0, 0.0, 1.0});
        }
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName(name);
    result.boolean = spec;
    const ObjectId newId = result.id;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool done = false;
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        done = run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target)));
        if (done)
            setSelection(object);
    } else {
        done = run(std::make_unique<CombineCommand>(std::move(result)));
        if (done)
            setSelection(newId);
    }
    QApplication::restoreOverrideCursor();
    return done;
}

void MainWindow::smoothSelected()
{
    const ObjectId id = selectedOne();
    if (toolOpen() || id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
        statusBar()->showMessage(tr("Smooth: select one shape on the plate first."), 6000);
        return;
    }
    if (const QString open = openMeshIn({history::tip(m_doc, id)}); !open.isEmpty()) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("'%1' has gaps in its surface (it isn't watertight), so it can't be smoothed.")
                                     .arg(open));
        return;
    }
    BooleanSpec spec;
    spec.op = BooleanOp::Smooth;
    spec.smooth = SmoothFinish{};
    if (addStepOnSelected(spec, "Smoothed"))
        statusBar()->showMessage(tr("Smoothed. Change how strongly in Properties (1 gentle to 10 strong)."), 8000);
}

void MainWindow::extrudeDownSelected()
{
    const ObjectId id = selectedOne();
    if (toolOpen() || id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
        statusBar()->showMessage(tr("Extrude Down: select one shape on the plate first."), 6000);
        return;
    }
    if (const QString open = openMeshIn({history::tip(m_doc, id)}); !open.isEmpty()) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("'%1' has gaps in its surface (it isn't watertight), so it can't be extruded "
                                    "down.")
                                     .arg(open));
        return;
    }
    try {
        if (m_doc.worldBounds(id).min[2] < -1e-3) {
            QMessageBox::information(this, tr("Forge3D"),
                                     tr("Part of '%1' is below the plate. Move it up (or Sit on Plate) first, so "
                                        "there's room to extrude down to the plate.")
                                         .arg(QString::fromStdString(m_doc.find(id)->name)));
            return;
        }
    } catch (const std::exception&) {
    }
    BooleanSpec spec;
    spec.op = BooleanOp::ExtrudeDown;
    if (addStepOnSelected(spec, "Extruded Down"))
        statusBar()->showMessage(tr("Extruded down to the plate. If you move it afterwards, the extra part moves "
                                    "with it."),
                                 8000);
}

void MainWindow::endSculpt()
{
    if (!m_sculptMode.active)
        return;
    m_sculptMode = SculptMode{};
    m_viewport->setBrushRing(std::nullopt);
    m_sculptPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

std::shared_ptr<const PictureData> MainWindow::loadPictureFile(const QString& path)
{
    QImage image;
    if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        // Drawn at the size kept (its longest side picture::kMaxSide), on white.
        QSvgRenderer svg(path);
        if (svg.isValid()) {
            QSizeF size = svg.defaultSize();
            if (size.isEmpty())
                size = QSizeF(picture::kMaxSide, picture::kMaxSide);
            size.scale(picture::kMaxSide, picture::kMaxSide, Qt::KeepAspectRatio);
            image = QImage(size.toSize(), QImage::Format_RGBA8888);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            svg.render(&painter);
        }
    } else {
        image.load(path);
    }
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("Could not read %1 as a picture.").arg(QFileInfo(path).fileName()));
        return nullptr;
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);
    std::vector<std::uint8_t> rgba(std::size_t(image.width()) * image.height() * 4);
    for (int y = 0; y < image.height(); ++y)
        std::memcpy(rgba.data() + std::size_t(y) * image.width() * 4, image.constScanLine(y),
                    std::size_t(image.width()) * 4);
    try {
        return picture::fromRgba(image.width(), image.height(), rgba);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return nullptr;
    }
}

void MainWindow::choosePicture()
{
    if (!m_embossMode.active)
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a picture"), startFolder(),
        tr("Pictures (*.png *.jpg *.jpeg *.bmp *.gif *.svg);;All files (*)"));
    if (path.isEmpty())
        return;
    rememberFolderOf(path);
    const auto image = loadPictureFile(path);
    if (!image)
        return;
    m_embossPanel->setPicture(QFileInfo(path).fileName().toStdString(), image);
    qInfo().noquote() << "Emboss picture:" << path << image->width << "x" << image->height;
    updateEmbossPreview();
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
    m_doc.setReferences({});
    m_doc.setPoses({});
    m_doc.setMannequin(std::nullopt);
    m_stack.clear();
    m_selection.clear();
    m_expanded.clear();
    m_filePath.clear();
    removeRecovery();
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
        m_doc.setReferences(file::loadReferences(toPath(path)));
        m_doc.setPoses(file::loadPoses(toPath(path)));
        m_doc.setMannequin(file::loadMannequin(toPath(path)));
        // The slow steps' results saved with the file: no need to work them out again.
        if (const std::size_t used = file::useSavedResults(m_doc, toPath(path)))
            qInfo().noquote() << "Saved results used:" << used;
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
    noteRecentFile(m_filePath);
    removeRecovery();
    refreshCommandState();
    refreshScene();
    m_viewport->fitView();
    qInfo().noquote() << "Opened" << m_filePath << "-" << m_doc.bodyCount() << "bodies";
    return true;
}

// ---- Backup copies and recovery -------------------------------------------------

namespace {
QString recoveryFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/recovery");
}
QString besideBackup(const QString& backup, const char* extension)
{
    return backup.left(backup.size() - 4) + QString::fromLatin1(extension);  // in place of ".f3d"
}
void removeBackup(const QString& backup)
{
    QFile::remove(backup);
    QFile::remove(besideBackup(backup, ".json"));
    QFile::remove(besideBackup(backup, ".lock"));
}
}  // namespace

void MainWindow::startAutosave()
{
    QDir().mkpath(recoveryFolder());
    m_recoveryPath = QStringLiteral("%1/%2-%3.f3d")
                         .arg(recoveryFolder())
                         .arg(QDateTime::currentMSecsSinceEpoch())
                         .arg(QCoreApplication::applicationPid());
    // Held while this session is alive, so another Forge3D starting up knows this
    // backup isn't left over from a crash.
    m_recoveryLock = std::make_unique<QLockFile>(besideBackup(m_recoveryPath, ".lock"));
    m_recoveryLock->setStaleLockTime(0);
    m_recoveryLock->tryLock(0);
    m_autosaveTimer = new QTimer(this);
    connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::autosaveNow);
    const auto apply = [this] {
        const Preferences settings;
        bool ok = false;
        const int seconds = qEnvironmentVariableIntValue("FORGE3D_AUTOSAVE_SECONDS", &ok);  // for testing
        const int minutes = std::clamp(settings.value(QStringLiteral("backup/minutes"), 2).toInt(), 2, 60);
        m_autosaveTimer->setInterval(ok && seconds > 0 ? seconds * 1000 : minutes * 60 * 1000);
        if (settings.value(QStringLiteral("backup/on"), true).toBool()) {
            m_autosaveTimer->start();
        } else {
            m_autosaveTimer->stop();
            removeRecovery();        // none wanted: none left lying about
            m_autosavedSerial = 0;   // (and a first one straight away if they're turned back on)
        }
    };
    apply();
    m_applyBackupSettings = apply;
}

void MainWindow::chooseBackups()
{
    Preferences settings;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Backup Copies"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* on = new QCheckBox(tr("Keep a backup copy of unsaved work"));
    on->setChecked(settings.value(QStringLiteral("backup/on"), true).toBool());
    layout->addWidget(on);
    layout->addSpacing(4);
    auto* label = new QLabel(tr("Make one every:"));
    layout->addWidget(label);
    auto* minutes = new QSpinBox;
    minutes->setRange(2, 60);
    minutes->setSuffix(tr(" minutes"));
    minutes->setValue(std::clamp(settings.value(QStringLiteral("backup/minutes"), 2).toInt(), 2, 60));
    minutes->setToolTip(tr("From 2 to 60 minutes"));
    layout->addWidget(minutes);
    layout->addSpacing(8);
    auto* note = new QLabel(tr("If Forge3D doesn't close properly (a crash, a power cut), the next start offers "
                               "the last backup back. With this off, unsaved work is lost if that happens. "
                               "Backups are only made when something has changed, and your own files are never "
                               "touched."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(mid);"));
    note->setMinimumWidth(340);
    layout->addWidget(note);
    layout->addSpacing(8);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    const auto enable = [&] {
        minutes->setEnabled(on->isChecked());
        label->setEnabled(on->isChecked());
    };
    enable();
    connect(on, &QCheckBox::toggled, &dialog, enable);
    if (dialog.exec() != QDialog::Accepted)
        return;
    settings.setValue(QStringLiteral("backup/on"), on->isChecked());
    settings.setValue(QStringLiteral("backup/minutes"), minutes->value());
    settings.sync();
    if (m_applyBackupSettings)
        m_applyBackupSettings();
    statusBar()->showMessage(on->isChecked() ? tr("Backup copies: every %1 minutes").arg(minutes->value())
                                             : tr("Backup copies are off"),
                             5000);
    qInfo() << "Backup copies" << (on->isChecked() ? "on, minutes:" : "off") << minutes->value();
}

void MainWindow::autosaveNow()
{
    // Only what isn't saved, only when it changed since the last backup, and not in
    // the middle of a drag.
    if (m_recoveryPath.isEmpty() || m_stack.isClean() || m_docSerial == m_autosavedSerial)
        return;
    if (m_doc.bodyCount() == 0) {
        removeRecovery();  // everything deleted: nothing worth offering back
        return;
    }
    if (QGuiApplication::mouseButtons() != Qt::NoButton) {
        QTimer::singleShot(5000, this, &MainWindow::autosaveNow);
        return;
    }
    QElapsedTimer took;
    took.start();
    try {
        // Written beside it first, then swapped in: a crash while writing leaves the last good one.
        const QString writing = m_recoveryPath + QStringLiteral(".part");
        file::save(m_doc, toPath(writing), std::string(build::kAppName) + " " + build::kVersion);
        QFile::remove(m_recoveryPath);
        if (!QFile::rename(writing, m_recoveryPath))
            throw std::runtime_error("could not put the backup in place");
        QFile about(besideBackup(m_recoveryPath, ".json"));
        if (about.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const QJsonObject o{{QStringLiteral("original"), m_filePath},
                                {QStringLiteral("saved"), QDateTime::currentDateTime().toString(Qt::ISODate)}};
            about.write(QJsonDocument(o).toJson());
        }
        m_autosavedSerial = m_docSerial;
        qInfo() << "Backup copy written in" << took.elapsed() << "ms";
    } catch (const std::exception& e) {
        qWarning() << "Backup copy failed:" << e.what();
    }
}

void MainWindow::removeRecovery()
{
    if (m_recoveryPath.isEmpty())
        return;
    QFile::remove(m_recoveryPath);
    QFile::remove(m_recoveryPath + QStringLiteral(".part"));
    QFile::remove(besideBackup(m_recoveryPath, ".json"));
    m_autosavedSerial = m_docSerial;
}

void MainWindow::offerRecovery()
{
    // Backups whose session is gone (its lock can be taken): Forge3D didn't close properly.
    QDir folder(recoveryFolder());
    QStringList left;
    for (const QFileInfo& info : folder.entryInfoList({QStringLiteral("*.f3d")}, QDir::Files, QDir::Time)) {
        const QString backup = info.absoluteFilePath();
        if (backup == m_recoveryPath)
            continue;
        QLockFile lock(besideBackup(backup, ".lock"));
        lock.setStaleLockTime(0);
        if (!lock.tryLock(0))
            continue;  // that Forge3D is still running
        lock.unlock();
        left << backup;
    }
    // Locks and half-written backups nobody owns any more.
    for (const QFileInfo& info : folder.entryInfoList({QStringLiteral("*.lock"), QStringLiteral("*.part"), QStringLiteral("*.json")}, QDir::Files)) {
        const QString path = info.absoluteFilePath();
        const QString backup = info.suffix() == QLatin1String("part") ? path.left(path.size() - 5)
                                                                      : path.left(path.size() - info.suffix().size() - 1) + QStringLiteral(".f3d");
        if (backup == m_recoveryPath || QFile::exists(backup))
            continue;
        if (info.suffix() == QLatin1String("lock")) {
            QLockFile lock(path);
            lock.setStaleLockTime(0);
            if (!lock.tryLock(0))
                continue;
            lock.unlock();
        }
        QFile::remove(path);
    }
    for (const QString& backup : left) {
        QString original, when;
        QFile about(besideBackup(backup, ".json"));
        if (about.open(QIODevice::ReadOnly)) {
            const QJsonObject o = QJsonDocument::fromJson(about.readAll()).object();
            original = o.value(QStringLiteral("original")).toString();
            when = QLocale().toString(QDateTime::fromString(o.value(QStringLiteral("saved")).toString(), Qt::ISODate),
                                      QLocale::ShortFormat);
        }
        if (when.isEmpty())
            when = QLocale().toString(QFileInfo(backup).lastModified(), QLocale::ShortFormat);
        const QString name = original.isEmpty() ? tr("a model that was never saved")
                                                : QFileInfo(original).fileName();
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Recover your work?"));
        box.setText(tr("Forge3D didn't close properly last time. It kept a backup copy of %1, made %2.")
                        .arg(name, when));
        box.setInformativeText(tr("Recover opens the backup with its changes not yet saved, so you can look "
                                  "at it and then Save. Nothing on disk is replaced until you save."));
        QPushButton* recover = box.addButton(tr("Recover"), QMessageBox::AcceptRole);
        QPushButton* discard = box.addButton(tr("Throw It Away"), QMessageBox::DestructiveRole);
        box.addButton(tr("Ask Me Next Time"), QMessageBox::RejectRole);
        box.setDefaultButton(recover);
        box.exec();
        if (box.clickedButton() == discard) {
            removeBackup(backup);
            qInfo().noquote() << "Backup thrown away:" << backup;
        } else if (box.clickedButton() == recover) {
            if (!m_stack.isClean() && !confirmDiscardChanges())
                return;
            if (recoverFrom(backup)) {
                m_filePath = original;
                refreshCommandState();
                removeBackup(backup);
                autosaveNow();  // protected again straight away, under this session
                return;         // one at a time; any others are offered next time
            }
        } else {
            return;
        }
    }
}

bool MainWindow::recoverFrom(const QString& backup)
{
    try {
        std::vector<Body> bodies = file::load(toPath(backup));
        m_doc.replaceAll(std::move(bodies));
        m_doc.setReferences(file::loadReferences(toPath(backup)));
        m_doc.setPoses(file::loadPoses(toPath(backup)));
        m_doc.setMannequin(file::loadMannequin(toPath(backup)));
        file::useSavedResults(m_doc, toPath(backup));
    } catch (const std::exception& e) {
        qWarning().noquote() << "Recovery failed:" << backup << "-" << e.what();
        QMessageBox::critical(this, tr("Could not recover the backup"), QString::fromUtf8(e.what()));
        return false;
    }
    m_stack.clear();
    m_stack.setModified();  // not saved anywhere yet
    m_selection.clear();
    m_expanded.clear();
    refreshCommandState();
    refreshScene();
    m_viewport->fitView();
    statusBar()->showMessage(tr("Recovered. Save it (Ctrl+S) to keep it."), 10000);
    qInfo().noquote() << "Recovered from" << backup << "-" << m_doc.bodyCount() << "bodies";
    return true;
}

// ---- Light / dark -----------------------------------------------------------------

void MainWindow::setIconNamed(QAction* action, const QString& name)
{
    action->setIcon(ribbonIcon(name));
    m_namedIcons.emplace_back(action, name);
}

void MainWindow::styleEmptyHint()
{
    if (!m_emptyHint)
        return;
    m_emptyHint->setStyleSheet(
        theme::dark() ? QStringLiteral("QLabel { background: rgba(45, 49, 56, 225); color: #e1e4eb; "
                                       "border: 1px solid #5a606c; border-radius: 6px; padding: 10px; }")
                      : QStringLiteral("QLabel { background: rgba(255, 255, 255, 215); color: #333; "
                                       "border: 1px solid #c8ccd2; border-radius: 6px; padding: 10px; }"));
}

void MainWindow::chooseTheme(int choice)
{
    const auto c = theme::Choice(choice);
    theme::save(c);
    theme::apply(*qApp, c);
    // Everything drawn from the old colours, again.
    for (const auto& [action, name] : m_namedIcons)
        action->setIcon(ribbonIcon(name));
    styleEmptyHint();
    m_viewport->themeChanged();
    refreshObjectList();
    if (m_sketchView)
        m_sketchView->update();
    for (QWidget* w : QApplication::allWidgets()) {
        // Widgets with a style sheet keep the palette they were polished with: polish
        // them again (setting the same sheet does it).
        if (!w->styleSheet().isEmpty())
            w->setStyleSheet(w->styleSheet());
        w->update();
    }
    qInfo() << "Theme:" << (theme::dark() ? "dark" : "light");
}

void MainWindow::noteRecentFile(const QString& path)
{
    Preferences settings;
    QStringList recent = settings.value(QString::fromLatin1(kRecentKey)).toStringList();
    if (!path.isEmpty()) {
        // Newest first, each file once (Windows paths: whatever the capitals).
        const QString full = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
        for (int i = recent.size(); i-- > 0;)
            if (recent[i].compare(full, Qt::CaseInsensitive) == 0)
                recent.removeAt(i);
        recent.prepend(full);
        while (recent.size() > kRecentCount)
            recent.removeLast();
        settings.setValue(QString::fromLatin1(kRecentKey), recent);
    }
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    int number = 1;
    for (const QString& file : recent) {
        // "&1 name.f3d": the number is the key to press; the folder shows as the tip.
        QAction* action = m_recentMenu->addAction(
            QStringLiteral("&%1  %2").arg(number++).arg(QFileInfo(file).fileName().replace(QLatin1Char('&'), QLatin1String("&&"))));
        action->setToolTip(file);
        action->setStatusTip(file);
        connect(action, &QAction::triggered, this, [this, file] {
            if (!QFileInfo::exists(file)) {
                QMessageBox::information(this, tr("Open Recent"),
                                         tr("%1 isn't there any more (moved, renamed or deleted), so it has been "
                                            "taken off the list.")
                                             .arg(file));
                forgetRecentFile(file);
                return;
            }
            if (confirmDiscardChanges())
                openFile(file);
        });
    }
    if (recent.isEmpty()) {
        m_recentMenu->addAction(tr("(No files opened or saved yet)"))->setEnabled(false);
    } else {
        m_recentMenu->addSeparator();
        connect(m_recentMenu->addAction(tr("&Clear This List")), &QAction::triggered, this, [this] {
            Preferences().remove(QString::fromLatin1(kRecentKey));
            noteRecentFile({});
        });
    }
}

void MainWindow::forgetRecentFile(const QString& path)
{
    Preferences settings;
    QStringList recent = settings.value(QString::fromLatin1(kRecentKey)).toStringList();
    recent.removeAll(path);
    settings.setValue(QString::fromLatin1(kRecentKey), recent);
    noteRecentFile({});
}

// ---- Build plate ----------------------------------------------------------------

void MainWindow::applyPlateSize()
{
    const Preferences settings;
    m_viewport->setPlateSize(settings.value(QStringLiteral("plate/x"), 220.0).toFloat(),
                             settings.value(QStringLiteral("plate/y"), 220.0).toFloat());
    m_viewport->setPlateHeight(settings.value(QStringLiteral("plate/z"), 250.0).toFloat());
    m_viewport->setFitWarning(settings.value(QStringLiteral("plate/warn"), true).toBool());
}

void MainWindow::choosePlateSize()
{
    struct Preset {
        const char* name;
        double x, y, z;
    };
    // Common printer beds (X across, Y front to back).
    static const Preset presets[] = {
        {"180 x 180 mm (Prusa MINI, Bambu Lab A1 mini)", 180, 180, 180},
        {"220 x 220 mm (Creality Ender 3, many others)", 220, 220, 250},
        {"235 x 235 mm", 235, 235, 250},
        {"250 x 210 mm (Prusa MK3 / MK4)", 250, 210, 210},
        {"256 x 256 mm (Bambu Lab X1 / P1 / A1)", 256, 256, 256},
        {"270 x 270 mm", 270, 270, 270},
        {"300 x 300 mm", 300, 300, 300},
        {"350 x 350 mm", 350, 350, 350},
        {"360 x 360 mm (Prusa XL)", 360, 360, 360},
    };
    const QSizeF now = m_viewport->plateSize();
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Build Plate Size"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(2);
    layout->addWidget(new QLabel(tr("Printer bed:")));  // the label rests on top of its list
    auto* list = new QComboBox;
    for (const Preset& p : presets)
        list->addItem(QString::fromLatin1(p.name).replace(QLatin1String(" x "), QStringLiteral(" \u00D7 ")));
    list->addItem(tr("Another size (type it below)"));
    layout->addWidget(list);
    layout->addSpacing(8);
    const auto spin = [](double value) {
        auto* s = new QDoubleSpinBox;
        s->setRange(20.0, 2000.0);
        s->setDecimals(0);
        s->setSingleStep(10.0);
        s->setSuffix(tr(" mm"));
        s->setValue(value);
        return s;
    };
    layout->addWidget(new QLabel(tr("Across (X):")));
    auto* x = spin(now.width());
    layout->addWidget(x);
    layout->addWidget(new QLabel(tr("Front to back (Y):")));
    auto* y = spin(now.height());
    layout->addWidget(y);
    layout->addWidget(new QLabel(tr("Height it can print (Z):")));
    auto* z = spin(m_viewport->plateHeight());
    z->setRange(10.0, 5000.0);
    z->setToolTip(tr("Choosing a bed from the list sets a usual height for it; check your printer's and change "
                     "it here if it differs"));
    layout->addWidget(z);
    layout->addSpacing(6);
    auto* warn = new QCheckBox(tr("Show shapes that don't fit in orange-red"));
    warn->setChecked(Preferences().value(QStringLiteral("plate/warn"), true).toBool());
    layout->addWidget(warn);
    layout->addSpacing(8);
    auto* note = new QLabel(tr("The grid is drawn this size, with a darker line round its edge and a faint box "
                               "up to the height, so you can see whether a design fits your printer. It's "
                               "remembered for every model."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(note);
    layout->addSpacing(8);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const int other = list->count() - 1;
    const auto matching = [&] {
        for (int i = 0; i < other; ++i)
            if (presets[i].x == x->value() && presets[i].y == y->value())
                return i;
        return other;
    };
    list->setCurrentIndex(matching());
    connect(list, &QComboBox::activated, &dialog, [&](int index) {
        if (index >= other)
            return;
        const QSignalBlocker a(x), b(y);
        x->setValue(presets[index].x);
        y->setValue(presets[index].y);
        z->setValue(presets[index].z);
    });
    const auto typed = [&] { list->setCurrentIndex(matching()); };
    connect(x, &QDoubleSpinBox::valueChanged, &dialog, typed);
    connect(y, &QDoubleSpinBox::valueChanged, &dialog, typed);

    if (dialog.exec() != QDialog::Accepted)
        return;
    Preferences settings;
    settings.setValue(QStringLiteral("plate/x"), x->value());
    settings.setValue(QStringLiteral("plate/y"), y->value());
    settings.setValue(QStringLiteral("plate/z"), z->value());
    settings.setValue(QStringLiteral("plate/warn"), warn->isChecked());
    applyPlateSize();
    if (m_doc.bodyCount() == 0)
        m_viewport->resetView();
    statusBar()->showMessage(
        tr("Build plate: %1 \u00D7 %2 mm, %3 mm high").arg(x->value()).arg(y->value()).arg(z->value()), 4000);
    qInfo() << "Build plate size" << x->value() << "x" << y->value();
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
           s.endsWith(QLatin1String(".obj")) || s.endsWith(QLatin1String(".ply")) ||
           s.endsWith(QLatin1String(".glb")) || s.endsWith(QLatin1String(".gltf")) ||
           s.endsWith(QLatin1String(".f3d"));
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
    Preferences settings;
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

    // Laid out on several plates: one file per plate, if wanted.
    std::map<int, std::vector<ObjectId>> byPlate;
    {
        const auto volume = printerVolume();
        const double pitch = volume[0] + kPlateGap;
        for (ObjectId id : ids) {
            const Bounds b = m_doc.worldBounds(id);
            int k = plateHolding(b, volume);
            if (k < 0)  // not wholly on one: the nearest
                k = std::max(0, int(std::lround((double(b.min[0]) + b.max[0]) / 2 / pitch)));
            byPlate[k].push_back(id);
        }
    }
    if (byPlate.size() > 1) {
        QMessageBox ask(this);
        ask.setIcon(QMessageBox::Question);
        ask.setWindowTitle(tr("Export"));
        ask.setText(tr("These shapes are laid out on %1 plates.").arg(byPlate.size()));
        ask.setInformativeText(tr("Save one file per plate (\"%1 - plate 1\", \"%1 - plate 2\"...), each with its "
                                  "shapes in the middle, or everything in one file?")
                                   .arg(QFileInfo(path).completeBaseName()));
        QPushButton* perPlate = ask.addButton(tr("One File per Plate"), QMessageBox::AcceptRole);
        QPushButton* one = ask.addButton(tr("One File"), QMessageBox::AcceptRole);
        ask.addButton(QMessageBox::Cancel);
        ask.setDefaultButton(perPlate);
        ask.exec();
        if (ask.clickedButton() != perPlate && ask.clickedButton() != one)
            return;
        if (ask.clickedButton() == perPlate) {
            const QFileInfo info(path);
            QStringList written;
            std::size_t triangles = 0;
            try {
                for (const auto& [plate, shapes] : byPlate) {
                    auto meshes = buildExportMeshes(m_doc, shapes);
                    // Each plate's shapes where they'd sit on plate 1.
                    const float dx = float(plateCentreX(plate, printerVolume()[0]));
                    for (auto& m : meshes)
                        for (std::size_t i = 0; i < m.positions.size(); i += 3)
                            m.positions[i] -= dx;
                    const QString file = info.dir().filePath(QStringLiteral("%1 - plate %2.%3")
                                                                 .arg(info.completeBaseName())
                                                                 .arg(plate + 1)
                                                                 .arg(info.suffix()));
                    writeMeshes(meshes, toPath(file), formatForPath(toPath(file)));
                    for (const auto& m : meshes)
                        triangles += m.triangleCount();
                    written << QFileInfo(file).fileName();
                    if (const QString warning = leakWarning(meshes); !warning.isEmpty())
                        QMessageBox::information(this, tr("Forge3D"), warning);
                }
            } catch (const std::exception& e) {
                qWarning() << "Export failed:" << e.what();
                QMessageBox::warning(this, tr("Forge3D"),
                                     tr("Export did not work:\n%1").arg(QString::fromUtf8(e.what())));
                return;
            }
            qInfo().noquote() << "Exported" << written.size() << "plates," << triangles << "triangles to"
                              << info.absolutePath();
            statusBar()->showMessage(tr("Exported %1 plates to %2: %3")
                                         .arg(written.size())
                                         .arg(QDir::toNativeSeparators(info.absolutePath()))
                                         .arg(written.join(QStringLiteral(", "))),
                                     12000);
            return;
        }
    }

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

    Preferences settings;
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
    Preferences settings;
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
    noteRecentFile(m_filePath);
    removeRecovery();
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
    if (confirmDiscardChanges()) {
        removeRecovery();  // closed properly: nothing to recover
        m_recoveryLock.reset();
        event->accept();
    } else {
        event->ignore();
    }
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
    if (m_emptyHint)
        m_emptyHint->setVisible(m_doc.empty() && !m_mannequinMesh);

    std::vector<DisplayItem> items;
    std::size_t triangles = 0;
    const auto colourOf = [this](ObjectId id) {
        const std::uint32_t c = m_doc.colourOf(id);
        return QVector3D(float((c >> 16) & 0xFF) / 255.0f, float((c >> 8) & 0xFF) / 255.0f, float(c & 0xFF) / 255.0f);
    };
    try {
        // Everything on the plate...
        if (m_embossMode.active && !m_doc.find(m_embossMode.target))
            endEmboss();  // e.g. Undo removed the shape
        if (m_edgesMode.active && !m_doc.find(m_edgesMode.target))
            endEdges();
        if (m_holeMode.active && !m_doc.find(m_holeMode.target))
            endHole();
        if (m_shellMode.active && !m_doc.find(m_shellMode.target))
            endShell();
        if (m_threadMode.active && !m_doc.find(m_threadMode.target))
            endThread();
        if (m_socketMode.active && !m_doc.find(m_socketMode.target))
            endSocket();
        if (m_hollowMode.active && !m_doc.find(m_hollowMode.target))
            endHollow();
        if (m_mouldMode.active && !m_doc.find(m_mouldMode.target))
            endMould();
        if (m_portMode.active && !m_doc.find(m_portMode.target))
            endPort();
        if (m_poseFigureMode.active && !m_doc.find(m_poseFigureMode.target))
            endPoseFigure();
        if (m_stampMode.active && !m_doc.find(m_stampMode.target))
            endStamp();
        if (m_panelLineMode.active && !m_doc.find(m_panelLineMode.target))
            endPanelLines();
        if (m_hingeMode.active && !m_doc.find(m_hingeMode.target))
            endHinge();
        if (m_maskMode.active && !m_doc.find(m_maskMode.target))
            endMask();
        if (m_paintMode.active && !m_doc.find(m_paintMode.target))
            endPaint();
        if (m_cutPartsMode.active && !m_doc.find(m_cutPartsMode.target))
            endCutParts();
        if (m_joinMode.active && ((m_joinMode.moving != kInvalidId && !m_doc.find(m_joinMode.moving)) ||
                                  (m_joinMode.fixed != kInvalidId && !m_doc.find(m_joinMode.fixed))))
            endJoin();
        if (m_followMode.active && !m_doc.find(m_followMode.target))
            endFollow();
        if (m_patternMode.active && !m_doc.find(m_patternMode.target))
            endPattern();
        QString unbuilt;  // the first shape that couldn't be built: the others still show
        for (ObjectId id : m_doc.topLevelIds()) {
            if (m_maskMode.active && id == m_maskMode.hidden)
                continue;  // the other piece, while its mask is being changed
            if (m_mouldMode.active && m_mouldMode.editing && id == m_mouldMode.other)
                continue;  // the other half, while the mould is being changed
            DisplayItem item;
            item.id = id;
            try {
                item.mesh = m_doc.mesh(id);  // rebuilt only if the shape changed
            } catch (const std::exception& e) {
                qCritical() << "Could not build" << id << "for display:" << e.what();
                if (unbuilt.isEmpty())
                    unbuilt = tr("'%1' could not be built: %2")
                                  .arg(QString::fromStdString(m_doc.find(id)->name), QString::fromUtf8(e.what()));
                continue;
            }
            item.model = toQMatrix(m_doc.worldTransform(id));
            if (m_mouldMode.active && m_mouldMode.editing && id == m_mouldMode.target && m_mouldMode.shownPart) {
                item.mesh = m_mouldMode.shownPart;  // the part, inside the box being changed
                item.model = toQMatrix(m_mouldMode.toPlate);
            }
            if (!(m_paintMode.active && id == m_paintMode.object))
                item.mesh = paintedMesh(id, item.mesh);  // painted: shown in its colours
            if (m_paintMode.active && id == m_paintMode.object && m_paintMode.shown) {
                item.mesh = m_paintMode.shown;
                item.model = toQMatrix(m_paintMode.toPlate);
                item.selected = false;  // the colours as they are, not lightened
            }
            if (m_maskMode.active && id == m_maskMode.object && m_maskMode.shown) {
                item.mesh = m_maskMode.shown;
                item.model = toQMatrix(m_maskMode.toPlate);
                item.maskColours = true;
            }
            if (m_joinMode.active && m_joinMode.placed && id == m_joinMode.moving)
                item.model = toQMatrix(placementTransform(*m_joinMode.placed, pivotOf(*m_doc.find(id))));
            if (m_edgesMode.active && id == history::objectOf(m_doc, m_edgesMode.target)) {
                if (m_edgesMode.preview) {
                    // The step being made or changed, shown for the whole object.
                    item.mesh = m_edgesMode.preview;
                    item.model = previewFrame(m_edgesMode.target, m_edgesMode.editing);
                }
                // Picked edges in red, the one under the mouse in orange, seen through the shape.
                for (const auto& [lines, colour, tag] :
                     {std::tuple{m_edgesMode.pickedLines, QVector3D(0.9f, 0.15f, 0.1f), kEdgesPickedItem},
                      std::tuple{m_edgesMode.hoverLine, QVector3D(1.0f, 0.6f, 0.0f), kEdgesHoverItem}}) {
                    if (!lines || lines->empty())
                        continue;
                    DisplayItem line;
                    line.id = tag;
                    line.mesh = lines;
                    line.color = colour;
                    line.ghost = true;
                    items.push_back(line);
                }
            }
            if (m_sculptMode.active && id == m_sculptMode.object && m_sculptMode.display) {
                item.mesh = m_sculptMode.display;  // being sculpted, in the part's coordinates
                item.model = toQMatrix(m_sculptMode.toPlate);
            }
            if (m_holeMode.active && m_holeMode.preview && id == history::objectOf(m_doc, m_holeMode.target)) {
                item.mesh = m_holeMode.preview;  // drilled, as it will be (plate coordinates)
                item.model = QMatrix4x4();
            }
            if (m_patternMode.active && m_patternMode.preview &&
                id == history::objectOf(m_doc, m_patternMode.target)) {
                item.mesh = m_patternMode.preview;
                item.model = previewFrame(m_patternMode.target, m_patternMode.editing);
            }
            if (m_followMode.active && id == history::objectOf(m_doc, m_followMode.target)) {
                if (m_followMode.preview) {
                    item.mesh = m_followMode.preview;
                    item.model = previewFrame(m_followMode.target, m_followMode.editing);
                }
                if (m_followMode.pathLines && !m_followMode.pathLines->empty()) {
                    DisplayItem line;
                    line.id = kEdgesPickedItem;
                    line.mesh = m_followMode.pathLines;
                    line.color = QVector3D(0.95f, 0.5f, 0.05f);
                    line.ghost = true;
                    items.push_back(line);
                }
            }
            if (m_socketMode.active && id == history::objectOf(m_doc, m_socketMode.target)) {
                if (m_socketMode.preview) {
                    item.mesh = m_socketMode.preview;
                    item.model = previewFrame(m_socketMode.target, m_socketMode.editing);
                }
                if (m_socketMode.cut) {
                    DisplayItem cut;
                    cut.id = kEdgesPickedItem;
                    cut.mesh = m_socketMode.cut;
                    cut.color = QVector3D(0.9f, 0.15f, 0.1f);
                    cut.ghost = true;
                    cut.helper = true;
                    items.push_back(cut);
                }
            }
            if (m_cutPartsMode.active && id == m_cutPartsMode.target) {
                // See-through, with the skeleton and the cuts (orange discs) inside.
                item.alpha = 0.45f;
                item.selected = false;
                for (const auto& shown : {m_cutPartsMode.skeleton, m_cutPartsMode.discs}) {
                    if (!shown)
                        continue;
                    DisplayItem extra;
                    extra.id = shown == m_cutPartsMode.skeleton ? kEdgesPickedItem : kEdgesHoverItem;
                    extra.mesh = shown;
                    extra.color = shown == m_cutPartsMode.skeleton ? QVector3D(1.0f, 0.55f, 0.1f)
                                                                   : QVector3D(0.9f, 0.2f, 0.15f);
                    extra.ghost = true;
                    extra.helper = true;
                    items.push_back(extra);
                }
            }
            if (m_poseFigureMode.active && id == history::objectOf(m_doc, m_poseFigureMode.target)) {
                // Fitting: the figure as made, see-through; posing: as posed.
                const bool posing = m_poseFigurePanel->posing() && m_poseFigureMode.posed;
                item.mesh = posing ? m_poseFigureMode.posed : m_poseFigureMode.rest;
                item.model = toQMatrix(m_poseFigureMode.frame);
                if (!posing)
                    item.alpha = 0.45f;
                if (m_poseFigureMode.skeleton) {
                    DisplayItem bones;
                    bones.id = kEdgesPickedItem;
                    bones.mesh = m_poseFigureMode.skeleton;
                    bones.model = item.model;
                    bones.color = QVector3D(1.0f, 0.55f, 0.1f);
                    bones.ghost = true;
                    bones.helper = true;
                    items.push_back(bones);
                }
            }
            if (m_hingeMode.active && id == history::objectOf(m_doc, m_hingeMode.target)) {
                if (m_hingeMode.preview) {
                    item.mesh = m_hingeMode.preview;
                    item.model = previewFrame(m_hingeMode.target, m_hingeMode.editing);
                }
                if (m_hingeMode.cut) {
                    DisplayItem cut;
                    cut.id = kEdgesPickedItem;
                    cut.mesh = m_hingeMode.cut;
                    cut.color = QVector3D(0.9f, 0.15f, 0.1f);
                    cut.ghost = true;
                    cut.helper = true;
                    items.push_back(cut);
                }
                if (m_hingeMode.ghost) {
                    DisplayItem ghost;
                    ghost.id = kEdgesPickedItem;
                    ghost.mesh = m_hingeMode.ghost;
                    ghost.color = QVector3D(0.17f, 0.55f, 0.85f);
                    ghost.ghost = true;
                    ghost.helper = true;
                    items.push_back(ghost);
                }
            }
            if (m_threadMode.active && id == history::objectOf(m_doc, m_threadMode.target)) {
                if (m_threadMode.preview) {
                    item.mesh = m_threadMode.preview;
                    item.model = previewFrame(m_threadMode.target, m_threadMode.editing);
                }
                if (m_threadMode.pickedLines && !m_threadMode.pickedLines->empty()) {
                    DisplayItem line;
                    line.id = kEdgesPickedItem;
                    line.mesh = m_threadMode.pickedLines;
                    line.color = QVector3D(0.9f, 0.15f, 0.1f);
                    line.ghost = true;
                    items.push_back(line);
                }
            }
            if (m_shellMode.active && id == history::objectOf(m_doc, m_shellMode.target)) {
                if (m_shellMode.preview) {
                    item.mesh = m_shellMode.preview;
                    item.model = previewFrame(m_shellMode.target, m_shellMode.editing);
                }
                if (m_shellMode.pickedLines && !m_shellMode.pickedLines->empty()) {
                    DisplayItem line;
                    line.id = kEdgesPickedItem;
                    line.mesh = m_shellMode.pickedLines;
                    line.color = QVector3D(0.9f, 0.15f, 0.1f);
                    line.ghost = true;
                    items.push_back(line);
                }
            }
            const bool embossHere =
                m_embossMode.active && m_embossMode.preview && id == history::objectOf(m_doc, m_embossMode.target);
            if (embossHere && m_embossMode.wordsOnly) {
                // A big shape stays as it is; the words are drawn over it (see below).
                DisplayItem words;
                words.id = kEmbossWordsItem;
                words.mesh = m_embossMode.preview;
                words.model = previewFrame(m_embossMode.target, m_embossMode.editing);
                words.color = QVector3D(1.0f, 0.55f, 0.1f);
                items.push_back(words);
            } else if (embossHere) {
                // The shape with the words on, as it would be, for the whole object.
                item.mesh = m_embossMode.preview;
                item.model = previewFrame(m_embossMode.target, m_embossMode.editing);
            }
            item.color = colourOf(id);
            item.selected = isSelected(id);
            if (m_poseMode.active) {
                if (const auto m = m_poseMode.moved.find(id); m != m_poseMode.moved.end())
                    item.model = toQMatrix(m->second) * item.model;
                if (m_poseMode.hitParts.count(id))
                    item.color = 0.45f * item.color + 0.55f * QVector3D(0.95f, 0.25f, 0.2f);
            }
            // Print Check: the part coloured by what was found (once it has been measured).
            if (m_printCheckMode.active) {
                const auto c = m_printCheckMode.parts.find(id);
                if (c != m_printCheckMode.parts.end() && c->second.shown && c->second.mesh == item.mesh) {
                    item.mesh = c->second.shown;
                    item.checkColours = true;
                    const auto v = [](const QColor& q) {
                        return QVector3D(float(q.redF()), float(q.greenF()), float(q.blueF()));
                    };
                    item.unmasked = v(PrintCheckPanel::okColour());
                    item.overhang = v(PrintCheckPanel::overhangColour());
                    item.masked = v(PrintCheckPanel::thinColour());
                    item.selected = false;
                }
            }
            // Painting a mask (Split by Mask, or sculpting's Mask brush): plain, strong colours.
            if (m_sculptMode.active && id == m_sculptMode.object && m_sculptPanel->brush() == int(sculpt::Brush::Mask))
                item.maskColours = true;
            if (item.maskColours) {
                const QColor u = m_maskPanel->unmaskedColour(), m = m_maskPanel->maskedColour();
                item.unmasked = QVector3D(float(u.redF()), float(u.greenF()), float(u.blueF()));
                item.masked = QVector3D(float(m.redF()), float(m.greenF()), float(m.blueF()));
                item.selected = false;
            }
            triangles += item.mesh->triangleCount();
            items.push_back(std::move(item));
        }
        // Panel lines drawn so far: what's cut away, red, seen through the shape.
        if (m_panelLineMode.active && m_panelLineMode.shown) {
            DisplayItem cut;
            cut.id = kEdgesPickedItem;
            cut.mesh = m_panelLineMode.shown;
            cut.color = QVector3D(0.9f, 0.15f, 0.1f);
            cut.ghost = true;
            cut.helper = true;
            items.push_back(cut);
        }
        // Stamps placed so far: gold standing out, red cut in (seen through the shape).
        if (m_stampMode.active && m_stampMode.shown) {
            DisplayItem marks;
            marks.id = kEdgesPickedItem;
            marks.mesh = m_stampMode.shown;
            const bool sunk = m_stampPanel->settings().sunk;
            marks.color = sunk ? QVector3D(0.9f, 0.15f, 0.1f) : QVector3D(0.9f, 0.68f, 0.18f);
            marks.ghost = sunk;
            marks.helper = true;
            items.push_back(marks);
        }
        // Make Hand: the hand to be made, see-through.
        if (m_handMode.active && m_handMode.preview && !m_handMode.preview->empty()) {
            DisplayItem h;
            h.id = kMouldItem - 6;
            h.mesh = m_handMode.preview;
            h.color = QVector3D(0.85f, 0.62f, 0.45f);
            h.alpha = 0.75f;
            h.helper = true;
            items.push_back(h);
        }
        // Hand Ports: the peg gold, a hole red, seen through.
        if (m_portMode.active && m_portMode.shape && !m_portMode.shape->empty()) {
            DisplayItem h;
            h.id = kMouldItem - 5;
            h.mesh = m_portMode.shape;
            h.model = toQMatrix(m_portMode.toPlate);
            h.color = m_portPanel->what() == PortPanel::What::Peg ? QVector3D(0.9f, 0.68f, 0.18f)
                                                                   : QVector3D(0.9f, 0.15f, 0.1f);
            h.ghost = true;
            h.helper = true;
            items.push_back(h);
        }
        // Make Mould: the box see-through, the cuts yellow, the undercuts, spout and vents red
        // and the keys gold (seen through).
        if (m_mouldMode.active) {
            const QMatrix4x4 frame = toQMatrix(m_mouldMode.toPlate);
            std::uint64_t next = kMouldItem;
            const auto helper = [&](const std::shared_ptr<const Mesh>& mesh, QVector3D colour, float alpha, bool ghost) {
                const std::uint64_t mine = next--;
                if (!mesh || mesh->empty())
                    return;
                DisplayItem h;
                h.id = mine;
                h.mesh = mesh;
                h.model = frame;
                h.color = colour;
                h.alpha = alpha;
                h.ghost = ghost;
                h.helper = true;
                items.push_back(h);
            };
            helper(m_mouldMode.undercuts, QVector3D(1.0f, 0.1f, 0.05f), 1.0f, true);
            helper(m_mouldMode.channels, QVector3D(0.9f, 0.15f, 0.1f), 1.0f, true);
            helper(m_mouldMode.keys, QVector3D(0.9f, 0.68f, 0.18f), 1.0f, true);
            helper(m_mouldMode.sheet, QVector3D(1.0f, 0.85f, 0.2f), 0.4f, false);
            helper(m_mouldMode.box, QVector3D(0.55f, 0.62f, 0.72f), 0.22f, false);
        }
        // Hollow: where the drain holes go, red, seen through the shape.
        if (m_hollowMode.active && m_hollowMode.cut) {
            DisplayItem cut;
            cut.id = kEdgesPickedItem;
            cut.mesh = m_hollowMode.cut;
            cut.color = QVector3D(0.9f, 0.15f, 0.1f);
            cut.ghost = true;
            cut.helper = true;
            items.push_back(cut);
        }
        // The figure guide: see-through, never picked or counted.
        if (m_mannequinMesh) {
            DisplayItem guide;
            guide.id = kMannequinItem;
            guide.mesh = m_mannequinMesh;
            guide.color = QVector3D(0.62f, 0.52f, 0.90f);
            guide.alpha = 0.45f;
            guide.helper = true;
            guide.fitted = true;
            items.push_back(guide);
        }
        // Pose Test: where parts overlap, bright red, seen through the parts.
        if (m_poseMode.active && m_poseMode.hitMesh) {
            DisplayItem hit;
            hit.id = kEdgesPickedItem;
            hit.mesh = m_poseMode.hitMesh;
            hit.color = QVector3D(1.0f, 0.1f, 0.05f);
            hit.ghost = true;
            hit.helper = true;
            items.push_back(hit);
        }
        // ...plus selected parts of combined shapes, see-through, where they are.
        for (ObjectId id : m_selection) {
            if (m_doc.isTopLevel(id))
                continue;
            DisplayItem item;
            item.id = id;
            try {
                item.mesh = m_doc.mesh(id);
            } catch (const std::exception& e) {
                qCritical() << "Could not build part" << id << "for display:" << e.what();
                continue;
            }
            item.model = toQMatrix(m_doc.worldTransform(id));
            item.color = colourOf(id);
            item.selected = true;
            item.ghost = true;
            items.push_back(std::move(item));
        }
        if (!unbuilt.isEmpty())
            statusBar()->showMessage(unbuilt, 8000);
    } catch (const std::exception& e) {
        qCritical() << "Could not build geometry for display:" << e.what();
        statusBar()->showMessage(tr("A shape could not be built: %1").arg(QString::fromUtf8(e.what())), 8000);
    }
    if (m_splitMode.active && !m_doc.find(m_splitMode.target))
        endSplit();  // e.g. Undo removed the shape being cut
    if (m_fitMode.active && (!m_doc.find(m_fitMode.target) || !m_doc.isTopLevel(m_fitMode.target)))
        endFit();
    addSplitItems(items);
    addFitItems(items);
    if (m_measureMode.active) {
        // Measured things: picked in red, under the mouse in orange; the nearest-points
        // line in green, centre to centre in blue. Seen through the shapes.
        for (const auto& [mesh, colour, tag] :
             {std::tuple{m_measureMode.pickedMesh, QVector3D(0.9f, 0.15f, 0.1f), kMeasurePickedItem},
              std::tuple{m_measureMode.hoverMesh, QVector3D(1.0f, 0.6f, 0.0f), kMeasureHoverItem},
              std::tuple{m_measureMode.lineMesh, QVector3D(0.1f, 0.7f, 0.2f), kMeasureLineItem},
              std::tuple{m_measureMode.centreMesh, QVector3D(0.15f, 0.35f, 0.95f), kMeasureCentreItem}}) {
            if (!mesh || mesh->empty())
                continue;
            DisplayItem item;
            item.id = tag;
            item.mesh = mesh;
            item.color = colour;
            item.ghost = true;
            items.push_back(std::move(item));
        }
    }
    {
        // Plates 2, 3...: drawn while something lies wholly on them (after Lay Out).
        int plates = 1;
        const auto volume = printerVolume();
        for (ObjectId id : m_doc.topLevelIds()) {
            try {
                plates = std::max(plates, plateHolding(m_doc.worldBounds(id), volume) + 1);
            } catch (const std::exception&) {
            }
        }
        m_viewport->setPlateCount(plates);
    }
    m_viewport->setItems(std::move(items));
    if (m_sectionDock && m_sectionDock->isVisible()) {
        // The model or the selection changed: the slider reaches across what's cut now.
        const Bounds box = m_viewport->sceneBounds(m_sectionPanel->selectedOnly());
        const auto axis = std::size_t(m_sectionPanel->axis());
        if (box.valid)
            m_sectionPanel->setReach(box.min[axis], box.max[axis]);
    }

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
    m_viewport->setGizmo(toolOpen() || (m_layFlat && m_layFlat->isChecked()) ? std::nullopt : selectionGizmo(),
                         m_gizmoMode);

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
            case BooleanOp::Edges: {
                const auto& f = selected->boolean->edges;
                const int n = f ? int(f->edges.size()) : 0;
                QString what;
                if (f)
                    switch (f->kind) {
                    case EdgeFinishKind::Rounding: what = tr("rounded R%1").arg(f->size); break;
                    case EdgeFinishKind::Fillet: what = tr("filleted R%1").arg(f->size); break;
                    case EdgeFinishKind::Chamfer: what = tr("chamfered %1 mm").arg(f->size); break;
                    case EdgeFinishKind::Bevel: what = tr("bevelled %1 mm at %2°").arg(f->size).arg(f->angle); break;
                    }
                const QString count = n == 1 ? tr("1 edge") : tr("%1 edges").arg(n);
                context = tr("Made from: %1, with %2 %3").arg(names.join(QString()), count, what);
                break;
            }
            case BooleanOp::Emboss:
                if (const auto& pic = selected->boolean->picture) {
                    context = tr("Made from: %1, with %2 %3")
                                  .arg(names.join(QString()),
                                       pic->relief ? tr("the photo %1").arg(QString::fromStdString(pic->name))
                                                   : tr("the picture %1").arg(QString::fromStdString(pic->name)),
                                       pic->raised ? tr("raised") : tr("engraved"));
                    break;
                }
                context = tr("Made from: %1, with the words \"%2\" %3")
                              .arg(names.join(QString()),
                                   QString::fromStdString(selected->boolean->emboss ? selected->boolean->emboss->text
                                                                                    : std::string()),
                                   selected->boolean->emboss && !selected->boolean->emboss->raised ? tr("engraved")
                                                                                                    : tr("raised"));
                break;
            case BooleanOp::Simplify: {
                context = tr("Made from: %1").arg(names.join(QString()));
                try {
                    const auto before = m_doc.mesh(selected->boolean->operands.front())->triangleCount();
                    const auto after = m_doc.mesh(selected->id)->triangleCount();
                    context += tr(", %1 triangles now %2.")
                                   .arg(QLocale().toString(qulonglong(before)), QLocale().toString(qulonglong(after)));
                } catch (const std::exception&) {
                }
                break;
            }
            default: context = tr("Made from: %1").arg(names.join(joiner)); break;
            }
        } else if (const auto* mesh = std::get_if<MeshSpec>(&selected->spec);
                   mesh && m_doc.isTopLevel(selected->id)) {
            context = tr("A mesh: %1 triangles.").arg(mesh->data->indices.size() / 3) +
                      (mesh->data->closed ? QString() : tr(" Not watertight: it can't be combined or split."));
        } else if (std::holds_alternative<PadSpec>(selected->spec)) {
            context = tr("Made from a sketch. To change the sketch, double-click it in Objects (or right-click "
                         "> Change...).");
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
    if (selected && selected->isBoolean() && selected->boolean->op == BooleanOp::Simplify &&
        selected->boolean->operands.size() == 1) {
        try {
            m_properties->setSimplifyCounts(m_doc.mesh(selected->boolean->operands.front())->triangleCount(),
                                            m_doc.mesh(selected->id)->triangleCount());
        } catch (const std::exception&) {
            m_properties->setSimplifyCounts(0, 0);
        }
    }
    refreshObjectList();

    const auto count = static_cast<int>(m_doc.topLevelIds().size());
    m_modelInfo->setText(count == 0 ? tr("Empty plate - click Add Shape")
                                    : (count == 1 ? tr("1 shape") : tr("%1 shapes").arg(count)) +
                                          tr("  |  %1 triangles").arg(triangles));
    const bool anySelected = !m_selection.empty();
    const bool allTopLevel = std::all_of(m_selection.begin(), m_selection.end(),
                                         [this](ObjectId id) { return m_doc.isTopLevel(id); });
    m_delete->setEnabled(anySelected && allTopLevel);
    m_deselect->setEnabled(anySelected || toolOpen() || (m_layFlat && m_layFlat->isChecked()));  // Esc also closes a tool
    // A rolled-back object takes new steps only at its bar (Edges, Emboss): roll it
    // forward before combining, mirroring or splitting it.
    const bool rolled = rolledBackSelected();
    for (QAction* a : {m_merge, m_subtract, m_intersect})
        a->setEnabled(m_selection.size() >= 2 && allTopLevel && !rolled);
    m_separate->setEnabled(selected && selected->isBoolean() && m_doc.isTopLevel(selected->id));
    m_alignMenu->menuAction()->setEnabled(m_selection.size() >= 2 && allTopLevel);
    m_joinAction->setEnabled(m_doc.bodyCount() >= 2 && !toolOpen());
    m_maskAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    for (QAction* a : {m_duplicate, m_copy, m_cut, m_saveCustomShape})
        a->setEnabled(anySelected && allTopLevel && !toolOpen());
    m_mirrorMenu->menuAction()->setEnabled(anySelected && allTopLevel && !toolOpen() && !rolled);
    m_colourMenu->menuAction()->setEnabled(anySelected && !toolOpen());
    m_layOutMenu->menuAction()->setEnabled(m_doc.bodyCount() > 0 && !toolOpen() && (!anySelected || allTopLevel));
    m_group->setEnabled(m_selection.size() >= 2 && allTopLevel && !rolled);
    m_paste->setEnabled(!m_clipboard.empty() && !toolOpen());
    m_export->setEnabled(!m_doc.empty());
    m_sendToSlicer->setEnabled(!m_doc.empty());
    m_layFlat->setEnabled(!m_doc.empty() && !toolOpen());
    if (!m_layFlat->isEnabled() && m_layFlat->isChecked())
        m_layFlat->setChecked(false);
    m_sitOnPlate->setEnabled(!m_selection.empty() && !toolOpen());
    m_sculptAction->setEnabled(m_selection.size() == 1 && m_doc.isTopLevel(m_selection.front()) && !toolOpen());
    m_simplifyAction->setEnabled(m_sculptAction->isEnabled());
    m_smoothAction->setEnabled(m_sculptAction->isEnabled());
    m_extrudeDownAction->setEnabled(m_sculptAction->isEnabled());
    m_split->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen() && !rolled);
    m_emboss->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_edgesAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_sketchAction->setEnabled((m_selection.empty() || (m_selection.size() == 1 && allTopLevel && !rolled)) &&
                               !toolOpen());
    m_holeAction->setEnabled(m_selection.size() == 1 && allTopLevel && !rolled && !toolOpen());
    m_shellAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_hollowAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_mouldAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_portAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_handAction->setEnabled(!toolOpen());
    m_poseFigureAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_paintAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_cutPartsAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_stampAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_panelLineAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_threadAction->setEnabled(m_shellAction->isEnabled());
    m_socketAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_hingeAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_poseAction->setEnabled(!toolOpen() && !m_doc.topLevelIds().empty());
    m_printCheckAction->setEnabled(!toolOpen() && !m_doc.topLevelIds().empty());
    m_followAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_patternAction->setEnabled(m_selection.size() == 1 && allTopLevel && !toolOpen());
    m_clear->setEnabled(!m_doc.empty());
    relockForSketch();
}

void MainWindow::addObjectRow(QTreeWidgetItem* parent, ObjectId id)
{
    const Body* body = m_doc.find(id);
    if (!body)
        return;
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_objects);
    item->setText(0, QString::fromStdString(body->name));
    item->setText(1, QString::fromStdString(history::describe(m_doc, id)));
    item->setForeground(1, palette().brush(QPalette::Disabled, QPalette::Text));
    item->setData(0, Qt::UserRole, QVariant::fromValue<ObjectId>(id));
    item->setData(0, kRowKindRole, kObjectRow);
    item->setToolTip(0, QString::fromStdString(typeName(*body)));
    if (body->isBoolean()) {
        QFont bold = item->font(0);
        bold.setBold(true);
        item->setFont(0, bold);
        addStepRows(item, id);
        const std::size_t shown = history::barAt(m_doc, id), total = history::steps(m_doc, id).size();
        if (shown < total)
            item->setText(1, tr("rolled back to step %1 of %2").arg(shown).arg(total));
        item->setExpanded(m_expanded.count({kObjectRow, id}) > 0);
    }
    item->setSelected(isSelected(id));
}

void MainWindow::addStepRows(QTreeWidgetItem* objectRow, ObjectId top)
{
    const std::vector<ObjectId> line = history::steps(m_doc, top);
    const std::size_t shown = history::barAt(m_doc, top);
    // A finished result (perhaps from the file) means every step in it was built, or
    // was clean when it was saved: don't build the steps just to list them.
    const bool built = m_doc.usesMeshEngine(top) && m_doc.cachedSolid(top).has_value();
    const auto problemOf = [this, built](ObjectId id) -> std::optional<std::string> {
        if (!built)
            return m_doc.problemWith(id);
        return m_doc.knownProblem(id) ? m_doc.problemWith(id) : std::nullopt;
    };
    const auto addBar = [&] {
        auto* bar = new QTreeWidgetItem(objectRow);
        bar->setText(0, tr("━━━ rollback bar ━━━"));
        bar->setData(0, kRowKindRole, kBarRow);
        bar->setData(0, kOwnerRole, QVariant::fromValue<ObjectId>(top));
        bar->setFlags(Qt::ItemIsEnabled);
        bar->setForeground(0, QColor(0xc0, 0x39, 0x2b));
        bar->setToolTip(0, tr("Drag up to see the shape as it was at an earlier step (new Edges and Emboss "
                              "steps go in here). Drag to the bottom to bring every step back."));
    };
    for (std::size_t i = 0; i < line.size(); ++i) {
        const ObjectId id = line[i];
        const Body& body = *m_doc.find(id);
        auto* item = new QTreeWidgetItem(objectRow);
        QString label = QString::fromStdString(body.name);
        if (body.suppressed)
            label += tr("  (off)");
        item->setText(0, QStringLiteral("%1. %2").arg(i + 1).arg(label));
        item->setText(1, QString::fromStdString(history::describe(m_doc, id)));
        item->setData(0, Qt::UserRole, QVariant::fromValue<ObjectId>(id));
        item->setData(0, kRowKindRole, kStepRow);
        item->setData(0, kOwnerRole, QVariant::fromValue<ObjectId>(top));
        item->setData(0, kStepIndexRole, int(i));
        item->setToolTip(0, tr("%1. Double-click to change it.").arg(QString::fromStdString(typeName(body))));
        if (const auto problem = body.suppressed || i >= shown ? std::nullopt : problemOf(id)) {
            item->setText(0, QStringLiteral("\u26A0 ") + item->text(0));
            item->setText(1, tr("didn't work: left out"));
            const QString why = QString::fromStdString(*problem);
            item->setToolTip(0, tr("This step can't be done any more, so it's left out: %1\nDouble-click to change "
                                   "it, or right-click to delete it or switch it off.")
                                    .arg(why));
            item->setToolTip(1, item->toolTip(0));
            QTreeWidgetItem* objectRowItem = objectRow;
            objectRowItem->setText(1, tr("\u26A0 a step didn't work"));
            objectRowItem->setForeground(1, QColor(0xc0, 0x39, 0x2b));
        }
        const bool dim = body.suppressed || i >= shown;
        const auto problem = dim ? std::nullopt : problemOf(id);
        for (int c = 0; c < 2; ++c) {
            item->setForeground(c, palette().brush(dim ? QPalette::Disabled : QPalette::Active,
                                                   c == 0 ? QPalette::Text : QPalette::PlaceholderText));
            if (problem)
                item->setForeground(c, QColor(0xc0, 0x39, 0x2b));
            if (body.suppressed) {
                QFont f = item->font(c);
                f.setItalic(true);
                item->setFont(c, f);
            }
        }
        // Shapes the step used as tools (the cylinder taken away...), each its own object.
        if (body.isBoolean())
            for (std::size_t k = 1; k < body.boolean->operands.size(); ++k)
                addObjectRow(item, body.boolean->operands[k]);
        item->setExpanded(m_expanded.count({kStepRow, id}) > 0);
        item->setSelected(isSelected(id));
        if (i + 1 == shown)
            addBar();
    }
}

void MainWindow::refreshObjectList()
{
    m_syncingList = true;
    m_objects->clear();
    for (ObjectId id : m_doc.topLevelIds())
        addObjectRow(nullptr, id);
    m_syncingList = false;
}

void MainWindow::rollTo(ObjectId top, std::size_t shown)
{
    auto after = history::rolledTo(m_doc, top, shown);
    if (after.empty())
        return;
    std::vector<Body> before;
    for (const Body& b : after)
        before.push_back(*m_doc.find(b.id));
    const std::size_t total = history::steps(m_doc, top).size();
    run(std::make_unique<EditBodiesCommand>(before, after, shown >= total ? "Roll forward" : "Roll back"));
    if (history::barAt(m_doc, top) < total)
        statusBar()->showMessage(tr("Rolled back: new Edges and Emboss steps go in at the bar. Drag the bar to the "
                                    "bottom to bring every step back."),
                                 8000);
}

void MainWindow::setStepOff(ObjectId id, bool off)
{
    const Body* body = m_doc.find(id);
    if (!body || !body->isBoolean() || body->suppressed == off)
        return;
    Body after = *body;
    after.suppressed = off;
    run(std::make_unique<EditBodyCommand>(*body, after, off ? "Switch step off" : "Switch step on"));
}

void MainWindow::editStep(ObjectId id)
{
    const Body* body = m_doc.find(id);
    if (!body || toolOpen())
        return;
    setSelection(id);
    if (const ObjectId tool = sketchedToolOf(id); tool != kInvalidId) {
        // A pad / revolve (or its step): change its sketch. Holes: the Hole tool.
        if (std::holds_alternative<HoleSpec>(m_doc.find(tool)->spec)) {
            const ObjectId step = m_doc.parentOf(tool);
            if (step != kInvalidId && sketchedToolOf(step) == tool)
                startHole(step, true);
        } else {
            startSketchEdit(id);
        }
        return;
    }
    if (!body->isBoolean())
        return;  // its size and place are in Properties
    switch (body->boolean->op) {
    case BooleanOp::Edges: startEdges(id, true); break;
    case BooleanOp::Shell: startShell(id, true); break;
    case BooleanOp::Thread: startThread(id, true); break;
    case BooleanOp::Socket: startSocket(id, true); break;
    case BooleanOp::Hollow: startHollow(id, true); break;
    case BooleanOp::Mould: startMould(id, true); break;
    case BooleanOp::Port: startPort(id, true); break;
    case BooleanOp::Posed: startPoseFigure(id, true); break;
    case BooleanOp::Paint: startPaint(id, true); break;
    case BooleanOp::Stamp: startStamp(id, true); break;
    case BooleanOp::PanelLines: startPanelLines(id, true); break;
    case BooleanOp::Hinge: startHinge(id, true); break;
    case BooleanOp::MaskSplit: startMask(id, true); break;
    case BooleanOp::Follow: startFollow(id, true); break;
    case BooleanOp::Pattern: startPattern(id, true); break;
    case BooleanOp::Emboss: startEmboss(id, true); break;
    case BooleanOp::Sculpt: startSculpt(id, true); break;
    case BooleanOp::Split:
        if (m_doc.isTopLevel(id))
            startSplit(id, true);
        break;
    default: break;  // Properties shows it; right-click to change the kind of combine
    }
}

std::vector<ObjectId> MainWindow::selectedObjects() const
{
    std::vector<ObjectId> out;
    for (ObjectId id : m_selection) {
        const ObjectId top = history::objectOf(m_doc, id);
        if (m_doc.find(top) && std::find(out.begin(), out.end(), top) == out.end())
            out.push_back(top);
    }
    return out;
}

void MainWindow::fillColourMenu(QMenu* menu)
{
    menu->setToolTipsVisible(true);
    // The swatches, in a grid at the top.
    auto* grid = new QWidget(menu);
    auto* layout = new QGridLayout(grid);
    layout->setContentsMargins(8, 8, 8, 4);
    layout->setSpacing(4);
    for (std::size_t i = 0; i < kSwatches.size(); ++i) {
        const Swatch swatch = kSwatches[i];
        auto* b = new QToolButton(grid);
        b->setFixedSize(26, 26);
        b->setAutoRaise(false);
        b->setToolTip(tr(swatch.name));
        b->setAccessibleName(tr(swatch.name));
        b->setStyleSheet(QStringLiteral("QToolButton { background: #%1; border: 1px solid palette(mid); "
                                        "border-radius: 4px; } QToolButton:hover { border: 2px solid palette(highlight); }")
                             .arg(swatch.rgb, 6, 16, QLatin1Char('0')));
        connect(b, &QToolButton::clicked, this, [this, menu, rgb = swatch.rgb] {
            menu->close();
            // After the click is over: the menu (and the menu that opened it) closes first.
            QTimer::singleShot(0, this, [this, rgb] { colourSelected(rgb); });
        });
        layout->addWidget(b, int(i / 7), int(i % 7));
    }
    auto* swatches = new QWidgetAction(menu);
    swatches->setDefaultWidget(grid);
    menu->addAction(swatches);
    menu->addSeparator();
    QAction* other = menu->addAction(tr("More Colours..."));
    other->setToolTip(tr("Pick any colour"));
    connect(other, &QAction::triggered, this, [this] { QTimer::singleShot(0, this, &MainWindow::chooseOtherColour); });
    QAction* automatic = menu->addAction(tr("Automatic"));
    automatic->setToolTip(tr("Back to the colour Forge3D gave it (a combined shape takes its first part's colour)"));
    connect(automatic, &QAction::triggered, this,
            [this] { QTimer::singleShot(0, this, [this] { colourSelected(std::nullopt); }); });
}

void MainWindow::colourSelected(std::optional<std::uint32_t> colour)
{
    const std::vector<ObjectId> ids = selectedObjects();
    if (ids.empty() || toolOpen())
        return;
    run(std::make_unique<SetColourCommand>(ids, colour));
}

void MainWindow::chooseOtherColour()
{
    const std::vector<ObjectId> ids = selectedObjects();
    if (ids.empty() || toolOpen())
        return;
    const std::uint32_t now = m_doc.colourOf(ids.front());
    const QColor picked = QColorDialog::getColor(QColor::fromRgb(QRgb(now)), this, tr("Colour"));
    if (picked.isValid())
        colourSelected(std::uint32_t(picked.rgb() & 0xFFFFFFu));
}

void MainWindow::showTreeMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_objects->itemAt(pos);
    if (!item || toolOpen())
        return;
    const int kind = item->data(0, kRowKindRole).toInt();
    QMenu menu(this);
    if (kind == kBarRow) {
        const ObjectId top = item->data(0, kOwnerRole).value<ObjectId>();
        menu.addAction(tr("Roll forward to the end"), this,
                       [this, top] { rollTo(top, history::steps(m_doc, top).size()); });
        menu.exec(m_objects->viewport()->mapToGlobal(pos));
        return;
    }
    const ObjectId id = item->data(0, Qt::UserRole).value<ObjectId>();
    const Body* body = m_doc.find(id);
    if (!body)
        return;
    const bool isStep = kind == kStepRow;
    const ObjectId top = isStep ? item->data(0, kOwnerRole).value<ObjectId>() : id;
    menu.addAction(tr("Change..."), this, [this, id] { editStep(id); });
    if (!isStep) {
        QMenu* colour = menu.addMenu(ribbonIcon(QStringLiteral("colour")), tr("Colour"));
        fillColourMenu(colour);
        if (!isSelected(id))
            setSelection(id);  // the colour goes on the shape clicked
    }
    if (body->isBoolean()) {
        menu.addAction(body->suppressed ? tr("Switch this step on") : tr("Switch this step off"), this,
                       [this, id, off = !body->suppressed] { setStepOff(id, off); });
        const BooleanOp op = body->boolean->op;
        if (op == BooleanOp::Merge || op == BooleanOp::Subtract || op == BooleanOp::Intersect) {
            QMenu* change = menu.addMenu(tr("Change to"));
            for (BooleanOp to : {BooleanOp::Merge, BooleanOp::Subtract, BooleanOp::Intersect}) {
                QAction* a = change->addAction(QString::fromStdString(toString(to)), this, [this, id, to] {
                    const Body* now = m_doc.find(id);
                    if (!now)
                        return;
                    Body after = *now;
                    after.boolean->op = to;
                    run(std::make_unique<EditBodyCommand>(*now, after, "Change to " + toString(to)));
                });
                a->setEnabled(to != op);
            }
        }
    }
    if (isStep) {
        menu.addSeparator();
        const std::size_t index = std::size_t(item->data(0, kStepIndexRole).toInt());
        menu.addAction(tr("Roll back to here"), this, [this, top, index] { rollTo(top, index + 1); });
        if (history::barAt(m_doc, top) < history::steps(m_doc, top).size())
            menu.addAction(tr("Roll forward to the end"), this,
                           [this, top] { rollTo(top, history::steps(m_doc, top).size()); });
        if (body->isBoolean() && !m_doc.isTopLevel(id)) {
            menu.addSeparator();
            menu.addAction(tr("Delete this step"), this, [this, id] {
                run(std::make_unique<RemoveStepCommand>(id));
                statusBar()->showMessage(tr("Step deleted. Any shapes it used as tools are back on the plate."),
                                         6000);
            });
        }
    }
    menu.exec(m_objects->viewport()->mapToGlobal(pos));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Dragging the rollback bar in the Objects tree.
    if (watched != m_objects->viewport())
        return QMainWindow::eventFilter(watched, event);
    const auto pointOf = [](QEvent* e) { return static_cast<QMouseEvent*>(e)->position().toPoint(); };
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        QTreeWidgetItem* item = m_objects->itemAt(pointOf(event));
        if (me->button() == Qt::LeftButton && item && item->data(0, kRowKindRole).toInt() == kBarRow &&
            !toolOpen()) {
            m_barDrag = item->data(0, kOwnerRole).value<ObjectId>();
            m_barDragShown = history::barAt(m_doc, m_barDrag);
            m_objects->viewport()->setCursor(Qt::SizeVerCursor);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonDblClick: {
        auto* me = static_cast<QMouseEvent*>(event);
        QTreeWidgetItem* item = m_objects->itemAt(pointOf(event));
        if (me->button() != Qt::LeftButton || !item || item->data(0, kRowKindRole).toInt() == kBarRow)
            break;
        const ObjectId id = item->data(0, Qt::UserRole).value<ObjectId>();
        const Body* body = m_doc.find(id);
        if (!body)
            return true;
        const BooleanOp op = body->isBoolean() ? body->boolean->op : BooleanOp::Merge;
        const bool opens = sketchedToolOf(id) != kInvalidId ||
                           (body->isBoolean() && (op == BooleanOp::Edges || op == BooleanOp::Shell ||
                                                  op == BooleanOp::Follow || op == BooleanOp::Pattern ||
                                                  op == BooleanOp::Emboss || op == BooleanOp::Sculpt ||
                                                  op == BooleanOp::Thread || op == BooleanOp::Socket ||
                                                  op == BooleanOp::Hinge || op == BooleanOp::MaskSplit ||
                                                  op == BooleanOp::Hollow || op == BooleanOp::Stamp ||
                                                  op == BooleanOp::PanelLines || op == BooleanOp::Posed || op == BooleanOp::Paint ||
                                                  op == BooleanOp::Mould || op == BooleanOp::Port ||
                                                  (op == BooleanOp::Split && m_doc.isTopLevel(id))));
        if (!opens) {
            // Nothing to open: show or hide what's inside, as a list does.
            if (item->childCount() > 0)
                item->setExpanded(!item->isExpanded());
            return true;
        }
        // Open it once the first click's selection has settled (it's applied a moment later).
        QTimer::singleShot(0, this, [this, id] {
            if (m_doc.find(id))
                editStep(id);
        });
        return true;
    }
    case QEvent::MouseMove:
        if (m_barDrag != kInvalidId) {
            // The bar goes after the step under the mouse (before it, in its top half).
            const QPoint p = pointOf(event);
            QTreeWidgetItem* item = m_objects->itemAt(p);
            while (item && item->data(0, kRowKindRole).toInt() == kObjectRow && item->parent())
                item = item->parent();  // a tool row: use its step
            if (item && item->data(0, kRowKindRole).toInt() == kStepRow &&
                item->data(0, kOwnerRole).value<ObjectId>() == m_barDrag) {
                const QRect r = m_objects->visualItemRect(item);
                const int index = item->data(0, kStepIndexRole).toInt();
                m_barDragShown = std::size_t(std::max(1, p.y() < r.center().y() ? index : index + 1));
                const std::size_t total = history::steps(m_doc, m_barDrag).size();
                statusBar()->showMessage(m_barDragShown >= total
                                             ? tr("Let go to show every step")
                                             : tr("Let go to show the first %1 of %2 steps")
                                                   .arg(m_barDragShown)
                                                   .arg(total));
            }
            return true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (m_barDrag != kInvalidId) {
            const ObjectId top = m_barDrag;
            m_barDrag = kInvalidId;
            m_objects->viewport()->unsetCursor();
            statusBar()->clearMessage();
            const std::size_t shown = m_barDragShown;
            QTimer::singleShot(0, this, [this, top, shown] { rollTo(top, shown); });
            return true;
        }
        break;
    default: break;
    }
    return QMainWindow::eventFilter(watched, event);
}

QMatrix4x4 MainWindow::previewFrame(ObjectId target, bool editing) const
{
    if (editing)
        return toQMatrix(m_doc.worldTransform(target));
    const ObjectId holder = m_doc.parentOf(target);
    return holder == kInvalidId ? QMatrix4x4() : toQMatrix(m_doc.worldTransform(holder));
}

bool MainWindow::rolledBackSelected() const
{
    for (ObjectId id : m_selection)
        if (m_doc.isTopLevel(id) && history::barAt(m_doc, id) < history::steps(m_doc, id).size())
            return true;
    return false;
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
                       .arg(name, QString::fromLatin1(build::kAppName), versionText()));
    setWindowModified(!m_stack.isClean());
    relockForSketch();
}

void MainWindow::updatesAtStart()
{
    Preferences settings;
    if (!settings.contains(QStringLiteral("updates/atStart"))) {
        // Asked once; Help → Check for Updates at Start changes it later.
        QMessageBox ask(QMessageBox::Question, tr("Forge3D"),
                        tr("Should Forge3D check for a newer version each time it starts?"), QMessageBox::NoButton,
                        this);
        ask.setInformativeText(tr("It only asks GitHub for the latest version number; nothing about you or your "
                                  "models is sent. You can change this any time in the Help menu."));
        ask.addButton(tr("Yes, check at start"), QMessageBox::YesRole);
        QPushButton* no = ask.addButton(tr("No, I'll check myself"), QMessageBox::NoRole);
        ask.exec();
        const bool yes = ask.clickedButton() != static_cast<QAbstractButton*>(no);
        m_checkAtStart->setChecked(yes);  // saves it
        if (!yes)
            return;
    }
    if (m_checkAtStart->isChecked())
        checkForUpdates(true);
}

void MainWindow::checkForUpdates(bool quietly)
{
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QString::fromLatin1(updates::kReleasesApi)));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Forge3D/%1").arg(QString::fromLatin1(build::kVersion)));
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_network->get(request);
    if (!quietly)
        statusBar()->showMessage(tr("Checking for updates..."));
    connect(reply, &QNetworkReply::finished, this, [this, reply, quietly] {
        reply->deleteLater();
        if (!quietly)
            statusBar()->clearMessage();
        const QString releases = QString::fromLatin1(updates::kReleasesPage);
        std::optional<updates::Release> newer;
        QString problem;
        if (reply->error() != QNetworkReply::NoError) {
            problem = reply->errorString();
        } else {
            try {
                newer = updates::newerRelease(reply->readAll().toStdString(), build::kVersion);
            } catch (const std::exception& e) {
                problem = QString::fromUtf8(e.what());
            }
        }
        if (!problem.isEmpty()) {
            qWarning().noquote() << "Update check failed:" << problem;
            if (!quietly)
                QMessageBox::warning(this, tr("Check for Updates"),
                                     tr("Couldn't check for updates (%1). You can look on the <a href=\"%2\">"
                                        "releases page</a> instead.")
                                         .arg(problem.toHtmlEscaped(), releases));
            return;
        }
        if (!newer) {
            qInfo() << "Update check: up to date";
            if (!quietly)
                QMessageBox::information(this, tr("Check for Updates"),
                                         tr("You have the newest Forge3D (%1).").arg(versionText()));
            return;
        }
        const QString name = QString::fromStdString(newer->name);
        const QString url = QString::fromStdString(newer->url);
        qInfo().noquote() << "Update check: newer release" << name << url;
        if (quietly) {
            // A note in the status bar, out of the way.
            auto* note = new QLabel(tr("<a href=\"%1\">%2 is out: download</a>").arg(url, name.toHtmlEscaped()));
            note->setOpenExternalLinks(true);
            statusBar()->addPermanentWidget(note);
            return;
        }
        QMessageBox box(QMessageBox::Information, tr("Check for Updates"),
                        tr("%1 is out (you have %2).").arg(name, versionText()), QMessageBox::NoButton, this);
        QPushButton* download = box.addButton(tr("Download"), QMessageBox::AcceptRole);
        box.addButton(tr("Later"), QMessageBox::RejectRole);
        box.exec();
        if (box.clickedButton() == static_cast<QAbstractButton*>(download))
            QDesktopServices::openUrl(QUrl(url));
    });
}

void MainWindow::showQuickStart()
{
    if (!m_quickStart) {
        m_quickStart = new QDialog(this);
        m_quickStart->setWindowTitle(tr("Quick Start - Forge3D"));
        auto* layout = new QVBoxLayout(m_quickStart);
        auto* browser = new QTextBrowser;
        browser->setOpenExternalLinks(true);
        browser->setSource(QUrl(QStringLiteral("qrc:/help/quick-start.html")));
        layout->addWidget(browser);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, m_quickStart, &QDialog::close);
        layout->addWidget(buttons);
        m_quickStart->resize(760, std::min(820, height() - 40));
    }
    // Beside the main window, so the steps can be followed while it's open.
    m_quickStart->show();
    m_quickStart->raise();
    m_quickStart->activateWindow();
}

void MainWindow::showFigureTools()
{
    if (!m_figureTools) {
        m_figureTools = new QDialog(this);
        m_figureTools->setWindowTitle(tr("Figure, Toy and Kit Tools - Forge3D"));
        auto* layout = new QVBoxLayout(m_figureTools);
        auto* browser = new QTextBrowser;
        browser->setOpenExternalLinks(true);
        browser->setSource(QUrl(QStringLiteral("qrc:/help/figure-tools.html")));
        layout->addWidget(browser);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(buttons, &QDialogButtonBox::rejected, m_figureTools, &QDialog::close);
        layout->addWidget(buttons);
        m_figureTools->resize(760, std::min(820, height() - 40));
    }
    m_figureTools->show();
    m_figureTools->raise();
    m_figureTools->activateWindow();
}

QWidget* MainWindow::ribbonButtonFor(QAction* action, int tab) const
{
    if (!m_ribbonTabs || tab >= m_ribbonTabs->count())
        return nullptr;
    for (auto* button : m_ribbonTabs->widget(tab)->findChildren<QToolButton*>())
        if (button->defaultAction() == action)
            return button;
    return nullptr;
}

void MainWindow::startTour()
{
    if (m_tour)
        return;
    if (toolOpen()) {
        statusBar()->showMessage(tr("Finish or cancel the tool you're in first (Esc), then take the tour."), 6000);
        return;
    }
    const auto tab = [this](int i) { return [this, i] { m_ribbonTabs->setCurrentIndex(i); }; };
    const auto onTab = [this](QAction* action, int i) { return [this, action, i] { return ribbonButtonFor(action, i); }; };
    std::vector<Tour::Step> steps{
        {tr("Welcome to Forge3D"),
         tr("This short tour shows where things are, so you can make and print your first part in a few "
            "minutes. Try each step as you go; the window keeps working while the tour is open."),
         {}, tab(0)},
        {tr("Add a shape"),
         tr("Start here: <b>Add Shape</b> puts a box, cylinder, sphere, letters and more on the plate. "
            "<b>Import</b> brings in an STL, 3MF or OBJ."),
         onTab(m_addMenu->menuAction(), 0), tab(0)},
        {tr("Size it exactly"),
         tr("<b>Properties</b> shows the selected shape: type its size in millimetres, where it sits, how "
            "it's turned, and its name."),
         [this] { return static_cast<QWidget*>(m_propsDock); }, tab(0)},
        {tr("Move, turn and resize"),
         tr("Pick <b>Move</b>, <b>Rotate</b> or <b>Resize</b>, then drag the handles on the shape. "
            "<b>Snap</b> keeps to whole millimetres and neat angles. Turn the view by dragging with the "
            "right mouse button; the wheel zooms."),
         onTab(m_modeMove, 0), tab(0)},
        {tr("Combine shapes"),
         tr("Select two shapes (<b>Ctrl+click</b> adds to the selection). On the <b>Design</b> tab, "
            "<b>Merge</b> joins them and "
            "<b>Subtract</b> cuts the others out of the first one you picked: that's how you make a hole."),
         onTab(m_subtract, 1), tab(1)},
        {tr("Every step, kept"),
         tr("<b>Objects</b> lists your shapes and the steps that made them. <b>Double-click</b> a step to "
            "change it later; everything after it follows."),
         [this] { return m_objects->parentWidget(); }, tab(0)},
        {tr("Words and pictures"),
         tr("Also on the <b>Design</b> tab, <b>Emboss</b> raises or engraves your name, or a logo or photo, on "
            "any surface, even round a cylinder. Sketches are here too; holes, threads, sockets, shells and "
            "rounded edges are on the <b>Features</b> tab."),
         onTab(m_emboss, 1), tab(1)},
        {tr("Ready to print"),
         tr("On the <b>Print/Save</b> tab, <b>Send to Slicer</b> opens it in your slicer (or <b>Export</b> "
            "saves an STL or 3MF). If it needs a different face down first, <b>Design → Lay on Face</b>."),
         onTab(m_sendToSlicer, 5), tab(5)},
        {tr("That's it"),
         tr("<b>Help → Quick Start Guide</b> walks through a first print (a key tag with your name) with "
            "pictures. <b>Help → Take the Tour</b> shows this again. Point at any button for a tip, and "
            "<b>Ctrl+Z</b> undoes anything."),
         {}, tab(0)},
    };
    m_tour = new Tour(this, std::move(steps));
    connect(m_tour, &Tour::finished, this, [this] {
        Preferences().setValue(QStringLiteral("tour/seen"), true);
        m_ribbonTabs->setCurrentIndex(0);
        QTimer::singleShot(400, this, &MainWindow::updatesAtStart);
    });
    m_tour->start();
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
            .arg(versionText(), QString::fromLatin1(qVersion()),
                 QString::fromLatin1(OCC_VERSION_COMPLETE), m_viewport->glVersionString()));
}

namespace {

gp_Trsf toGp(const Transform& t)
{
    gp_Trsf trsf;
    trsf.SetValues(t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3],  //
                   t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3],  //
                   t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);
    return trsf;
}

// A shape as it stands on the plate (the whole step, rounding and all).
TopoDS_Shape inPlate(const Document& doc, ObjectId id)
{
    return BRepBuilderAPI_Transform(doc.shape(id), toGp(doc.worldTransform(id)), true).Shape();
}

}  // namespace

// ---- Sketches ---------------------------------------------------------------------

void MainWindow::createSketchBar()
{
    m_sketchBar = new QToolBar(tr("Sketch"), this);
    m_sketchBar->setObjectName(QStringLiteral("SketchToolBar"));
    m_sketchBar->setMovable(false);
    m_sketchBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addToolBarBreak();
    addToolBar(m_sketchBar);
    m_sketchBar->hide();

    const auto add = [this](const QString& text, const QKeySequence& key, const QString& tip, auto&& slot) {
        auto* a = new QAction(text, m_sketchBar);
        if (!key.isEmpty()) {
            a->setShortcut(key);
            a->setToolTip(QStringLiteral("%1 (%2)").arg(tip, key.toString(QKeySequence::NativeText)));
        } else {
            a->setToolTip(tip);
        }
        connect(a, &QAction::triggered, this, slot);
        m_sketchBar->addAction(a);
        return a;
    };
    auto* tools = new QActionGroup(m_sketchBar);
    const struct {
        SketchView::Tool tool;
        QString text;
        QKeySequence key;
        QString tip;
    } toolList[] = {
        {SketchView::Tool::Select, tr("Pick"), {}, tr("Pick and drag (Esc gets back here)")},
        {SketchView::Tool::Line, tr("Line"), Qt::Key_L, tr("Lines, one after another; click the first point to close")},
        {SketchView::Tool::Rectangle, tr("Rectangle"), Qt::Key_R, tr("A rectangle from two corners")},
        {SketchView::Tool::Circle, tr("Circle"), Qt::Key_C, tr("A circle from its middle and a point on its edge")},
        {SketchView::Tool::Arc, tr("Arc"), Qt::Key_A, tr("An arc: its middle, then its start, then its end")},
        {SketchView::Tool::Slot, tr("Slot"), Qt::Key_S, tr("A slot: the middles of its round ends, then its width")},
        {SketchView::Tool::Point, tr("Point"), Qt::Key_P, tr("A point on its own (where a hole goes)")},
        {SketchView::Tool::Trim, tr("Trim"), Qt::Key_X,
         tr("Click pieces of lines and curves to take them away, back to where other lines cross them")},
        {SketchView::Tool::Text, tr("Text"), Qt::Key_W,
         tr("Words whose letters are outlines: click where they start. Double-click them later to change them")},
    };
    for (const auto& t : toolList) {
        QAction* a = add(t.text, t.key, t.tip, [this, tool = t.tool] { m_sketchView->setTool(tool); });
        a->setCheckable(true);
        tools->addAction(a);
        m_sketchToolActions.push_back(a);
    }
    m_sketchToolActions.front()->setChecked(true);
    m_sketchBar->addSeparator();
    using sketch::Rule;
    add(tr("Horizontal"), Qt::Key_H, tr("Make the picked lines level (or two points level with each other)"),
        [this] { m_sketchView->addRule(Rule::Horizontal); });
    add(tr("Vertical"), Qt::Key_V, tr("Make the picked lines upright (or two points above each other)"),
        [this] { m_sketchView->addRule(Rule::Vertical); });
    add(tr("Parallel"), {}, tr("Make two picked lines parallel"), [this] { m_sketchView->addRule(Rule::Parallel); });
    add(tr("Perpendicular"), {}, tr("Make two picked lines meet square"),
        [this] { m_sketchView->addRule(Rule::Perpendicular); });
    add(tr("Equal"), Qt::Key_E, tr("Make the picked lines the same length, or circles / arcs the same size"),
        [this] { m_sketchView->addRule(Rule::Equal); });
    add(tr("Tangent"), Qt::Key_T, tr("Make a line touch a circle / arc (or two circles touch) smoothly"),
        [this] { m_sketchView->addRule(Rule::Tangent); });
    add(tr("Coincident"), Qt::Key_O, tr("Join two picked points into one, or put a point on a line / circle"),
        [this] { m_sketchView->addRule(Rule::Coincident); });
    add(tr("Symmetric"), {}, tr("Mirror two picked points about a picked line"),
        [this] { m_sketchView->addRule(Rule::Symmetric); });
    add(tr("Lock"), {}, tr("Lock the picked points where they are"), [this] { m_sketchView->addRule(Rule::Fixed); });
    m_sketchBar->addSeparator();
    add(tr("Dimension"), Qt::Key_D,
        tr("A size on what's picked: a line's length, two points' distance, a circle's radius, two lines' angle"),
        [this] { m_sketchView->addDimension(0); });
    add(tr("Across"), QKeySequence(Qt::SHIFT | Qt::Key_H),
        tr("A distance measured across only (X): a line, two points, or one point from the origin"),
        [this] { m_sketchView->addDimension(1); });
    add(tr("Up"), QKeySequence(Qt::SHIFT | Qt::Key_V),
        tr("A distance measured up only (Y): a line, two points, or one point from the origin"),
        [this] { m_sketchView->addDimension(2); });
    m_sketchBar->addSeparator();
    add(tr("Helper"), Qt::Key_G,
        tr("Turn the picked lines into helper lines (dashed: used by rules, not part of the shape), or back"),
        [this] { m_sketchView->toggleConstruction(); });
    add(tr("Round corner"), QKeySequence(Qt::SHIFT | Qt::Key_R),
        tr("Round the picked corners (where two lines meet) with an arc"), [this] { m_sketchView->roundCorners(); });
    add(tr("Offset"), QKeySequence(Qt::SHIFT | Qt::Key_O),
        tr("Copy the picked outline a set distance out (or in): pick any line or curve of it"),
        [this] { m_sketchView->offsetPicked(); });
    add(tr("Mirror"), Qt::Key_M,
        tr("Copy what's picked to the other side of a line: pick things, Ctrl+click the mirror line last "
           "(or pick a helper line), then Mirror"),
        [this] { m_sketchView->mirrorPicked(); });
    m_sketchBar->addSeparator();
    add(tr("Delete"), QKeySequence::Delete, tr("Delete what's picked"), [this] { m_sketchView->deleteSelection(); });
    add(tr("Undo"), QKeySequence::Undo, tr("Undo the last change to the sketch"), [this] { m_sketchView->undo(); });
    add(tr("Redo"), QKeySequence::Redo, tr("Redo"), [this] { m_sketchView->redo(); });
    add(tr("Fit"), Qt::Key_F, tr("Show the whole sketch"), [this] { m_sketchView->fit(); });
    // While sketching only: the window's own commands are off then.
    for (QAction* a : m_sketchBar->actions())
        a->setEnabled(false);
}

ObjectId MainWindow::sketchedToolOf(ObjectId id) const
{
    const Body* body = m_doc.find(id);
    if (!body)
        return kInvalidId;
    if (!body->isBoolean())
        return isSketched(body->spec) ? id : kInvalidId;
    const BooleanOp op = body->boolean->op;
    if ((op != BooleanOp::Merge && op != BooleanOp::Subtract) || body->boolean->operands.size() != 2)
        return kInvalidId;
    const ObjectId added = body->boolean->operands[1];
    const Body* tool = m_doc.find(added);
    return tool && !tool->isBoolean() && isSketched(tool->spec) ? added : kInvalidId;
}

bool MainWindow::addWithStep(ObjectId target, Body tool, BooleanOp op, bool throughAll, const std::string& label,
                             const std::string& resultName, const Bounds& toolBox)
{
    const ObjectId toolId = tool.id;
    BooleanSpec spec;
    spec.op = op;
    spec.operands = {target, toolId};
    spec.throughAll = throughAll;
    Bounds box = m_doc.worldBounds(target);
    if (op == BooleanOp::Merge) {
        if (toolBox.valid) {
            box.expand(toolBox.min[0], toolBox.min[1], toolBox.min[2]);
            box.expand(toolBox.max[0], toolBox.max[1], toolBox.max[2]);
        }
    }
    const auto centre = box.center();
    spec.pivot = {centre[0], centre[1], centre[2]};
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName(resultName);
    result.boolean = spec;
    const ObjectId resultId = result.id;
    m_stack.beginMacro(label);
    try {
        m_stack.execute(std::make_unique<AddBodyCommand>(std::move(tool)));
        m_stack.execute(std::make_unique<CombineCommand>(std::move(result)));
    } catch (const std::exception& e) {
        qWarning().noquote() << QString::fromStdString(label) << "failed:" << e.what();
        m_stack.abortMacro();  // undo what was done of it
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("%1 did not work:\n%2").arg(QString::fromStdString(label), QString::fromUtf8(e.what())));
        return false;
    }
    m_stack.endMacro();
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    setSelection(resultId);
    return true;
}

void MainWindow::sketchClicked()
{
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    if (m_selection.empty()) {
        m_sketchMode = SketchMode{};
        openSketch(sketch::Sketch{}, {}, tr("Sketch on the plate"), false, SketchPanel::Options{});
        return;
    }
    const ObjectId id = selectedOne();
    if (!sketchTargetOk(id))
        return;
    m_sketchMode = SketchMode{};
    m_sketchMode.picking = true;
    m_sketchMode.target = id;
    statusBar()->showMessage(tr("Sketch: click the flat face of '%1' to draw on. Esc cancels.")
                                 .arg(QString::fromStdString(m_doc.find(id)->name)));
    refreshScene();
}

bool MainWindow::sketchTargetOk(ObjectId id)
{
    if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
        statusBar()->showMessage(tr("Sketch: select one shape on the plate (to sketch on one of its faces), or "
                                    "nothing (to sketch on the plate)."),
                                 8000);
        return false;
    }
    if (rolledBackSelected()) {
        statusBar()->showMessage(tr("'%1' is rolled back: roll it forward to the end before sketching on it.")
                                     .arg(QString::fromStdString(m_doc.find(id)->name)),
                                 8000);
        return false;
    }
    if (m_doc.usesMeshEngine(id)) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("Sketches go on solid shapes made in Forge3D, not on imported meshes or shapes "
                                    "made with them. Deselect it to sketch on the plate instead."));
        return false;
    }
    return true;
}

void MainWindow::sketchOnSection()
{
    if (!m_sectionDock || !m_sectionDock->isVisible()) {
        statusBar()->showMessage(tr("Turn Section View on first (View tab), and slide the cut to where you want "
                                    "to draw."),
                                 8000);
        return;
    }
    // Cut across X: the side plane; across Y: the front plane; across Z: the top plane.
    static const int planeFor[] = {1, 0, 2};
    sketchOnPlane(planeFor[m_sectionPanel->axis()], true);
}

void MainWindow::sketchOnPlane(int which, bool atSection)
{
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    static const char* names[] = {"front", "side", "top"};
    const QString planeName = tr(names[which]);
    // On the section's cut: the same plane, where the cut is. What's made goes into the
    // part that shows (the hidden side is the other way).
    static const int axisFor[] = {1, 0, 2};
    const auto atCut = [&](Vec3 centre) {
        if (atSection)
            centre[std::size_t(axisFor[which])] = m_sectionPanel->position();
        return centre;
    };
    SketchPanel::Options options;
    if (atSection) {
        const Transform plane = sketch::middlePlane(which, {0, 0, 0});
        const auto& section = m_viewport->section();
        const double along = section ? plane.m[0][2] * section->normal.x() + plane.m[1][2] * section->normal.y() +
                                           plane.m[2][2] * section->normal.z()
                                     : 0.0;
        options.otherWay = along > 0.0;  // the plane faces the hidden side: go the other way
    }
    if (m_selection.empty()) {
        // Through the plate's middle: the front and side planes stand up from it.
        m_sketchMode = SketchMode{};
        m_sketchMode.plane = which;
        m_sketchMode.frame = sketch::middlePlane(which, atCut({0, 0, 0}));
        openSketch(sketch::Sketch{}, {},
                   atSection ? tr("Sketch on the section view's cut") : tr("Sketch on the %1 plane").arg(planeName),
                   false, options);
        return;
    }
    const ObjectId id = selectedOne();
    if (!sketchTargetOk(id))
        return;
    const auto c = m_doc.worldBounds(id).center();
    m_sketchMode = SketchMode{};
    m_sketchMode.target = id;
    m_sketchMode.plane = which;
    m_sketchMode.frame = sketch::middlePlane(which, atCut({c[0], c[1], c[2]}));
    std::vector<std::vector<QPointF>> reference;
    try {
        for (const auto& line : sketch::sectionOutlines(inPlate(m_doc, id), m_sketchMode.frame)) {
            std::vector<QPointF> q;
            for (const auto& p : line)
                q.emplace_back(p.x, p.y);
            reference.push_back(std::move(q));
        }
    } catch (const std::exception& e) {
        qWarning() << "Cutting through the shape failed:" << e.what();
    }
    if (atSection && reference.empty())
        statusBar()->showMessage(tr("The cut doesn't pass through '%1' here, so there's no outline to snap to.")
                                     .arg(QString::fromStdString(m_doc.find(id)->name)),
                                 8000);
    options.length = 5.0;
    openSketch(sketch::Sketch{}, std::move(reference),
               atSection ? tr("Sketch on the section view's cut through '%1'")
                               .arg(QString::fromStdString(m_doc.find(id)->name))
                         : tr("Sketch on the %1 plane through the middle of '%2'")
                               .arg(planeName, QString::fromStdString(m_doc.find(id)->name)),
               true, options);
}

void MainWindow::sketchFaceClick(const QVector3D& origin, const QVector3D& direction)
{
    const ObjectId target = m_sketchMode.target;
    const Body* body = m_doc.find(target);
    if (!body) {
        m_sketchMode = SketchMode{};
        return;
    }
    std::optional<sketch::FacePlane> plane;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        const TopoDS_Shape shape = pickableShape(m_doc, target);
        // With the view cut open, a click on the cut face itself means: sketch there.
        // (The ray comes through the hidden side and meets the cut inside the shape.)
        if (const auto& section = m_viewport->section();
            section && (!section->selectedOnly || m_viewport->sectionShapes().count(target))) {
            const float towards = QVector3D::dotProduct(direction, section->normal);
            const float t = towards < -1e-6f
                                ? (section->offset - QVector3D::dotProduct(origin, section->normal)) / towards
                                : -1.0f;
            if (t > 0.0f) {
                const QVector3D p = origin + direction * t;
                BRepClass3d_SolidClassifier inside(shape, gp_Pnt(p.x(), p.y(), p.z()), 1e-6);
                if (inside.State() == TopAbs_IN) {
                    m_sketchMode = SketchMode{};
                    sketchOnSection();
                    return;
                }
            }
        }
        const auto hit = edges::faceUnder(shape, {origin.x(), origin.y(), origin.z()},
                                          {direction.x(), direction.y(), direction.z()});
        if (!hit) {
            statusBar()->showMessage(tr("Sketch: click on '%1' itself, on the flat face to draw on. Esc cancels.")
                                         .arg(QString::fromStdString(body->name)));
            return;
        }
        plane = sketch::planeOfFace(shape, hit->face);
    } catch (const std::exception& e) {
        qWarning() << "Finding the face failed:" << e.what();
    }
    if (!plane) {
        statusBar()->showMessage(tr("Sketch: that face is curved. Click a flat face. Esc cancels."));
        return;
    }
    m_sketchMode.picking = false;
    m_sketchMode.frame = plane->frame;
    std::vector<std::vector<QPointF>> reference;
    for (const auto& line : plane->outlines) {
        std::vector<QPointF> q;
        for (const auto& p : line)
            q.emplace_back(p.x, p.y);
        reference.push_back(std::move(q));
    }
    SketchPanel::Options options;
    options.length = 5.0;
    openSketch(sketch::Sketch{}, std::move(reference),
               tr("Sketch on a face of '%1'").arg(QString::fromStdString(body->name)), true, options);
}

void MainWindow::startSketchEdit(ObjectId id)
{
    const ObjectId toolId = sketchedToolOf(id);
    const Body* tool = m_doc.find(toolId);
    if (!tool || std::holds_alternative<HoleSpec>(tool->spec))
        return;
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    m_sketchMode = SketchMode{};
    m_sketchMode.pad = toolId;
    const ObjectId holder = m_doc.parentOf(toolId);
    if (holder != kInvalidId && sketchedToolOf(holder) == toolId)
        m_sketchMode.step = holder;
    m_sketchMode.frame = m_doc.worldTransform(toolId);
    const Body* step = m_doc.find(m_sketchMode.step);
    const bool cuts = step && step->boolean->op == BooleanOp::Subtract;
    SketchPanel::Options options;
    sketch::Sketch drawn;
    int axis = -1;
    if (const auto* pad = std::get_if<PadSpec>(&tool->spec)) {
        drawn = pad->sketch;
        options.make = cuts ? SketchPanel::Make::Pocket : SketchPanel::Make::Pad;
        options.length = pad->length;
        options.otherWay = cuts ? !pad->reversed : pad->reversed;
        options.bothWays = pad->midplane;
        options.taper = pad->taper;
        options.throughAll = step && step->boolean->throughAll;
    } else if (const auto* loft = std::get_if<LoftSpec>(&tool->spec)) {
        if (loft->sections.empty())
            return;
        drawn = loft->sections.front().sketch;
        options.make = SketchPanel::Make::Loft;
        options.ruled = loft->ruled;
    } else if (const auto* sweep = std::get_if<SweepSpec>(&tool->spec)) {
        drawn = sweep->profile;
        options.make = SketchPanel::Make::Sweep;
        options.spiral = sweep->spiral;
        options.pitch = sweep->pitch;
        options.turns = sweep->turns;
        options.leftHand = sweep->leftHand;
    } else if (const auto* revolve = std::get_if<RevolveSpec>(&tool->spec)) {
        drawn = revolve->sketch;
        axis = revolve->axis;
        options.make = cuts ? SketchPanel::Make::Groove : SketchPanel::Make::Revolve;
        options.angle = revolve->angle;
        options.otherWay = revolve->reversed;
        options.bothWays = revolve->midplane;
    } else {
        return;
    }
    const QString name = QString::fromStdString((step ? step : tool)->name);
    // The face it was drawn on, to show again.
    std::vector<std::vector<QPointF>> reference;
    if (step) {
        try {
            const ObjectId under = step->boolean->operands.front();
            if (!m_doc.usesMeshEngine(under)) {
                const TopoDS_Shape shape = pickableShape(m_doc, under);
                auto lines = sketch::outlinesOnPlane(shape, m_sketchMode.frame);
                if (lines.empty())  // drawn on a middle plane: show the cut through it
                    lines = sketch::sectionOutlines(shape, m_sketchMode.frame);
                for (const auto& line : lines) {
                    std::vector<QPointF> q;
                    for (const auto& p : line)
                        q.emplace_back(p.x, p.y);
                    reference.push_back(std::move(q));
                }
            }
        } catch (const std::exception& e) {
            qWarning() << "Finding the face under the sketch failed:" << e.what();
        }
    }
    openSketch(drawn, std::move(reference), tr("Change the sketch of '%1'").arg(name), step != nullptr, options);
    if (const auto* loft = std::get_if<LoftSpec>(&tool->spec)) {
        m_sketchMode.loft = *loft;
        m_sketchPanel->setLoftOutline(0, int(loft->sections.size()), 0.0);
    }
    if (const auto* sweep = std::get_if<SweepSpec>(&tool->spec))
        m_sketchMode.sweep = *sweep;
    if (axis >= 0)
        m_sketchView->pickGeo(axis);  // the line it spins round
}

void MainWindow::openSketch(const sketch::Sketch& sketch, std::vector<std::vector<QPointF>> reference,
                            const QString& title, bool onFace, const SketchPanel::Options& options)
{
    if (m_splitMode.active)
        endSplit();
    if (m_embossMode.active)
        endEmboss();
    if (m_edgesMode.active)
        endEdges();
    m_sketchMode.active = true;
    m_sketchMode.reference = reference;
    const bool editing = m_sketchMode.pad != kInvalidId;
    m_central->setCurrentWidget(m_sketchView);
    if (m_holeMode.positioning)
        m_sketchPanel->startPositions(
            title, tr("Each point on its own (Point tool, P) is a hole. Add dimensions to place them exactly."));
    else if (m_followMode.sketching == 1)
        m_sketchPanel->startPositions(title, tr("Draw the path: lines, arcs, circles or slots, joined or not. "
                                                "The grey dot is the spot you clicked; it's laid over the surface "
                                                "from there."));
    else if (m_followMode.sketching == 2)
        m_sketchPanel->startPositions(title, tr("Draw the shape across the path as one closed outline: the "
                                                "dashed line is the surface, up is out of it, the grey dot is "
                                                "where the path runs. A groove is the part below the line."));
    else
        m_sketchPanel->start(title, onFace, editing, options);
    if (!m_holeMode.active && !m_followMode.active)
        m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_holePanel->hide();
    m_followPanel->hide();
    m_sketchPanel->show();
    m_sketchView->start(sketch, std::move(reference));
    if (m_holeMode.positioning)
        m_sketchView->setTool(SketchView::Tool::Point);
    else if (m_followMode.sketching != 0)
        m_sketchView->setTool(sketch::isEmpty(m_sketchView->sketch()) ? SketchView::Tool::Line : SketchView::Tool::Select);
    else if (!editing)
        m_sketchView->setTool(SketchView::Tool::Line);
    m_sketchBar->show();
    lockForSketch(true);
    m_sketchView->setFocus();
    statusBar()->showMessage(m_holeMode.positioning      ? tr("Place the holes: points, dimensions and rules. Then Done.")
                             : m_followMode.sketching != 0 ? tr("Draw it, then click Done.")
                                                           : tr("Sketching: draw a closed outline, then click Finish."));
    // The view only knows its size once shown.
    QTimer::singleShot(0, this, [this] {
        if (m_sketchMode.active)
            m_sketchView->fit();
    });
    updateSketchStatus();
}

void MainWindow::updateSketchStatus()
{
    if (!m_sketchMode.active)
        return;
    const sketch::Sketch& s = m_sketchView->sketch();
    const sketch::Solved& solved = m_sketchView->solved();
    const auto loose = sketch::looseEnds(s);
    QString text;
    bool problem = false, held = false;
    const std::size_t spots = sketch::loosePoints(s).size();
    if (!solved.ok) {
        text = tr("Some rules can't all be met: they're shown in red. Delete or change one.");
        problem = true;
    } else if (m_holeMode.positioning) {
        text = (spots == 1 ? tr("1 hole.") : tr("%1 holes.").arg(spots)) + QLatin1Char(' ') +
               (solved.freedom == 0 ? tr("Every place is set.")
                                    : tr("Add dimensions (Across, Up) to set their places exactly."));
        held = solved.freedom == 0 && spots > 0;
    } else if (sketch::isEmpty(s)) {
        text = tr("Nothing drawn yet.");
    } else if (solved.freedom == 0) {
        text = tr("Fully set: every size and place is fixed.");
        held = true;
    } else {
        text = solved.freedom == 1 ? tr("It can still move or change size 1 way. (That's fine: add dimensions "
                                        "and rules to pin it down.)")
                                   : tr("It can still move or change size %1 ways. (That's fine: add dimensions "
                                        "and rules to pin it down.)")
                                         .arg(solved.freedom);
    }
    if (!loose.empty() && m_followMode.sketching != 1 && !m_sketchMode.pathStage)  // a path may well have ends
        text += QStringLiteral("\n") + (loose.size() == 1 ? tr("1 loose end (red): the outline isn't closed there.")
                                                          : tr("%1 loose ends (red): the outline isn't closed there.")
                                                                .arg(loose.size()));
    m_sketchPanel->setStatus(text, problem, held);
}

void MainWindow::finishSketch()
{
    if (!m_sketchMode.active)
        return;
    const sketch::Solved& solved = m_sketchView->solved();
    if (!solved.ok) {
        m_sketchPanel->setMessage(tr("Some rules can't all be met (shown in red). Fix them first."), true);
        return;
    }
    if (m_holeMode.positioning) {
        // Back to the Hole tool with the spots placed.
        m_holeMode.spots = m_sketchView->sketch();
        endSketch();
        return;
    }
    if (m_followMode.sketching != 0) {
        // Back to the Follow tool with the path (or cross-section) drawn.
        (m_followMode.sketching == 1 ? m_followMode.finish.path : m_followMode.finish.custom) = m_sketchView->sketch();
        endSketch();
        return;
    }
    using Make = SketchPanel::Make;
    const Make make = m_sketchPanel->make();
    const bool cut = m_sketchPanel->pocket();
    PrimitiveSpec spec;
    if (make == Make::Loft) {
        storeSketchStage();
        LoftSpec loft = m_sketchMode.loft;
        loft.ruled = m_sketchPanel->ruled();
        if (loft.sections.size() < 2) {
            m_sketchPanel->setMessage(tr("A loft blends between outlines: click \"Add one above\" and draw the "
                                         "next one first."),
                                      true);
            return;
        }
        spec = loft;
    } else if (make == Make::Sweep) {
        storeSketchStage();
        SweepSpec sweep = m_sketchMode.sweep;
        sweep.spiral = m_sketchPanel->spiral();
        sweep.pitch = m_sketchPanel->pitch();
        sweep.turns = m_sketchPanel->turns();
        sweep.leftHand = m_sketchPanel->leftHand();
        if (!sweep.spiral && sweep.path.geos.empty()) {
            m_sketchPanel->setMessage(tr("Draw the path it goes along first: click \"Draw the path\"."), true);
            return;
        }
        spec = sweep;
    } else if (m_sketchPanel->revolve()) {
        RevolveSpec revolve;
        revolve.sketch = m_sketchView->sketch();
        // The axis: the picked line, else the only helper line.
        std::vector<int> lines;
        for (int g : m_sketchView->pickedGeos())
            if (revolve.sketch.geos[std::size_t(g)].kind == sketch::GeoKind::Line)
                lines.push_back(g);
        if (lines.empty())
            for (std::size_t g = 0; g < revolve.sketch.geos.size(); ++g)
                if (revolve.sketch.geos[g].kind == sketch::GeoKind::Line && revolve.sketch.geos[g].construction)
                    lines.push_back(int(g));
        if (lines.size() != 1) {
            m_sketchPanel->setMessage(lines.empty() ? tr("Draw a line to spin round, and press G to make it a helper "
                                                         "line (or pick it).")
                                                    : tr("There are several helper lines: pick the one to spin "
                                                         "round."),
                                      true);
            return;
        }
        revolve.axis = lines.front();
        revolve.sketch.geos[std::size_t(revolve.axis)].construction = true;
        revolve.angle = m_sketchPanel->angle();
        revolve.reversed = m_sketchPanel->otherWay();
        revolve.midplane = m_sketchPanel->bothWays();
        spec = revolve;
    } else {
        PadSpec pad;
        pad.sketch = m_sketchView->sketch();
        pad.length = m_sketchPanel->length();
        pad.reversed = cut ? !m_sketchPanel->otherWay() : m_sketchPanel->otherWay();
        pad.midplane = m_sketchPanel->bothWays();
        pad.taper = m_sketchPanel->taper();
        spec = pad;
    }
    TopoDS_Shape solid;
    try {
        solid = primitives::buildShape(spec);
    } catch (const std::exception& e) {
        m_sketchPanel->setMessage(QString::fromUtf8(e.what()), true);
        return;
    }
    const bool throughAll = m_sketchPanel->throughAll();
    const SketchMode mode = m_sketchMode;
    endSketch();

    if (mode.pad != kInvalidId) {  // changing one
        const Body* now = m_doc.find(mode.pad);
        if (!now)
            return;
        Body after = *now;
        after.spec = spec;
        if (typeName(now->spec) != typeName(spec) && now->name.rfind(typeName(now->spec), 0) == 0)
            after.name = m_doc.uniqueName(typeName(spec));  // a Pad turned Revolve is named so
        const Body* step = m_doc.find(mode.step);
        if (step && step->boolean->throughAll != throughAll && step->boolean->op == BooleanOp::Subtract) {
            Body stepAfter = *step;
            stepAfter.boolean->throughAll = throughAll;
            m_stack.beginMacro("Change sketch");
            try {
                m_stack.execute(std::make_unique<EditBodyCommand>(*now, after, "Change sketch"));
                m_stack.execute(std::make_unique<EditBodyCommand>(*m_doc.find(mode.step), stepAfter, "Through all"));
            } catch (const std::exception& e) {
                qWarning() << "Changing the sketch failed:" << e.what();
            }
            m_stack.endMacro();
        } else {
            run(std::make_unique<EditBodyCommand>(*now, after, "Change sketch"));
        }
        setSelection(mode.step != kInvalidId ? history::objectOf(m_doc, mode.step) : mode.pad);
        return;
    }

    static const char* toolNames[] = {"Pad", "Pocket", "Revolve", "Groove", "Loft", "Sweep"};
    static const char* resultNames[] = {"Padded", "Pocketed", "Revolved", "Grooved", "Lofted", "Swept"};
    Body body;
    body.id = m_doc.allocateId();
    body.spec = spec;
    body.name = m_doc.uniqueName(toolNames[int(make)]);
    body.placement = placementFromTransform(mode.frame, pivotOf(body.spec));
    const ObjectId toolId = body.id;
    if (mode.target == kInvalidId) {  // on the plate
        const auto* sweep = std::get_if<SweepSpec>(&spec);
        if (mode.plane < 0 && (std::holds_alternative<RevolveSpec>(spec) || (sweep && sweep->spiral))) {
            // Spun round: the sketch is the side view, standing up (its Y is the plate's Z),
            // and the shape sits on the plate.
            Transform upright;
            upright.m[1][1] = 0.0;
            upright.m[1][2] = -1.0;
            upright.m[2][1] = 1.0;
            upright.m[2][2] = 0.0;
            const Bounds box = shapeBounds(solid, upright);
            if (box.valid)
                upright.m[2][3] = -box.min[2];
            body.placement = placementFromTransform(upright, pivotOf(body.spec));
        }
        if (run(std::make_unique<AddBodyCommand>(std::move(body))))
            setSelection(toolId);
        return;
    }
    // On a face: added to (or cut from) the shape it was drawn on.
    if (!m_doc.find(mode.target))
        return;
    addWithStep(mode.target, std::move(body), cut ? BooleanOp::Subtract : BooleanOp::Merge, throughAll,
                toolNames[int(make)], resultNames[int(make)], shapeBounds(solid, mode.frame));
}

void MainWindow::storeSketchStage()
{
    auto& mode = m_sketchMode;
    if (!mode.active)
        return;
    const auto make = m_sketchPanel->make();
    if (make == SketchPanel::Make::Loft) {
        if (mode.loft.sections.size() <= std::size_t(mode.outline))
            mode.loft.sections.resize(std::size_t(mode.outline) + 1);
        LoftSection& section = mode.loft.sections[std::size_t(mode.outline)];
        section.sketch = m_sketchView->sketch();
        section.height = mode.outline == 0 ? 0.0 : m_sketchPanel->loftHeight();
    } else if (make == SketchPanel::Make::Sweep) {
        (mode.pathStage ? mode.sweep.path : mode.sweep.profile) = m_sketchView->sketch();
    }
}

namespace {
std::vector<std::vector<QPointF>> asReference(const sketch::Sketch& s)
{
    std::vector<std::vector<QPointF>> out;
    for (const auto& line : sketch::outlinesOf(s)) {
        std::vector<QPointF> q;
        for (const auto& p : line)
            q.emplace_back(p.x, p.y);
        out.push_back(std::move(q));
    }
    return out;
}
}  // namespace

void MainWindow::loftStep(int step)
{
    auto& mode = m_sketchMode;
    if (!mode.active)
        return;
    storeSketchStage();
    auto& sections = mode.loft.sections;
    if (step == 0) {
        // Remove this one (never the first, on the sketch plane): show the one below.
        if (mode.outline <= 0 || mode.outline >= int(sections.size()))
            return;
        sections.erase(sections.begin() + mode.outline);
        step = -1;
    }
    const int next = mode.outline + step;
    if (next < 0)
        return;
    if (next >= int(sections.size())) {
        // A new outline above: a copy of the top one, as far above it again.
        LoftSection above = sections.back();
        const double gap = sections.size() >= 2 ? sections.back().height - sections[sections.size() - 2].height : 20.0;
        above.height = sections.back().height + std::max(gap, 1.0);
        sections.push_back(above);
    }
    mode.outline = next;
    // The outlines either side in grey (and the face, for the first).
    std::vector<std::vector<QPointF>> reference = next == 0 ? mode.reference : std::vector<std::vector<QPointF>>{};
    for (int k : {next - 1, next + 1})
        if (k >= 0 && k < int(sections.size()))
            for (auto& line : asReference(sections[std::size_t(k)].sketch))
                reference.push_back(std::move(line));
    m_sketchView->start(sections[std::size_t(next)].sketch, std::move(reference));
    m_sketchPanel->setLoftOutline(next, int(sections.size()), sections[std::size_t(next)].height);
    m_sketchPanel->setMessage({}, false);
    updateSketchStatus();
}

void MainWindow::sweepStage(bool path)
{
    auto& mode = m_sketchMode;
    if (!mode.active || path == mode.pathStage)
        return;
    storeSketchStage();
    mode.pathStage = path;
    std::vector<std::vector<QPointF>> reference;
    if (path) {
        // The outline seen edge-on: a grey line across, where the path starts.
        double lo = 0.0, hi = 0.0;
        for (const auto& line : sketch::outlinesOf(mode.sweep.profile))
            for (const auto& p : line) {
                lo = std::min(lo, p.x);
                hi = std::max(hi, p.x);
            }
        reference.push_back({QPointF(lo, 0.0), QPointF(hi, 0.0)});
        // And a guide straight up from it, the way a path usually starts (and room to draw).
        reference.push_back({QPointF(0.0, 0.0), QPointF(0.0, std::max(20.0, 5.0 * (hi - lo)))});
    } else {
        reference = mode.reference;
    }
    m_sketchView->start(path ? mode.sweep.path : mode.sweep.profile, std::move(reference));
    if (path && mode.sweep.path.geos.empty())
        m_sketchView->setTool(SketchView::Tool::Line);
    m_sketchPanel->setSweepStage(path);
    m_sketchPanel->setMessage({}, false);
    updateSketchStatus();
}

void MainWindow::endSketch()
{
    const bool wasOpen = m_sketchMode.active;
    m_sketchMode = SketchMode{};
    if (!wasOpen)
        return;
    lockForSketch(false);
    m_sketchBar->hide();
    m_sketchPanel->hide();
    m_central->setCurrentWidget(m_viewport);
    statusBar()->clearMessage();
    if (m_followMode.sketching != 0) {
        // Back to the Follow tool (its panel still open).
        m_followMode.sketching = 0;
        m_followPanel->show();
        updateFollowPreview();
        refreshCommandState();
        return;
    }
    if (m_holeMode.positioning) {
        // Back to the Hole tool (its panel still open).
        m_holeMode.positioning = false;
        m_holePanel->show();
        showHoleSpots();
        updateHolePreview();
        refreshCommandState();
        return;
    }
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    refreshScene();
    refreshCommandState();
}

void MainWindow::lockForSketch(bool on)
{
    if (on) {
        m_lockedActions.clear();
        for (QAction* a : findChildren<QAction*>()) {
            QObject* owner = a->parent();
            if (owner == m_sketchBar || !(owner == this || qobject_cast<QMenu*>(owner) || qobject_cast<QActionGroup*>(owner)))
                continue;
            if (qobject_cast<QActionGroup*>(owner) && owner->parent() == m_sketchBar)
                continue;
            if (a->isEnabled())
                m_lockedActions.push_back(a);
            a->setEnabled(false);
        }
        for (QAction* a : m_sketchBar->actions())
            a->setEnabled(true);
        m_objects->setEnabled(false);
    } else {
        for (QAction* a : m_sketchBar->actions())
            a->setEnabled(false);
        for (QAction* a : m_lockedActions)
            a->setEnabled(true);
        m_lockedActions.clear();
        m_objects->setEnabled(true);
    }
}

void MainWindow::relockForSketch()
{
    if (!m_sketchMode.active)
        return;
    for (QAction* a : findChildren<QAction*>()) {
        QObject* owner = a->parent();
        if (owner == m_sketchBar || (qobject_cast<QActionGroup*>(owner) && owner->parent() == m_sketchBar))
            continue;
        if (owner == this || qobject_cast<QMenu*>(owner) || qobject_cast<QActionGroup*>(owner)) {
            if (a->isEnabled() && std::find(m_lockedActions.begin(), m_lockedActions.end(), a) == m_lockedActions.end())
                m_lockedActions.push_back(a);
            a->setEnabled(false);
        }
    }
}

// ---- Holes ------------------------------------------------------------------------


void MainWindow::holeClicked()
{
    if (toolOpen())
        return;
    const ObjectId id = selectedOne();
    if (id == kInvalidId || m_selection.size() != 1 || !m_doc.isTopLevel(id)) {
        statusBar()->showMessage(tr("Hole: select one shape on the plate first."), 6000);
        return;
    }
    if (rolledBackSelected()) {
        statusBar()->showMessage(tr("'%1' is rolled back: roll it forward to the end before drilling it.")
                                     .arg(QString::fromStdString(m_doc.find(id)->name)),
                                 8000);
        return;
    }
    if (m_doc.usesMeshEngine(id)) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("Holes go into solid shapes made in Forge3D, not imported meshes or shapes made "
                                    "with them. (Subtract a cylinder from a mesh instead.)"));
        return;
    }
    startHole(id, false);
}

void MainWindow::startHole(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    HoleMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    HoleSpec hole = sketch::sizedAs(HoleSpec{}, "M3", false);
    bool throughAll = false;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (editing) {
            if (!body->isBoolean() || body->boolean->operands.size() != 2)
                return;
            const ObjectId under = body->boolean->operands[0];
            mode.tool = body->boolean->operands[1];
            const Body* tool = m_doc.find(mode.tool);
            if (!tool || !std::holds_alternative<HoleSpec>(tool->spec))
                return;
            hole = std::get<HoleSpec>(tool->spec);
            throughAll = body->boolean->throughAll;
            mode.shape = inPlate(m_doc, under);
            mode.frame = m_doc.worldTransform(mode.tool);
            mode.placed = true;
            mode.spots = hole.sketch;
        } else {
            mode.shape = inPlate(m_doc, target);
            // Deep enough for most things this size, not past half its height.
            const auto size = m_doc.worldBounds(target).size();
            hole.depth = std::clamp(std::round(double(*std::min_element(size.begin(), size.end())) * 0.6), 1.0, 20.0);
        }
        mode.finder = std::make_shared<edges::FaceFinder>(mode.shape);
        const auto size = m_doc.worldBounds(history::objectOf(m_doc, target)).size();
        mode.size = std::sqrt(double(size[0]) * size[0] + double(size[1]) * size[1] + double(size[2]) * size[2]);
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_holeMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_holePanel->start(editing ? tr("Change the holes in '%1'").arg(name) : tr("Holes in '%1'").arg(name), hole,
                       throughAll, editing);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_holePanel->show();
    statusBar()->showMessage(editing ? tr("Hole: click the face to add holes; change the sizes; then Apply.")
                                     : tr("Hole: click the flat face where the first hole goes. Esc cancels."));
    showHoleSpots();
    updateHolePreview();
    refreshCommandState();
}

HoleSpec MainWindow::currentHole() const
{
    HoleSpec h = m_holePanel->settings();
    h.sketch = m_holeMode.spots;
    return h;
}

void MainWindow::showHoleSpots()
{
    std::vector<sketch::Point> spots;
    for (int p : sketch::loosePoints(m_holeMode.spots))
        spots.push_back(m_holeMode.spots.points[std::size_t(p)]);
    m_holePanel->setSpots(spots, m_holeMode.placed);
}

void MainWindow::holeClick(const QVector3D& origin, const QVector3D& direction)
{
    if (m_holeMode.positioning)
        return;
    const QVector3D dir = direction.normalized();
    const auto hit = m_holeMode.finder->under({origin.x(), origin.y(), origin.z()}, {dir.x(), dir.y(), dir.z()});
    if (!hit) {
        statusBar()->showMessage(tr("Hole: click on the shape itself."), 4000);
        return;
    }
    if (!m_holeMode.placed) {
        const auto plane = sketch::planeOfFace(m_holeMode.shape, hit->face);
        if (!plane) {
            statusBar()->showMessage(tr("Hole: that face is curved. Click a flat face."), 6000);
            return;
        }
        m_holeMode.frame = plane->frame;
        m_holeMode.placed = true;
    }
    const Vec3 local = m_holeMode.frame.inverse().apply(hit->point);
    if (std::abs(local[2]) > std::max(m_holeMode.size * 1e-4, 1e-3)) {
        statusBar()->showMessage(tr("Hole: click on the same flat face as the other holes (or Cancel to start "
                                    "again on another)."),
                                 6000);
        return;
    }
    const auto tenth = [](double v) { return std::round(v * 10.0) / 10.0; };
    sketch::addPoint(m_holeMode.spots, {tenth(local[0]), tenth(local[1])});
    statusBar()->showMessage(tr("Hole: click to add more holes on this face, or type their places. Then Apply."));
    showHoleSpots();
    m_holeTimer->start(0);
}

void MainWindow::updateHolePreview()
{
    if (!m_holeMode.active || m_holeMode.positioning)
        return;
    m_holeTimer->stop();
    m_holeMode.preview.reset();
    bool ok = false;
    if (sketch::loosePoints(m_holeMode.spots).empty()) {
        m_holePanel->setStatus(m_holeMode.placed ? tr("Click on the face to add a hole.")
                                                 : tr("Click the flat face where the first hole goes."),
                               false);
    } else {
        try {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            HoleSpec h = currentHole();
            if (m_holePanel->throughAll())
                h.depth = std::max(h.depth, m_holeMode.size * 2.0);
            const TopoDS_Shape cutter =
                BRepBuilderAPI_Transform(sketch::holeShape(h), toGp(m_holeMode.frame), true).Shape();
            BRepAlgoAPI_Cut cut(m_holeMode.shape, cutter);
            if (!cut.IsDone() || cut.HasErrors())
                throw GeometryError("The holes could not be cut here");
            m_holeMode.preview = std::make_shared<const Mesh>(tessellate(cut.Shape()));
            m_holePanel->setStatus(tr("This is how it will look."), false);
            ok = true;
        } catch (const std::exception& e) {
            m_holePanel->setStatus(QString::fromUtf8(e.what()), true);
        } catch (const Standard_Failure&) {
            m_holePanel->setStatus(tr("The holes could not be cut here."), true);
        }
    }
    m_holePanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyHole()
{
    if (!m_holeMode.active)
        return;
    const HoleSpec hole = currentHole();
    try {
        (void)sketch::holeShape(hole);
    } catch (const std::exception& e) {
        m_holePanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    const bool throughAll = m_holePanel->throughAll();
    const HoleMode mode = m_holeMode;
    endHole();
    if (mode.editing) {
        const Body* tool = m_doc.find(mode.tool);
        const Body* step = m_doc.find(mode.target);
        if (!tool || !step)
            return;
        Body after = *tool;
        after.spec = hole;
        if (step->boolean->throughAll != throughAll) {
            Body stepAfter = *step;
            stepAfter.boolean->throughAll = throughAll;
            m_stack.beginMacro("Change holes");
            try {
                m_stack.execute(std::make_unique<EditBodyCommand>(*tool, after, "Change holes"));
                m_stack.execute(std::make_unique<EditBodyCommand>(*m_doc.find(mode.target), stepAfter, "Through all"));
            } catch (const std::exception& e) {
                qWarning() << "Changing the holes failed:" << e.what();
            }
            m_stack.endMacro();
        } else {
            run(std::make_unique<EditBodyCommand>(*tool, after, "Change holes"));
        }
        setSelection(history::objectOf(m_doc, mode.target));
        return;
    }
    Body tool;
    tool.id = m_doc.allocateId();
    tool.spec = hole;
    tool.name = m_doc.uniqueName("Hole");
    tool.placement = placementFromTransform(mode.frame, pivotOf(tool.spec));
    addWithStep(mode.target, std::move(tool), BooleanOp::Subtract, throughAll, "Hole", "Drilled", Bounds{});
}

void MainWindow::endHole()
{
    if (!m_holeMode.active)
        return;
    if (m_holeMode.positioning)
        endSketch();
    m_holeTimer->stop();
    m_holeMode = HoleMode{};
    m_holePanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Shell ------------------------------------------------------------------------

void MainWindow::startShell(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    ShellMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    ShellFinish finish;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        mode.shape = editing ? pickableShape(m_doc, target) : inPlate(m_doc, target);
        mode.found = edges::analyse(mode.shape);
        mode.finder = std::make_shared<edges::FaceFinder>(mode.shape);
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Shell)
                return;
            finish = shellInPlate(m_doc, target);
            const std::vector<int> found = pickedFaces(m_doc, target);
            for (std::size_t i = 0; i < found.size(); ++i)
                if (found[i] >= 0)
                    mode.picks.push_back({found[i], shell::refFor(mode.shape, found[i], finish.open[i].point)});
        } else {
            auto sides = m_doc.worldBounds(target).size();
            std::sort(sides.begin(), sides.end());
            finish.thickness = std::clamp(std::round(double(sides[0]) * 0.1 * 2.0) / 2.0, 0.5, 3.0);
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_shellMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_shellPanel->start(editing ? tr("Change the shell of '%1'").arg(name) : tr("Shell '%1'").arg(name), editing,
                        finish.thickness, finish.outward);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_shellPanel->show();
    statusBar()->showMessage(tr("Shell: click the faces to leave open, then click Apply. Esc cancels."));
    rebuildShellHighlights();
    updateShellPreview();
    refreshCommandState();
}

void MainWindow::shellClick(const QVector3D& origin, const QVector3D& direction)
{
    const QVector3D dir = direction.normalized();
    const auto hit = m_shellMode.finder->under({origin.x(), origin.y(), origin.z()}, {dir.x(), dir.y(), dir.z()});
    if (!hit)
        return;
    auto& picks = m_shellMode.picks;
    const auto it = std::find_if(picks.begin(), picks.end(), [&](const auto& p) { return p.face == hit->face; });
    if (it != picks.end())
        picks.erase(it);  // clicked again: closed again
    else
        picks.push_back({hit->face, shell::refFor(m_shellMode.shape, hit->face, hit->point)});
    rebuildShellHighlights();
    m_shellTimer->start(0);
}

void MainWindow::rebuildShellHighlights()
{
    const float radius = std::max(0.02f, m_viewport->camera().distance() * 0.0022f);
    std::vector<const std::vector<Vec3>*> lines;
    for (const auto& pick : m_shellMode.picks)
        if (std::size_t(pick.face) < m_shellMode.found.faceEdges.size())
            for (int e : m_shellMode.found.faceEdges[std::size_t(pick.face)])
                lines.push_back(&m_shellMode.found.edges[std::size_t(e)].polyline);
    m_shellMode.pickedLines = lines.empty() ? nullptr : tubes(lines, radius);
    m_shellPanel->setOpenCount(int(m_shellMode.picks.size()));
}

std::optional<BooleanSpec> MainWindow::currentShellSpec() const
{
    if (!m_shellMode.active)
        return std::nullopt;
    ShellFinish f;
    f.thickness = m_shellPanel->thickness();
    f.outward = m_shellPanel->outward();
    for (const auto& pick : m_shellMode.picks)
        f.open.push_back(pick.ref);
    if (m_shellMode.editing)
        return withShell(m_doc, m_shellMode.target, f).boolean;
    return shellSpec(m_doc, m_shellMode.target, f);
}

void MainWindow::updateShellPreview()
{
    if (!m_shellMode.active)
        return;
    m_shellTimer->stop();
    m_shellMode.preview.reset();
    bool ok = false;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (const auto spec = currentShellSpec()) {
            m_shellMode.preview = m_doc.previewMesh(*spec);
            m_shellPanel->setStatus(tr("This is how it will look."), false);
            ok = true;
        }
    } catch (const std::exception& e) {
        m_shellPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    m_shellPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyShell()
{
    if (!m_shellMode.active)
        return;
    std::optional<BooleanSpec> spec;
    try {
        spec = currentShellSpec();
        if (!spec)
            return;
        m_doc.previewMesh(*spec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("The shape could not be hollowed out:\n%1")
                                                      .arg(QString::fromUtf8(e.what())));
        return;
    }
    const ObjectId target = m_shellMode.target;
    if (m_shellMode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endShell();
        const Body before = *now;
        Body after = before;
        after.boolean = *spec;
        endShell();
        run(std::make_unique<EditBodyCommand>(before, after, "Change shell"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Shelled");
    result.boolean = *spec;
    const ObjectId id = result.id;
    endShell();
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::endShell()
{
    if (!m_shellMode.active)
        return;
    m_shellTimer->stop();
    m_shellMode = ShellMode{};
    m_shellPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Thread -----------------------------------------------------------------------

void MainWindow::startThread(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    ThreadMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    ThreadFinish finish;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        mode.shape = editing ? pickableShape(m_doc, target) : inPlate(m_doc, target);
        mode.found = edges::analyse(mode.shape);
        mode.finder = std::make_shared<edges::FaceFinder>(mode.shape);
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Thread)
                return;
            finish = threadInPlate(m_doc, target);
            const std::vector<int> found = shell::match(mode.shape, {finish.face});
            if (!found.empty() && found.front() >= 0) {
                mode.face = found.front();
                mode.ref = shell::refFor(mode.shape, mode.face, finish.face.point);
            }
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_threadMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_threadPanel->start(editing ? tr("Change the thread of '%1'").arg(name) : tr("Thread '%1'").arg(name), editing);
    m_threadPanel->setValues(finish.size, finish.pitch, finish.fit, finish.leftHand);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_threadPanel->show();
    statusBar()->showMessage(tr("Thread: click the round face of a peg or a hole, then Apply. Esc cancels."));
    threadPicked(false);
    updateThreadPreview();
    refreshCommandState();
}

void MainWindow::threadClick(const QVector3D& origin, const QVector3D& direction)
{
    const QVector3D dir = direction.normalized();
    const auto hit = m_threadMode.finder->under({origin.x(), origin.y(), origin.z()}, {dir.x(), dir.y(), dir.z()});
    if (!hit)
        return;
    if (!thread::roundFace(m_threadMode.shape, hit->face)) {
        m_threadPanel->setStatus(tr("That face isn't round. Click the round side of a peg, or the inside of a "
                                    "hole."),
                                 true);
        return;
    }
    const bool changed = hit->face != m_threadMode.face;
    m_threadMode.face = hit->face;
    m_threadMode.ref = shell::refFor(m_threadMode.shape, hit->face, hit->point);
    threadPicked(changed);
    m_threadTimer->start(0);
}

void MainWindow::threadPicked(bool suggest)
{
    auto& mode = m_threadMode;
    const float radius = std::max(0.02f, m_viewport->camera().distance() * 0.0022f);
    std::vector<const std::vector<Vec3>*> lines;
    if (mode.face >= 0 && std::size_t(mode.face) < mode.found.faceEdges.size())
        for (int e : mode.found.faceEdges[std::size_t(mode.face)])
            lines.push_back(&mode.found.edges[std::size_t(e)].polyline);
    mode.pickedLines = lines.empty() ? nullptr : tubes(lines, radius);
    const auto round = mode.face >= 0 ? thread::roundFace(mode.shape, mode.face) : std::nullopt;
    if (!round) {
        m_threadPanel->setPicked({});
        return;
    }
    m_threadPanel->setPicked(
        (round->hole ? tr("<b>A hole</b>, Ø %1 mm, %2 mm deep: an inside thread (a nut).")
                     : tr("<b>A peg</b>, Ø %1 mm, %2 mm long: an outside thread (a bolt)."))
            .arg(QString::number(2 * round->radius, 'f', 2), QString::number(round->to - round->from, 'f', 2)));
    if (suggest) {
        const thread::Size s = thread::suggest(*round);
        m_threadPanel->setValues(s.diameter, s.pitch, m_threadPanel->fit(), m_threadPanel->leftHand());
    }
}

std::optional<BooleanSpec> MainWindow::currentThreadSpec() const
{
    if (!m_threadMode.active || m_threadMode.face < 0)
        return std::nullopt;
    ThreadFinish f;
    f.face = m_threadMode.ref;
    f.size = m_threadPanel->size();
    f.pitch = m_threadPanel->pitch();
    f.fit = m_threadPanel->fit();
    f.leftHand = m_threadPanel->leftHand();
    if (m_threadMode.editing)
        return withThread(m_doc, m_threadMode.target, f).boolean;
    return threadSpec(m_doc, m_threadMode.target, f);
}

void MainWindow::updateThreadPreview()
{
    if (!m_threadMode.active)
        return;
    m_threadTimer->stop();
    m_threadMode.preview.reset();
    bool ok = false;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (const auto spec = currentThreadSpec()) {
            m_threadMode.preview = m_doc.previewMesh(*spec);
            m_threadPanel->setStatus(tr("This is how it will look."), false);
            ok = true;
        } else {
            m_threadPanel->setStatus({}, false);
        }
    } catch (const std::exception& e) {
        m_threadPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    m_threadPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyThread()
{
    if (!m_threadMode.active)
        return;
    std::optional<BooleanSpec> spec;
    try {
        spec = currentThreadSpec();
        if (!spec)
            return;
        m_doc.previewMesh(*spec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("The thread could not be cut:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    const ObjectId target = m_threadMode.target;
    if (m_threadMode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endThread();
        const Body before = *now;
        Body after = before;
        after.boolean = *spec;
        endThread();
        run(std::make_unique<EditBodyCommand>(before, after, "Change thread"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Threaded");
    result.boolean = *spec;
    const ObjectId id = result.id;
    endThread();
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::endThread()
{
    if (!m_threadMode.active)
        return;
    m_threadTimer->stop();
    m_threadMode = ThreadMode{};
    m_threadPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Quick Socket -----------------------------------------------------------------

namespace {

// The settings used last (kept between runs), so a figure's joints come out alike.
SocketFinish rememberedSocket()
{
    const Preferences p;
    SocketFinish f;
    const QString kind = p.value(QStringLiteral("socket/kind")).toString();
    f.kind = kind == QStringLiteral("peg")      ? SocketKind::Peg
             : kind == QStringLiteral("magnet") ? SocketKind::Magnet
                                                : SocketKind::Ball;
    f.size = p.value(QStringLiteral("socket/size"), f.size).toDouble();
    f.depth = p.value(QStringLiteral("socket/depth"), f.depth).toDouble();
    f.resin = p.value(QStringLiteral("socket/printer"), QStringLiteral("resin")).toString() != QStringLiteral("fdm");
    f.fit = p.value(QStringLiteral("socket/fit"), f.resin ? 0.1 : 0.2).toDouble();
    f.snap = p.value(QStringLiteral("socket/snap"), true).toBool();
    f.grip = p.value(QStringLiteral("socket/grip"), f.grip).toDouble();
    f.shape = PegShape(std::clamp(p.value(QStringLiteral("socket/shape"), 0).toInt(), 0, 3));
    f.reliefCuts = p.value(QStringLiteral("socket/reliefCuts"), 0).toInt();
    f.ridges = std::clamp(p.value(QStringLiteral("socket/ridges"), 0).toInt(), 0, 24);
    f.ridgeHeight = p.value(QStringLiteral("socket/ridgeHeight"), f.ridgeHeight).toDouble();
    f.reliefWidth = p.value(QStringLiteral("socket/reliefWidth"), f.reliefWidth).toDouble();
    f.cover = f.kind == SocketKind::Magnet ? p.value(QStringLiteral("socket/cover"), 0.0).toDouble() : 0.0;
    try {
        socket::check(f);
    } catch (const std::exception&) {
        return SocketFinish{};
    }
    return f;
}

void rememberSocket(const SocketFinish& f)
{
    Preferences p;
    p.setValue(QStringLiteral("socket/kind"), f.kind == SocketKind::Peg      ? QStringLiteral("peg")
                                              : f.kind == SocketKind::Magnet ? QStringLiteral("magnet")
                                                                             : QStringLiteral("ball"));
    p.setValue(QStringLiteral("socket/cover"), f.cover);
    p.setValue(QStringLiteral("socket/size"), f.size);
    p.setValue(QStringLiteral("socket/depth"), f.depth);
    p.setValue(QStringLiteral("socket/printer"), f.resin ? QStringLiteral("resin") : QStringLiteral("fdm"));
    p.setValue(QStringLiteral("socket/fit"), f.fit);
    p.setValue(QStringLiteral("socket/snap"), f.snap);
    p.setValue(QStringLiteral("socket/grip"), f.grip);
    p.setValue(QStringLiteral("socket/shape"), int(f.shape));
    p.setValue(QStringLiteral("socket/reliefCuts"), f.reliefCuts);
    p.setValue(QStringLiteral("socket/ridges"), f.ridges);
    p.setValue(QStringLiteral("socket/ridgeHeight"), f.ridgeHeight);
    p.setValue(QStringLiteral("socket/reliefWidth"), f.reliefWidth);
}

// The cut on its own, to show on a shape too big to preview quickly.
std::shared_ptr<const Mesh> cutShown(const SocketFinish& f, const emboss::SurfacePoint& spot)
{
    auto out = std::make_shared<Mesh>();
    for (const Mesh& piece : socket::cutters(f, spot.point, spot.normal)) {
        const auto first = std::uint32_t(out->positions.size() / 3);
        out->positions.insert(out->positions.end(), piece.positions.begin(), piece.positions.end());
        for (std::uint32_t i : piece.indices)
            out->indices.push_back(first + i);
    }
    // Facings for the shading: each point, the sum of its triangles'.
    out->normals.assign(out->positions.size(), 0.0f);
    for (std::size_t t = 0; t + 2 < out->indices.size(); t += 3) {
        const float* a = &out->positions[3 * out->indices[t]];
        const float* b = &out->positions[3 * out->indices[t + 1]];
        const float* c = &out->positions[3 * out->indices[t + 2]];
        const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        for (int k = 0; k < 3; ++k)
            for (int j = 0; j < 3; ++j)
                out->normals[3 * out->indices[t + std::size_t(k)] + std::size_t(j)] += n[j];
    }
    for (std::size_t i = 0; i + 2 < out->normals.size(); i += 3) {
        const float l = std::sqrt(out->normals[i] * out->normals[i] + out->normals[i + 1] * out->normals[i + 1] +
                                  out->normals[i + 2] * out->normals[i + 2]);
        if (l > 0)
            for (int j = 0; j < 3; ++j)
                out->normals[i + std::size_t(j)] /= l;
    }
    return out;
}

// Shapes with more triangles than this show just the cut while the socket is set up
// (working the whole result out again on every change would be slow).
constexpr std::size_t kSocketPreviewTriangles = 300'000;

}  // namespace

void MainWindow::startSocket(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    SocketMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    SocketFinish finish = rememberedSocket();
    if (editing) {
        if (!body->isBoolean() || body->boolean->op != BooleanOp::Socket || !body->boolean->socket)
            return;
        finish = *body->boolean->socket;
        try {
            mode.spot = socketSpot(m_doc, target);
        } catch (const std::exception& e) {
            QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
            return;
        }
    }
    m_socketMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    refreshStandardRows();
    m_socketPanel->start(editing ? tr("Change the socket in '%1'").arg(name) : tr("Socket in '%1'").arg(name),
                         editing);
    m_socketPanel->setFinish(finish);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_socketPanel->show();
    statusBar()->showMessage(editing ? tr("Quick Socket: change the settings, or click elsewhere on the shape to "
                                          "move it, then Apply. Esc cancels.")
                                     : tr("Quick Socket: click the spot on the shape where the socket goes. Esc "
                                          "cancels."));
    updateSocketPreview();
    refreshCommandState();
}

void MainWindow::socketClick(const QVector3D& origin, const QVector3D& direction)
{
    const QVector3D d = direction.normalized();
    if (m_socketMode.pickingOther) {
        Ray ray;
        ray.origin = {origin.x(), origin.y(), origin.z()};
        ray.direction = {d.x(), d.y(), d.z()};
        const auto hit = pickShown(ray);
        const ObjectId self = history::objectOf(m_doc, m_socketMode.target);
        if (!hit || history::objectOf(m_doc, hit->id) == self) {
            statusBar()->showMessage(tr("Click the other part (the one against this face)."), 5000);
            return;
        }
        m_socketMode.other = history::objectOf(m_doc, hit->id);
        m_socketMode.pickingOther = false;
        m_socketPanel->setOtherPart(QString::fromStdString(m_doc.find(m_socketMode.other)->name), false);
        m_socketTimer->start(0);
        return;
    }
    std::optional<emboss::SurfacePoint> spot;
    try {
        // The facing averaged over about the socket's size, so a bumpy surface gives a
        // steady direction.
        const double radius = std::max(0.25, m_socketPanel->finish().size / 2.0);
        spot = surfaceUnder(m_doc, m_socketMode.target, {origin.x(), origin.y(), origin.z()}, {d.x(), d.y(), d.z()},
                            radius);
    } catch (const std::exception& e) {
        m_socketPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    if (!spot) {
        statusBar()->showMessage(tr("Quick Socket: click on the shape itself."), 4000);
        return;
    }
    m_socketMode.spot = spot;
    m_socketPanel->setPlaced(true);
    m_socketTimer->start(0);
}

std::optional<BooleanSpec> MainWindow::currentSocketSpec() const
{
    if (!m_socketMode.active || !m_socketMode.spot)
        return std::nullopt;
    const SocketFinish f = m_socketPanel->finish();
    if (m_socketMode.editing)
        return withSocket(m_doc, m_socketMode.target, *m_socketMode.spot, f).boolean;
    return socketSpec(m_doc, m_socketMode.target, *m_socketMode.spot, f);
}

void MainWindow::updateSocketPreview()
{
    if (!m_socketMode.active)
        return;
    m_socketTimer->stop();
    m_socketMode.preview.reset();
    m_socketMode.cut.reset();
    bool ok = false;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        socket::check(m_socketPanel->finish());
        if (const auto spec = currentSocketSpec()) {
            const ObjectId part = spec->operands.front();
            const auto shown = m_doc.mesh(part);
            if (shown && shown->triangleCount() > kSocketPreviewTriangles) {
                m_socketMode.cut = cutShown(m_socketPanel->finish(), *m_socketMode.spot);
                m_socketPanel->setStatus(tr("The red shape is what will be cut away (the shape is too big to show "
                                            "the finished socket quickly). Apply cuts it."),
                                         false);
            } else {
                m_socketMode.preview = m_doc.previewMesh(*spec);
                m_socketPanel->setStatus(tr("This is how it will look."), false);
            }
            ok = true;
            if (m_socketPanel->matchingPocket()) {
                // The matching pocket needs the other part, against this face.
                if (m_socketMode.other == kInvalidId || !m_doc.find(m_socketMode.other)) {
                    m_socketPanel->setStatus(tr("Pick the part this magnet holds (or untick the matching pocket)."),
                                             false);
                    ok = false;
                } else if (!facingSpot(m_doc, history::tip(m_doc, m_socketMode.other), *m_socketMode.spot)) {
                    m_socketPanel->setStatus(tr("%1 isn't against this face (within 3 mm), so there's nowhere to "
                                                "line its pocket up. Join it on first.")
                                                 .arg(QString::fromStdString(m_doc.find(m_socketMode.other)->name)),
                                             true);
                    ok = false;
                }
            }
        } else {
            m_socketPanel->setStatus({}, false);
        }
    } catch (const std::exception& e) {
        m_socketPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    m_socketPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applySocket()
{
    if (!m_socketMode.active)
        return;
    std::optional<BooleanSpec> spec;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        spec = currentSocketSpec();
        if (!spec)
            return;
        m_doc.previewMesh(*spec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("The socket could not be cut:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    rememberSocket(*spec->socket);
    const ObjectId target = m_socketMode.target;
    if (m_socketMode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endSocket();
        const Body before = *now;
        Body after = before;
        after.boolean = *spec;
        endSocket();
        run(std::make_unique<EditBodyCommand>(before, after, "Change socket"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Socketed");
    result.boolean = *spec;
    const ObjectId id = result.id;
    // A magnet's matching pocket, in the part against the face.
    std::optional<Body> matching;
    if (m_socketPanel->matchingPocket() && m_socketMode.other != kInvalidId) {
        try {
            const ObjectId otherTip = history::tip(m_doc, m_socketMode.other);
            const auto facing = facingSpot(m_doc, otherTip, *m_socketMode.spot);
            if (!facing)
                throw std::invalid_argument("The other part isn't against this face.");
            Body pocket;
            pocket.id = m_doc.allocateId();
            pocket.name = m_doc.uniqueName("Socketed");
            pocket.boolean = socketSpec(m_doc, otherTip, *facing, *spec->socket);
            m_doc.previewMesh(*pocket.boolean);
            matching = std::move(pocket);
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Forge3D"),
                                 tr("The matching pocket could not be cut:\n%1").arg(QString::fromUtf8(e.what())));
            return;
        }
    }
    endSocket();
    if (matching) {
        try {
            m_stack.beginMacro("Magnet pockets");
            if (!m_doc.isTopLevel(target))
                m_stack.execute(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target)));
            else
                m_stack.execute(std::make_unique<CombineCommand>(std::move(result)));
            matching->name = m_doc.uniqueName("Socketed");  // after the first is named
            m_stack.execute(std::make_unique<CombineCommand>(std::move(*matching)));
            m_stack.endMacro();
        } catch (const std::exception& e) {
            m_stack.abortMacro();
            QMessageBox::warning(this, tr("Forge3D"),
                                 tr("The pockets could not be cut:\n%1").arg(QString::fromUtf8(e.what())));
            refreshScene();
            return;
        }
        refreshScene();
        refreshCommandState();
        setSelection(history::objectOf(m_doc, target));
        statusBar()->showMessage(tr("Both pockets cut. Mind the magnets' poles: one pocket's magnet goes in turned "
                                    "over, so they pull together."),
                                 10000);
        return;
    }
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::addMatchingPart()
{
    const SocketFinish f = m_socketPanel->finish();
    Body body;
    try {
        body.spec = socket::matchingPart(f);
        body.hardware = true;  // keeps its size when the design is rescaled
    } catch (const std::exception& e) {
        m_socketPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    body.id = m_doc.allocateId();
    // "Ball Ø3", or "Ball Ø3 2" if there's one already.
    const std::string base = socket::matchingName(f);
    std::string name = base;
    const auto taken = [this](const std::string& n) {
        return std::any_of(m_doc.bodies().begin(), m_doc.bodies().end(), [&](const Body& b) { return b.name == n; });
    };
    for (int n = 2; taken(name); ++n)
        name = base + " " + std::to_string(n);
    body.name = name;
    body.placement = suggestPlacement(m_doc, f.kind == SocketKind::Ball ? f.size : f.size * 1.5);
    try {
        m_stack.beginMacro("Add " + name);  // "Add Ball Ø3" rather than "Add Sphere"
        m_stack.execute(std::make_unique<AddBodyCommand>(std::move(body)));
        m_stack.endMacro();
    } catch (const std::exception& e) {
        m_stack.abortMacro();
        m_socketPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    refreshScene();
    refreshCommandState();
    statusBar()->showMessage(tr("Added '%1' beside the shapes, at its true size.").arg(QString::fromStdString(name)),
                             6000);
}

void MainWindow::endSocket()
{
    if (!m_socketMode.active)
        return;
    m_socketTimer->stop();
    m_socketMode = SocketMode{};
    m_socketPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

namespace {
std::shared_ptr<const Mesh> together(const std::vector<Mesh>& pieces);
}

// ---- Tolerance test print -----------------------------------------------------------

void MainWindow::addToleranceTest(const tolerance::Settings& settings)
{
    if (toolOpen()) {
        statusBar()->showMessage(tr("Close the open tool first."), 5000);
        return;
    }
    std::vector<Body> bodies;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        for (const tolerance::Piece& piece : tolerance::pieces(settings))
            bodies.push_back(hinge::pieceBody(piece.mesh, piece.name));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("The test could not be made:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    // Beside anything already on the plate.
    const Bounds there = m_doc.worldBounds();
    double shift = 0.0;
    if (there.valid) {
        double lowest = 1e300;
        for (const Body& b : bodies)
            lowest = std::min(lowest, b.placement.x - std::get<MeshSpec>(b.spec).width / 2.0);
        shift = there.max[0] + 10.0 - lowest;
    }
    std::vector<ObjectId> added;
    m_stack.beginMacro("Tolerance Test");
    try {
        for (Body& b : bodies) {
            b.id = m_doc.allocateId();
            b.placement.x += shift;
            b.name = m_doc.uniqueName(b.name);
            added.push_back(b.id);
            m_stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
        }
        m_stack.endMacro();
    } catch (const std::exception& e) {
        m_stack.abortMacro();
        QMessageBox::warning(this, tr("Forge3D"), tr("The test could not be made:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    refreshScene();
    setSelection(added);
    m_viewport->fitView();
    refreshCommandState();
}

// ---- Pose Test ----------------------------------------------------------------------

void MainWindow::startPose()
{
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    m_poseMode = PoseMode{};
    m_poseMode.active = true;
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_posePanel->show();
    m_posePanel->setRange({});
    m_posePanel->setStatus({}, false);
    rebuildPose();
    if (m_poseMode.joints.empty())
        m_posePanel->setStatus(tr("No joints yet: make some with Quick Hinge or Quick Socket (Features tab), then "
                                  "come back."),
                               false);
    refreshSavedPoses();
    statusBar()->showMessage(tr("Pose Test: drag a part to turn its joint. Parts that hit show in red. Done or Esc "
                                "puts them back as built."));
    refreshCommandState();
}

void MainWindow::endPose()
{
    if (!m_poseMode.active)
        return;
    m_poseTimer->stop();
    ++m_poseMode.generation;  // any answer still coming is dropped
    m_poseMode = PoseMode{};
    m_posePanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::rebuildPose(ObjectId hold)
{
    auto& mode = m_poseMode;
    const ObjectId chosenStep = mode.joint >= 0 && mode.joint < int(mode.joints.size())
                                    ? mode.joints[std::size_t(mode.joint)].step
                                    : kInvalidId;
    if (hold == kInvalidId && m_doc.find(mode.rig.hold) && m_doc.isTopLevel(mode.rig.hold))
        hold = mode.rig.hold;
    mode.joints = pose::findJoints(m_doc);
    std::map<ObjectId, double> sizes;
    std::map<ObjectId, Bounds> bounds;
    for (ObjectId id : m_doc.topLevelIds()) {
        const Bounds b = m_doc.worldBounds(id);
        bounds[id] = b;
        if (b.valid) {
            const auto s = b.size();
            sizes[id] = double(s[0]) * s[1] * s[2];
        }
    }
    mode.rig = pose::build(mode.joints, sizes, bounds, hold);
    // Angles for joints that are gone are forgotten.
    std::map<ObjectId, std::array<double, 3>> kept;
    mode.joint = -1;
    QStringList names;
    for (std::size_t i = 0; i < mode.joints.size(); ++i) {
        const pose::Joint& j = mode.joints[i];
        if (const auto a = mode.angles.find(j.step); a != mode.angles.end())
            kept[j.step] = a->second;
        if (j.step == chosenStep)
            mode.joint = int(i);
        QString name = QString::fromStdString(j.name);
        if (j.other != kInvalidId && m_doc.find(j.other))
            name += tr(" ↔ %1").arg(QString::fromStdString(m_doc.find(j.other)->name));
        else
            name += tr(" (nothing turns in it)");
        names << name;
    }
    mode.angles = std::move(kept);
    m_posePanel->setJoints(names);
    const Body* held = m_doc.find(mode.rig.hold);
    m_posePanel->setHold(held ? QString::fromStdString(held->name) : QString());
    choosePoseJoint(mode.joint);
    posed();
}

void MainWindow::posed(bool checkSoon)
{
    auto& mode = m_poseMode;
    mode.moved = pose::transforms(mode.joints, mode.rig, mode.angles);
    ++mode.generation;
    if (checkSoon) {
        mode.hitParts.clear();
        mode.hitMesh.reset();
        m_posePanel->setHits(tr("Checking for parts that hit..."), false);
        m_poseTimer->start();
    }
    refreshScene();
}

void MainWindow::choosePoseJoint(int index)
{
    auto& mode = m_poseMode;
    if (index < 0 || index >= int(mode.joints.size())) {
        mode.joint = -1;
        m_posePanel->showJoint(-1, pose::JointKind::Hinge, {0, 0, 0});
        return;
    }
    mode.joint = index;
    const pose::Joint& j = mode.joints[std::size_t(index)];
    const auto a = mode.angles.find(j.step);
    m_posePanel->showJoint(index, j.kind, a == mode.angles.end() ? std::array<double, 3>{0, 0, 0} : a->second);
    m_posePanel->setRange({});
    if (j.other == kInvalidId)
        m_posePanel->setStatus(tr("Nothing was found turning in this joint (the other part should reach in to "
                                  "the pin or ball)."),
                               true);
    else if (j.kind != pose::JointKind::Fixed)
        m_posePanel->setStatus({}, false);
}

std::vector<pose::PosedPart> MainWindow::posedParts() const
{
    std::vector<pose::PosedPart> parts;
    for (ObjectId id : m_doc.topLevelIds()) {
        try {
            pose::PosedPart p;
            p.id = id;
            p.solid = meshkernel::transformed(m_doc.meshSolid(id), m_doc.worldTransform(id));
            if (const auto m = m_poseMode.moved.find(id); m != m_poseMode.moved.end())
                p.moved = m->second;
            parts.push_back(std::move(p));
        } catch (const std::exception&) {
            // a part that can't be worked out isn't checked
        }
    }
    return parts;
}

void MainWindow::checkPoseHits()
{
    if (!m_poseMode.active)
        return;
    if (m_poseMode.moved.empty()) {
        m_poseMode.hitParts.clear();
        m_poseMode.hitMesh.reset();
        m_posePanel->setHits(tr("The build pose."), false);
        refreshScene();
        return;
    }
    const int generation = m_poseMode.generation;
    auto parts = posedParts();
    QPointer<MainWindow> self(this);
    QThreadPool::globalInstance()->start([self, generation, parts = std::move(parts)] {
        std::vector<pose::Hit> hits;
        try {
            hits = pose::collisions(parts);
        } catch (const std::exception& e) {
            qWarning() << "Checking the pose failed:" << e.what();
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, hits = std::move(hits)] {
                if (!self || !self->m_poseMode.active || self->m_poseMode.generation != generation)
                    return;  // the pose has changed since
                auto& mode = self->m_poseMode;
                mode.hitParts.clear();
                std::vector<Mesh> overlaps;
                QStringList pairs;
                for (const pose::Hit& h : hits) {
                    mode.hitParts.insert(h.a);
                    mode.hitParts.insert(h.b);
                    overlaps.push_back(h.overlap);
                    const Body* a = self->m_doc.find(h.a);
                    const Body* b = self->m_doc.find(h.b);
                    if (a && b)
                        pairs << tr("%1 and %2").arg(QString::fromStdString(a->name), QString::fromStdString(b->name));
                }
                mode.hitMesh = overlaps.empty() ? nullptr : together(overlaps);
                if (hits.empty())
                    self->m_posePanel->setHits(tr("Clear: no parts hit."), false);
                else
                    self->m_posePanel->setHits(tr("Hit: %1.").arg(pairs.join(QStringLiteral("; "))), true);
                self->refreshScene();
            },
            Qt::QueuedConnection);
    });
}

void MainWindow::findPoseRange()
{
    auto& mode = m_poseMode;
    if (mode.busy || mode.joint < 0)
        return;
    const std::size_t index = std::size_t(mode.joint);
    const pose::Joint& j = mode.joints[index];
    if (j.kind != pose::JointKind::Hinge && j.kind != pose::JointKind::Swivel)
        return;
    if (j.other == kInvalidId) {
        m_posePanel->setRange(tr("Nothing turns in this joint."));
        return;
    }
    mode.busy = true;
    m_posePanel->setRange(tr("Turning it each way until parts hit..."));
    const int generation = mode.generation;
    auto parts = posedParts();
    for (auto& p : parts)
        p.moved = Transform{};
    QPointer<MainWindow> self(this);
    QThreadPool::globalInstance()->start([self, generation, index, joints = mode.joints, rig = mode.rig,
                                          angles = mode.angles, parts = std::move(parts)] {
        std::optional<std::array<double, 2>> range;
        try {
            range = pose::range(joints, rig, angles, index, parts);
        } catch (const std::exception& e) {
            qWarning() << "Finding the range failed:" << e.what();
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, generation, index, range] {
                if (!self || !self->m_poseMode.active)
                    return;
                self->m_poseMode.busy = false;
                if (self->m_poseMode.joint != int(index))
                    return;
                if (!range) {
                    self->m_posePanel->setRange(tr("Couldn't find the range."));
                    return;
                }
                const double back = -(*range)[0], forward = (*range)[1];
                const auto say = [](double a) {
                    return a >= 180.0 ? tr("all the way (180°)") : tr("%1°").arg(a, 0, 'f', 0);
                };
                self->m_posePanel->setRange(tr("From straight it turns %1 one way (+) and %2 the other (−) "
                                               "before parts hit.")
                                                .arg(say(forward), say(back)));
                if (forward < 3.0 && back < 3.0)
                    self->m_posePanel->setRange(
                        tr("It hits almost straight away, both ways: the parts' ends are too close round the joint. "
                           "Round them off or bevel them (or move the parts apart a little) so they can swing."));
                Q_UNUSED(generation);
            },
            Qt::QueuedConnection);
    });
}

void MainWindow::makePoseRoom()
{
    auto& mode = m_poseMode;
    if (mode.busy || mode.joint < 0)
        return;
    const std::size_t index = std::size_t(mode.joint);
    const pose::Joint& j = mode.joints[index];
    if (j.kind == pose::JointKind::Fixed)
        return;
    const double target = m_posePanel->roomTarget();
    const int which = m_posePanel->roomWhich();
    const bool fromTurning = m_posePanel->roomFromTurning();
    const double clearance = m_posePanel->roomClearance();
    const std::string what = j.name + " to " + QString::number(target, 'f', 0).toStdString() + "\xC2\xB0";
    mode.busy = true;
    m_posePanel->setRoomBusy(true);
    m_posePanel->setStatus(tr("Turning it to %1° to see what's in the way...").arg(target, 0, 'f', 0), false);
    auto parts = posedParts();
    for (auto& p : parts)
        p.moved = Transform{};
    const int serial = m_docSerial;
    QPointer<MainWindow> self(this);
    QThreadPool::globalInstance()->start([self, serial, index, which, target, fromTurning, clearance, what,
                                          joints = mode.joints, rig = mode.rig, angles = mode.angles,
                                          parts = std::move(parts)] {
        std::vector<pose::Room> rooms;
        QString problem;
        try {
            rooms = pose::room(joints, rig, angles, index, which, target, parts, fromTurning, clearance);
        } catch (const std::exception& e) {
            problem = QString::fromUtf8(e.what());
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, serial, rooms = std::move(rooms), problem, what] {
                if (!self || !self->m_poseMode.active)
                    return;
                self->m_poseMode.busy = false;
                self->m_posePanel->setRoomBusy(false);
                if (self->m_docSerial != serial) {
                    self->m_posePanel->setStatus(tr("The shapes changed meanwhile: try again."), true);
                    return;
                }
                if (!problem.isEmpty()) {
                    self->m_posePanel->setStatus(problem, true);
                    return;
                }
                if (rooms.empty()) {
                    self->m_posePanel->setStatus(tr("Nothing's in the way: it turns that far already."), false);
                    return;
                }
                double volume = 0.0;
                for (const auto& r : rooms)
                    volume += r.volume;
                try {
                    makeRoom(self->m_stack, self->m_doc, rooms, what);
                    qInfo().noquote() << "Command:" << QString::fromStdString(self->m_stack.undoLabel());
                    self->m_posePanel->setStatus(tr("Made room: %1 mm³ cut away. Turn it to check; Undo puts it "
                                                    "back.")
                                                     .arg(volume, 0, 'f', volume < 10 ? 1 : 0),
                                                 false);
                } catch (const std::exception& e) {
                    self->m_posePanel->setStatus(tr("Couldn't make room: %1").arg(QString::fromUtf8(e.what())),
                                                 true);
                }
            },
            Qt::QueuedConnection);
    });
}

void MainWindow::poseClick(const QVector3D& origin, const QVector3D& direction)
{
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {direction.x(), direction.y(), direction.z()};
    const auto hit = pick(m_doc, ray, nullptr, &m_poseMode.moved);
    if (!hit)
        return;
    ObjectId part = hit->id;
    if (const auto r = m_poseMode.rig.rides.find(part); r != m_poseMode.rig.rides.end())
        part = r->second;
    setSelection(part);
    const auto j = m_poseMode.rig.joint.find(part);
    if (j == m_poseMode.rig.joint.end()) {
        statusBar()->showMessage(part == m_poseMode.rig.hold || !m_poseMode.rig.parent.count(part)
                                     ? tr("That part is held still (Hold Still holds another).")
                                     : tr("That part isn't on a joint."),
                                 5000);
        return;
    }
    choosePoseJoint(int(j->second));
}

bool MainWindow::poseGrab(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_poseMode;
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {direction.x(), direction.y(), direction.z()};
    const auto hit = pick(m_doc, ray, nullptr, &mode.moved);
    if (!hit)
        return false;
    ObjectId part = hit->id;
    if (const auto r = mode.rig.rides.find(part); r != mode.rig.rides.end())
        part = r->second;
    const auto ji = mode.rig.joint.find(part);
    if (ji == mode.rig.joint.end())
        return false;
    const pose::Joint& j = mode.joints[ji->second];
    if (j.kind == pose::JointKind::Fixed)
        return false;
    choosePoseJoint(int(ji->second));
    setSelection(part);
    // The joint where it is now: moved with the part it turns from.
    const ObjectId parent = mode.rig.parent.at(part);
    const auto pm = mode.moved.find(parent);
    const Transform parentMoved = pm == mode.moved.end() ? Transform{} : pm->second;
    mode.dragPivot = parentMoved.apply(j.pivot);
    const QVector3D view = direction.normalized();
    const Vec3 viewDir{view.x(), view.y(), view.z()};
    mode.dragAxis = j.kind == pose::JointKind::Ball ? viewDir : parentMoved.applyDirection(j.axis);
    const auto a = mode.angles.find(j.step);
    mode.startAngles = a == mode.angles.end() ? std::array<double, 3>{0, 0, 0} : a->second;
    mode.startTurn = pose::turn(j, mode.startAngles);
    mode.dragViewAxis = parentMoved.inverse().applyDirection(viewDir);
    // Where the mouse is on the plane the joint turns in.
    const QVector3D n(float(mode.dragAxis[0]), float(mode.dragAxis[1]), float(mode.dragAxis[2]));
    const QVector3D p(float(mode.dragPivot[0]), float(mode.dragPivot[1]), float(mode.dragPivot[2]));
    const auto at = view::intersectPlane(origin, direction, p, n);
    if (!at)
        return false;
    mode.grabbed = {at->x(), at->y(), at->z()};
    mode.dragging = true;
    return true;
}

void MainWindow::poseDrag(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_poseMode;
    if (!mode.dragging || mode.joint < 0)
        return;
    const pose::Joint& j = mode.joints[std::size_t(mode.joint)];
    const QVector3D n(float(mode.dragAxis[0]), float(mode.dragAxis[1]), float(mode.dragAxis[2]));
    const QVector3D p(float(mode.dragPivot[0]), float(mode.dragPivot[1]), float(mode.dragPivot[2]));
    const auto at = view::intersectPlane(origin, direction, p, n);
    if (!at)
        return;
    // The angle the mouse has gone round the pivot, round the axis.
    const QVector3D from = QVector3D(float(mode.grabbed[0]), float(mode.grabbed[1]), float(mode.grabbed[2])) - p;
    const QVector3D to = *at - p;
    if (from.lengthSquared() < 1e-8f || to.lengthSquared() < 1e-8f)
        return;
    const float s = QVector3D::dotProduct(n, QVector3D::crossProduct(from, to));
    const float c = QVector3D::dotProduct(from, to);
    const double degrees = std::atan2(double(s), double(c)) * 180.0 / std::numbers::pi;
    std::array<double, 3> angles = mode.startAngles;
    if (j.kind == pose::JointKind::Ball) {
        const Transform r = pose::rotationAbout(j.pivot, mode.dragViewAxis, degrees) * mode.startTurn;
        angles = pose::ballAngles(j, r);
    } else {
        double a = mode.startAngles[0] + degrees;
        while (a > 180.0)
            a -= 360.0;
        while (a < -180.0)
            a += 360.0;
        angles[0] = j.clicks > 0 ? pose::nearestClick(j, a)  // it only rests on a click
                                 : std::round(a * 2.0) / 2.0;  // half degrees: easy to read
    }
    mode.angles[j.step] = angles;
    m_posePanel->showJoint(mode.joint, j.kind, angles);
    mode.savedPose = -1;
    posed();
}

void MainWindow::refreshSavedPoses()
{
    QStringList names;
    for (const SavedPose& p : m_doc.poses())
        names << QString::fromStdString(p.name);
    m_posePanel->setPoses(names, m_poseMode.savedPose);
}

// ---- Quick Hinge -------------------------------------------------------------------

namespace {

// The settings used last (kept between runs), so a figure's joints come out alike.
HingeFinish rememberedHinge()
{
    const Preferences p;
    HingeFinish f;
    f.kind = HingeKind(std::clamp(p.value(QStringLiteral("hinge/kind"), 0).toInt(), 0, 2));
    f.width = p.value(QStringLiteral("hinge/width"), f.width).toDouble();
    f.barrel = p.value(QStringLiteral("hinge/barrel"), f.barrel).toDouble();
    f.pin = PinKind(std::clamp(p.value(QStringLiteral("hinge/pin"), 0).toInt(), 0, 2));
    f.pinSize = p.value(QStringLiteral("hinge/pinSize"), f.pinSize).toDouble();
    f.resin = p.value(QStringLiteral("hinge/printer"), QStringLiteral("resin")).toString() != QStringLiteral("fdm");
    f.fit = p.value(QStringLiteral("hinge/fit"), f.resin ? 0.1 : 0.2).toDouble();
    f.stiff = p.value(QStringLiteral("hinge/stiff"), true).toBool();
    f.swing = HingeSwing(std::clamp(p.value(QStringLiteral("hinge/swing"), 0).toInt(), 0, 2));
    f.gap = p.value(QStringLiteral("hinge/gap"), f.gap).toDouble();
    f.lip = p.value(QStringLiteral("hinge/lip"), f.lip).toDouble();
    f.clicks = std::clamp(p.value(QStringLiteral("hinge/clicks"), 0).toInt(), 0, 36);
    f.clickDepth = p.value(QStringLiteral("hinge/clickDepth"), f.clickDepth).toDouble();
    try {
        hinge::check(f);
    } catch (const std::exception&) {
        return HingeFinish{};
    }
    return f;
}

void rememberHinge(const HingeFinish& f, bool fromPart)
{
    Preferences p;
    p.setValue(QStringLiteral("hinge/kind"), int(f.kind));
    p.setValue(QStringLiteral("hinge/width"), f.width);
    p.setValue(QStringLiteral("hinge/barrel"), f.barrel);
    p.setValue(QStringLiteral("hinge/pin"), int(f.pin));
    p.setValue(QStringLiteral("hinge/pinSize"), f.pinSize);
    p.setValue(QStringLiteral("hinge/printer"), f.resin ? QStringLiteral("resin") : QStringLiteral("fdm"));
    p.setValue(QStringLiteral("hinge/fit"), f.fit);
    p.setValue(QStringLiteral("hinge/stiff"), f.stiff);
    p.setValue(QStringLiteral("hinge/swing"), int(f.swing));
    p.setValue(QStringLiteral("hinge/gap"), f.gap);
    p.setValue(QStringLiteral("hinge/lip"), f.lip);
    p.setValue(QStringLiteral("hinge/clicks"), f.clicks);
    p.setValue(QStringLiteral("hinge/clickDepth"), f.clickDepth);
    p.setValue(QStringLiteral("hinge/fromPart"), fromPart);
}

// Closed surfaces, as one mesh to show.
std::shared_ptr<const Mesh> together(const std::vector<Mesh>& pieces)
{
    auto out = std::make_shared<Mesh>();
    for (const Mesh& piece : pieces) {
        const auto first = std::uint32_t(out->positions.size() / 3);
        out->positions.insert(out->positions.end(), piece.positions.begin(), piece.positions.end());
        for (std::uint32_t i : piece.indices)
            out->indices.push_back(first + i);
    }
    out->normals.assign(out->positions.size(), 0.0f);
    for (std::size_t t = 0; t + 2 < out->indices.size(); t += 3) {
        const float* a = &out->positions[3 * out->indices[t]];
        const float* b = &out->positions[3 * out->indices[t + 1]];
        const float* c = &out->positions[3 * out->indices[t + 2]];
        const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        for (int k = 0; k < 3; ++k)
            for (int j = 0; j < 3; ++j)
                out->normals[3 * out->indices[t + std::size_t(k)] + std::size_t(j)] += n[j];
    }
    for (std::size_t i = 0; i + 2 < out->normals.size(); i += 3) {
        const float l = std::sqrt(out->normals[i] * out->normals[i] + out->normals[i + 1] * out->normals[i + 1] +
                                  out->normals[i + 2] * out->normals[i + 2]);
        if (l > 0)
            for (int j = 0; j < 3; ++j)
                out->normals[i + std::size_t(j)] /= l;
    }
    return out;
}

// A number both halves of one hinge share.
std::uint32_t newHingeLink()
{
    std::uint32_t link = 0;
    while (link == 0)
        link = QRandomGenerator::global()->generate();
    return link;
}

}  // namespace

void MainWindow::startHinge(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    HingeMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    HingeFinish finish = rememberedHinge();
    if (editing) {
        if (!body->isBoolean() || body->boolean->op != BooleanOp::Hinge || !body->boolean->hinge)
            return;
        finish = *body->boolean->hinge;
        try {
            mode.spot = hingeSpot(m_doc, target);
        } catch (const std::exception& e) {
            QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
            return;
        }
    }
    m_hingeMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    refreshStandardRows();
    m_hingePanel->start(editing ? tr("Change the joint in '%1'").arg(name) : tr("Joint in '%1'").arg(name), editing);
    m_hingePanel->setFinish(finish);
    if (!editing)
        m_hingePanel->setOtherFromPart(Preferences().value(QStringLiteral("hinge/fromPart"), false).toBool());
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_hingePanel->show();
    statusBar()->showMessage(editing ? tr("Quick Hinge: change the settings, or click elsewhere on the shape to "
                                          "move it, then Apply. The other half changes with it. Esc cancels.")
                                     : tr("Quick Hinge: click the spot on the shape where the joint goes. Esc "
                                          "cancels."));
    updateHingePreview();
    refreshCommandState();
}

void MainWindow::hingeClick(const QVector3D& origin, const QVector3D& direction)
{
    const QVector3D d = direction.normalized();
    const Vec3 o{origin.x(), origin.y(), origin.z()}, dir{d.x(), d.y(), d.z()};
    if (m_hingeMode.pickingOther) {
        Ray ray;
        ray.origin = o;
        ray.direction = dir;
        const auto hit = pickShown(ray);
        const ObjectId fork = history::objectOf(m_doc, m_hingeMode.target);
        if (!hit) {
            statusBar()->showMessage(tr("Quick Hinge: click on the other part."), 4000);
            return;
        }
        const ObjectId part = history::objectOf(m_doc, hit->id);
        if (part == fork) {
            statusBar()->showMessage(tr("That's the part with the fork: click the other part."), 5000);
            return;
        }
        m_hingeMode.other = part;
        m_hingeMode.pickingOther = false;
        m_hingePanel->setOtherPart(QString::fromStdString(m_doc.find(part)->name), false);
        m_hingeTimer->start(0);
        return;
    }
    std::optional<emboss::SurfacePoint> spot;
    try {
        const HingeFinish f = m_hingePanel->finish();
        spot = surfaceUnder(m_doc, m_hingeMode.target, o, dir, std::max(0.25, std::max(f.width, f.barrel) / 2.0));
    } catch (const std::exception& e) {
        m_hingePanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    if (!spot) {
        statusBar()->showMessage(tr("Quick Hinge: click on the shape itself."), 4000);
        return;
    }
    m_hingeMode.spot = spot;
    m_hingePanel->setPlaced(true);
    m_hingeTimer->start(0);
}

std::optional<BooleanSpec> MainWindow::currentHingeSpec() const
{
    if (!m_hingeMode.active || !m_hingeMode.spot)
        return std::nullopt;
    HingeFinish f = m_hingePanel->finish();
    if (m_hingeMode.editing) {
        const Body* now = m_doc.find(m_hingeMode.target);
        if (now && now->boolean && now->boolean->hinge) {
            f.half = now->boolean->hinge->half;
            f.link = now->boolean->hinge->link;
        }
        return withHinge(m_doc, m_hingeMode.target, *m_hingeMode.spot, f).boolean;
    }
    return hingeSpec(m_doc, m_hingeMode.target, *m_hingeMode.spot, f);
}

std::array<double, 2> MainWindow::hingePinSpan(const HingeFinish& f, const emboss::SurfacePoint& spot,
                                               ObjectId forkPart) const
{
    // A printed pin as long as the part is wide there: from the surface on one side of
    // the pin's line to the surface on the other.
    std::array<double, 2> span{0.0, 0.0};
    if (f.pin == PinKind::Printed && f.kind != HingeKind::Swivel) {
        const Vec3 axis = hinge::pinAxis(f, spot.normal);
        const double depth = hinge::pinDepth(f);
        const Vec3 centre{spot.point[0] - spot.normal[0] * depth, spot.point[1] - spot.normal[1] * depth,
                          spot.point[2] - spot.normal[2] * depth};
        const double far = 4.0 * hinge::reachOf(f) + 50.0;
        std::optional<double> ends[2];
        for (int side = 0; side < 2; ++side) {
            const double s = side == 0 ? 1.0 : -1.0;
            const Vec3 from{centre[0] + axis[0] * far * s, centre[1] + axis[1] * far * s, centre[2] + axis[2] * far * s};
            const Vec3 toward{-axis[0] * s, -axis[1] * s, -axis[2] * s};
            try {
                if (const auto hit = surfaceUnder(m_doc, forkPart, from, toward, 0.0)) {
                    const Vec3 v{hit->point[0] - centre[0], hit->point[1] - centre[1], hit->point[2] - centre[2]};
                    ends[side] = std::abs(v[0] * axis[0] + v[1] * axis[1] + v[2] * axis[2]);
                }
            } catch (const std::exception&) {
            }
        }
        // ends[0] is on the + side of the pin's line; the span runs - then +.
        if (ends[0] && ends[1])
            span = {std::min(*ends[1], hinge::reachOf(f)), std::min(*ends[0], hinge::reachOf(f))};
    }
    return span;
}

std::vector<Body> MainWindow::hingePieceBodies(const HingeFinish& f, const emboss::SurfacePoint& spot, bool withTab,
                                               ObjectId forkPart) const
{
    const auto span = hingePinSpan(f, spot, forkPart);
    std::vector<Body> out;
    for (const hinge::Piece& piece : hinge::pieces(f, spot.point, spot.normal, withTab, span)) {
        out.push_back(hinge::pieceBody(piece.mesh, piece.name));
        out.back().hardware = true;  // a tab, link or pin: keeps its size when the design is rescaled
    }
    return out;
}

void MainWindow::updateHingePreview()
{
    if (!m_hingeMode.active)
        return;
    m_hingeTimer->stop();
    m_hingeMode.preview.reset();
    m_hingeMode.cut.reset();
    m_hingeMode.ghost.reset();
    bool ok = false;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        const HingeFinish f = m_hingePanel->finish();
        hinge::check(f);
        if (const auto spec = currentHingeSpec()) {
            const ObjectId part = spec->operands.front();
            const auto shown = m_doc.mesh(part);
            const HingeFinish& g = *spec->hinge;
            if (shown && shown->triangleCount() > kSocketPreviewTriangles) {
                if (g.half == HingeHalf::Fork)
                    m_hingeMode.cut = together(hinge::forkCutters(g, m_hingeMode.spot->point, m_hingeMode.spot->normal));
                m_hingePanel->setStatus(tr("Red is what will be cut away (the shape is too big to show the finished "
                                           "joint quickly); blue is the other half. Apply makes it."),
                                        false);
            } else {
                m_hingeMode.preview = m_doc.previewMesh(*spec);
                m_hingePanel->setStatus(tr("This is how it will look; blue is the other half."), false);
            }
            // The other half, as a ghost: the tab (or link, or peg) and the pins.
            if (g.half == HingeHalf::Fork) {
                std::vector<Mesh> ghosts;
                const auto span = hingePinSpan(g, *m_hingeMode.spot, part);
                for (const hinge::Piece& p :
                     hinge::pieces(g, m_hingeMode.spot->point, m_hingeMode.spot->normal, true, span))
                    ghosts.push_back(p.mesh);
                if (!ghosts.empty())
                    m_hingeMode.ghost = together(ghosts);
            }
            ok = !m_hingePanel->otherFromPart() || m_hingeMode.editing || m_hingeMode.other != kInvalidId;
            if (!ok)
                m_hingePanel->setStatus(tr("Pick the other part, or choose a new shape for the other half."), false);
        } else {
            m_hingePanel->setStatus({}, false);
        }
    } catch (const std::exception& e) {
        m_hingePanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    m_hingePanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyHinge()
{
    if (!m_hingeMode.active)
        return;
    std::optional<BooleanSpec> spec;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        spec = currentHingeSpec();
        if (!spec)
            return;
        m_doc.previewMesh(*spec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("The joint could not be made:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    const bool fromPart = m_hingePanel->otherFromPart();
    const emboss::SurfacePoint spot = *m_hingeMode.spot;
    const ObjectId target = m_hingeMode.target;
    HingeFinish f = *spec->hinge;

    if (m_hingeMode.editing) {
        rememberHinge(f, fromPart);
        const Body* now = m_doc.find(target);
        if (!now)
            return endHinge();
        const ObjectId partner = hingePartner(m_doc, target);
        endHinge();
        try {
            m_stack.beginMacro("Change joint");
            Body after = *now;
            after.boolean = *spec;
            m_stack.execute(std::make_unique<EditBodyCommand>(*now, after, "Change joint"));
            if (partner != kInvalidId) {
                // The other half follows: the same joint, from the new spot.
                const Body* other = m_doc.find(partner);
                const hinge::Half h = hinge::otherHalf(f, spot.point, spot.normal);
                HingeFinish g = h.finish;
                g.half = other->boolean->hinge->half;
                const Body moved = withHinge(m_doc, partner, {h.point, h.normal}, g);
                m_doc.previewMesh(*moved.boolean);
                m_stack.execute(std::make_unique<EditBodyCommand>(*other, moved, "Change joint"));
            }
            m_stack.endMacro();
        } catch (const std::exception& e) {
            m_stack.abortMacro();
            QMessageBox::warning(this, tr("Forge3D"),
                                 tr("The joint could not be changed:\n%1").arg(QString::fromUtf8(e.what())));
            return;
        }
        refreshScene();
        refreshCommandState();
        setSelection(history::objectOf(m_doc, target));
        return;
    }

    const ObjectId other = fromPart ? m_hingeMode.other : kInvalidId;
    if (other != kInvalidId)
        f.link = newHingeLink();
    spec->hinge = f;
    rememberHinge(f, fromPart);
    // The shapes to print with it (worked out before anything changes, on the part as it is).
    std::vector<Body> pieces;
    try {
        pieces = hingePieceBodies(f, spot, other == kInvalidId, target);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("The joint could not be made:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    endHinge();
    try {
        m_stack.beginMacro(f.kind == HingeKind::Swivel ? "Quick Swivel" : "Quick Hinge");
        Body result;
        result.id = m_doc.allocateId();
        result.name = m_doc.uniqueName("Hinged");
        result.boolean = *spec;
        if (!m_doc.isTopLevel(target))
            m_stack.execute(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target)));
        else
            m_stack.execute(std::make_unique<CombineCommand>(std::move(result)));
        if (other != kInvalidId && m_doc.find(other)) {
            const hinge::Half h = hinge::otherHalf(f, spot.point, spot.normal);
            Body half;
            half.id = m_doc.allocateId();
            half.name = m_doc.uniqueName("Hinged");
            half.boolean = hingeSpec(m_doc, history::tip(m_doc, other), {h.point, h.normal}, h.finish);
            m_doc.previewMesh(*half.boolean);
            m_stack.execute(std::make_unique<CombineCommand>(std::move(half)));
        }
        for (Body& piece : pieces) {
            piece.id = m_doc.allocateId();
            std::string name = piece.name;
            for (int n = 2; std::any_of(m_doc.bodies().begin(), m_doc.bodies().end(),
                                        [&](const Body& b) { return b.name == name; });
                 ++n)
                name = piece.name + " " + std::to_string(n);
            piece.name = name;
            m_stack.execute(std::make_unique<AddBodyCommand>(std::move(piece)));
        }
        m_stack.endMacro();
    } catch (const std::exception& e) {
        m_stack.abortMacro();
        QMessageBox::warning(this, tr("Forge3D"), tr("The joint could not be made:\n%1").arg(QString::fromUtf8(e.what())));
        refreshScene();
        return;
    }
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    refreshScene();
    refreshCommandState();
    setSelection(history::objectOf(m_doc, target));
    if (!pieces.empty())
        statusBar()->showMessage(tr("Made. The tab, link, peg or pins are new shapes in place: Merge a tab onto its "
                                    "part, or lay them out to print."),
                                 8000);
}

void MainWindow::addHingePieces()
{
    if (!m_hingeMode.active || !m_hingeMode.spot)
        return;
    const HingeFinish f = m_hingePanel->finish();
    std::vector<Body> pieces;
    try {
        const ObjectId part = m_hingeMode.editing ? m_doc.find(m_hingeMode.target)->boolean->operands.front()
                                                  : m_hingeMode.target;
        pieces = hingePieceBodies(f, *m_hingeMode.spot, true, part);
        m_stack.beginMacro("Add joint pieces");
        for (Body& piece : pieces) {
            piece.id = m_doc.allocateId();
            m_stack.execute(std::make_unique<AddBodyCommand>(std::move(piece)));
        }
        m_stack.endMacro();
    } catch (const std::exception& e) {
        m_stack.abortMacro();
        m_hingePanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    refreshScene();
    refreshCommandState();
    m_hingePanel->setStatus(tr("Added %n shape(s) in place.", nullptr, int(pieces.size())), false);
}

void MainWindow::endHinge()
{
    if (!m_hingeMode.active)
        return;
    m_hingeTimer->stop();
    m_hingeMode = HingeMode{};
    m_hingePanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Split by Mask -----------------------------------------------------------------

namespace {

std::uint64_t maskCellKey(std::int64_t i, std::int64_t j, std::int64_t k)
{
    const auto q = [](std::int64_t v) { return std::uint64_t(v + (1 << 20)) & 0x1FFFFF; };
    return q(i) | q(j) << 21 | q(k) << 42;
}

}  // namespace

void MainWindow::startMask(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    MaskMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    mode.object = history::objectOf(m_doc, target);
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::MaskSplit)
                return;
            mode.part = body->boolean->operands.front();
            mode.dabs = maskDabs(m_doc, target);
            for (const Body& b : withMask(m_doc, target, mode.dabs))
                if (b.id != target)
                    mode.hidden = history::objectOf(m_doc, b.id);
            if (!mode.dabs.empty())
                mode.strokeStarts.push_back(0);  // what was painted before, as one stroke
        } else {
            mode.part = target;
        }
        mode.toPlate = m_doc.worldTransform(mode.part);
        const auto surface = m_doc.mesh(mode.part);
        if (!surface || surface->empty())
            throw std::runtime_error("The shape has no surface to paint on.");
        // Shown finely enough for the paint to show where it goes (a box or a cylinder
        // has few points); the cut itself is worked out on the real surface.
        auto shown = std::make_shared<Mesh>(*surface);
        if (shown->triangleCount() < 60'000) {
            const Mesh welded = sculpt::welded(*surface, 1e-4);
            *shown = sculpt::refined(welded, sculpt::edgeForTriangles(welded, 60'000), 200'000);
        }
        shown->shade.assign(shown->vertexCount(), 0.0f);
        mode.shown = shown;
        // The points filed in a grid, to find those under a dab quickly.
        const Bounds b = shown->bounds();
        mode.cell = std::max(0.25, double(b.radius()) / 60.0);
        for (std::uint32_t v = 0; v < shown->vertexCount(); ++v) {
            const float* p = &shown->positions[3 * std::size_t(v)];
            mode.grid[maskCellKey(std::int64_t(std::floor(p[0] / mode.cell)), std::int64_t(std::floor(p[1] / mode.cell)),
                                  std::int64_t(std::floor(p[2] / mode.cell)))]
                .push_back(v);
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_maskMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    double keyFit = -1.0;
    if (editing && body->boolean && body->boolean->maskSplit && body->boolean->maskSplit->keys)
        keyFit = body->boolean->maskSplit->keyFit;
    m_maskPanel->start(editing ? tr("Change the mask of '%1'").arg(name) : tr("Split '%1' by mask").arg(name), editing,
                       keyFit);
    m_maskPanel->setPaintColours(
        paint::colours(paint::objectPaint(m_doc, history::objectOf(m_doc, m_maskMode.part))));
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_maskPanel->show();
    statusBar()->showMessage(tr("Split by Mask: drag over the shape to paint the part to cut off (Ctrl rubs it out), "
                                "then Apply. Esc cancels."));
    repaintMask();
    refreshScene();
    refreshCommandState();
}

std::optional<emboss::SurfacePoint> MainWindow::maskHit(const QVector3D& origin, const QVector3D& direction) const
{
    const auto& mode = m_maskMode;
    if (!mode.shown)
        return std::nullopt;
    const Transform toLocal = mode.toPlate.inverse();
    const auto hit = emboss::surfaceAt(mode.shown->positions, mode.shown->indices,
                                       toLocal.apply({origin.x(), origin.y(), origin.z()}),
                                       toLocal.applyDirection({direction.x(), direction.y(), direction.z()}), 0.2);
    if (!hit)
        return std::nullopt;
    return emboss::SurfacePoint{mode.toPlate.apply(hit->point), mode.toPlate.applyDirection(hit->normal)};
}

void MainWindow::maskHover(const QVector3D& origin, const QVector3D& direction)
{
    const auto hit = maskHit(origin, direction);
    if (!hit) {
        m_viewport->setBrushRing(std::nullopt);
        return;
    }
    m_viewport->setBrushRing(Viewport::BrushRing{
        QVector3D(float(hit->point[0]), float(hit->point[1]), float(hit->point[2])),
        QVector3D(float(hit->normal[0]), float(hit->normal[1]), float(hit->normal[2])), float(m_maskPanel->brushSize())});
}

void MainWindow::maskAt(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_maskMode;
    const auto hit = maskHit(origin, direction);
    if (!hit)
        return;
    const double radius = m_maskPanel->brushSize();
    // Dabs about a quarter of the brush apart, filled in along the way when the mouse
    // jumps, so a stroke is even however fast it goes.
    const double spacing = radius * 0.25;
    if (mode.lastDab) {
        const Vec3 last = *mode.lastDab;
        const double d = std::hypot(hit->point[0] - last[0], hit->point[1] - last[1], hit->point[2] - last[2]);
        if (d < spacing)
            return;
        if (d < radius * 8.0) {  // a jump further than that is to another place: no line to it
            const int steps = int(std::floor(d / spacing));
            for (int i = 1; i < steps; ++i) {
                const double t = double(i) / steps;
                maskDab({last[0] + (hit->point[0] - last[0]) * t, last[1] + (hit->point[1] - last[1]) * t,
                         last[2] + (hit->point[2] - last[2]) * t},
                        radius, mode.erasing);
            }
        }
    }
    mode.lastDab = hit->point;
    maskDab(hit->point, radius, mode.erasing);
}

void MainWindow::maskDab(const Vec3& centre, double radius, bool erase)
{
    auto& mode = m_maskMode;
    mode.dabs.push_back({centre, radius, erase});
    const Vec3 c = mode.toPlate.inverse().apply(centre);
    std::vector<std::uint32_t> touched;
    const auto lo = [&](int k) { return std::int64_t(std::floor((c[std::size_t(k)] - radius) / mode.cell)); };
    const auto hi = [&](int k) { return std::int64_t(std::floor((c[std::size_t(k)] + radius) / mode.cell)); };
    auto& shade = mode.shown->shade;
    const auto& pos = mode.shown->positions;
    for (std::int64_t i = lo(0); i <= hi(0); ++i)
        for (std::int64_t j = lo(1); j <= hi(1); ++j)
            for (std::int64_t k = lo(2); k <= hi(2); ++k) {
                const auto found = mode.grid.find(maskCellKey(i, j, k));
                if (found == mode.grid.end())
                    continue;
                for (std::uint32_t v : found->second) {
                    const double dx = pos[3 * std::size_t(v)] - c[0], dy = pos[3 * std::size_t(v) + 1] - c[1],
                                 dz = pos[3 * std::size_t(v) + 2] - c[2];
                    if (dx * dx + dy * dy + dz * dz <= radius * radius) {
                        shade[v] = erase ? 0.0f : 1.0f;
                        touched.push_back(v);
                    }
                }
            }
    if (touched.empty())
        return;
    std::sort(touched.begin(), touched.end());
    Viewport::MeshChanges changes;
    for (std::size_t i = 0; i < touched.size();) {
        std::size_t j = i + 1;
        while (j < touched.size() && touched[j] <= touched[j - 1] + 1)
            ++j;
        changes.points.push_back({touched[i], touched[j - 1] + 1});
        i = j;
    }
    m_viewport->meshChanged(mode.object, changes);
}

void MainWindow::repaintMask()
{
    auto& mode = m_maskMode;
    if (!mode.active || !mode.shown)
        return;
    const mask::Field field(mode.dabs);
    auto& shade = mode.shown->shade;
    const auto& pos = mode.shown->positions;
    for (std::size_t v = 0; v < shade.size(); ++v)
        shade[v] = field.masked(mode.toPlate.apply({pos[3 * v], pos[3 * v + 1], pos[3 * v + 2]})) ? 1.0f : 0.0f;
    Viewport::MeshChanges all;
    all.everything = true;
    m_viewport->meshChanged(mode.object, all);
    m_maskPanel->setStrokes(int(mode.strokeStarts.size()));
    m_maskPanel->setApplyEnabled(!mode.dabs.empty());
}

void MainWindow::undoMaskStroke()
{
    auto& mode = m_maskMode;
    if (!mode.active || mode.strokeStarts.empty())
        return;
    mode.dabs.resize(mode.strokeStarts.back());
    mode.strokeStarts.pop_back();
    repaintMask();
}

void MainWindow::applyMask()
{
    auto& mode = m_maskMode;
    if (!mode.active)
        return;
    if (mode.dabs.empty()) {
        m_maskPanel->setStatus(tr("Paint over the part to cut off first."), true);
        return;
    }
    const ObjectId target = mode.target;
    const std::vector<MaskDab> dabs = mode.dabs;
    std::vector<ObjectId> made;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (mode.editing) {
            const std::vector<Body> after = withMask(m_doc, target, dabs, m_maskPanel->keyFit());
            std::vector<Body> before;
            for (const Body& b : after)
                before.push_back(*m_doc.find(b.id));
            // Worked out first, so a mask that doesn't cut it is refused here.
            for (const Body& b : after)
                if (m_doc.previewResult(*b.boolean).empty)
                    throw std::invalid_argument("The mask doesn't cut the shape in two.");
            endMask();
            run(std::make_unique<EditBodiesCommand>(before, after, "Change mask"));
            setSelection(history::objectOf(m_doc, target));
            return;
        }
        made = splitByMask(m_stack, m_doc, target, dabs, m_maskPanel->keep(), m_maskPanel->keyFit());
    } catch (const std::exception& e) {
        m_maskPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    endMask();
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    setSelection(made);
}

void MainWindow::endMask()
{
    if (!m_maskMode.active)
        return;
    m_maskMode = MaskMode{};
    m_viewport->setBrushRing(std::nullopt);
    m_maskPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Cut into Parts ------------------------------------------------------------------------

void MainWindow::startCutParts(ObjectId target)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen() || !m_doc.isTopLevel(target))
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    CutPartsMode mode;
    mode.active = true;
    mode.target = target;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        auto rest = std::make_shared<Mesh>(*m_doc.mesh(target));
        const Transform t = m_doc.worldTransform(target);
        for (std::size_t i = 0; i + 2 < rest->positions.size(); i += 3) {
            const Vec3 p = t.apply(Vec3{rest->positions[i], rest->positions[i + 1], rest->positions[i + 2]});
            for (std::size_t k = 0; k < 3; ++k)
                rest->positions[i + k] = float(p[k]);
        }
        if (rest->empty())
            throw std::invalid_argument("There's no surface to cut.");
        mode.fitted = rig::fit(*rest).points;
        mode.points = mode.fitted;
        mode.rest = std::move(rest);
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("This shape can't be cut into parts:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    QStringList standard;
    if (const auto s = standardsfile::active())
        for (const auto& j : s->joints) {
            mode.standard.push_back(j);
            standard << QStringLiteral("%1 (%2)").arg(QString::fromStdString(j.name),
                                                      QString::fromStdString(standards::describe(j)));
        }
    m_cutPartsMode = std::move(mode);
    m_cutPartsPanel->start(tr("Cut '%1' into parts").arg(QString::fromStdString(body->name)), standard);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_cutPartsPanel->show();
    statusBar()->showMessage(tr("Cut into Parts: drag the balls into the joints, choose where to cut (Usual Figure, "
                                "or a joint at a time), then Apply. Esc cancels."));
    updateCutPartsPreview(true);
    refreshCommandState();
}

void MainWindow::endCutParts()
{
    if (!m_cutPartsMode.active)
        return;
    m_cutPartsMode = CutPartsMode{};
    m_cutPartsPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

std::vector<limbcut::Choice> MainWindow::cutPartsChoices() const
{
    const auto& mode = m_cutPartsMode;
    std::vector<limbcut::Choice> out;
    const bool resin = m_cutPartsPanel->resin();
    for (const auto& [joint, chosen] : m_cutPartsPanel->choices()) {
        if (!chosen.cut)
            continue;
        limbcut::Choice c;
        c.joint = rig::Joint(joint);
        c.tilt = {chosen.forward, chosen.side};
        const auto planned = mode.cuts.find(joint);
        const double radius = planned != mode.cuts.end() ? planned->second.limbRadius : 4.0;
        c.ball = limbcut::ballFor(radius);
        c.ball.resin = resin;
        c.ball.fit = fits::starting(resin, tolerance::Kind::Ball);
        const auto hinge = [&](HingeKind kind) {
            c.kind = limbcut::JointKind::Hinge;
            c.hinge = limbcut::hingeFor(radius, kind);
        };
        switch (chosen.type) {
        case CutPartsPanel::JustCut: c.kind = limbcut::JointKind::Cut; break;
        case CutPartsPanel::Ball: c.kind = limbcut::JointKind::Ball; break;
        case CutPartsPanel::Hinge: hinge(HingeKind::Single); break;
        case CutPartsPanel::DoubleHinge: hinge(HingeKind::Double); break;
        case CutPartsPanel::Swivel: hinge(HingeKind::Swivel); break;
        default: {
            const int k = chosen.type - CutPartsPanel::FirstStandard;
            if (k < 0 || k >= int(mode.standard.size()))
                break;
            const auto& j = mode.standard[std::size_t(k)];
            if (j.socket && j.socket->kind == SocketKind::Ball) {
                c.kind = limbcut::JointKind::Ball;
                c.ball = standards::applied(*j.socket, c.ball);
            } else if (j.hinge) {
                c.kind = limbcut::JointKind::Hinge;
                c.hinge = standards::applied(*j.hinge, c.hinge);
            } else {
                c.kind = limbcut::JointKind::Cut;  // a peg or magnet socket isn't a joint for a cut
            }
        }
        }
        c.hinge.resin = resin;
        c.hinge.fit = fits::starting(resin, c.hinge.kind == HingeKind::Swivel ? tolerance::Kind::Peg
                                                                               : tolerance::Kind::Pin);
        out.push_back(c);
    }
    return out;
}

void MainWindow::updateCutPartsPreview(bool replan)
{
    auto& mode = m_cutPartsMode;
    if (!mode.active)
        return;
    const double ball = std::max(0.3, 0.015 * rig::height(mode.points));
    try {
        // Each joint's cut as turned by hand: planned again when a turn changes.
        std::map<int, limbcut::Tilt> tilts;
        for (const auto& [joint, chosen] : m_cutPartsPanel->choices())
            tilts[joint] = {chosen.forward, chosen.side};
        if (replan || mode.cuts.empty() || tilts != mode.plannedTilts) {
            mode.cuts.clear();
            const auto joints = limbcut::cuttable();
            std::vector<limbcut::Tilt> turned;
            for (rig::Joint j : joints)
                turned.push_back(tilts[int(j)]);
            const auto planned = limbcut::plan(*mode.rest, mode.points, joints, turned);
            for (std::size_t i = 0; i < joints.size(); ++i)
                mode.cuts[int(joints[i])] = planned[i];
            mode.plannedTilts = tilts;
        }
        m_cutPartsPanel->setStatus({}, false);
    } catch (const std::exception& e) {
        mode.cuts.clear();
        m_cutPartsPanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    // The skeleton, the joint being chosen bigger.
    const int highlight = mode.picked >= 0 ? mode.picked : int(rig::pivot(rig::Joint(m_cutPartsPanel->joint())));
    mode.skeleton = std::make_shared<const Mesh>(rig::skeletonMesh(mode.points, ball, highlight));
    // A disc at each cut, as wide as the limb's reach there.
    std::vector<Mesh> discs;
    QString info;
    const auto choices = cutPartsChoices();
    for (const limbcut::Choice& c : choices) {
        const auto planned = mode.cuts.find(int(c.joint));
        if (planned == mode.cuts.end())
            continue;
        const LimbCutFinish& f = planned->second.finish;
        // A flat disc a little wider than the limb.
        const QVector3D n(float(f.normal[0]), float(f.normal[1]), float(f.normal[2]));
        QVector3D x = QVector3D::crossProduct(std::abs(n.z()) < 0.9f ? QVector3D(0, 0, 1) : QVector3D(1, 0, 0), n);
        x.normalize();
        const QVector3D y = QVector3D::crossProduct(n, x);
        const QVector3D centre(float(f.point[0]), float(f.point[1]), float(f.point[2]));
        const float r = float(std::max(1.0, planned->second.limbRadius * 1.5));
        Mesh disc;
        constexpr int around = 36;
        for (int side = 0; side < 2; ++side) {
            const QVector3D c = centre + n * (side == 0 ? -0.1f : 0.1f);
            const QVector3D facing = side == 0 ? -n : n;
            const auto base = std::uint32_t(disc.vertexCount());
            for (int k = -1; k < around; ++k) {
                const double t = 2 * std::numbers::pi * k / around;
                const QVector3D q = k < 0 ? c : c + r * (float(std::cos(t)) * x + float(std::sin(t)) * y);
                disc.positions.insert(disc.positions.end(), {q.x(), q.y(), q.z()});
                disc.normals.insert(disc.normals.end(), {facing.x(), facing.y(), facing.z()});
            }
            for (int k = 0; k < around; ++k) {
                const std::uint32_t a = base + 1 + std::uint32_t(k), b = base + 1 + std::uint32_t((k + 1) % around);
                if (side == 0)
                    disc.indices.insert(disc.indices.end(), {base, b, a});
                else
                    disc.indices.insert(disc.indices.end(), {base, a, b});
            }
        }
        discs.push_back(std::move(disc));
        if (int(c.joint) == m_cutPartsPanel->joint()) {
            switch (c.kind) {
            case limbcut::JointKind::Ball:
                info = tr("A %1 mm ball on the piece nearer the middle, its socket in the limb (fit %2 mm).")
                           .arg(c.ball.size)
                           .arg(c.ball.fit);
                break;
            case limbcut::JointKind::Hinge:
                info = QString::fromStdString(hinge::describe(c.hinge)) +
                       tr(" (fit %1 mm); the limb's end becomes the tab.").arg(c.hinge.fit);
                break;
            default: info = tr("Cut straight across, nothing put in."); break;
            }
            info += tr(" The limb is about %1 mm thick here.").arg(2.0 * planned->second.limbRadius, 0, 'f', 1);
        }
    }
    m_cutPartsPanel->setInfo(info);
    mode.discs = discs.empty() ? nullptr : together(discs);
    refreshScene();
}

bool MainWindow::cutPartsGrab(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_cutPartsMode;
    const QVector3D d = direction.normalized();
    const double reach = 2.0 * std::max(0.3, 0.015 * rig::height(mode.points));
    int best = -1;
    double bestT = std::numeric_limits<double>::max();
    for (int i = 0; i < kRigPointCount; ++i) {
        const Vec3& p = mode.points[std::size_t(i)];
        const QVector3D c{float(p[0]), float(p[1]), float(p[2])};
        const float t = QVector3D::dotProduct(c - origin, d);
        if (t < 0)
            continue;
        if ((origin + t * d - c).length() <= reach && t < bestT) {
            bestT = t;
            best = i;
        }
    }
    if (best < 0)
        return false;
    // The joint at this ball, chosen in the panel.
    for (rig::Joint j : limbcut::cuttable())
        if (int(rig::pivot(j)) == best)
            m_cutPartsPanel->setJoint(int(j));
    mode.picked = best;
    mode.dragPlanePoint = mode.points[std::size_t(best)];
    mode.dragPlaneNormal = {-d.x(), -d.y(), -d.z()};
    updateCutPartsPreview(false);
    return true;
}

void MainWindow::cutPartsDrag(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_cutPartsMode;
    if (mode.picked < 0)
        return;
    const QVector3D p(float(mode.dragPlanePoint[0]), float(mode.dragPlanePoint[1]), float(mode.dragPlanePoint[2]));
    const QVector3D n(float(mode.dragPlaneNormal[0]), float(mode.dragPlaneNormal[1]), float(mode.dragPlaneNormal[2]));
    const auto hit = view::intersectPlane(origin, direction, p, n);
    if (!hit)
        return;
    const Vec3 target{hit->x(), hit->y(), hit->z()};
    const rig::Points before = mode.points;
    mode.points[std::size_t(mode.picked)] = target;
    const rig::Point other = rig::mirrored(rig::Point(mode.picked));
    if (m_cutPartsPanel->mirror() && int(other) != mode.picked) {
        const double middle = mode.points[std::size_t(rig::Point::Waist)][0];
        mode.points[std::size_t(other)] = {2 * middle - target[0], target[1], target[2]};
    }
    PosedFinish f;
    f.points = mode.points;
    try {
        rig::check(f);
    } catch (const std::exception&) {
        mode.points = before;
        return;
    }
    updateCutPartsPreview(false);  // planned again when the ball is let go
}

void MainWindow::applyCutParts()
{
    auto& mode = m_cutPartsMode;
    if (!mode.active)
        return;
    const auto choices = cutPartsChoices();
    if (choices.empty()) {
        m_cutPartsPanel->setStatus(tr("Choose where to cut first: try Usual Figure."), true);
        return;
    }
    const ObjectId target = mode.target;
    const rig::Points points = mode.points;
    limbcut::Made made;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        std::uint32_t link = 0;
        while (link == 0)
            link = QRandomGenerator::global()->generate() & 0x3FFFFFFFu;
        made = limbcut::cutIntoParts(m_stack, m_doc, target, points, choices, link);
    } catch (const std::exception& e) {
        m_cutPartsPanel->setStatus(tr("It couldn't be cut: %1").arg(QString::fromUtf8(e.what())), true);
        refreshScene();
        return;
    }
    endCutParts();
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    refreshScene();
    refreshCommandState();
    setSelection(made.pieces);
    statusBar()->showMessage(
        made.extras.empty()
            ? tr("Cut into %1 parts. Pose Test turns them at their joints.").arg(made.pieces.size())
            : tr("Cut into %1 parts, with %2 hinge pins or links to print beside them. Pose Test turns them at "
                 "their joints.")
                  .arg(made.pieces.size())
                  .arg(made.extras.size()),
        10000);
}

// ---- Joint standards ---------------------------------------------------------------------

void MainWindow::refreshStandardRows()
{
    m_standardSockets.clear();
    m_standardHinges.clear();
    m_standardPorts.clear();
    const auto standard = standardsfile::active();
    QStringList sockets, hinges, ports;
    if (standard)
        for (const auto& j : standard->joints) {
            const QString label = QStringLiteral("%1 (%2)").arg(QString::fromStdString(j.name),
                                                               QString::fromStdString(standards::describe(j)));
            if (j.socket) {
                m_standardSockets.push_back(j);
                sockets << label;
                if (j.socket->kind != SocketKind::Magnet) {
                    m_standardPorts.push_back(j);
                    ports << label;
                }
            } else if (j.hinge) {
                m_standardHinges.push_back(j);
                hinges << label;
            }
        }
    const QString name = standard ? QString::fromStdString(standard->name) : QString();
    m_socketPanel->standardRow()->setJoints(name, sockets);
    m_hingePanel->standardRow()->setJoints(name, hinges);
    m_portPanel->standardRow()->setJoints(name, ports);
    m_handPanel->standardRow()->setJoints(name, ports);
}

void MainWindow::saveToStandard(bool hinge, const SocketFinish* socket)
{
    std::vector<standards::Standard> list;
    try {
        list = standardsfile::load();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    // The standard in use, or a new one if there's none yet.
    const QString active = standardsfile::activeName();
    auto it = std::find_if(list.begin(), list.end(),
                           [&](const standards::Standard& s) { return QString::fromStdString(s.name) == active; });
    QString standardName = active;
    if (it == list.end()) {
        bool ok = false;
        standardName = QInputDialog::getText(this, tr("New Joint Standard"),
                                             tr("There's no joint standard in use yet. Name one (a figure line, a "
                                                "scale...):"),
                                             QLineEdit::Normal,
                                             QString::fromStdString(standards::uniqueName(list, "My figure line")), &ok)
                           .trimmed();
        if (!ok || standardName.isEmpty())
            return;
        standards::Standard s;
        s.name = standards::uniqueName(list, standardName.toStdString());
        standardName = QString::fromStdString(s.name);
        list.push_back(s);
        it = list.end() - 1;
    }
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Save to Joint Standard"),
                              tr("A name for this joint in '%1' (Shoulder ball, Wrist peg, Knee hinge...):")
                                  .arg(standardName),
                              QLineEdit::Normal, QString(), &ok)
            .trimmed();
    if (!ok || name.isEmpty())
        return;
    const standards::Joint joint = hinge    ? standards::jointFrom(name.toStdString(), m_hingePanel->finish())
                                   : socket ? standards::jointFrom(name.toStdString(), *socket)
                                            : standards::jointFrom(name.toStdString(), m_socketPanel->finish());
    try {
        standards::check(joint);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    auto& joints = it->joints;
    const auto same = std::find_if(joints.begin(), joints.end(),
                                   [&](const standards::Joint& j) { return j.name == joint.name; });
    if (same != joints.end()) {
        if (QMessageBox::question(this, tr("Save to Joint Standard"),
                                  tr("'%1' already has a joint called '%2'. Replace it with these sizes?")
                                      .arg(standardName, name)) != QMessageBox::Yes)
            return;
        *same = joint;
    } else {
        joints.push_back(joint);
    }
    try {
        standardsfile::save(list);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    standardsfile::setActiveName(standardName);
    refreshStandardRows();
    if (m_standardsDialog)
        m_standardsDialog->reload();
    statusBar()->showMessage(tr("Saved '%1' to the joint standard '%2'.").arg(name, standardName), 6000);
}

// ---- Paint ------------------------------------------------------------------------------

void MainWindow::startPaint(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    // Painting a shape whose last step is paint goes on with that step.
    if (!editing && body->isBoolean() && body->boolean->op == BooleanOp::Paint && !body->isOff())
        editing = true;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    PaintMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    mode.object = history::objectOf(m_doc, target);
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (editing && (!body->isBoolean() || body->boolean->op != BooleanOp::Paint))
            return;
        mode.part = editing ? body->boolean->operands.front() : target;
        mode.toPlate = m_doc.worldTransform(mode.part);
        // Paint from the steps before this one shows (and stays) as it is.
        for (ObjectId step : history::steps(m_doc, mode.object)) {
            if (editing && step == target)
                break;
            const Body* b = m_doc.find(step);
            if (b && b->isBoolean() && !b->isOff() && b->boolean->op == BooleanOp::Paint && b->boolean->paint) {
                const auto dabs = paint::moved(b->boolean->paint->dabs, m_doc.worldTransform(step));
                mode.below.insert(mode.below.end(), dabs.begin(), dabs.end());
            }
            if (step == target)
                break;
        }
        if (editing) {
            mode.dabs = paintInPlate(m_doc, target);
            if (!mode.dabs.empty())
                mode.strokeStarts.push_back(0);  // what was painted before, as one stroke
        }
        const auto surface = m_doc.mesh(mode.part);
        if (!surface || surface->empty())
            throw std::runtime_error("The shape has no surface to paint on.");
        // The surface as the step will leave it (a shape with few triangles divided
        // finer), each triangle with its own corners so it shows one colour.
        const Mesh fine = paint::paintSurface(*surface);
        auto shown = std::make_shared<Mesh>();
        const std::size_t triangles = fine.triangleCount();
        shown->positions.resize(triangles * 9);
        shown->normals.resize(triangles * 9);
        shown->indices.resize(triangles * 3);
        mode.middles.resize(triangles * 3);
        for (std::size_t t = 0; t < triangles; ++t) {
            Vec3 p[3];
            for (int k = 0; k < 3; ++k) {
                const std::size_t v = fine.indices[3 * t + std::size_t(k)];
                p[k] = {fine.positions[3 * v], fine.positions[3 * v + 1], fine.positions[3 * v + 2]};
            }
            const Vec3 u{p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
            const Vec3 w{p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
            Vec3 n{u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
            const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 0)
                n = {n[0] / len, n[1] / len, n[2] / len};
            for (int k = 0; k < 3; ++k) {
                for (std::size_t a = 0; a < 3; ++a) {
                    shown->positions[9 * t + 3 * std::size_t(k) + a] = float(p[k][a]);
                    shown->normals[9 * t + 3 * std::size_t(k) + a] = float(n[a]);
                }
                shown->indices[3 * t + std::size_t(k)] = std::uint32_t(3 * t + std::size_t(k));
            }
            for (std::size_t a = 0; a < 3; ++a)
                mode.middles[3 * t + a] = float((p[0][a] + p[1][a] + p[2][a]) / 3.0);
        }
        shown->paint.assign(shown->vertexCount() * 4, 0.0f);
        mode.shown = shown;
        const Bounds b = shown->bounds();
        mode.mirrorX = 0.5 * (b.min[0] + b.max[0]);
        // The triangles' middles filed in a grid, to find those under a dab quickly.
        mode.cell = std::max(0.25, double(b.radius()) / 60.0);
        for (std::uint32_t t = 0; t < triangles; ++t) {
            const float* m = &mode.middles[3 * std::size_t(t)];
            mode.grid[maskCellKey(std::int64_t(std::floor(m[0] / mode.cell)), std::int64_t(std::floor(m[1] / mode.cell)),
                                  std::int64_t(std::floor(m[2] / mode.cell)))]
                .push_back(t);
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_paintMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_paintPanel->start(editing ? tr("Change the paint on '%1'").arg(name) : tr("Paint '%1'").arg(name), editing);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_paintPanel->show();
    statusBar()->showMessage(tr("Paint: drag over the shape to paint it (Ctrl rubs out), then Apply. Esc cancels."));
    repaintPaint();
    refreshScene();
    refreshCommandState();
}

std::optional<emboss::SurfacePoint> MainWindow::paintHit(const QVector3D& origin, const QVector3D& direction) const
{
    const auto& mode = m_paintMode;
    if (!mode.shown)
        return std::nullopt;
    const Transform toLocal = mode.toPlate.inverse();
    const auto hit = emboss::surfaceAt(mode.shown->positions, mode.shown->indices,
                                       toLocal.apply({origin.x(), origin.y(), origin.z()}),
                                       toLocal.applyDirection({direction.x(), direction.y(), direction.z()}), 0.2);
    if (!hit)
        return std::nullopt;
    return emboss::SurfacePoint{mode.toPlate.apply(hit->point), mode.toPlate.applyDirection(hit->normal)};
}

void MainWindow::paintHover(const QVector3D& origin, const QVector3D& direction)
{
    const auto hit = paintHit(origin, direction);
    if (!hit) {
        m_viewport->setBrushRing(std::nullopt);
        return;
    }
    m_viewport->setBrushRing(Viewport::BrushRing{
        QVector3D(float(hit->point[0]), float(hit->point[1]), float(hit->point[2])),
        QVector3D(float(hit->normal[0]), float(hit->normal[1]), float(hit->normal[2])),
        float(m_paintPanel->brushSize())});
}

void MainWindow::paintAt(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_paintMode;
    const auto hit = paintHit(origin, direction);
    if (!hit)
        return;
    const double radius = m_paintPanel->brushSize();
    // Dabs about a quarter of the brush apart, filled in along the way when the mouse
    // jumps, so a stroke is even however fast it goes.
    const double spacing = radius * 0.25;
    if (mode.lastDab) {
        const Vec3 last = *mode.lastDab;
        const double d = std::hypot(hit->point[0] - last[0], hit->point[1] - last[1], hit->point[2] - last[2]);
        if (d < spacing)
            return;
        if (d < radius * 8.0) {
            const int steps = int(std::floor(d / spacing));
            for (int i = 1; i < steps; ++i) {
                const double t = double(i) / steps;
                paintDab({last[0] + (hit->point[0] - last[0]) * t, last[1] + (hit->point[1] - last[1]) * t,
                          last[2] + (hit->point[2] - last[2]) * t},
                         radius, mode.erasing);
            }
        }
    }
    mode.lastDab = hit->point;
    paintDab(hit->point, radius, mode.erasing);
}

void MainWindow::paintDab(const Vec3& centre, double radius, bool erase)
{
    auto& mode = m_paintMode;
    const std::uint32_t colour = std::uint32_t(m_paintPanel->colour().rgb() & 0xFFFFFFu);
    std::vector<Vec3> centres{centre};
    if (m_paintPanel->mirror()) {
        // The same on the other side, across the part's middle (its own X).
        Vec3 c = mode.toPlate.inverse().apply(centre);
        c[0] = 2 * mode.mirrorX - c[0];
        centres.push_back(mode.toPlate.apply(c));
    }
    std::vector<std::uint32_t> touched;
    const float rgb[3] = {float((colour >> 16) & 0xFF) / 255.f, float((colour >> 8) & 0xFF) / 255.f,
                          float(colour & 0xFF) / 255.f};
    auto& paintAttr = mode.shown->paint;
    for (const Vec3& at : centres) {
        mode.dabs.push_back({at, radius, colour, erase});
        const Vec3 c = mode.toPlate.inverse().apply(at);
        const auto lo = [&](int k) { return std::int64_t(std::floor((c[std::size_t(k)] - radius) / mode.cell)); };
        const auto hi = [&](int k) { return std::int64_t(std::floor((c[std::size_t(k)] + radius) / mode.cell)); };
        for (std::int64_t i = lo(0); i <= hi(0); ++i)
            for (std::int64_t j = lo(1); j <= hi(1); ++j)
                for (std::int64_t k = lo(2); k <= hi(2); ++k) {
                    const auto found = mode.grid.find(maskCellKey(i, j, k));
                    if (found == mode.grid.end())
                        continue;
                    for (std::uint32_t t : found->second) {
                        const float* m = &mode.middles[3 * std::size_t(t)];
                        const double dx = m[0] - c[0], dy = m[1] - c[1], dz = m[2] - c[2];
                        if (dx * dx + dy * dy + dz * dz > radius * radius)
                            continue;
                        for (std::size_t v = 3 * std::size_t(t); v < 3 * std::size_t(t) + 3; ++v) {
                            paintAttr[4 * v] = rgb[0];
                            paintAttr[4 * v + 1] = rgb[1];
                            paintAttr[4 * v + 2] = rgb[2];
                            paintAttr[4 * v + 3] = erase ? 0.0f : 1.0f;
                        }
                        touched.push_back(t);
                    }
                }
    }
    if (touched.empty())
        return;
    std::sort(touched.begin(), touched.end());
    Viewport::MeshChanges changes;
    for (std::size_t i = 0; i < touched.size();) {
        std::size_t j = i + 1;
        while (j < touched.size() && touched[j] <= touched[j - 1] + 1)
            ++j;
        changes.points.push_back({3 * touched[i], 3 * (touched[j - 1] + 1)});
        i = j;
    }
    m_viewport->meshChanged(mode.object, changes);
}

void MainWindow::repaintPaint()
{
    auto& mode = m_paintMode;
    if (!mode.active || !mode.shown)
        return;
    std::vector<PaintDab> all = mode.below;
    all.insert(all.end(), mode.dabs.begin(), mode.dabs.end());
    const auto colours = paint::triangleColours(mode.shown->positions, mode.shown->indices, paint::Field(all),
                                                mode.toPlate);
    paint::colourMesh(*mode.shown, colours);
    Viewport::MeshChanges everything;
    everything.everything = true;
    m_viewport->meshChanged(mode.object, everything);
    m_paintPanel->setStrokes(int(mode.strokeStarts.size()));
    m_paintPanel->setUsed(paint::colours(all));
}

void MainWindow::undoPaintStroke()
{
    auto& mode = m_paintMode;
    if (!mode.active || mode.strokeStarts.empty())
        return;
    mode.dabs.resize(mode.strokeStarts.back());
    mode.strokeStarts.pop_back();
    repaintPaint();
}

void MainWindow::applyPaint()
{
    auto& mode = m_paintMode;
    if (!mode.active)
        return;
    if (mode.dabs.empty() && !mode.editing) {
        m_paintPanel->setStatus(tr("Paint something first: drag over the shape."), true);
        return;
    }
    const ObjectId target = mode.target;
    const std::vector<PaintDab> dabs = mode.dabs;
    std::optional<Body> result;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (mode.editing) {
            result = withPaint(m_doc, target, dabs);
        } else {
            Body r;
            r.id = m_doc.allocateId();
            r.name = m_doc.uniqueName("Painted");
            r.boolean = paintSpec(m_doc, target, dabs);
            m_doc.previewMesh(*r.boolean);  // worked out (and kept) before anything changes
            result = std::move(r);
        }
    } catch (const std::exception& e) {
        m_paintPanel->setStatus(tr("It couldn't be painted: %1").arg(QString::fromUtf8(e.what())), true);
        return;
    }
    if (mode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endPaint();
        const Body before = *now;
        endPaint();
        if (before != *result)
            run(std::make_unique<EditBodyCommand>(before, *result, "Change paint"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    const ObjectId id = result->id;
    endPaint();
    try {
        if (!m_doc.isTopLevel(target))
            m_stack.execute(std::make_unique<InsertStepCommand>(std::move(*result), m_doc.parentOf(target)));
        else
            m_stack.execute(std::make_unique<CombineCommand>(std::move(*result)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("It couldn't be painted:\n%1").arg(QString::fromUtf8(e.what())));
        refreshScene();
        return;
    }
    refreshScene();
    refreshCommandState();
    setSelection(m_doc.isTopLevel(id) ? id : history::objectOf(m_doc, target));
    statusBar()->showMessage(tr("Painted. Export as 3MF for a colour printer; Split by Mask can cut a colour off "
                                "as its own piece."),
                             10000);
}

void MainWindow::endPaint()
{
    if (!m_paintMode.active)
        return;
    m_paintMode = PaintMode{};
    m_viewport->setBrushRing(std::nullopt);
    m_paintPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

std::shared_ptr<const Mesh> MainWindow::paintedMesh(ObjectId id, const std::shared_ptr<const Mesh>& mesh)
{
    std::vector<PaintDab> dabs;
    try {
        dabs = paint::objectPaint(m_doc, id);
    } catch (const std::exception&) {
        return mesh;
    }
    if (dabs.empty() || !mesh) {
        m_paintedShown.erase(id);
        return mesh;
    }
    auto& cached = m_paintedShown[id];
    if (cached.source == mesh && cached.dabs == dabs && cached.painted)
        return cached.painted;
    auto painted = std::make_shared<Mesh>(*mesh);
    paint::colourMesh(*painted,
                      paint::triangleColours(painted->positions, painted->indices, paint::Field(dabs)));
    cached = {mesh, std::move(dabs), painted};
    return painted;
}

// ---- Reference Images ----------------------------------------------------------------

void MainWindow::startReferences(std::optional<RefView> view)
{
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    m_referenceMode = ReferenceMode{};
    m_referenceMode.active = true;
    const auto& images = m_doc.references();
    if (view)
        m_referencePanel->setDirection(*view);
    else if (!reference::find(images, m_referencePanel->direction()) && !images.empty())
        m_referencePanel->setDirection(images.front().view);
    m_referencePanel->setPicking(0);
    m_referencePanel->setStatus({}, false);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_referencePanel->show();
    if (!m_showPicturesAction->isChecked())
        m_showPicturesAction->setChecked(true);  // you'd want to see what you're changing
    statusBar()->showMessage(tr("Reference Images: add a picture for a side, drag it into place, size it from two "
                                "points. Done or Esc closes the panel; the pictures stay."));
    refreshReferences();
    refreshCommandState();
}

void MainWindow::endReferences()
{
    if (!m_referenceMode.active)
        return;
    m_referenceMode = ReferenceMode{};
    m_referencePanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    showLabels();  // any kept measurements, instead of the picking marks
    refreshReferences();
    refreshCommandState();
}

void MainWindow::refreshReferences()
{
    const auto& images = m_doc.references();
    std::vector<Viewport::Picture> pictures;
    if (m_showPicturesAction && m_showPicturesAction->isChecked())
        for (const ReferenceImage& r : images) {
            if (!r.shown)
                continue;
            Viewport::Picture p;
            p.file = r.data;
            const auto c = reference::corners(r);
            for (int i = 0; i < 4; ++i)
                p.corners[std::size_t(i)] = QVector3D(float(c[std::size_t(i)][0]), float(c[std::size_t(i)][1]),
                                                      float(c[std::size_t(i)][2]));
            const auto f = reference::frameOf(r.view);
            p.toward = QVector3D(float(f.toward[0]), float(f.toward[1]), float(f.toward[2]));
            p.opacity = float(r.opacity);
            p.chosen = m_referenceMode.active && r.view == m_referencePanel->direction();
            pictures.push_back(std::move(p));
        }
    m_viewport->setPictures(std::move(pictures));
    if (m_referenceMode.active) {
        std::vector<RefView> taken;
        for (const ReferenceImage& r : images)
            taken.push_back(r.view);
        m_referencePanel->showPicture(reference::find(images, m_referencePanel->direction()), taken);
    }
}

void MainWindow::referenceEdited(const QString& what, bool dragging)
{
    auto images = m_doc.references();
    const auto it = std::find_if(images.begin(), images.end(),
                                 [this](const ReferenceImage& r) { return r.view == m_referencePanel->direction(); });
    if (it == images.end())
        return;
    ReferenceImage& r = *it;
    const double width = m_referencePanel->widthMm();
    if (std::abs(width - r.widthMm()) > 1e-9 && r.pixelsWide > 0)
        r.mmPerPixel = width / r.pixelsWide;
    r.across = m_referencePanel->across();
    r.up = m_referencePanel->up();
    r.depth = m_referencePanel->behind();
    r.turn = m_referencePanel->turn();
    r.opacity = m_referencePanel->opacity();
    r.shown = m_referencePanel->shown();
    r.active = m_referencePanel->active();
    try {
        m_stack.execute(std::make_unique<SetReferencesCommand>(std::move(images), what.toStdString(), dragging));
    } catch (const std::exception& e) {
        m_referencePanel->setStatus(QString::fromUtf8(e.what()), true);
    }
}

void MainWindow::addReferencePicture()
{
    const RefView view = m_referencePanel->direction();
    QStringList patterns;
    for (const QByteArray& format : QImageReader::supportedImageFormats())
        patterns << QStringLiteral("*.%1").arg(QString::fromLatin1(format).toLower());
    Preferences prefs;
    QString folder = prefs.value(QStringLiteral("reference/folder")).toString();
    if (folder.isEmpty() || !QDir(folder).exists())
        folder = startFolder();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose a picture for the %1 side").arg(tr(reference::title(view))), folder,
        tr("Pictures (%1);;All files (*)").arg(patterns.join(QLatin1Char(' '))));
    if (path.isEmpty())
        return;
    prefs.setValue(QStringLiteral("reference/folder"), QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Forge3D"), tr("Could not read %1.").arg(QFileInfo(path).fileName()));
        return;
    }
    constexpr qint64 kMostBytes = 64 * 1024 * 1024;
    if (file.size() > kMostBytes) {
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("%1 is over 64 MB. Save it smaller (a few thousand pixels across is plenty) and "
                                "try again.").arg(QFileInfo(path).fileName()));
        return;
    }
    QByteArray bytes = file.readAll();
    const QImage image = Viewport::readPicture(bytes);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Forge3D"),
                             tr("%1 isn't a picture Forge3D can read.").arg(QFileInfo(path).fileName()));
        return;
    }

    ReferenceImage r;
    r.view = view;
    r.name = QFileInfo(path).fileName().toStdString();
    r.suffix = QFileInfo(path).suffix().toLower().toStdString();
    r.data = std::make_shared<const std::string>(bytes.constData(), std::size_t(bytes.size()));
    r.pixelsWide = image.width();
    r.pixelsHigh = image.height();
    auto images = m_doc.references();
    const auto old = std::find_if(images.begin(), images.end(), [view](const ReferenceImage& i) { return i.view == view; });
    const bool replacing = old != images.end();
    if (replacing) {
        // A new picture for the same side: just as wide, in the same place.
        r.mmPerPixel = old->widthMm() / r.pixelsWide;
        r.across = old->across;
        r.up = old->up;
        r.depth = old->depth;
        r.turn = old->turn;
        r.opacity = old->opacity;
        r.shown = old->shown;
        r.active = old->active;
        *old = r;
    } else {
        // A bit bigger than the model (at least 100 mm; 150 on an empty plate), standing behind it.
        const Bounds box = m_doc.worldBounds();
        double fit = 150.0, behind = 10.0;
        if (box.valid) {
            const auto size = box.size();
            fit = std::max(100.0, 1.2 * std::max({size[0], size[1], size[2]}));
            const auto f = reference::frameOf(view);
            double far = 0.0;  // the farthest the model reaches away from the viewer
            for (int i = 0; i < 8; ++i) {
                const Vec3 corner{i & 1 ? box.max[0] : box.min[0], i & 2 ? box.max[1] : box.min[1],
                                  i & 4 ? box.max[2] : box.min[2]};
                far = std::max(far, -(corner[0] * f.toward[0] + corner[1] * f.toward[1] + corner[2] * f.toward[2]));
            }
            behind = far + 10.0;
        }
        images.push_back(reference::placed(r, fit, behind));
    }
    try {
        m_stack.execute(std::make_unique<SetReferencesCommand>(
            std::move(images), replacing ? "Replace Picture" : "Add Picture"));
    } catch (const std::exception& e) {
        m_referencePanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    m_referencePanel->setStatus(tr("Added. Drag it into place, or size it from two points."), false);
    const auto f = reference::frameOf(view);
    m_viewport->turnTo(QVector3D(float(f.toward[0]), float(f.toward[1]), float(f.toward[2])));
}

void MainWindow::removeReferencePicture()
{
    auto images = m_doc.references();
    const RefView view = m_referencePanel->direction();
    const auto before = images.size();
    std::erase_if(images, [view](const ReferenceImage& r) { return r.view == view; });
    if (images.size() == before)
        return;
    m_stack.execute(std::make_unique<SetReferencesCommand>(std::move(images), "Remove Picture"));
    m_referencePanel->setStatus(tr("Removed (Undo brings it back)."), false);
}

void MainWindow::referenceClick(const QVector3D& origin, const QVector3D& direction)
{
    const Vec3 o{origin.x(), origin.y(), origin.z()}, d{direction.x(), direction.y(), direction.z()};
    auto& mode = m_referenceMode;
    if (mode.picking) {
        // Sizing: points on the chosen picture (even an inactive one: you asked to click on it).
        const ReferenceImage* image = reference::find(m_doc.references(), m_referencePanel->direction());
        const auto h = image ? reference::hit(*image, o, d) : std::nullopt;
        if (!h) {
            m_referencePanel->setStatus(tr("Click on the picture itself (looking at it from its own side)."), true);
            return;
        }
        if (mode.picking == 1) {
            mode.first = {h->px, h->py};
            mode.firstAt = reference::pointAt(*image, h->px, h->py);
            mode.picking = 2;
            m_referencePanel->setPicking(2);
            const QVector3D at(float(mode.firstAt[0]), float(mode.firstAt[1]), float(mode.firstAt[2]));
            m_viewport->setLabels({{at, at, tr("1")}});
            return;
        }
        try {
            const double mm = m_referencePanel->calibrationLength();
            auto images = m_doc.references();
            for (ReferenceImage& r : images)
                if (r.view == image->view)
                    r = reference::calibrated(r, mode.first, {h->px, h->py}, mm);
            m_stack.execute(std::make_unique<SetReferencesCommand>(std::move(images), "Size Picture"));
            m_referencePanel->setStatus(tr("Sized: those points are now %1 mm apart.").arg(mm, 0, 'f', 2), false);
        } catch (const std::exception& e) {
            m_referencePanel->setStatus(QString::fromUtf8(e.what()), true);
        }
        mode.picking = 0;
        m_referencePanel->setPicking(0);
        m_viewport->setLabels({});
        return;
    }
    // A click on an active picture chooses it.
    if (!m_showPicturesAction->isChecked())
        return;
    const auto& images = m_doc.references();
    if (const auto i = reference::pick(images, o, d); i && images[*i].view != m_referencePanel->direction())
        m_referencePanel->setDirection(images[*i].view);
}

bool MainWindow::grabReference(const QVector3D& origin, const QVector3D& direction)
{
    if (!m_showPicturesAction->isChecked())
        return false;
    const auto& images = m_doc.references();
    reference::Hit where;
    const auto i = reference::pick(images, {origin.x(), origin.y(), origin.z()},
                                   {direction.x(), direction.y(), direction.z()}, &where);
    if (!i)
        return false;
    const ReferenceImage& r = images[*i];
    if (r.view != m_referencePanel->direction())
        m_referencePanel->setDirection(r.view);
    m_referenceMode.dragging = true;
    m_referenceMode.dragged = r.view;
    m_referenceMode.grabbed = reference::pointAt(r, where.px, where.py);
    m_referenceMode.startAcross = r.across;
    m_referenceMode.startUp = r.up;
    return true;
}

void MainWindow::dragReference(const QVector3D& origin, const QVector3D& direction)
{
    auto images = m_doc.references();
    const auto it = std::find_if(images.begin(), images.end(),
                                 [this](const ReferenceImage& r) { return r.view == m_referenceMode.dragged; });
    if (it == images.end())
        return;
    const auto f = reference::frameOf(it->view);
    const Vec3& g = m_referenceMode.grabbed;
    const QVector3D normal(float(f.toward[0]), float(f.toward[1]), float(f.toward[2]));
    const auto at = view::intersectPlane(origin, direction, QVector3D(float(g[0]), float(g[1]), float(g[2])), normal);
    if (!at)
        return;
    const Vec3 shift{at->x() - g[0], at->y() - g[1], at->z() - g[2]};
    it->across = m_referenceMode.startAcross + shift[0] * f.right[0] + shift[1] * f.right[1] + shift[2] * f.right[2];
    it->up = m_referenceMode.startUp + shift[0] * f.up[0] + shift[1] * f.up[1] + shift[2] * f.up[2];
    try {
        m_stack.execute(std::make_unique<SetReferencesCommand>(std::move(images), "Move Picture", true));
    } catch (const std::exception& e) {
        qWarning() << "Picture move refused:" << e.what();
    }
}

bool MainWindow::referenceClickedOutside(const QVector3D& origin, const QVector3D& direction)
{
    if (!m_showPicturesAction->isChecked() || toolOpen())
        return false;
    const auto& images = m_doc.references();
    const auto i = reference::pick(images, {origin.x(), origin.y(), origin.z()},
                                   {direction.x(), direction.y(), direction.z()});
    if (!i)
        return false;
    setSelection(kInvalidId);
    startReferences(images[*i].view);
    statusBar()->showMessage(tr("You clicked the %1 picture. Untick Active in the panel to click straight through "
                                "it.").arg(tr(reference::title(images[*i].view))),
                             10000);
    return true;
}

// ---- Join Parts -------------------------------------------------------------------

void MainWindow::startJoin()
{
    if (toolOpen() || m_doc.bodyCount() < 2)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    m_joinMode = JoinMode{};
    m_joinMode.active = true;
    m_joinPanel->start();
    m_joinPanel->setKeepTogether(Preferences().value(QStringLiteral("join/keepTogether"), false).toBool());
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_joinPanel->show();
    statusBar()->showMessage(tr("Join Parts: click the part that moves, where it joins (its peg, ball or face). Esc "
                                "cancels."));
    refreshScene();
    refreshCommandState();
}

void MainWindow::joinClick(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_joinMode;
    if (mode.picking > 1)
        mode.picking = 1;  // both picked: another click changes where it goes
    const QVector3D d = direction.normalized();
    Ray ray;
    ray.origin = {origin.x(), origin.y(), origin.z()};
    ray.direction = {d.x(), d.y(), d.z()};
    const auto hit = pick(m_doc, ray);
    if (!hit) {
        statusBar()->showMessage(tr("Join Parts: click on a part."), 4000);
        return;
    }
    const ObjectId id = history::objectOf(m_doc, hit->id);
    const int which = mode.picking;
    const ObjectId other = which == 0 ? mode.fixed : mode.moving;
    if (id == other) {
        m_joinPanel->setStatus(which == 0 ? tr("That's the part it goes on: click the part that moves.")
                                          : tr("That's the part that moves: click the other part."),
                               true);
        return;
    }
    std::optional<join::Feature> f;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        f = join::featureAt(m_doc, id, ray.origin, ray.direction);
    } catch (const std::exception& e) {
        m_joinPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    if (!f)
        return;
    const QString text = QStringLiteral("<b>%1</b>: %2")
                             .arg(QString::fromStdString(m_doc.find(id)->name).toHtmlEscaped(),
                                  QString::fromStdString(f->text).toHtmlEscaped());
    if (which == 0) {
        mode.moving = id;
        mode.movingFeature = f;
        m_joinPanel->setProperty("movingText", text);
        mode.picking = mode.fixedFeature ? 2 : 1;
    } else {
        mode.fixed = id;
        mode.fixedFeature = f;
        m_joinPanel->setProperty("fixedText", text);
        mode.picking = 2;
        // Start from the gap it suggests (a peg socket's fit), the rest as they were.
        join::Offsets o = m_joinPanel->offsets();
        o.along = f->gap;
        m_joinPanel->setOffsets(o);
    }
    m_joinPanel->setPicked(0, mode.movingFeature ? m_joinPanel->property("movingText").toString() : QString(),
                           mode.picking == 0);
    m_joinPanel->setPicked(1, mode.fixedFeature ? m_joinPanel->property("fixedText").toString() : QString(),
                           mode.picking != 0);
    statusBar()->showMessage(mode.picking == 1 ? tr("Join Parts: now click where it goes on the other part.")
                                               : tr("Join Parts: set the gap or turn if needed, then Apply. "
                                                    "Clicking again changes where it goes."));
    updateJoinPreview();
}

void MainWindow::updateJoinPreview()
{
    auto& mode = m_joinMode;
    if (!mode.active)
        return;
    mode.placed.reset();
    bool ok = false;
    if (mode.movingFeature && mode.fixedFeature) {
        try {
            mode.placed = join::joinedPlacement(m_doc, mode.moving, *mode.movingFeature, *mode.fixedFeature,
                                                m_joinPanel->offsets());
            m_joinPanel->setStatus(tr("This is where it goes. If it points the wrong way, tick Flip."), false);
            ok = true;
        } catch (const std::exception& e) {
            m_joinPanel->setStatus(QString::fromUtf8(e.what()), true);
        }
    } else {
        m_joinPanel->setStatus({}, false);
    }
    m_joinPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyJoin()
{
    auto& mode = m_joinMode;
    if (!mode.active || !mode.placed)
        return;
    const ObjectId moving = mode.moving, fixed = mode.fixed;
    const Placement placed = *mode.placed;
    const bool together = m_joinPanel->keepTogether();
    Preferences().setValue(QStringLiteral("join/keepTogether"), together);
    endJoin();
    ObjectId selected = moving;
    try {
        selected = join::apply(m_stack, m_doc, moving, fixed, placed, together);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("The part could not be moved:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
    setSelection(selected);
}

void MainWindow::endJoin()
{
    if (!m_joinMode.active)
        return;
    m_joinMode = JoinMode{};
    m_joinPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Follow -----------------------------------------------------------------------

void MainWindow::startFollow(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    FollowMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        mode.shape = editing ? pickableShape(m_doc, target) : inPlate(m_doc, target);
        mode.finder = std::make_shared<edges::FaceFinder>(mode.shape);
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Follow)
                return;
            mode.finish = followInPlate(m_doc, target);
            mode.placed = true;
        } else {
            // A groove about a twentieth of the shape's smallest side.
            auto sides = m_doc.worldBounds(target).size();
            std::sort(sides.begin(), sides.end());
            mode.finish.width = std::clamp(std::round(double(sides[0]) * 0.1 * 2.0) / 2.0, 0.5, 5.0);
            mode.finish.depth = mode.finish.width / 2.0;
            mode.finish.profile = FollowProfile::Round;
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_followMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_followPanel->start(editing ? tr("Change the groove / ridge in '%1'").arg(name)
                                 : tr("Groove or ridge on '%1'").arg(name),
                         m_followMode.finish, editing);
    m_followPanel->setPlaced(m_followMode.placed);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_followPanel->show();
    statusBar()->showMessage(editing ? tr("Follow: change the settings or the path, then Apply. Clicking the shape "
                                          "moves the path there.")
                                     : tr("Follow: click the spot on the shape where the path goes. Esc cancels."));
    updateFollowPreview();
    refreshCommandState();
}

void MainWindow::followClick(const QVector3D& origin, const QVector3D& direction)
{
    if (m_followMode.sketching != 0)
        return;
    const QVector3D d = direction.normalized();
    const auto hit = m_followMode.finder->under({origin.x(), origin.y(), origin.z()}, {d.x(), d.y(), d.z()});
    if (!hit) {
        statusBar()->showMessage(tr("Follow: click on the shape itself."), 4000);
        return;
    }
    // The spot, facing out of the surface there; X level (or along the plate's X on a
    // top or bottom), the way Emboss lays words.
    const FaceRef ref = shell::refFor(m_followMode.shape, hit->face, hit->point);
    const Vec3 n = ref.normal, up = emboss::upFor(n);
    Vec3 x{up[1] * n[2] - up[2] * n[1], up[2] * n[0] - up[0] * n[2], up[0] * n[1] - up[1] * n[0]};
    m_followMode.finish.origin = ref.point;
    m_followMode.finish.normal = n;
    m_followMode.finish.xAxis = x;
    const bool first = !m_followMode.placed;
    m_followMode.placed = true;
    m_followPanel->setPlaced(true);
    if (first && m_followMode.finish.path.geos.empty()) {
        openFollowSketch(false);  // straight on to drawing the path
        return;
    }
    m_followTimer->start(0);
}

void MainWindow::openFollowSketch(bool profile)
{
    if (!m_followMode.active || !m_followMode.placed)
        return;
    const FollowFinish& f = m_followMode.finish;
    std::vector<std::vector<QPointF>> reference;
    if (profile) {
        // The surface, seen end-on: a line through the dot.
        const double w = std::max(f.width, 1.0) * 2.0;
        reference.push_back({QPointF(-w, 0), QPointF(w, 0)});
    } else {
        try {
            const Transform frame = follow::frameOf(f);
            for (const auto& line : sketch::outlinesOnPlane(m_followMode.shape, frame)) {
                std::vector<QPointF> q;
                for (const auto& p : line)
                    q.emplace_back(p.x, p.y);
                reference.push_back(std::move(q));
            }
        } catch (const std::exception& e) {
            qWarning() << "Finding the face's outline failed:" << e.what();
        }
    }
    m_followMode.sketching = profile ? 2 : 1;
    m_followTimer->stop();
    m_sketchMode = SketchMode{};
    m_sketchMode.frame = follow::frameOf(f);
    openSketch(profile ? f.custom : f.path, std::move(reference),
               profile ? tr("Draw the shape across the path") : tr("Draw the path"), false, SketchPanel::Options{});
}

FollowFinish MainWindow::currentFollow() const
{
    FollowFinish f = m_followMode.finish;
    f.ridge = m_followPanel->ridge();
    f.profile = m_followPanel->profile();
    f.width = m_followPanel->width();
    f.depth = m_followPanel->depth();
    return f;
}

void MainWindow::updateFollowPreview()
{
    if (!m_followMode.active || m_followMode.sketching != 0)
        return;
    m_followTimer->stop();
    m_followMode.preview.reset();
    m_followMode.pathLines.reset();
    bool ok = false;
    const FollowFinish f = currentFollow();
    if (!m_followMode.placed) {
        m_followPanel->setStatus(tr("Click the spot on the shape where the path goes."), false);
    } else if (f.path.geos.empty()) {
        m_followPanel->setStatus(tr("Draw the path (Draw the path...)."), false);
    } else {
        try {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            // The path as it lies on the surface, shown as a line.
            const auto laid = followPathInPlate(m_doc, m_followMode.target, f, m_followMode.editing);
            std::vector<const std::vector<Vec3>*> lines;
            for (const auto& l : laid)
                lines.push_back(&l);
            const float radius = std::max(0.02f, m_viewport->camera().distance() * 0.0018f);
            m_followMode.pathLines = tubes(lines, radius);
            const BooleanSpec spec = m_followMode.editing
                                         ? *withFollow(m_doc, m_followMode.target, f).boolean
                                         : followSpec(m_doc, m_followMode.target, f);
            m_followMode.preview = m_doc.previewMesh(spec);
            m_followPanel->setStatus(tr("This is how it will look. Click the shape to move the path."), false);
            ok = true;
        } catch (const std::exception& e) {
            m_followPanel->setStatus(QString::fromUtf8(e.what()), true);
        }
    }
    m_followPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyFollow()
{
    if (!m_followMode.active)
        return;
    const FollowFinish f = currentFollow();
    const ObjectId target = m_followMode.target;
    const bool editing = m_followMode.editing;
    std::optional<BooleanSpec> spec;
    try {
        spec = editing ? *withFollow(m_doc, target, f).boolean : followSpec(m_doc, target, f);
        m_doc.previewMesh(*spec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("That did not work:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    endFollow();
    if (editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return;
        Body after = *now;
        after.boolean = *spec;
        run(std::make_unique<EditBodyCommand>(*now, after, f.ridge ? "Change ridge" : "Change groove"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName(f.ridge ? "Ridged" : "Grooved");
    result.boolean = *spec;
    const ObjectId id = result.id;
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::endFollow()
{
    if (!m_followMode.active)
        return;
    if (m_followMode.sketching != 0) {
        m_followMode.sketching = 0;
        endSketch();
    }
    m_followTimer->stop();
    m_followMode = FollowMode{};
    m_followPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Pattern ----------------------------------------------------------------------

void MainWindow::startPattern(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body)
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    PatternMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    PatternFinish finish;
    std::optional<QString> feature;
    try {
        if (editing) {
            if (!body->isBoolean() || body->boolean->op != BooleanOp::Pattern)
                return;
            finish = patternInPlate(m_doc, target);
            if (const auto name = pattern::featureName(m_doc, body->boolean->operands.front()))
                feature = QString::fromStdString(*name);
        } else {
            if (const auto name = pattern::featureName(m_doc, target))
                feature = QString::fromStdString(*name);
            // A shape whose last step drilled or cut something: repeat that, to begin with.
            finish = startingPattern(m_doc, target, feature.has_value());
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_patternMode = mode;
    const QString name = QString::fromStdString(m_doc.find(history::objectOf(m_doc, target))->name);
    m_patternPanel->start(editing ? tr("Change the pattern of '%1'").arg(name) : tr("Pattern '%1'").arg(name),
                          feature, finish, editing);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_patternPanel->show();
    statusBar()->showMessage(tr("Pattern: set the copies, then Apply. Esc cancels."));
    updatePatternPreview();
    refreshCommandState();
}

void MainWindow::updatePatternPreview()
{
    if (!m_patternMode.active)
        return;
    m_patternTimer->stop();
    m_patternMode.preview.reset();
    bool ok = false;
    const PatternFinish f = m_patternPanel->finish();
    if (f.kind == PatternKind::Grid && f.dir1 == f.dir2) {
        m_patternPanel->setStatus(tr("Pick two different directions for the grid."), true);
    } else {
        try {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            struct RestoreCursor {
                ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
            } restore;
            const BooleanSpec spec = m_patternMode.editing
                                         ? *withPattern(m_doc, m_patternMode.target, f).boolean
                                         : patternSpec(m_doc, m_patternMode.target, f);
            m_patternMode.preview = m_doc.previewMesh(spec);
            const int n = pattern::copies(f);
            m_patternPanel->setStatus(n == 1 ? tr("Just the original: add more copies.")
                                             : tr("%1 in all. This is how it will look.").arg(n),
                                      false);
            ok = n > 1;
        } catch (const std::exception& e) {
            m_patternPanel->setStatus(QString::fromUtf8(e.what()), true);
        }
    }
    m_patternPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyPattern()
{
    if (!m_patternMode.active)
        return;
    const PatternFinish f = m_patternPanel->finish();
    const ObjectId target = m_patternMode.target;
    const bool editing = m_patternMode.editing;
    std::optional<BooleanSpec> spec;
    try {
        spec = editing ? *withPattern(m_doc, target, f).boolean : patternSpec(m_doc, target, f);
        m_doc.previewMesh(*spec);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("That did not work:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    endPattern();
    if (editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return;
        Body after = *now;
        after.boolean = *spec;
        run(std::make_unique<EditBodyCommand>(*now, after, "Change pattern"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    Body result;
    result.id = m_doc.allocateId();
    result.name = m_doc.uniqueName("Pattern");
    result.boolean = *spec;
    const ObjectId id = result.id;
    if (!m_doc.isTopLevel(target)) {
        // Rolled back: the step goes in at the bar.
        const ObjectId object = history::objectOf(m_doc, target);
        if (run(std::make_unique<InsertStepCommand>(std::move(result), m_doc.parentOf(target))))
            setSelection(object);
        return;
    }
    if (run(std::make_unique<CombineCommand>(std::move(result))))
        setSelection(id);
}

void MainWindow::endPattern()
{
    if (!m_patternMode.active)
        return;
    m_patternTimer->stop();
    m_patternMode = PatternMode{};
    m_patternPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

// ---- Print Check ------------------------------------------------------------------

void MainWindow::startPrintCheck()
{
    if (toolOpen() || m_doc.topLevelIds().empty())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    m_printCheckMode = PrintCheckMode{};
    m_printCheckMode.active = true;
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_printCheckPanel->show();
    m_printCheckPanel->setStatus({}, false);
    updatePrintCheck();
    statusBar()->showMessage(tr("Print Check: red is too thin, blue needs supports. Done or Esc closes it."));
    refreshCommandState();
}

void MainWindow::endPrintCheck()
{
    if (!m_printCheckMode.active)
        return;
    m_printCheckMode = PrintCheckMode{};
    m_printCheckPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::updatePrintCheck()
{
    auto& mode = m_printCheckMode;
    if (!mode.active)
        return;
    const auto ids = m_doc.topLevelIds();
    std::erase_if(mode.parts, [&](const auto& p) { return std::find(ids.begin(), ids.end(), p.first) == ids.end(); });
    QPointer<MainWindow> self(this);
    for (ObjectId id : ids) {
        std::shared_ptr<const Mesh> mesh;
        try {
            mesh = m_doc.mesh(id);
        } catch (const std::exception&) {
            mode.parts.erase(id);  // a part that can't be built isn't checked
            continue;
        }
        auto& part = mode.parts[id];
        if (part.mesh == mesh && part.thick)
            continue;
        if (part.mesh == mesh)
            continue;  // already being measured
        part = PrintCheckMode::Part{};
        part.mesh = mesh;
        QThreadPool::globalInstance()->start([self, id, mesh] {
            std::shared_ptr<const std::vector<float>> thick;
            try {
                thick = std::make_shared<const std::vector<float>>(printcheck::faceThickness(*mesh));
            } catch (const std::exception& e) {
                qWarning() << "Measuring wall thickness failed:" << e.what();
                thick = std::make_shared<const std::vector<float>>();
            }
            QMetaObject::invokeMethod(
                qApp,
                [self, id, mesh, thick] {
                    if (!self || !self->m_printCheckMode.active)
                        return;
                    const auto p = self->m_printCheckMode.parts.find(id);
                    if (p == self->m_printCheckMode.parts.end() || p->second.mesh != mesh)
                        return;  // the part has changed since
                    p->second.thick = thick;
                    self->colourPrintCheck();
                },
                Qt::QueuedConnection);
        });
    }
    colourPrintCheck();
}

void MainWindow::colourPrintCheck()
{
    auto& mode = m_printCheckMode;
    if (!mode.active)
        return;
    const printcheck::Settings settings = m_printCheckPanel->settings();
    const auto area = [](double mm2) { return QString::number(mm2, 'f', mm2 < 10.0 ? 1 : 0); };
    QStringList lines;
    int measuring = 0;
    for (auto& [id, part] : mode.parts) {
        const Body* body = m_doc.find(id);
        if (!body || !part.mesh)
            continue;
        if (!part.thick)
            ++measuring;
        static const std::vector<float> none;
        auto shown = std::make_shared<Mesh>();
        part.report = printcheck::classify(*part.mesh, part.thick ? *part.thick : none, m_doc.worldTransform(id),
                                           settings, shown.get());
        part.shown = std::move(shown);
        const auto& r = part.report;
        QStringList found;
        if (settings.thin && r.thinArea > 0.0)
            found << tr("<span style=\"color:#d02018\">too thin</span> over %1 mm² (thinnest %2 mm)")
                         .arg(area(r.thinArea), QString::number(r.thinnest, 'f', 2));
        if (settings.overhangs && r.overhangArea > 0.0)
            found << tr("<span style=\"color:#2060e0\">needs supports</span> under %1 mm²").arg(area(r.overhangArea));
        if (!found.isEmpty())
            lines << tr("<b>%1</b>: %2").arg(QString::fromStdString(body->name).toHtmlEscaped(),
                                             found.join(QStringLiteral("; ")));
    }
    QString html;
    if (!settings.thin && !settings.overhangs)
        html = tr("Tick Thin walls or Overhangs to check for them.");
    else if (lines.isEmpty())
        html = measuring > 0 ? QString() : tr("Nothing found: every part looks ready to print this way up.");
    else
        html = lines.join(QStringLiteral("<br>"));
    if (measuring > 0 && settings.thin)
        html += (html.isEmpty() ? QString() : QStringLiteral("<br><br>")) +
                (measuring == 1 ? tr("<i>Measuring wall thickness (1 part to go)...</i>")
                                : tr("<i>Measuring wall thickness (%1 parts to go)...</i>").arg(measuring));
    m_printCheckPanel->setResult(html);
    refreshScene();
}

// ---- Rescale Design -----------------------------------------------------------------

void MainWindow::rescaleDesign()
{
    if (toolOpen()) {
        statusBar()->showMessage(tr("Close the open tool first."), 5000);
        return;
    }
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    const std::vector<ObjectId> all = m_doc.topLevelIds();
    if (all.empty()) {
        statusBar()->showMessage(tr("Rescale Design: there's nothing on the plate."), 5000);
        return;
    }
    std::vector<ObjectId> picked;
    for (ObjectId id : m_selection)
        if (m_doc.isTopLevel(id))
            picked.push_back(id);
    const auto boxOf = [this](const std::vector<ObjectId>& ids) {
        Bounds b;
        for (ObjectId id : ids) {
            try {
                const Bounds w = m_doc.worldBounds(id);
                if (w.valid) {
                    b.expand(w.min[0], w.min[1], w.min[2]);
                    b.expand(w.max[0], w.max[1], w.max[2]);
                }
            } catch (const std::exception&) {
            }
        }
        return b;
    };
    const auto choiceOf = [&](const std::vector<ObjectId>& ids) {
        RescaleDialog::Choice c;
        const Bounds b = boxOf(ids);
        c.height = b.valid ? double(b.max[2] - b.min[2]) : 0.0;
        c.joints = jointCount(m_doc, ids);
        c.shapes = int(ids.size());
        return c;
    };
    RescaleDialog dialog(choiceOf(all), choiceOf(picked), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const std::vector<ObjectId>& ids = dialog.selectedOnly() ? picked : all;
    const Bounds box = boxOf(ids);
    if (!box.valid)
        return;
    // About the middle of its footprint, at its bottom: what stands on the plate stays there.
    const auto c = box.center();
    const Vec3 anchor{c[0], c[1], box.min[2]};
    ScaleOptions options;
    options.keepJoints = dialog.keepJoints();
    options.keepWalls = dialog.keepWalls();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        std::vector<Body> after = rescaledDesign(m_doc, ids, dialog.factor(), anchor, options);
        std::vector<Body> before;
        before.reserve(after.size());
        for (const Body& b : after)
            before.push_back(*m_doc.find(b.id));
        const std::string label = "Rescale " + dialog.describe().toStdString();
        const bool done = run(std::make_unique<EditBodiesCommand>(std::move(before), std::move(after), label));
        QApplication::restoreOverrideCursor();
        if (done) {
            const Bounds now = boxOf(ids);
            statusBar()->showMessage(tr("Rescaled %1: now %2 mm tall.")
                                         .arg(dialog.describe(), QString::number(now.max[2] - now.min[2], 'f', 1)),
                                     8000);
        }
    } catch (const std::exception& e) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Rescale Design"),
                             tr("The design couldn't be rescaled:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

// ---- Figure guide ---------------------------------------------------------------------

void MainWindow::startMannequin()
{
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    m_mannequinOpen = true;
    m_mannequinPanel->setMannequin(m_doc.mannequin());
    m_mannequinPanel->setStatus({}, false);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_mannequinPanel->show();
    statusBar()->showMessage(tr("Figure Guide: set its size and proportions, then pose it. Done or Esc closes the "
                                "panel; the guide stays."));
    refreshCommandState();
}

void MainWindow::endMannequin()
{
    if (!m_mannequinOpen)
        return;
    m_mannequinOpen = false;
    m_mannequinPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshCommandState();
}

// ---- Make Hand ------------------------------------------------------------------------

void MainWindow::startHand()
{
    if (toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    HandMode mode;
    mode.active = true;
    // Beside the selected shape (an arm, a figure), else in the plate's middle.
    if (const ObjectId id = selectedOne(); id != kInvalidId && m_doc.find(id)) {
        const Bounds b = m_doc.worldBounds(id);
        const double length = m_handPanel->settings().length;
        mode.x = double(b.max[0]) + length * (m_handPanel->pair() ? 1.6 : 0.8);
        mode.y = (double(b.min[1]) + b.max[1]) / 2.0;
    }
    m_handMode = std::move(mode);
    refreshStandardRows();
    m_handPanel->start();
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_handPanel->show();
    statusBar()->showMessage(tr("Make Hand: choose the pose and size, then Make. Esc cancels."));
    updateHandPreview();
    refreshCommandState();
}

void MainWindow::endHand()
{
    if (!m_handMode.active)
        return;
    m_handTimer->stop();
    m_handMode = HandMode{};
    m_handPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::updateHandPreview()
{
    auto& mode = m_handMode;
    if (!mode.active)
        return;
    m_handTimer->stop();
    mode.preview.reset();
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        const HandSettings s = m_handPanel->settings();
        const auto wrist = m_handPanel->wrist();
        if (wrist)
            socket::check(*wrist);
        const double r = wrist ? hand::socketRadius(*wrist) : 0.0, d = wrist ? hand::socketDepth(*wrist) : 0.0;
        std::vector<Mesh> pieces;
        const bool pair = m_handPanel->pair();
        const double gap = s.length * 0.9;
        for (int i = 0; i < (pair ? 2 : 1); ++i) {
            HandSettings h = s;
            if (pair)
                h.left = i == 1;
            Mesh m = hand::make(h, r, d).mesh;
            const double dx = mode.x + (pair ? (i == 0 ? -gap / 2.0 : gap / 2.0) : 0.0);
            for (std::size_t v = 0; v < m.vertexCount(); ++v) {
                m.positions[3 * v] += float(dx);
                m.positions[3 * v + 1] += float(mode.y);
            }
            pieces.push_back(std::move(m));
        }
        mode.preview = together(pieces);
        m_handPanel->setStatus(tr("Make adds %1, standing on the wrist.")
                                   .arg(pair ? tr("the pair") : QString::fromStdString(hand::nameOf(s))),
                               false);
        m_handPanel->setApplyEnabled(true);
    } catch (const std::exception& e) {
        m_handPanel->setStatus(QString::fromUtf8(e.what()), true);
        m_handPanel->setApplyEnabled(false);
    }
    refreshScene();
}

void MainWindow::applyHand()
{
    auto& mode = m_handMode;
    if (!mode.active)
        return;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        const std::vector<ObjectId> made = hand::addHands(m_stack, m_doc, m_handPanel->settings(),
                                                          m_handPanel->wrist(), m_handPanel->pair(), mode.x, mode.y);
        endHand();
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        setSelection(made);
    } catch (const std::exception& e) {
        m_handPanel->setStatus(tr("The hand couldn't be made: %1").arg(QString::fromUtf8(e.what())), true);
    }
}

// ---- Hand Ports -----------------------------------------------------------------------

PortFinish MainWindow::portSettings() const
{
    PortFinish f = m_portPanel->finish();
    if (m_portMode.spot) {
        f.point = m_portMode.spot->point;
        f.direction = m_portMode.spot->normal;
    }
    if (f.kind == PortKind::Grip && m_portMode.exit) {
        f.hasExit = true;
        f.exit = *m_portMode.exit;
    }
    return f;
}

void MainWindow::startPort(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    PortMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    // A hand's name: the socket for the peg, to start.
    PortPanel::What what = QString::fromStdString(body->name).contains(QStringLiteral("hand"), Qt::CaseInsensitive)
                               ? PortPanel::What::Socket
                               : PortPanel::What::Peg;
    PortFinish f;
    try {
        if (editing) {
            f = portOf(m_doc, target);
            mode.toPlate = m_doc.worldTransform(target);
            mode.spot = emboss::SurfacePoint{f.point, f.direction};
            if (f.hasExit)
                mode.exit = f.exit;
            what = f.kind == PortKind::Grip ? PortPanel::What::Grip : PortPanel::What::Peg;
        }
        const Bounds b = m_doc.worldBounds(target);
        const auto size = b.size();
        mode.reach = std::sqrt(double(size[0]) * size[0] + double(size[1]) * size[1] + double(size[2]) * size[2]) + 2.0;
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    m_portMode = std::move(mode);
    refreshStandardRows();
    const QString name = QString::fromStdString(body->name);
    m_portPanel->start(editing ? tr("Change the port on '%1'").arg(name) : tr("Hand Ports on '%1'").arg(name), editing,
                       what, f);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_portPanel->show();
    statusBar()->showMessage(tr("Hand Ports: click the shape where it goes, then Apply. Esc cancels."));
    updatePortPreview();
    refreshCommandState();
}

void MainWindow::endPort()
{
    if (!m_portMode.active)
        return;
    m_portMode = PortMode{};
    m_portPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::portClick(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_portMode;
    const QVector3D d = direction.normalized();
    std::optional<emboss::SurfacePoint> spot;
    try {
        spot = surfaceUnder(m_doc, mode.target, {origin.x(), origin.y(), origin.z()}, {d.x(), d.y(), d.z()}, 0.3);
    } catch (const std::exception& e) {
        m_portPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    if (!spot) {
        statusBar()->showMessage(tr("Hand Ports: click on the shape itself."), 4000);
        return;
    }
    const Transform back = mode.toPlate.inverse();
    const Vec3 p = back.apply(spot->point);
    const Vec3 tip = back.apply({spot->point[0] + spot->normal[0], spot->point[1] + spot->normal[1],
                                 spot->point[2] + spot->normal[2]});
    const emboss::SurfacePoint here{p, {tip[0] - p[0], tip[1] - p[1], tip[2] - p[2]}};
    if (m_portPanel->clickSetsExit()) {
        if (!mode.spot) {
            m_portPanel->setStatus(tr("Set the way in first: choose \"The way in\" and click where the handle goes "
                                      "in."),
                                   true);
            return;
        }
        mode.exit = here.point;
    } else {
        mode.spot = here;
    }
    updatePortPreview();
}

void MainWindow::updatePortPreview()
{
    auto& mode = m_portMode;
    if (!mode.active)
        return;
    mode.shape.reset();
    bool ok = false;
    if (!mode.spot) {
        m_portPanel->setStatus(tr("Click the shape where it goes."), false);
    } else {
        try {
            const PortFinish f = portSettings();
            if (m_portPanel->what() == PortPanel::What::Socket) {
                const SocketFinish sf = port::socketFor(f, m_portPanel->fit(), m_portPanel->resin());
                socket::check(sf);
                mode.shape = together(socket::cutters(sf, mode.spot->point, mode.spot->normal));
                m_portPanel->setStatus(tr("%1 for the peg. Apply cuts it.")
                                           .arg(QString::fromStdString(socket::describe(sf))),
                                       false);
            } else {
                port::check(f);
                mode.shape = together({port::shape(f, mode.reach)});
                m_portPanel->setStatus(QString::fromStdString(port::describe(f)) + tr(". Apply adds it."), false);
            }
            ok = true;
        } catch (const std::exception& e) {
            m_portPanel->setStatus(QString::fromUtf8(e.what()), true);
        }
    }
    m_portPanel->setApplyEnabled(ok);
    refreshScene();
}

void MainWindow::applyPort()
{
    auto& mode = m_portMode;
    if (!mode.active || !mode.spot)
        return;
    const ObjectId target = mode.target;
    const PortFinish f = portSettings();
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (mode.editing) {
            Body after = withPort(m_doc, target, f);
            m_doc.previewMesh(*after.boolean);  // worked out (and kept) before anything changes
            const Body before = *m_doc.find(target);
            endPort();
            run(std::make_unique<EditBodyCommand>(before, std::move(after), "Change port"));
            setSelection(target);
            return;
        }
        Body r;
        r.id = m_doc.allocateId();
        r.name = m_doc.find(target)->name;
        if (m_portPanel->what() == PortPanel::What::Socket)
            r.boolean = socketSpec(m_doc, target, *mode.spot,
                                   port::socketFor(f, m_portPanel->fit(), m_portPanel->resin()));
        else
            r.boolean = portSpec(m_doc, target, *mode.spot, f);
        m_doc.previewMesh(*r.boolean);
        const ObjectId id = r.id;
        endPort();
        run(std::make_unique<CombineCommand>(std::move(r)));
        setSelection(id);
    } catch (const std::exception& e) {
        if (m_portMode.active)
            m_portPanel->setStatus(tr("It couldn't be added: %1").arg(QString::fromUtf8(e.what())), true);
        else
            QMessageBox::warning(this, tr("Forge3D"), tr("It couldn't be added:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

// ---- Make Mould -----------------------------------------------------------------------

MouldFinish MainWindow::mouldSettings() const
{
    MouldFinish f = m_mouldPanel->settings();
    f.spout = m_mouldMode.spout;
    f.vents = m_mouldMode.vents;
    // Which piece: the first (the others follow from it).
    f.top = true;
    f.side2 = f.second && f.secondIn != -1 ? 1 : 0;
    return f;
}

void MainWindow::startMould(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    MouldMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    MouldFinish f;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        mode.part = mouldPart(m_doc, target);
        if (mode.part.empty())
            throw std::invalid_argument("The shape has no surface to make a mould of.");
        mode.partBounds = mode.part.bounds();
        if (editing) {
            f = mouldOf(m_doc, target);
            mode.toPlate = m_doc.worldTransform(target);
            MouldFinish pair = f;
            pair.top = !f.top;
            for (const Body& o : m_doc.bodies())
                if (o.id != target && m_doc.isTopLevel(o.id) && o.isBoolean() && o.boolean->op == BooleanOp::Mould &&
                    o.boolean->mould && *o.boolean->mould == pair) {
                    mode.other = o.id;
                    break;
                }
            mode.shownPart = together({mode.part});
        } else {
            f = mould::suggested(mode.part);
        }
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
        return;
    }
    mode.spout = f.spout;
    mode.vents = f.vents;
    m_mouldMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_mouldPanel->start(editing ? tr("Change the mould '%1'").arg(name) : tr("Make a mould of '%1'").arg(name), editing,
                        f, m_mouldMode.partBounds);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_mouldPanel->show();
    statusBar()->showMessage(tr("Make Mould: set where the halves meet, click the part to add air vents (or move the "
                                "spout), then Apply. Esc cancels."));
    updateMouldPreview();
    refreshCommandState();
}

void MainWindow::endMould()
{
    if (!m_mouldMode.active)
        return;
    m_mouldMode = MouldMode{};
    m_mouldPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::mouldClick(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_mouldMode;
    const QVector3D d = direction.normalized();
    std::optional<emboss::SurfacePoint> spot;
    try {
        spot = surfaceUnder(m_doc, mode.target, {origin.x(), origin.y(), origin.z()}, {d.x(), d.y(), d.z()}, 0.5);
    } catch (const std::exception& e) {
        m_mouldPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    if (!spot) {
        statusBar()->showMessage(tr("Make Mould: click on the part itself."), 4000);
        return;
    }
    const Vec3 p = mode.toPlate.inverse().apply(spot->point);
    const MouldFinish f = mouldSettings();
    if (f.axis == 2 && p[2] <= f.parting) {
        m_mouldPanel->setStatus(tr("Click the part above the parting face (yellow): the spout and vents go up out of "
                                   "the top half."),
                                true);
        return;
    }
    if (m_mouldPanel->clickSetsSpout()) {
        mode.spout = p;
    } else {
        if (mode.vents.size() >= 20) {
            m_mouldPanel->setStatus(tr("That's as many air vents as there can be (20)."), true);
            return;
        }
        mode.vents.push_back(p);
    }
    updateMouldPreview();
}

void MainWindow::updateMouldPreview()
{
    auto& mode = m_mouldMode;
    if (!mode.active)
        return;
    const MouldFinish f = mouldSettings();
    const Bounds& pb = mode.partBounds;
    mode.box = together({mould::boxMesh(mould::box(pb, f.wall))});
    mode.sheet = together({mould::partingSheets(pb, f)});
    mode.channels = together({mould::channels(pb, f)});
    mode.keys = f.keys ? together({mould::keys(pb, f)}) : nullptr;
    const mould::Undercuts u = mould::undercuts(mode.part, f);
    mode.undercuts = u.triangles.empty() ? nullptr : together({mould::undercutMesh(mode.part, u)});
    m_mouldPanel->setUndercut(u.area, u.total);
    int vents = 0;
    for (const auto& v : f.vents)
        if (f.axis != 2 || v[2] > f.parting)
            ++vents;
    m_mouldPanel->setVentCount(vents);
    const bool spoutOk = f.axis != 2 || f.spout[2] > f.parting;
    if (!spoutOk)
        m_mouldPanel->setStatus(tr("The spout is below the parting face: lower the parting face, or choose \"The "
                                   "spout\" under Clicking the part places and click the part higher up."),
                                true);
    else if (f.keys && mould::keyRadius(f) <= 0.0)
        m_mouldPanel->setStatus(tr("The walls are too thin for keys: make them thicker, or the halves won't line up "
                                   "by themselves."),
                                true);
    else
        m_mouldPanel->setStatus(tr("Apply makes the pieces (a few seconds on a big part), closed round the part. "
                                   "Print each piece with its cut face down."),
                                false);
    m_mouldPanel->setApplyEnabled(spoutOk);
    refreshScene();
}

void MainWindow::applyMould()
{
    auto& mode = m_mouldMode;
    if (!mode.active)
        return;
    const MouldFinish f = mouldSettings();
    const ObjectId target = mode.target;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        mould::check(f);
        if (mode.editing) {
            std::vector<Body> after = withMould(m_doc, target, f);
            std::vector<Body> before;
            for (const Body& b : after) {
                m_doc.previewMesh(*b.boolean);  // worked out (and kept) before anything changes
                before.push_back(*m_doc.find(b.id));
            }
            endMould();
            run(std::make_unique<EditBodiesCommand>(std::move(before), std::move(after), "Change mould"));
            setSelection(target);
            return;
        }
        const std::vector<ObjectId> made = makeMould(m_stack, m_doc, target, f);
        endMould();
        qInfo().noquote() << "Command:" << QString::fromStdString(m_stack.undoLabel());
        setSelection(made);
    } catch (const std::exception& e) {
        if (m_mouldMode.active)
            m_mouldPanel->setStatus(tr("The mould couldn't be made: %1").arg(QString::fromUtf8(e.what())), true);
        else
            QMessageBox::warning(this, tr("Forge3D"),
                                 tr("The mould couldn't be made:\n%1").arg(QString::fromUtf8(e.what())));
    }
}

// ---- Hollow ---------------------------------------------------------------------------

void MainWindow::startHollow(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    HollowMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    HollowFinish finish;
    if (editing) {
        try {
            finish = hollowInPlate(m_doc, target);
        } catch (const std::exception& e) {
            QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
            return;
        }
        mode.holes = finish.holes;
    }
    m_hollowMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_hollowPanel->start(editing ? tr("Change the hollowing of '%1'").arg(name) : tr("Hollow '%1'").arg(name), editing,
                         finish.thickness, finish.holeSize);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_hollowPanel->show();
    statusBar()->showMessage(tr("Hollow: click the shape where each drain hole goes (or Place Them for Me), then "
                                "Apply. Esc cancels."));
    updateHollowPreview();
    refreshCommandState();
}

void MainWindow::endHollow()
{
    if (!m_hollowMode.active)
        return;
    m_hollowMode = HollowMode{};
    m_hollowPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::hollowClick(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_hollowMode;
    const QVector3D d = direction.normalized();
    std::optional<emboss::SurfacePoint> spot;
    try {
        // The way out averaged over the hole's size, so a bumpy surface gives a steady one.
        spot = surfaceUnder(m_doc, mode.target, {origin.x(), origin.y(), origin.z()}, {d.x(), d.y(), d.z()},
                            std::max(0.25, m_hollowPanel->holeSize() / 2.0));
    } catch (const std::exception& e) {
        m_hollowPanel->setStatus(QString::fromUtf8(e.what()), true);
        return;
    }
    if (!spot) {
        statusBar()->showMessage(tr("Hollow: click on the shape itself."), 4000);
        return;
    }
    if (mode.holes.size() >= 20) {
        m_hollowPanel->setStatus(tr("That's as many holes as there can be (20)."), true);
        return;
    }
    DrainHole h;
    h.point = spot->point;
    h.normal = spot->normal;
    mode.holes.push_back(h);
    updateHollowPreview();
}

void MainWindow::updateHollowPreview()
{
    auto& mode = m_hollowMode;
    if (!mode.active)
        return;
    std::vector<Mesh> cutters;
    for (const DrainHole& h : mode.holes)
        cutters.push_back(hollow::holeCutter(h, m_hollowPanel->holeSize(), m_hollowPanel->thickness()));
    mode.cut = cutters.empty() ? nullptr : together(cutters);
    m_hollowPanel->setHoleCount(int(mode.holes.size()));
    m_hollowPanel->setStatus(mode.holes.empty() ? QString()
                                                : tr("The red rods are the drain holes. Apply hollows it (a few "
                                                     "seconds on a big shape); Section View shows the inside "
                                                     "afterwards."),
                             false);
    m_hollowPanel->setApplyEnabled(true);
    refreshScene();
}

void MainWindow::applyHollow()
{
    auto& mode = m_hollowMode;
    if (!mode.active)
        return;
    HollowFinish f;
    f.thickness = m_hollowPanel->thickness();
    f.holeSize = m_hollowPanel->holeSize();
    f.holes = mode.holes;
    const bool plugs = m_hollowPanel->plugs() && !f.holes.empty();
    const ObjectId target = mode.target;
    std::optional<Body> result;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        hollow::check(f);
        if (mode.editing) {
            result = withHollow(m_doc, target, f);
            m_doc.previewMesh(*result->boolean);
        } else {
            Body r;
            r.id = m_doc.allocateId();
            r.name = m_doc.uniqueName("Hollowed");
            r.boolean = hollowSpec(m_doc, target, f);
            m_doc.previewMesh(*r.boolean);  // worked out (and kept) before anything changes
            result = std::move(r);
        }
    } catch (const std::exception& e) {
        m_hollowPanel->setStatus(tr("It couldn't be hollowed: %1").arg(QString::fromUtf8(e.what())), true);
        return;
    }
    if (mode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endHollow();
        const Body before = *now;
        endHollow();
        run(std::make_unique<EditBodyCommand>(before, *result, "Change hollowing"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    const ObjectId id = result->id;
    // Plugs stand beside the model, one per hole.
    std::vector<Body> plugBodies;
    if (plugs) {
        const Bounds box = m_doc.worldBounds(target);
        const PrimitiveSpec spec = hollow::plug(f, fits::starting(true, tolerance::Kind::Peg));
        const double r = std::get<CylinderSpec>(spec).radiusX;
        double x = box.max[0] + 5.0 + r;
        for (std::size_t i = 0; i < f.holes.size(); ++i) {
            Body p;
            p.id = m_doc.allocateId();
            p.spec = spec;
            p.hardware = true;
            p.placement.x = x;
            p.placement.y = box.min[1] + r;
            x += 2 * r + 3.0;
            plugBodies.push_back(std::move(p));
        }
    }
    endHollow();
    try {
        m_stack.beginMacro("Hollow");
        if (!m_doc.isTopLevel(target))
            m_stack.execute(std::make_unique<InsertStepCommand>(std::move(*result), m_doc.parentOf(target)));
        else
            m_stack.execute(std::make_unique<CombineCommand>(std::move(*result)));
        for (Body& p : plugBodies) {
            p.name = m_doc.uniqueName("Plug");
            m_stack.execute(std::make_unique<AddBodyCommand>(std::move(p)));
        }
        m_stack.endMacro();
    } catch (const std::exception& e) {
        m_stack.abortMacro();
        QMessageBox::warning(this, tr("Forge3D"), tr("It couldn't be hollowed:\n%1").arg(QString::fromUtf8(e.what())));
        refreshScene();
        return;
    }
    refreshScene();
    refreshCommandState();
    setSelection(m_doc.isTopLevel(id) ? id : history::objectOf(m_doc, target));
    statusBar()->showMessage(f.holes.empty() ? tr("Hollowed, with no drain holes: resin will stay trapped inside. "
                                                  "Double-click it in Objects to add some.")
                             : plugs ? tr("Hollowed, with %1 drain holes and a plug for each (beside the model).")
                                           .arg(f.holes.size())
                                     : tr("Hollowed, with %1 drain holes.").arg(f.holes.size()),
                             10000);
}

// ---- Pose Figure ------------------------------------------------------------------------

namespace {

// The joint a ball bends when it's dragged: the one at the start of the bone it ends
// (none for the hips, waist and shoulders).
std::optional<rig::Joint> jointBending(rig::Point p)
{
    using P = rig::Point;
    using J = rig::Joint;
    switch (p) {
    case P::Neck: return J::Waist;
    case P::Head: return J::Neck;
    case P::LeftElbow: return J::LeftShoulder;
    case P::RightElbow: return J::RightShoulder;
    case P::LeftWrist: return J::LeftElbow;
    case P::RightWrist: return J::RightElbow;
    case P::LeftHand: return J::LeftWrist;
    case P::RightHand: return J::RightWrist;
    case P::LeftKnee: return J::LeftHip;
    case P::RightKnee: return J::RightHip;
    case P::LeftAnkle: return J::LeftKnee;
    case P::RightAnkle: return J::RightKnee;
    case P::LeftToe: return J::LeftAnkle;
    case P::RightToe: return J::RightAnkle;
    default: return std::nullopt;
    }
}

// The ball the joint's bone ends at.
rig::Point endOf(rig::Joint j)
{
    for (int p = 0; p < kRigPointCount; ++p)
        if (jointBending(rig::Point(p)) == j)
            return rig::Point(p);
    return rig::Point::Waist;
}

double ballRadius(const rig::Points& points)
{
    return std::max(0.3, 0.015 * rig::height(points));
}

}  // namespace

void MainWindow::startPoseFigure(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    PoseFigureMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        if (editing && (!body->boolean || !body->boolean->posed))
            throw std::invalid_argument("That isn't a Pose Figure step.");
        const ObjectId figure = editing ? body->boolean->operands.front() : target;
        mode.frame = editing ? m_doc.worldTransform(target) : rigFrame(m_doc, target);
        // The figure as made, in the step's coordinates.
        auto rest = std::make_shared<Mesh>(*m_doc.mesh(figure));
        const Transform t = m_doc.localTransform(figure);
        for (std::size_t i = 0; i + 2 < rest->positions.size(); i += 3) {
            const Vec3 p = t.apply(Vec3{rest->positions[i], rest->positions[i + 1], rest->positions[i + 2]});
            for (std::size_t k = 0; k < 3; ++k)
                rest->positions[i + k] = float(p[k]);
        }
        for (std::size_t i = 0; i + 2 < rest->normals.size(); i += 3) {
            const Vec3 n = t.applyDirection(Vec3{rest->normals[i], rest->normals[i + 1], rest->normals[i + 2]});
            for (std::size_t k = 0; k < 3; ++k)
                rest->normals[i + k] = float(n[k]);
        }
        if (rest->empty())
            throw std::invalid_argument("There's no surface to pose.");
        mode.fitted = rig::fit(*rest);
        mode.finish = editing ? *body->boolean->posed : mode.fitted;
        mode.posedYet = editing && mode.finish.angles != rig::restAngles(mode.finish.points);
        mode.rest = std::move(rest);
    } catch (const std::exception& e) {
        QMessageBox::information(this, tr("Forge3D"),
                                 tr("This shape can't be posed:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    m_poseFigureMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_poseFigurePanel->start(editing ? tr("Change the pose of '%1'").arg(name) : tr("Pose '%1'").arg(name), editing,
                             m_poseFigureMode.finish, m_doc.mannequin().has_value());
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_poseFigurePanel->show();
    statusBar()->showMessage(editing ? tr("Pose Figure: drag a ball to bend the joint above it, or choose a pose. "
                                          "Apply when it's right; Esc cancels.")
                                     : tr("Pose Figure: drag the skeleton's balls onto the figure's joints, then "
                                          "click 2. Pose. Esc cancels."));
    updatePoseFigurePreview();
    refreshCommandState();
}

void MainWindow::endPoseFigure()
{
    if (!m_poseFigureMode.active)
        return;
    m_poseFigureMode = PoseFigureMode{};
    m_poseFigurePanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::updatePoseFigurePreview()
{
    auto& mode = m_poseFigureMode;
    if (!mode.active)
        return;
    const bool posing = m_poseFigurePanel->posing();
    int highlight = mode.picked;
    try {
        rig::check(mode.finish);
        if (posing) {
            if (!mode.skin || !mode.skin->sameWeights(mode.finish)) {
                QApplication::setOverrideCursor(Qt::WaitCursor);
                struct RestoreCursor {
                    ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
                } restore;
                mode.skin = std::make_shared<rig::Skin>(*mode.rest, mode.finish);
            }
            auto posed = mode.skin->posed(mode.finish);
            mode.shown = posed.points;
            mode.posed = std::make_shared<const Mesh>(std::move(posed.mesh));
            if (highlight < 0)
                highlight = int(rig::pivot(rig::Joint(m_poseFigurePanel->joint())));
        } else {
            mode.shown = mode.finish.points;
        }
        m_poseFigurePanel->setStatus({}, false);
    } catch (const std::exception& e) {
        mode.shown = mode.finish.points;
        mode.posed.reset();
        m_poseFigurePanel->setStatus(QString::fromUtf8(e.what()), true);
    }
    mode.skeleton = std::make_shared<const Mesh>(rig::skeletonMesh(mode.shown, ballRadius(mode.shown), highlight));
    m_poseFigurePanel->setFinish(mode.finish);
    refreshScene();
}

bool MainWindow::poseFigureGrab(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_poseFigureMode;
    const QVector3D d = direction.normalized();
    // The ball nearest the eye under the mouse (a little room around each).
    const double reach = 2.0 * ballRadius(mode.shown);
    int best = -1;
    double bestT = std::numeric_limits<double>::max();
    for (int i = 0; i < kRigPointCount; ++i) {
        const Vec3 p = mode.frame.apply(mode.shown[std::size_t(i)]);
        const QVector3D c{float(p[0]), float(p[1]), float(p[2])};
        const float t = QVector3D::dotProduct(c - origin, d);
        if (t < 0)
            continue;
        const float miss = (origin + t * d - c).length();
        if (miss <= reach && t < bestT) {
            bestT = t;
            best = i;
        }
    }
    if (best < 0)
        return false;
    const auto point = rig::Point(best);
    if (m_poseFigurePanel->posing()) {
        const auto joint = jointBending(point);
        if (!joint) {
            statusBar()->showMessage(tr("Pose Figure: the %1 doesn't bend anything; drag a ball further along "
                                        "(an elbow, a hand, a knee, a foot, the head).")
                                         .arg(tr(rig::title(point)).toLower()),
                                     6000);
            return false;
        }
        if (mode.finish.feetFlat && (*joint == rig::Joint::LeftAnkle || *joint == rig::Joint::RightAnkle)) {
            statusBar()->showMessage(tr("Pose Figure: the feet stay flat on the ground (untick it to point them)."),
                                     6000);
            return false;
        }
        m_poseFigurePanel->setJoint(int(*joint));
    }
    mode.picked = best;
    const Vec3 at = mode.frame.apply(mode.shown[std::size_t(best)]);
    mode.dragPlanePoint = at;
    mode.dragPlaneNormal = {-d.x(), -d.y(), -d.z()};
    updatePoseFigurePreview();
    return true;
}

void MainWindow::poseFigureDrag(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_poseFigureMode;
    if (mode.picked < 0)
        return;
    const QVector3D p(float(mode.dragPlanePoint[0]), float(mode.dragPlanePoint[1]), float(mode.dragPlanePoint[2]));
    const QVector3D n(float(mode.dragPlaneNormal[0]), float(mode.dragPlaneNormal[1]), float(mode.dragPlaneNormal[2]));
    const auto hit = view::intersectPlane(origin, direction, p, n);
    if (!hit)
        return;
    const Vec3 target = mode.frame.inverse().apply(Vec3{hit->x(), hit->y(), hit->z()});
    const auto point = rig::Point(mode.picked);
    if (!m_poseFigurePanel->posing()) {
        // Fitting: the ball goes where the mouse is (and its partner the same way, mirrored).
        const Vec3 old = mode.finish.points[std::size_t(mode.picked)];
        mode.finish.points[std::size_t(mode.picked)] = target;
        const rig::Point other = rig::mirrored(point);
        if (m_poseFigurePanel->mirror() && other != point) {
            const double middle = mode.finish.points[std::size_t(rig::Point::Waist)][0];
            mode.finish.points[std::size_t(other)] = {2 * middle - target[0], target[1], target[2]};
        }
        try {
            rig::check(mode.finish);
        } catch (const std::exception&) {
            mode.finish.points[std::size_t(mode.picked)] = old;  // not on top of another
            return;
        }
        if (!mode.posedYet)
            mode.finish.angles = rig::restAngles(mode.finish.points);
        updatePoseFigurePreview();
        return;
    }
    // Posing: the joint above the ball turns so the ball follows the mouse.
    const auto joint = jointBending(point);
    if (!joint)
        return;
    const std::size_t j = std::size_t(*joint);
    const rig::Point pivot = rig::pivot(*joint);
    const rig::Point end = endOf(*joint);
    const int dofs = rig::angleCount(*joint);
    std::array<double, 2> lo{}, hi{};
    for (int k = 0; k < dofs; ++k) {
        const auto r = rig::angleRange(*joint, k);
        const double now = mode.finish.angles[j][std::size_t(k)];
        lo[std::size_t(k)] = std::min(r[0], now);
        hi[std::size_t(k)] = std::max(r[1], now);
    }
    PosedFinish trial = mode.finish;
    trial.onGround = false;  // directions only: where it stands doesn't matter
    // How far off the ball's bone points from the mouse (the shift onto the ground is the
    // same for every point, so the shown pivot works).
    const Vec3 shownPivot = mode.shown[std::size_t(pivot)];
    const Vec3 want{target[0] - shownPivot[0], target[1] - shownPivot[1], target[2] - shownPivot[2]};
    const double wantLen = std::sqrt(want[0] * want[0] + want[1] * want[1] + want[2] * want[2]);
    if (wantLen < 1e-6)
        return;
    const auto cost = [&](const std::array<double, 2>& a) {
        trial.angles[j] = a;
        const rig::Moves m = rig::moves(trial);
        const Vec3& b0 = m.points[std::size_t(pivot)];
        const Vec3& b1 = m.points[std::size_t(end)];
        const Vec3 bone{b1[0] - b0[0], b1[1] - b0[1], b1[2] - b0[2]};
        const double len = std::sqrt(bone[0] * bone[0] + bone[1] * bone[1] + bone[2] * bone[2]);
        if (len < 1e-9)
            return 2.0;
        return 1.0 - (bone[0] * want[0] + bone[1] * want[1] + bone[2] * want[2]) / (len * wantLen);
    };
    // A simple search: steps either way on each angle, halving when nothing helps.
    std::array<double, 2> best = mode.finish.angles[j];
    double bestCost = cost(best);
    for (double step = 16.0; step > 0.05;) {
        bool better = false;
        for (int k = 0; k < dofs; ++k)
            for (double sign : {-1.0, 1.0}) {
                std::array<double, 2> a = best;
                a[std::size_t(k)] = std::clamp(a[std::size_t(k)] + sign * step, lo[std::size_t(k)], hi[std::size_t(k)]);
                const double c = cost(a);
                if (c < bestCost - 1e-12) {
                    best = a;
                    bestCost = c;
                    better = true;
                }
            }
        if (!better)
            step /= 2;
    }
    for (int k = 0; k < dofs; ++k)
        best[std::size_t(k)] = std::round(best[std::size_t(k)] * 10.0) / 10.0;
    mode.finish.angles[j] = best;
    mode.posedYet = true;
    updatePoseFigurePreview();
}

void MainWindow::poseFigureRelease()
{
    auto& mode = m_poseFigureMode;
    if (mode.picked < 0)
        return;
    mode.picked = -1;
    updatePoseFigurePreview();
}

void MainWindow::applyPoseFigure()
{
    auto& mode = m_poseFigureMode;
    if (!mode.active)
        return;
    const PosedFinish f = mode.finish;
    const ObjectId target = mode.target;
    std::optional<Body> result;
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        struct RestoreCursor {
            ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
        } restore;
        rig::check(f);
        if (mode.editing) {
            result = withPosed(m_doc, target, f);
            m_doc.previewMesh(*result->boolean);
        } else {
            Body r;
            r.id = m_doc.allocateId();
            r.name = m_doc.uniqueName("Posed");
            r.boolean = posedSpec(m_doc, target, f);
            m_doc.previewMesh(*r.boolean);  // worked out (and kept) before anything changes
            result = std::move(r);
        }
    } catch (const std::exception& e) {
        m_poseFigurePanel->setStatus(tr("It couldn't be posed: %1").arg(QString::fromUtf8(e.what())), true);
        return;
    }
    if (mode.editing) {
        const Body* now = m_doc.find(target);
        if (!now)
            return endPoseFigure();
        const Body before = *now;
        endPoseFigure();
        run(std::make_unique<EditBodyCommand>(before, *result, "Change pose"));
        setSelection(history::objectOf(m_doc, target));
        return;
    }
    const ObjectId id = result->id;
    endPoseFigure();
    try {
        if (!m_doc.isTopLevel(target))
            m_stack.execute(std::make_unique<InsertStepCommand>(std::move(*result), m_doc.parentOf(target)));
        else
            m_stack.execute(std::make_unique<CombineCommand>(std::move(*result)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Forge3D"), tr("It couldn't be posed:\n%1").arg(QString::fromUtf8(e.what())));
        refreshScene();
        return;
    }
    refreshScene();
    refreshCommandState();
    setSelection(m_doc.isTopLevel(id) ? id : history::objectOf(m_doc, target));
    statusBar()->showMessage(tr("Posed. Double-click the step in Objects to change the pose; sculpting goes on "
                                "from the posed figure."),
                             10000);
}

// ---- Surface stamps ---------------------------------------------------------------------

QStringList MainWindow::stampLibrary() const
{
    QStringList names;
    for (const auto& e : library::list(toPath(customShapesFolder())))
        names << QString::fromStdString(e.name);
    return names;
}

void MainWindow::startStamp(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    StampMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    StampFinish finish;
    if (editing) {
        try {
            finish = stampInPlate(m_doc, target);
        } catch (const std::exception& e) {
            QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
            return;
        }
        mode.spots = finish.spots;
        const auto& ops = body->boolean->operands;
        if (finish.kind == StampKind::Custom && ops.size() > 1) {
            try {
                mode.custom = meshkernel::transformed(m_doc.meshSolid(ops[1]), m_doc.worldTransform(ops[1]));
                mode.customName = QString::fromStdString(m_doc.find(ops[1])->name);
            } catch (const std::exception&) {
            }
        }
    }
    m_stampMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_stampPanel->start(editing ? tr("Change the stamps on '%1'").arg(name) : tr("Stamp '%1'").arg(name),
                        stampLibrary(), editing ? &finish : nullptr, m_stampMode.customName);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_stampPanel->show();
    if (!editing)
        loadStampShape();
    statusBar()->showMessage(tr("Stamp: click the shape for one stamp, drag across it for a row, then Apply. Esc "
                                "cancels."));
    updateStampPreview();
    refreshCommandState();
}

void MainWindow::endStamp()
{
    if (!m_stampMode.active)
        return;
    m_stampMode = StampMode{};
    m_stampPanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

void MainWindow::loadStampShape()
{
    auto& mode = m_stampMode;
    if (!mode.active || mode.editing)
        return;
    mode.custom.reset();
    mode.customName.clear();
    const int index = m_stampPanel->customIndex();
    if (index < 0)
        return;
    const auto entries = library::list(toPath(customShapesFolder()));
    if (index >= int(entries.size()))
        return;
    try {
        // Built in a scratch document, to see its shape before it's added.
        Document scratch;
        CommandStack stack(scratch);
        const auto ids = library::add(stack, scratch, entries[std::size_t(index)].file);
        std::vector<MeshSolid> pieces;
        for (ObjectId id : ids)
            pieces.push_back(meshkernel::transformed(scratch.meshSolid(id), scratch.worldTransform(id)));
        mode.custom = meshkernel::together(pieces);
        mode.customName = QString::fromStdString(entries[std::size_t(index)].name);
    } catch (const std::exception& e) {
        m_stampPanel->setStatus(tr("That custom shape couldn't be used: %1").arg(QString::fromUtf8(e.what())), true);
    }
}

std::optional<StampSpot> MainWindow::stampSpotAt(const QVector3D& origin, const QVector3D& direction) const
{
    const QVector3D d = direction.normalized();
    std::optional<emboss::SurfacePoint> hit;
    try {
        hit = surfaceUnder(m_doc, m_stampMode.target, {origin.x(), origin.y(), origin.z()}, {d.x(), d.y(), d.z()},
                           std::max(0.1, m_stampPanel->settings().size / 2.0));
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!hit)
        return std::nullopt;
    StampSpot s;
    s.point = hit->point;
    s.normal = hit->normal;
    // Along: the plate's X, as it lies on the surface (or Y where the surface faces X).
    s.along = std::abs(hit->normal[0]) > 0.9 ? std::array<double, 3>{0, 1, 0} : std::array<double, 3>{1, 0, 0};
    return s;
}

void MainWindow::stampClick(const QVector3D& origin, const QVector3D& direction)
{
    auto& mode = m_stampMode;
    const auto spot = stampSpotAt(origin, direction);
    if (!spot) {
        statusBar()->showMessage(tr("Stamp: click on the shape itself."), 4000);
        return;
    }
    mode.groups.push_back(mode.spots.size());
    mode.spots.push_back(*spot);
    updateStampPreview();
}

void MainWindow::stampDrag(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers)
{
    auto& mode = m_stampMode;
    if (!mode.rowStart)
        return;
    auto end = stampSpotAt(origin, direction);
    if (!end)
        return;
    const double radius = std::max(0.1, m_stampPanel->settings().size / 2.0);
    const auto plane = straightPlane(mode.rowStart->point, modifiers, direction);
    if (plane) {
        // Shift: a level row; Ctrl: an upright one.
        const auto q = onSurfaceInPlane(mode.target, {end->point[0], end->point[1], end->point[2]},
                                        {end->normal[0], end->normal[1], end->normal[2]}, *plane, radius);
        if (!q)
            return;
        end->point = q->point;
        end->normal = q->normal;
    }
    const StampSpot& a = *mode.rowStart;
    const StampFinish f = m_stampPanel->settings();
    const Vec3 pa{a.point[0], a.point[1], a.point[2]}, pb{end->point[0], end->point[1], end->point[2]};
    Vec3 along{pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
    const double length = std::sqrt(along[0] * along[0] + along[1] * along[1] + along[2] * along[2]);
    if (length < 1e-6)
        return;
    along = {along[0] / length, along[1] / length, along[2] / length};
    Vec3 n{a.normal[0] + end->normal[0], a.normal[1] + end->normal[1], a.normal[2] + end->normal[2]};
    const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    n = nl > 1e-9 ? Vec3{n[0] / nl, n[1] / nl, n[2] / nl} : Vec3{a.normal[0], a.normal[1], a.normal[2]};
    mode.row.clear();
    const double lift = 4.0 * f.size + 5.0;
    for (const Vec3& p : stamp::rowPoints(pa, pb, f.spacing)) {
        if (plane) {
            // Kept in the straight row's plane.
            if (const auto q = onSurfaceInPlane(mode.target, p, n, *plane, radius)) {
                StampSpot s;
                s.point = q->point;
                s.normal = q->normal;
                s.along = along;
                mode.row.push_back(s);
            }
        } else {
            // Each one dropped onto the surface from just outside it.
            const QVector3D from(float(p[0] + n[0] * lift), float(p[1] + n[1] * lift), float(p[2] + n[2] * lift));
            const QVector3D down(float(-n[0]), float(-n[1]), float(-n[2]));
            if (auto s = stampSpotAt(from, down)) {
                s->along = along;
                mode.row.push_back(*s);
            }
        }
        if (mode.spots.size() + mode.row.size() >= 2000)
            break;
    }
    updateStampPreview();
}

void MainWindow::updateStampPreview()
{
    auto& mode = m_stampMode;
    if (!mode.active)
        return;
    StampFinish f = m_stampPanel->settings();
    f.spots = mode.spots;
    f.spots.insert(f.spots.end(), mode.row.begin(), mode.row.end());
    m_stampPanel->setCount(int(f.spots.size()));
    mode.shown.reset();
    if (f.kind == StampKind::Custom && !mode.custom) {
        m_stampPanel->setStatus(tr("Pick a stamp."), true);
        refreshScene();
        return;
    }
    if (!f.spots.empty()) {
        try {
            mode.shown = std::make_shared<const Mesh>(
                meshkernel::displayMesh(stamp::placed(f, mode.custom ? &*mode.custom : nullptr)));
            m_stampPanel->setStatus({}, false);
        } catch (const std::exception& e) {
            m_stampPanel->setStatus(QString::fromUtf8(e.what()), true);
        }
    }
    refreshScene();
}

void MainWindow::applyStamp()
{
    auto& mode = m_stampMode;
    if (!mode.active || mode.spots.empty())
        return;
    StampFinish f = m_stampPanel->settings();
    f.spots = mode.spots;
    const ObjectId target = mode.target;
    const QString customName = mode.customName;
    const int customIndex = m_stampPanel->customIndex();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    struct RestoreCursor {
        ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
    } restore;
    if (mode.editing) {
        try {
            const Body after = withStamp(m_doc, target, f);
            m_doc.previewMesh(*after.boolean);
            const Body before = *m_doc.find(target);
            endStamp();
            run(std::make_unique<EditBodyCommand>(before, after, "Change stamps"));
            setSelection(history::objectOf(m_doc, target));
        } catch (const std::exception& e) {
            m_stampPanel->setStatus(tr("It couldn't be stamped: %1").arg(QString::fromUtf8(e.what())), true);
        }
        return;
    }
    if (f.kind != StampKind::Custom) {
        try {
            Body r;
            r.id = m_doc.allocateId();
            r.name = m_doc.uniqueName("Stamped");
            r.boolean = stampSpec(m_doc, target, f);
            m_doc.previewMesh(*r.boolean);  // worked out (and kept) before anything changes
            const ObjectId id = r.id;
            endStamp();
            const bool topLevel = m_doc.isTopLevel(target);
            if (topLevel)
                run(std::make_unique<CombineCommand>(std::move(r)));
            else
                run(std::make_unique<InsertStepCommand>(std::move(r), m_doc.parentOf(target)));
            setSelection(topLevel ? id : history::objectOf(m_doc, target));
        } catch (const std::exception& e) {
            m_stampPanel->setStatus(tr("It couldn't be stamped: %1").arg(QString::fromUtf8(e.what())), true);
        }
        return;
    }
    // A custom shape: added from the library, then taken into the step as its stamp.
    const auto entries = library::list(toPath(customShapesFolder()));
    if (customIndex < 0 || customIndex >= int(entries.size()))
        return;
    endStamp();
    try {
        m_stack.beginMacro("Stamp " + customName.toStdString());
        std::vector<ObjectId> ids = library::add(m_stack, m_doc, entries[std::size_t(customIndex)].file);
        ObjectId shape = ids.empty() ? kInvalidId : ids.front();
        if (ids.size() > 1) {
            Body group;
            group.id = m_doc.allocateId();
            group.name = customName.toStdString();
            group.boolean = BooleanSpec{};
            group.boolean->op = BooleanOp::Group;
            group.boolean->operands = ids;
            shape = group.id;
            m_stack.execute(std::make_unique<CombineCommand>(std::move(group)));
        }
        Body r;
        r.id = m_doc.allocateId();
        r.name = m_doc.uniqueName("Stamped");
        r.boolean = stampSpec(m_doc, target, f, shape);
        const ObjectId id = r.id;
        const bool topLevel = m_doc.isTopLevel(target);
        if (topLevel)
            m_stack.execute(std::make_unique<CombineCommand>(std::move(r)));
        else
            m_stack.execute(std::make_unique<InsertStepCommand>(std::move(r), m_doc.parentOf(target)));
        m_stack.endMacro();
        refreshScene();
        refreshCommandState();
        setSelection(topLevel ? id : history::objectOf(m_doc, target));
    } catch (const std::exception& e) {
        m_stack.abortMacro();
        QMessageBox::warning(this, tr("Forge3D"), tr("It couldn't be stamped:\n%1").arg(QString::fromUtf8(e.what())));
        refreshScene();
    }
}

// ---- Panel lines ------------------------------------------------------------------------

void MainWindow::startPanelLines(ObjectId target, bool editing)
{
    const Body* body = m_doc.find(target);
    if (!body || toolOpen())
        return;
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    PanelLineMode mode;
    mode.active = true;
    mode.editing = editing;
    mode.target = target;
    PanelLineFinish finish;
    if (editing) {
        try {
            finish = panelLinesInPlate(m_doc, target);
        } catch (const std::exception& e) {
            QMessageBox::information(this, tr("Forge3D"), QString::fromUtf8(e.what()));
            return;
        }
        mode.lines = finish.lines;
    }
    m_panelLineMode = std::move(mode);
    const QString name = QString::fromStdString(body->name);
    m_panelLinePanel->start(editing ? tr("Change the panel lines on '%1'").arg(name)
                                    : tr("Panel lines on '%1'").arg(name),
                            editing ? &finish : nullptr);
    m_propsDockWidth = m_propsDock->width();
    m_properties->hide();
    m_panelLinePanel->show();
    statusBar()->showMessage(tr("Panel Lines: click points along the surface; End Line or Close Loop, then Apply. "
                                "Esc cancels."));
    updatePanelLinePreview();
    refreshCommandState();
}

void MainWindow::endPanelLines()
{
    if (!m_panelLineMode.active)
        return;
    m_panelLineMode = PanelLineMode{};
    m_panelLinePanel->hide();
    m_properties->show();
    QTimer::singleShot(0, this, [this] { resizeDocks({m_propsDock}, {m_propsDockWidth}, Qt::Horizontal); });
    statusBar()->clearMessage();
    refreshScene();
    refreshCommandState();
}

std::optional<PanelPoint> MainWindow::panelPointAt(const QVector3D& origin, const QVector3D& direction) const
{
    const QVector3D d = direction.normalized();
    std::optional<emboss::SurfacePoint> hit;
    try {
        hit = surfaceUnder(m_doc, m_panelLineMode.target, {origin.x(), origin.y(), origin.z()},
                           {d.x(), d.y(), d.z()}, std::max(0.05, m_panelLinePanel->settings().width / 2.0));
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!hit)
        return std::nullopt;
    PanelPoint p;
    p.point = hit->point;
    p.normal = hit->normal;
    return p;
}

std::vector<PanelPoint> MainWindow::panelPiece(const PanelPoint& a, const PanelPoint& b,
                                               const std::optional<StraightPlane>& plane) const
{
    const Vec3 pa{a.point[0], a.point[1], a.point[2]}, pb{b.point[0], b.point[1], b.point[2]};
    Vec3 n{a.normal[0] + b.normal[0], a.normal[1] + b.normal[1], a.normal[2] + b.normal[2]};
    const double nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    n = nl > 1e-9 ? Vec3{n[0] / nl, n[1] / nl, n[2] / nl} : Vec3{a.normal[0], a.normal[1], a.normal[2]};
    const double width = m_panelLinePanel->settings().width;
    const auto points = stamp::rowPoints(pa, pb, panel::pointSpacing(width));
    std::vector<PanelPoint> out;
    const double lift = 2.0 + 4.0 * width;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Vec3& p = points[i];
        if (i + 1 == points.size()) {
            out.push_back(b);  // ends exactly where it was clicked
            break;
        }
        if (plane) {
            // Kept in the straight line's plane.
            if (const auto q = onSurfaceInPlane(m_panelLineMode.target, p, n, *plane, std::max(0.05, width / 2.0)))
                out.push_back({q->point, q->normal});
            continue;
        }
        // Dropped onto the surface from just outside it.
        const QVector3D from(float(p[0] + n[0] * lift), float(p[1] + n[1] * lift), float(p[2] + n[2] * lift));
        const QVector3D down(float(-n[0]), float(-n[1]), float(-n[2]));
        if (auto q = panelPointAt(from, down))
            out.push_back(*q);
    }
    if (out.empty())
        out.push_back(b);
    return out;
}

PanelLine MainWindow::currentPanelLine(bool closing) const
{
    const auto& mode = m_panelLineMode;
    PanelLine line;
    if (mode.anchors.empty())
        return line;
    line.points.push_back(mode.anchors.front());
    for (const auto& piece : mode.pieces)
        line.points.insert(line.points.end(), piece.begin(), piece.end());
    if (closing && mode.anchors.size() >= 3) {
        // Back round to the start (the last point before it; the loop joins itself).
        auto back = panelPiece(mode.anchors.back(), mode.anchors.front());
        back.pop_back();
        line.points.insert(line.points.end(), back.begin(), back.end());
        line.closed = true;
    }
    return line;
}

void MainWindow::endPanelLine(bool closing)
{
    auto& mode = m_panelLineMode;
    if (closing && mode.anchors.size() < 3) {
        m_panelLinePanel->setStatus(tr("A loop needs at least three points."), true);
        return;
    }
    if (mode.anchors.size() >= 2) {
        PanelLine line = currentPanelLine(closing);
        if (line.points.size() >= 2)
            mode.lines.push_back(std::move(line));
    }
    mode.anchors.clear();
    mode.pieces.clear();
    mode.planes.clear();
    updatePanelLinePreview();
}

void MainWindow::panelLineClick(const QVector3D& origin, const QVector3D& direction, Qt::KeyboardModifiers modifiers)
{
    auto& mode = m_panelLineMode;
    auto p = panelPointAt(origin, direction);
    if (!p) {
        statusBar()->showMessage(tr("Panel Lines: click on the shape itself."), 4000);
        return;
    }
    if (!mode.anchors.empty()) {
        const auto plane = straightPlane(mode.anchors.back().point, modifiers, direction);
        if (plane) {
            // Shift: level with the last point; Ctrl: straight up or down from it.
            const auto q = onSurfaceInPlane(mode.target, {p->point[0], p->point[1], p->point[2]},
                                            {p->normal[0], p->normal[1], p->normal[2]}, *plane,
                                            std::max(0.05, m_panelLinePanel->settings().width / 2.0));
            if (!q) {
                statusBar()->showMessage(tr("Panel Lines: that's not on the shape, kept straight."), 4000);
                return;
            }
            p = PanelPoint{q->point, q->normal};
        }
        mode.pieces.push_back(panelPiece(mode.anchors.back(), *p, plane));
        mode.planes.push_back(plane);
    }
    mode.anchors.push_back(*p);
    updatePanelLinePreview();
}

void MainWindow::updatePanelLinePreview()
{
    auto& mode = m_panelLineMode;
    if (!mode.active)
        return;
    PanelLineFinish f = m_panelLinePanel->settings();
    f.lines = mode.lines;
    const PanelLine current = currentPanelLine(false);
    if (current.points.size() >= 2)
        f.lines.push_back(current);
    m_panelLinePanel->setCounts(int(mode.lines.size()), int(mode.anchors.size()));
    mode.shown.reset();
    if (!f.lines.empty()) {
        try {
            mode.shown = std::make_shared<const Mesh>(panel::shown(f));
            m_panelLinePanel->setStatus({}, false);
        } catch (const std::exception& e) {
            m_panelLinePanel->setStatus(QString::fromUtf8(e.what()), true);
        }
    }
    refreshScene();
}

void MainWindow::applyPanelLines()
{
    auto& mode = m_panelLineMode;
    if (!mode.active)
        return;
    if (mode.anchors.size() >= 2)
        endPanelLine(false);  // the line being drawn counts
    PanelLineFinish f = m_panelLinePanel->settings();
    f.lines = mode.lines;
    if (f.lines.empty())
        return;
    const ObjectId target = mode.target;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    struct RestoreCursor {
        ~RestoreCursor() { QApplication::restoreOverrideCursor(); }
    } restore;
    try {
        if (mode.editing) {
            const Body after = withPanelLines(m_doc, target, f);
            m_doc.previewMesh(*after.boolean);
            const Body before = *m_doc.find(target);
            endPanelLines();
            run(std::make_unique<EditBodyCommand>(before, after, "Change panel lines"));
            setSelection(history::objectOf(m_doc, target));
            return;
        }
        Body r;
        r.id = m_doc.allocateId();
        r.name = m_doc.uniqueName("Panel-lined");
        r.boolean = panelLineSpec(m_doc, target, f);
        m_doc.previewMesh(*r.boolean);  // worked out (and kept) before anything changes
        const ObjectId id = r.id;
        endPanelLines();
        const bool topLevel = m_doc.isTopLevel(target);
        if (topLevel)
            run(std::make_unique<CombineCommand>(std::move(r)));
        else
            run(std::make_unique<InsertStepCommand>(std::move(r), m_doc.parentOf(target)));
        setSelection(topLevel ? id : history::objectOf(m_doc, target));
    } catch (const std::exception& e) {
        m_panelLinePanel->setStatus(tr("The lines couldn't be cut: %1").arg(QString::fromUtf8(e.what())), true);
    }
}

// ---- Straight lines (Shift: level, Ctrl: upright) -------------------------------------

std::optional<MainWindow::StraightPlane> MainWindow::straightPlane(const std::array<double, 3>& from,
                                                                   Qt::KeyboardModifiers modifiers,
                                                                   const QVector3D& direction) const
{
    StraightPlane plane;
    plane.point = {from[0], from[1], from[2]};
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        plane.normal = {0, 0, 1};  // level: the same height all the way
        return plane;
    }
    if (modifiers.testFlag(Qt::ControlModifier)) {
        // Upright: the up-and-down plane facing the way you're looking.
        Vec3 look{direction.x(), direction.y(), 0.0};
        const double l = std::sqrt(look[0] * look[0] + look[1] * look[1]);
        look = l > 1e-6 ? Vec3{look[0] / l, look[1] / l, 0.0} : Vec3{0.0, 1.0, 0.0};
        plane.normal = {-look[1], look[0], 0.0};  // up × look: across the view
        return plane;
    }
    return std::nullopt;
}

std::optional<emboss::SurfacePoint> MainWindow::onSurfaceInPlane(ObjectId target, const Vec3& q, const Vec3& n,
                                                                 const StraightPlane& plane, double radius) const
{
    const Vec3& pn = plane.normal;
    const auto dot = [](const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    // Into the plane...
    const double off = dot({q[0] - plane.point[0], q[1] - plane.point[1], q[2] - plane.point[2]}, pn);
    const Vec3 t{q[0] - pn[0] * off, q[1] - pn[1] * off, q[2] - pn[2] * off};
    // ...then onto the surface along the way out, as it lies in the plane.
    const double along = dot(n, pn);
    Vec3 d{n[0] - pn[0] * along, n[1] - pn[1] * along, n[2] - pn[2] * along};
    const double dl = std::sqrt(dot(d, d));
    if (dl < 1e-6)
        return std::nullopt;  // the surface faces along the plane here: nowhere in it to land
    d = {d[0] / dl, d[1] / dl, d[2] / dl};
    // From just outside: as far as the point was moved, and a little more (not so far
    // that it lands on some other part of the shape in front).
    const double lift = std::abs(off) + 2.0 + 4.0 * radius;
    try {
        return surfaceUnder(m_doc, target, {t[0] + d[0] * lift, t[1] + d[1] * lift, t[2] + d[2] * lift},
                            {-d[0], -d[1], -d[2]}, radius);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// ---- Kit layout ---------------------------------------------------------------------------

void MainWindow::makeKit()
{
    if (toolOpen()) {
        statusBar()->showMessage(tr("Close the open tool first."), 5000);
        return;
    }
    if (m_dragState.kind != Drag::Kind::None)
        cancelDrag();
    const std::vector<ObjectId> all = m_doc.topLevelIds();
    if (all.empty()) {
        statusBar()->showMessage(tr("Kit Layout: there's nothing on the plate."), 5000);
        return;
    }
    std::vector<ObjectId> picked;
    for (ObjectId id : m_selection)
        if (m_doc.isTopLevel(id))
            picked.push_back(id);
    KitDialog dialog(int(picked.size()), int(all.size()), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const std::vector<ObjectId>& ids = dialog.selectedOnly() ? picked : all;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    try {
        const ObjectId made = kit::make(m_stack, m_doc, ids, dialog.settings());
        QApplication::restoreOverrideCursor();
        refreshScene();
        refreshCommandState();
        setSelection(made);
        m_viewport->fitView();
        statusBar()->showMessage(tr("Made a kit of %1 parts.").arg(ids.size()), 8000);
    } catch (const std::exception& e) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Kit Layout"), tr("The kit couldn't be made:\n%1").arg(QString::fromUtf8(e.what())));
        refreshScene();
    }
}

// ---- Parts list ---------------------------------------------------------------------------

void MainWindow::showPartsList()
{
    if (m_doc.topLevelIds().empty()) {
        statusBar()->showMessage(tr("Parts List: there's nothing on the plate."), 5000);
        return;
    }
    // The selected shapes, or everything.
    std::vector<ObjectId> ids;
    for (ObjectId id : m_selection)
        if (m_doc.isTopLevel(id))
            ids.push_back(id);
    if (ids.empty())
        ids = m_doc.topLevelIds();
    const QString title = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).completeBaseName();
    PartsListDialog dialog(title, [this, ids](const parts::Settings& s) { return parts::rows(m_doc, ids, s); }, this);
    dialog.exec();
}

}  // namespace forge::app
