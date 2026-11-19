// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Split.h"

#include <QWidget>

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

    void start(const QString& title, bool editing);
    double brushSize() const;  // the brush's radius, mm
    void setBrushSize(double radius);
    SplitKeep keep() const;
    void setStrokes(int strokes);
    void setStatus(const QString& text, bool problem);
    void setApplyEnabled(bool on);

signals:
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
    QPushButton* m_undo = nullptr;
    QPushButton* m_clear = nullptr;
    QLabel* m_strokes = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_apply = nullptr;
};

}  // namespace forge::app
