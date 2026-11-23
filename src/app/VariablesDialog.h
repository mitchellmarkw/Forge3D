// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Variables.h"

#include <QDialog>

#include <map>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace forge {
class Document;
}

namespace forge::app {

// The design variables table (slice 77): name, value or formula, what it works out to,
// how many shapes use it, and a note.
class VariablesDialog : public QDialog {
    Q_OBJECT
public:
    VariablesDialog(const Document& doc, QWidget* parent = nullptr);

    std::vector<Variable> table() const;
    // Old name -> new, for the variables renamed here.
    std::map<std::string, std::string> renames() const;

signals:
    // Apply (the window stays open); the main window answers with applied(problem).
    void applyRequested();

public slots:
    // After an Apply: empty if it worked, else why not.
    void applied(const QString& problem);

private:
    std::vector<Variable> rows() const;  // as typed
    void addRow(const Variable& v, const QString& original);
    void refresh();
    void removeSelected();
    int usedBy(const std::string& name) const;

    const Document& m_doc;
    QTableWidget* m_table = nullptr;
    QLabel* m_note = nullptr;
    QPushButton* m_ok = nullptr;
    QPushButton* m_apply = nullptr;
    bool m_refreshing = false;
    bool m_closeAfter = false;
};

// The formula for one field: what it equals, with the variables to choose from.
class FormulaDialog : public QDialog {
    Q_OBJECT
public:
    FormulaDialog(const QString& field, const QString& formula, const std::vector<Variable>& table,
                  QWidget* parent = nullptr);
    QString formula() const;

private:
    void refresh();

    variables::Values m_values;
    QLineEdit* m_formula = nullptr;
    QComboBox* m_pick = nullptr;
    QLabel* m_result = nullptr;
    QPushButton* m_ok = nullptr;
};

}  // namespace forge::app
