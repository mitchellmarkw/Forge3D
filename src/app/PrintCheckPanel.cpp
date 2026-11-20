// SPDX-License-Identifier: GPL-3.0-or-later
#include "PrintCheckPanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace forge::app {

namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

// A small square of colour and what it means.
QWidget* swatch(const QColor& colour, const QString& text)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* square = new QLabel;
    square->setFixedSize(16, 16);
    square->setStyleSheet(QStringLiteral("background: %1; border: 1px solid palette(mid);").arg(colour.name()));
    layout->addWidget(square);
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    layout->addWidget(label, 1);
    return row;
}

}  // namespace

PrintCheckPanel::PrintCheckPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Print Check"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    layout->addWidget(title);
    auto* hint = new QLabel(tr("Shows where the parts on the plate may not print well, the way they sit now. "
                               "Nothing is changed. To try a part another way up, click Done, turn it, and check again."));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto prefs = preferences();
    m_thin = new QCheckBox(tr("Thin walls"));
    m_thin->setChecked(prefs.value(QStringLiteral("printCheck/thin"), true).toBool());
    m_thin->setToolTip(tr("Show places thinner than this: they may break, bend or not print at all (fingers, "
                          "cape edges, weapon tips)"));
    m_minWall = new QDoubleSpinBox;
    m_minWall->setRange(0.2, printcheck::kMaxWall);
    m_minWall->setDecimals(2);
    m_minWall->setSingleStep(0.1);
    m_minWall->setSuffix(tr(" mm"));
    m_minWall->setKeyboardTracking(false);
    m_minWall->setValue(prefs.value(QStringLiteral("printCheck/minWall"), 0.8).toDouble());
    m_minWall->setToolTip(tr("About 0.8 mm for a filament printer, 0.5 mm for resin; more for parts that get "
                             "handled a lot"));
    m_overhangs = new QCheckBox(tr("Overhangs"));
    m_overhangs->setChecked(prefs.value(QStringLiteral("printCheck/overhangs"), true).toBool());
    m_overhangs->setToolTip(tr("Show faces that hang out over the air: they'll need supports (faces resting on "
                               "the plate don't count)"));
    m_angle = new QDoubleSpinBox;
    m_angle->setRange(10.0, 80.0);
    m_angle->setDecimals(0);
    m_angle->setSingleStep(5.0);
    m_angle->setSuffix(tr("°"));
    m_angle->setKeyboardTracking(false);
    m_angle->setValue(prefs.value(QStringLiteral("printCheck/angle"), 45.0).toDouble());
    m_angle->setToolTip(tr("How far a face can lean out from upright before it needs supports: 45° suits most "
                           "printers"));

    auto* form = new QFormLayout;
    form->addRow(m_thin);
    form->addRow(tr("Thinner than:"), m_minWall);
    form->addRow(m_overhangs);
    form->addRow(tr("Leaning out past:"), m_angle);
    layout->addLayout(form);

    layout->addWidget(swatch(thinColour(), tr("Too thin")));
    layout->addWidget(swatch(overhangColour(), tr("Needs supports")));
    layout->addWidget(swatch(okColour(), tr("Fine")));

    m_result = new QLabel;
    m_result->setWordWrap(true);
    m_result->setTextFormat(Qt::RichText);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->setMinimumHeight(90);
    m_result->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_result->setStyleSheet(QStringLiteral("QLabel { background: palette(base); border: 1px solid palette(mid); "
                                           "padding: 6px; }"));
    layout->addWidget(m_result);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* done = new QPushButton(tr("Done"));
    done->setDefault(true);
    done->setToolTip(tr("Close Print Check (Esc)"));
    buttons->addStretch(1);
    buttons->addWidget(done);
    layout->addLayout(buttons);
    layout->addStretch(1);

    const auto changed = [this] {
        m_minWall->setEnabled(m_thin->isChecked());
        m_angle->setEnabled(m_overhangs->isChecked());
        remember();
        emit settingsChanged();
    };
    connect(m_thin, &QCheckBox::toggled, this, changed);
    connect(m_overhangs, &QCheckBox::toggled, this, changed);
    connect(m_minWall, &QDoubleSpinBox::valueChanged, this, changed);
    connect(m_angle, &QDoubleSpinBox::valueChanged, this, changed);
    connect(done, &QPushButton::clicked, this, &PrintCheckPanel::doneRequested);
    m_minWall->setEnabled(m_thin->isChecked());
    m_angle->setEnabled(m_overhangs->isChecked());
}

printcheck::Settings PrintCheckPanel::settings() const
{
    printcheck::Settings s;
    s.thin = m_thin->isChecked();
    s.minWall = m_minWall->value();
    s.overhangs = m_overhangs->isChecked();
    s.angle = m_angle->value();
    return s;
}

void PrintCheckPanel::remember()
{
    auto prefs = preferences();
    prefs.setValue(QStringLiteral("printCheck/thin"), m_thin->isChecked());
    prefs.setValue(QStringLiteral("printCheck/minWall"), m_minWall->value());
    prefs.setValue(QStringLiteral("printCheck/overhangs"), m_overhangs->isChecked());
    prefs.setValue(QStringLiteral("printCheck/angle"), m_angle->value());
}

void PrintCheckPanel::setResult(const QString& html)
{
    m_result->setText(html);
}

void PrintCheckPanel::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
