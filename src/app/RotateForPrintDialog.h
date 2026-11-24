// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/PrintOrient.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QTableWidget;

namespace forge::app {

// Rotate for Best Print: choose what matters (a fast print, a clean surface, a strong
// part), see how the best way up compares with how it sits now, and turn it.
class RotateForPrintDialog : public QDialog {
    Q_OBJECT

public:
    // `work`: the facts now and at the best way for a goal (all the shapes together).
    RotateForPrintDialog(int shapes, std::function<orient::Choice(orient::Goal, const orient::Settings&)> work,
                         QWidget* parent = nullptr);

    orient::Goal goal() const;
    orient::Settings settings() const;

private:
    void refresh();
    std::function<orient::Choice(orient::Goal, const orient::Settings&)> m_work;
    QComboBox* m_printer = nullptr;
    QComboBox* m_goal = nullptr;
    QCheckBox* m_turn = nullptr;
    QLabel* m_about = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_verdict = nullptr;
};

}  // namespace forge::app
