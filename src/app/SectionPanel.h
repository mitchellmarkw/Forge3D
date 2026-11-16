// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSlider;

namespace forge::app {

// Section view: where the view is cut open to look inside. Only what's shown changes;
// the model stays whole.
class SectionPanel : public QWidget {
    Q_OBJECT

public:
    explicit SectionPanel(QWidget* parent = nullptr);

    int axis() const;          // 0 = X (left to right), 1 = Y (front to back), 2 = Z (bottom to top)
    double position() const;   // along that axis, plate mm
    bool otherSide() const;    // hide the low side instead of the high side
    // How far the model reaches along the axis (the slider's travel). keepPosition:
    // leave the cut where it is if it's still within reach, else go to the middle.
    void setRange(double low, double high, bool keepPosition);

signals:
    void changed();
    void axisChanged();

private:
    void fromSlider(int value);
    void fromSpin(double value);

    QComboBox* m_axis = nullptr;
    QSlider* m_slider = nullptr;
    QDoubleSpinBox* m_position = nullptr;
    QCheckBox* m_otherSide = nullptr;
    double m_low = -50.0, m_high = 50.0;
    bool m_placed = false;  // the cut has been put somewhere (first time: the middle)
};

}  // namespace forge::app
