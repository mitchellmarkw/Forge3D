// SPDX-License-Identifier: GPL-3.0-or-later
#include "SculptPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {

namespace {
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

SculptPanel::SculptPanel(QWidget* parent) : QWidget(parent)
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

    m_brush = new QComboBox;
    // Each item's data is its sculpt::Brush number (the list isn't in that order).
    const std::pair<const char*, int> brushes[] = {
        {QT_TR_NOOP("Draw"), 0},    {QT_TR_NOOP("Smooth"), 1},  {QT_TR_NOOP("Inflate"), 2},
        {QT_TR_NOOP("Flatten"), 3}, {QT_TR_NOOP("Pinch"), 4},   {QT_TR_NOOP("Clay"), 5},
        {QT_TR_NOOP("Grab"), 6},    {QT_TR_NOOP("Crease"), 7},  {QT_TR_NOOP("Snake hook"), 8},
        {QT_TR_NOOP("Pattern"), 13}, {QT_TR_NOOP("Mask"), 9},
    };
    for (const auto& [name, id] : brushes)
        m_brush->addItem(tr(name), id);
    m_brush->setToolTip(tr("Hold Shift with any brush to smooth"));
    layout->addWidget(labelled(tr("Brush:"), m_brush));
    m_brushHint = new QLabel;
    m_brushHint->setWordWrap(true);
    m_brushHint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(m_brushHint);

    // The Pattern brush's pattern (its label above it) and repeat size.
    m_patternBox = new QWidget;
    auto* patternLayout = new QVBoxLayout(m_patternBox);
    patternLayout->setContentsMargins(0, 0, 0, 0);
    m_pattern = new QComboBox;
    const std::pair<const char*, const char*> builtIn[] = {
        {"scales", QT_TR_NOOP("Scales")}, {"hexagons", QT_TR_NOOP("Hexagons")}, {"weave", QT_TR_NOOP("Woven cloth")},
        {"skin", QT_TR_NOOP("Skin")},     {"bricks", QT_TR_NOOP("Bricks")},
        {"chainmail", QT_TR_NOOP("Chainmail")}, {"twill", QT_TR_NOOP("Twill (denim)")}, {"knit", QT_TR_NOOP("Knitting")},
        {"leather", QT_TR_NOOP("Leather")}, {"cracks", QT_TR_NOOP("Cracked stone")}, {"wood", QT_TR_NOOP("Wood grain")},
        {"pores", QT_TR_NOOP("Skin pores")},
    };
    for (const auto& [name, label] : builtIn) {
        SculptPattern p;
        p.name = name;
        m_patterns.push_back(p);
        m_pattern->addItem(tr(label));
    }
    m_pattern->addItem(tr("Your own picture..."));
    m_pattern->setToolTip(tr("What the brush presses in; light parts stand highest. Your own picture: a logo, an "
                             "emblem, a scar..."));
    patternLayout->addWidget(labelled(tr("Pattern:"), m_pattern));
    m_patternPress = new QComboBox;
    m_patternPress->addItem(tr("Repeating, where you drag"));
    m_patternPress->addItem(tr("Once, as a stamp, where you click"));
    m_patternPress->setToolTip(tr("Repeating covers the surface you drag over; a stamp presses the picture once, "
                                  "centred where you click (good for an emblem or a patch)"));
    patternLayout->addWidget(labelled(tr("Press it:"), m_patternPress));
    auto* patternForm = new QFormLayout;
    patternForm->setContentsMargins(0, 0, 0, 0);
    m_patternSize = new QDoubleSpinBox;
    m_patternSize->setRange(0.2, 100.0);
    m_patternSize->setDecimals(1);
    m_patternSize->setSingleStep(0.5);
    m_patternSize->setValue(4.0);
    m_patternSize->setSuffix(tr(" mm"));
    m_patternSize->setToolTip(tr("How wide one repeat of the pattern is. Strength sets how deep it goes."));
    m_patternSizeLabel = new QLabel(tr("Pattern size:"));
    patternForm->addRow(m_patternSizeLabel, m_patternSize);
    m_patternTurn = new QDoubleSpinBox;
    m_patternTurn->setRange(-180.0, 180.0);
    m_patternTurn->setDecimals(0);
    m_patternTurn->setSingleStep(15.0);
    m_patternTurn->setSuffix(tr("°"));
    m_patternTurn->setToolTip(tr("Turns the picture round, flat on the surface"));
    patternForm->addRow(tr("Turned:"), m_patternTurn);
    patternLayout->addLayout(patternForm);
    layout->addWidget(m_patternBox);
    connect(m_pattern, &QComboBox::activated, this, [this](int index) {
        if (index == m_pattern->count() - 1) {
            // "Your own picture...": keep the last choice until a picture comes.
            const QSignalBlocker block(m_pattern);
            m_pattern->setCurrentIndex(m_lastPattern);
            emit ownPatternRequested();
            return;
        }
        m_lastPattern = index;
    });
    connect(m_patternPress, &QComboBox::currentIndexChanged, this, [this](int once) {
        m_patternSizeLabel->setText(once == 1 ? tr("Stamp size:") : tr("Pattern size:"));
        m_patternSize->setToolTip(once == 1 ? tr("How wide the stamp is. Strength sets how deep it goes.")
                                            : tr("How wide one repeat of the pattern is. Strength sets how deep it "
                                                 "goes."));
    });

    const auto describe = [this](int index) {
        const int brush = m_brush->itemData(index).toInt();
        const char* hint = "";
        switch (brush) {
        case 0: hint = QT_TR_NOOP("Pushes the surface out where you drag. Ctrl: pushes in."); break;
        case 1: hint = QT_TR_NOOP("Evens out bumps and dents."); break;
        case 2:
            hint = QT_TR_NOOP("Swells the surface out like a balloon, each spot along its own facing. Ctrl: shrinks.");
            break;
        case 3: hint = QT_TR_NOOP("Presses the area flat."); break;
        case 4:
            hint = QT_TR_NOOP("Draws the surface together under the brush, for sharp creases and edges. Ctrl: pushes "
                              "apart.");
            break;
        case 5: hint = QT_TR_NOOP("Builds up in flat layers, like adding strips of clay. Ctrl: digs out."); break;
        case 6: hint = QT_TR_NOOP("Pulls a whole area along with the mouse, like soft toffee."); break;
        case 7:
            hint = QT_TR_NOOP("Cuts a sharp, narrow groove where you drag, for folds and seams. Ctrl: a sharp ridge.");
            break;
        case 8:
            hint = QT_TR_NOOP("Pulls the surface out after the mouse, for horns, tails and tentacles. Drag away "
                              "from the shape.");
            break;
        case 9:
            hint = QT_TR_NOOP("Paints a mask: the other brushes leave masked parts alone. Ctrl: unpaints. While "
                              "this brush is on, the shape shows in plain colours (black paint on yellow; change "
                              "them in Split by Mask).");
            break;
        case 13:
            hint = QT_TR_NOOP("Presses a pattern into the surface where you drag (scales, cloth, chainmail, skin...), "
                              "or a picture once as a stamp where you click. Going over again doesn't press deeper. "
                              "Ctrl: engraves it.");
            break;
        default: break;
        }
        m_brushHint->setText(tr(hint));
        m_patternBox->setVisible(brush == 13);
    };
    connect(m_brush, &QComboBox::currentIndexChanged, this, describe);
    connect(m_brush, &QComboBox::currentIndexChanged, this, &SculptPanel::brushChanged);
    describe(0);

    // The whole mask at once.
    auto* maskButtons = new QHBoxLayout;
    auto* clearMask = new QPushButton(tr("Clear mask"));
    clearMask->setToolTip(tr("Take the mask off everywhere"));
    auto* invertMask = new QPushButton(tr("Invert mask"));
    invertMask->setToolTip(tr("Mask what's free and free what's masked"));
    maskButtons->addWidget(clearMask);
    maskButtons->addWidget(invertMask);
    maskButtons->addStretch(1);
    layout->addLayout(maskButtons);
    connect(clearMask, &QPushButton::clicked, this, &SculptPanel::clearMaskRequested);
    connect(invertMask, &QPushButton::clicked, this, &SculptPanel::invertMaskRequested);

    // Mirror: the label above its three ticks.
    auto* mirrorBox = new QWidget;
    auto* mirrorLayout = new QVBoxLayout(mirrorBox);
    mirrorLayout->setContentsMargins(0, 0, 0, 0);
    mirrorLayout->setSpacing(2);
    mirrorLayout->addWidget(new QLabel(tr("Mirror (across the middle of the shape):")));
    auto* ticks = new QHBoxLayout;
    const char* names[] = {"X (left-right)", "Y (front-back)", "Z (up-down)"};
    for (int i = 0; i < 3; ++i) {
        m_mirror[i] = new QCheckBox(tr(names[i]));
        m_mirror[i]->setToolTip(tr("What you do on one side happens on the other side too"));
        ticks->addWidget(m_mirror[i]);
    }
    ticks->addStretch(1);
    mirrorLayout->addLayout(ticks);
    layout->addWidget(mirrorBox);

    auto* form = new QFormLayout;
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.2, 200.0);
    m_size->setDecimals(1);
    m_size->setSingleStep(0.5);
    m_size->setSuffix(tr(" mm"));
    m_size->setToolTip(tr("The brush's radius. [ and ] make it smaller and bigger."));
    form->addRow(tr("Size:"), m_size);
    auto* strengthRow = new QHBoxLayout;
    m_strength = new QSlider(Qt::Horizontal);
    m_strength->setRange(1, 100);
    m_strength->setValue(50);
    m_strengthValue = new QLabel(QStringLiteral("50"));
    m_strengthValue->setMinimumWidth(28);
    strengthRow->addWidget(m_strength, 1);
    strengthRow->addWidget(m_strengthValue);
    form->addRow(tr("Strength:"), strengthRow);
    layout->addLayout(form);

    m_detail = new QComboBox;
    m_detail->addItem(tr("Low (about 40,000 triangles)"));
    m_detail->addItem(tr("Medium (about 150,000 triangles)"));
    m_detail->addItem(tr("High (about 500,000 triangles)"));
    m_detail->setCurrentIndex(1);
    m_detail->setToolTip(tr("How finely the shape is divided for sculpting. More is smoother, but slower. Choose "
                            "before you start."));
    layout->addWidget(labelled(tr("Detail:"), m_detail));
    m_addDetail = new QCheckBox(tr("Add detail where I sculpt"));
    m_addDetail->setChecked(true);
    m_addDetail->setToolTip(tr("Splits the triangles under the brush as you go, so fine strokes stay crisp "
                               "without making the whole shape heavy"));
    layout->addWidget(m_addDetail);
    auto* finestForm = new QFormLayout;
    m_finest = new QDoubleSpinBox;
    m_finest->setRange(0.01, 50.0);
    m_finest->setValue(0.1);
    m_finest->setDecimals(2);
    m_finest->setSingleStep(0.05);
    m_finest->setSuffix(tr(" mm"));
    m_finest->setToolTip(tr("How small the added triangles get: smaller is finer, but heavier. (A big brush "
                            "adds coarser ones: a twelfth of its size.)"));
    finestForm->addRow(tr("Finest detail:"), m_finest);
    layout->addLayout(finestForm);
    connect(m_addDetail, &QCheckBox::toggled, this, [this](bool on) {
        m_finest->setEnabled(on);
        emit addDetailChanged();
    });
    connect(m_finest, &QDoubleSpinBox::valueChanged, this, &SculptPanel::addDetailChanged);

    // Remesh: the whole shape made of even triangles of this size.
    auto* remeshRow = new QHBoxLayout;
    auto* remesh = new QPushButton(tr("Remesh"));
    remesh->setToolTip(tr("Make the whole shape of even triangles of this size: tidies up stretched areas "
                          "(after Snake hook or Grab) and thins out over-detailed ones"));
    m_remeshSize = new QDoubleSpinBox;
    m_remeshSize->setRange(0.01, 50.0);
    m_remeshSize->setDecimals(2);
    m_remeshSize->setSingleStep(0.05);
    m_remeshSize->setSuffix(tr(" mm"));
    m_remeshSize->setToolTip(tr("How big the triangles are made: smaller is finer, but heavier"));
    remeshRow->addWidget(remesh);
    remeshRow->addWidget(new QLabel(tr("Triangle size:")));
    remeshRow->addWidget(m_remeshSize, 1);
    layout->addLayout(remeshRow);
    connect(remesh, &QPushButton::clicked, this, &SculptPanel::remeshRequested);

    auto* hint = new QLabel(tr("Drag with the left button over the shape to sculpt. Hold <b>Ctrl</b> for the "
                               "opposite, <b>Shift</b> to smooth. <b>[</b> and <b>]</b> change the size. Turn the view with "
                               "the right button as usual."));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* note = new QLabel(tr("Done keeps the strokes as a step in the shape's history: change a size or a "
                               "hole below it and they are made again on the new shape. Double-click the step to "
                               "add more."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(note);

    auto* buttons = new QHBoxLayout;
    m_undo = new QPushButton(tr("Undo stroke"));
    m_undo->setToolTip(tr("Take back the last stroke (Ctrl+Z)"));
    auto* done = new QPushButton(tr("Done"));
    done->setDefault(true);
    done->setToolTip(tr("Keep the sculpted shape"));
    auto* cancel = new QPushButton(tr("Cancel"));
    cancel->setToolTip(tr("Close without changing anything (Esc)"));
    buttons->addWidget(m_undo);
    buttons->addWidget(done);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    layout->addStretch(1);

    connect(m_strength, &QSlider::valueChanged, this, [this](int v) { m_strengthValue->setNum(v); });
    connect(m_detail, &QComboBox::currentIndexChanged, this, &SculptPanel::detailChanged);
    connect(m_undo, &QPushButton::clicked, this, &SculptPanel::undoRequested);
    connect(done, &QPushButton::clicked, this, &SculptPanel::doneRequested);
    connect(cancel, &QPushButton::clicked, this, &SculptPanel::cancelRequested);
}

void SculptPanel::start(const QString& title, double suggestedSize)
{
    m_title->setText(title);
    const QSignalBlocker block(m_detail);
    m_size->setValue(suggestedSize);
    setDetailEnabled(true);
    setUndoEnabled(false);
}

int SculptPanel::brush() const
{
    return m_brush->currentData().toInt();
}

SculptPattern SculptPanel::pattern() const
{
    const int i = std::clamp(m_pattern->currentIndex(), 0, int(m_patterns.size()) - 1);
    return m_patterns[std::size_t(i)];
}

double SculptPanel::patternSize() const
{
    return m_patternSize->value();
}

bool SculptPanel::patternOnce() const
{
    return m_patternPress->currentIndex() == 1;
}

double SculptPanel::patternTurn() const
{
    return m_patternTurn->value();
}

void SculptPanel::addOwnPattern(const SculptPattern& pattern)
{
    // Before "Your own picture...", which stays last.
    const int at = m_pattern->count() - 1;
    m_patterns.push_back(pattern);
    m_pattern->insertItem(at, QString::fromStdString(pattern.name));
    m_pattern->setCurrentIndex(at);
    m_lastPattern = at;
}

bool SculptPanel::mirrorX() const
{
    return m_mirror[0]->isChecked();
}

bool SculptPanel::mirrorY() const
{
    return m_mirror[1]->isChecked();
}

bool SculptPanel::mirrorZ() const
{
    return m_mirror[2]->isChecked();
}

double SculptPanel::size() const
{
    return m_size->value();
}

double SculptPanel::strength() const
{
    return m_strength->value() / 100.0;
}

SculptPanel::Detail SculptPanel::detail() const
{
    return m_detail->currentIndex() == 2 ? Detail::High : m_detail->currentIndex() == 0 ? Detail::Low : Detail::Medium;
}

void SculptPanel::setSize(double radius)
{
    m_size->setValue(radius);
}

bool SculptPanel::addDetail() const
{
    return m_addDetail->isChecked();
}

double SculptPanel::finestDetail() const
{
    return m_finest->value();
}

void SculptPanel::setDetail(int index)
{
    const QSignalBlocker block(m_detail);
    m_detail->setCurrentIndex(std::clamp(index, 0, 2));
}

double SculptPanel::remeshSize() const
{
    return m_remeshSize->value();
}

void SculptPanel::setRemeshSize(double mm)
{
    m_remeshSize->setValue(mm);
}

void SculptPanel::setFinestDetail(double mm)
{
    const QSignalBlocker block(m_finest);
    m_finest->setValue(mm);
}

void SculptPanel::setDetailEnabled(bool on)
{
    m_detail->setEnabled(on);
}

void SculptPanel::setUndoEnabled(bool on)
{
    m_undo->setEnabled(on);
}

void SculptPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #c0392b;") : QString());
}

}  // namespace forge::app
