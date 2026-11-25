// SPDX-License-Identifier: GPL-3.0-or-later
#include "LassoPanel.h"

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

LassoPanel::LassoPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Drag a loop round the part to cut, as you see it on the screen. The cut goes "
                               "straight through the shape, the way you're looking: turn the view first "
                               "(right-drag) to look from the best side. Drag again to draw a new loop."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(6);

    // What to keep: the label resting on its list.
    auto* keepBox = new QWidget;
    auto* keepColumn = new QVBoxLayout(keepBox);
    keepColumn->setContentsMargins(0, 0, 0, 0);
    keepColumn->setSpacing(0);
    keepColumn->addWidget(new QLabel(tr("What to keep:")));
    m_keep = new QComboBox;
    m_keep->addItem(tr("Cut away inside the loop"), int(lasso::Keep::Outside));
    m_keep->addItem(tr("Keep only inside the loop"), int(lasso::Keep::Inside));
    m_keep->addItem(tr("Both pieces (inside and outside)"), int(lasso::Keep::Both));
    m_keep->setToolTip(tr("Cut away: a hole or notch right through where the loop is. Keep only inside: the rest "
                          "is cut away. Both: two shapes, as if cut with a knife."));
    m_keep->setCurrentIndex(std::clamp(prefs().value(QStringLiteral("lasso/keep"), 0).toInt(), 0, 2));
    keepColumn->addWidget(m_keep);
    layout->addWidget(keepBox);
    connect(m_keep, &QComboBox::currentIndexChanged, this,
            [](int i) { prefs().setValue(QStringLiteral("lasso/keep"), i); });

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    m_clear = new QPushButton(tr("Clear the Loop"));
    m_apply = new QPushButton(tr("Cut"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    buttons->addWidget(m_clear);
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_clear, &QPushButton::clicked, this, &LassoPanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &LassoPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &LassoPanel::cancelRequested);
}

void LassoPanel::start(const QString& title)
{
    m_title->setText(title);
    setStatus(tr("Drag a loop round part of the shape."), false);
    setApplyEnabled(false);
}

lasso::Keep LassoPanel::keep() const
{
    return lasso::Keep(m_keep->currentData().toInt());
}

void LassoPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void LassoPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
    m_clear->setEnabled(on);
}

}  // namespace forge::app
