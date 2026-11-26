// SPDX-License-Identifier: GPL-3.0-or-later
#include "MaskPanel.h"

#include "JointAtCutBox.h"

#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace forge::app {

namespace {

// The app's preferences (see MainWindow.cpp).
class Preferences : public QSettings {
public:
    Preferences() : QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"))
    {
    }
};

const QString kUnmaskedDefault = QStringLiteral("#ffd21a");  // yellow
const QString kMaskedDefault = QStringLiteral("#141418");    // black

}  // namespace

MaskPanel::MaskPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    m_help = new QLabel;
    m_help->setWordWrap(true);
    m_help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_help);
    layout->addSpacing(6);

    // A painted colour as the mask (Split for painting along the colours).
    m_fromPaintBox = new QWidget;
    auto* fromColumn = new QVBoxLayout(m_fromPaintBox);
    fromColumn->setContentsMargins(0, 0, 0, 0);
    fromColumn->setSpacing(0);
    fromColumn->addWidget(new QLabel(tr("Mask from a painted color:")));
    m_fromPaint = new QComboBox;
    m_fromPaint->setToolTip(tr("Masks wherever this color is painted (instead of the mask so far), to cut that "
                               "color off as its own piece"));
    fromColumn->addWidget(m_fromPaint);
    layout->addWidget(m_fromPaintBox);
    m_fromPaintBox->hide();
    connect(m_fromPaint, &QComboBox::activated, this, [this](int index) {
        if (index <= 0)
            return;
        emit paintColourChosen(m_fromPaint->itemData(index).toUInt());
    });

    auto* form = new QFormLayout;
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.1, 200.0);
    m_size->setDecimals(1);
    m_size->setSingleStep(0.5);
    m_size->setSuffix(tr(" mm"));
    m_size->setValue(3.0);
    m_size->setToolTip(tr("How far the brush reaches from its middle (the ring shows it on the shape)"));
    form->addRow(tr("Brush size:"), m_size);
    layout->addLayout(form);
    m_lasso = new QPushButton(tr("Lasso: draw a loop instead"));
    m_lasso->setCheckable(true);
    m_lasso->setToolTip(tr("While on, drag a loop round the part instead of brushing: everything inside it is "
                           "painted, right through the shape (front and back), so a loop round a hand paints all "
                           "of the hand. Ctrl: rubs out inside it."));
    layout->addWidget(m_lasso);

    m_keepBox = new QWidget;
    auto* keepColumn = new QVBoxLayout(m_keepBox);
    keepColumn->setContentsMargins(0, 0, 0, 0);
    keepColumn->setSpacing(0);
    keepColumn->addWidget(new QLabel(tr("Keep:")));
    m_keep = new QComboBox;
    m_keep->addItem(tr("Both pieces"));
    m_keep->addItem(tr("Just the painted piece"));
    m_keep->addItem(tr("Just the rest"));
    m_keep->setToolTip(tr("Both: two shapes, the painted piece and the rest"));
    keepColumn->addWidget(m_keep);
    layout->addWidget(m_keepBox);

    // What only a split needs (hidden for Make Part).
    m_splitOptions = new QWidget;
    auto* splitLayout = new QVBoxLayout(m_splitOptions);
    splitLayout->setContentsMargins(0, 0, 0, 0);
    // The cut's surface: along the mask, flat, or flat with a ball and socket.
    auto* cutColumn = new QVBoxLayout;
    cutColumn->setSpacing(0);
    cutColumn->addWidget(new QLabel(tr("Cut surface:")));
    m_cut = new QComboBox;
    m_cut->addItem(tr("Curved (follows the mask)"));
    m_cut->addItem(tr("Flat"));
    m_cut->addItem(tr("Ball and socket"));
    m_cut->setToolTip(tr("Curved: the cut follows the mask's edge, a gently curved surface\n"
                         "Flat: a flat cut through the middle of the mask's edge\n"
                         "Ball and socket: a flat cut with a dome on the painted piece and a matching "
                         "socket in the rest, so the pieces sit together one way and turn on it"));
    cutColumn->addWidget(m_cut);
    splitLayout->addLayout(cutColumn);
    auto* socketRow = new QFormLayout;
    m_socketFit = new QDoubleSpinBox;
    m_socketFit->setRange(0.0, 1.0);
    m_socketFit->setDecimals(2);
    m_socketFit->setSingleStep(0.05);
    m_socketFit->setSuffix(tr(" mm"));
    m_socketFit->setToolTip(tr("How much bigger the socket is than the dome, all round: 0.15 mm for resin, "
                               "0.25 for filament"));
    socketRow->addRow(tr("Socket fit:"), m_socketFit);
    splitLayout->addLayout(socketRow);

    // A joint where the pieces meet (a new split only).
    m_joint = new JointAtCutBox(QStringLiteral("mask"));
    splitLayout->addWidget(m_joint);
    connect(m_joint, &JointAtCutBox::changed, this, [this] { showCut(); });

    // Alignment keys, so painted pieces glue back only one way.
    m_keys = new QCheckBox(tr("Alignment keys on the cut"));
    m_keys->setToolTip(tr("A three-sided peg on the painted piece and a matching hole in the rest, on each cut, so "
                          "the pieces only go back together one way (for painting apart and gluing). Cuts too "
                          "small for one are left plain."));
    splitLayout->addWidget(m_keys);
    auto* fitRow = new QFormLayout;
    m_keyFit = new QDoubleSpinBox;
    m_keyFit->setRange(0.0, 1.0);
    m_keyFit->setDecimals(2);
    m_keyFit->setSingleStep(0.05);
    m_keyFit->setSuffix(tr(" mm"));
    m_keyFit->setToolTip(tr("How much bigger the hole is than the peg, all round: 0.1 mm for resin, 0.2 for "
                            "filament"));
    fitRow->addRow(tr("Key fit:"), m_keyFit);
    splitLayout->addLayout(fitRow);
    layout->addWidget(m_splitOptions);

    // Make Part: how thick, and whether it sits on the body or is set into it.
    m_partOptions = new QWidget;
    auto* partLayout = new QVBoxLayout(m_partOptions);
    partLayout->setContentsMargins(0, 0, 0, 0);
    auto* placeColumn = new QVBoxLayout;
    placeColumn->setSpacing(0);
    placeColumn->addWidget(new QLabel(tr("The new part:")));
    m_partPlace = new QComboBox;
    m_partPlace->addItem(tr("Sits on top of the body"));
    m_partPlace->addItem(tr("Set into the body (flush)"));
    m_partPlace->setToolTip(tr("Sits on top: the part lies over the body with a small gap, like real armor (the "
                               "figure is a little bulkier there).\nSet into the body: the body is cut back under "
                               "the part, so the outline stays the same and the part sits flush (a removable plate)."));
    placeColumn->addWidget(m_partPlace);
    partLayout->addLayout(placeColumn);
    auto* partForm = new QFormLayout;
    m_partThickness = new QDoubleSpinBox;
    m_partThickness->setRange(0.3, 20.0);
    m_partThickness->setDecimals(2);
    m_partThickness->setSingleStep(0.1);
    m_partThickness->setSuffix(tr(" mm"));
    m_partThickness->setToolTip(tr("How thick the part is: about 1 mm for clothes, 1.5 to 2 for armor"));
    partForm->addRow(tr("Thickness:"), m_partThickness);
    m_partFit = new QDoubleSpinBox;
    m_partFit->setRange(0.0, 1.0);
    m_partFit->setDecimals(2);
    m_partFit->setSingleStep(0.05);
    m_partFit->setSuffix(tr(" mm"));
    m_partFit->setToolTip(tr("The gap between the part and the body, so it goes on: 0.1 mm for resin, 0.2 for "
                             "filament (0 to make it touch, to glue)"));
    partForm->addRow(tr("Fit:"), m_partFit);
    partLayout->addLayout(partForm);
    layout->addWidget(m_partOptions);
    m_partOptions->hide();

    {
        const QSettings prefs(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"));
        m_keys->setChecked(prefs.value(QStringLiteral("mask/keys"), true).toBool());
        m_keyFit->setValue(prefs.value(QStringLiteral("mask/keyFit"), 0.1).toDouble());
        m_cut->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("mask/cut"), 0).toInt(), 0, 2));
        m_socketFit->setValue(prefs.value(QStringLiteral("mask/socketFit"), 0.15).toDouble());
    }
    showCut();
    const auto rememberKeys = [this] {
        showCut();
        if (m_keys->property("editing").toBool())
            return;
        QSettings prefs(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                        QStringLiteral("Forge3D"));
        prefs.setValue(QStringLiteral("mask/keys"), m_keys->isChecked());
        prefs.setValue(QStringLiteral("mask/keyFit"), m_keyFit->value());
        prefs.setValue(QStringLiteral("mask/cut"), m_cut->currentIndex());
        prefs.setValue(QStringLiteral("mask/socketFit"), m_socketFit->value());
    };
    connect(m_keys, &QCheckBox::toggled, this, rememberKeys);
    connect(m_keyFit, &QDoubleSpinBox::valueChanged, this, rememberKeys);
    connect(m_cut, &QComboBox::currentIndexChanged, this, rememberKeys);
    connect(m_socketFit, &QDoubleSpinBox::valueChanged, this, rememberKeys);

    auto* edit = new QHBoxLayout;
    m_undo = new QPushButton(tr("Undo Stroke"));
    m_undo->setToolTip(tr("Take back the last stroke (Ctrl+Z)"));
    m_clear = new QPushButton(tr("Clear"));
    m_clear->setToolTip(tr("Rub out all the paint"));
    edit->addWidget(m_undo);
    edit->addWidget(m_clear);
    layout->addLayout(edit);
    m_strokes = new QLabel;
    layout->addWidget(m_strokes);

    // Strong, plain colours, so it's plain what's painted (shadows don't look like paint).
    auto* colours = new QGroupBox(tr("Colors while painting"));
    auto* colourGrid = new QGridLayout(colours);
    m_unmaskedButton = new QPushButton;
    m_unmaskedButton->setToolTip(tr("The color of the shape where it isn't painted"));
    m_maskedButton = new QPushButton;
    m_maskedButton->setToolTip(tr("The color of the paint (the mask)"));
    auto* swap = new QPushButton(tr("Swap"));
    swap->setToolTip(tr("Swap the two colors"));
    auto* reset = new QPushButton(tr("Reset"));
    reset->setToolTip(tr("Back to yellow and black"));
    colourGrid->addWidget(new QLabel(tr("Not painted:")), 0, 0);
    colourGrid->addWidget(m_unmaskedButton, 0, 1);
    colourGrid->addWidget(new QLabel(tr("Painted:")), 1, 0);
    colourGrid->addWidget(m_maskedButton, 1, 1);
    auto* colourButtons = new QHBoxLayout;
    colourButtons->addWidget(swap);
    colourButtons->addWidget(reset);
    colourGrid->addLayout(colourButtons, 2, 0, 1, 2);
    layout->addWidget(colours);
    {
        const Preferences prefs;
        m_unmaskedColour = QColor(prefs.value(QStringLiteral("mask/unmaskedColour"), kUnmaskedDefault).toString());
        m_maskedColour = QColor(prefs.value(QStringLiteral("mask/maskedColour"), kMaskedDefault).toString());
        if (!m_unmaskedColour.isValid())
            m_unmaskedColour = QColor(kUnmaskedDefault);
        if (!m_maskedColour.isValid())
            m_maskedColour = QColor(kMaskedDefault);
    }
    showColours();
    const auto pick = [this](QColor& colour, const QString& title) {
        const QColor chosen = QColorDialog::getColor(colour, this, title);
        if (!chosen.isValid())
            return;
        colour = chosen;
        saveColours();
    };
    connect(m_unmaskedButton, &QPushButton::clicked, this,
            [this, pick] { pick(m_unmaskedColour, tr("Color where it isn't painted")); });
    connect(m_maskedButton, &QPushButton::clicked, this, [this, pick] { pick(m_maskedColour, tr("Color of the paint")); });
    connect(swap, &QPushButton::clicked, this, [this] {
        std::swap(m_unmaskedColour, m_maskedColour);
        saveColours();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        m_unmaskedColour = QColor(kUnmaskedDefault);
        m_maskedColour = QColor(kMaskedDefault);
        saveColours();
    });

    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto* buttons = new QHBoxLayout;
    {
        const QSettings prefs(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"));
        m_partPlace->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("part/place"), 0).toInt(), 0, 1));
        m_partThickness->setValue(prefs.value(QStringLiteral("part/thickness"), 1.5).toDouble());
        m_partFit->setValue(prefs.value(QStringLiteral("part/fit"), 0.1).toDouble());
    }
    const auto rememberPart = [this] {
        QSettings prefs(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                        QStringLiteral("Forge3D"));
        prefs.setValue(QStringLiteral("part/place"), m_partPlace->currentIndex());
        prefs.setValue(QStringLiteral("part/thickness"), m_partThickness->value());
        prefs.setValue(QStringLiteral("part/fit"), m_partFit->value());
    };
    connect(m_partPlace, &QComboBox::currentIndexChanged, this, rememberPart);
    connect(m_partThickness, &QDoubleSpinBox::valueChanged, this, rememberPart);
    connect(m_partFit, &QDoubleSpinBox::valueChanged, this, rememberPart);

    m_apply = new QPushButton(tr("Apply"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_size, &QDoubleSpinBox::valueChanged, this, &MaskPanel::brushSizeChanged);
    connect(m_undo, &QPushButton::clicked, this, &MaskPanel::undoStrokeRequested);
    connect(m_clear, &QPushButton::clicked, this, &MaskPanel::clearRequested);
    connect(m_apply, &QPushButton::clicked, this, &MaskPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &MaskPanel::cancelRequested);
}

void MaskPanel::showColours()
{
    const auto paint = [](QPushButton* button, const QColor& c) {
        button->setText(c.name().toUpper());
        const QColor text = c.lightnessF() > 0.55 ? Qt::black : Qt::white;
        button->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; border: 1px solid "
                                             "palette(mid); padding: 4px; }")
                                  .arg(c.name(), text.name()));
    };
    paint(m_unmaskedButton, m_unmaskedColour);
    paint(m_maskedButton, m_maskedColour);
}

void MaskPanel::saveColours()
{
    Preferences prefs;
    prefs.setValue(QStringLiteral("mask/unmaskedColour"), m_unmaskedColour.name());
    prefs.setValue(QStringLiteral("mask/maskedColour"), m_maskedColour.name());
    showColours();
    emit coloursChanged();
}

void MaskPanel::start(const QString& title, bool editing, double keyFit, MaskCut cut, double socketFit)
{
    m_title->setText(title);
    m_makingPart = false;
    m_lasso->setChecked(false);
    m_splitOptions->show();
    m_partOptions->hide();
    m_apply->setText(tr("Apply"));
    m_help->setText(tr("Paint over the part to cut off: drag on the shape. Hold <b>Ctrl</b> to rub paint out. The "
                       "shape is cut along the edge of the paint, and both pieces are closed with the same smooth "
                       "cap, so they fit back together. [ and ] change the brush size."));
    m_keys->setProperty("editing", editing);
    if (editing) {
        const QSignalBlocker a(m_keys), b(m_keyFit), c(m_cut), d(m_socketFit);
        m_keys->setChecked(keyFit >= 0.0);
        if (keyFit >= 0.0)
            m_keyFit->setValue(keyFit);
        m_cut->setCurrentIndex(cut == MaskCut::Flat ? 1 : cut == MaskCut::Dome ? 2 : 0);
        m_socketFit->setValue(socketFit);
    }
    showCut();
    m_keepBox->setVisible(!editing);  // a step being changed keeps the pieces it has
    m_joint->setVisible(!editing);   // a joint is put in once, when splitting
    showCut();
    m_apply->setToolTip(editing ? tr("Cut along the changed mask") : tr("Cut the shape along the mask"));
    setStatus({}, false);
}

void MaskPanel::startPart(const QString& title)
{
    m_title->setText(title);
    m_makingPart = true;
    m_splitOptions->hide();
    m_keepBox->hide();
    m_partOptions->show();
    m_apply->setText(tr("Make Part"));
    m_apply->setToolTip(tr("Make a new part over the painted area"));
    m_help->setText(tr("Paint where the new part goes (armor, a glove, a belt...): drag on the shape. Hold "
                       "<b>Ctrl</b> to rub paint out. The part follows the body's surface and the edge of the paint. "
                       "[ and ] change the brush size."));
    setStatus({}, false);
}

void MaskPanel::startHead(const QString& title)
{
    m_title->setText(title);
    m_makingPart = false;
    m_splitOptions->hide();
    m_keepBox->hide();
    m_partOptions->hide();
    m_apply->setText(tr("Cut Off Head"));
    m_apply->setToolTip(tr("Cut the head off along the paint's edge, with the spring's socket and post"));
    m_help->setText(tr("Paint over the whole head, down to where it should come off the neck: drag on the shape, or "
                       "click Lasso and draw round the head from the front. Hold <b>Ctrl</b> to rub paint out. The "
                       "cut is flat, through the paint's edge, so keep the edge going once round the neck. [ and ] "
                       "change the brush size."));
    setStatus({}, false);
}

void MaskPanel::startBrickHead(const QString& title)
{
    startHead(title);
    m_apply->setText(tr("Use This Mask"));
    m_apply->setToolTip(tr("Back to Brick Figure, the head being what's painted"));
}

MaskPanel::PartOptions MaskPanel::partOptions() const
{
    PartOptions o;
    o.setIn = m_partPlace->currentIndex() == 1;
    o.thickness = m_partThickness->value();
    o.fit = m_partFit->value();
    return o;
}

bool MaskPanel::lasso() const
{
    return m_lasso->isChecked();
}

double MaskPanel::brushSize() const
{
    return m_size->value();
}

void MaskPanel::setBrushSize(double radius)
{
    m_size->setValue(radius);
}

SplitKeep MaskPanel::keep() const
{
    switch (m_keep->currentIndex()) {
    case 1: return SplitKeep::Front;
    case 2: return SplitKeep::Back;
    default: return SplitKeep::Both;
    }
}

void MaskPanel::setStrokes(int strokes)
{
    m_strokes->setText(strokes == 0 ? tr("<i>Nothing painted yet.</i>")
                                    : tr("%n stroke(s) painted.", nullptr, strokes));
    m_undo->setEnabled(strokes > 0);
    m_clear->setEnabled(strokes > 0);
}

void MaskPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void MaskPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

double MaskPanel::keyFit() const
{
    return m_keys->isChecked() && cut() != MaskCut::Dome && joint().kind == splitjoint::Kind::None ? m_keyFit->value()
                                                                                                    : -1.0;
}

splitjoint::Options MaskPanel::joint() const
{
    splitjoint::Options o = m_joint->options();
    if (m_joint->isHidden() || cut() == MaskCut::Dome || keep() != SplitKeep::Both)
        o.kind = splitjoint::Kind::None;
    return o;
}

MaskCut MaskPanel::cut() const
{
    switch (m_cut->currentIndex()) {
    case 1: return MaskCut::Flat;
    case 2: return MaskCut::Dome;
    default: return MaskCut::Curved;
    }
}

double MaskPanel::socketFit() const
{
    return m_socketFit->value();
}

void MaskPanel::showCut()
{
    // The ball and socket lines the pieces up itself: no keys with it.
    const bool dome = cut() == MaskCut::Dome;
    const bool joint = m_joint && !m_joint->isHidden() && m_joint->wanted() && !dome;
    m_socketFit->setEnabled(dome);
    if (m_joint)
        m_joint->setEnabled(!dome);
    m_keys->setEnabled(!dome && !joint);
    m_keyFit->setEnabled(!dome && !joint && m_keys->isChecked());
}

}  // namespace forge::app

namespace forge::app {

void MaskPanel::setPaintColours(const std::vector<std::uint32_t>& colours)
{
    m_fromPaint->clear();
    m_fromPaint->addItem(tr("Choose a color..."));
    for (std::uint32_t c : colours) {
        QPixmap pix(16, 16);
        pix.fill(QColor::fromRgb(QRgb(c)));
        m_fromPaint->addItem(QIcon(pix), QColor::fromRgb(QRgb(c)).name().toUpper(), QVariant::fromValue(uint(c)));
    }
    m_fromPaintBox->setVisible(!colours.empty());
}

}  // namespace forge::app
