// SPDX-License-Identifier: GPL-3.0-or-later
#include "VariablesDialog.h"

#include "forge/core/Document.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <set>

namespace forge::app {

namespace {

enum Column { Name, Formula, Result, Used, Note, ColumnCount };
constexpr int kOriginalRole = Qt::UserRole + 1;  // the name it had when the window opened

QString number(double v)
{
    return QString::number(v, 'g', 8);
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

}  // namespace

VariablesDialog::VariablesDialog(const Document& doc, QWidget* parent) : QDialog(parent), m_doc(doc)
{
    setWindowTitle(tr("Design Variables"));
    resize(720, 420);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Named values that sizes and positions can follow. Give a value (3) or a formula of "
                                "others (PegDia + 2 * Fit). In Properties, press = in a size or position (or "
                                "right-click it) to make it follow them; change a value here and every shape that "
                                "uses it changes."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(0, ColumnCount);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Value or formula"), tr("Works out to"), tr("Used by"),
                                        tr("Note")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(Note, QHeaderView::Stretch);
    m_table->setColumnWidth(Name, 130);
    m_table->setColumnWidth(Formula, 180);
    m_table->setColumnWidth(Result, 110);
    m_table->setColumnWidth(Used, 80);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setToolTip(tr("Double-click a cell to change it. Names use letters, digits and _, starting with a "
                           "letter. Formulas may use + - * / ^, brackets, pi, min( ), max( ), abs( ), sqrt( ), "
                           "round( ), floor( ) and ceil( )."));
    layout->addWidget(m_table, 1);

    auto* rowButtons = new QHBoxLayout;
    auto* add = new QPushButton(tr("Add Variable"));
    auto* remove = new QPushButton(tr("Delete"));
    remove->setToolTip(tr("Shapes that used it keep their sizes as they are now, and stop following it"));
    rowButtons->addWidget(add);
    rowButtons->addWidget(remove);
    rowButtons->addStretch();
    layout->addLayout(rowButtons);

    m_note = new QLabel;
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("color: #c0392b;"));
    layout->addWidget(m_note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_apply = buttons->button(QDialogButtonBox::Apply);
    layout->addWidget(buttons);

    for (const Variable& v : doc.variables())
        addRow(v, QString::fromStdString(v.name));

    connect(add, &QPushButton::clicked, this, [this] {
        std::set<std::string> taken;
        for (const Variable& v : rows())
            taken.insert(v.name);
        int n = 1;
        while (taken.count("Value" + std::to_string(n)))
            ++n;
        addRow({"Value" + std::to_string(n), "0", ""}, {});
        m_table->setCurrentCell(m_table->rowCount() - 1, Name);
        m_table->editItem(m_table->item(m_table->rowCount() - 1, Name));
    });
    connect(remove, &QPushButton::clicked, this, &VariablesDialog::removeSelected);
    connect(m_table, &QTableWidget::itemChanged, this, [this] {
        if (!m_refreshing)
            refresh();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        m_closeAfter = true;
        emit applyRequested();
    });
    connect(m_apply, &QPushButton::clicked, this, [this] {
        m_closeAfter = false;
        emit applyRequested();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refresh();
}

void VariablesDialog::addRow(const Variable& v, const QString& original)
{
    m_refreshing = true;
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto* name = new QTableWidgetItem(QString::fromStdString(v.name));
    name->setData(kOriginalRole, original);
    m_table->setItem(row, Name, name);
    m_table->setItem(row, Formula, new QTableWidgetItem(QString::fromStdString(v.formula)));
    for (int c : {int(Result), int(Used)}) {
        auto* item = new QTableWidgetItem;
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, c, item);
    }
    m_table->setItem(row, Note, new QTableWidgetItem(QString::fromStdString(v.note)));
    m_refreshing = false;
    refresh();
}

std::vector<Variable> VariablesDialog::rows() const
{
    std::vector<Variable> out;
    for (int r = 0; r < m_table->rowCount(); ++r)
        out.push_back({m_table->item(r, Name)->text().trimmed().toStdString(),
                       variables::cleaned(m_table->item(r, Formula)->text().toStdString()),
                       m_table->item(r, Note)->text().trimmed().toStdString()});
    return out;
}

std::vector<Variable> VariablesDialog::table() const
{
    std::vector<Variable> t = rows();
    // Renamed here: the others' formulas still name it as it was.
    const auto names = renames();
    for (Variable& v : t)
        for (const auto& [from, to] : names)
            v.formula = variables::renamed(v.formula, from, to);
    return t;
}

std::map<std::string, std::string> VariablesDialog::renames() const
{
    std::map<std::string, std::string> out;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const QString was = m_table->item(r, Name)->data(kOriginalRole).toString();
        const QString now = m_table->item(r, Name)->text().trimmed();
        if (!was.isEmpty() && was != now)
            out[was.toStdString()] = now.toStdString();
    }
    return out;
}

int VariablesDialog::usedBy(const std::string& name) const
{
    // It, and every variable (as saved) worked out from it, however far along.
    std::set<std::string> reach{name};
    for (bool grew = true; grew;) {
        grew = false;
        for (const Variable& v : m_doc.variables())
            if (!reach.count(v.name))
                for (const std::string& used : variables::namesIn(v.formula))
                    if (reach.count(used)) {
                        reach.insert(v.name);
                        grew = true;
                        break;
                    }
    }
    int n = 0;
    for (const Body& b : m_doc.bodies()) {
        bool uses = false;
        for (const auto& [key, f] : b.formulas)
            for (const std::string& used : variables::namesIn(f))
                uses = uses || reach.count(used);
        n += uses ? 1 : 0;
    }
    return n;
}

void VariablesDialog::refresh()
{
    m_refreshing = true;
    const std::vector<Variable> t = table();
    QString problem;
    try {
        variables::checkTable(t);
    } catch (const std::exception& e) {
        problem = QString::fromUtf8(e.what());
    }
    const variables::Values values = variables::compute(t);
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const std::string name = t[std::size_t(r)].name;
        QTableWidgetItem* result = m_table->item(r, Result);
        if (const auto v = values.value.find(name); v != values.value.end()) {
            result->setText(number(v->second));
            result->setForeground(palette().text());
            result->setToolTip({});
        } else {
            const auto p = values.problem.find(name);
            const QString why = p != values.problem.end() ? QString::fromUtf8(p->second.c_str()) : QString();
            result->setText(tr("Problem"));
            result->setForeground(QColor(0xc0, 0x39, 0x2b));
            result->setToolTip(why);
            if (problem.isEmpty())
                problem = tr("%1: %2").arg(QString::fromStdString(name), why);
        }
        const QString was = m_table->item(r, Name)->data(kOriginalRole).toString();
        const int used = was.isEmpty() ? 0 : usedBy(was.toStdString());
        m_table->item(r, Used)->setText(used == 0 ? tr("—") : used == 1 ? tr("1 shape") : tr("%1 shapes").arg(used));
    }
    m_note->setText(problem);
    m_note->setVisible(!problem.isEmpty());
    m_ok->setEnabled(problem.isEmpty());
    m_apply->setEnabled(problem.isEmpty());
    m_refreshing = false;
}

void VariablesDialog::removeSelected()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const QString was = m_table->item(row, Name)->data(kOriginalRole).toString();
    const int used = was.isEmpty() ? 0 : usedBy(was.toStdString());
    if (used > 0 &&
        QMessageBox::question(this, tr("Delete Variable"),
                              tr("%1 is used by %2. They'll keep their sizes as they are now, and stop following "
                                 "it. Delete it?")
                                  .arg(was, used == 1 ? tr("1 shape") : tr("%1 shapes").arg(used))) !=
            QMessageBox::Yes)
        return;
    m_table->removeRow(row);
    refresh();
}

void VariablesDialog::applied(const QString& problem)
{
    if (!problem.isEmpty()) {
        m_note->setText(problem);
        m_note->setVisible(true);
        return;
    }
    // What's applied is now where things start from.
    for (int r = 0; r < m_table->rowCount(); ++r)
        m_table->item(r, Name)->setData(kOriginalRole, m_table->item(r, Name)->text().trimmed());
    refresh();
    if (m_closeAfter)
        accept();
}

// ---- FormulaDialog ----

FormulaDialog::FormulaDialog(const QString& field, const QString& formula, const std::vector<Variable>& table,
                             QWidget* parent)
    : QDialog(parent), m_values(variables::compute(table))
{
    setWindowTitle(tr("Use a Variable"));
    auto* layout = new QVBoxLayout(this);
    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(tr("%1 =").arg(field)));
    m_formula = new QLineEdit(formula);
    m_formula->setPlaceholderText(tr("PegDia + 0.4"));
    m_formula->setMinimumWidth(260);
    m_formula->setToolTip(tr("A variable's name, or a formula: + - * / ^, brackets, pi, min( ), max( ), abs( ), "
                             "sqrt( ), round( ), floor( ), ceil( )"));
    row->addWidget(m_formula, 1);
    layout->addLayout(row);

    m_pick = new QComboBox;
    m_pick->addItem(tr("Choose one to put in..."));
    for (const Variable& v : table) {
        const auto value = m_values.value.find(v.name);
        m_pick->addItem(value != m_values.value.end()
                            ? tr("%1 (%2)").arg(QString::fromStdString(v.name), number(value->second))
                            : QString::fromStdString(v.name),
                        QString::fromStdString(v.name));
    }
    m_pick->setEnabled(!table.empty());
    layout->addWidget(labelled(tr("Variables:"), m_pick));
    if (table.empty()) {
        auto* none = new QLabel(tr("There are no variables yet: make some in Build → Variables."));
        none->setWordWrap(true);
        layout->addWidget(none);
    }

    m_result = new QLabel;
    m_result->setWordWrap(true);
    layout->addWidget(m_result);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_formula, &QLineEdit::textChanged, this, &FormulaDialog::refresh);
    connect(m_pick, &QComboBox::activated, this, [this](int i) {
        if (i <= 0)
            return;
        m_formula->insert(m_pick->itemData(i).toString());
        m_pick->setCurrentIndex(0);
        m_formula->setFocus();
    });
    refresh();
}

QString FormulaDialog::formula() const
{
    return QString::fromStdString(variables::cleaned(m_formula->text().toStdString()));
}

void FormulaDialog::refresh()
{
    try {
        const double v = variables::evaluate(formula().toStdString(), [this](const std::string& n) -> std::optional<double> {
            if (const auto f = m_values.value.find(n); f != m_values.value.end())
                return f->second;
            if (const auto p = m_values.problem.find(n); p != m_values.problem.end())
                throw std::invalid_argument(n + " doesn't work out: " + p->second);
            return std::nullopt;
        });
        m_result->setText(tr("Works out to %1").arg(number(v)));
        m_result->setStyleSheet({});
        m_ok->setEnabled(true);
    } catch (const std::exception& e) {
        m_result->setText(QString::fromUtf8(e.what()));
        m_result->setStyleSheet(QStringLiteral("color: #c0392b;"));
        m_ok->setEnabled(false);
    }
}

}  // namespace forge::app
