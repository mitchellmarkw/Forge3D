// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Split.h"
#include "forge/core/SplitJoint.h"

#include <QColor>
#include <QWidget>

#include <cstdint>
#include <vector>

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
    // `cut`, `socketFit`: the step's cut surface when changing one.
    void start(const QString& title, bool editing, double keyFit = -1.0, MaskCut cut = MaskCut::Curved,
               double socketFit = 0.15);
    // The alignment keys' fit, or negative for no keys.
    double keyFit() const;
    MaskCut cut() const;     // the cut's surface
    double socketFit() const;  // ball and socket: the socket's clearance, mm
    // The joint to put in at the cut (kind None: none).
    splitjoint::Options joint() const;
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
    // The colours painted on the shape (Paint): one can be picked as the mask. None: hidden.
    void setPaintColours(const std::vector<std::uint32_t>& colours);

signals:
    void coloursChanged();
    // The mask made from where this painted colour is.
    void paintColourChosen(std::uint32_t colour);
    void brushSizeChanged(double radius);
    void undoStrokeRequested();
    void clearRequested();
    void applyRequested();
    void cancelRequested();

private:
    QLabel* m_title = nullptr;
    QWidget* m_fromPaintBox = nullptr;
    QComboBox* m_fromPaint = nullptr;
    QDoubleSpinBox* m_size = nullptr;
    QWidget* m_keepBox = nullptr;
    QComboBox* m_keep = nullptr;
    void showCut();
    class JointAtCutBox* m_joint = nullptr;
    QComboBox* m_cut = nullptr;
    QDoubleSpinBox* m_socketFit = nullptr;
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
