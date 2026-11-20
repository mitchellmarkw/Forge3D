// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/PartsList.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace forge::app {

// Tools → Parts List: every part's size, volume, weight and cost for a material, saved
// as a spreadsheet (CSV) or a PDF to print, or copied.
class PartsListDialog : public QDialog {
    Q_OBJECT

public:
    using Lister = std::function<std::vector<parts::Row>(const parts::Settings&)>;
    PartsListDialog(const QString& title, Lister lister, QWidget* parent = nullptr);

private:
    void refresh();
    void remember() const;
    parts::Settings settings() const;
    QString note() const;
    void saveCsv();
    void savePdf();
    void copy();

    QString m_title;
    Lister m_lister;
    std::vector<parts::Row> m_rows;
    bool m_loading = false;
    QComboBox* m_material = nullptr;
    QDoubleSpinBox* m_density = nullptr;
    QDoubleSpinBox* m_fill = nullptr;
    QDoubleSpinBox* m_waste = nullptr;
    QDoubleSpinBox* m_price = nullptr;
    QLineEdit* m_currency = nullptr;
    QCheckBox* m_inside = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
