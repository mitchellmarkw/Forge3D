// SPDX-License-Identifier: GPL-3.0-or-later
#include "CutPartsPanel.h"

#include "forge/core/LimbCut.h"
#include "forge/core/Rig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QStandardItemModel>
#include <QDoubleSpinBox>
#include <QFormLayout>
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
    auto* poseable = new QPushButton(tr("Super Poseable"));
    poseable->setToolTip(tr("A toy's full set: a ball at the neck, butterfly shoulders, an ab crunch, drop-down hips, "
                            "double elbows and knees, and ankle rockers"));
    auto* none = new QPushButton(tr("No Cuts"));
    ready->addWidget(usual);
    ready->addWidget(poseable);
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
    // Turning the cut by hand, for a figure whose joints don't sit square to the body.
    m_tiltBox = new QWidget;
    auto* tilt = new QFormLayout(m_tiltBox);
    tilt->setContentsMargins(0, 0, 0, 0);
    m_forward = new QDoubleSpinBox;
    m_side = new QDoubleSpinBox;
    for (QDoubleSpinBox* spin : {m_forward, m_side}) {
        spin->setRange(-80.0, 80.0);
        spin->setDecimals(0);
        spin->setSingleStep(5.0);
        spin->setSuffix(tr("°"));
        spin->setKeyboardTracking(false);
    }
    m_forward->setToolTip(tr("Tips the cut towards the front (less than 0: towards the back)"));
    m_side->setToolTip(tr("Turns the cut round the line from front to back. At a shoulder, more makes it square "
                          "to an arm hanging down, less to an arm raised; elsewhere it tips the cut "
                          "towards one side. The same number on the other side makes a mirrored pair"));
    auto* straighten = new QPushButton(tr("Straighten"));
    straighten->setToolTip(tr("Back to the way Forge3D turned the cut"));
    tilt->addRow(tr("Tip forward:"), m_forward);
    tilt->addRow(tr("Tilt sideways:"), m_side);
    tilt->addRow(QString(), straighten);
    cuts->addWidget(m_tiltBox);
    connect(straighten, &QPushButton::clicked, this, [this] {
        m_forward->setValue(0.0);
        m_side->setValue(0.0);
    });
    m_info = wrapped(QString());
    m_info->setStyleSheet(QStringLiteral("color: palette(mid);"));
    cuts->addWidget(m_info);
    m_printer = new QComboBox;
    m_printer->addItem(tr("Resin"));
    m_printer->addItem(tr("FDM (filament)"));
    m_printer->setToolTip(tr("The joints' fit is your printer's (from the Tolerance Test Print, if you made one)"));
    cuts->addWidget(labelled(tr("Printer:"), m_printer));
    m_inPlace = new QCheckBox(tr("Print in place (filament)"));
    m_inPlace->setToolTip(tr("Builds every joint captive, with room all round, so the figure prints already put "
                             "together on a filament printer and moves once it's off the plate: balls held in "
                             "their sockets, hinge pins part of the fork. No clicks or ridges (they'd print "
                             "joined)."));
    cuts->addWidget(m_inPlace);
    m_clearanceRow = new QWidget;
    {
        auto* form = new QFormLayout(m_clearanceRow);
        form->setContentsMargins(0, 0, 0, 0);
        m_clearance = new QDoubleSpinBox;
        m_clearance->setRange(limbcut::kLeastClearance, limbcut::kMostClearance);
        m_clearance->setDecimals(2);
        m_clearance->setSingleStep(0.05);
        m_clearance->setSuffix(tr(" mm"));
        m_clearance->setToolTip(tr("Room all round each joint: 0.3 to 0.4 suits most filament printers (more if "
                                   "the joints print stuck, less if they're too loose)"));
        form->addRow(tr("Clearance:"), m_clearance);
    }
    cuts->addWidget(m_clearanceRow);
    {
        const QSettings p(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                          QStringLiteral("Forge3D"));
        m_inPlace->setChecked(p.value(QStringLiteral("cutParts/inPlace"), false).toBool());
        m_clearance->setValue(p.value(QStringLiteral("cutParts/clearance"), 0.35).toDouble());
    }
    m_clearanceRow->setVisible(m_inPlace->isChecked());
    connect(m_inPlace, &QCheckBox::toggled, this, [this](bool on) {
        m_clearanceRow->setVisible(on);
        if (on)
            m_printer->setCurrentIndex(1);  // filament
        QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"))
            .setValue(QStringLiteral("cutParts/inPlace"), on);
        showSummary();
    });
    connect(m_clearance, &QDoubleSpinBox::valueChanged, this, [](double v) {
        QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"))
            .setValue(QStringLiteral("cutParts/clearance"), v);
    });
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
        c.forward = m_forward->value();
        c.side = m_side->value();
        choose(joint(), c);
        showJoint();
        emit changed();
    };
    connect(m_cut, &QCheckBox::toggled, this, edited);
    connect(m_type, &QComboBox::currentIndexChanged, this, edited);
    connect(m_forward, &QDoubleSpinBox::valueChanged, this, edited);
    connect(m_side, &QDoubleSpinBox::valueChanged, this, edited);
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
    connect(poseable, &QPushButton::clicked, this, [this] {
        using J = rig::Joint;
        for (auto& [j, c] : m_choices)
            c = Choice{};
        m_choices[int(J::Neck)] = {true, Ball};
        m_choices[int(J::Waist)] = {true, AbCrunch};
        for (J j : {J::LeftShoulder, J::RightShoulder})
            m_choices[int(j)] = {true, Butterfly};
        for (J j : {J::LeftHip, J::RightHip})
            m_choices[int(j)] = {true, DropHip};
        for (J j : {J::LeftElbow, J::RightElbow, J::LeftKnee, J::RightKnee})
            m_choices[int(j)] = {true, DoubleHinge};
        for (J j : {J::LeftAnkle, J::RightAnkle})
            m_choices[int(j)] = {true, AnkleRocker};
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
    m_type->addItems({tr("Just cut"), tr("Ball joint"), tr("Hinge"), tr("Double hinge"), tr("Swivel"),
                      tr("Butterfly shoulder (hinge, then ball)"), tr("Drop-down hip (hinge, then ball)"),
                      tr("Ankle rocker (two hinges)"), tr("Ab crunch (swivel, then hinge)")});
    m_type->setItemData(Butterfly, tr("A small block in the chest on an upright hinge, carrying the shoulder's ball: "
                                      "the arm can swing forward, for crossed arms and two-handed poses"),
                        Qt::ToolTipRole);
    m_type->setItemData(DropHip, tr("The hip's ball on a block that hinges down, so the leg kicks higher and sits "
                                    "wider"),
                        Qt::ToolTipRole);
    m_type->setItemData(AnkleRocker, tr("A hinge for the foot up and down, then one for it to rock side to side, so "
                                        "it stays flat in a wide stance"),
                        Qt::ToolTipRole);
    m_type->setItemData(AbCrunch, tr("A swivel at the waist, then a hinge so the chest bends forward"),
                        Qt::ToolTipRole);
    for (const QString& s : standardJoints)
        m_type->addItem(tr("Standard: %1").arg(s));
    m_joint->setCurrentIndex(0);
    m_loading = false;
    showJoint();
    setStatus({}, false);
}

limbcut::Recipe CutPartsPanel::recipeOf(int type)
{
    switch (type) {
    case Butterfly: return limbcut::Recipe::Butterfly;
    case DropHip: return limbcut::Recipe::DropHip;
    case AnkleRocker: return limbcut::Recipe::AnkleRocker;
    case AbCrunch: return limbcut::Recipe::AbCrunch;
    default: return limbcut::Recipe::None;
    }
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

limbcut::InPlace CutPartsPanel::inPlace() const
{
    limbcut::InPlace p;
    p.on = m_inPlace->isChecked();
    p.clearance = m_clearance->value();
    return p;
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
            if (int(j) != joint && rig::pivot(j) == rig::mirrored(p)) {
                m_choices[int(j)] = c;  // the same numbers make a mirrored pair
            }
        }
    }
}

void CutPartsPanel::showJoint()
{
    const Choice c = m_choices[joint()];
    const bool was = m_loading;
    m_loading = true;
    m_cut->setChecked(c.cut);
    // Each recipe only at its own joints.
    if (auto* model = qobject_cast<QStandardItemModel*>(m_type->model()))
        for (int t = Butterfly; t <= AbCrunch && t < model->rowCount(); ++t)
            model->item(t)->setEnabled(limbcut::recipeFits(recipeOf(t), rig::Joint(joint())));
    m_type->setCurrentIndex(std::min(c.type, m_type->count() - 1));
    m_type->setEnabled(c.cut);
    m_forward->setValue(c.forward);
    m_side->setValue(c.side);
    m_tiltBox->setEnabled(c.cut);
    m_loading = was;
    showSummary();
}

void CutPartsPanel::showSummary()
{
    QStringList lines;
    for (rig::Joint j : limbcut::cuttable()) {
        const Choice& c = m_choices[int(j)];
        if (!c.cut)
            continue;
        QString line = tr("%1: %2").arg(tr(rig::title(j)), m_type->itemText(c.type));
        if (c.forward != 0.0 || c.side != 0.0)
            line += tr(" (turned %1°, %2°)").arg(c.forward).arg(c.side);
        lines << line;
    }
    m_summary->setText(lines.isEmpty() ? tr("No cuts chosen yet: try Usual Figure, or pick a joint and tick Cut here.")
                                       : tr("Cuts (%1):\n%2").arg(lines.size()).arg(lines.join(QLatin1Char('\n'))) +
                                             (m_inPlace && m_inPlace->isChecked()
                                                  ? tr("\nPrinted in place: the pieces stay put together.")
                                                  : QString()));
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
