// SPDX-License-Identifier: GPL-3.0-or-later
#include "MaskPanel.h"

#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSettings>
#include <QVBoxLayout>

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
    auto* help = new QLabel(tr("Paint over the part to cut off: drag on the shape. Hold <b>Ctrl</b> to rub paint "
                               "out. The shape is cut along the edge of the paint, and both pieces are closed with "
                               "the same smooth cap, so they fit back together. [ and ] change the brush size."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);
    layout->addSpacing(6);

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

    // Alignment keys, so painted pieces glue back only one way.
    m_keys = new QCheckBox(tr("Alignment keys on the cut"));
    m_keys->setToolTip(tr("A three-sided peg on the painted piece and a matching hole in the rest, on each cut, so "
                          "the pieces only go back together one way (for painting apart and gluing). Cuts too "
                          "small for one are left plain."));
    layout->addWidget(m_keys);
    auto* fitRow = new QFormLayout;
    m_keyFit = new QDoubleSpinBox;
    m_keyFit->setRange(0.0, 1.0);
    m_keyFit->setDecimals(2);
    m_keyFit->setSingleStep(0.05);
    m_keyFit->setSuffix(tr(" mm"));
    m_keyFit->setToolTip(tr("How much bigger the hole is than the peg, all round: 0.1 mm for resin, 0.2 for "
                            "filament"));
    fitRow->addRow(tr("Key fit:"), m_keyFit);
    layout->addLayout(fitRow);
    {
        const QSettings prefs(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"));
        m_keys->setChecked(prefs.value(QStringLiteral("mask/keys"), true).toBool());
        m_keyFit->setValue(prefs.value(QStringLiteral("mask/keyFit"), 0.1).toDouble());
    }
    m_keyFit->setEnabled(m_keys->isChecked());
    const auto rememberKeys = [this] {
        m_keyFit->setEnabled(m_keys->isChecked());
        if (m_keys->property("editing").toBool())
            return;
        QSettings prefs(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                        QStringLiteral("Forge3D"));
        prefs.setValue(QStringLiteral("mask/keys"), m_keys->isChecked());
        prefs.setValue(QStringLiteral("mask/keyFit"), m_keyFit->value());
    };
    connect(m_keys, &QCheckBox::toggled, this, rememberKeys);
    connect(m_keyFit, &QDoubleSpinBox::valueChanged, this, rememberKeys);

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
    auto* colours = new QGroupBox(tr("Colours while painting"));
    auto* colourGrid = new QGridLayout(colours);
    m_unmaskedButton = new QPushButton;
    m_unmaskedButton->setToolTip(tr("The colour of the shape where it isn't painted"));
    m_maskedButton = new QPushButton;
    m_maskedButton->setToolTip(tr("The colour of the paint (the mask)"));
    auto* swap = new QPushButton(tr("Swap"));
    swap->setToolTip(tr("Swap the two colours"));
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
            [this, pick] { pick(m_unmaskedColour, tr("Colour where it isn't painted")); });
    connect(m_maskedButton, &QPushButton::clicked, this, [this, pick] { pick(m_maskedColour, tr("Colour of the paint")); });
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

void MaskPanel::start(const QString& title, bool editing, double keyFit)
{
    m_title->setText(title);
    m_keys->setProperty("editing", editing);
    if (editing) {
        const QSignalBlocker a(m_keys), b(m_keyFit);
        m_keys->setChecked(keyFit >= 0.0);
        if (keyFit >= 0.0)
            m_keyFit->setValue(keyFit);
        m_keyFit->setEnabled(m_keys->isChecked());
    }
    m_keepBox->setVisible(!editing);  // a step being changed keeps the pieces it has
    m_apply->setToolTip(editing ? tr("Cut along the changed mask") : tr("Cut the shape along the mask"));
    setStatus({}, false);
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
    return m_keys->isChecked() ? m_keyFit->value() : -1.0;
}

}  // namespace forge::app
