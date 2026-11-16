// SPDX-License-Identifier: GPL-3.0-or-later
#include "SketchPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
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
    m_makeGroup = makeGroup;
    auto* makeLayout = new QVBoxLayout(makeGroup);
    makeLayout->setSpacing(2);
    // The label sits right on top of its list.
    m_makeLabel = new QLabel(tr("Make:"));
    makeLayout->addWidget(m_makeLabel);
    m_make = new QComboBox;
    m_make->addItem(tr("Pad (push the outline out)"), int(Make::Pad));
    m_make->addItem(tr("Pocket (cut the outline in)"), int(Make::Pocket));
    m_make->addItem(tr("Revolve (spin it round a helper line)"), int(Make::Revolve));
    m_make->addItem(tr("Groove (spin it round, cutting in)"), int(Make::Groove));
    m_make->addItem(tr("Loft (blend up to other outlines)"), int(Make::Loft));
    m_make->addItem(tr("Sweep (along a path, or a spiral)"), int(Make::Sweep));
    m_make->setToolTip(tr("Pad adds a solid shaped like the outline; Pocket cuts it into the shape you sketched "
                          "on. Revolve spins the outline round a helper line (a knob, a bottle); Groove cuts "
                          "that in. Loft blends the outline into others drawn above it (a square base to a "
                          "round top). Sweep moves the outline along a path you draw (a pipe, a handle) or "
                          "round a spiral (a spring)."));
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
    m_angleLabel = new QLabel(tr("Angle:"));
    makeLayout->addWidget(m_angleLabel);
    m_angle = new QDoubleSpinBox;
    m_angle->setRange(0.1, 360.0);
    m_angle->setDecimals(1);
    m_angle->setSingleStep(15.0);
    m_angle->setSuffix(QStringLiteral("\u00B0"));
    m_angle->setToolTip(tr("How far round it spins (360\u00B0: all the way)"));
    makeLayout->addWidget(m_angle);
    m_axisHelp = new QLabel(tr("It spins round the helper line you pick (or the only one there is). Draw a line "
                               "and press G (Helper) to make one."));
    m_axisHelp->setWordWrap(true);
    m_axisHelp->setStyleSheet(QStringLiteral("color: palette(mid);"));
    makeLayout->addWidget(m_axisHelp);
    makeLayout->addSpacing(6);
    m_otherWay = new QCheckBox(tr("The other way"));
    m_bothWays = new QCheckBox(tr("Both ways (half each side)"));
    m_throughAll = new QCheckBox(tr("Through all"));
    m_throughAll->setToolTip(tr("Cut right through the shape, however thick it gets"));
    makeLayout->addWidget(m_otherWay);
    makeLayout->addWidget(m_bothWays);
    makeLayout->addWidget(m_throughAll);

    // Loft: the outlines one by one, each at its height.
    m_loftGroup = new QWidget;
    auto* loft = new QVBoxLayout(m_loftGroup);
    loft->setContentsMargins(0, 0, 0, 0);
    m_loftOutline = new QLabel;
    QFont boldish = m_loftOutline->font();
    boldish.setBold(true);
    m_loftOutline->setFont(boldish);
    loft->addWidget(m_loftOutline);
    m_loftHeightLabel = new QLabel(tr("Height of this outline:"));
    loft->addWidget(m_loftHeightLabel);
    m_loftHeight = new QDoubleSpinBox;
    m_loftHeight->setRange(0.0, 10000.0);
    m_loftHeight->setDecimals(2);
    m_loftHeight->setSingleStep(1.0);
    m_loftHeight->setSuffix(tr(" mm"));
    m_loftHeight->setToolTip(tr("How far above the first outline (the sketch plane) this one is"));
    loft->addWidget(m_loftHeight);
    auto* steps = new QHBoxLayout;
    m_below = new QPushButton(tr("◀ Below"));
    m_below->setToolTip(tr("Go to the outline below this one"));
    m_above = new QPushButton;
    steps->addWidget(m_below);
    steps->addWidget(m_above);
    loft->addLayout(steps);
    m_removeOutline = new QPushButton(tr("Remove this outline"));
    m_removeOutline->setToolTip(tr("Take this outline out of the loft (the first one can't be removed)"));
    loft->addWidget(m_removeOutline);
    m_ruled = new QCheckBox(tr("Straight sides (not a smooth blend)"));
    loft->addWidget(m_ruled);
    auto* loftHelp = new QLabel(tr("Each outline is one closed shape. The next one starts as a copy of this one: "
                                   "change it to what it becomes higher up (the one below shows in grey)."));
    loftHelp->setWordWrap(true);
    loftHelp->setStyleSheet(QStringLiteral("color: palette(mid);"));
    loft->addWidget(loftHelp);
    makeLayout->addWidget(m_loftGroup);

    // Sweep: along a path drawn standing up from the outline, or round a spiral.
    m_sweepGroup = new QWidget;
    auto* sweep = new QVBoxLayout(m_sweepGroup);
    sweep->setContentsMargins(0, 0, 0, 0);
    sweep->addWidget(new QLabel(tr("Along:")));
    m_along = new QComboBox;
    m_along->addItem(tr("A path you draw"));
    m_along->addItem(tr("A spiral (a spring, a coil)"));
    sweep->addWidget(m_along);
    m_spiralGroup = new QWidget;
    auto* spiral = new QFormLayout(m_spiralGroup);
    spiral->setContentsMargins(0, 0, 0, 0);
    m_pitch = new QDoubleSpinBox;
    m_pitch->setRange(0.1, 10000.0);
    m_pitch->setDecimals(2);
    m_pitch->setSuffix(tr(" mm"));
    m_pitch->setToolTip(tr("How far it rises each turn (more than the outline's height, or the turns touch)"));
    spiral->addRow(tr("Pitch:"), m_pitch);
    m_turns = new QDoubleSpinBox;
    m_turns->setRange(0.1, 1000.0);
    m_turns->setDecimals(2);
    m_turns->setToolTip(tr("How many times round"));
    spiral->addRow(tr("Turns:"), m_turns);
    m_leftHand = new QCheckBox(tr("Turns the other way (left-hand)"));
    spiral->addRow(m_leftHand);
    sweep->addWidget(m_spiralGroup);
    m_pathButton = new QPushButton;
    sweep->addWidget(m_pathButton);
    m_sweepHelp = new QLabel;
    m_sweepHelp->setWordWrap(true);
    m_sweepHelp->setStyleSheet(QStringLiteral("color: palette(mid);"));
    sweep->addWidget(m_sweepHelp);
    makeLayout->addWidget(m_sweepGroup);
    layout->addWidget(makeGroup);

    auto* help = new QLabel(tr("<b>Draw</b>: Line (L), Rectangle (R), Circle (C), Arc (A), Slot (S), Point (P). "
                               "Esc stops drawing, then goes back to picking.<br>"
                               "<b>Pick</b>: click; Ctrl+click picks more. Drag points and lines to move them.<br>"
                               "<b>Rules</b> go on what's picked: Horizontal (H), Vertical (V), Parallel, "
                               "Perpendicular, Equal (E), Tangent (T), Coincident (O), Symmetric, Lock.<br>"
                               "<b>Dimension</b> (D): a line's length, two points' distance, a circle's radius or "
                               "two lines' angle. Double-click a dimension to change it.<br>"
                               "Shapes go <span style='color:#1e8c3c'>green</span> when every size and place "
                               "is set. <span style='color:#d22'>Red</span> points are loose ends.<br>"
                               "Wheel zooms; right- or middle-drag pans; F fits."));
    m_help = help;
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
    connect(m_make, &QComboBox::currentIndexChanged, this, &SketchPanel::makeChanged);
    connect(m_along, &QComboBox::currentIndexChanged, this, &SketchPanel::updateLabels);
    connect(m_below, &QPushButton::clicked, this, [this] { emit outlineRequested(-1); });
    connect(m_above, &QPushButton::clicked, this, [this] { emit outlineRequested(1); });
    connect(m_removeOutline, &QPushButton::clicked, this, [this] { emit outlineRequested(0); });
    connect(m_pathButton, &QPushButton::clicked, this, [this] { emit pathRequested(!m_pathStage); });
    connect(m_bothWays, &QCheckBox::toggled, this, &SketchPanel::updateLabels);
    connect(m_otherWay, &QCheckBox::toggled, this, &SketchPanel::updateLabels);
    connect(m_throughAll, &QCheckBox::toggled, this, &SketchPanel::updateLabels);
    connect(m_finish, &QPushButton::clicked, this, &SketchPanel::finishRequested);
    connect(cancel, &QPushButton::clicked, this, &SketchPanel::cancelRequested);
}

void SketchPanel::start(const QString& title, bool onFace, bool editing, const Options& options)
{
    m_title->setText(title);
    m_makeGroup->show();
    const QSignalBlocker b0(m_make), b1(m_bothWays), b2(m_throughAll), b3(m_otherWay);
    // On the plate there's nothing to cut into. Changing one keeps it adding or cutting.
    const bool cutting = options.make == Make::Pocket || options.make == Make::Groove;
    auto* model = qobject_cast<QStandardItemModel*>(m_make->model());
    for (int i = 0; i < m_make->count(); ++i) {
        const auto kind = Make(m_make->itemData(i).toInt());
        const bool cuts = kind == Make::Pocket || kind == Make::Groove;
        bool allowed = onFace || !cuts;
        if (editing)
            allowed = cuts == cutting;
        if (model)
            if (QStandardItem* item = model->item(i))
                item->setEnabled(allowed);
    }
    m_make->setCurrentIndex(m_make->findData(int(options.make)));
    m_length->setValue(options.length);
    m_angle->setValue(options.angle);
    m_otherWay->setChecked(options.otherWay);
    m_bothWays->setChecked(options.bothWays);
    m_throughAll->setChecked(options.throughAll);
    m_ruled->setChecked(options.ruled);
    {
        const QSignalBlocker b4(m_along);
        m_along->setCurrentIndex(options.spiral ? 1 : 0);
    }
    m_pitch->setValue(options.pitch);
    m_turns->setValue(options.turns);
    m_leftHand->setChecked(options.leftHand);
    m_pathStage = false;
    m_outline = 0;
    m_outlines = 1;
    m_finish->setText(tr("Finish"));
    m_finish->setToolTip(editing ? tr("Change it to this sketch") : tr("Make it from this sketch"));
    setMessage({}, false);
    updateLabels();
}

void SketchPanel::startPositions(const QString& title, const QString& message)
{
    m_title->setText(title);
    m_makeGroup->hide();
    m_finish->setText(tr("Done"));
    m_finish->setToolTip(tr("Use this and go back"));
    setMessage(message, false);
}

SketchPanel::Make SketchPanel::make() const
{
    return Make(m_make->currentData().toInt());
}

double SketchPanel::length() const
{
    return m_length->value();
}

double SketchPanel::angle() const
{
    return m_angle->value();
}

bool SketchPanel::otherWay() const
{
    return m_otherWay->isChecked();
}

bool SketchPanel::bothWays() const
{
    return m_bothWays->isChecked() && !throughAll();
}

bool SketchPanel::throughAll() const
{
    return m_throughAll->isChecked() && make() == Make::Pocket;
}

void SketchPanel::setLoftOutline(int index, int count, double height)
{
    m_outline = index;
    m_outlines = count;
    m_loftHeight->setValue(height);
    updateLabels();
}

double SketchPanel::loftHeight() const
{
    return m_outline == 0 ? 0.0 : m_loftHeight->value();
}

bool SketchPanel::ruled() const
{
    return m_ruled->isChecked();
}

bool SketchPanel::spiral() const
{
    return m_along->currentIndex() == 1;
}

double SketchPanel::pitch() const
{
    return m_pitch->value();
}

double SketchPanel::turns() const
{
    return m_turns->value();
}

bool SketchPanel::leftHand() const
{
    return m_leftHand->isChecked();
}

void SketchPanel::setSweepStage(bool path)
{
    m_pathStage = path;
    updateLabels();
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
    const Make kind = make();
    const bool spin = revolve();
    const bool cut = pocket();
    const bool loft = kind == Make::Loft, sweep = kind == Make::Sweep;
    m_loftGroup->setVisible(loft);
    m_sweepGroup->setVisible(sweep);
    if (loft) {
        m_loftOutline->setText(tr("Outline %1 of %2").arg(m_outline + 1).arg(std::max(m_outlines, 2)));
        m_loftHeightLabel->setEnabled(m_outline > 0);
        m_loftHeight->setEnabled(m_outline > 0);
        m_below->setEnabled(m_outline > 0);
        m_removeOutline->setEnabled(m_outline > 0);
        m_above->setText(m_outline + 1 >= m_outlines ? tr("Add one above ▶") : tr("Above ▶"));
        m_above->setToolTip(m_outline + 1 >= m_outlines ? tr("Draw another outline higher up (it starts as a copy)")
                                                        : tr("Go to the outline above this one"));
    }
    if (sweep) {
        const bool spiralling = spiral();
        m_spiralGroup->setVisible(spiralling);
        m_pathButton->setVisible(!spiralling);
        m_pathButton->setText(m_pathStage ? tr("◀ Back to the outline") : tr("Draw the path ▶"));
        m_sweepHelp->setText(spiralling ? tr("Draw the outline as seen from the side, to one side of the up-down "
                                             "line through the dot: it goes round that line, rising as it goes.")
                             : m_pathStage ? tr("The path, seen from the side: it starts at the dot (where the "
                                                "outline is, the grey line across) and goes up out of the "
                                                "sketch. Lines and arcs, joined end to end.")
                                           : tr("Draw the outline (one closed shape), then the path it goes along."));
    }
    m_makeLabel->setEnabled(!m_pathStage && m_outline == 0);
    m_make->setEnabled(!m_pathStage && m_outline == 0);
    if (loft || sweep) {
        for (QWidget* w : {static_cast<QWidget*>(m_lengthLabel), static_cast<QWidget*>(m_length),
                           static_cast<QWidget*>(m_angleLabel), static_cast<QWidget*>(m_angle),
                           static_cast<QWidget*>(m_axisHelp), static_cast<QWidget*>(m_otherWay),
                           static_cast<QWidget*>(m_bothWays), static_cast<QWidget*>(m_throughAll)})
            w->setVisible(false);
        return;
    }
    m_lengthLabel->setText(cut ? tr("Depth:") : tr("Length:"));
    m_length->setToolTip(cut ? tr("How deep the pocket goes into the shape")
                             : tr("How far the outline is pushed out"));
    m_lengthLabel->setVisible(!spin);
    m_length->setVisible(!spin);
    m_angleLabel->setVisible(spin);
    m_angle->setVisible(spin);
    m_axisHelp->setVisible(spin);
    m_otherWay->setVisible(true);
    m_bothWays->setVisible(true);
    m_throughAll->setVisible(kind == Make::Pocket);
    m_otherWay->setToolTip(spin  ? tr("Spin the other way round")
                           : cut ? tr("Cut outwards from the face instead of into it")
                                 : tr("Push the other way (down / into the face)"));
    const bool through = kind == Make::Pocket && m_throughAll->isChecked();
    m_otherWay->setEnabled(!m_bothWays->isChecked() && !through);
    m_bothWays->setEnabled(!m_otherWay->isChecked() && !through);
    m_length->setEnabled(!(kind == Make::Pocket && m_throughAll->isChecked()));
    m_lengthLabel->setEnabled(m_length->isEnabled());
}

}  // namespace forge::app
