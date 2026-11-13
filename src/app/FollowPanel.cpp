// SPDX-License-Identifier: GPL-3.0-or-later
#include "FollowPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

FollowPanel::FollowPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Cuts a groove (or raises a ridge) along a path you draw on the surface. Click the "
                               "spot on the shape where the path goes, then draw it. On a curved surface it bends "
                               "round with the surface."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(6);
    // Each list has its label right on top of it.
    layout->addWidget(new QLabel(tr("Make:")));
    m_make = new QComboBox;
    m_make->addItem(tr("Groove (cut in)"));
    m_make->addItem(tr("Ridge (raised)"));
    layout->addWidget(m_make);
    layout->addSpacing(4);
    layout->addWidget(new QLabel(tr("Shape across the path:")));
    m_profile = new QComboBox;
    m_profile->addItem(tr("Round"), int(FollowProfile::Round));
    m_profile->addItem(tr("Square"), int(FollowProfile::Square));
    m_profile->addItem(tr("V"), int(FollowProfile::V));
    m_profile->addItem(tr("My own (drawn)"), int(FollowProfile::Custom));
    layout->addWidget(m_profile);
    layout->addSpacing(4);
    m_widthLabel = new QLabel(tr("Width:"));
    layout->addWidget(m_widthLabel);
    m_width = new QDoubleSpinBox;
    m_width->setRange(0.05, 1000.0);
    m_width->setDecimals(2);
    m_width->setSingleStep(0.5);
    m_width->setSuffix(tr(" mm"));
    m_width->setToolTip(tr("How wide the groove or ridge is at the surface"));
    layout->addWidget(m_width);
    m_depthLabel = new QLabel;
    layout->addWidget(m_depthLabel);
    m_depth = new QDoubleSpinBox;
    m_depth->setRange(0.05, 1000.0);
    m_depth->setDecimals(2);
    m_depth->setSingleStep(0.5);
    m_depth->setSuffix(tr(" mm"));
    layout->addWidget(m_depth);
    m_drawProfile = new QPushButton(tr("Draw the shape across..."));
    m_drawProfile->setToolTip(tr("Draw your own cross-section: across the path to the sides, up out of the surface "
                                 "above the line"));
    layout->addWidget(m_drawProfile);
    layout->addSpacing(6);
    m_path = new QPushButton(tr("Draw the path..."));
    m_path->setToolTip(tr("Draw (or change) the path: lines, arcs, circles, slots"));
    layout->addWidget(m_path);
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

    connect(m_make, &QComboBox::currentIndexChanged, this, [this] {
        updateLabels();
        emit changed();
    });
    connect(m_profile, &QComboBox::currentIndexChanged, this, [this] {
        updateLabels();
        emit changed();
    });
    connect(m_width, &QDoubleSpinBox::valueChanged, this, &FollowPanel::changed);
    connect(m_depth, &QDoubleSpinBox::valueChanged, this, &FollowPanel::changed);
    connect(m_path, &QPushButton::clicked, this, &FollowPanel::pathRequested);
    connect(m_drawProfile, &QPushButton::clicked, this, &FollowPanel::profileRequested);
    connect(m_apply, &QPushButton::clicked, this, &FollowPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &FollowPanel::cancelRequested);
}

void FollowPanel::start(const QString& title, const FollowFinish& finish, bool editing)
{
    m_title->setText(title);
    const QSignalBlocker b0(m_make), b1(m_profile), b2(m_width), b3(m_depth);
    m_make->setCurrentIndex(finish.ridge ? 1 : 0);
    m_profile->setCurrentIndex(m_profile->findData(int(finish.profile)));
    m_width->setValue(finish.width);
    m_depth->setValue(finish.depth);
    m_apply->setToolTip(editing ? tr("Change it") : tr("Cut the groove / raise the ridge"));
    updateLabels();
}

bool FollowPanel::ridge() const
{
    return m_make->currentIndex() == 1;
}

FollowProfile FollowPanel::profile() const
{
    return FollowProfile(m_profile->currentData().toInt());
}

double FollowPanel::width() const
{
    return m_width->value();
}

double FollowPanel::depth() const
{
    return m_depth->value();
}

void FollowPanel::setPlaced(bool placed)
{
    m_path->setEnabled(placed);
}

void FollowPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void FollowPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

void FollowPanel::updateLabels()
{
    const bool own = profile() == FollowProfile::Custom;
    m_depthLabel->setText(ridge() ? tr("Height:") : tr("Depth:"));
    m_depth->setToolTip(ridge() ? tr("How high the ridge stands") : tr("How deep the groove goes"));
    for (QWidget* w : {static_cast<QWidget*>(m_widthLabel), static_cast<QWidget*>(m_width),
                       static_cast<QWidget*>(m_depthLabel), static_cast<QWidget*>(m_depth)})
        w->setVisible(!own);
    m_drawProfile->setVisible(own);
}

}  // namespace forge::app
