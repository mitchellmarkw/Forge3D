// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

ShellPanel::ShellPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Hollows the shape out, leaving walls of this thickness. Click the faces to leave "
                               "open (the top of a box makes a cup); click again to close one. With none "
                               "picked, it's hollow and sealed."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(6);
    layout->addWidget(new QLabel(tr("Wall thickness:")));
    m_thickness = new QDoubleSpinBox;
    m_thickness->setRange(0.05, 1000.0);
    m_thickness->setDecimals(2);
    m_thickness->setSingleStep(0.5);
    m_thickness->setSuffix(tr(" mm"));
    m_thickness->setMinimumWidth(110);
    layout->addWidget(m_thickness);
    m_outward = new QCheckBox(tr("Walls outside (keep the inside the size it is)"));
    m_outward->setToolTip(tr("Grow the walls outwards instead of in, so the hollow is the shape's size"));
    layout->addWidget(m_outward);
    layout->addSpacing(6);
    m_open = new QLabel;
    layout->addWidget(m_open);
    auto* clear = new QPushButton(tr("Close all faces"));
    clear->setToolTip(tr("Pick no faces: hollow and sealed"));
    layout->addWidget(clear);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Apply"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_thickness, &QDoubleSpinBox::valueChanged, this, &ShellPanel::changed);
    connect(m_outward, &QCheckBox::toggled, this, &ShellPanel::changed);
    connect(clear, &QPushButton::clicked, this, &ShellPanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &ShellPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &ShellPanel::cancelRequested);
}

void ShellPanel::start(const QString& title, bool editing, double thickness, bool outward)
{
    m_title->setText(title);
    const QSignalBlocker b0(m_thickness), b1(m_outward);
    m_thickness->setValue(thickness);
    m_outward->setChecked(outward);
    m_apply->setToolTip(editing ? tr("Change the shell") : tr("Hollow it out"));
}

double ShellPanel::thickness() const
{
    return m_thickness->value();
}

bool ShellPanel::outward() const
{
    return m_outward->isChecked();
}

void ShellPanel::setOpenCount(int count)
{
    m_open->setText(count == 0   ? tr("No faces open: hollow and sealed.")
                    : count == 1 ? tr("1 face left open.")
                                 : tr("%1 faces left open.").arg(count));
}

void ShellPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void ShellPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
