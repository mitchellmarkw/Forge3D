// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

class StandardRow;

// Quick Hinge's settings: the kind of joint (single or double hinge, swivel), how the
// other half is made, its sizes, the pin, which way it bends, the printer and fit. The
// spot is picked in the view.
class HingePanel : public QWidget {
    Q_OBJECT

public:
    // Its boxes that can follow the design variables (slice 81).
    void attachFormulas(class FormulaFields* fields);
    explicit HingePanel(QWidget* parent = nullptr);

    // `editing`: changing a hinge already made (its kind and other half stay as they are).
    void start(const QString& title, bool editing);
    void setFinish(const HingeFinish& finish);
    HingeFinish finish() const;
    // The other half made from another part (else added as a shape of its own; a double
    // hinge's link always is, and this says whether the other part gets its fork too).
    bool otherFromPart() const;
    void setOtherFromPart(bool on);
    // The other part's name ("" while none is picked), and whether it's being picked.
    void setOtherPart(const QString& name, bool picking);
    void setPlaced(bool placed);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);
    // The joint standard's joints to start from (Tools → Joint Standards).
    StandardRow* standardRow() const { return m_standardRow; }

signals:
    void changed();
    void applyRequested();
    void cancelRequested();
    void pickOtherRequested();
    // Move the spot to the middle of the face it's on: 0 both ways, 1 up-down, 2 across.
    void centerRequested(int how);
    // Editing: add the tab, link or pins again (for these settings), as shapes.
    void addPiecesRequested();

private:
    void showKind();

    bool m_editing = false;
    QLabel* m_title = nullptr;
    StandardRow* m_standardRow = nullptr;
    QLabel* m_placed = nullptr;
    QComboBox* m_kind = nullptr;
    QLabel* m_kindHelp = nullptr;
    QComboBox* m_other = nullptr;
    QWidget* m_otherBox = nullptr;
    QPushButton* m_pickOther = nullptr;
    QLabel* m_otherName = nullptr;
    QLabel* m_widthLabel = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QLabel* m_barrelLabel = nullptr;
    QDoubleSpinBox* m_barrel = nullptr;
    QLabel* m_gapLabel = nullptr;
    QDoubleSpinBox* m_gap = nullptr;
    QLabel* m_lipLabel = nullptr;
    QDoubleSpinBox* m_lip = nullptr;
    QWidget* m_pinBox = nullptr;
    QComboBox* m_pin = nullptr;
    QDoubleSpinBox* m_pinSize = nullptr;
    QCheckBox* m_stiff = nullptr;
    QSpinBox* m_clicks = nullptr;
    QDoubleSpinBox* m_clickDepth = nullptr;
    QWidget* m_swingBox = nullptr;
    QComboBox* m_swing = nullptr;
    QDoubleSpinBox* m_turn = nullptr;
    QDoubleSpinBox* m_reach = nullptr;
    QComboBox* m_printer = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
    QPushButton* m_addPieces = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
    double m_lastPinSize = 2.0;  // put back when leaving Filament
};

}  // namespace forge::app
