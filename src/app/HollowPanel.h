// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Hollow: wall thickness, drain holes (clicked on the shape, or placed for you), and
// plugs to close them.
class HollowPanel : public QWidget {
    Q_OBJECT

public:
    explicit HollowPanel(QWidget* parent = nullptr);

    void start(const QString& title, bool editing, double thickness, double holeSize);
    double thickness() const;
    double holeSize() const;
    bool plugs() const;
    void setHoleCount(int count);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void placeRequested();
    void removeLastRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QDoubleSpinBox* m_thickness = nullptr;
    QDoubleSpinBox* m_holeSize = nullptr;
    QLabel* m_holes = nullptr;
    QCheckBox* m_plugs = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
