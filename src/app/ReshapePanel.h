// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Reshape (slice 102): a box of handles round a shape; drag a handle and the shape bends.
class ReshapePanel : public QWidget {
    Q_OBJECT
public:
    explicit ReshapePanel(QWidget* parent = nullptr);

    void start(const QString& title, int handles);
    int handles() const;  // along each side: 2, 3 or 4
    bool mirror() const;  // left and right move together
    void setStatus(const QString& text, bool problem);

signals:
    void handlesChanged(int handles);
    void resetRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QComboBox* m_handles = nullptr;
    QCheckBox* m_mirror = nullptr;
    QLabel* m_status = nullptr;
    bool m_loading = false;
};

}  // namespace forge::app
