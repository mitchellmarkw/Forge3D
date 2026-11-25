// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Lasso.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Lasso Cut (slice 99): draw a loop round part of a shape and cut straight through.
class LassoPanel : public QWidget {
    Q_OBJECT
public:
    explicit LassoPanel(QWidget* parent = nullptr);

    void start(const QString& title);
    lasso::Keep keep() const;
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QComboBox* m_keep = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_clear = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
