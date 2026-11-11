// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Align.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
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
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QMenu;
class QActionGroup;

namespace forge::app {

class PropertiesPanel;
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
    void combineSelected(BooleanOp op);
    void alignSelected(AlignMode mode);
    void separateSelected();
    void clearPlate();

    // Files
    void newDocument();
    void openDocument();
    bool save();
    bool saveAs();
    bool saveTo(const QString& path);
    bool confirmDiscardChanges();

    // Keeping the UI in step with the document
    void scheduleSceneRefresh();
    void refreshScene();
    void refreshObjectList();
    void addTreeItem(QTreeWidgetItem* parent, ObjectId id);
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
        enum class Kind { None, Plate, Arrow, Ring, Face };
        Kind kind = Kind::None;
        Body before;                  // the body when the drag started
        view::GizmoHandle handle;
        view::GizmoTarget frame;      // gizmo frame when the drag started
        QVector3D grabPoint;          // Plate: where the body was grabbed
        float startParam = 0.0f;      // Arrow / Face: position along the handle's line
        QPointF lastPixel;            // Ring: for accumulating the turn
        QPointF centrePixel;
        double turned = 0.0;          // Ring: degrees so far (unsnapped)
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
    std::set<ObjectId> m_expanded;  // results open in the Objects tree
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
    QMenu* m_alignMenu = nullptr;  // Align Shapes (toolbar dropdown and Edit submenu)
    QAction* m_fit = nullptr;
    QAction* m_resetView = nullptr;
    QAction* m_modeMove = nullptr;
    QAction* m_modeRotate = nullptr;
    QAction* m_modeResize = nullptr;
    QAction* m_snapAction = nullptr;
    QAction* m_uniformAction = nullptr;
};

}  // namespace forge::app
