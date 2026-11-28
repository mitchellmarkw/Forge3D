// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/BuildingSpec.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace forge::scenery {

// The building designer's panel (slice 111): the walls (material, sizes), their units
// (bricks, blocks, logs, planks; mortar), the roof, and the doors and windows (added by a
// click on a wall). Real-world sizes; the model's are shown.
class BuildingPanel : public QWidget {
    Q_OBJECT

public:
    explicit BuildingPanel(QWidget* parent = nullptr);
    // `editing`: changing a building already made (Update), or a new one (Add).
    void start(const BuildingSpec& spec, bool editing);
    BuildingSpec spec() const { return m_spec; }
    // From the view: the spec changed there (an opening placed or moved).
    void setSpec(const BuildingSpec& spec);
    int selectedOpening() const { return m_selected; }
    void selectOpening(int index);
    void setPlacing(int kind);  // -1: not placing; else OpeningKind, or kPlaceTower
    static constexpr int kPlaceTower = 2;
    int selectedTower() const { return m_tower; }
    void selectTower(int index);
    void setNotes(const QStringList& notes);
    void setStatus(const QString& text, bool problem);

protected:
    // The wheel scrolls the panel, not a field it happens to pass over (unless that's being typed in).
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void changed();                    // the building's description changed (in the panel)
    void placeRequested(int kind);     // OpeningKind: the next click on a wall places one
    void selectionChanged(int index);  // the opening picked (-1: none)
    void towerSelectionChanged(int index);  // the tower picked (-1: none)
    void applyRequested();
    void cancelRequested();

private:
    BuildingSpec m_spec;
    int m_selected = -1;
    int m_tower = -1;
    bool m_loading = false;
    QCheckBox* m_battlements = nullptr;
    QWidget* m_parapetFields = nullptr;
    QDoubleSpinBox* m_parapet = nullptr;
    QDoubleSpinBox* m_merlon = nullptr;
    QDoubleSpinBox* m_crenel = nullptr;
    QPushButton* m_addTower = nullptr;
    QListWidget* m_towers = nullptr;
    QWidget* m_towerDetails = nullptr;
    QComboBox* m_towerShape = nullptr;
    QDoubleSpinBox* m_towerSize = nullptr;
    QDoubleSpinBox* m_towerHeight = nullptr;
    QComboBox* m_towerTop = nullptr;
    QComboBox* m_towerMaterial = nullptr;
    void fillTowers();
    void showTower();
    void showParapet();

    QLabel* m_title = nullptr;
    QComboBox* m_shape = nullptr;
    QWidget* m_wingFields = nullptr;
    QLabel* m_wingDepthLabel = nullptr;
    QDoubleSpinBox* m_wingDepth = nullptr;
    QLabel* m_wingWidthLabel = nullptr;
    QDoubleSpinBox* m_wingWidth = nullptr;
    QComboBox* m_material = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QDoubleSpinBox* m_height = nullptr;
    QDoubleSpinBox* m_thickness = nullptr;
    QLabel* m_thicknessLabel = nullptr;
    QGroupBox* m_unitsBox = nullptr;
    QLabel* m_unitLengthLabel = nullptr;
    QDoubleSpinBox* m_unitLength = nullptr;
    QLabel* m_unitHeightLabel = nullptr;
    QDoubleSpinBox* m_unitHeight = nullptr;
    QCheckBox* m_mortar = nullptr;
    QLabel* m_jointLabel = nullptr;
    QDoubleSpinBox* m_joint = nullptr;
    QComboBox* m_roof = nullptr;
    QDoubleSpinBox* m_pitch = nullptr;
    QDoubleSpinBox* m_overhang = nullptr;
    QCheckBox* m_roofSeparate = nullptr;
    QWidget* m_roofDetails = nullptr;
    QPushButton* m_addDoor = nullptr;
    QPushButton* m_addWindow = nullptr;
    QLabel* m_placeHint = nullptr;
    QListWidget* m_openings = nullptr;
    QWidget* m_openingDetails = nullptr;
    QDoubleSpinBox* m_openWidth = nullptr;
    QDoubleSpinBox* m_openHeight = nullptr;
    QLabel* m_sillLabel = nullptr;
    QDoubleSpinBox* m_sill = nullptr;
    QDoubleSpinBox* m_smallest = nullptr;
    QLabel* m_size = nullptr;
    QLabel* m_notes = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;

    void load();             // the fields from m_spec
    void readFields();       // m_spec from the fields
    void showMaterial();     // which fields the material has
    void fillOpenings();
    void showOpening();
};

}  // namespace forge::scenery
