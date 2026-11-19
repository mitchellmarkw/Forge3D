// SPDX-License-Identifier: GPL-3.0-or-later
#include "MaskPanel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

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

void MaskPanel::start(const QString& title, bool editing)
{
    m_title->setText(title);
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

}  // namespace forge::app
