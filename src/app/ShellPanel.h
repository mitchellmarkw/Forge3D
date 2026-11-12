// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The Shell tool's settings: wall thickness, inside or outside, and the faces picked
// (in the view) to be left open.
class ShellPanel : public QWidget {
    Q_OBJECT

public:
    explicit ShellPanel(QWidget* parent = nullptr);

    void start(const QString& title, bool editing, double thickness, bool outward);
    double thickness() const;
    bool outward() const;
    void setOpenCount(int count);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QDoubleSpinBox* m_thickness = nullptr;
    QCheckBox* m_outward = nullptr;
    QLabel* m_open = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
