// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace forge::app {

// The Pattern tool's settings: what's repeated (the whole shape, or its last feature),
// and a row, grid or circle of copies.
class PatternPanel : public QWidget {
    Q_OBJECT

public:
    explicit PatternPanel(QWidget* parent = nullptr);

    // feature: the name of the last step's feature, if it can be repeated.
    void start(const QString& title, const std::optional<QString>& feature, const PatternFinish& finish,
               bool editing);
    // Fill in the numbers (keeping what's repeated and the kind of pattern).
    void setNumbers(const PatternFinish& finish);
    PatternFinish finish() const;
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void repeatChanged(bool feature);  // the user switched what's repeated
    void applyRequested();
    void cancelRequested();

private:
    void updateLabels();

    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QComboBox* m_what = nullptr;
    QComboBox* m_kind = nullptr;
    QWidget* m_line = nullptr;    // row / grid
    QComboBox* m_dir1 = nullptr;
    QCheckBox* m_reverse1 = nullptr;
    QSpinBox* m_count1 = nullptr;
    QDoubleSpinBox* m_spacing1 = nullptr;
    QWidget* m_second = nullptr;  // grid
    QComboBox* m_dir2 = nullptr;
    QCheckBox* m_reverse2 = nullptr;
    QSpinBox* m_count2 = nullptr;
    QDoubleSpinBox* m_spacing2 = nullptr;
    QWidget* m_circle = nullptr;
    QComboBox* m_axis = nullptr;
    QDoubleSpinBox* m_centre[3] = {nullptr, nullptr, nullptr};
    QSpinBox* m_count = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QPushButton* m_apply = nullptr;
    bool m_filling = false;
};

}  // namespace forge::app
