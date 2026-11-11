// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Split.h"

#include <QWidget>

class QButtonGroup;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;

namespace forge::app {

// The Split tool's settings: where the cutting plane is, and what to keep.
// It only reports changes; the main window owns the plane and the document.
class SplitPanel : public QWidget {
    Q_OBJECT

public:
    explicit SplitPanel(QWidget* parent = nullptr);

    // editing: moving the cut of an existing half (no "keep" choice).
    void start(const QString& title, bool editing, double tiltX, double tiltY, double offset);
    // How far the plane can move each way (the shape's extent along the plane's direction).
    void setOffsetRange(double minOffset, double maxOffset);
    // The names of the two sides ("top" / "bottom") for the Keep choices.
    void setSideNames(const QString& front, const QString& back);

    double tiltX() const;
    double tiltY() const;
    double offset() const;
    SplitKeep keep() const;

signals:
    void planeChanged();
    void applyRequested();
    void cancelRequested();

private:
    void setTilts(double tiltX, double tiltY);
    void offsetFromSlider(int value);
    void syncSlider();

    QLabel* m_title = nullptr;
    QDoubleSpinBox* m_tiltX = nullptr;
    QDoubleSpinBox* m_tiltY = nullptr;
    QDoubleSpinBox* m_offset = nullptr;
    QSlider* m_slider = nullptr;
    QGroupBox* m_keepGroup = nullptr;
    QRadioButton* m_keepBoth = nullptr;
    QRadioButton* m_keepFront = nullptr;
    QRadioButton* m_keepBack = nullptr;
    QPushButton* m_apply = nullptr;
    double m_min = -10.0;
    double m_max = 10.0;
};

}  // namespace forge::app
