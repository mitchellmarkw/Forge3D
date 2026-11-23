// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QWidget;

namespace forge::app {

class StandardRow;

// Hand Ports: a peg on an arm (or an accessory), the socket for it in a hand, or a grip
// hole through a hand sized to a handle. Sizes from the joint standard, or set here.
class PortPanel : public QWidget {
    Q_OBJECT

public:
    // Its boxes that can follow the design variables (slice 81).
    void attachFormulas(class FormulaFields* fields);
    enum class What { Peg, Socket, Grip };

    explicit PortPanel(QWidget* parent = nullptr);

    // Editing: what it is can't change.
    void start(const QString& title, bool editing, What what, const PortFinish& f);
    What what() const;
    // The peg or grip's sizes (the spot is the window's).
    PortFinish finish() const;
    // Sizes from a standard's joint.
    void setFinish(const PortFinish& f);
    // The socket in the hand: its fit and the printer it's for.
    double fit() const;
    bool resin() const;
    // Grip: clicking the part sets the way out (else the way in).
    bool clickSetsExit() const;
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);
    StandardRow* standardRow() const { return m_standard; }

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void changed();
    void straightRequested();
    void applyRequested();
    void cancelRequested();

private:
    void refresh();
    void fitHeight();

    QLabel* m_title = nullptr;
    StandardRow* m_standard = nullptr;
    QComboBox* m_what = nullptr;
    QWidget* m_pegBox = nullptr;
    QComboBox* m_shape = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QDoubleSpinBox* m_length = nullptr;
    QComboBox* m_end = nullptr;
    QWidget* m_ballRow = nullptr;
    QDoubleSpinBox* m_ball = nullptr;
    QDoubleSpinBox* m_turn = nullptr;
    QWidget* m_socketBox = nullptr;
    QComboBox* m_printer = nullptr;
    QDoubleSpinBox* m_socketFit = nullptr;
    QWidget* m_gripBox = nullptr;
    QDoubleSpinBox* m_handle = nullptr;
    QDoubleSpinBox* m_gripFit = nullptr;
    QComboBox* m_click = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
