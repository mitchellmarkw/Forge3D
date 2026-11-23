// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <map>
#include <string>
#include <vector>

class QDoubleSpinBox;

namespace forge::app {

// Boxes in a tool's panel (Quick Socket, Quick Hinge, Hand Ports) that can follow the
// design variables, as sizes in Properties can (slice 81): press = in one, or right-click
// it, to give it a formula; it turns blue; typing a number in it makes it a plain size
// again. Several boxes may share one field (a socket's ball size and peg size).
class FormulaFields : public QObject {
    Q_OBJECT
public:
    explicit FormulaFields(QObject* parent = nullptr);

    void add(QDoubleSpinBox* spin, const QString& key, const QString& label);

    // The formulas to show (the boxes' values already set): field key -> formula.
    void setFormulas(const std::map<std::string, std::string>& formulas);
    const std::map<std::string, std::string>& formulas() const { return m_formulas; }
    // A formula for `key`, its value put in every box of that field.
    void setFormula(const QString& key, const QString& formula, double value);
    void clearFormula(const QString& key);

signals:
    // = pressed (or the menu): the main window asks for the formula.
    void formulaRequested(const QString& key, const QString& label, const QString& current);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Field {
        QDoubleSpinBox* spin;
        QString key, label, tip;
    };
    const Field* fieldOf(QObject* watched) const;
    void show();

    std::vector<Field> m_fields;
    std::map<std::string, std::string> m_formulas;
    QDoubleSpinBox* m_touched = nullptr;  // the box the user is typing in
    bool m_setting = false;
};

}  // namespace forge::app
