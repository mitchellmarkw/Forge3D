// SPDX-License-Identifier: GPL-3.0-or-later
#include "EdgesPanel.h"

#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

EdgesPanel::EdgesPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* kindGroup = new QGroupBox(tr("Edges"));
    auto* kindLayout = new QVBoxLayout(kindGroup);
    m_kinds = new QButtonGroup(this);
    const struct {
        EdgeFinishKind kind;
        QString label;
        QString tip;
    } kinds[] = {
        {EdgeFinishKind::Rounding, tr("Rounding (outside edges)"), tr("Turn sharp outside edges into a curve")},
        {EdgeFinishKind::Fillet, tr("Fillet (inside corners, or a cove)"),
         tr("Fill inside corners with a curve. On an outside edge it cuts a curved hollow instead, as if a "
            "cylinder centred on the edge were taken away")},
        {EdgeFinishKind::Chamfer, tr("Chamfer"), tr("Cut the corner off flat, the same distance back on both faces")},
        {EdgeFinishKind::Bevel, tr("Bevel"), tr("A flat slope across the edge at an angle to the face you pick")},
    };
    for (const auto& k : kinds) {
        auto* radio = new QRadioButton(k.label);
        radio->setToolTip(k.tip);
        m_kinds->addButton(radio, int(k.kind));
        kindLayout->addWidget(radio);
    }
    m_kinds->button(int(EdgeFinishKind::Rounding))->setChecked(true);
    layout->addWidget(kindGroup);

    auto* sizeGroup = new QGroupBox(tr("Size"));
    auto* form = new QFormLayout(sizeGroup);
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.01, 1000.0);
    m_size->setDecimals(2);
    m_size->setSingleStep(0.5);
    m_size->setSuffix(tr(" mm"));
    m_size->setKeyboardTracking(false);
    m_size->setMinimumWidth(110);
    m_sizeLabel = new QLabel;
    form->addRow(m_sizeLabel, m_size);
    m_angle = new QDoubleSpinBox;
    m_angle->setRange(1.0, 89.0);
    m_angle->setDecimals(1);
    m_angle->setSingleStep(5.0);
    m_angle->setSuffix(tr(" °"));
    m_angle->setKeyboardTracking(false);
    m_angle->setToolTip(tr("The slope's angle from the face you picked the edge on"));
    m_angleLabel = new QLabel(tr("Angle:"));
    form->addRow(m_angleLabel, m_angle);
    m_kindHelp = new QLabel;
    m_kindHelp->setWordWrap(true);
    m_kindHelp->setStyleSheet(QStringLiteral("color: palette(mid);"));
    form->addRow(m_kindHelp);
    layout->addWidget(sizeGroup);

    auto* pickGroup = new QGroupBox(tr("Picked"));
    auto* pickLayout = new QVBoxLayout(pickGroup);
    m_picked = new QLabel;
    pickLayout->addWidget(m_picked);
    auto* pickButtons = new QHBoxLayout;
    auto* all = new QPushButton(tr("All edges"));
    all->setToolTip(tr("Pick every edge this kind can go on"));
    auto* clear = new QPushButton(tr("Clear"));
    clear->setToolTip(tr("Pick no edges"));
    pickButtons->addWidget(all);
    pickButtons->addWidget(clear);
    pickLayout->addLayout(pickButtons);
    auto* hint = new QLabel(tr("Click an edge to pick it (click again to drop it). Click a face to pick all its "
                               "edges at once. Edges light up as the mouse passes over them."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    pickLayout->addWidget(hint);
    layout->addWidget(pickGroup);

    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Apply"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setToolTip(tr("Close without changing anything (Esc)"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_kinds, &QButtonGroup::idToggled, this, [this](int, bool on) {
        if (on) {
            updateLabels();
            emit kindChanged();
        }
    });
    connect(m_size, &QDoubleSpinBox::valueChanged, this, &EdgesPanel::sizeChanged);
    connect(m_angle, &QDoubleSpinBox::valueChanged, this, &EdgesPanel::sizeChanged);
    connect(all, &QPushButton::clicked, this, &EdgesPanel::allRequested);
    connect(clear, &QPushButton::clicked, this, &EdgesPanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &EdgesPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &EdgesPanel::cancelRequested);
    updateLabels();
}

void EdgesPanel::start(const QString& title, bool editing, EdgeFinishKind kind, double size, double angle)
{
    m_title->setText(title);
    m_apply->setToolTip(editing ? tr("Change the edges") : tr("Finish the picked edges"));
    const QSignalBlocker b0(m_kinds), b1(m_size), b2(m_angle);
    m_kinds->button(int(kind))->setChecked(true);
    m_size->setValue(size);
    m_angle->setValue(angle);
    updateLabels();
}

EdgeFinishKind EdgesPanel::kind() const
{
    return EdgeFinishKind(std::max(0, m_kinds->checkedId()));
}

double EdgesPanel::size() const
{
    return m_size->value();
}

double EdgesPanel::angle() const
{
    return m_angle->value();
}

void EdgesPanel::setPickedCount(int count)
{
    m_picked->setText(count == 0   ? tr("No edges picked yet.")
                      : count == 1 ? tr("1 edge picked.")
                                   : tr("%1 edges picked.").arg(count));
}

void EdgesPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void EdgesPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

void EdgesPanel::updateLabels()
{
    const bool bevel = kind() == EdgeFinishKind::Bevel;
    m_angleLabel->setVisible(bevel);
    m_angle->setVisible(bevel);
    switch (kind()) {
    case EdgeFinishKind::Rounding:
        m_sizeLabel->setText(tr("Radius:"));
        m_size->setToolTip(tr("How round: R1, R2, R3..."));
        m_kindHelp->setText(tr("Only outside edges can be picked."));
        break;
    case EdgeFinishKind::Fillet:
        m_sizeLabel->setText(tr("Radius:"));
        m_size->setToolTip(tr("How big the curve is: the radius filling an inside corner, or of the cylinder "
                              "taken away along an outside edge"));
        m_kindHelp->setText(tr("Inside corners are filled with a curve. Outside edges get a curved hollow (a "
                               "cove), as if a cylinder of this radius centred on the edge were subtracted."));
        break;
    case EdgeFinishKind::Chamfer:
        m_sizeLabel->setText(tr("Distance:"));
        m_size->setToolTip(tr("How far back the cut goes along each face"));
        m_kindHelp->setText(tr("Cuts the corner off at 45°."));
        break;
    case EdgeFinishKind::Bevel:
        m_sizeLabel->setText(tr("Width:"));
        m_size->setToolTip(tr("How far the slope reaches across the face you picked the edge on"));
        m_kindHelp->setText(tr("Measured on the face you click: click a face to bevel all its edges from it, "
                               "or click an edge just inside that face."));
        break;
    }
}

}  // namespace forge::app
