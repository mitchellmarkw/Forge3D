// SPDX-License-Identifier: GPL-3.0-or-later
#include "ReshapePanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {

namespace {

QSettings prefs()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                     QStringLiteral("Forge3D"));
}

}  // namespace

ReshapePanel::ReshapePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("A box of handles round the shape: drag a handle and the shape bends smoothly with "
                               "it, like pulling on a cage round it. A handle moves across the screen, so turn the "
                               "view (right-drag) to move it the other way."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(6);

    // The label resting on its list.
    auto* handlesBox = new QWidget;
    auto* column = new QVBoxLayout(handlesBox);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(tr("Handles:")));
    m_handles = new QComboBox;
    m_handles->addItem(tr("Corners only (2 × 2 × 2): stretch and lean"), 2);
    m_handles->addItem(tr("3 × 3 × 3: bulge, pinch and bend"), 3);
    m_handles->addItem(tr("4 × 4 × 4: finer shaping"), 4);
    m_handles->setToolTip(tr("More handles shape smaller areas. Changing this puts the handles back."));
    column->addWidget(m_handles);
    layout->addWidget(handlesBox);
    m_mirror = new QCheckBox(tr("Move left and right together"));
    m_mirror->setToolTip(tr("Dragging a handle moves its partner on the other side (across the middle of the box, "
                            "left to right) the mirror way"));
    m_mirror->setChecked(prefs().value(QStringLiteral("reshape/mirror"), true).toBool());
    layout->addWidget(m_mirror);
    auto* reset = new QPushButton(tr("Put the Handles Back"));
    layout->addWidget(reset);

    m_status = new QLabel;
    m_status->setWordWrap(true);
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

    connect(m_handles, &QComboBox::currentIndexChanged, this, [this] {
        if (m_loading)
            return;
        prefs().setValue(QStringLiteral("reshape/handles"), handles());
        emit handlesChanged(handles());
    });
    connect(m_mirror, &QCheckBox::toggled, this,
            [](bool on) { prefs().setValue(QStringLiteral("reshape/mirror"), on); });
    connect(reset, &QPushButton::clicked, this, &ReshapePanel::resetRequested);
    connect(apply, &QPushButton::clicked, this, &ReshapePanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &ReshapePanel::cancelRequested);
}

void ReshapePanel::start(const QString& title, int handles)
{
    m_title->setText(title);
    m_loading = true;
    if (handles <= 0)
        handles = prefs().value(QStringLiteral("reshape/handles"), 3).toInt();
    m_handles->setCurrentIndex(std::clamp(handles, 2, 4) - 2);
    m_loading = false;
    setStatus(tr("Drag a handle (the gold balls)."), false);
}

int ReshapePanel::handles() const
{
    return m_handles->currentData().toInt();
}

bool ReshapePanel::mirror() const
{
    return m_mirror->isChecked();
}

void ReshapePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

}  // namespace forge::app
