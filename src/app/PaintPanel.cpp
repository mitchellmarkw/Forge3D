// SPDX-License-Identifier: GPL-3.0-or-later
#include "PaintPanel.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

namespace forge::app {

namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

// Ready colours for figures and toys.
struct Named {
    std::uint32_t rgb;
    const char* name;
};
constexpr Named kPalette[] = {
    {0xF1C27D, QT_TRANSLATE_NOOP("PaintPanel", "Light skin")},
    {0xC68642, QT_TRANSLATE_NOOP("PaintPanel", "Medium skin")},
    {0x8D5524, QT_TRANSLATE_NOOP("PaintPanel", "Dark skin")},
    {0x6B4226, QT_TRANSLATE_NOOP("PaintPanel", "Brown")},
    {0x1A1A1A, QT_TRANSLATE_NOOP("PaintPanel", "Black")},
    {0xF5F5F5, QT_TRANSLATE_NOOP("PaintPanel", "White")},
    {0x808080, QT_TRANSLATE_NOOP("PaintPanel", "Grey")},
    {0xBFC3C7, QT_TRANSLATE_NOOP("PaintPanel", "Silver")},
    {0xC0282D, QT_TRANSLATE_NOOP("PaintPanel", "Red")},
    {0xE87722, QT_TRANSLATE_NOOP("PaintPanel", "Orange")},
    {0xF2C230, QT_TRANSLATE_NOOP("PaintPanel", "Yellow")},
    {0xC9A13B, QT_TRANSLATE_NOOP("PaintPanel", "Gold")},
    {0x2E8B3E, QT_TRANSLATE_NOOP("PaintPanel", "Green")},
    {0x2456B5, QT_TRANSLATE_NOOP("PaintPanel", "Blue")},
    {0x1B2A4A, QT_TRANSLATE_NOOP("PaintPanel", "Navy")},
    {0x6A3D9A, QT_TRANSLATE_NOOP("PaintPanel", "Purple")},
};

QIcon swatchIcon(const QColor& c, int size)
{
    QPixmap pix(size, size);
    pix.fill(c);
    return QIcon(pix);
}

}  // namespace

PaintPanel::PaintPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    m_title->setWordWrap(true);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    layout->addWidget(m_title);
    auto* help = new QLabel(tr("Drag over the shape to paint it. Hold <b>Ctrl</b> to rub paint out (back to the "
                               "shape's own color). [ and ] change the brush size. Export as 3MF for a color "
                               "printer. Pose a figure before painting it."));
    help->setWordWrap(true);
    help->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(help);

    // The colour painting with.
    auto* currentRow = new QHBoxLayout;
    currentRow->addWidget(new QLabel(tr("Painting with:")));
    m_current = new QPushButton;
    m_current->setMinimumSize(48, 28);
    m_current->setToolTip(tr("Choose any color"));
    currentRow->addWidget(m_current);
    m_currentName = new QLabel;
    currentRow->addWidget(m_currentName, 1);
    layout->addLayout(currentRow);

    auto* palette = new QGroupBox(tr("Colors"));
    auto* paletteLayout = new QVBoxLayout(palette);
    auto* grid = new QGridLayout;
    grid->setSpacing(3);
    int i = 0;
    for (const Named& n : kPalette) {
        grid->addWidget(swatch(QColor::fromRgb(QRgb(n.rgb)), tr(n.name)), i / 8, i % 8);
        ++i;
    }
    paletteLayout->addLayout(grid);
    auto* other = new QPushButton(tr("Other Color..."));
    other->setToolTip(tr("Choose any color"));
    paletteLayout->addWidget(other);
    m_usedBox = new QWidget;
    auto* usedColumn = new QVBoxLayout(m_usedBox);
    usedColumn->setContentsMargins(0, 4, 0, 0);
    usedColumn->addWidget(new QLabel(tr("On this shape:")));
    m_usedGrid = new QGridLayout;
    m_usedGrid->setSpacing(3);
    usedColumn->addLayout(m_usedGrid);
    paletteLayout->addWidget(m_usedBox);
    m_usedBox->hide();
    layout->addWidget(palette);

    auto* form = new QFormLayout;
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.1, 200.0);
    m_size->setDecimals(1);
    m_size->setSingleStep(0.5);
    m_size->setSuffix(tr(" mm"));
    m_size->setToolTip(tr("How far the brush reaches from its middle (the ring shows it on the shape)"));
    form->addRow(tr("Brush size:"), m_size);
    layout->addLayout(form);
    m_mirror = new QCheckBox(tr("Mirror left-right"));
    m_mirror->setToolTip(tr("Paint the other side too, mirrored across the shape's middle (for a figure facing the "
                            "front: both eyes, both boots)"));
    layout->addWidget(m_mirror);

    auto* edit = new QHBoxLayout;
    auto* fill = new QPushButton(tr("Fill All"));
    fill->setToolTip(tr("Paint the whole shape this color (then paint the details over it)"));
    auto* undo = new QPushButton(tr("Undo Stroke"));
    undo->setToolTip(tr("Take back the last stroke (Ctrl+Z)"));
    auto* clear = new QPushButton(tr("Clear"));
    clear->setToolTip(tr("Take off all the paint added here"));
    edit->addWidget(fill);
    edit->addWidget(undo);
    edit->addWidget(clear);
    layout->addLayout(edit);
    m_strokes = new QLabel;
    layout->addWidget(m_strokes);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton(tr("Cancel"));
    auto* apply = new QPushButton(tr("Apply"));
    apply->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(apply);
    layout->addLayout(buttons);
    layout->addStretch(1);

    const QSettings prefs = preferences();
    m_colour = QColor(prefs.value(QStringLiteral("paint/colour"), QStringLiteral("#c0282d")).toString());
    if (!m_colour.isValid())
        m_colour = QColor(0xC0, 0x28, 0x2D);
    m_size->setValue(prefs.value(QStringLiteral("paint/size"), 2.0).toDouble());
    m_mirror->setChecked(prefs.value(QStringLiteral("paint/mirror"), false).toBool());
    showColour();

    const auto choose = [this] {
        const QColor c = QColorDialog::getColor(m_colour, this, tr("Paint color"));
        if (c.isValid())
            setColour(c);
    };
    connect(m_current, &QPushButton::clicked, this, choose);
    connect(other, &QPushButton::clicked, this, choose);
    connect(m_size, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        preferences().setValue(QStringLiteral("paint/size"), v);
        emit brushSizeChanged(v);
    });
    connect(m_mirror, &QCheckBox::toggled, this,
            [](bool on) { preferences().setValue(QStringLiteral("paint/mirror"), on); });
    connect(fill, &QPushButton::clicked, this, &PaintPanel::fillRequested);
    connect(undo, &QPushButton::clicked, this, &PaintPanel::undoStrokeRequested);
    connect(clear, &QPushButton::clicked, this, &PaintPanel::clearRequested);
    connect(apply, &QPushButton::clicked, this, &PaintPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &PaintPanel::cancelRequested);
}

QToolButton* PaintPanel::swatch(const QColor& c, const QString& name)
{
    auto* b = new QToolButton;
    b->setIcon(swatchIcon(c, 18));
    b->setIconSize(QSize(18, 18));
    b->setAutoRaise(true);
    b->setToolTip(name.isEmpty() ? c.name().toUpper() : name);
    connect(b, &QToolButton::clicked, this, [this, c] { setColour(c); });
    return b;
}

void PaintPanel::start(const QString& title, bool editing)
{
    Q_UNUSED(editing);
    m_title->setText(title);
    setStatus({}, false);
}

void PaintPanel::setColour(const QColor& c)
{
    m_colour = c;
    preferences().setValue(QStringLiteral("paint/colour"), c.name());
    showColour();
}

void PaintPanel::showColour()
{
    m_current->setIcon(swatchIcon(m_colour, 20));
    m_current->setIconSize(QSize(20, 20));
    QString name = m_colour.name().toUpper();
    for (const Named& n : kPalette)
        if (QColor::fromRgb(QRgb(n.rgb)).rgb() == m_colour.rgb())
            name = tr(n.name);
    m_currentName->setText(name);
}

double PaintPanel::brushSize() const
{
    return m_size->value();
}

void PaintPanel::setBrushSize(double radius)
{
    m_size->setValue(radius);
}

bool PaintPanel::mirror() const
{
    return m_mirror->isChecked();
}

void PaintPanel::setUsed(const std::vector<std::uint32_t>& colours)
{
    while (QLayoutItem* item = m_usedGrid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    int i = 0;
    for (std::uint32_t c : colours) {
        m_usedGrid->addWidget(swatch(QColor::fromRgb(QRgb(c)), QString()), i / 8, i % 8);
        ++i;
    }
    m_usedBox->setVisible(!colours.empty());
}

void PaintPanel::setStrokes(int strokes)
{
    m_strokes->setText(strokes == 0   ? tr("Nothing painted yet.")
                       : strokes == 1 ? tr("1 stroke.")
                                      : tr("%1 strokes.").arg(strokes));
}

void PaintPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
