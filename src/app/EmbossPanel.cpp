// SPDX-License-Identifier: GPL-3.0-or-later
#include "EmbossPanel.h"

#include "WordsEditor.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

namespace {
QDoubleSpinBox* spin(double min, double max, double step, const QString& suffix, int decimals, const QString& tip)
{
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    s->setSuffix(suffix);
    s->setKeyboardTracking(false);
    s->setAccelerated(true);
    s->setToolTip(tip);
    s->setMinimumWidth(110);
    return s;
}
}  // namespace

EmbossPanel::EmbossPanel(QWidget* parent) : QWidget(parent)
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

    m_words = new WordsEditor;
    layout->addWidget(m_words);

    auto* sizeGroup = new QGroupBox(tr("Size"));
    auto* form = new QFormLayout(sizeGroup);
    m_letterHeight = spin(0.5, 1000.0, 1.0, tr(" mm"), 2, tr("How tall a capital letter is"));
    m_depth = spin(0.05, 100.0, 0.25, tr(" mm"), 2,
                   tr("How far the letters stand out from the surface (raised) or go into it (engraved)"));
    m_angle = spin(-180.0, 180.0, 15.0, tr(" °"), 1, tr("Turn the words on the surface (anticlockwise)"));
    m_angle->setWrapping(true);
    form->addRow(tr("Letter height:"), m_letterHeight);
    form->addRow(tr("Depth:"), m_depth);
    form->addRow(tr("Turn:"), m_angle);
    layout->addWidget(sizeGroup);

    auto* kindGroup = new QGroupBox(tr("Letters"));
    auto* kindLayout = new QVBoxLayout(kindGroup);
    m_raised = new QRadioButton(tr("Raised (standing out)"));
    m_engraved = new QRadioButton(tr("Engraved (cut in)"));
    m_raised->setChecked(true);
    kindLayout->addWidget(m_raised);
    kindLayout->addWidget(m_engraved);
    layout->addWidget(kindGroup);

    auto* centerGroup = new QGroupBox(tr("Center on the face"));
    auto* centerLayout = new QHBoxLayout(centerGroup);
    const struct {
        QString label, tip;
    } centers[] = {
        {tr("All"), tr("Move the words to the middle of the face they're on")},
        {tr("Top-Bottom"), tr("Move the words up or down to the middle of the face, as the words read")},
        {tr("Left-Right"), tr("Move the words across to the middle of the face, as the words read")},
    };
    for (int i = 0; i < 3; ++i) {
        auto* button = new QPushButton(centers[i].label);
        button->setToolTip(centers[i].tip);
        connect(button, &QPushButton::clicked, this, [this, i] { emit centerRequested(i); });
        centerLayout->addWidget(button);
    }
    layout->addWidget(centerGroup);

    auto* hint = new QLabel(tr("Click anywhere on the shape to move the words there. They follow the "
                               "surface, so they bend round curves."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(hint);

    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Emboss"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setToolTip(tr("Close without changing anything (Esc)"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_words, &WordsEditor::changed, this, &EmbossPanel::changed);
    connect(m_words, &WordsEditor::textEdited, this, &EmbossPanel::typing);
    for (QDoubleSpinBox* s : {m_letterHeight, m_depth, m_angle})
        connect(s, &QDoubleSpinBox::valueChanged, this, &EmbossPanel::changed);
    connect(m_raised, &QRadioButton::toggled, this, &EmbossPanel::changed);
    connect(m_apply, &QPushButton::clicked, this, &EmbossPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &EmbossPanel::cancelRequested);
}

void EmbossPanel::start(const QString& title, bool editing, const EmbossText& words)
{
    m_title->setText(title);
    m_apply->setText(editing ? tr("Apply") : tr("Emboss"));
    m_apply->setToolTip(editing ? tr("Change the words on the shape") : tr("Put the words on the shape"));
    m_words->setWords(words.text, words.font, words.bold, words.italic, words.vertical);
    const QSignalBlocker b0(m_letterHeight), b1(m_depth), b2(m_angle), b3(m_raised), b4(m_engraved);
    m_letterHeight->setValue(words.letterHeight);
    m_depth->setValue(words.depth);
    m_angle->setValue(words.angle);
    m_raised->setChecked(words.raised);
    m_engraved->setChecked(!words.raised);
}

EmbossText EmbossPanel::words() const
{
    EmbossText w;
    w.text = m_words->text();
    w.font = m_words->font();
    w.bold = m_words->bold();
    w.italic = m_words->italic();
    w.vertical = m_words->vertical();
    w.letterHeight = m_letterHeight->value();
    w.depth = m_depth->value();
    w.angle = m_angle->value();
    w.raised = m_raised->isChecked();
    return w;
}

void EmbossPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

void EmbossPanel::setApplyEnabled(bool on)
{
    m_apply->setEnabled(on);
}

}  // namespace forge::app
