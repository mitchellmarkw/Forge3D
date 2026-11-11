// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"

#include <QMainWindow>
#include <QString>

#include <memory>

class QAction;
class QCloseEvent;
class QLabel;
class QListWidget;
class QMenu;
class QVector3D;

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

    // Selection (not part of the undo history, like in most CAD tools)
    void setSelection(ObjectId id);
    void onViewportClicked(const QVector3D& origin, const QVector3D& direction);

    // Editing
    void addBody(const PrimitiveSpec& spec);
    void deleteSelected();
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
    void refreshCommandState();
    void showAbout();

    Document m_doc;
    CommandStack m_stack{m_doc};
    ObjectId m_selected = kInvalidId;
    QString m_filePath;  // empty = never saved
    bool m_refreshQueued = false;
    bool m_syncingList = false;

    Viewport* m_viewport = nullptr;
    PropertiesPanel* m_properties = nullptr;
    QListWidget* m_objects = nullptr;
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
    QAction* m_addBox = nullptr;
    QAction* m_addCylinder = nullptr;
    QAction* m_clear = nullptr;
    QAction* m_fit = nullptr;
    QAction* m_resetView = nullptr;
};

}  // namespace forge::app
