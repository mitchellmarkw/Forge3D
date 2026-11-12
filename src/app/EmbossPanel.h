// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QRadioButton;

namespace forge::app {

class WordsEditor;

// The Emboss tool's settings: the words, how big and deep, raised or engraved, and
// their turn. Clicking the shape (in the view) chooses where they go. It only reports
// changes; the main window owns the spot and the document.
class EmbossPanel : public QWidget {
    Q_OBJECT

public:
    explicit EmbossPanel(QWidget* parent = nullptr);

    // editing: changing the words of an existing embossed shape.
    void start(const QString& title, bool editing, const EmbossText& words);
    EmbossText words() const;
    // The line under the title: what to do next, or what's wrong.
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();         // a finished change: preview now
    void typing();          // a key press in the words: preview soon
    void applyRequested();
    // Move the words to the middle of their face: 0 both ways, 1 top-bottom, 2 left-right.
    void centerRequested(int how);
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    WordsEditor* m_words = nullptr;
    QDoubleSpinBox* m_letterHeight = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QRadioButton* m_raised = nullptr;
    QRadioButton* m_engraved = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
