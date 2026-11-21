// SPDX-License-Identifier: GPL-3.0-or-later
#include "CutPartsPanel.h"

#include "forge/core/LimbCut.h"
#include "forge/core/Rig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace forge::app {

namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

// A label with its combobox right under it (the label resting on top of the box).
QWidget* labelled(const QString& text, QComboBox* box)
{
    auto* holder = new QWidget;
    auto* column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(box);
    return holder;
}

QLabel* wrapped(const QString& text)
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}

}  // namespace

CutPartsPanel::CutPartsPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    auto* hint = wrapped(tr("Cuts a figure sculpted in one piece at its joints, square to each limb, and puts a "
                            "joint in each cut. Best on a figure standing straight, facing the front, arms away "
                            "from its sides."));
    hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(hint);

    auto* fitBox = new QGroupBox(tr("1. Check the skeleton"));
    auto* fit = new QVBoxLayout(fitBox);
    fit->addWidget(wrapped(tr("Drag the balls into the middle of each joint you'll cut at (look from the front, "
                              "then the side). Each cut goes through its ball, square to the limb.")));
    m_mirror = new QCheckBox(tr("Move both sides together"));
    fit->addWidget(m_mirror);
    auto* again = new QPushButton(tr("Place It Again"));
    again->setToolTip(tr("Put the skeleton back where Forge3D first placed it in the figure"));
    fit->addWidget(again);
    layout->addWidget(fitBox);

    auto* cutBox = new QGroupBox(tr("2. Where to cut"));
    auto* cuts = new QVBoxLayout(cutBox);
    auto* ready = new QHBoxLayout;
    auto* usual = new QPushButton(tr("Usual Figure"));
    usual->setToolTip(tr("Ball joints at the neck, shoulders and hips; hinges at the elbows and knees"));
    auto* none = new QPushButton(tr("No Cuts"));
    ready->addWidget(usual);
    ready->addWidget(none);
    cuts->addLayout(ready);
    m_joint = new QComboBox;
    for (rig::Joint j : limbcut::cuttable())
        m_joint->addItem(tr(rig::title(j)), int(j));
    m_joint->setToolTip(tr("Or click a ball in the view"));
    cuts->addWidget(labelled(tr("Joint:"), m_joint));
    m_cut = new QCheckBox(tr("Cut here"));
    cuts->addWidget(m_cut);
    m_type = new QComboBox;
    m_type->setToolTip(tr("What goes in the cut. A ball joint puts the ball on the piece nearer the middle and "
                          "the socket in the limb; a hinge puts the fork in the piece nearer the middle and makes "
                          "the tab from the limb's end. Sized to the limb, or from your joint standard."));
    cuts->addWidget(labelled(tr("Joint type:"), m_type));
    m_info = wrapped(QString());
    m_info->setStyleSheet(QStringLiteral("color: palette(mid);"));
    cuts->addWidget(m_info);
    m_printer = new QComboBox;
    m_printer->addItem(tr("Resin"));
    m_printer->addItem(tr("FDM (filament)"));
    m_printer->setToolTip(tr("The joints' fit is your printer's (from the Tolerance Test Print, if you made one)"));
    cuts->addWidget(labelled(tr("Printer:"), m_printer));
    m_summary = wrapped(QString());
    cuts->addWidget(m_summary);
    layout->addWidget(cutBox);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton(tr("Cancel"));
    auto* apply = new QPushButton(tr("Apply"));
    apply->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(apply);
    layout->addLayout(buttons);
    layout->addStretch(1);

    const QSettings prefs = preferences();
    m_mirror->setChecked(prefs.value(QStringLiteral("cutParts/mirror"), true).toBool());
    m_printer->setCurrentIndex(prefs.value(QStringLiteral("cutParts/resin"), true).toBool() ? 0 : 1);
    connect(m_mirror, &QCheckBox::toggled, this,
            [](bool on) { preferences().setValue(QStringLiteral("cutParts/mirror"), on); });
    connect(m_printer, &QComboBox::currentIndexChanged, this, [this](int i) {
        preferences().setValue(QStringLiteral("cutParts/resin"), i == 0);
        emit changed();
    });
    connect(again, &QPushButton::clicked, this, &CutPartsPanel::fitAgainRequested);
    connect(m_joint, &QComboBox::currentIndexChanged, this, [this] {
        showJoint();
        if (!m_loading)
            emit changed();
    });
    const auto edited = [this] {
        if (m_loading)
            return;
        Choice c;
        c.cut = m_cut->isChecked();
        c.type = m_type->currentIndex();
        choose(joint(), c);
        showJoint();
        emit changed();
    };
    connect(m_cut, &QCheckBox::toggled, this, edited);
    connect(m_type, &QComboBox::currentIndexChanged, this, edited);
    connect(usual, &QPushButton::clicked, this, [this] {
        using J = rig::Joint;
        for (auto& [j, c] : m_choices)
            c = Choice{};
        for (J j : {J::Neck, J::LeftShoulder, J::RightShoulder, J::LeftHip, J::RightHip})
            m_choices[int(j)] = {true, Ball};
        for (J j : {J::LeftElbow, J::RightElbow, J::LeftKnee, J::RightKnee})
            m_choices[int(j)] = {true, Hinge};
        showJoint();
        emit changed();
    });
    connect(none, &QPushButton::clicked, this, [this] {
        for (auto& [j, c] : m_choices)
            c = Choice{};
        showJoint();
        emit changed();
    });
    connect(apply, &QPushButton::clicked, this, &CutPartsPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &CutPartsPanel::cancelRequested);
}

void CutPartsPanel::start(const QString& title, const QStringList& standardJoints)
{
    m_title->setText(title);
    m_standard = standardJoints;
    m_choices.clear();
    for (rig::Joint j : limbcut::cuttable())
        m_choices[int(j)] = Choice{};
    m_loading = true;
    m_type->clear();
    m_type->addItems({tr("Just cut"), tr("Ball joint"), tr("Hinge"), tr("Double hinge"), tr("Swivel")});
    for (const QString& s : standardJoints)
        m_type->addItem(tr("Standard: %1").arg(s));
    m_joint->setCurrentIndex(0);
    m_loading = false;
    showJoint();
    setStatus({}, false);
}

int CutPartsPanel::joint() const
{
    return m_joint->currentData().toInt();
}

void CutPartsPanel::setJoint(int joint)
{
    const int i = m_joint->findData(joint);
    if (i >= 0)
        m_joint->setCurrentIndex(i);
}

bool CutPartsPanel::mirror() const
{
    return m_mirror->isChecked();
}

bool CutPartsPanel::resin() const
{
    return m_printer->currentIndex() == 0;
}

void CutPartsPanel::choose(int joint, const Choice& c)
{
    m_choices[joint] = c;
    // The other side the same, when both sides go together.
    if (m_mirror->isChecked()) {
        for (rig::Joint j : limbcut::cuttable()) {
            const rig::Point p = rig::pivot(rig::Joint(joint));
            if (int(j) != joint && rig::pivot(j) == rig::mirrored(p))
                m_choices[int(j)] = c;
        }
    }
}

void CutPartsPanel::showJoint()
{
    const Choice c = m_choices[joint()];
    const bool was = m_loading;
    m_loading = true;
    m_cut->setChecked(c.cut);
    m_type->setCurrentIndex(std::min(c.type, m_type->count() - 1));
    m_type->setEnabled(c.cut);
    m_loading = was;
    showSummary();
}

void CutPartsPanel::showSummary()
{
    QStringList lines;
    for (rig::Joint j : limbcut::cuttable()) {
        const Choice& c = m_choices[int(j)];
        if (c.cut)
            lines << tr("%1: %2").arg(tr(rig::title(j)), m_type->itemText(c.type));
    }
    m_summary->setText(lines.isEmpty() ? tr("No cuts chosen yet: try Usual Figure, or pick a joint and tick Cut here.")
                                       : tr("Cuts (%1):\n%2").arg(lines.size()).arg(lines.join(QLatin1Char('\n'))));
}

void CutPartsPanel::setInfo(const QString& text)
{
    m_info->setText(text);
    m_info->setVisible(!text.isEmpty());
}

void CutPartsPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
