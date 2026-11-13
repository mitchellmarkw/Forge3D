// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The Follow tool's settings: groove or ridge, the cross-section and its size, and the
// buttons to draw the path (and an own cross-section).
class FollowPanel : public QWidget {
    Q_OBJECT

public:
    explicit FollowPanel(QWidget* parent = nullptr);

    void start(const QString& title, const FollowFinish& finish, bool editing);
    // The settings (not the path or the drawn cross-section).
    bool ridge() const;
    FollowProfile profile() const;
    double width() const;
    double depth() const;
    void setPlaced(bool placed);  // the path's spot is chosen
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void changed();
    void pathRequested();
    void profileRequested();
    void applyRequested();
    void cancelRequested();

private:
    void updateLabels();

    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QComboBox* m_make = nullptr;
    QComboBox* m_profile = nullptr;
    QLabel* m_widthLabel = nullptr;
    QDoubleSpinBox* m_width = nullptr;
    QLabel* m_depthLabel = nullptr;
    QDoubleSpinBox* m_depth = nullptr;
    QPushButton* m_path = nullptr;
    QPushButton* m_drawProfile = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
