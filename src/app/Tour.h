// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

class QLabel;
class QPushButton;

namespace forge::app {

// A guided tour: a note pointing at one part of the window at a time (the rest
// dimmed), with Back / Next / Skip. The window stays usable while it runs.
class Tour : public QWidget {
    Q_OBJECT

public:
    struct Step {
        QString title;
        QString text;
        // What to point at (nothing: the note sits in the middle). Called when the step
        // shows, after `before`.
        std::function<QWidget*()> target;
        std::function<void()> before;  // e.g. switch to the right ribbon tab
    };

    Tour(QWidget* window, std::vector<Step> steps);
    void start();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void show(int index);
    void place();
    void finish();
    QRect targetRect() const;

    QWidget* m_window;
    std::vector<Step> m_steps;
    int m_index = 0;
    QPointer<QWidget> m_target;
    QWidget* m_note = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_text = nullptr;
    QLabel* m_count = nullptr;
    QPushButton* m_back = nullptr;
    QPushButton* m_next = nullptr;
    QPushButton* m_skip = nullptr;
};

}  // namespace forge::app
