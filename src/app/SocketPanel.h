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

// Quick Socket's settings: what the socket takes (a ball or a peg, and its size), how a
// ball is held, the printer and fit, the turn, and relief cuts. The spot is picked in
// the view.
class SocketPanel : public QWidget {
    Q_OBJECT

public:
    explicit SocketPanel(QWidget* parent = nullptr);

    void start(const QString& title, bool editing);
    void setFinish(const SocketFinish& finish);
    SocketFinish finish() const;
    // Whether a spot has been picked (the hint changes).
    void setPlaced(bool placed);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);
    // A magnet: a matching pocket in another part, and which part ("" none yet).
    bool matchingPocket() const;
    void setOtherPart(const QString& name, bool picking);

signals:
    void changed();
    void applyRequested();
    void cancelRequested();
    // Add the ball or peg this socket takes, as a shape of its own.
    void addMatchingRequested();
    // Move the socket to the middle of the face it's on: 0 both ways, 1 up-down, 2 across.
    void centerRequested(int how);
    // A magnet: pick the part against this face, for the matching pocket.
    void pickOtherRequested();

private:
    void showKind();

    QLabel* m_title = nullptr;
    QLabel* m_placed = nullptr;
    QComboBox* m_kind = nullptr;
    QWidget* m_ballBox = nullptr;
    QWidget* m_pegBox = nullptr;
    QDoubleSpinBox* m_ballSize = nullptr;
    QComboBox* m_hold = nullptr;
    QLabel* m_gripLabel = nullptr;
    QDoubleSpinBox* m_grip = nullptr;
    QWidget* m_magnetBox = nullptr;
    QComboBox* m_magnetSize = nullptr;
    QDoubleSpinBox* m_magnetDiameter = nullptr;
    QDoubleSpinBox* m_magnetThickness = nullptr;
    QCheckBox* m_buried = nullptr;
    QDoubleSpinBox* m_cover = nullptr;
    QCheckBox* m_matchingPocket = nullptr;
    QPushButton* m_pickOther = nullptr;
    QLabel* m_otherName = nullptr;
    QWidget* m_cutsRow = nullptr;
    QSpinBox* m_ridges = nullptr;
    QDoubleSpinBox* m_ridgeHeight = nullptr;
    QComboBox* m_shape = nullptr;
    QLabel* m_pegSizeLabel = nullptr;
    QDoubleSpinBox* m_pegSize = nullptr;
    QDoubleSpinBox* m_pegDepth = nullptr;
    QDoubleSpinBox* m_turn = nullptr;
    QComboBox* m_printer = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
    QComboBox* m_cuts = nullptr;
    QWidget* m_cutBox = nullptr;
    QDoubleSpinBox* m_cutWidth = nullptr;
    QDoubleSpinBox* m_cutDepth = nullptr;
    QPushButton* m_matching = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
