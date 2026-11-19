// SPDX-License-Identifier: GPL-3.0-or-later
#include "JoinPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

namespace {

QDoubleSpinBox* distance(const QString& tip)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(-1000.0, 1000.0);
    box->setDecimals(2);
    box->setSingleStep(0.1);
    box->setSuffix(QObject::tr(" mm"));
    box->setKeyboardTracking(false);
    box->setToolTip(tip);
    return box;
}

}  // namespace

JoinPanel::JoinPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Join Parts"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* help = new QLabel(tr("Moves one part so it joins another: a peg into its socket or hole, a ball into "
                               "its cup, a face flat against a face. Pick the place on each part."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(6);

    const QString heads[2] = {tr("1. On the part that moves:"), tr("2. Where it goes, on the other part:")};
    for (int i = 0; i < 2; ++i) {
        auto* head = new QLabel(heads[i]);
        head->setFont(bold);
        layout->addWidget(head);
        auto* row = new QHBoxLayout;
        m_picked[i] = new QLabel;
        m_picked[i]->setWordWrap(true);
        row->addWidget(m_picked[i], 1);
        m_again[i] = new QPushButton(tr("Pick Again"));
        m_again[i]->setToolTip(i == 0 ? tr("Click the part that moves again") : tr("Click where it goes again"));
        row->addWidget(m_again[i]);
        layout->addLayout(row);
        connect(m_again[i], &QPushButton::clicked, this, [this, i] { emit pickAgainRequested(i); });
    }
    layout->addSpacing(6);

    auto* form = new QFormLayout;
    m_along = distance(tr("How far short of meeting it stops, along the join. For a peg in a Quick Socket it "
                          "starts at the socket's fit, so the peg's shoulder meets the surface. Below 0: further "
                          "in."));
    form->addRow(tr("Gap:"), m_along);
    m_acrossX = distance(tr("Slides it across the join, one way"));
    form->addRow(tr("Across:"), m_acrossX);
    m_acrossY = distance(tr("Slides it across the join, the other way"));
    form->addRow(tr("Across (other way):"), m_acrossY);
    m_turn = new QDoubleSpinBox;
    m_turn->setRange(-360.0, 360.0);
    m_turn->setDecimals(1);
    m_turn->setSingleStep(15.0);
    m_turn->setSuffix(tr("°"));
    m_turn->setKeyboardTracking(false);
    m_turn->setToolTip(tr("Turns the moving part round the line of the join (to face it the right way, or line "
                          "up a peg's flats)"));
    form->addRow(tr("Turn:"), m_turn);
    layout->addLayout(form);
    m_flip = new QCheckBox(tr("Flip it round (if it goes in the wrong way)"));
    m_flip->setToolTip(tr("Turns the moving part end for end, for when it was worked out pointing the wrong way"));
    layout->addWidget(m_flip);
    m_together = new QCheckBox(tr("Keep them together (group them)"));
    m_together->setToolTip(tr("Groups the two parts once joined, so moving or turning one moves the other with it. "
                              "Joining onto a group adds to that group. Separate takes them apart again."));
    layout->addWidget(m_together);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Apply"));
    m_apply->setDefault(true);
    m_apply->setToolTip(tr("Move the part there"));
    auto* cancel = new QPushButton(tr("Cancel"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    for (QDoubleSpinBox* box : {m_along, m_acrossX, m_acrossY, m_turn})
        connect(box, &QDoubleSpinBox::valueChanged, this, &JoinPanel::changed);
    connect(m_flip, &QCheckBox::toggled, this, &JoinPanel::changed);
    connect(m_apply, &QPushButton::clicked, this, &JoinPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &JoinPanel::cancelRequested);
}

void JoinPanel::start()
{
    setOffsets({});
    setPicked(0, {}, true);
    setPicked(1, {}, false);
    setStatus({}, false);
    setApplyEnabled(false);
}

void JoinPanel::setPicked(int which, const QString& text, bool waiting)
{
    if (which < 0 || which > 1)
        return;
    QString shown = text;
    if (shown.isEmpty())
        shown = which == 0 ? tr("<i>Click the part to move, where it joins: its peg, ball or face.</i>")
                           : tr("<i>Click where it goes on the other part: the socket, hole, cup or face.</i>");
    if (waiting && !text.isEmpty())
        shown += tr(" <i>(click to change)</i>");
    m_picked[which]->setText(shown);
    m_again[which]->setEnabled(!text.isEmpty());
}

void JoinPanel::setOffsets(const join::Offsets& o)
{
    const QSignalBlocker b0(m_along), b1(m_acrossX), b2(m_acrossY), b3(m_turn), b4(m_flip);
    m_along->setValue(o.along);
    m_acrossX->setValue(o.acrossX);
    m_acrossY->setValue(o.acrossY);
    m_turn->setValue(o.turn);
    m_flip->setChecked(o.flip);
}

join::Offsets JoinPanel::offsets() const
{
    join::Offsets o;
    o.along = m_along->value();
    o.acrossX = m_acrossX->value();
    o.acrossY = m_acrossY->value();
    o.turn = m_turn->value();
    o.flip = m_flip->isChecked();
    return o;
}

bool JoinPanel::keepTogether() const
{
    return m_together->isChecked();
}

void JoinPanel::setKeepTogether(bool on)
{
    m_together->setChecked(on);
}

void JoinPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void JoinPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
