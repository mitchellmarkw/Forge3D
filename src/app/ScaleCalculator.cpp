// SPDX-License-Identifier: GPL-3.0-or-later
#include "ScaleCalculator.h"

#include "forge/core/Reference.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <algorithm>

namespace forge::app {
namespace {

// The app's preferences (see MainWindow.cpp).
class Preferences : public QSettings {
public:
    Preferences() : QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"),
                              QStringLiteral("Forge3D"))
    {
    }
};

QStringList unitNames()
{
    return {QObject::tr("mm"),     QObject::tr("cm"),   QObject::tr("m"),
            QObject::tr("inches"), QObject::tr("feet"), QObject::tr("yards")};
}

// A label with its box right under it (the label resting on top of the box).
QWidget* labelled(const QString& text, QWidget* box)
{
    auto* holder = new QWidget;
    auto* column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(box);
    return holder;
}

}  // namespace

ScaleCalculator::ScaleCalculator(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Scale Calculator"));
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(tr("Type a real-world size and the scale you're working at, to get the size on the "
                               "model."));
    help->setWordWrap(true);
    layout->addWidget(help);

    auto* grid = new QGridLayout;
    m_size = new QDoubleSpinBox;
    m_size->setRange(0.0, 1e9);
    m_size->setDecimals(3);
    m_size->setToolTip(tr("The real size (a person's height, a car's length...)"));
    m_unit = new QComboBox;
    m_unit->addItems(unitNames());
    m_unit->setToolTip(tr("The unit the real size is in"));
    grid->addWidget(labelled(tr("Real size:"), m_size), 0, 0);
    grid->addWidget(labelled(tr("Unit:"), m_unit), 0, 1);

    m_scale = new QDoubleSpinBox;
    m_scale->setRange(0.001, 1e6);
    m_scale->setDecimals(2);
    m_scale->setPrefix(tr("1 : "));
    m_scale->setToolTip(tr("The scale: type X for 1:X (12 for 1:12, 60 for 1:60...). 1 is full size."));
    grid->addWidget(labelled(tr("Scale:"), m_scale), 1, 0);
    m_resultUnit = new QComboBox;
    m_resultUnit->addItems(unitNames());
    m_resultUnit->setToolTip(tr("The unit for the size on the model"));
    grid->addWidget(labelled(tr("Answer in:"), m_resultUnit), 1, 1);
    layout->addLayout(grid);

    m_result = new QLabel;
    QFont big = m_result->font();
    big.setPointSizeF(big.pointSizeF() * 1.6);
    big.setBold(true);
    m_result->setFont(big);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setWordWrap(true);
    layout->addWidget(m_result);

    auto* buttons = new QHBoxLayout;
    auto* copy = new QPushButton(tr("Copy"));
    copy->setToolTip(tr("Copy the size on the model, to paste into a size box"));
    auto* close = new QPushButton(tr("Close"));
    buttons->addWidget(copy);
    buttons->addStretch(1);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    {
        const Preferences prefs;
        m_size->setValue(prefs.value(QStringLiteral("scaleCalculator/size"), 6.0).toDouble());
        m_unit->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("scaleCalculator/unit"), 4).toInt(), 0,
                                           reference::kUnitCount - 1));
        m_scale->setValue(prefs.value(QStringLiteral("scaleCalculator/scale"), 12.0).toDouble());
        m_resultUnit->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("scaleCalculator/resultUnit"), 0).toInt(),
                                                 0, reference::kUnitCount - 1));
    }
    connect(m_size, &QDoubleSpinBox::valueChanged, this, &ScaleCalculator::recalculate);
    connect(m_scale, &QDoubleSpinBox::valueChanged, this, &ScaleCalculator::recalculate);
    connect(m_unit, &QComboBox::currentIndexChanged, this, &ScaleCalculator::recalculate);
    connect(m_resultUnit, &QComboBox::currentIndexChanged, this, &ScaleCalculator::recalculate);
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_resultText); });
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    recalculate();
}

void ScaleCalculator::recalculate()
{
    Preferences prefs;
    prefs.setValue(QStringLiteral("scaleCalculator/size"), m_size->value());
    prefs.setValue(QStringLiteral("scaleCalculator/unit"), m_unit->currentIndex());
    prefs.setValue(QStringLiteral("scaleCalculator/scale"), m_scale->value());
    prefs.setValue(QStringLiteral("scaleCalculator/resultUnit"), m_resultUnit->currentIndex());

    const auto unit = static_cast<reference::Unit>(m_unit->currentIndex());
    const auto resultUnit = static_cast<reference::Unit>(m_resultUnit->currentIndex());
    const double mm = reference::modelLength(m_size->value(), unit, m_scale->value());
    const double answer = reference::fromMm(mm, resultUnit);
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);  // pastes cleanly into a size box
    const QString number = locale.toString(answer, 'f', resultUnit == reference::Unit::Millimetres ? 2 : 3);
    m_resultText = number;
    m_result->setText(tr("= %1 %2 on the model").arg(number, m_resultUnit->currentText()));
}

}  // namespace forge::app
