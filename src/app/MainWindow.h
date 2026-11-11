// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"

#include <QMainWindow>

#include <memory>

class QAction;
class QCloseEvent;
class QLabel;

namespace forge::app {

class Viewport;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Adds a few bodies through the normal commands. For screenshots and demos.
    void loadDemoScene();

signals:
    void firstFrameRendered();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();

    // Runs a command through the undo stack; shows a message if it fails.
    bool run(std::unique_ptr<Command> command);

    // Test tools for slice 2 (replaced by the real Build tools in slice 3).
    void addBox();
    void addCylinder();
    void deleteLast();
    void makeLastTaller();
    void clearPlate();
    void newDocument();

    bool confirmDiscardChanges();
    void scheduleSceneRefresh();
    void refreshScene();
    void refreshCommandState();
    void showAbout();

    Document m_doc;
    CommandStack m_stack{m_doc};
    bool m_refreshQueued = false;

    Viewport* m_viewport = nullptr;
    QLabel* m_modelInfo = nullptr;
    QLabel* m_glInfo = nullptr;

    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    QAction* m_new = nullptr;
    QAction* m_quit = nullptr;
    QAction* m_addBox = nullptr;
    QAction* m_addCylinder = nullptr;
    QAction* m_deleteLast = nullptr;
    QAction* m_taller = nullptr;
    QAction* m_clear = nullptr;
    QAction* m_fit = nullptr;
    QAction* m_resetView = nullptr;
};

}  // namespace forge::app
