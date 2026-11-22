// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Make Mould: a two-part mould box round a part, for casting resin copies. Where the
// halves meet, the walls, the registration keys, the pour spout and air vents (clicked on
// the part, or placed for you).
class MouldPanel : public QWidget {
    Q_OBJECT

public:
    explicit MouldPanel(QWidget* parent = nullptr);

    // `low`..`high`: the part's height range (the parting face must be between).
    void start(const QString& title, bool editing, const MouldFinish& f, double low, double high);
    // The sizes and choices (the spout and vents' places are the window's).
    MouldFinish settings() const;
    void setParting(double z);
    // Clicking the part puts the spout there (else it adds an air vent).
    bool clickSetsSpout() const;
    void setVentCount(int count);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void widestRequested();
    void placeVentsRequested();
    void removeLastRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QDoubleSpinBox* m_parting = nullptr;
    QDoubleSpinBox* m_wall = nullptr;
    QCheckBox* m_keys = nullptr;
    QDoubleSpinBox* m_keySize = nullptr;
    QDoubleSpinBox* m_keyFit = nullptr;
    QDoubleSpinBox* m_spoutSize = nullptr;
    QDoubleSpinBox* m_ventSize = nullptr;
    QComboBox* m_click = nullptr;
    QLabel* m_vents = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
