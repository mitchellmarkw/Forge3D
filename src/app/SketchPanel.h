// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// The sketch's side panel: how held it is, and what Finish makes of it (a pad pushed
// out, or a pocket cut in, and how far).
class SketchPanel : public QWidget {
    Q_OBJECT

public:
    explicit SketchPanel(QWidget* parent = nullptr);

    // pocketAllowed: sketching on a shape's face (or changing a pocket), so it can cut in.
    void start(const QString& title, bool pocketAllowed, bool pocket, double length, bool otherWay, bool bothWays,
               bool throughAll, bool editing);
    bool pocket() const;
    double length() const;
    bool otherWay() const;
    bool bothWays() const;
    bool throughAll() const;
    void setStatus(const QString& text, bool problem, bool held);
    void setMessage(const QString& text, bool problem);

signals:
    void finishRequested();
    void cancelRequested();

private:
    void updateLabels();

    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_message = nullptr;
    QLabel* m_makeLabel = nullptr;
    QComboBox* m_make = nullptr;
    QLabel* m_lengthLabel = nullptr;
    QDoubleSpinBox* m_length = nullptr;
    QCheckBox* m_otherWay = nullptr;
    QCheckBox* m_bothWays = nullptr;
    QCheckBox* m_throughAll = nullptr;
    QPushButton* m_finish = nullptr;
};

}  // namespace forge::app
