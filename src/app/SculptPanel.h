// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QCheckBox;
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
    enum class Detail { Low, Medium, High };

    explicit SculptPanel(QWidget* parent = nullptr);

    void start(const QString& title, double suggestedSize);
    // 0 Draw, 1 Smooth, 2 Inflate, 3 Flatten, 4 Pinch, 5 Clay, 6 Grab (sculpt::Brush order)
    int brush() const;
    bool mirrorX() const;
    bool mirrorY() const;
    bool mirrorZ() const;
    double size() const;       // the brush's radius, mm
    double strength() const;   // 0..1
    Detail detail() const;
    void setSize(double radius);
    // Detail where you sculpt: on, and the finest edge (mm).
    bool addDetail() const;
    double finestDetail() const;
    void setFinestDetail(double mm);
    // Detail can only change before the first stroke (it remakes the mesh).
    void setDetailEnabled(bool on);
    void setUndoEnabled(bool on);
    void setStatus(const QString& text, bool problem);

signals:
    void detailChanged();
    void addDetailChanged();
    void undoRequested();
    void doneRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QLabel* m_status = nullptr;
    QComboBox* m_brush = nullptr;
    QLabel* m_brushHint = nullptr;
    QCheckBox* m_mirror[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* m_size = nullptr;
    QSlider* m_strength = nullptr;
    QLabel* m_strengthValue = nullptr;
    QComboBox* m_detail = nullptr;
    QCheckBox* m_addDetail = nullptr;
    QDoubleSpinBox* m_finest = nullptr;
    QPushButton* m_undo = nullptr;
};

}  // namespace forge::app
