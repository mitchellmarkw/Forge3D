// SPDX-License-Identifier: GPL-3.0-or-later
#include "HingePanel.h"

#include "FormulaFields.h"

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

#include <cmath>

namespace forge::app {

namespace {

constexpr double kFilament = 1.75;

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

HingePanel::HingePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Cuts a fork (a slot with a pin hole across it) into this part, with a tab that "
                               "turns in it on a pin, sized to fit once printed. Click the spot on the face where "
                               "the joint goes (the end of a thigh, say)."));
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
    m_kind->addItem(tr("Single hinge"));
    m_kind->addItem(tr("Double hinge"));
    m_kind->addItem(tr("Swivel"));
    m_kind->setToolTip(tr("Single: a fork and a tab on one pin. Double: a fork in each part and a link "
                          "between them on two pins, so it bends further. Swivel: a peg that turns in a bore, "
                          "held by a lip."));
    layout->addWidget(labelled(tr("Joint:"), m_kind));
    m_kindHelp = new QLabel;
    m_kindHelp->setWordWrap(true);
    m_kindHelp->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_kindHelp);

    m_otherBox = new QWidget;
    auto* otherColumn = new QVBoxLayout(m_otherBox);
    otherColumn->setContentsMargins(0, 0, 0, 0);
    m_other = new QComboBox;
    m_other->addItem(QString());
    m_other->addItem(QString());
    otherColumn->addWidget(labelled(tr("The other half:"), m_other));
    auto* pickRow = new QHBoxLayout;
    m_pickOther = new QPushButton(tr("Pick the Other Part"));
    m_pickOther->setToolTip(tr("Then click the other part in the view (the shin, say)"));
    m_otherName = new QLabel;
    m_otherName->setWordWrap(true);
    pickRow->addWidget(m_pickOther);
    pickRow->addWidget(m_otherName, 1);
    otherColumn->addLayout(pickRow);
    layout->addWidget(m_otherBox);

    auto* sizes = new QFormLayout;
    m_width = millimetres(0.5, 500.0, 0.25);
    m_widthLabel = new QLabel;
    sizes->addRow(m_widthLabel, m_width);
    m_barrel = millimetres(0.5, 500.0, 0.25);
    m_barrelLabel = new QLabel;
    sizes->addRow(m_barrelLabel, m_barrel);
    m_gap = millimetres(0.0, 100.0, 0.25);
    m_gap->setToolTip(tr("Room between the two parts' faces, straight"));
    m_gapLabel = new QLabel(tr("Gap between parts:"));
    sizes->addRow(m_gapLabel, m_gap);
    m_lip = millimetres(0.05, 50.0, 0.05);
    m_lip->setToolTip(tr("How far the lip round the peg's end stands out. It snaps into a groove at the bottom "
                         "of the bore and holds the peg in."));
    m_lipLabel = new QLabel(tr("Lip:"));
    sizes->addRow(m_lipLabel, m_lip);
    layout->addLayout(sizes);

    m_pinBox = new QWidget;
    auto* pinColumn = new QVBoxLayout(m_pinBox);
    pinColumn->setContentsMargins(0, 0, 0, 0);
    m_pin = new QComboBox;
    m_pin->addItem(tr("Printed pin"));
    m_pin->addItem(tr("Filament (1.75 mm)"));
    m_pin->addItem(tr("Wire or rod"));
    m_pin->setToolTip(tr("Printed: a pin shape is added to print with the parts. Filament: cut from a length of "
                         "1.75 mm filament. Wire or rod: brass or steel, the size you type."));
    pinColumn->addWidget(labelled(tr("Pin:"), m_pin));
    auto* pinForm = new QFormLayout;
    m_pinSize = millimetres(0.3, 50.0, 0.05);
    m_pinSize->setToolTip(tr("The pin's diameter. The holes are made bigger by the fit."));
    pinForm->addRow(tr("Pin diameter:"), m_pinSize);
    pinColumn->addLayout(pinForm);
    m_stiff = new QCheckBox(tr("Stiff: holds a pose"));
    m_stiff->setToolTip(tr("Ticked: the tab's pin hole is snug, so the joint holds where you leave it. "
                           "Unticked: it turns freely."));
    pinColumn->addWidget(m_stiff);
    m_swingBox = new QWidget;
    auto* swingColumn = new QVBoxLayout(m_swingBox);
    swingColumn->setContentsMargins(0, 0, 0, 0);
    m_swing = new QComboBox;
    m_swing->addItem(tr("Both ways"));
    m_swing->addItem(tr("One way (forward)"));
    m_swing->addItem(tr("One way (back)"));
    m_swing->setToolTip(tr("Which way the joint bends from straight. One way: the slot is only cut deep on that "
                           "side, so the joint stops straight the other way (like a knee)."));
    swingColumn->addWidget(labelled(tr("Bends:"), m_swing));
    pinColumn->addWidget(m_swingBox);
    layout->addWidget(m_pinBox);
    auto* clickForm = new QFormLayout;
    m_clicks = new QSpinBox;
    m_clicks->setRange(0, 36);
    m_clicks->setSpecialValueText(tr("None"));
    m_clicks->setKeyboardTracking(false);
    m_clicks->setToolTip(tr("A ratchet: ridges on the tab's faces (a swivel: along its peg) that snap into "
                            "grooves, so the joint clicks round and holds a pose. How many clicks in a whole turn"));
    clickForm->addRow(tr("Clicks a turn:"), m_clicks);
    m_clickDepth = millimetres(0.05, 2.0, 0.05);
    m_clickDepth->setToolTip(tr("How far the ridges stand out. More than the fit to click: the more, the firmer"));
    clickForm->addRow(tr("Click depth:"), m_clickDepth);
    layout->addLayout(clickForm);

    auto* placeForm = new QFormLayout;
    auto* turnRow = new QHBoxLayout;
    m_turn = new QDoubleSpinBox;
    m_turn->setRange(-360.0, 360.0);
    m_turn->setDecimals(1);
    m_turn->setSingleStep(15.0);
    m_turn->setSuffix(tr("°"));
    m_turn->setKeyboardTracking(false);
    m_turn->setToolTip(tr("Turns the joint round the spot: which way the pin lies, so which way it bends"));
    auto* quarter = new QPushButton(tr("+90°"));
    quarter->setToolTip(tr("Turn the pin a quarter round"));
    turnRow->addWidget(m_turn, 1);
    turnRow->addWidget(quarter);
    placeForm->addRow(tr("Turn:"), turnRow);
    m_reach = millimetres(0.0, 1000.0, 0.5);
    m_reach->setSpecialValueText(tr("Auto"));
    m_reach->setToolTip(tr("How far the slot and the pin hole go out each side of the spot (right through a "
                           "limb). Auto: twice the tab's end plus its thickness. Make it smaller if they reach "
                           "something else."));
    placeForm->addRow(tr("Reach:"), m_reach);
    layout->addLayout(placeForm);

    auto* centerGroup = new QGroupBox(tr("Center on the face"));
    centerGroup->setToolTip(tr("Moves the joint to the middle of the face it's on, so it's exactly central"));
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

    layout->addSpacing(6);
    m_printer = new QComboBox;
    m_printer->addItem(tr("Resin"));
    m_printer->addItem(tr("FDM (filament)"));
    m_printer->setToolTip(tr("Choosing one sets the fit to suit it (0.1 mm for resin, 0.2 mm for filament); "
                             "change the fit to suit your printer"));
    layout->addWidget(labelled(tr("Printer:"), m_printer));
    auto* fitForm = new QFormLayout;
    m_fit = millimetres(0.0, 1.0, 0.05);
    m_fit->setToolTip(tr("Room all round the tab (or peg) and the pin, so printed parts go together and turn"));
    fitForm->addRow(tr("Fit:"), m_fit);
    layout->addLayout(fitForm);

    layout->addSpacing(6);
    m_addPieces = new QPushButton(tr("Add the Tab and Pins Again"));
    m_addPieces->setToolTip(tr("Shapes added with the joint don't change with it: this adds them again for these "
                               "settings (delete the old ones)"));
    layout->addWidget(m_addPieces);
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

    for (QComboBox* box : {m_kind, m_other, m_pin, m_swing})
        connect(box, &QComboBox::currentIndexChanged, this, [this] {
            showKind();
            emit changed();
        });
    connect(m_pin, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QSignalBlocker block(m_pinSize);
        if (index == 1) {
            m_pinSize->setValue(kFilament);
        } else if (std::abs(m_pinSize->value() - kFilament) < 1e-9) {
            m_pinSize->setValue(m_lastPinSize);
        }
        emit changed();
    });
    // The fit found with the tolerance test print for this printer, if any (pins; a
    // swivel's peg is a peg).
    connect(m_printer, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto kind = m_kind->currentIndex() == 2 ? tolerance::Kind::Peg : tolerance::Kind::Pin;
        m_fit->setValue(fits::starting(index == 0, kind));  // emits changed
    });
    for (QDoubleSpinBox* box : {m_width, m_barrel, m_gap, m_lip, m_pinSize, m_turn, m_reach, m_fit})
        connect(box, &QDoubleSpinBox::valueChanged, this, &HingePanel::changed);
    connect(m_pinSize, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (m_pin->currentIndex() != 1)
            m_lastPinSize = v;
    });
    connect(m_stiff, &QCheckBox::toggled, this, &HingePanel::changed);
    connect(m_clicks, &QSpinBox::valueChanged, this, [this] {
        m_clickDepth->setEnabled(m_clicks->value() > 0);
        emit changed();
    });
    connect(m_clickDepth, &QDoubleSpinBox::valueChanged, this, &HingePanel::changed);
    connect(quarter, &QPushButton::clicked, this, [this] {
        double t = m_turn->value() + 90.0;
        if (t > 180.0)
            t -= 360.0;
        m_turn->setValue(t);
    });
    connect(m_pickOther, &QPushButton::clicked, this, &HingePanel::pickOtherRequested);
    connect(m_addPieces, &QPushButton::clicked, this, &HingePanel::addPiecesRequested);
    connect(m_apply, &QPushButton::clicked, this, &HingePanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &HingePanel::cancelRequested);
    setFinish(HingeFinish{});
}

void HingePanel::showKind()
{
    const auto kind = HingeKind(m_kind->currentIndex());
    const bool swivel = kind == HingeKind::Swivel;
    const bool dbl = kind == HingeKind::Double;
    m_kindHelp->setText(swivel ? tr("This part gets the bore; the peg goes on the other half.")
                        : dbl  ? tr("This part gets a fork; a link with a pin hole at each end joins it to a fork "
                                    "in the other part.")
                               : tr("This part gets the fork; the tab is the other half."));
    {
        const QSignalBlocker block(m_other);
        if (dbl) {
            m_other->setItemText(0, tr("Just the link (fork the other part later)"));
            m_other->setItemText(1, tr("Fork another part too"));
        } else if (swivel) {
            m_other->setItemText(0, tr("A new peg shape, in place"));
            m_other->setItemText(1, tr("Made from another part"));
        } else {
            m_other->setItemText(0, tr("A new tab shape, in place"));
            m_other->setItemText(1, tr("Made from another part"));
        }
    }
    m_other->setToolTip(dbl ? tr("The link is added as a shape of its own. Fork another part too: the other part "
                                 "gets its fork, lined up with the link's other end.")
                            : tr("A new shape: added in place, to Merge onto the other part or keep apart. Made "
                                 "from another part: that part's end is trimmed into the tab (or the peg goes "
                                 "on it), standing just clear of this part's face."));
    m_otherBox->setVisible(!m_editing);
    const bool fromPart = m_other->currentIndex() == 1;
    m_pickOther->setVisible(fromPart);
    m_otherName->setVisible(fromPart);
    m_widthLabel->setText(swivel ? tr("Peg diameter:") : tr("Tab thickness:"));
    m_width->setToolTip(swivel ? tr("The peg's diameter, as it's made (the bore is bigger by the fit)")
                               : tr("How thick the tab is, along the pin (the slot is wider by the fit each side)"));
    m_barrelLabel->setText(swivel ? tr("Peg goes in:") : tr("Tab end across:"));
    m_barrel->setToolTip(swivel ? tr("How far the peg goes into the bore")
                                : tr("The diameter of the tab's rounded end, round the pin"));
    m_gapLabel->setVisible(dbl);
    m_gap->setVisible(dbl);
    m_lipLabel->setVisible(swivel);
    m_lip->setVisible(swivel);
    m_pinBox->setVisible(!swivel);
    m_pinSize->setEnabled(m_pin->currentIndex() != 1);
    m_addPieces->setVisible(m_editing);
    m_addPieces->setText(swivel ? tr("Add the Peg Again") : dbl ? tr("Add the Link and Pins Again")
                                                               : tr("Add the Tab and Pins Again"));
}

void HingePanel::start(const QString& title, bool editing)
{
    m_editing = editing;
    m_title->setText(title);
    m_kind->setEnabled(!editing);
    m_apply->setToolTip(editing ? tr("Change the joint") : tr("Make the joint"));
    setPlaced(editing);
    setOtherPart({}, false);
    showKind();
}

void HingePanel::setFinish(const HingeFinish& f)
{
    {
        const QSignalBlocker b0(m_kind), b1(m_pin), b2(m_swing), b3(m_printer), b4(m_width), b5(m_barrel),
            b6(m_gap), b7(m_lip), b8(m_pinSize), b9(m_turn), b10(m_reach), b11(m_fit), b12(m_stiff);
        m_kind->setCurrentIndex(int(f.kind));
        m_width->setValue(f.width);
        m_barrel->setValue(f.barrel);
        m_gap->setValue(f.gap);
        m_lip->setValue(f.lip);
        m_pin->setCurrentIndex(int(f.pin));
        m_pinSize->setValue(f.pinSize);
        if (f.pin != PinKind::Filament)
            m_lastPinSize = f.pinSize;
        m_stiff->setChecked(f.stiff);
        const QSignalBlocker c1(m_clicks), c2(m_clickDepth);
        m_clicks->setValue(f.clicks);
        m_clickDepth->setValue(f.clickDepth);
        m_clickDepth->setEnabled(f.clicks > 0);
        m_swing->setCurrentIndex(int(f.swing));
        m_turn->setValue(f.turn);
        m_reach->setValue(f.reach);
        m_printer->setCurrentIndex(f.resin ? 0 : 1);
        m_fit->setValue(f.fit);
    }
    showKind();
}

HingeFinish HingePanel::finish() const
{
    HingeFinish f;
    f.kind = HingeKind(m_kind->currentIndex());
    f.width = m_width->value();
    f.barrel = m_barrel->value();
    f.gap = m_gap->value();
    f.lip = m_lip->value();
    f.pin = PinKind(m_pin->currentIndex());
    f.pinSize = f.pin == PinKind::Filament ? kFilament : m_pinSize->value();
    f.stiff = m_stiff->isChecked();
    f.clicks = m_clicks->value();
    f.clickDepth = m_clickDepth->value();
    f.swing = HingeSwing(m_swing->currentIndex());
    f.turn = m_turn->value();
    f.reach = m_reach->value();
    f.resin = m_printer->currentIndex() == 0;
    f.fit = m_fit->value();
    return f;
}

bool HingePanel::otherFromPart() const { return m_other->currentIndex() == 1; }

void HingePanel::setOtherFromPart(bool on)
{
    m_other->setCurrentIndex(on ? 1 : 0);
}

void HingePanel::setOtherPart(const QString& name, bool picking)
{
    m_pickOther->setText(picking ? tr("Picking...") : tr("Pick the Other Part"));
    m_otherName->setText(picking       ? tr("<i>Click the other part in the view.</i>")
                         : name.isEmpty() ? tr("<i>None picked yet.</i>")
                                          : tr("<b>%1</b>").arg(name.toHtmlEscaped()));
}

void HingePanel::setPlaced(bool placed)
{
    m_placed->setText(placed ? tr("Click elsewhere on the shape to move the joint.")
                             : tr("<i>Click the spot on the shape where the joint goes.</i>"));
}

void HingePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void HingePanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

void HingePanel::attachFormulas(FormulaFields* fields)
{
    fields->add(m_width, QStringLiteral("hinge.width"), tr("Tab width"));
    fields->add(m_barrel, QStringLiteral("hinge.barrel"), tr("Tab end"));
    fields->add(m_gap, QStringLiteral("hinge.gap"), tr("Gap"));
    fields->add(m_lip, QStringLiteral("hinge.lip"), tr("Lip"));
    fields->add(m_pinSize, QStringLiteral("hinge.pinSize"), tr("Pin diameter"));
    fields->add(m_fit, QStringLiteral("hinge.fit"), tr("Fit"));
}

}  // namespace forge::app
