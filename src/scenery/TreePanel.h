// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/TreeSpec.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace forge::scenery {

// The tree designer's panel (slice 114): the kind of tree, its real size, its branches and
// foliage, its lean, and a new one of the same (its seed).
class TreePanel : public QWidget {
    Q_OBJECT

public:
    explicit TreePanel(QWidget* parent = nullptr);
    // How tall and wide the printer prints (model mm): a new tree, or another kind, is sized to fit.
    void setPrinter(double height, double width);
    void start(const TreeSpec& spec, bool editing);
    TreeSpec spec() const { return m_spec; }
    void setStatus(const QString& text, bool problem);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void changed();
    void applyRequested();
    void cancelRequested();

private:
    TreeSpec m_spec;
    bool m_loading = false;
    QLabel* m_title = nullptr;
    QComboBox* m_kind = nullptr;
    QDoubleSpinBox* m_height = nullptr;
    QDoubleSpinBox* m_crown = nullptr;
    QDoubleSpinBox* m_trunk = nullptr;
    QSpinBox* m_branches = nullptr;
    QCheckBox* m_leaves = nullptr;
    QLabel* m_foliageLabel = nullptr;
    QSpinBox* m_foliage = nullptr;
    QSpinBox* m_lean = nullptr;
    QSpinBox* m_seed = nullptr;
    QDoubleSpinBox* m_smallest = nullptr;
    QLabel* m_size = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
    void load();
    void readFields();
    void showKind();
    void fitPrinter();
    double m_printHeight = 250.0, m_printWidth = 220.0;
};

}  // namespace forge::scenery
