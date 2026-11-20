// SPDX-License-Identifier: GPL-3.0-or-later
#include "KitDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
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

QDoubleSpinBox* millimetres(double low, double high, double step, double value)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setDecimals(1);
    box->setSingleStep(step);
    box->setSuffix(QObject::tr(" mm"));
    box->setValue(value);
    return box;
}

}  // namespace

KitDialog::KitDialog(int selected, int everything, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Kit Layout"));
    const QSettings prefs = preferences();
    const kit::Settings d;
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(tr("Lays small parts out in rows on a frame like a model kit's sprue: each joined to "
                               "the nearest bar by a thin gate, with its number on a tag beside it."));
    help->setWordWrap(true);
    layout->addWidget(help);

    auto* nameForm = new QFormLayout;
    m_name = new QLineEdit(prefs.value(QStringLiteral("kit/name")).toString());
    m_name->setMaxLength(60);
    m_name->setPlaceholderText(tr("e.g. Hero Hands A"));
    m_name->setToolTip(tr("Raised on a tag under the frame, as on a model kit's tree (and the joined kit is named "
                          "after it)"));
    nameForm->addRow(tr("Tree Name:"), m_name);
    layout->addLayout(nameForm);
    m_includeName = new QCheckBox(tr("Include Name"));
    m_includeName->setChecked(prefs.value(QStringLiteral("kit/includeName"), true).toBool());
    m_includeName->setToolTip(tr("Print the tree's name on its tag"));
    layout->addWidget(m_includeName);
    connect(m_includeName, &QCheckBox::toggled, m_name, &QWidget::setEnabled);
    m_name->setEnabled(m_includeName->isChecked());

    m_scope = new QComboBox;
    m_scope->addItem(selected == 1 ? tr("The selected shape") : tr("The %1 selected shapes").arg(selected));
    m_scope->addItem(tr("Everything on the plate (%1 shapes)").arg(everything));
    m_scope->setCurrentIndex(selected > 0 ? 0 : 1);
    m_scope->setEnabled(selected > 0);
    layout->addWidget(labelled(tr("Parts:"), m_scope));

    auto* form = new QFormLayout;
    m_gap = millimetres(1.0, 50.0, 0.5, prefs.value(QStringLiteral("kit/gap"), d.gap).toDouble());
    m_gap->setToolTip(tr("Between a part and the bars, and between parts in a row (more if the numbers need it)"));
    form->addRow(tr("Gap:"), m_gap);
    m_bar = millimetres(0.8, 10.0, 0.5, prefs.value(QStringLiteral("kit/bar"), d.bar).toDouble());
    m_bar->setToolTip(tr("The frame's bars: square, this wide and tall"));
    form->addRow(tr("Frame bars:"), m_bar);
    m_gate = millimetres(0.3, 10.0, 0.1, prefs.value(QStringLiteral("kit/gate"), d.gate).toDouble());
    m_gate->setDecimals(2);
    m_gate->setToolTip(tr("How thick the gates are: thin enough to cut or twist off cleanly"));
    form->addRow(tr("Gates:"), m_gate);
    layout->addLayout(form);

    m_numbers = new QCheckBox(tr("Number the parts"));
    m_numbers->setChecked(prefs.value(QStringLiteral("kit/numbers"), d.numbers).toBool());
    m_numbers->setToolTip(tr("A raised number on a tag by each part, and the part renamed to match (\"1 Hand\")"));
    layout->addWidget(m_numbers);
    auto* numberForm = new QFormLayout;
    m_numberHeight = millimetres(1.5, 10.0, 0.5, prefs.value(QStringLiteral("kit/numberHeight"), d.numberHeight).toDouble());
    numberForm->addRow(tr("Number height:"), m_numberHeight);
    layout->addLayout(numberForm);
    m_join = new QCheckBox(tr("Join the parts and frame into one shape"));
    m_join->setChecked(prefs.value(QStringLiteral("kit/join"), d.join).toBool());
    m_join->setToolTip(tr("One shape to print (the parts stay in its history); otherwise the frame is a shape of "
                          "its own beside them"));
    layout->addWidget(m_join);
    connect(m_numbers, &QCheckBox::toggled, m_numberHeight, &QWidget::setEnabled);
    m_numberHeight->setEnabled(m_numbers->isChecked());

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Make Kit"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        remember();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool KitDialog::selectedOnly() const
{
    return m_scope->currentIndex() == 0;
}

kit::Settings KitDialog::settings() const
{
    kit::Settings s;
    s.gap = m_gap->value();
    s.bar = m_bar->value();
    s.gate = m_gate->value();
    s.numbers = m_numbers->isChecked();
    s.numberHeight = m_numberHeight->value();
    s.join = m_join->isChecked();
    s.name = m_name->text().trimmed().toStdString();
    s.includeName = m_includeName->isChecked();
    return s;
}

void KitDialog::remember() const
{
    QSettings prefs = preferences();
    const kit::Settings s = settings();
    prefs.setValue(QStringLiteral("kit/gap"), s.gap);
    prefs.setValue(QStringLiteral("kit/bar"), s.bar);
    prefs.setValue(QStringLiteral("kit/gate"), s.gate);
    prefs.setValue(QStringLiteral("kit/numbers"), s.numbers);
    prefs.setValue(QStringLiteral("kit/numberHeight"), s.numberHeight);
    prefs.setValue(QStringLiteral("kit/join"), s.join);
    prefs.setValue(QStringLiteral("kit/name"), m_name->text().trimmed());
    prefs.setValue(QStringLiteral("kit/includeName"), s.includeName);
}

}  // namespace forge::app
