// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Pose.h"

#include <QWidget>

#include <array>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;

namespace forge::app {

// Pose Test: the figure's joints, the chosen one's angles, how far it turns before parts
// hit, the part held still, and poses saved by name. Nothing here changes the parts.
class PosePanel : public QWidget {
    Q_OBJECT

public:
    explicit PosePanel(QWidget* parent = nullptr);

    void setJoints(const QStringList& names);
    int joint() const;
    // The chosen joint (-1: none), its kind and angles.
    void showJoint(int index, pose::JointKind kind, const std::array<double, 3>& angles);
    std::array<double, 3> angles() const;
    void setHold(const QString& name);
    // What hits what ("" while checking: says so).
    void setHits(const QString& text, bool problem);
    void setRange(const QString& text);
    void setPoses(const QStringList& names, int current);
    void setStatus(const QString& text, bool problem);

signals:
    void jointChosen(int index);
    // An angle changed; `dragging`: a slider still moving.
    void anglesEdited(bool dragging);
    void rangeRequested();
    void straightenRequested();
    void buildPoseRequested();
    void holdRequested();
    void poseChosen(int index);
    void savePoseRequested();
    void deletePoseRequested();
    void doneRequested();

private:
    QListWidget* m_joints = nullptr;
    QWidget* m_angleBox = nullptr;
    QLabel* m_angleLabels[3] = {};
    QDoubleSpinBox* m_angle[3] = {};
    QSlider* m_slider = nullptr;
    QPushButton* m_range = nullptr;
    QLabel* m_rangeText = nullptr;
    QLabel* m_hold = nullptr;
    QLabel* m_hits = nullptr;
    QComboBox* m_poses = nullptr;
    QPushButton* m_deletePose = nullptr;
    QLabel* m_status = nullptr;
    pose::JointKind m_kind = pose::JointKind::Hinge;
};

}  // namespace forge::app
