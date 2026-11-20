// SPDX-License-Identifier: GPL-3.0-or-later
#include "RescaleDialog.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace forge::app {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

// A label with its combobox right under it (the label resting on top of the box).
QWidget* labelled(const QString& text, QComboBox* box)
{
    auto* holder = new QWidget;
    auto* column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(box);
    return holder;
}

QDoubleSpinBox* scaleBox(double value)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(1.0, 10000.0);
    box->setDecimals(1);
    box->setSingleStep(1.0);
    box->setPrefix(QStringLiteral("1 : "));
    box->setValue(value);
    box->setToolTip(QObject::tr("Type X for 1:X (12 for 1:12, 18 for 1:18). 1 is full size."));
    return box;
}

QString number(double v)
{
    return QString::number(v, 'f', v == double(qint64(v)) ? 0 : 1);
}

}  // namespace

RescaleDialog::RescaleDialog(const Choice& everything, const Choice& selected, QWidget* parent)
    : QDialog(parent), m_everything(everything), m_selected(selected)
{
    setWindowTitle(tr("Rescale Design"));
    const QSettings prefs = preferences();
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(tr("Move a design to another scale. Everything grows or shrinks together; joint parts "
                               "and joints can keep their real sizes, so pegs, pins, magnets and sockets still fit."));
    help->setWordWrap(true);
    layout->addWidget(help);

    m_scope = new QComboBox;
    m_scope->addItem(tr("Everything on the plate"));
    m_scope->addItem(tr("The selected shapes"));
    m_scope->setCurrentIndex(selected.shapes > 0 ? 1 : 0);
    m_scope->setEnabled(selected.shapes > 0);
    layout->addWidget(labelled(tr("Rescale:"), m_scope));

    m_by = new QComboBox;
    m_by->addItem(tr("From one scale to another"));
    m_by->addItem(tr("To a new height"));
    m_by->addItem(tr("By a percentage"));
    m_by->setCurrentIndex(std::clamp(prefs.value(QStringLiteral("rescale/by"), 0).toInt(), 0, 2));
    layout->addWidget(labelled(tr("How:"), m_by));

    m_pages = new QStackedWidget;
    {
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        m_from = scaleBox(prefs.value(QStringLiteral("rescale/from"), 12.0).toDouble());
        m_to = scaleBox(prefs.value(QStringLiteral("rescale/to"), 18.0).toDouble());
        form->addRow(tr("It's now at:"), m_from);
        form->addRow(tr("Make it:"), m_to);
        m_pages->addWidget(page);
    }
    {
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        m_height = new QDoubleSpinBox;
        m_height->setRange(0.1, 100000.0);
        m_height->setDecimals(1);
        m_height->setSuffix(tr(" mm"));
        m_height->setToolTip(tr("How tall it should be, from the bottom to the top"));
        form->addRow(tr("New height:"), m_height);
        m_pages->addWidget(page);
    }
    {
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        m_percent = new QDoubleSpinBox;
        m_percent->setRange(1.0, 10000.0);
        m_percent->setDecimals(1);
        m_percent->setSuffix(tr(" %"));
        m_percent->setValue(prefs.value(QStringLiteral("rescale/percent"), 100.0).toDouble());
        form->addRow(tr("New size:"), m_percent);
        m_pages->addWidget(page);
    }
    layout->addWidget(m_pages);

    m_keepJoints = new QCheckBox;
    m_keepJoints->setChecked(prefs.value(QStringLiteral("rescale/keepJoints"), true).toBool());
    m_keepJoints->setToolTip(tr("Balls, pegs, hinge pieces and pins, split pegs and stand-in magnets (joint parts) "
                                "move with the design but keep their size; so do the sockets, magnet pockets, "
                                "hinges and threads cut into the shapes"));
    layout->addWidget(m_keepJoints);
    m_keepWalls = new QCheckBox(tr("Keep wall thickness (hollowed shapes)"));
    m_keepWalls->setChecked(prefs.value(QStringLiteral("rescale/keepWalls"), false).toBool());
    m_keepWalls->setToolTip(tr("Shapes hollowed with Shell keep their walls as thick as they are now"));
    layout->addWidget(m_keepWalls);

    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet(QStringLiteral("QLabel { background: palette(base); border: 1px solid palette(mid); "
                                            "padding: 6px; }"));
    layout->addWidget(m_summary);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(tr("Rescale"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        remember();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_height->setValue(choice().height);
    connect(m_scope, &QComboBox::currentIndexChanged, this, [this] {
        m_height->setValue(choice().height);
        update();
    });
    connect(m_by, &QComboBox::currentIndexChanged, this, &RescaleDialog::update);
    for (QDoubleSpinBox* box : {m_from, m_to, m_height, m_percent})
        connect(box, &QDoubleSpinBox::valueChanged, this, &RescaleDialog::update);
    connect(m_keepJoints, &QCheckBox::toggled, this, &RescaleDialog::update);
    update();
}

const RescaleDialog::Choice& RescaleDialog::choice() const
{
    return selectedOnly() ? m_selected : m_everything;
}

bool RescaleDialog::selectedOnly() const
{
    return m_scope->currentIndex() == 1;
}

double RescaleDialog::factor() const
{
    switch (m_by->currentIndex()) {
    case 0:
        return m_from->value() / m_to->value();
    case 1:
        return choice().height > 0.0 ? m_height->value() / choice().height : 1.0;
    default:
        return m_percent->value() / 100.0;
    }
}

bool RescaleDialog::keepJoints() const
{
    return m_keepJoints->isChecked();
}

bool RescaleDialog::keepWalls() const
{
    return m_keepWalls->isChecked();
}

QString RescaleDialog::describe() const
{
    switch (m_by->currentIndex()) {
    case 0:
        return tr("1:%1 to 1:%2").arg(number(m_from->value()), number(m_to->value()));
    case 1:
        return tr("to %1 mm tall").arg(number(m_height->value()));
    default:
        return tr("to %1 %").arg(number(m_percent->value()));
    }
}

void RescaleDialog::update()
{
    m_pages->setCurrentIndex(m_by->currentIndex());
    const Choice& c = choice();
    m_keepJoints->setText(c.joints == 0   ? tr("Keep joint sizes (none found)")
                          : c.joints == 1 ? tr("Keep joint sizes (1 joint found)")
                                          : tr("Keep joint sizes (%1 joints found)").arg(c.joints));
    const double f = factor();
    QString text = tr("<b>× %1</b>: %2 mm tall becomes about %3 mm.")
                       .arg(QString::number(f, 'f', 3), QString::number(c.height, 'f', 1),
                            QString::number(c.height * f, 'f', 1));
    if (c.joints > 0 && keepJoints() && f < 1.0)
        text += tr(" Joints keep their size: check they still fit inside the smaller parts.");
    m_summary->setText(text);
    m_ok->setEnabled(c.shapes > 0 && std::abs(f - 1.0) > 1e-9);
}

void RescaleDialog::remember() const
{
    QSettings prefs = preferences();
    prefs.setValue(QStringLiteral("rescale/by"), m_by->currentIndex());
    prefs.setValue(QStringLiteral("rescale/from"), m_from->value());
    prefs.setValue(QStringLiteral("rescale/to"), m_to->value());
    prefs.setValue(QStringLiteral("rescale/percent"), m_percent->value());
    prefs.setValue(QStringLiteral("rescale/keepJoints"), m_keepJoints->isChecked());
    prefs.setValue(QStringLiteral("rescale/keepWalls"), m_keepWalls->isChecked());
}

}  // namespace forge::app
