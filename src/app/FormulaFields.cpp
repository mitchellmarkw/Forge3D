// SPDX-License-Identifier: GPL-3.0-or-later
#include "FormulaFields.h"

#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

namespace forge::app {

FormulaFields::FormulaFields(QObject* parent) : QObject(parent) {}

void FormulaFields::add(QDoubleSpinBox* spin, const QString& key, const QString& label)
{
    m_fields.push_back({spin, key, label, spin->toolTip()});
    spin->installEventFilter(this);
    if (auto* edit = spin->findChild<QLineEdit*>())
        edit->installEventFilter(this);
    // A number typed (or stepped) in by hand: the box no longer follows the variables.
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this, spin, key] {
        if (m_setting || spin != m_touched || !m_formulas.count(key.toStdString()))
            return;
        clearFormula(key);
    });
    show();
}

const FormulaFields::Field* FormulaFields::fieldOf(QObject* watched) const
{
    for (const Field& f : m_fields)
        if (watched == f.spin || (watched && watched->parent() == f.spin))
            return &f;
    return nullptr;
}

void FormulaFields::setFormulas(const std::map<std::string, std::string>& formulas)
{
    m_formulas.clear();
    for (const auto& [key, formula] : formulas)
        for (const Field& f : m_fields)
            if (f.key.toStdString() == key) {
                m_formulas[key] = formula;
                break;
            }
    m_touched = nullptr;
    show();
}

void FormulaFields::setFormula(const QString& key, const QString& formula, double value)
{
    m_setting = true;
    for (const Field& f : m_fields)
        if (f.key == key)
            f.spin->setValue(value);
    m_setting = false;
    m_formulas[key.toStdString()] = formula.toStdString();
    show();
}

void FormulaFields::clearFormula(const QString& key)
{
    if (m_formulas.erase(key.toStdString()))
        show();
}

void FormulaFields::show()
{
    for (const Field& f : m_fields) {
        const auto found = m_formulas.find(f.key.toStdString());
        if (found != m_formulas.end()) {
            f.spin->setToolTip(tr("= %1 (from the design variables). Press = to change it; type a number to stop "
                                  "following it.\n\n%2")
                                   .arg(QString::fromStdString(found->second), f.tip));
            f.spin->setStyleSheet(QStringLiteral("QDoubleSpinBox { background-color: rgba(52, 120, 246, 0.12); }"));
        } else {
            f.spin->setToolTip(f.tip + tr("\n\nPress = to make it follow the design variables."));
            f.spin->setStyleSheet({});
        }
    }
}

bool FormulaFields::eventFilter(QObject* watched, QEvent* event)
{
    const Field* field = nullptr;
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::ContextMenu:
    case QEvent::Wheel:
    case QEvent::MouseButtonPress:
        field = fieldOf(watched);
        break;
    default:
        return QObject::eventFilter(watched, event);
    }
    if (!field)
        return QObject::eventFilter(watched, event);
    const std::string key = field->key.toStdString();
    const QString current =
        m_formulas.count(key) ? QString::fromStdString(m_formulas.at(key)) : QString();
    if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent*>(event)->text() == QLatin1String("=")) {
            emit formulaRequested(field->key, field->label, current);
            return true;
        }
        m_touched = field->spin;
        return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::ContextMenu) {
        m_touched = field->spin;  // the wheel or the arrows
        return QObject::eventFilter(watched, event);
    }
    QMenu menu;
    QAction* use = menu.addAction(current.isEmpty() ? tr("Use a Variable... (=)") : tr("Change the Formula... (=)"));
    QAction* stop = current.isEmpty() ? nullptr : menu.addAction(tr("Stop Following the Variables"));
    QAction* chosen = menu.exec(static_cast<QContextMenuEvent*>(event)->globalPos());
    if (chosen == use)
        emit formulaRequested(field->key, field->label, current);
    else if (chosen && chosen == stop)
        clearFormula(field->key);
    return true;
}

}  // namespace forge::app
