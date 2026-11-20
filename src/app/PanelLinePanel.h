// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The panel-line cutter: the groove (V, square or round; width and depth) and the
// lines drawn on the surface so far.
class PanelLinePanel : public QWidget {
    Q_OBJECT

public:
    explicit PanelLinePanel(QWidget* parent = nullptr);

    void start(const QString& title, const PanelLineFinish* editing);
    // The groove (no lines).
    PanelLineFinish settings() const;
    void setCounts(int lines, int points);
    void setStatus(const QString& text, bool problem);

signals:
    void changed();
    void endLineRequested();
    void closeLoopRequested();
    void undoPointRequested();
    void removeLineRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QComboBox* m_profile = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QLabel* m_counts = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
