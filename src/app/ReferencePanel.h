// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Reference.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

namespace forge::app {

// Reference Images: a picture for each direction, its size, place and opacity, whether
// it shows and whether it can be clicked; and sizing it from two points of known length.
class ReferencePanel : public QWidget {
    Q_OBJECT

public:
    explicit ReferencePanel(QWidget* parent = nullptr);

    RefView direction() const;
    void setDirection(RefView view);
    // Fills the fields from the direction's picture (none: just the Add button).
    // `taken`: which directions have a picture (marked in the list).
    void showPicture(const ReferenceImage* image, const std::vector<RefView>& taken);

    // What the fields say now.
    double widthMm() const;
    double across() const;
    double up() const;
    double behind() const;
    double turn() const;
    double opacity() const;  // 0..1
    bool shown() const;
    bool active() const;

    // Sizing from two points: the length between them on the model, mm.
    double calibrationLength() const;
    // 0: not picking; 1: waiting for the first point; 2: for the second.
    void setPicking(int stage);
    void setStatus(const QString& text, bool problem);

signals:
    void directionChanged(RefView view);
    void addRequested();
    void removeRequested();
    void lookRequested();
    // A field changed. `dragging`: a slider or spin box still moving (one undo step for
    // the lot); `what` names the change for Undo ("Picture Opacity"...).
    void edited(const QString& what, bool dragging);
    void pickRequested();
    void doneRequested();

private:
    void refreshLength();

    QComboBox* m_direction = nullptr;
    QPushButton* m_add = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_look = nullptr;
    QLabel* m_file = nullptr;
    QWidget* m_settings = nullptr;
    QCheckBox* m_shown = nullptr;
    QCheckBox* m_active = nullptr;
    QSlider* m_opacity = nullptr;
    QLabel* m_opacityValue = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QLabel* m_height = nullptr;
    QDoubleSpinBox* m_across = nullptr;
    QDoubleSpinBox* m_up = nullptr;
    QDoubleSpinBox* m_behind = nullptr;
    QDoubleSpinBox* m_turn = nullptr;
    QDoubleSpinBox* m_length = nullptr;
    QComboBox* m_unit = nullptr;
    QDoubleSpinBox* m_scale = nullptr;
    QLabel* m_modelLength = nullptr;
    QPushButton* m_pick = nullptr;
    QLabel* m_status = nullptr;
    double m_aspect = 1.0;  // height / width of the picture shown
};

}  // namespace forge::app
