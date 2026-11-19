// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Tolerance.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace forge::app {

// Tools → Tolerance Test Print: make bars of holes, cups and pin holes at stepped fits
// (and the pieces to try in them), then, once printed, record the fit that worked best
// for each. Those become the starting fits for Quick Socket and Quick Hinge.
class ToleranceDialog : public QDialog {
    Q_OBJECT

public:
    explicit ToleranceDialog(QWidget* parent = nullptr);

signals:
    void addRequested(const forge::tolerance::Settings& settings);

private:
    void showRecorded();
    void showFits();

    QCheckBox* m_pegs = nullptr;
    QCheckBox* m_balls = nullptr;
    QCheckBox* m_pins = nullptr;
    QDoubleSpinBox* m_pegSize = nullptr;
    QDoubleSpinBox* m_pegDepth = nullptr;
    QDoubleSpinBox* m_ballSize = nullptr;
    QDoubleSpinBox* m_pinSize = nullptr;
    QDoubleSpinBox* m_from = nullptr;
    QDoubleSpinBox* m_to = nullptr;
    QDoubleSpinBox* m_step = nullptr;
    QLabel* m_fitsText = nullptr;
    QComboBox* m_printer = nullptr;
    QDoubleSpinBox* m_best[3] = {};
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
