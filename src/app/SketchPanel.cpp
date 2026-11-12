// SPDX-License-Identifier: GPL-3.0-or-later
#include "SketchPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

SketchPanel::SketchPanel(QWidget* parent) : QWidget(parent)
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
    m_message = new QLabel;
    m_message->setWordWrap(true);
    layout->addWidget(m_message);

    auto* makeGroup = new QGroupBox(tr("When finished"));
    auto* makeLayout = new QVBoxLayout(makeGroup);
    makeLayout->setSpacing(2);
    // The label sits right on top of its list.
    m_makeLabel = new QLabel(tr("Make:"));
    makeLayout->addWidget(m_makeLabel);
    m_make = new QComboBox;
    m_make->addItem(tr("Pad (push the outline out)"));
    m_make->addItem(tr("Pocket (cut the outline in)"));
    m_make->setToolTip(tr("Pad adds a solid shaped like the outline. Pocket cuts it into the shape you sketched on."));
    makeLayout->addWidget(m_make);
    makeLayout->addSpacing(8);
    m_lengthLabel = new QLabel;
    makeLayout->addWidget(m_lengthLabel);
    m_length = new QDoubleSpinBox;
    m_length->setRange(0.01, 10000.0);
    m_length->setDecimals(2);
    m_length->setSingleStep(1.0);
    m_length->setSuffix(tr(" mm"));
    m_length->setMinimumWidth(110);
    makeLayout->addWidget(m_length);
    makeLayout->addSpacing(6);
    m_otherWay = new QCheckBox(tr("The other way"));
    m_bothWays = new QCheckBox(tr("Both ways (half each side)"));
    m_throughAll = new QCheckBox(tr("Through all"));
    m_throughAll->setToolTip(tr("Cut right through the shape, however thick it gets"));
    makeLayout->addWidget(m_otherWay);
    makeLayout->addWidget(m_bothWays);
    makeLayout->addWidget(m_throughAll);
    layout->addWidget(makeGroup);

    auto* help = new QLabel(tr("<b>Draw</b>: Line (L), Rectangle (R), Circle (C), Arc (A), Slot (S). "
                               "Esc stops drawing, then goes back to picking.<br>"
                               "<b>Pick</b>: click; Ctrl+click picks more. Drag points and lines to move them.<br>"
                               "<b>Rules</b> go on what's picked: Horizontal (H), Vertical (V), Parallel, "
                               "Perpendicular, Equal (E), Tangent (T), Coincident (O), Symmetric, Lock.<br>"
                               "<b>Dimension</b> (D): a line's length, two points' distance, a circle's radius or "
                               "two lines' angle. Double-click a dimension to change it.<br>"
                               "Shapes go <span style='color:#1e8c3c'>green</span> when every size and place "
                               "is set. <span style='color:#d22'>Red</span> points are loose ends.<br>"
                               "Wheel zooms; right- or middle-drag pans; F fits."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);

    auto* buttons = new QHBoxLayout;
    m_finish = new QPushButton(tr("Finish"));
    m_finish->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setToolTip(tr("Close the sketch without changing anything"));
    buttons->addWidget(m_finish);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_make, &QComboBox::currentIndexChanged, this, &SketchPanel::updateLabels);
    connect(m_bothWays, &QCheckBox::toggled, this, &SketchPanel::updateLabels);
    connect(m_throughAll, &QCheckBox::toggled, this, &SketchPanel::updateLabels);
    connect(m_finish, &QPushButton::clicked, this, &SketchPanel::finishRequested);
    connect(cancel, &QPushButton::clicked, this, &SketchPanel::cancelRequested);
}

void SketchPanel::start(const QString& title, bool pocketAllowed, bool pocket, double length, bool otherWay,
                        bool bothWays, bool throughAll, bool editing)
{
    m_title->setText(title);
    const QSignalBlocker b0(m_make), b1(m_bothWays), b2(m_throughAll);
    m_make->setCurrentIndex(pocketAllowed && pocket ? 1 : 0);
    // Changing a sketch keeps what it makes (a pad stays a pad).
    m_make->setEnabled(pocketAllowed && !editing);
    m_makeLabel->setVisible(pocketAllowed);
    m_make->setVisible(pocketAllowed);
    m_length->setValue(length);
    m_otherWay->setChecked(otherWay);
    m_bothWays->setChecked(bothWays);
    m_throughAll->setChecked(throughAll);
    m_finish->setToolTip(editing ? tr("Change it to this sketch") : tr("Make the pad or pocket from this sketch"));
    setMessage({}, false);
    updateLabels();
}

bool SketchPanel::pocket() const
{
    return m_make->isVisibleTo(this) && m_make->currentIndex() == 1;
}

double SketchPanel::length() const
{
    return m_length->value();
}

bool SketchPanel::otherWay() const
{
    return m_otherWay->isChecked();
}

bool SketchPanel::bothWays() const
{
    return m_bothWays->isChecked() && !pocket();
}

bool SketchPanel::throughAll() const
{
    return m_throughAll->isChecked() && pocket();
}

void SketchPanel::setStatus(const QString& text, bool problem, bool held)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;")
                                    : held ? QStringLiteral("color: #1e8c3c;") : QString());
}

void SketchPanel::setMessage(const QString& text, bool problem)
{
    m_message->setText(text);
    m_message->setVisible(!text.isEmpty());
    m_message->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QStringLiteral("color: palette(mid);"));
}

void SketchPanel::updateLabels()
{
    const bool cut = pocket();
    m_lengthLabel->setText(cut ? tr("Depth:") : tr("Length:"));
    m_length->setToolTip(cut ? tr("How deep the pocket goes into the shape")
                             : tr("How far the outline is pushed out"));
    m_bothWays->setVisible(!cut);
    m_throughAll->setVisible(cut);
    m_otherWay->setToolTip(cut ? tr("Cut outwards from the face instead of into it")
                               : tr("Push the other way (down / into the face)"));
    m_otherWay->setEnabled(cut || !m_bothWays->isChecked());
    m_length->setEnabled(!(cut && m_throughAll->isChecked()));
    m_lengthLabel->setEnabled(m_length->isEnabled());
}

}  // namespace forge::app
