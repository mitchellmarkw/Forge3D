// SPDX-License-Identifier: GPL-3.0-or-later
#include "PoseFigurePanel.h"

#include "forge/core/Mannequin.h"
#include "forge/core/Rig.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

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

PoseFigurePanel::PoseFigurePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Bends a figure sculpted in one piece at its joints, the surface following. Best "
                                 "on a figure standing straight, facing the front, with its arms away from its "
                                 "sides (an A or T pose).")));

    // The two stages.
    auto* stages = new QHBoxLayout;
    m_fitStage = new QPushButton(tr("1. Fit the Skeleton"));
    m_poseStage = new QPushButton(tr("2. Pose"));
    auto* group = new QButtonGroup(this);
    for (QPushButton* b : {m_fitStage, m_poseStage}) {
        b->setCheckable(true);
        group->addButton(b);
        stages->addWidget(b);
    }
    group->setExclusive(true);
    layout->addLayout(stages);

    // Fitting.
    m_fitBox = new QGroupBox(tr("Fit the skeleton"));
    auto* fit = new QVBoxLayout(m_fitBox);
    fit->addWidget(wrapped(tr("Drag the balls onto the figure's joints: the middle of each elbow, wrist, knee and "
                              "ankle, and the hips. Look from the front, then from the side.")));
    m_mirror = new QCheckBox(tr("Move both sides together"));
    m_mirror->setToolTip(tr("Dragging a ball on one side moves its partner on the other side the same way"));
    fit->addWidget(m_mirror);
    auto* blendForm = new QFormLayout;
    m_blend = new QDoubleSpinBox;
    m_blend->setRange(0.1, 200.0);
    m_blend->setDecimals(1);
    m_blend->setSingleStep(0.5);
    m_blend->setSuffix(tr(" mm"));
    m_blend->setKeyboardTracking(false);
    m_blend->setToolTip(tr("How far a bend spreads either side of a joint: more is softer and rounder, less keeps "
                           "the bend tight to the joint"));
    blendForm->addRow(tr("Bends spread:"), m_blend);
    fit->addLayout(blendForm);
    auto* again = new QPushButton(tr("Place It Again"));
    again->setToolTip(tr("Put the skeleton back where Forge3D first placed it in the figure"));
    fit->addWidget(again);
    layout->addWidget(m_fitBox);

    // Posing.
    m_poseBox = new QGroupBox(tr("Pose"));
    auto* pose = new QVBoxLayout(m_poseBox);
    pose->addWidget(wrapped(tr("Drag a ball to bend the joint above it, or choose a joint and type its angles.")));
    m_preset = new QComboBox;
    m_preset->setToolTip(tr("Start from a ready pose (the Figure Guide's), then change any joint"));
    pose->addWidget(labelled(tr("Pose:"), m_preset));
    m_joint = new QComboBox;
    for (int j = 0; j < kRigJointCount; ++j)
        m_joint->addItem(tr(rig::title(rig::Joint(j))));
    pose->addWidget(labelled(tr("Joint:"), m_joint));
    auto* angles = new QFormLayout;
    for (int k = 0; k < 2; ++k) {
        m_angleLabel[k] = new QLabel;
        m_angle[k] = new QDoubleSpinBox;
        m_angle[k]->setDecimals(1);
        m_angle[k]->setSingleStep(5.0);
        m_angle[k]->setSuffix(tr("°"));
        m_angle[k]->setKeyboardTracking(false);
        angles->addRow(m_angleLabel[k], m_angle[k]);
    }
    pose->addLayout(angles);
    m_feetFlat = new QCheckBox(tr("Feet flat on the ground"));
    m_feetFlat->setToolTip(tr("The feet keep their angle as the legs bend (the ankles' angles aren't used)"));
    pose->addWidget(m_feetFlat);
    m_onGround = new QCheckBox(tr("Keep it standing on the plate"));
    m_onGround->setToolTip(tr("Moved up or down so its lowest point stays where it was (bent legs bring it down)"));
    pose->addWidget(m_onGround);
    layout->addWidget(m_poseBox);

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

    m_mirror->setChecked(preferences().value(QStringLiteral("poseFigure/mirror"), true).toBool());
    connect(m_mirror, &QCheckBox::toggled, this,
            [](bool on) { preferences().setValue(QStringLiteral("poseFigure/mirror"), on); });

    connect(group, &QButtonGroup::buttonToggled, this, [this](QAbstractButton* b, bool on) {
        if (!on)
            return;
        const bool posing = b == m_poseStage;
        m_fitBox->setVisible(!posing);
        m_poseBox->setVisible(posing);
        if (!m_loading)
            emit stageChanged(posing);
    });
    connect(again, &QPushButton::clicked, this, &PoseFigurePanel::fitAgainRequested);
    connect(m_blend, &QDoubleSpinBox::valueChanged, this, [this] {
        if (!m_loading)
            emit optionsChanged();
    });
    for (QCheckBox* box : {m_feetFlat, m_onGround})
        connect(box, &QCheckBox::toggled, this, [this] {
            if (m_loading)
                return;
            showJoint();
            emit optionsChanged();
        });
    connect(m_preset, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (!m_loading && i < m_preset->count() - 1)
            emit presetChosen(i);
    });
    connect(m_joint, &QComboBox::currentIndexChanged, this, [this](int j) {
        showJoint();
        if (!m_loading)
            emit jointChosen(j);
    });
    for (int k = 0; k < 2; ++k)
        connect(m_angle[k], &QDoubleSpinBox::valueChanged, this, [this, k](double v) {
            if (!m_loading)
                emit angleChanged(m_joint->currentIndex(), k, v);
        });
    connect(apply, &QPushButton::clicked, this, &PoseFigurePanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &PoseFigurePanel::cancelRequested);
}

void PoseFigurePanel::start(const QString& title, bool editing, const PosedFinish& f, bool guide)
{
    m_title->setText(title);
    m_guide = guide;
    m_loading = true;
    m_preset->clear();
    m_preset->addItem(tr("As it was made"));
    for (int p = 0; p < mannequin::kPosePresetCount; ++p)
        m_preset->addItem(tr(mannequin::title(mannequin::PosePreset(p))));
    if (guide)
        m_preset->addItem(tr("The Figure Guide's pose"));
    m_preset->addItem(tr("Your own"));
    (editing ? m_poseStage : m_fitStage)->setChecked(true);
    m_fitBox->setVisible(!editing);
    m_poseBox->setVisible(editing);
    m_joint->setCurrentIndex(int(rig::Joint::LeftElbow));
    m_loading = false;
    setFinish(f);
    setStatus({}, false);
}

void PoseFigurePanel::setFinish(const PosedFinish& f)
{
    m_f = f;
    m_loading = true;
    m_blend->setValue(f.blend);
    m_feetFlat->setChecked(f.feetFlat);
    m_onGround->setChecked(f.onGround);
    // Which pose it is, if it's one in the list.
    const auto same = [&](const rig::Angles& a) {
        for (int j = 0; j < kRigJointCount; ++j) {
            if (f.feetFlat && (rig::Joint(j) == rig::Joint::LeftAnkle || rig::Joint(j) == rig::Joint::RightAnkle))
                continue;
            for (int k = 0; k < 2; ++k)
                if (std::abs(a[std::size_t(j)][std::size_t(k)] - f.angles[std::size_t(j)][std::size_t(k)]) > 0.05)
                    return false;
        }
        return true;
    };
    int preset = m_preset->count() - 1;
    if (same(rig::restAngles(f.points)))
        preset = kAsMade;
    else
        for (int p = 0; p < mannequin::kPosePresetCount; ++p)
            if (same(rig::fromMannequin(mannequin::preset(mannequin::PosePreset(p)))))
                preset = p + 1;
    if (m_preset->count() > 0)
        m_preset->setCurrentIndex(preset);
    m_loading = false;
    showJoint();
}

void PoseFigurePanel::showJoint()
{
    const auto joint = rig::Joint(std::max(0, m_joint->currentIndex()));
    const bool flatAnkle =
        m_feetFlat->isChecked() && (joint == rig::Joint::LeftAnkle || joint == rig::Joint::RightAnkle);
    const bool was = m_loading;
    m_loading = true;
    for (int k = 0; k < 2; ++k) {
        const bool used = k < rig::angleCount(joint);
        m_angleLabel[k]->setVisible(used);
        m_angle[k]->setVisible(used);
        if (!used)
            continue;
        m_angleLabel[k]->setText(tr(rig::angleTitle(joint, k)) + QLatin1Char(':'));
        const double value = m_f.angles[std::size_t(joint)][std::size_t(k)];
        const auto range = rig::angleRange(joint, k);
        // Wide enough for the angle it was made at, whatever that is.
        m_angle[k]->setRange(std::min(range[0], std::floor(value)), std::max(range[1], std::ceil(value)));
        m_angle[k]->setValue(value);
        m_angle[k]->setEnabled(!flatAnkle);
        m_angle[k]->setToolTip(flatAnkle ? tr("Feet flat on the ground is ticked: the feet keep their angle")
                                         : QString());
    }
    m_loading = was;
}

bool PoseFigurePanel::posing() const
{
    return m_poseStage->isChecked();
}

bool PoseFigurePanel::mirror() const
{
    return m_mirror->isChecked();
}

int PoseFigurePanel::joint() const
{
    return m_joint->currentIndex();
}

void PoseFigurePanel::setJoint(int joint)
{
    const bool was = m_loading;
    m_loading = true;
    m_joint->setCurrentIndex(joint);
    m_loading = was;
    showJoint();
}

double PoseFigurePanel::blend() const
{
    return m_blend->value();
}

bool PoseFigurePanel::feetFlat() const
{
    return m_feetFlat->isChecked();
}

bool PoseFigurePanel::onGround() const
{
    return m_onGround->isChecked();
}

void PoseFigurePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
