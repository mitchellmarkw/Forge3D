// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStringList>
#include "forge/core/LimbCut.h"

#include <QWidget>

#include <map>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace forge::app {

// Cut into Parts: the skeleton fitted to a one-piece figure (its balls dragged into the
// joints), then where to cut and what joint goes in each cut.
class CutPartsPanel : public QWidget {
    Q_OBJECT

public:
    // What goes in a cut: the first five, the toy joint recipes (slice 96: each only at its
    // own joints), then the joint standard's joints in turn.
    enum Type {
        JustCut,
        Ball,
        Hinge,
        DoubleHinge,
        Swivel,
        Butterfly,
        DropHip,
        AnkleRocker,
        AbCrunch,
        FirstStandard
    };
    // The recipe a type is (Recipe::None for the others).
    static limbcut::Recipe recipeOf(int type);
    struct Choice {
        bool cut = false;
        int type = JustCut;
        double forward = 0.0;  // the cut turned by hand (limbcut::Tilt), degrees
        double side = 0.0;
    };

    explicit CutPartsPanel(QWidget* parent = nullptr);

    // `standardJoints`: the joint standard in use's joints ("Shoulder ball (Ball socket Ø6)").
    void start(const QString& title, const QStringList& standardJoints);
    // Every joint that can be cut (rig::Joint numbers) and what's chosen there.
    const std::map<int, Choice>& choices() const { return m_choices; }
    int joint() const;
    void setJoint(int joint);
    bool mirror() const;
    bool resin() const;
    // A line under the choices: what will be made at this joint (its sizes).
    void setInfo(const QString& text);
    void setStatus(const QString& text, bool problem);

signals:
    void changed();
    void fitAgainRequested();
    void applyRequested();
    void cancelRequested();

private:
    void showJoint();
    void choose(int joint, const Choice& c);
    void showSummary();

    std::map<int, Choice> m_choices;
    QStringList m_standard;
    bool m_loading = false;
    QCheckBox* m_mirror = nullptr;
    QComboBox* m_joint = nullptr;
    QCheckBox* m_cut = nullptr;
    QComboBox* m_type = nullptr;
    QDoubleSpinBox* m_forward = nullptr;
    QDoubleSpinBox* m_side = nullptr;
    QWidget* m_tiltBox = nullptr;
    QComboBox* m_printer = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_info = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
