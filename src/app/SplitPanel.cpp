// SPDX-License-Identifier: GPL-3.0-or-later
#include "SplitPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace forge::app {

namespace {
constexpr int kSliderSteps = 1000;

QDoubleSpinBox* angleSpin(const QString& tip)
{
    auto* spin = new QDoubleSpinBox;
    spin->setRange(-180.0, 180.0);
    spin->setDecimals(1);
    spin->setSingleStep(5.0);
    spin->setSuffix(QStringLiteral(" °"));
    spin->setWrapping(true);
    spin->setKeyboardTracking(false);
    spin->setToolTip(tip);
    return spin;
}
}  // namespace

SplitPanel::SplitPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* hint = new QLabel(tr("The yellow plane shows where the cut goes; drag it (or its arrow) to move it. "
                               "The arrow points to the first side. Orbit the view to check it."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_smart = new QPushButton(tr("Smart Split: Fit the Printer"));
    m_smart->setToolTip(tr("Let Forge3D choose the cuts: as few as will make every piece fit your printer "
                           "(Build Plate Size), each where it cuts through least, keyed with the pegs set "
                           "below"));
    layout->addWidget(m_smart);

    // Direction
    auto* dirGroup = new QGroupBox(tr("Direction"));
    auto* dirLayout = new QVBoxLayout(dirGroup);
    auto* presets = new QHBoxLayout;
    const auto preset = [&](const QString& text, const QString& tip, double tx, double ty) {
        auto* b = new QPushButton(text);
        b->setToolTip(tip);
        connect(b, &QPushButton::clicked, this, [this, tx, ty] { setTilts(tx, ty); });
        presets->addWidget(b);
    };
    preset(tr("Flat"), tr("Cut straight across, parallel to the plate (top and bottom)"), 0.0, 0.0);
    preset(tr("Across X"), tr("Cut upright, splitting left from right"), 0.0, 90.0);
    preset(tr("Across Y"), tr("Cut upright, splitting front from back"), -90.0, 0.0);
    dirLayout->addLayout(presets);
    auto* tiltForm = new QFormLayout;
    m_tiltX = angleSpin(tr("Tip the plane around the X axis"));
    m_tiltY = angleSpin(tr("Tip the plane around the Y axis"));
    tiltForm->addRow(tr("Tilt around X:"), m_tiltX);
    tiltForm->addRow(tr("Tilt around Y:"), m_tiltY);
    dirLayout->addLayout(tiltForm);
    layout->addWidget(dirGroup);

    // Position
    auto* posGroup = new QGroupBox(tr("Position"));
    auto* posLayout = new QVBoxLayout(posGroup);
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, kSliderSteps);
    m_slider->setToolTip(tr("Slide the plane through the shape"));
    posLayout->addWidget(m_slider);
    auto* posForm = new QFormLayout;
    m_offset = new QDoubleSpinBox;
    m_offset->setDecimals(2);
    m_offset->setSuffix(tr(" mm"));
    m_offset->setKeyboardTracking(false);
    m_offset->setToolTip(tr("Distance from the middle of the shape, along the arrow"));
    posForm->addRow(tr("From the middle:"), m_offset);
    posLayout->addLayout(posForm);
    layout->addWidget(posGroup);

    // Keep
    m_keepGroup = new QGroupBox(tr("Keep"));
    auto* keepLayout = new QVBoxLayout(m_keepGroup);
    m_keepBoth = new QRadioButton(tr("Both halves (as two shapes)"));
    m_keepFront = new QRadioButton;
    m_keepBack = new QRadioButton;
    m_keepBoth->setChecked(true);
    keepLayout->addWidget(m_keepBoth);
    keepLayout->addWidget(m_keepFront);
    keepLayout->addWidget(m_keepBack);
    layout->addWidget(m_keepGroup);

    // Alignment pegs
    m_keysGroup = new QGroupBox(tr("Alignment pegs"));
    auto* keysLayout = new QVBoxLayout(m_keysGroup);
    m_keys = new QCheckBox(tr("Key the cut, so the pieces line up"));
    m_keys->setToolTip(tr("Pegs on one piece fit tapered holes in the other, so the pieces only go together "
                          "one way when you glue them"));
    keysLayout->addWidget(m_keys);
    m_pegs = new QRadioButton(tr("Pegs on one piece"));
    m_pegs->setToolTip(tr("Pegs stand out of one piece (with a level cut, the lower one) into holes in the "
                          "other"));
    m_pins = new QRadioButton(tr("Loose pins (holes in both)"));
    m_pins->setToolTip(tr("Both pieces get holes, and pins are added beside the model to print on their "
                          "own: each piece keeps a flat cut face to print on"));
    keysLayout->addWidget(m_pegs);
    keysLayout->addWidget(m_pins);
    auto* sizes = new QFormLayout;
    const auto spin = [](double lo, double hi, double step, int decimals, const QString& suffix, const QString& tip) {
        auto* s = new QDoubleSpinBox;
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setDecimals(decimals);
        s->setSuffix(suffix);
        s->setToolTip(tip);
        s->setKeyboardTracking(false);
        return s;
    };
    m_diameter = spin(2.0, 30.0, 0.5, 1, tr(" mm"), tr("How thick a peg is where it leaves the piece (smaller "
                                                         "where a cut has no room for it)"));
    m_length = spin(2.0, 40.0, 0.5, 1, tr(" mm"), tr("How far a peg stands out (a pin: its whole length)"));
    m_taper = spin(0.0, 10.0, 0.5, 1, QStringLiteral("\u00B0"),
                   tr("How much a peg narrows towards its tip, so it starts in its hole easily"));
    m_clearance = spin(0.0, 1.0, 0.05, 2, tr(" mm"),
                       tr("Extra room in the holes: 0.2 mm suits most printers; more if they're too tight"));
    sizes->addRow(tr("Diameter:"), m_diameter);
    m_lengthLabel = new QLabel(tr("Length:"));
    sizes->addRow(m_lengthLabel, m_length);
    sizes->addRow(tr("Taper:"), m_taper);
    sizes->addRow(tr("Clearance:"), m_clearance);
    keysLayout->addLayout(sizes);
    layout->addWidget(m_keysGroup);
    setKeyOptions(KeyOptions{});

    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Split"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setToolTip(tr("Leave without cutting (Esc)"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_tiltX, &QDoubleSpinBox::valueChanged, this, &SplitPanel::planeChanged);
    connect(m_tiltY, &QDoubleSpinBox::valueChanged, this, &SplitPanel::planeChanged);
    connect(m_offset, &QDoubleSpinBox::valueChanged, this, [this] {
        syncSlider();
        emit planeChanged();
    });
    connect(m_slider, &QSlider::valueChanged, this, &SplitPanel::offsetFromSlider);
    connect(m_apply, &QPushButton::clicked, this, &SplitPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &SplitPanel::cancelRequested);
    connect(m_smart, &QPushButton::clicked, this, &SplitPanel::smartRequested);
    for (QRadioButton* b : {m_keepBoth, m_keepFront, m_keepBack, m_pegs, m_pins})
        connect(b, &QRadioButton::toggled, this, [this] {
            updateKeys();
            emit keysChanged();
        });
    connect(m_keys, &QCheckBox::toggled, this, [this] {
        updateKeys();
        emit keysChanged();
    });
    for (QDoubleSpinBox* s : {m_diameter, m_length, m_taper, m_clearance})
        connect(s, &QDoubleSpinBox::valueChanged, this, &SplitPanel::keysChanged);
}

void SplitPanel::updateKeys()
{
    const bool both = !m_keepGroup->isHidden() && m_keepBoth->isChecked();
    m_keysGroup->setVisible(!m_keepGroup->isHidden());
    m_keysGroup->setEnabled(both);
    const bool on = m_keys->isChecked();
    for (QWidget* w : {static_cast<QWidget*>(m_pegs), static_cast<QWidget*>(m_pins),
                       static_cast<QWidget*>(m_diameter), static_cast<QWidget*>(m_length),
                       static_cast<QWidget*>(m_taper), static_cast<QWidget*>(m_clearance)})
        w->setEnabled(on);
    m_lengthLabel->setText(m_pins->isChecked() ? tr("Pin length:") : tr("Peg length:"));
}

KeyOptions SplitPanel::keyOptions() const
{
    KeyOptions o;
    o.keys = m_keys->isChecked() && !m_keepGroup->isHidden() && m_keepBoth->isChecked();
    o.loosePins = m_pins->isChecked();
    o.diameter = m_diameter->value();
    o.length = m_length->value();
    o.taper = m_taper->value();
    o.clearance = m_clearance->value();
    return o;
}

KeyOptions SplitPanel::keySettings() const
{
    KeyOptions o = keyOptions();
    o.keys = m_keys->isChecked();
    return o;
}

void SplitPanel::setKeyOptions(const KeyOptions& o)
{
    const QSignalBlocker b1(m_keys), b2(m_pegs), b3(m_pins), b4(m_diameter), b5(m_length), b6(m_taper),
        b7(m_clearance);
    m_keys->setChecked(o.keys);
    (o.loosePins ? m_pins : m_pegs)->setChecked(true);
    m_diameter->setValue(o.diameter);
    m_length->setValue(o.length);
    m_taper->setValue(o.taper);
    m_clearance->setValue(o.clearance);
    updateKeys();
}

void SplitPanel::start(const QString& title, bool editing, double tiltX, double tiltY, double offset)
{
    const QSignalBlocker b1(m_tiltX), b2(m_tiltY), b3(m_offset), b4(m_slider);
    m_title->setText(title);
    m_keepGroup->setVisible(!editing);
    m_smart->setVisible(!editing);
    m_apply->setText(editing ? tr("Apply") : tr("Split"));
    m_apply->setToolTip(editing ? tr("Move the cut to here") : tr("Cut the shape here"));
    m_keepBoth->setChecked(true);
    m_tiltX->setValue(tiltX);
    m_tiltY->setValue(tiltY);
    m_offset->setRange(-1e5, 1e5);
    m_offset->setValue(offset);
    updateKeys();
}

void SplitPanel::setOffsetRange(double minOffset, double maxOffset)
{
    m_min = minOffset;
    m_max = maxOffset > minOffset ? maxOffset : minOffset + 1.0;
    syncSlider();
}

void SplitPanel::setSideNames(const QString& front, const QString& back)
{
    m_keepFront->setText(tr("Only the %1 (arrow side)").arg(front));
    m_keepBack->setText(tr("Only the %1").arg(back));
}

double SplitPanel::tiltX() const { return m_tiltX->value(); }
double SplitPanel::tiltY() const { return m_tiltY->value(); }
double SplitPanel::offset() const { return m_offset->value(); }

void SplitPanel::setOffset(double offset)
{
    m_offset->setValue(std::round(offset * 100.0) / 100.0);  // emits valueChanged -> planeChanged
}

SplitKeep SplitPanel::keep() const
{
    if (m_keepGroup->isHidden() || m_keepBoth->isChecked())
        return SplitKeep::Both;
    return m_keepFront->isChecked() ? SplitKeep::Front : SplitKeep::Back;
}

void SplitPanel::setTilts(double tiltX, double tiltY)
{
    {
        const QSignalBlocker b1(m_tiltX), b2(m_tiltY), b3(m_offset);
        m_tiltX->setValue(tiltX);
        m_tiltY->setValue(tiltY);
        m_offset->setValue(0.0);  // back through the middle
    }
    syncSlider();
    emit planeChanged();
}

void SplitPanel::offsetFromSlider(int value)
{
    const double v = m_min + (m_max - m_min) * value / kSliderSteps;
    const QSignalBlocker block(m_offset);
    m_offset->setValue(std::round(v * 10.0) / 10.0);  // 0.1 mm steps
    emit planeChanged();
}

void SplitPanel::syncSlider()
{
    const QSignalBlocker block(m_slider);
    const double t = (m_offset->value() - m_min) / (m_max - m_min);
    m_slider->setValue(static_cast<int>(std::lround(std::clamp(t, 0.0, 1.0) * kSliderSteps)));
}

}  // namespace forge::app
