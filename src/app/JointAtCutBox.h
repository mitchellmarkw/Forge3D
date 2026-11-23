// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/SplitJoint.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace forge::app {

// "Joint at the cut": which joint to put in where a split's pieces meet (none, a ball
// joint, a hinge...), its size (or to fit the cut) and its fit. Remembered under
// `prefsKey`/joint... between runs.
class JointAtCutBox : public QWidget {
    Q_OBJECT

public:
    explicit JointAtCutBox(const QString& prefsKey, QWidget* parent = nullptr);

    splitjoint::Options options() const;
    bool wanted() const { return options().kind != splitjoint::Kind::None; }

signals:
    void changed();

private:
    void update();
    QString m_key;
    QComboBox* m_kind = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
};

}  // namespace forge::app
