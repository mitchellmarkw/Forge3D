// SPDX-License-Identifier: GPL-3.0-or-later
#include "PartsListDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPageSize>
#include <QPdfWriter>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

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

QDoubleSpinBox* spin(double low, double high, double step, int decimals, const QString& suffix)
{
    auto* box = new QDoubleSpinBox;
    box->setRange(low, high);
    box->setSingleStep(step);
    box->setDecimals(decimals);
    box->setSuffix(suffix);
    return box;
}

}  // namespace

PartsListDialog::PartsListDialog(const QString& title, Lister lister, QWidget* parent)
    : QDialog(parent), m_title(title), m_lister(std::move(lister))
{
    setWindowTitle(tr("Parts List"));
    resize(820, 560);
    auto* layout = new QVBoxLayout(this);

    auto* top = new QHBoxLayout;
    auto* left = new QVBoxLayout;
    m_material = new QComboBox;
    for (const auto& m : parts::materials())
        m_material->addItem(QString::fromStdString(m.name));
    m_material->addItem(tr("Other"));
    m_material->setToolTip(tr("Fills in the density and how solid it prints; change them if yours differ"));
    left->addWidget(labelled(tr("Material:"), m_material));
    m_inside = new QCheckBox(tr("List the parts inside joined shapes"));
    m_inside->setToolTip(tr("For a kit or a merged shape, list each part in it underneath (the total counts each "
                            "shape once)"));
    left->addWidget(m_inside);
    left->addStretch(1);
    top->addLayout(left, 1);
    auto* form = new QFormLayout;
    m_density = spin(0.2, 20.0, 0.01, 2, tr(" g/cm³"));
    form->addRow(tr("Density:"), m_density);
    m_fill = spin(1.0, 100.0, 5.0, 0, tr(" %"));
    m_fill->setToolTip(tr("How much of each part's inside is printed: 100 % for resin; about 40 % for filament at "
                          "20 % infill (the walls count too)"));
    form->addRow(tr("Fill:"), m_fill);
    m_waste = spin(0.0, 200.0, 5.0, 0, tr(" %"));
    m_waste->setToolTip(tr("Extra for supports, rafts, brims and what's wasted"));
    form->addRow(tr("Extra for supports:"), m_waste);
    auto* priceRow = new QHBoxLayout;
    m_currency = new QLineEdit;
    m_currency->setMaxLength(4);
    m_currency->setFixedWidth(48);
    m_currency->setToolTip(tr("Shown before the costs"));
    m_price = spin(0.0, 100000.0, 1.0, 2, tr(" per kg"));
    priceRow->addWidget(m_currency);
    priceRow->addWidget(m_price, 1);
    form->addRow(tr("Price:"), priceRow);
    top->addLayout(form, 1);
    layout->addLayout(top);

    m_table = new QTableWidget;
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Part"), tr("Size (mm)"), tr("Volume (cm³)"), tr("Weight (g)"), tr("Cost")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    layout->addWidget(m_table, 1);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* copyButton = new QPushButton(tr("Copy"));
    copyButton->setToolTip(tr("Copy the table, to paste into a spreadsheet or a message"));
    auto* csvButton = new QPushButton(tr("Save as Spreadsheet..."));
    auto* pdfButton = new QPushButton(tr("Save as PDF..."));
    pdfButton->setToolTip(tr("A page to print or send"));
    auto* close = new QPushButton(tr("Close"));
    buttons->addWidget(copyButton);
    buttons->addWidget(csvButton);
    buttons->addWidget(pdfButton);
    buttons->addStretch(1);
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(copyButton, &QPushButton::clicked, this, &PartsListDialog::copy);
    connect(csvButton, &QPushButton::clicked, this, &PartsListDialog::saveCsv);
    connect(pdfButton, &QPushButton::clicked, this, &PartsListDialog::savePdf);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    // Remembered settings.
    const QSettings prefs = preferences();
    m_loading = true;
    m_material->setCurrentIndex(
        std::clamp(prefs.value(QStringLiteral("parts/material"), 0).toInt(), 0, m_material->count() - 1));
    m_density->setValue(prefs.value(QStringLiteral("parts/density"), 1.24).toDouble());
    m_fill->setValue(prefs.value(QStringLiteral("parts/fill"), 40.0).toDouble());
    m_waste->setValue(prefs.value(QStringLiteral("parts/waste"), 10.0).toDouble());
    m_price->setValue(prefs.value(QStringLiteral("parts/price"), 20.0).toDouble());
    m_currency->setText(prefs.value(QStringLiteral("parts/currency"), QStringLiteral("$")).toString());
    m_inside->setChecked(prefs.value(QStringLiteral("parts/inside"), true).toBool());
    m_loading = false;

    connect(m_material, &QComboBox::currentIndexChanged, this, [this](int i) {
        const auto& list = parts::materials();
        if (i >= 0 && i < int(list.size())) {
            m_loading = true;
            m_density->setValue(list[std::size_t(i)].density);
            m_fill->setValue(list[std::size_t(i)].fill * 100.0);
            m_loading = false;
        }
        refresh();
    });
    const auto edited = [this] {
        if (m_loading)
            return;
        // Typed numbers that aren't the chosen material's: it's another one.
        const auto& list = parts::materials();
        const int i = m_material->currentIndex();
        if (i >= 0 && i < int(list.size()) &&
            (std::abs(list[std::size_t(i)].density - m_density->value()) > 1e-6 ||
             std::abs(list[std::size_t(i)].fill * 100.0 - m_fill->value()) > 1e-6)) {
            const QSignalBlocker block(m_material);
            m_material->setCurrentIndex(m_material->count() - 1);
        }
        refresh();
    };
    for (QDoubleSpinBox* box : {m_density, m_fill, m_waste, m_price})
        connect(box, &QDoubleSpinBox::valueChanged, this, edited);
    connect(m_currency, &QLineEdit::textChanged, this, &PartsListDialog::refresh);
    connect(m_inside, &QCheckBox::toggled, this, &PartsListDialog::refresh);
    refresh();
}

parts::Settings PartsListDialog::settings() const
{
    parts::Settings s;
    s.density = m_density->value();
    s.fill = m_fill->value() / 100.0;
    s.waste = m_waste->value() / 100.0;
    s.pricePerKg = m_price->value();
    s.inside = m_inside->isChecked();
    return s;
}

QString PartsListDialog::note() const
{
    return tr("%1: %2 g/cm³, %3 % fill, %4 % extra for supports, %5%6 per kg. Weights and costs are estimates.")
        .arg(m_material->currentText())
        .arg(m_density->value(), 0, 'f', 2)
        .arg(m_fill->value(), 0, 'f', 0)
        .arg(m_waste->value(), 0, 'f', 0)
        .arg(m_currency->text())
        .arg(m_price->value(), 0, 'f', 2);
}

void PartsListDialog::refresh()
{
    if (m_loading)
        return;
    remember();
    try {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_rows = m_lister(settings());
        QApplication::restoreOverrideCursor();
    } catch (const std::exception& e) {
        QApplication::restoreOverrideCursor();
        m_rows.clear();
        m_status->setText(QString::fromUtf8(e.what()));
        m_table->setRowCount(0);
        return;
    }
    const QString money = m_currency->text();
    const auto item = [](const QString& text, bool right, bool bold = false) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment((right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
        if (bold) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
        return it;
    };
    m_table->setRowCount(int(m_rows.size()) + 1);
    for (int r = 0; r < int(m_rows.size()); ++r) {
        const parts::Row& row = m_rows[std::size_t(r)];
        const QString name = (row.depth > 0 ? QStringLiteral("      ") : QString()) + QString::fromStdString(row.name);
        m_table->setItem(r, 0, item(name, false));
        m_table->setItem(r, 1, item(QStringLiteral("%1 × %2 × %3")
                                        .arg(row.size[0], 0, 'f', 1)
                                        .arg(row.size[1], 0, 'f', 1)
                                        .arg(row.size[2], 0, 'f', 1),
                                    true));
        m_table->setItem(r, 2, item(QString::number(row.volume / 1000.0, 'f', 2), true));
        m_table->setItem(r, 3, item(QString::number(row.grams, 'f', 1), true));
        m_table->setItem(r, 4, item(money + QString::number(row.cost, 'f', 2), true));
    }
    const parts::Row t = parts::total(m_rows);
    const int last = int(m_rows.size());
    m_table->setItem(last, 0, item(tr("Total"), false, true));
    m_table->setItem(last, 1, item(QString(), true));
    m_table->setItem(last, 2, item(QString::number(t.volume / 1000.0, 'f', 2), true, true));
    m_table->setItem(last, 3, item(QString::number(t.grams, 'f', 1), true, true));
    m_table->setItem(last, 4, item(money + QString::number(t.cost, 'f', 2), true, true));
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_status->setText(note());
}

void PartsListDialog::remember() const
{
    QSettings prefs = preferences();
    prefs.setValue(QStringLiteral("parts/material"), m_material->currentIndex());
    prefs.setValue(QStringLiteral("parts/density"), m_density->value());
    prefs.setValue(QStringLiteral("parts/fill"), m_fill->value());
    prefs.setValue(QStringLiteral("parts/waste"), m_waste->value());
    prefs.setValue(QStringLiteral("parts/price"), m_price->value());
    prefs.setValue(QStringLiteral("parts/currency"), m_currency->text());
    prefs.setValue(QStringLiteral("parts/inside"), m_inside->isChecked());
}

void PartsListDialog::copy()
{
    // Tabs between columns: pastes into a spreadsheet as cells.
    QString text;
    for (int r = -1; r < m_table->rowCount(); ++r) {
        QStringList cells;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            const QTableWidgetItem* it = r < 0 ? m_table->horizontalHeaderItem(c) : m_table->item(r, c);
            cells << (it ? it->text().trimmed() : QString());
        }
        text += cells.join(QLatin1Char('\t')) + QLatin1Char('\n');
    }
    QApplication::clipboard()->setText(text);
    m_status->setText(tr("Copied."));
}

void PartsListDialog::saveCsv()
{
    const QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QLatin1Char('/') +
                          m_title + tr(" parts.csv");
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Save Parts List"), start, tr("Spreadsheet (CSV) (*.csv)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_status->setText(tr("It couldn't be saved there: %1").arg(f.errorString()));
        return;
    }
    // A byte-order mark, so spreadsheets read "×" and "³" right.
    f.write("\xEF\xBB\xBF");
    f.write(QByteArray::fromStdString(parts::csv(m_rows, m_currency->text().toStdString())));
    m_status->setText(tr("Saved %1.").arg(QDir::toNativeSeparators(path)));
}

void PartsListDialog::savePdf()
{
    const QString start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QLatin1Char('/') +
                          m_title + tr(" parts.pdf");
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Parts List"), start, tr("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    QPdfWriter pdf(path);
    pdf.setPageSize(QPageSize(QPageSize::Letter));
    pdf.setResolution(150);
    QTextDocument page;
    page.setHtml(QString::fromStdString(parts::html(m_rows, m_currency->text().toStdString(),
                                                    (m_title + tr(" — Parts List")).toStdString(),
                                                    note().toStdString())));
    page.print(&pdf);
    m_status->setText(tr("Saved %1.").arg(QDir::toNativeSeparators(path)));
}

}  // namespace forge::app
