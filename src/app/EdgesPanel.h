// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QButtonGroup;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The Edges tool's settings: rounding, fillet, chamfer or bevel, its size (and angle),
// and the edges picked so far. Edges are picked in the view; the main window owns them.
class EdgesPanel : public QWidget {
    Q_OBJECT

public:
    explicit EdgesPanel(QWidget* parent = nullptr);

    void start(const QString& title, bool editing, EdgeFinishKind kind, double size, double angle);
    EdgeFinishKind kind() const;
    double size() const;
    double angle() const;
    void setPickedCount(int count);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void kindChanged();
    void sizeChanged();  // size or angle
    void allRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    void updateLabels();

    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QButtonGroup* m_kinds = nullptr;
    QLabel* m_kindHelp = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QLabel* m_angleLabel = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QLabel* m_picked = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
