// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QWidget>

#include <cstdint>
#include <vector>

class QCheckBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QToolButton;

namespace forge::app {

// Paint: a colour brush for one shape, with a palette (ready colours and the ones used on
// this shape), the brush size, mirroring, Fill All, and the strokes painted so far.
class PaintPanel : public QWidget {
    Q_OBJECT

public:
    explicit PaintPanel(QWidget* parent = nullptr);

    void start(const QString& title, bool editing);
    QColor colour() const { return m_colour; }
    void setColour(const QColor& c);
    double brushSize() const;  // the brush's radius, mm
    void setBrushSize(double radius);
    bool mirror() const;
    // The colours already on the shape (shown under the ready ones, to pick again).
    void setUsed(const std::vector<std::uint32_t>& colours);
    void setStrokes(int strokes);
    void setStatus(const QString& text, bool problem);

signals:
    void brushSizeChanged(double radius);
    void fillRequested();
    void undoStrokeRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QToolButton* swatch(const QColor& c, const QString& name);
    void showColour();

    QColor m_colour;
    QLabel* m_title = nullptr;
    QPushButton* m_current = nullptr;
    QLabel* m_currentName = nullptr;
    QWidget* m_usedBox = nullptr;
    QGridLayout* m_usedGrid = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QCheckBox* m_mirror = nullptr;
    QLabel* m_strokes = nullptr;
    QLabel* m_status = nullptr;
};

}  // namespace forge::app
