// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The Thread tool's settings: the round face picked (in the view), its size (a standard
// metric one, or any), the pitch, the fit for printing, and which way it turns.
class ThreadPanel : public QWidget {
    Q_OBJECT

public:
    explicit ThreadPanel(QWidget* parent = nullptr);

    void start(const QString& title, bool editing);
    // The face picked: what it is (e.g. "A hole, Ø 5.00 mm"), or none.
    void setPicked(const QString& text);
    // Sets the size (choosing the standard one if it is one) and the rest.
    void setValues(double size, double pitch, double fit, bool leftHand);
    double size() const;
    double pitch() const;
    double fit() const;
    bool leftHand() const;
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void applyRequested();
    void cancelRequested();

private:
    void sizeChosen(int index);

    QLabel* m_title = nullptr;
    QLabel* m_picked = nullptr;
    QComboBox* m_standard = nullptr;  // the metric sizes, then "Other size"
    QDoubleSpinBox* m_size = nullptr;
    QDoubleSpinBox* m_pitch = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
    QCheckBox* m_leftHand = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
