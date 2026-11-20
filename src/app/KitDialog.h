// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Kit.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

namespace forge::app {

// Tools → Kit Layout: which parts, and the frame, gates, numbers and joining.
class KitDialog : public QDialog {
    Q_OBJECT

public:
    // `selected`: how many shapes are selected (0: everything on the plate it is).
    KitDialog(int selected, int everything, QWidget* parent = nullptr);

    bool selectedOnly() const;
    kit::Settings settings() const;

private:
    void remember() const;

    QComboBox* m_scope = nullptr;
    QDoubleSpinBox* m_gap = nullptr;
    QDoubleSpinBox* m_bar = nullptr;
    QDoubleSpinBox* m_gate = nullptr;
    QCheckBox* m_numbers = nullptr;
    QDoubleSpinBox* m_numberHeight = nullptr;
    QCheckBox* m_join = nullptr;
    QLineEdit* m_name = nullptr;
    QCheckBox* m_includeName = nullptr;
};

}  // namespace forge::app
