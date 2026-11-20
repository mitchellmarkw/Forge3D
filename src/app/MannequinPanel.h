// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mannequin.h"

#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The figure guide: a posable mannequin on the plate to build a figure to. Its size (a
// figure line's height, or typed), proportions in heads, place and pose. Never printed.
class MannequinPanel : public QWidget {
    Q_OBJECT

public:
    explicit MannequinPanel(QWidget* parent = nullptr);

    // What's in the document now (none: only "Add" shows).
    void setMannequin(const std::optional<Mannequin>& m);
    void setStatus(const QString& text, bool problem);

    // A new guide with the remembered size and proportions, standing at the plate's middle.
    Mannequin fresh() const;

signals:
    void addRequested();
    void removeRequested();
    // A change; `label` names the undo step (changes in a row with the same label merge).
    void changed(const forge::Mannequin& m, const QString& label);
    void doneRequested();

private:
    void edited(const QString& label);
    void showJoint();
    void remember() const;

    std::optional<Mannequin> m_m;
    bool m_loading = false;
    QWidget* m_controls = nullptr;
    QPushButton* m_add = nullptr;
    QCheckBox* m_shown = nullptr;
    QComboBox* m_size = nullptr;
    QDoubleSpinBox* m_height = nullptr;
    QComboBox* m_proportions = nullptr;
    QDoubleSpinBox* m_heads = nullptr;
    QDoubleSpinBox* m_x = nullptr;
    QDoubleSpinBox* m_y = nullptr;
    QDoubleSpinBox* m_turn = nullptr;
    QComboBox* m_preset = nullptr;
    QComboBox* m_joint = nullptr;
    QLabel* m_angleLabel[2] = {nullptr, nullptr};
    QDoubleSpinBox* m_angle[2] = {nullptr, nullptr};
    QLabel* m_headSize = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
