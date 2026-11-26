// SPDX-License-Identifier: GPL-3.0-or-later
#include "BrickFigureDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

}  // namespace

BrickFigureDialog::BrickFigureDialog(double figureHeight, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Brick Figure"));
    auto* layout = new QVBoxLayout(this);
    auto* about = new QLabel(tr("Puts the figure's own head on a new toy body of blocks: legs, hips and torso, arms "
                                "with round hands, and a post the head sits on. The figure stays as it is; the brick "
                                "figure is made beside it."));
    about->setWordWrap(true);
    layout->addWidget(about);

    // The build list, its label resting on it.
    m_build = new QComboBox;
    m_build->addItem(tr("Parts that go together (the arms, legs and head turn)"));
    m_build->addItem(tr("One piece"));
    m_build->setToolTip(tr("Parts: the head, torso, two arms and two legs, joined on pegs with the fit below. One "
                           "piece: the whole figure as a single shape (a statue)"));
    auto* buildColumn = new QVBoxLayout;
    buildColumn->setSpacing(0);
    buildColumn->addWidget(new QLabel(tr("Build as:")));
    buildColumn->addWidget(m_build);
    layout->addLayout(buildColumn);

    auto* form = new QFormLayout;
    m_height = new QDoubleSpinBox;
    m_height->setRange(brickfig::kLeastHeight, brickfig::kMostHeight);
    m_height->setDecimals(1);
    m_height->setSingleStep(5.0);
    m_height->setSuffix(tr(" mm"));
    m_height->setToolTip(tr("How tall the brick figure is printed, head included"));
    form->addRow(tr("Height:"), m_height);
    m_headSize = new QSpinBox;
    m_headSize->setRange(int(brickfig::kLeastHeadSize * 100), int(brickfig::kMostHeadSize * 100));
    m_headSize->setSingleStep(5);
    m_headSize->setSuffix(tr(" %"));
    m_headSize->setToolTip(tr("The head's size: 100% is the usual (the top 30% of the height); more makes a bigger, "
                              "chunkier head on the same body"));
    form->addRow(tr("Head size:"), m_headSize);
    m_fit = new QDoubleSpinBox;
    m_fit->setRange(0.0, 1.0);
    m_fit->setDecimals(2);
    m_fit->setSingleStep(0.05);
    m_fit->setSuffix(tr(" mm"));
    m_fit->setToolTip(tr("Room round each peg in its hole, and round the head's post: more if the parts are tight"));
    form->addRow(tr("Fit:"), m_fit);
    m_grip = new QDoubleSpinBox;
    m_grip->setRange(0.0, 10.0);
    m_grip->setDecimals(1);
    m_grip->setSingleStep(0.5);
    m_grip->setSuffix(tr(" mm"));
    m_grip->setSpecialValueText(tr("None"));
    m_grip->setToolTip(tr("A hole down through each hand for an accessory's handle (Accessories makes handles 3 mm "
                          "across). None: solid hands"));
    form->addRow(tr("Grip holes:"), m_grip);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Make Brick Figure"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    const auto s = preferences();
    const double offered = std::clamp(figureHeight, brickfig::kLeastHeight, brickfig::kMostHeight);
    m_height->setValue(s.value(QStringLiteral("brickFigure/height"), offered).toDouble());
    m_headSize->setValue(s.value(QStringLiteral("brickFigure/headSize"), 100).toInt());
    m_build->setCurrentIndex(s.value(QStringLiteral("brickFigure/onePiece"), false).toBool() ? 1 : 0);
    m_fit->setValue(s.value(QStringLiteral("brickFigure/fit"), 0.15).toDouble());
    m_grip->setValue(s.value(QStringLiteral("brickFigure/grip"), 3.0).toDouble());
    connect(this, &QDialog::accepted, this, [this] {
        auto p = preferences();
        p.setValue(QStringLiteral("brickFigure/height"), m_height->value());
        p.setValue(QStringLiteral("brickFigure/headSize"), m_headSize->value());
        p.setValue(QStringLiteral("brickFigure/onePiece"), m_build->currentIndex() == 1);
        p.setValue(QStringLiteral("brickFigure/fit"), m_fit->value());
        p.setValue(QStringLiteral("brickFigure/grip"), m_grip->value());
    });
}

brickfig::Settings BrickFigureDialog::settings() const
{
    brickfig::Settings s;
    s.height = m_height->value();
    s.headSize = m_headSize->value() / 100.0;
    s.build = m_build->currentIndex() == 1 ? brickfig::Build::OnePiece : brickfig::Build::Parts;
    s.fit = m_fit->value();
    s.grip = m_grip->value();
    return s;
}

}  // namespace forge::app
