// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMainWindow>

class QLabel;

namespace forge::app {

class Viewport;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void createMenus();
    void createStatusBar();
    void loadStarterModel();
    void showAbout();

    Viewport* m_viewport = nullptr;
    QLabel* m_modelInfo = nullptr;
    QLabel* m_glInfo = nullptr;
};

}  // namespace forge::app
