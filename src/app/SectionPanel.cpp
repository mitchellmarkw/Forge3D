// SPDX-License-Identifier: GPL-3.0-or-later
#include "SectionPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace forge::app {

namespace {
constexpr int kSteps = 1000;
}

SectionPanel::SectionPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    // The label rests on top of its list.
    layout->addWidget(new QLabel(tr("Cut across:")));
    m_axis = new QComboBox;
    m_axis->addItem(tr("Left to right (X)"));
    m_axis->addItem(tr("Front to back (Y)"));
    m_axis->addItem(tr("Bottom to top (Z)"));
    m_axis->setToolTip(tr("Which way the cut moves through the model"));
    layout->addWidget(m_axis);
    layout->addSpacing(8);
    layout->addWidget(new QLabel(tr("Where:")));
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, kSteps);
    m_slider->setValue(kSteps / 2);
    m_slider->setToolTip(tr("Slide the cut through the model"));
    layout->addWidget(m_slider);
    m_position = new QDoubleSpinBox;
    m_position->setRange(-100000.0, 100000.0);
    m_position->setDecimals(2);
    m_position->setSingleStep(1.0);
    m_position->setSuffix(tr(" mm"));
    m_position->setToolTip(tr("Where the cut is, measured from the middle of the plate (Z: from the plate up)"));
    layout->addWidget(m_position);
    auto* middle = new QPushButton(tr("Middle of the model"));
    layout->addWidget(middle);
    layout->addSpacing(8);
    m_otherSide = new QCheckBox(tr("Show the other side"));
    m_otherSide->setToolTip(tr("Hide the part before the cut instead of the part after it"));
    layout->addWidget(m_otherSide);
    layout->addSpacing(8);
    auto* help = new QLabel(tr("Only the view is cut: the model stays whole. Close this panel to see "
                               "everything again."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addStretch(1);

    connect(m_axis, &QComboBox::currentIndexChanged, this, [this] {
        m_placed = false;  // another way through: start from the middle again
        emit axisChanged();
        emit changed();
    });
    connect(m_slider, &QSlider::valueChanged, this, &SectionPanel::fromSlider);
    connect(m_position, &QDoubleSpinBox::valueChanged, this, &SectionPanel::fromSpin);
    connect(m_otherSide, &QCheckBox::toggled, this, &SectionPanel::changed);
    connect(middle, &QPushButton::clicked, this, [this] { m_position->setValue((m_low + m_high) / 2); });
}

int SectionPanel::axis() const
{
    return m_axis->currentIndex();
}

double SectionPanel::position() const
{
    return m_position->value();
}

bool SectionPanel::otherSide() const
{
    return m_otherSide->isChecked();
}

void SectionPanel::setRange(double low, double high, bool keepPosition)
{
    if (!(high - low > 1e-6)) {
        low -= 5.0;
        high += 5.0;
    }
    // A little past each end, so the cut can clear the model.
    const double margin = (high - low) * 0.02;
    m_low = low - margin;
    m_high = high + margin;
    const double now = m_position->value();
    const double to = keepPosition && m_placed && now >= m_low && now <= m_high ? now : (m_low + m_high) / 2;
    m_placed = true;
    {
        const QSignalBlocker a(m_position), b(m_slider);
        m_position->setSingleStep(std::max(0.1, std::round((m_high - m_low) / 100.0 * 10.0) / 10.0));
        m_position->setValue(to);
        m_slider->setValue(int(std::lround((to - m_low) / (m_high - m_low) * kSteps)));
    }
    emit changed();
}

void SectionPanel::fromSlider(int value)
{
    {
        const QSignalBlocker block(m_position);
        m_position->setValue(m_low + (m_high - m_low) * value / kSteps);
    }
    emit changed();
}

void SectionPanel::fromSpin(double value)
{
    {
        const QSignalBlocker block(m_slider);
        m_slider->setValue(int(std::lround(std::clamp((value - m_low) / (m_high - m_low), 0.0, 1.0) * kSteps)));
    }
    emit changed();
}

}  // namespace forge::app
