// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/TerrainSpec.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;

namespace forge::scenery {

// The terrain designer's panel (slice 113): the base (shape, size, edge), the ground's hills,
// and its craters, crags and paths (added by a click, or scattered).
class TerrainPanel : public QWidget {
    Q_OBJECT

public:
    explicit TerrainPanel(QWidget* parent = nullptr);
    void start(const TerrainSpec& spec, bool editing);
    TerrainSpec spec() const { return m_spec; }
    void setSpec(const TerrainSpec& spec);  // from the view (a feature placed or moved)
    int selectedFeature() const { return m_selected; }
    void selectFeature(int index);
    void setPlacing(int kind);  // -1: not placing; else TerrainFeatureKind
    void setStatus(const QString& text, bool problem);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void changed();
    void placeRequested(int kind);
    void selectionChanged(int index);
    void applyRequested();
    void cancelRequested();

private:
    TerrainSpec m_spec;
    int m_selected = -1;
    bool m_loading = false;

    QLabel* m_title = nullptr;
    QComboBox* m_base = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QDoubleSpinBox* m_thickness = nullptr;
    QComboBox* m_edge = nullptr;
    QLabel* m_bevelLabel = nullptr;
    QDoubleSpinBox* m_bevel = nullptr;
    QDoubleSpinBox* m_hillHeight = nullptr;
    QDoubleSpinBox* m_hillSize = nullptr;
    QSlider* m_rough = nullptr;
    QSpinBox* m_roughBox = nullptr;
    QCheckBox* m_levelEdges = nullptr;
    QSpinBox* m_seed = nullptr;
    QPushButton* m_addCrater = nullptr;
    QPushButton* m_addCrag = nullptr;
    QPushButton* m_addPath = nullptr;
    QListWidget* m_features = nullptr;
    QWidget* m_featureDetails = nullptr;
    QLabel* m_featureSizeLabel = nullptr;
    QDoubleSpinBox* m_featureSize = nullptr;
    QLabel* m_featureDepthLabel = nullptr;
    QDoubleSpinBox* m_featureDepth = nullptr;
    QSpinBox* m_craters = nullptr;
    QSpinBox* m_crags = nullptr;
    QSpinBox* m_paths = nullptr;
    QDoubleSpinBox* m_smallest = nullptr;
    QLabel* m_size = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;

    void load();
    void readFields();
    void showFields();
    void fillFeatures();
    void showFeature();
};

}  // namespace forge::scenery
