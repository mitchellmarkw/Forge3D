// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QWidget>

#include <string>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace forge::app {

// The words, font, bold and italic: used by the Text shape's properties and by the
// Emboss tool. The font list (the fonts installed on this PC) is filled the first
// time it is shown.
class WordsEditor : public QWidget {
    Q_OBJECT

public:
    explicit WordsEditor(QWidget* parent = nullptr);

    // Shows these values without sending signals. An empty font = the default one.
    void setWords(const std::string& text, const std::string& font, bool bold, bool italic, bool vertical);
    std::string text() const;
    std::string font() const;  // the family chosen (never empty once fonts are listed)
    bool bold() const;
    bool italic() const;
    bool vertical() const;  // letters stacked top to bottom

signals:
    // Each key press in the words box (for live previews).
    void textEdited();
    // A finished change: Enter or leaving the words box, or a new font / style.
    void changed();

private:
    void ensureFonts();

    QLineEdit* m_text = nullptr;
    QComboBox* m_font = nullptr;
    QCheckBox* m_bold = nullptr;
    QCheckBox* m_italic = nullptr;
    QCheckBox* m_vertical = nullptr;
    std::string m_lastText;
    bool m_fontsListed = false;
};

}  // namespace forge::app
