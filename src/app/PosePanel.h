// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Pose.h"

#include <QWidget>

#include <array>

class QCheckBox;
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
    // Make Room: the angle to turn to (which of the joint's angles), what to cut it from,
    // and the clearance.
    double roomTarget() const;
    int roomWhich() const;
    bool roomFromTurning() const;
    double roomClearance() const;
    void setRoomBusy(bool busy);
    // Will it stand? Shown (the centre of weight, where it touches the ground), what it
    // says, and the stand peg's size.
    bool showBalance() const;
    void setShowBalance(bool on);
    void setBalance(const QString& text, bool problem);
    double standPegSize() const;
    double standPegDepth() const;

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
    void makeRoomRequested();
    void balanceToggled(bool on);
    void standPegsRequested();

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
    QWidget* m_roomBox = nullptr;
    QWidget* m_roomWhichRow = nullptr;
    QComboBox* m_roomWhich = nullptr;
    QDoubleSpinBox* m_roomTarget = nullptr;
    QComboBox* m_roomFrom = nullptr;
    QDoubleSpinBox* m_roomClearance = nullptr;
    QPushButton* m_roomButton = nullptr;
    QCheckBox* m_showBalance = nullptr;
    QLabel* m_balance = nullptr;
    QDoubleSpinBox* m_standPeg = nullptr;
    QDoubleSpinBox* m_standDepth = nullptr;
    QWidget* m_standBox = nullptr;
};

}  // namespace forge::app
