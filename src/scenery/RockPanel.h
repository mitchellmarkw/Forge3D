// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/RockSpec.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

namespace forge::scenery {

// The rock designer's panel (slice 112): the kind of rock, its real size, how rough, its
// layers, how deep it's sunk, a pile's rocks, and a new one of the same (its seed).
class RockPanel : public QWidget {
    Q_OBJECT

public:
    explicit RockPanel(QWidget* parent = nullptr);
    void start(const RockSpec& spec, bool editing);
    RockSpec spec() const { return m_spec; }
    void setStatus(const QString& text, bool problem);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void changed();
    void applyRequested();
    void cancelRequested();

private:
    RockSpec m_spec;
    bool m_loading = false;
    QLabel* m_title = nullptr;
    QComboBox* m_kind = nullptr;
    QDoubleSpinBox* m_length = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QDoubleSpinBox* m_height = nullptr;
    QSlider* m_rough = nullptr;
    QSpinBox* m_roughBox = nullptr;
    QCheckBox* m_layers = nullptr;
    QLabel* m_layerLabel = nullptr;
    QDoubleSpinBox* m_layerThickness = nullptr;
    QSpinBox* m_sunk = nullptr;
    QLabel* m_countLabel = nullptr;
    QSpinBox* m_count = nullptr;
    QSpinBox* m_seed = nullptr;
    QDoubleSpinBox* m_smallest = nullptr;
    QLabel* m_size = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
    void load();
    void readFields();
    void showKind();
};

}  // namespace forge::scenery
