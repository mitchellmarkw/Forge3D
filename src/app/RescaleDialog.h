// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace forge::app {

// Tools → Rescale Design: the whole design (or the selected shapes) to another scale
// (1:12 to 1:18, say), a new height or a percentage, with joint parts and joint steps
// kept at their real size if wanted.
class RescaleDialog : public QDialog {
    Q_OBJECT

public:
    // What's there to rescale: everything on the plate, and the selection (height 0:
    // nothing selected), and how many joints each holds.
    struct Choice {
        double height = 0.0;  // mm, top to bottom
        int joints = 0;
        int shapes = 0;
    };
    RescaleDialog(const Choice& everything, const Choice& selected, QWidget* parent = nullptr);

    bool selectedOnly() const;
    double factor() const;
    bool keepJoints() const;
    bool keepWalls() const;
    // "1:12 → 1:18", "to 80 mm tall" or "to 75 %": for the undo step.
    QString describe() const;

private:
    void update();
    void remember() const;
    const Choice& choice() const;

    Choice m_everything, m_selected;
    QComboBox* m_scope = nullptr;
    QComboBox* m_by = nullptr;
    QStackedWidget* m_pages = nullptr;
    QDoubleSpinBox* m_from = nullptr;
    QDoubleSpinBox* m_to = nullptr;
    QDoubleSpinBox* m_height = nullptr;
    QDoubleSpinBox* m_percent = nullptr;
    QCheckBox* m_keepJoints = nullptr;
    QCheckBox* m_keepWalls = nullptr;
    QLabel* m_summary = nullptr;
    QPushButton* m_ok = nullptr;
};

}  // namespace forge::app
