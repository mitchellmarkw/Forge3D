// SPDX-License-Identifier: GPL-3.0-or-later
#include "SocketPanel.h"

#include "Fits.h"

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

    m_kind = new QComboBox;
    m_kind->addItem(tr("A ball (ball joint)"));
    m_kind->addItem(tr("A peg"));
    m_kind->setToolTip(tr("What goes in the socket"));
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
    layout->addWidget(labelled(tr("Relief cuts:"), m_cuts));
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
    m_ballBox->setVisible(isBall);
    m_pegBox->setVisible(!isBall);
    const bool snap = m_hold->currentIndex() == 0;
    m_gripLabel->setVisible(snap);
    m_grip->setVisible(snap);
    m_pegSizeLabel->setText(m_shape->currentIndex() == 0 ? tr("Diameter:") : tr("Across flats:"));
    m_pegSize->setToolTip(m_shape->currentIndex() == 0
                              ? tr("The peg's diameter, as it's made (the socket is made bigger by the fit)")
                              : tr("The peg's width from one flat side to the opposite one, as it's made"));
    m_cutBox->setVisible(m_cuts->currentIndex() > 0);
    m_matching->setText(isBall ? tr("Add the Matching Ball") : tr("Add the Matching Peg"));
    m_matching->setToolTip(isBall ? tr("Adds a ball this socket takes, at its true size, beside the shapes")
                                  : tr("Adds a peg this socket takes, at its true size, beside the shapes"));
}

void SocketPanel::start(const QString& title, bool editing)
{
    m_title->setText(title);
    m_apply->setToolTip(editing ? tr("Change the socket") : tr("Cut the socket"));
    setPlaced(editing);
}

void SocketPanel::setFinish(const SocketFinish& f)
{
    {
        const QSignalBlocker b0(m_kind), b1(m_hold), b2(m_shape), b3(m_cuts), b4(m_printer), b5(m_ballSize),
            b6(m_grip), b7(m_pegSize), b8(m_pegDepth), b9(m_turn), b10(m_fit), b11(m_cutWidth), b12(m_cutDepth);
        m_kind->setCurrentIndex(f.kind == SocketKind::Ball ? 0 : 1);
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
    f.kind = m_kind->currentIndex() == 0 ? SocketKind::Ball : SocketKind::Peg;
    f.size = f.kind == SocketKind::Ball ? m_ballSize->value() : m_pegSize->value();
    f.depth = m_pegDepth->value();
    f.snap = m_hold->currentIndex() == 0;
    f.grip = m_grip->value();
    f.ridges = f.kind == SocketKind::Ball ? m_ridges->value() : 0;
    f.ridgeHeight = m_ridgeHeight->value();
    f.shape = PegShape(m_shape->currentIndex());
    f.turn = m_turn->value();
    f.resin = m_printer->currentIndex() == 0;
    f.fit = m_fit->value();
    f.reliefCuts = kCutCounts[std::size_t(std::max(0, m_cuts->currentIndex()))];
    f.reliefWidth = m_cutWidth->value();
    f.reliefDepth = m_cutDepth->value();
    return f;
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
