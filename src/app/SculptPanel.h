// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

namespace forge::app {

// The Sculpt tool's settings: the brush, its size and strength, and how
// fine the mesh is. It only reports changes; the main window does the sculpting.
class SculptPanel : public QWidget {
    Q_OBJECT

public:
    enum class Detail { Low, Medium, High };

    explicit SculptPanel(QWidget* parent = nullptr);

    void start(const QString& title, double suggestedSize);
    // The brush chosen, as a sculpt::Brush number.
    int brush() const;
    // Pattern brush: the pattern chosen and how wide one repeat of it is (mm).
    SculptPattern pattern() const;
    double patternSize() const;
    // Pressed once as a stamp (not repeating), and turned round the surface's facing.
    bool patternOnce() const;
    double patternTurn() const;
    // A picture of the user's to press in: added to the list and chosen.
    void addOwnPattern(const SculptPattern& pattern);
    bool mirrorX() const;
    bool mirrorY() const;
    bool mirrorZ() const;
    double size() const;       // the brush's radius, mm
    double strength() const;   // 0..1
    Detail detail() const;
    void setDetail(int index);
    double remeshSize() const;
    void setRemeshSize(double mm);  // 0 Low, 1 Medium, 2 High (no detailChanged)
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
    void brushChanged();
    void detailChanged();
    void addDetailChanged();
    void undoRequested();
    void clearMaskRequested();
    void remeshRequested();
    void invertMaskRequested();
    // "Your own picture..." was chosen: the main window asks for one (addOwnPattern).
    void ownPatternRequested();
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
    QDoubleSpinBox* m_remeshSize = nullptr;
    QWidget* m_patternBox = nullptr;     // shown for the Pattern brush only
    QComboBox* m_pattern = nullptr;      // built-ins, own pictures, then "Your own picture..."
    QDoubleSpinBox* m_patternSize = nullptr;
    QLabel* m_patternSizeLabel = nullptr;
    QComboBox* m_patternPress = nullptr;  // repeating, or once (a stamp)
    QDoubleSpinBox* m_patternTurn = nullptr;
    std::vector<SculptPattern> m_patterns;  // in the list's order (without the last item)
    int m_lastPattern = 0;
    QPushButton* m_undo = nullptr;
};

}  // namespace forge::app
