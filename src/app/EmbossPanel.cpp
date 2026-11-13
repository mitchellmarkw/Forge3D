// SPDX-License-Identifier: GPL-3.0-or-later
#include "EmbossPanel.h"

#include "WordsEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
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

// A label with its combo box right under it.
QWidget* labelled(const QString& label, QWidget* field)
{
    auto* box = new QWidget;
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    auto* l = new QLabel(label);
    l->setBuddy(field);
    v->addWidget(l);
    v->addWidget(field);
    return box;
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

    m_what = new QComboBox;
    m_what->addItem(tr("Words"));
    m_what->addItem(tr("A picture (logo or photo)"));
    m_what->setToolTip(tr("Emboss words, or a picture from a file"));
    layout->addWidget(labelled(tr("Emboss:"), m_what));

    // Words.
    m_wordsBox = new QWidget;
    auto* wordsLayout = new QVBoxLayout(m_wordsBox);
    wordsLayout->setContentsMargins(0, 0, 0, 0);
    m_words = new WordsEditor;
    wordsLayout->addWidget(m_words);
    auto* letterForm = new QFormLayout;
    m_letterHeight = spin(0.5, 1000.0, 1.0, tr(" mm"), 2, tr("How tall a capital letter is"));
    letterForm->addRow(tr("Letter height:"), m_letterHeight);
    wordsLayout->addLayout(letterForm);
    layout->addWidget(m_wordsBox);

    // A picture.
    m_pictureBox = new QGroupBox(tr("Picture"));
    auto* pictureLayout = new QVBoxLayout(m_pictureBox);
    auto* choose = new QPushButton(tr("Choose a picture..."));
    choose->setToolTip(tr("A PNG, JPG, BMP, GIF or SVG file"));
    pictureLayout->addWidget(choose);
    m_pictureName = new QLabel;
    m_pictureName->setWordWrap(true);
    pictureLayout->addWidget(m_pictureName);
    m_kind = new QComboBox;
    m_kind->addItem(tr("Logo (one height)"));
    m_kind->addItem(tr("Photo (shades become heights)"));
    m_kind->setToolTip(tr("Logo: the dark parts traced to a clean outline, all one height.\n"
                          "Photo: every shade a height, like a carving (a lithophane look)."));
    pictureLayout->addWidget(labelled(tr("Kind:"), m_kind));
    m_thresholdRow = new QWidget;
    auto* thresholdLayout = new QVBoxLayout(m_thresholdRow);
    thresholdLayout->setContentsMargins(0, 0, 0, 0);
    thresholdLayout->setSpacing(2);
    m_thresholdLabel = new QLabel(tr("Where dark ends:"));
    thresholdLayout->addWidget(m_thresholdLabel);
    auto* sliderRow = new QHBoxLayout;
    m_threshold = new QSlider(Qt::Horizontal);
    m_threshold->setRange(1, 254);
    m_threshold->setValue(128);
    m_threshold->setTracking(false);  // preview when let go
    m_threshold->setToolTip(tr("Shades darker than this make the shape. Slide right to take in lighter greys, "
                               "left for only the darkest parts."));
    m_thresholdValue = new QLabel;
    m_thresholdValue->setMinimumWidth(30);
    sliderRow->addWidget(m_threshold, 1);
    sliderRow->addWidget(m_thresholdValue);
    thresholdLayout->addLayout(sliderRow);
    pictureLayout->addWidget(m_thresholdRow);
    m_invert = new QCheckBox;
    pictureLayout->addWidget(m_invert);
    auto* pictureForm = new QFormLayout;
    m_pictureWidth = spin(1.0, 2000.0, 1.0, tr(" mm"), 1, tr("How wide the picture is on the surface (its height "
                                                             "follows its shape)"));
    pictureForm->addRow(tr("Width:"), m_pictureWidth);
    pictureLayout->addLayout(pictureForm);
    layout->addWidget(m_pictureBox);

    auto* sizeGroup = new QGroupBox(tr("Depth and turn"));
    auto* form = new QFormLayout(sizeGroup);
    m_depth = spin(0.05, 100.0, 0.25, tr(" mm"), 2,
                   tr("How far it stands out from the surface (raised) or goes into it (engraved). For a photo, "
                      "the most: the darkest parts."));
    m_angle = spin(-180.0, 180.0, 15.0, tr(" °"), 1, tr("Turn it on the surface (anticlockwise)"));
    m_angle->setWrapping(true);
    form->addRow(tr("Depth:"), m_depth);
    form->addRow(tr("Turn:"), m_angle);
    layout->addWidget(sizeGroup);

    m_raiseGroup = new QGroupBox(tr("Raised or engraved"));
    auto* kindLayout = new QVBoxLayout(m_raiseGroup);
    m_raised = new QRadioButton(tr("Raised (standing out)"));
    m_engraved = new QRadioButton(tr("Engraved (cut in)"));
    m_raised->setChecked(true);
    kindLayout->addWidget(m_raised);
    kindLayout->addWidget(m_engraved);
    layout->addWidget(m_raiseGroup);

    auto* centerGroup = new QGroupBox(tr("Center on the face"));
    auto* centerLayout = new QHBoxLayout(centerGroup);
    const struct {
        QString label, tip;
    } centers[] = {
        {tr("All"), tr("Move it to the middle of the face it's on")},
        {tr("Top-Bottom"), tr("Move it up or down to the middle of the face")},
        {tr("Left-Right"), tr("Move it across to the middle of the face")},
    };
    for (int i = 0; i < 3; ++i) {
        auto* button = new QPushButton(centers[i].label);
        button->setToolTip(centers[i].tip);
        connect(button, &QPushButton::clicked, this, [this, i] { emit centerRequested(i); });
        centerLayout->addWidget(button);
    }
    layout->addWidget(centerGroup);

    m_hint = new QLabel;
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_hint);

    auto* buttons = new QHBoxLayout;
    m_apply = new QPushButton(tr("Emboss"));
    m_apply->setDefault(true);
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setToolTip(tr("Close without changing anything (Esc)"));
    buttons->addWidget(m_apply);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_what, &QComboBox::currentIndexChanged, this, [this] {
        showWhat();
        emit changed();
    });
    connect(m_words, &WordsEditor::changed, this, &EmbossPanel::changed);
    connect(m_words, &WordsEditor::textEdited, this, &EmbossPanel::typing);
    connect(choose, &QPushButton::clicked, this, &EmbossPanel::pictureRequested);
    connect(m_kind, &QComboBox::currentIndexChanged, this, [this] {
        showWhat();
        emit changed();
    });
    connect(m_threshold, &QSlider::sliderMoved, this, [this](int v) { m_thresholdValue->setNum(v); });
    connect(m_threshold, &QSlider::valueChanged, this, [this](int v) {
        m_thresholdValue->setNum(v);
        emit changed();
    });
    connect(m_invert, &QCheckBox::toggled, this, &EmbossPanel::changed);
    for (QDoubleSpinBox* s : {m_letterHeight, m_pictureWidth, m_depth, m_angle})
        connect(s, &QDoubleSpinBox::valueChanged, this, &EmbossPanel::changed);
    connect(m_raised, &QRadioButton::toggled, this, &EmbossPanel::changed);
    connect(m_apply, &QPushButton::clicked, this, &EmbossPanel::applyRequested);
    connect(cancel, &QPushButton::clicked, this, &EmbossPanel::cancelRequested);
    showWhat();
}

void EmbossPanel::showWhat()
{
    const bool picture = usingPicture();
    const bool photo = m_kind->currentIndex() == 1;
    m_wordsBox->setVisible(!picture);
    m_pictureBox->setVisible(picture);
    m_thresholdRow->setVisible(!photo);
    m_invert->setText(photo ? tr("Invert (light parts stand highest)") : tr("Invert (light parts make the shape)"));
    m_hint->setText(picture ? tr("Click anywhere on the shape to move the picture there. It follows the surface, so "
                                 "it bends round curves.")
                            : tr("Click anywhere on the shape to move the words there. They follow the surface, "
                                 "so they bend round curves."));
    m_apply->setToolTip(m_editing ? tr("Make the change on the shape")
                                  : (picture ? tr("Put the picture on the shape") : tr("Put the words on the shape")));
}

void EmbossPanel::start(const QString& title, bool editing, const EmbossText& words, const EmbossPicture& picture,
                        bool usePicture)
{
    m_editing = editing;
    m_title->setText(title);
    m_apply->setText(editing ? tr("Apply") : tr("Emboss"));
    m_words->setWords(words.text, words.font, words.bold, words.italic, words.vertical);
    m_pictureFile = picture.name;
    m_image = picture.image;
    const QSignalBlocker b0(m_letterHeight), b1(m_depth), b2(m_angle), b3(m_raised), b4(m_engraved), b5(m_what),
        b6(m_kind), b7(m_threshold), b8(m_invert), b9(m_pictureWidth);
    m_what->setCurrentIndex(usePicture ? 1 : 0);
    m_letterHeight->setValue(words.letterHeight);
    m_kind->setCurrentIndex(picture.relief ? 1 : 0);
    m_threshold->setValue(picture.threshold);
    m_thresholdValue->setNum(picture.threshold);
    m_invert->setChecked(picture.invert);
    m_pictureWidth->setValue(picture.width);
    const bool raised = usePicture ? picture.raised : words.raised;
    m_depth->setValue(usePicture ? picture.depth : words.depth);
    m_angle->setValue(usePicture ? picture.angle : words.angle);
    m_raised->setChecked(raised);
    m_engraved->setChecked(!raised);
    // Editing one changes that one: the other kind can't be swapped in.
    m_what->setEnabled(!editing);
    setPicture(picture.name, picture.image);
    showWhat();
}

bool EmbossPanel::usingPicture() const
{
    return m_what->currentIndex() == 1;
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

EmbossPicture EmbossPanel::picture() const
{
    EmbossPicture p;
    p.name = m_pictureFile;
    p.image = m_image;
    p.relief = m_kind->currentIndex() == 1;
    p.threshold = m_threshold->value();
    p.invert = m_invert->isChecked();
    p.width = m_pictureWidth->value();
    p.depth = m_depth->value();
    p.angle = m_angle->value();
    p.raised = m_raised->isChecked();
    return p;
}

void EmbossPanel::setPicture(const std::string& name, std::shared_ptr<const PictureData> image)
{
    m_pictureFile = name;
    m_image = std::move(image);
    if (!m_image)
        m_pictureName->setText(tr("No picture yet."));
    else
        m_pictureName->setText(tr("%1 (%2 × %3)")
                                   .arg(QString::fromStdString(name.empty() ? std::string("Picture") : name))
                                   .arg(m_image->width)
                                   .arg(m_image->height));
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
