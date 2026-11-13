// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace forge::app {

// The Hole tool's settings: the kind of hole, its size (a standard metric size or typed),
// how deep, and the spots (clicked on the face, typed here, or placed in a sketch).
class HolePanel : public QWidget {
    Q_OBJECT

public:
    explicit HolePanel(QWidget* parent = nullptr);

    void start(const QString& title, const HoleSpec& hole, bool throughAll, bool editing);
    // The settings (without the spots).
    HoleSpec settings() const;
    bool throughAll() const;
    // The spots, in face coordinates (mm from the face's middle).
    void setSpots(const std::vector<sketch::Point>& spots, bool placed);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void spotEdited(int index, double x, double y);
    void removeSpotRequested(int index);
    void sketchRequested();
    void applyRequested();
    void cancelRequested();

private:
    void sizeChosen();
    void typedSize();
    void updateLabels();

    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QComboBox* m_kind = nullptr;
    QComboBox* m_size = nullptr;
    QLabel* m_fitLabel = nullptr;
    QComboBox* m_fit = nullptr;
    QDoubleSpinBox* m_diameter = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QLabel* m_depthLabel = nullptr;
    QCheckBox* m_throughAll = nullptr;
    QLabel* m_headDiameterLabel = nullptr;
    QDoubleSpinBox* m_headDiameter = nullptr;
    QLabel* m_headDepthLabel = nullptr;
    QDoubleSpinBox* m_headDepth = nullptr;
    QLabel* m_sinkDiameterLabel = nullptr;
    QDoubleSpinBox* m_sinkDiameter = nullptr;
    QLabel* m_sinkAngleLabel = nullptr;
    QDoubleSpinBox* m_sinkAngle = nullptr;
    QTableWidget* m_spots = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_sketch = nullptr;
    QPushButton* m_apply = nullptr;
    bool m_filling = false;  // setting values ourselves: not the user typing
};

}  // namespace forge::app
