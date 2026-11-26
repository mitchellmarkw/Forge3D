// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/BrickFigure.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace forge::app {

// Brick Figure (slice 107; a panel in 108): where the head comes off (a disc shown on the
// figure, moved up or down the neck and tipped), how it sits on the body, and the body.
class BrickFigurePanel : public QWidget {
    Q_OBJECT

public:
    explicit BrickFigurePanel(QWidget* parent = nullptr);
    // `figureHeight`: offered as the height the first time.
    void start(const QString& title, double figureHeight);
    brickfig::Settings settings() const;  // the head's mask isn't here (MainWindow has it)
    bool headByMask() const;
    bool showPreview() const;
    // How much of the head is painted (0: none yet).
    void setMaskStrokes(int strokes);
    void setStatus(const QString& text, bool problem);

signals:
    void cutChanged();
    void changed();  // anything that changes the brick figure (its preview follows)
    void paintHeadRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QComboBox* m_headCut = nullptr;
    QWidget* m_cutPlace = nullptr;
    QWidget* m_maskPlace = nullptr;
    QLabel* m_maskState = nullptr;
    QCheckBox* m_preview = nullptr;
    void showHeadCut();
    QDoubleSpinBox* m_raise = nullptr;
    QDoubleSpinBox* m_tilt = nullptr;
    QSpinBox* m_headSize = nullptr;
    QDoubleSpinBox* m_forward = nullptr;
    QComboBox* m_build = nullptr;
    QDoubleSpinBox* m_height = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
    QDoubleSpinBox* m_grip = nullptr;
    QLabel* m_status = nullptr;
    void remember() const;
};

}  // namespace forge::app
