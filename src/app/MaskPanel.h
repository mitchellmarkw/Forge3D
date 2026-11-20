// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Split.h"

#include <QColor>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace forge::app {

// Split by Mask: the brush for painting the mask, which pieces to keep, and the
// strokes painted so far.
class MaskPanel : public QWidget {
    Q_OBJECT

public:
    explicit MaskPanel(QWidget* parent = nullptr);

    // `keyFit`: the step's keys when changing one (negative: none).
    void start(const QString& title, bool editing, double keyFit = -1.0);
    // The alignment keys' fit, or negative for no keys.
    double keyFit() const;
    double brushSize() const;  // the brush's radius, mm
    void setBrushSize(double radius);
    SplitKeep keep() const;
    void setStrokes(int strokes);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);
    // The colours the shape is shown in while painting (kept between runs; sculpting's
    // Mask brush uses them too).
    QColor unmaskedColour() const { return m_unmaskedColour; }
    QColor maskedColour() const { return m_maskedColour; }

signals:
    void coloursChanged();
    void brushSizeChanged(double radius);
    void undoStrokeRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QWidget* m_keepBox = nullptr;
    QComboBox* m_keep = nullptr;
    QCheckBox* m_keys = nullptr;
    QDoubleSpinBox* m_keyFit = nullptr;
    QPushButton* m_undo = nullptr;
    QPushButton* m_clear = nullptr;
    QLabel* m_strokes = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
    QPushButton* m_unmaskedButton = nullptr;
    QPushButton* m_maskedButton = nullptr;
    QColor m_unmaskedColour;
    QColor m_maskedColour;
    void showColours();
    void saveColours();
};

}  // namespace forge::app
