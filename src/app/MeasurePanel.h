// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QLabel;

namespace forge::app {

// The Measure tool: what to click, and what was measured.
class MeasurePanel : public QWidget {
    Q_OBJECT

public:
    explicit MeasurePanel(QWidget* parent = nullptr);

    // What was measured (rich text), or the hint to click when nothing is picked.
    void setResult(const QString& html);
    // What is under the mouse now (plain, one line), or nothing.
    void setHover(const QString& text);

signals:
    void clearRequested();
    void doneRequested();

private:
    QLabel* m_result = nullptr;
    QLabel* m_hover = nullptr;
};

}  // namespace forge::app
