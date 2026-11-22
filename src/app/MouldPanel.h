// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Make Mould: a mould box round a part, for casting resin copies. Which way and where it
// splits (two, three or four pieces), the undercuts, the walls, the registration keys, the
// pour spout and air vents (clicked on the part, or placed for you).
class MouldPanel : public QWidget {
    Q_OBJECT

public:
    explicit MouldPanel(QWidget* parent = nullptr);

    // `part`: the part's box (each cut must go through it).
    void start(const QString& title, bool editing, const MouldFinish& f, const Bounds& part);
    // The cuts, sizes and choices (the spout and vents' places are the window's).
    MouldFinish settings() const;
    void setParting(double at);
    // The first cut's direction and place (Find Best Split).
    void setSplit(int axis, double at);
    // How much of the part's surface the pieces would catch on.
    void setUndercut(double area, double total);
    // Clicking the part puts the spout there (else it adds an air vent).
    bool clickSetsSpout() const;
    void setVentCount(int count);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

protected:
    void resizeEvent(QResizeEvent* event) override;

signals:
    void changed();
    // The first cut turned another way: the window puts it where the part is widest.
    void directionChanged();
    void widestRequested();
    void bestRequested();
    void placeVentsRequested();
    void removeLastRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    void refreshCuts();
    int axis() const;
    int axis2() const;
    void setRange(QDoubleSpinBox* spin, int axis);

    Bounds m_part;
    QLabel* m_title = nullptr;
    QComboBox* m_direction = nullptr;
    QLabel* m_partingLabel = nullptr;
    QDoubleSpinBox* m_parting = nullptr;
    QPushButton* m_best = nullptr;
    QLabel* m_undercut = nullptr;
    QComboBox* m_pieces = nullptr;
    QWidget* m_secondBox = nullptr;
    QComboBox* m_direction2 = nullptr;
    QWidget* m_secondInRow = nullptr;
    QComboBox* m_secondIn = nullptr;
    QDoubleSpinBox* m_parting2 = nullptr;
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
