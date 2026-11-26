// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/ModelScale.h"

#include <QMainWindow>

class QComboBox;
class QLabel;
class QListWidget;
class QNetworkAccessManager;

namespace forge::app {
class Viewport;
}

namespace forge::scenery {

// Forge3D Scenery's window (slice 110: the groundwork). The same 3D view and .f3d files as
// Forge3D: open a model, see it on the plate, save it, export it for printing; and the
// model scale the scenery tools will build to. The tools themselves come next.
class SceneryWindow : public QMainWindow {
    Q_OBJECT

public:
    SceneryWindow();
    ~SceneryWindow() override;
    void openFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    Document m_doc;
    CommandStack m_stack{m_doc};
    QString m_filePath;
    modelscale::Scale m_scale;

    app::Viewport* m_viewport = nullptr;
    QListWidget* m_objects = nullptr;
    QComboBox* m_scaleBox = nullptr;
    QLabel* m_status = nullptr;
    QNetworkAccessManager* m_network = nullptr;

    void buildMenus();
    QWidget* buildRibbon();
    void refreshScene();
    void refreshTitle();
    bool modified() const;
    bool askToSave();  // false: cancelled
    void newModel();
    void openModel();
    bool saveModel();
    bool saveModelAs();
    bool saveTo(const QString& path);
    void exportModel();
    void useScale(const modelscale::Scale& s);
    void checkForUpdates(bool quietly);
    void showAbout();
};

}  // namespace forge::scenery
