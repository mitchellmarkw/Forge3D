// SPDX-License-Identifier: GPL-3.0-or-later
#include "BrickFigurePanel.h"

#include "forge/core/FigureStyle.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

QLabel* wrapped(const QString& text)
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    return label;
}

QDoubleSpinBox* spin(double lo, double hi, int decimals, double step, const QString& suffix, const QString& tip)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(lo, hi);
    box->setDecimals(decimals);
    box->setSingleStep(step);
    box->setSuffix(suffix);
    box->setKeyboardTracking(false);
    box->setToolTip(tip);
    return box;
}

}  // namespace

BrickFigurePanel::BrickFigurePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_title = new QLabel;
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    layout->addWidget(m_title);
    layout->addWidget(wrapped(tr("Puts the figure's own head on a new toy body in the build of the brick-compatible "
                                 "figures. The figure stays as it is; the brick figure is made beside it.")));

    auto* cutBox = new QGroupBox(tr("Where the head comes off"));
    auto* cut = new QVBoxLayout(cutBox);
    // The way it's chosen, its label resting on its list.
    m_headCut = new QComboBox;
    m_headCut->addItem(tr("A flat cut at the neck (the orange disc)"));
    m_headCut->addItem(tr("A mask painted over the head"));
    m_headCut->setToolTip(tr("A flat cut you move up or down the neck and tip; or a mask you paint over the head "
                             "(for hair or a collar a flat cut wouldn't follow): the cut goes flat through the paint's "
                             "edge"));
    auto* headCutColumn = new QVBoxLayout;
    headCutColumn->setSpacing(0);
    headCutColumn->addWidget(new QLabel(tr("Choose the head by:")));
    headCutColumn->addWidget(m_headCut);
    cut->addLayout(headCutColumn);
    m_cutPlace = new QWidget;
    auto* cutColumn = new QVBoxLayout(m_cutPlace);
    cutColumn->setContentsMargins(0, 0, 0, 0);
    cutColumn->addWidget(wrapped(tr("The orange disc on the figure is the cut: found under the chin, where the neck "
                                    "is narrowest. Move it until it's where the head should come off.")));
    auto* cutForm = new QFormLayout;
    m_raise = spin(-figurestyle::kMostRaise, figurestyle::kMostRaise, 1, 0.5, tr(" mm"),
                   tr("Move the cut up the neck (more) or down it (less than 0) from where Forge3D found it"));
    cutForm->addRow(tr("Cut higher by:"), m_raise);
    m_tilt = spin(-figurestyle::kMostTilt, figurestyle::kMostTilt, 0, 5.0, tr("°"),
                  tr("Tip the cut forward, lower at the front (more), or back (less than 0): under a chin, or along "
                     "a jaw"));
    cutForm->addRow(tr("Tip forward:"), m_tilt);
    cutColumn->addLayout(cutForm);
    cut->addWidget(m_cutPlace);
    m_maskPlace = new QWidget;
    auto* maskColumn = new QVBoxLayout(m_maskPlace);
    maskColumn->setContentsMargins(0, 0, 0, 0);
    maskColumn->addWidget(wrapped(tr("Paint over the whole head, down to where it should come off the neck. The cut "
                                     "goes flat through the paint's edge.")));
    m_maskState = wrapped({});
    maskColumn->addWidget(m_maskState);
    auto* paint = new QPushButton(tr("Paint the Head..."));
    paint->setToolTip(tr("Paint the mask over the head (the lasso works well from the front), then come back here"));
    maskColumn->addWidget(paint);
    cut->addWidget(m_maskPlace);
    layout->addWidget(cutBox);

    auto* headBox = new QGroupBox(tr("The head on the body"));
    auto* headForm = new QFormLayout(headBox);
    m_headSize = new QSpinBox;
    m_headSize->setRange(int(brickfig::kLeastHeadSize * 100), int(brickfig::kMostHeadSize * 100));
    m_headSize->setSingleStep(5);
    m_headSize->setSuffix(tr(" %"));
    m_headSize->setToolTip(tr("The head's size: 100% is the usual (the top 30% of the height); more makes a bigger "
                              "head on the same body"));
    headForm->addRow(tr("Head size:"), m_headSize);
    m_forward = spin(-40.0, 40.0, 1, 0.5, tr(" mm"),
                     tr("The middle of the head's neck sits on the post's middle; move the head forward (more) or "
                        "back (less than 0) from there"));
    headForm->addRow(tr("Head forward by:"), m_forward);
    layout->addWidget(headBox);

    auto* bodyBox = new QGroupBox(tr("The body"));
    auto* bodyLayout = new QVBoxLayout(bodyBox);
    // The build list, its label resting on it.
    m_build = new QComboBox;
    m_build->addItem(tr("Parts that go together (the arms, legs and head turn)"));
    m_build->addItem(tr("One piece"));
    m_build->setToolTip(tr("Parts: the head, torso, two arms and two legs: the arms turn on pegs, the legs on the "
                           "hips' axle, the head on its post. One piece: the whole figure as a single shape"));
    auto* buildColumn = new QVBoxLayout;
    buildColumn->setSpacing(0);
    buildColumn->addWidget(new QLabel(tr("Build as:")));
    buildColumn->addWidget(m_build);
    bodyLayout->addLayout(buildColumn);
    auto* bodyForm = new QFormLayout;
    m_height = spin(brickfig::kLeastHeight, brickfig::kMostHeight, 1, 5.0, tr(" mm"),
                    tr("How tall the brick figure is printed, head included"));
    bodyForm->addRow(tr("Height:"), m_height);
    m_fit = spin(0.0, 1.0, 2, 0.05, tr(" mm"),
                 tr("Room round each peg and the axle in their holes, and round the head's post: more if the parts "
                    "are tight"));
    bodyForm->addRow(tr("Fit:"), m_fit);
    m_grip = spin(0.0, 10.0, 1, 0.5, tr(" mm"),
                  tr("The hole through each C-shaped hand, for an accessory's handle (Accessories makes handles 3 mm "
                     "across). None: solid hands"));
    m_grip->setSpecialValueText(tr("None"));
    bodyForm->addRow(tr("Grip holes:"), m_grip);
    bodyLayout->addLayout(bodyForm);
    layout->addWidget(bodyBox);

    m_preview = new QCheckBox(tr("Show the brick figure beside the figure"));
    m_preview->setToolTip(tr("A quick, rougher look at the brick figure where it will be made, following every change "
                             "(the head's socket isn't shown)"));
    m_preview->setChecked(preferences().value(QStringLiteral("brickFigure/preview"), true).toBool());
    layout->addWidget(m_preview);
    m_status = wrapped({});
    m_status->hide();
    layout->addWidget(m_status);
    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton(tr("Cancel"));
    auto* apply = new QPushButton(tr("Make Brick Figure"));
    apply->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(apply);
    layout->addLayout(buttons);
    layout->addStretch(1);

    for (QDoubleSpinBox* box : {m_raise, m_tilt})
        connect(box, &QDoubleSpinBox::valueChanged, this, &BrickFigurePanel::cutChanged);
    for (QDoubleSpinBox* box : {m_raise, m_tilt, m_forward, m_height, m_fit, m_grip})
        connect(box, &QDoubleSpinBox::valueChanged, this, &BrickFigurePanel::changed);
    connect(m_headSize, &QSpinBox::valueChanged, this, &BrickFigurePanel::changed);
    connect(m_build, &QComboBox::currentIndexChanged, this, &BrickFigurePanel::changed);
    connect(m_headCut, &QComboBox::currentIndexChanged, this, [this] {
        showHeadCut();
        emit cutChanged();
        emit changed();
    });
    connect(m_preview, &QCheckBox::toggled, this, [this](bool on) {
        preferences().setValue(QStringLiteral("brickFigure/preview"), on);
        emit changed();
    });
    connect(paint, &QPushButton::clicked, this, &BrickFigurePanel::paintHeadRequested);
    showHeadCut();
    connect(apply, &QPushButton::clicked, this, [this] {
        remember();
        emit applyRequested();
    });
    connect(cancel, &QPushButton::clicked, this, &BrickFigurePanel::cancelRequested);
}

void BrickFigurePanel::start(const QString& title, double figureHeight)
{
    m_title->setText(title);
    const auto s = preferences();
    const QSignalBlocker e(m_height), f(m_headSize), g(m_build), h(m_fit), i(m_grip);
    const QSignalBlocker a(m_raise), b(m_tilt);
    m_raise->setValue(0.0);  // each figure's own cut
    m_tilt->setValue(0.0);
    const QSignalBlocker c(m_forward), d(m_headCut);
    m_forward->setValue(0.0);
    m_headCut->setCurrentIndex(0);
    showHeadCut();
    setMaskStrokes(0);
    const double offered = std::clamp(figureHeight, brickfig::kLeastHeight, brickfig::kMostHeight);
    m_height->setValue(s.value(QStringLiteral("brickFigure/height"), offered).toDouble());
    m_headSize->setValue(s.value(QStringLiteral("brickFigure/headSize"), 100).toInt());
    m_build->setCurrentIndex(s.value(QStringLiteral("brickFigure/onePiece"), false).toBool() ? 1 : 0);
    m_fit->setValue(s.value(QStringLiteral("brickFigure/fit"), 0.15).toDouble());
    m_grip->setValue(s.value(QStringLiteral("brickFigure/grip"), 3.0).toDouble());
    setStatus({}, false);
}

bool BrickFigurePanel::headByMask() const
{
    return m_headCut->currentIndex() == 1;
}

bool BrickFigurePanel::showPreview() const
{
    return m_preview->isChecked();
}

void BrickFigurePanel::showHeadCut()
{
    m_cutPlace->setVisible(!headByMask());
    m_maskPlace->setVisible(headByMask());
}

void BrickFigurePanel::setMaskStrokes(int strokes)
{
    m_maskState->setText(strokes > 0 ? tr("Painted (%n stroke(s)).", nullptr, strokes)
                                     : tr("Nothing painted yet: click Paint the Head."));
}

void BrickFigurePanel::remember() const
{
    auto p = preferences();
    p.setValue(QStringLiteral("brickFigure/height"), m_height->value());
    p.setValue(QStringLiteral("brickFigure/headSize"), m_headSize->value());
    p.setValue(QStringLiteral("brickFigure/onePiece"), m_build->currentIndex() == 1);
    p.setValue(QStringLiteral("brickFigure/fit"), m_fit->value());
    p.setValue(QStringLiteral("brickFigure/grip"), m_grip->value());
}

brickfig::Settings BrickFigurePanel::settings() const
{
    brickfig::Settings s;
    s.height = m_height->value();
    s.headSize = m_headSize->value() / 100.0;
    s.build = m_build->currentIndex() == 1 ? brickfig::Build::OnePiece : brickfig::Build::Parts;
    s.fit = m_fit->value();
    s.grip = m_grip->value();
    s.raise = m_raise->value();
    s.tilt = m_tilt->value();
    s.headForward = m_forward->value();
    return s;
}

void BrickFigurePanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
