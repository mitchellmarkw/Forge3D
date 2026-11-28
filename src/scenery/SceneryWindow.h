// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/Building.h"
#include "forge/core/Rock.h"
#include "forge/core/Terrain.h"
#include "forge/core/Tree.h"
#include "forge/core/ModelScale.h"

#include <QMainWindow>

#include <future>
#include <memory>

class QComboBox;
class QLabel;
class QListWidget;
class QNetworkAccessManager;
class QDockWidget;
class QPushButton;
class QTimer;

namespace forge::app {
class Viewport;
}

namespace forge::scenery {

class BuildingPanel;
class RockPanel;
class TerrainPanel;
class TreePanel;

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
    // The shape picked in Objects.
    ObjectId m_selected = kInvalidId;
    std::vector<ObjectId> m_rows;  // Objects' rows' shapes
    QPushButton* m_newBuilding = nullptr;
    QPushButton* m_newRock = nullptr;
    QPushButton* m_newTerrain = nullptr;
    QPushButton* m_newTree = nullptr;
    QPushButton* m_editBuilding = nullptr;  // Change: the picked building or rock

    // The building designer (slice 111).
    BuildingPanel* m_buildingPanel = nullptr;
    QDockWidget* m_panelDock = nullptr;
    struct BuildMode {
        bool active = false;
        ObjectId editing = kInvalidId;  // the walls being changed; none: a new building
        ObjectId roof = kInvalidId;     // its roof, if it has its own
        double x = 0.0, y = 0.0, z = 0.0;  // where the building's middle is on the plate
        int placing = -1;               // the next click puts a door (0) or window (1)
        int dragging = -1;              // the opening being dragged
        double grabAlong = 0.0, grabUp = 0.0;  // where it was held, from its middle and bottom
        std::shared_ptr<const Mesh> walls, roofMesh, marker, posts;
        int corner = -1;         // the footprint's corner being dragged (slice 115)
        int tower = -1;          // the tower being dragged (slice 117)
        double towerZ = 0.0;     // the height it was grabbed at (model mm)
        double cornerZ = 0.0;    // the height it was grabbed at (model mm)
        int generation = 0;  // the latest description (a detailed preview for an older one is dropped)
    };
    BuildMode m_build;
    QTimer* m_detailTimer = nullptr;
    std::future<void> m_detail;

    // The rock designer (slice 112).
    RockPanel* m_rockPanel = nullptr;
    QDockWidget* m_rockDock = nullptr;
    struct RockMode {
        bool active = false;
        ObjectId editing = kInvalidId;
        double x = 0.0, y = 0.0, z = 0.0;
        std::shared_ptr<const Mesh> mesh;
        int generation = 0;
    };
    RockMode m_rock;
    QTimer* m_rockTimer = nullptr;
    std::future<void> m_rockDetail;
    // The tree designer (slice 114).
    TreePanel* m_treePanel = nullptr;
    QDockWidget* m_treeDock = nullptr;
    struct TreeMode {
        bool active = false;
        ObjectId editing = kInvalidId;
        double x = 0.0, y = 0.0, z = 0.0;
        std::shared_ptr<const Mesh> mesh;
        int generation = 0;
    };
    TreeMode m_tree;
    QTimer* m_treeTimer = nullptr;
    std::future<void> m_treeDetail;
    // The terrain designer (slice 113).
    TerrainPanel* m_terrainPanel = nullptr;
    QDockWidget* m_terrainDock = nullptr;
    struct TerrainMode {
        bool active = false;
        ObjectId editing = kInvalidId;
        double x = 0.0, y = 0.0, z = 0.0;  // the terrain's own origin on the plate
        int placing = -1;                   // the next click puts a TerrainFeatureKind
        std::optional<std::array<double, 2>> pathStart;  // a path's first click (real mm)
        int dragging = -1;                  // the feature being dragged
        int dragEnd = 0;                    // a path's: 0 all of it, 1 its start, 2 its end
        double grabX = 0.0, grabY = 0.0;    // where it was held, from its point
        std::shared_ptr<const Mesh> mesh, marker;
        int generation = 0;
    };
    TerrainMode m_terrain;
    QTimer* m_terrainTimer = nullptr;
    std::future<void> m_terrainDetail;
    void startTerrain(ObjectId editing);
    void endTerrain();
    void applyTerrain();
    void updateTerrainPreview(bool quick);
    void requestTerrainDetail();
    std::optional<std::array<double, 2>> terrainSpot(const QVector3D& origin, const QVector3D& direction) const;
    void terrainClicked(const QVector3D& origin, const QVector3D& direction);
    void terrainPressed(const QVector3D& origin, const QVector3D& direction);
    void terrainDragged(const QVector3D& origin, const QVector3D& direction);

    bool designing() const { return m_build.active || m_rock.active || m_terrain.active || m_tree.active; }
    void startRock(ObjectId editing);
    void endRock();
    void applyRock();
    void updateRockPreview();
    void requestRockDetail();
    void startTree(ObjectId editing);
    void endTree();
    void applyTree();
    void updateTreePreview();
    void requestTreeDetail();
    void changeSelected();

    void startBuilding(ObjectId editing);
    void endBuilding();
    void applyBuilding();
    void updateBuildingPreview(bool quick);
    void requestDetail();
    std::optional<building::Spot> spotFromRay(const QVector3D& origin, const QVector3D& direction) const;
    void onViewClicked(const QVector3D& origin, const QVector3D& direction);
    void onViewPressed(const QVector3D& origin, const QVector3D& direction);
    void onViewDragged(const QVector3D& origin, const QVector3D& direction);
    void onViewReleased();
    void deleteSelected();
    void refreshCommands();
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
    // The scale picked by the user: what's being designed follows it, and scenery already
    // made can be remade at it.
    void changeScale(const modelscale::Scale& s);
    void checkForUpdates(bool quietly);
    void showAbout();
};

}  // namespace forge::scenery
