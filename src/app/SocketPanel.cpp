// SPDX-License-Identifier: GPL-3.0-or-later
#include "SocketPanel.h"

#include "Standards.h"

#include "Fits.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <array>

namespace forge::app {

namespace {

const std::array<int, 6> kCutCounts{0, 2, 3, 4, 6, 8};

QDoubleSpinBox* millimetres(double low, double high, double step, int decimals = 2)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setSuffix(QObject::tr(" mm"));
    box->setKeyboardTracking(false);  // worked out once, when typing stops
    return box;
}

// A label with its combobox right under it (the label resting on top of the box).
QWidget* labelled(const QString& text, QComboBox* box)
{
    auto* holder = new QWidget;
    auto* column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(box);
    return holder;
}

}  // namespace

SocketPanel::SocketPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Cuts a socket that takes a ball or a peg once printed, with room for the fit. "
                               "Click the spot on the shape where the opening goes."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    m_placed = new QLabel;
    m_placed->setWordWrap(true);
    layout->addWidget(m_placed);
    layout->addSpacing(6);
    m_standardRow = new StandardRow;
    layout->addWidget(m_standardRow);

    m_kind = new QComboBox;
    m_kind->addItem(tr("A ball (ball joint)"));
    m_kind->addItem(tr("A peg"));
    m_kind->addItem(tr("A magnet"));
    m_kind->setToolTip(tr("What goes in the socket. A magnet: a round pocket it sits in, for swappable heads, "
                          "hands and accessories"));
    layout->addWidget(labelled(tr("Socket for:"), m_kind));

    // A ball: its size, and how it's held.
    m_ballBox = new QWidget;
    auto* ball = new QVBoxLayout(m_ballBox);
    ball->setContentsMargins(0, 0, 0, 0);
    auto* ballForm = new QFormLayout;
    m_ballSize = millimetres(0.2, 500.0, 0.1);
    m_ballSize->setToolTip(tr("The ball's diameter, as it's made (the socket is made bigger by the fit)"));
    ballForm->addRow(tr("Ball diameter:"), m_ballSize);
    ball->addLayout(ballForm);
    m_hold = new QComboBox;
    m_hold->addItem(tr("Snap-in: the opening grips the ball"));
    m_hold->addItem(tr("Open: the ball drops in"));
    m_hold->setToolTip(tr("Snap-in: the opening is a little narrower than the ball, so it pops in and stays. "
                          "Open: the ball drops in, to be glued or pinned."));
    ball->addWidget(labelled(tr("Hold:"), m_hold));
    auto* gripForm = new QFormLayout;
    m_grip = new QDoubleSpinBox;
    m_grip->setRange(0.0, 50.0);
    m_grip->setDecimals(0);
    m_grip->setSingleStep(1.0);
    m_grip->setSuffix(tr(" %"));
    m_grip->setKeyboardTracking(false);
    m_grip->setToolTip(tr("How much narrower the opening is than the ball. More holds it harder but needs more "
                          "force (and a flexible print, or relief cuts). 5–15% suits most."));
    m_gripLabel = new QLabel(tr("Grip:"));
    gripForm->addRow(m_gripLabel, m_grip);
    m_ridges = new QSpinBox;
    m_ridges->setRange(0, 24);
    m_ridges->setSpecialValueText(tr("None"));
    m_ridges->setKeyboardTracking(false);
    m_ridges->setToolTip(tr("Ridges left standing inside the cup that press on the ball, for a stiffer hold "
                            "(they sit between any relief cuts)"));
    gripForm->addRow(tr("Ridges:"), m_ridges);
    m_ridgeHeight = new QDoubleSpinBox;
    m_ridgeHeight->setRange(0.05, 2.0);
    m_ridgeHeight->setDecimals(2);
    m_ridgeHeight->setSingleStep(0.05);
    m_ridgeHeight->setSuffix(tr(" mm"));
    m_ridgeHeight->setKeyboardTracking(false);
    m_ridgeHeight->setToolTip(tr("How far the ridges stand in. More than the fit presses on the ball: the more, the "
                                 "stiffer"));
    gripForm->addRow(tr("Ridge height:"), m_ridgeHeight);
    ball->addLayout(gripForm);
    layout->addWidget(m_ballBox);

    // A magnet: its size, open or under a skin, and a matching pocket in the part it holds.
    m_magnetBox = new QWidget;
    auto* mag = new QVBoxLayout(m_magnetBox);
    mag->setContentsMargins(0, 0, 0, 0);
    m_magnetSize = new QComboBox;
    for (const char* size : {"6 \u00D7 2 mm", "5 \u00D7 1 mm", "5 \u00D7 2 mm", "4 \u00D7 2 mm", "3 \u00D7 1 mm", "3 \u00D7 2 mm",
                             "8 \u00D7 3 mm", "10 \u00D7 2 mm"})
        m_magnetSize->addItem(QString::fromUtf8(size));
    m_magnetSize->addItem(tr("Other size"));
    m_magnetSize->setToolTip(tr("A round magnet's diameter \u00D7 thickness"));
    mag->addWidget(labelled(tr("Magnet:"), m_magnetSize));
    auto* magForm = new QFormLayout;
    m_magnetDiameter = millimetres(0.5, 100.0, 0.5);
    m_magnetDiameter->setToolTip(tr("The magnet's diameter (the pocket is bigger by the fit)"));
    magForm->addRow(tr("Diameter:"), m_magnetDiameter);
    m_magnetThickness = millimetres(0.2, 100.0, 0.5);
    m_magnetThickness->setToolTip(tr("The magnet's thickness (the pocket is deeper by the fit, so it sits flush)"));
    magForm->addRow(tr("Thickness:"), m_magnetThickness);
    mag->addLayout(magForm);
    m_buried = new QCheckBox(tr("Hidden under a skin"));
    m_buried->setToolTip(tr("Leave a thin skin over the magnet: pause the print at that layer and drop the magnet "
                            "in (filament printers). Unticked: an open pocket, the magnet flush with the surface, "
                            "glued in."));
    mag->addWidget(m_buried);
    auto* coverForm = new QFormLayout;
    m_cover = millimetres(0.1, 5.0, 0.1);
    m_cover->setToolTip(tr("How thick the skin over the magnet is (thinner holds harder)"));
    coverForm->addRow(tr("Skin:"), m_cover);
    mag->addLayout(coverForm);
    m_matchingPocket = new QCheckBox(tr("A matching pocket in the part it holds"));
    m_matchingPocket->setToolTip(tr("The part that sits against this face (a head on a neck) gets a pocket too, "
                                    "lined up with this one"));
    mag->addWidget(m_matchingPocket);
    auto* pickRow = new QHBoxLayout;
    m_pickOther = new QPushButton(tr("Pick the Other Part"));
    m_pickOther->setToolTip(tr("Then click the part against this face"));
    m_otherName = new QLabel;
    m_otherName->setWordWrap(true);
    pickRow->addWidget(m_pickOther);
    pickRow->addWidget(m_otherName, 1);
    mag->addLayout(pickRow);
    layout->addWidget(m_magnetBox);

    // A peg: its shape, size and length.
    m_pegBox = new QWidget;
    auto* peg = new QVBoxLayout(m_pegBox);
    peg->setContentsMargins(0, 0, 0, 0);
    m_shape = new QComboBox;
    m_shape->addItem(tr("Round"));
    m_shape->addItem(tr("Square"));
    m_shape->addItem(tr("Hexagon"));
    m_shape->addItem(tr("Octagon"));
    m_shape->setToolTip(tr("A peg with flats (square, hexagon, octagon) can't turn in its socket"));
    peg->addWidget(labelled(tr("Peg shape:"), m_shape));
    auto* pegForm = new QFormLayout;
    m_pegSize = millimetres(0.2, 500.0, 0.1);
    m_pegSizeLabel = new QLabel(tr("Diameter:"));
    pegForm->addRow(m_pegSizeLabel, m_pegSize);
    m_pegDepth = millimetres(0.2, 1000.0, 0.5);
    m_pegDepth->setToolTip(tr("How far the peg goes in (the socket is this deep, plus the fit)"));
    pegForm->addRow(tr("Length:"), m_pegDepth);
    peg->addLayout(pegForm);
    layout->addWidget(m_pegBox);

    auto* centerGroup = new QGroupBox(tr("Center on the face"));
    centerGroup->setToolTip(tr("Moves the socket to the middle of the face it's on, so it's exactly central"));
    auto* centerRow = new QHBoxLayout(centerGroup);
    const QString centerNames[3] = {tr("All"), tr("Top-Bottom"), tr("Left-Right")};
    const QString centerTips[3] = {tr("Move it to the middle of the face it's on"),
                                   tr("Move it up or down to the middle of the face"),
                                   tr("Move it across to the middle of the face")};
    for (int i = 0; i < 3; ++i) {
        auto* button = new QPushButton(centerNames[i]);
        button->setToolTip(centerTips[i]);
        connect(button, &QPushButton::clicked, this, [this, i] { emit centerRequested(i); });
        centerRow->addWidget(button);
    }
    layout->addWidget(centerGroup);

    auto* turnForm = new QFormLayout;
    m_turn = new QDoubleSpinBox;
    m_turn->setRange(-360.0, 360.0);
    m_turn->setDecimals(1);
    m_turn->setSingleStep(15.0);
    m_turn->setSuffix(tr("°"));
    m_turn->setKeyboardTracking(false);
    m_turn->setToolTip(tr("Turns the socket round its middle: which way a peg's flats and the relief cuts face"));
    turnForm->addRow(tr("Turn:"), m_turn);
    layout->addLayout(turnForm);

    layout->addSpacing(6);
    m_printer = new QComboBox;
    m_printer->addItem(tr("Resin"));
    m_printer->addItem(tr("FDM (filament)"));
    m_printer->setToolTip(tr("Choosing one sets the fit to suit it (0.1 mm for resin, 0.2 mm for filament); "
                             "change the fit to suit your printer"));
    layout->addWidget(labelled(tr("Printer:"), m_printer));
    auto* fitForm = new QFormLayout;
    m_fit = millimetres(0.0, 5.0, 0.05);
    m_fit->setToolTip(tr("Room all round (and at the bottom of a peg socket), so printed parts go together. "
                         "The ball or peg keeps its size."));
    fitForm->addRow(tr("Fit:"), m_fit);
    layout->addLayout(fitForm);

    layout->addSpacing(6);
    m_cuts = new QComboBox;
    m_cuts->addItem(tr("None"));
    for (std::size_t i = 1; i < kCutCounts.size(); ++i)
        m_cuts->addItem(QString::number(kCutCounts[i]));
    m_cuts->setToolTip(tr("Strain relief: slots from the opening down through the socket's side, so it can "
                          "open a little as the ball or peg goes in, instead of cracking"));
    m_cutsRow = labelled(tr("Relief cuts:"), m_cuts);
    layout->addWidget(m_cutsRow);
    m_cutBox = new QWidget;
    auto* cutForm = new QFormLayout(m_cutBox);
    cutForm->setContentsMargins(0, 0, 0, 0);
    m_cutWidth = millimetres(0.05, 50.0, 0.05);
    m_cutWidth->setToolTip(tr("How wide each slot is"));
    cutForm->addRow(tr("Cut width:"), m_cutWidth);
    m_cutDepth = millimetres(0.0, 1000.0, 0.25);
    m_cutDepth->setSpecialValueText(tr("Auto"));
    m_cutDepth->setToolTip(tr("How far down from the opening the slots go. Auto: to the ball's middle (half way "
                              "down a peg socket)."));
    cutForm->addRow(tr("Cut depth:"), m_cutDepth);
    layout->addWidget(m_cutBox);

    layout->addSpacing(6);
    m_matching = new QPushButton;
    layout->addWidget(m_matching);

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

    for (QComboBox* box : {m_kind, m_hold, m_shape, m_cuts})
        connect(box, &QComboBox::currentIndexChanged, this, [this] {
            showKind();
            emit changed();
        });
    // The fit found with the tolerance test print for this printer and joint, if any.
    const auto fitKind = [this] { return m_kind->currentIndex() == 0 ? tolerance::Kind::Ball : tolerance::Kind::Peg; };
    connect(m_printer, &QComboBox::currentIndexChanged, this, [this, fitKind](int index) {
        m_fit->setValue(fits::starting(index == 0, fitKind()));  // emits changed
    });
    connect(m_kind, &QComboBox::currentIndexChanged, this, [this, fitKind] {
        if (const auto f = fits::measured(m_printer->currentIndex() == 0, fitKind()))
            m_fit->setValue(*f);
    });
    connect(m_magnetSize, &QComboBox::currentIndexChanged, this, [this](int index) {
        static const double sizes[][2] = {{6, 2}, {5, 1}, {5, 2}, {4, 2}, {3, 1}, {3, 2}, {8, 3}, {10, 2}};
        if (index >= 0 && index < int(std::size(sizes))) {
            const QSignalBlocker a(m_magnetDiameter), b(m_magnetThickness);
            m_magnetDiameter->setValue(sizes[index][0]);
            m_magnetThickness->setValue(sizes[index][1]);
        }
        showKind();
        emit changed();
    });
    for (QDoubleSpinBox* box : {m_magnetDiameter, m_magnetThickness, m_cover})
        connect(box, &QDoubleSpinBox::valueChanged, this, &SocketPanel::changed);
    for (QCheckBox* box : {m_buried, m_matchingPocket})
        connect(box, &QCheckBox::toggled, this, [this] {
            showKind();
            emit changed();
        });
    connect(m_pickOther, &QPushButton::clicked, this, &SocketPanel::pickOtherRequested);
    connect(m_ridges, &QSpinBox::valueChanged, this, [this] {
        m_ridgeHeight->setEnabled(m_ridges->value() > 0);
        emit changed();
    });
    for (QDoubleSpinBox* box :
         {m_ballSize, m_grip, m_pegSize, m_pegDepth, m_turn, m_fit, m_cutWidth, m_cutDepth, m_ridgeHeight})
        connect(box, &QDoubleSpinBox::valueChanged, this, &SocketPanel::changed);
    connect(m_matching, &QPushButton::clicked, this, &SocketPanel::addMatchingRequested);
    connect(m_apply, &QPushButton::clicked, this, &SocketPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &SocketPanel::cancelRequested);
    setFinish(SocketFinish{});
}

void SocketPanel::showKind()
{
    const bool isBall = m_kind->currentIndex() == 0;
    const bool isMagnet = m_kind->currentIndex() == 2;
    m_ballBox->setVisible(isBall);
    m_pegBox->setVisible(!isBall && !isMagnet);
    m_magnetBox->setVisible(isMagnet);
    m_cutsRow->setVisible(!isMagnet);
    const bool other = m_magnetSize->currentIndex() == m_magnetSize->count() - 1;
    m_magnetDiameter->setEnabled(other);
    m_magnetThickness->setEnabled(other);
    m_cover->setEnabled(m_buried->isChecked());
    m_pickOther->setVisible(m_matchingPocket->isChecked() && m_matchingPocket->isEnabled());
    m_otherName->setVisible(m_pickOther->isVisible());
    const bool snap = m_hold->currentIndex() == 0;
    m_gripLabel->setVisible(snap);
    m_grip->setVisible(snap);
    m_pegSizeLabel->setText(m_shape->currentIndex() == 0 ? tr("Diameter:") : tr("Across flats:"));
    m_pegSize->setToolTip(m_shape->currentIndex() == 0
                              ? tr("The peg's diameter, as it's made (the socket is made bigger by the fit)")
                              : tr("The peg's width from one flat side to the opposite one, as it's made"));
    m_cutBox->setVisible(!isMagnet && m_cuts->currentIndex() > 0);
    m_matching->setText(isBall ? tr("Add the Matching Ball") : isMagnet ? tr("Add a Stand-in Magnet")
                                                                        : tr("Add the Matching Peg"));
    m_matching->setToolTip(isBall ? tr("Adds a ball this socket takes, at its true size, beside the shapes")
                                  : tr("Adds a peg this socket takes, at its true size, beside the shapes"));
}

void SocketPanel::start(const QString& title, bool editing)
{
    m_title->setText(title);
    m_apply->setToolTip(editing ? tr("Change the socket") : tr("Cut the socket"));
    m_matchingPocket->setEnabled(!editing);  // each pocket is changed on its own
    setOtherPart({}, false);
    setPlaced(editing);
}

void SocketPanel::setFinish(const SocketFinish& f)
{
    {
        const QSignalBlocker b0(m_kind), b1(m_hold), b2(m_shape), b3(m_cuts), b4(m_printer), b5(m_ballSize),
            b6(m_grip), b7(m_pegSize), b8(m_pegDepth), b9(m_turn), b10(m_fit), b11(m_cutWidth), b12(m_cutDepth);
        m_kind->setCurrentIndex(f.kind == SocketKind::Ball ? 0 : f.kind == SocketKind::Magnet ? 2 : 1);
        const QSignalBlocker m1(m_magnetSize), m2(m_magnetDiameter), m3(m_magnetThickness), m4(m_buried), m5(m_cover);
        if (f.kind == SocketKind::Magnet) {
            static const double sizes[][2] = {{6, 2}, {5, 1}, {5, 2}, {4, 2}, {3, 1}, {3, 2}, {8, 3}, {10, 2}};
            int preset = m_magnetSize->count() - 1;
            for (int i = 0; i < int(std::size(sizes)); ++i)
                if (std::abs(sizes[i][0] - f.size) < 1e-9 && std::abs(sizes[i][1] - f.depth) < 1e-9)
                    preset = i;
            m_magnetSize->setCurrentIndex(preset);
            m_magnetDiameter->setValue(f.size);
            m_magnetThickness->setValue(f.depth);
        } else if (m_magnetDiameter->value() <= 0.5) {
            m_magnetSize->setCurrentIndex(0);
            m_magnetDiameter->setValue(6.0);
            m_magnetThickness->setValue(2.0);
        }
        m_buried->setChecked(f.cover > 0.0);
        m_cover->setValue(f.cover > 0.0 ? f.cover : 0.6);
        m_ballSize->setValue(f.size);
        m_pegSize->setValue(f.size);
        m_hold->setCurrentIndex(f.snap ? 0 : 1);
        m_grip->setValue(f.grip);
        const QSignalBlocker r1(m_ridges), r2(m_ridgeHeight);
        m_ridges->setValue(f.ridges);
        m_ridgeHeight->setValue(f.ridgeHeight);
        m_ridgeHeight->setEnabled(f.ridges > 0);
        m_shape->setCurrentIndex(int(f.shape));
        m_pegDepth->setValue(f.depth);
        m_turn->setValue(f.turn);
        m_printer->setCurrentIndex(f.resin ? 0 : 1);
        m_fit->setValue(f.fit);
        int cuts = 0;
        for (std::size_t i = 0; i < kCutCounts.size(); ++i)
            if (kCutCounts[i] == f.reliefCuts)
                cuts = int(i);
        m_cuts->setCurrentIndex(cuts);
        m_cutWidth->setValue(f.reliefWidth);
        m_cutDepth->setValue(f.reliefDepth);
    }
    showKind();
}

SocketFinish SocketPanel::finish() const
{
    SocketFinish f;
    f.kind = m_kind->currentIndex() == 0 ? SocketKind::Ball : m_kind->currentIndex() == 2 ? SocketKind::Magnet
                                                                                       : SocketKind::Peg;
    f.size = f.kind == SocketKind::Ball ? m_ballSize->value()
             : f.kind == SocketKind::Magnet ? m_magnetDiameter->value()
                                            : m_pegSize->value();
    f.depth = f.kind == SocketKind::Magnet ? m_magnetThickness->value() : m_pegDepth->value();
    f.cover = f.kind == SocketKind::Magnet && m_buried->isChecked() ? m_cover->value() : 0.0;
    f.snap = m_hold->currentIndex() == 0;
    f.grip = m_grip->value();
    f.ridges = f.kind == SocketKind::Ball ? m_ridges->value() : 0;
    f.ridgeHeight = m_ridgeHeight->value();
    f.shape = PegShape(m_shape->currentIndex());
    f.turn = m_turn->value();
    f.resin = m_printer->currentIndex() == 0;
    f.fit = m_fit->value();
    f.reliefCuts = f.kind == SocketKind::Magnet ? 0 : kCutCounts[std::size_t(std::max(0, m_cuts->currentIndex()))];
    f.reliefWidth = m_cutWidth->value();
    f.reliefDepth = m_cutDepth->value();
    return f;
}

bool SocketPanel::matchingPocket() const
{
    return m_kind->currentIndex() == 2 && m_matchingPocket->isEnabled() && m_matchingPocket->isChecked();
}

void SocketPanel::setOtherPart(const QString& name, bool picking)
{
    m_pickOther->setText(picking ? tr("Picking...") : tr("Pick the Other Part"));
    m_otherName->setText(picking          ? tr("<i>Click the part against this face.</i>")
                         : name.isEmpty() ? tr("<i>None picked yet.</i>")
                                          : tr("<b>%1</b>").arg(name.toHtmlEscaped()));
}

void SocketPanel::setPlaced(bool placed)
{
    m_placed->setText(placed ? tr("Click elsewhere on the shape to move the socket.")
                             : tr("<i>Click the spot on the shape where the socket's opening goes.</i>"));
}

void SocketPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void SocketPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
