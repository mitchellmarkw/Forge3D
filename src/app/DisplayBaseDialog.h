// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/DisplayBase.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Make a Display Base (slice 76): the shape, size and pegs of a base for the figure as posed.
class DisplayBaseDialog : public QDialog {
    Q_OBJECT
public:
    DisplayBaseDialog(const displaybase::Settings& start, bool holesInFeet, double holeDepth, QWidget* parent = nullptr);

    displaybase::Settings settings() const;
    // Also make the holes in the feet (when the base has pegs).
    bool holesInFeet() const;
    double holeDepth() const;

private:
    void refresh();

    QComboBox* m_shape = nullptr;
    QDoubleSpinBox* m_margin = nullptr;
    QDoubleSpinBox* m_thickness = nullptr;
    QDoubleSpinBox* m_bevel = nullptr;
    QComboBox* m_kind = nullptr;
    QDoubleSpinBox* m_pegSize = nullptr;
    QDoubleSpinBox* m_pegHeight = nullptr;
    QLabel* m_pegHeightLabel = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
    QLabel* m_fitLabel = nullptr;
    QCheckBox* m_holes = nullptr;
    QDoubleSpinBox* m_holeDepth = nullptr;
    QLabel* m_holeDepthLabel = nullptr;
    QLabel* m_note = nullptr;
    QPushButton* m_ok = nullptr;
};

}  // namespace forge::app
