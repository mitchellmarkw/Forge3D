// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/BrickFigure.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

namespace forge::app {

// Brick Figure (slice 107): the settings for a figure's head on a new body of blocks.
class BrickFigureDialog : public QDialog {
    Q_OBJECT

public:
    // `figureHeight`: the figure's own, offered as the height the first time.
    explicit BrickFigureDialog(double figureHeight, QWidget* parent = nullptr);
    brickfig::Settings settings() const;

private:
    QDoubleSpinBox* m_height = nullptr;
    QSpinBox* m_headSize = nullptr;
    QComboBox* m_build = nullptr;
    QDoubleSpinBox* m_fit = nullptr;
    QDoubleSpinBox* m_grip = nullptr;
};

}  // namespace forge::app
