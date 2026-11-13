// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSlider;

namespace forge::app {

class WordsEditor;

// The Emboss tool's settings: words or a picture, how big and deep, raised or
// engraved, and their turn. Clicking the shape (in the view) chooses where they go. It
// only reports changes; the main window owns the spot and the document.
class EmbossPanel : public QWidget {
    Q_OBJECT

public:
    explicit EmbossPanel(QWidget* parent = nullptr);

    // editing: changing an existing embossed shape. usePicture: start with the picture.
    void start(const QString& title, bool editing, const EmbossText& words, const EmbossPicture& picture,
               bool usePicture);
    bool usingPicture() const;
    EmbossText words() const;
    EmbossPicture picture() const;
    // A newly chosen picture (keeps the other settings).
    void setPicture(const std::string& name, std::shared_ptr<const PictureData> image);
    // The line under the title: what to do next, or what's wrong.
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();         // a finished change: preview now
    void typing();          // a key press in the words: preview soon
    void applyRequested();
    void pictureRequested();  // "Choose a picture..."
    // Move the words to the middle of their face: 0 both ways, 1 top-bottom, 2 left-right.
    void centerRequested(int how);
    void cancelRequested();

private:
    void showWhat();

    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QComboBox* m_what = nullptr;
    QWidget* m_wordsBox = nullptr;
    WordsEditor* m_words = nullptr;
    QDoubleSpinBox* m_letterHeight = nullptr;
    QGroupBox* m_pictureBox = nullptr;
    QLabel* m_pictureName = nullptr;
    QComboBox* m_kind = nullptr;
    QLabel* m_thresholdLabel = nullptr;
    QWidget* m_thresholdRow = nullptr;
    QSlider* m_threshold = nullptr;
    QLabel* m_thresholdValue = nullptr;
    QCheckBox* m_invert = nullptr;
    QDoubleSpinBox* m_pictureWidth = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QGroupBox* m_raiseGroup = nullptr;
    QRadioButton* m_raised = nullptr;
    QRadioButton* m_engraved = nullptr;
    QLabel* m_hint = nullptr;
    QPushButton* m_apply = nullptr;
    bool m_editing = false;
    // The picture's own data (the widgets hold the settings).
    std::string m_pictureFile;
    std::shared_ptr<const PictureData> m_image;
};

}  // namespace forge::app
