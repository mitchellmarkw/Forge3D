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

    // What Finish makes. Pocket and Groove cut into the shape sketched on.
    enum class Make { Pad, Pocket, Revolve, Groove };
    struct Options {
        Make make = Make::Pad;
        double length = 10.0;
        double angle = 360.0;
        bool otherWay = false;
        bool bothWays = false;
        bool throughAll = false;
    };
    // onFace: sketching on a shape's face, so it can cut in. editing: changing one made
    // before (it stays adding, or cutting).
    void start(const QString& title, bool onFace, bool editing, const Options& options);
    // Only placing points (a Hole's spots): no Make section.
    void startPositions(const QString& title, const QString& message);
    Make make() const;
    bool pocket() const { return make() == Make::Pocket || make() == Make::Groove; }
    bool revolve() const { return make() == Make::Revolve || make() == Make::Groove; }
    double length() const;
    double angle() const;
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
    QLabel* m_angleLabel = nullptr;
    QDoubleSpinBox* m_angle = nullptr;
    QLabel* m_axisHelp = nullptr;
    QWidget* m_makeGroup = nullptr;
    QLabel* m_help = nullptr;
    QCheckBox* m_otherWay = nullptr;
    QCheckBox* m_bothWays = nullptr;
    QCheckBox* m_throughAll = nullptr;
    QPushButton* m_finish = nullptr;
};

}  // namespace forge::app
