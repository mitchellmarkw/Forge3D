// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Join.h"

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Join Parts: the two things picked (on the part that moves, and where it goes) and how
// they sit together (a gap, a slide across, a turn, the moving part flipped round).
class JoinPanel : public QWidget {
    Q_OBJECT

public:
    explicit JoinPanel(QWidget* parent = nullptr);

    void start();
    // What's picked for the part that moves (0) or where it goes (1); `waiting`: the
    // next click picks this one.
    void setPicked(int which, const QString& text, bool waiting);
    void setOffsets(const join::Offsets& offsets);
    join::Offsets offsets() const;
    // Group them after joining, so they move as one.
    bool keepTogether() const;
    void setKeepTogether(bool on);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void pickAgainRequested(int which);
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_picked[2] = {nullptr, nullptr};
    QPushButton* m_again[2] = {nullptr, nullptr};
    QDoubleSpinBox* m_along = nullptr;
    QDoubleSpinBox* m_acrossX = nullptr;
    QDoubleSpinBox* m_acrossY = nullptr;
    QDoubleSpinBox* m_turn = nullptr;
    QCheckBox* m_flip = nullptr;
    QCheckBox* m_together = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
