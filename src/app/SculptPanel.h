// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

namespace forge::app {

// The Sculpt tool's settings (prototype): the brush, its size and strength, and how
// fine the mesh is. It only reports changes; the main window does the sculpting.
class SculptPanel : public QWidget {
    Q_OBJECT

public:
    enum class Detail { Medium, High };

    explicit SculptPanel(QWidget* parent = nullptr);

    void start(const QString& title, double suggestedSize);
    bool smoothBrush() const;  // else Draw
    double size() const;       // the brush's radius, mm
    double strength() const;   // 0..1
    Detail detail() const;
    void setSize(double radius);
    // Detail can only change before the first stroke (it remakes the mesh).
    void setDetailEnabled(bool on);
    void setUndoEnabled(bool on);
    void setStatus(const QString& text, bool problem);

signals:
    void detailChanged();
    void undoRequested();
    void doneRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QComboBox* m_brush = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QSlider* m_strength = nullptr;
    QLabel* m_strengthValue = nullptr;
    QComboBox* m_detail = nullptr;
    QPushButton* m_undo = nullptr;
};

}  // namespace forge::app
