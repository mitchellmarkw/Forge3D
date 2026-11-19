// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace forge::app {

// Tools → Scale Calculator: a real-world size, in any unit, at a scale of 1 : X, gives
// the size on the model (a 6 ft person at 1:60 is 30.48 mm). Stays open beside the
// window while you work; remembers what was typed.
class ScaleCalculator : public QDialog {
    Q_OBJECT

public:
    explicit ScaleCalculator(QWidget* parent = nullptr);

private:
    void recalculate();

    QDoubleSpinBox* m_size = nullptr;
    QComboBox* m_unit = nullptr;
    QDoubleSpinBox* m_scale = nullptr;
    QComboBox* m_resultUnit = nullptr;
    QLabel* m_result = nullptr;
    QString m_resultText;  // just the number and unit, for copying
};

}  // namespace forge::app
