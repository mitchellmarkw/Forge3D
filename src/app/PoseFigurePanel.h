// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Rig.h"

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QPushButton;
class QSlider;

namespace forge::app {

// Pose Figure: bones in one sculpted figure, to bend it at its joints. First the
// skeleton is fitted (its balls dragged onto the figure's joints), then the figure is
// posed: a ready pose, a joint's angles typed, or a ball dragged.
class PoseFigurePanel : public QWidget {
    Q_OBJECT

public:
    explicit PoseFigurePanel(QWidget* parent = nullptr);

    // Opens on fitting for a new step, on posing for one being changed. `guide`: there's a
    // figure guide whose pose can be copied.
    void start(const QString& title, bool editing, const PosedFinish& f, bool guide);
    // Opens straight on the proportions (slice 101).
    void showProportions();
    rig::Proportions proportions() const;
    // The settings now (after a ball was dragged, a pose chosen...).
    void setFinish(const PosedFinish& f);
    bool posing() const;
    bool mirror() const;
    int joint() const;
    void setJoint(int joint);
    double blend() const;
    bool feetFlat() const;
    bool onGround() const;
    void setStatus(const QString& text, bool problem);

    // The pose list: 0 as made, then the figure guide's ready poses, then (if there is one)
    // the guide's own pose; "your own" last.
    static constexpr int kAsMade = 0;

signals:
    void stageChanged(bool posing);
    void fitAgainRequested();
    void presetChosen(int index);
    void angleChanged(int joint, int which, double degrees);
    void optionsChanged();
    void proportionsChanged();
    void jointChosen(int joint);
    void applyRequested();
    void cancelRequested();

private:
    void showJoint();

    PosedFinish m_f;
    bool m_loading = false;
    bool m_guide = false;
    QLabel* m_title = nullptr;
    QPushButton* m_fitStage = nullptr;
    QPushButton* m_poseStage = nullptr;
    QPushButton* m_sizeStage = nullptr;
    QWidget* m_sizeBox = nullptr;
    QComboBox* m_sizePreset = nullptr;
    QSlider* m_sizeSlider[kProportionCount] = {};
    QSpinBox* m_sizePercent[kProportionCount] = {};
    void showSizePreset();
    QWidget* m_fitBox = nullptr;
    QWidget* m_poseBox = nullptr;
    QCheckBox* m_mirror = nullptr;
    QDoubleSpinBox* m_blend = nullptr;
    QComboBox* m_preset = nullptr;
    QComboBox* m_joint = nullptr;
    QLabel* m_angleLabel[2] = {nullptr, nullptr};
    QDoubleSpinBox* m_angle[2] = {nullptr, nullptr};
    QCheckBox* m_feetFlat = nullptr;
    QCheckBox* m_onGround = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
