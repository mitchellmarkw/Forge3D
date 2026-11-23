// SPDX-License-Identifier: GPL-3.0-or-later
#include "JointAtCutBox.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {

namespace {

#define FORGE_PREFS(name)                                                                                    \
    QSettings name(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"))

const splitjoint::Kind kKinds[] = {splitjoint::Kind::None,        splitjoint::Kind::Ball,
                                   splitjoint::Kind::Hinge,       splitjoint::Kind::DoubleHinge,
                                   splitjoint::Kind::Swivel,      splitjoint::Kind::Magnets};

}  // namespace

JointAtCutBox::JointAtCutBox(const QString& prefsKey, QWidget* parent) : QWidget(parent), m_key(prefsKey)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* kindColumn = new QVBoxLayout;
    kindColumn->setSpacing(0);
    kindColumn->addWidget(new QLabel(tr("Joint at the cut:")));
    m_kind = new QComboBox;
    m_kind->addItem(tr("None"));
    m_kind->addItem(tr("Ball joint"));
    m_kind->addItem(tr("Hinge"));
    m_kind->addItem(tr("Double hinge"));
    m_kind->addItem(tr("Swivel"));
    m_kind->addItem(tr("Magnets"));
    m_kind->setToolTip(tr("A joint where the pieces meet, so they move (or come apart) there:\n"
                          "Ball joint: a snap-in socket in one piece, the ball on a neck on the other\n"
                          "Hinge: a fork in one piece, the other's end made into the tab, and a pin\n"
                          "Double hinge: two forks with a link between them, to bend further\n"
                          "Swivel: a peg turning in a bore\n"
                          "Magnets: a pocket for a magnet in each face"));
    kindColumn->addWidget(m_kind);
    layout->addLayout(kindColumn);
    auto* form = new QFormLayout;
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.0, 30.0);
    m_size->setDecimals(1);
    m_size->setSingleStep(0.5);
    m_size->setSuffix(tr(" mm"));
    m_size->setSpecialValueText(tr("Fit the cut"));
    m_size->setToolTip(tr("The joint's size: \"Fit the cut\" sizes it to how thick the cut is"));
    m_sizeLabel = new QLabel(tr("Size:"));
    form->addRow(m_sizeLabel, m_size);
    m_fit = new QDoubleSpinBox;
    m_fit->setRange(0.0, 1.0);
    m_fit->setDecimals(2);
    m_fit->setSingleStep(0.05);
    m_fit->setSuffix(tr(" mm"));
    m_fit->setToolTip(tr("Room all round the moving parts: 0.1 mm for resin, 0.2 for filament"));
    form->addRow(tr("Joint fit:"), m_fit);
    layout->addLayout(form);
    {
        const FORGE_PREFS(s);
        m_kind->setCurrentIndex(std::clamp(s.value(m_key + QStringLiteral("/joint"), 0).toInt(), 0, 5));
        m_size->setValue(s.value(m_key + QStringLiteral("/jointSize"), 0.0).toDouble());
        m_fit->setValue(s.value(m_key + QStringLiteral("/jointFit"), 0.1).toDouble());
    }
    update();
    const auto remember = [this] {
        update();
        FORGE_PREFS(s);
        s.setValue(m_key + QStringLiteral("/joint"), m_kind->currentIndex());
        s.setValue(m_key + QStringLiteral("/jointSize"), m_size->value());
        s.setValue(m_key + QStringLiteral("/jointFit"), m_fit->value());
        emit changed();
    };
    connect(m_kind, &QComboBox::currentIndexChanged, this, remember);
    connect(m_size, &QDoubleSpinBox::valueChanged, this, remember);
    connect(m_fit, &QDoubleSpinBox::valueChanged, this, remember);
}

void JointAtCutBox::update()
{
    const splitjoint::Kind kind = kKinds[std::clamp(m_kind->currentIndex(), 0, 5)];
    const bool on = kind != splitjoint::Kind::None;
    m_size->setEnabled(on);
    m_fit->setEnabled(on);
    switch (kind) {
    case splitjoint::Kind::Ball: m_sizeLabel->setText(tr("Ball size:")); break;
    case splitjoint::Kind::Hinge:
    case splitjoint::Kind::DoubleHinge: m_sizeLabel->setText(tr("Tab thickness:")); break;
    case splitjoint::Kind::Swivel: m_sizeLabel->setText(tr("Peg size:")); break;
    case splitjoint::Kind::Magnets: m_sizeLabel->setText(tr("Magnet size:")); break;
    default: m_sizeLabel->setText(tr("Size:")); break;
    }
}

splitjoint::Options JointAtCutBox::options() const
{
    splitjoint::Options o;
    o.kind = kKinds[std::clamp(m_kind->currentIndex(), 0, 5)];
    o.size = m_size->value();
    o.fit = m_fit->value();
    return o;
}

}  // namespace forge::app
