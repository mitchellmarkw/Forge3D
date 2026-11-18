// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/PrintSplit.h"

#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace forge::app {

// Split to Fit, before anything is cut: the planned cuts, each of which can be moved
// (here or by dragging its plane in the view) or leaned, and what's wrong with them.
// It only reports; the main window owns the plan and the document.
class FitPanel : public QWidget {
    Q_OBJECT

public:
    explicit FitPanel(QWidget* parent = nullptr);

    void start(const QString& title, const QString& summary, bool angled, bool layOut);
    // The cuts as they are now, and which one is picked (-1 none).
    void setCuts(const std::vector<FitCut>& cuts, int selected);
    void setSummary(const QString& summary);
    // Empty: the cuts will do.
    void setProblems(const QStringList& problems);
    bool angled() const;
    // Lay the pieces out on plates once they're cut.
    bool layOut() const;

signals:
    void cutSelected(int index);
    void cutEdited(int index, forge::FitCut cut);
    void angledChanged(bool angled);
    void layOutChanged(bool layOut);
    void replanRequested();
    void applyRequested();
    void cancelRequested();

private:
    void showSelected();

    QLabel* m_title = nullptr;
    QLabel* m_summary = nullptr;
    QListWidget* m_list = nullptr;
    QDoubleSpinBox* m_position = nullptr;
    QDoubleSpinBox* m_tilt = nullptr;
    QComboBox* m_tiltAxis = nullptr;
    QLabel* m_tiltAxisLabel = nullptr;
    QCheckBox* m_angled = nullptr;
    QCheckBox* m_layOut = nullptr;
    QLabel* m_problems = nullptr;
    QPushButton* m_apply = nullptr;
    std::vector<FitCut> m_cuts;
    int m_selected = -1;
};

}  // namespace forge::app
