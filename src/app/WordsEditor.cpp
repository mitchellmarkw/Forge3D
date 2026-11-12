// SPDX-License-Identifier: GPL-3.0-or-later
#include "WordsEditor.h"

#include "forge/core/Text.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace forge::app {

WordsEditor::WordsEditor(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* textLabel = new QLabel(tr("Words:"));
    m_text = new QLineEdit;
    m_text->setPlaceholderText(tr("Type the words here"));
    m_text->setToolTip(tr("The letters to make. Press Enter (or click elsewhere) to apply"));
    textLabel->setBuddy(m_text);
    layout->addWidget(textLabel);
    layout->addWidget(m_text);
    layout->addSpacing(6);

    // The label sits right on top of its list.
    auto* fontLabel = new QLabel(tr("Font:"));
    m_font = new QComboBox;
    m_font->setToolTip(tr("The fonts installed on this PC. Type the first letters to jump to one"));
    m_font->setMaxVisibleItems(20);
    m_font->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_font->setMinimumContentsLength(12);
    fontLabel->setBuddy(m_font);
    layout->addWidget(fontLabel);
    layout->addWidget(m_font);
    layout->addSpacing(4);

    auto* style = new QHBoxLayout;
    m_bold = new QCheckBox(tr("Bold"));
    m_italic = new QCheckBox(tr("Italic"));
    m_bold->setToolTip(tr("Thicker letters (stronger, and easier to print small)"));
    m_italic->setToolTip(tr("Slanted letters"));
    style->addWidget(m_bold);
    style->addWidget(m_italic);
    style->addStretch(1);
    layout->addLayout(style);
    m_vertical = new QCheckBox(tr("Stack the letters (top to bottom)"));
    m_vertical->setToolTip(tr("Each letter under the one before, standing upright, like a sign on a post"));
    layout->addWidget(m_vertical);

    connect(m_text, &QLineEdit::textEdited, this, &WordsEditor::textEdited);
    connect(m_text, &QLineEdit::editingFinished, this, [this] {
        if (text() != m_lastText) {
            m_lastText = text();
            emit changed();
        }
    });
    connect(m_font, &QComboBox::currentIndexChanged, this, [this] { emit changed(); });
    connect(m_bold, &QCheckBox::toggled, this, [this] { emit changed(); });
    connect(m_italic, &QCheckBox::toggled, this, [this] { emit changed(); });
    connect(m_vertical, &QCheckBox::toggled, this, [this] { emit changed(); });
}

void WordsEditor::ensureFonts()
{
    if (m_fontsListed)
        return;
    m_fontsListed = true;
    QApplication::setOverrideCursor(Qt::WaitCursor);  // the first look at the system fonts can take a moment
    const auto fonts = text::availableFonts();
    QApplication::restoreOverrideCursor();
    const QSignalBlocker block(m_font);
    for (const std::string& f : fonts)
        m_font->addItem(QString::fromStdString(f));
}

void WordsEditor::setWords(const std::string& text, const std::string& font, bool bold, bool italic, bool vertical)
{
    ensureFonts();
    const QSignalBlocker b0(m_text), b1(m_font), b2(m_bold), b3(m_italic), b4(m_vertical);
    if (!m_text->hasFocus() || this->text() != text)
        m_text->setText(QString::fromStdString(text));
    m_lastText = text;
    const QString name = QString::fromStdString(text::resolvedFont(font));
    int index = m_font->findText(name, Qt::MatchFixedString);
    if (index < 0) {
        m_font->addItem(name);
        index = m_font->count() - 1;
    }
    m_font->setCurrentIndex(index);
    m_bold->setChecked(bold);
    m_italic->setChecked(italic);
    m_vertical->setChecked(vertical);
}

std::string WordsEditor::text() const
{
    return m_text->text().toStdString();
}

std::string WordsEditor::font() const
{
    return m_font->currentText().toStdString();
}

bool WordsEditor::bold() const
{
    return m_bold->isChecked();
}

bool WordsEditor::italic() const
{
    return m_italic->isChecked();
}

}  // namespace forge::app

namespace forge::app {

bool WordsEditor::vertical() const
{
    return m_vertical->isChecked();
}

}  // namespace forge::app
