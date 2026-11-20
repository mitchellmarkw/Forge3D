// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

#include <array>
#include <optional>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QStackedWidget;

namespace forge::app {

class WordsEditor;

// Shows the selected body's name, size and position, and turns edits into
// requests. It never changes the document itself: the main window turns each
// request into an undoable command, then calls setBody() with the result.
class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    // nullptr = nothing selected (or several). `context` is a line shown under the
    // name ("Made from: ...", "Part of ..."). `isPart`: the body is inside a combined
    // shape, so "Sit on plate" does not apply.
    void setBody(const Body* body, const QString& context = {}, bool isPart = false);

    // "Uniform scale": changing one size scales the others by the same factor.
    void setUniform(bool on);
    bool uniform() const;
    // "Round" (cylinder, sphere, cone, torus, tube): the X / Y radii change together.
    void setKeepRound(bool on);
    bool keepRound() const;
    // Simplify: how many triangles it started with and has now (0, 0: not known).
    void setSimplifyCounts(std::size_t from, std::size_t now);

signals:
    void renameRequested(ObjectId id, const QString& name);
    void specRequested(ObjectId id, const PrimitiveSpec& spec);
    void placementRequested(ObjectId id, const Placement& placement);
    void sitOnPlateRequested(ObjectId id);
    void uniformChanged(bool on);
    void keepRoundChanged(bool on);
    void separateRequested();
    void editCutRequested();
    void editEmbossRequested();
    void editEdgesRequested();
    // Subtract: tools go right through ("through all").
    void throughAllRequested(ObjectId id, bool on);
    // A joint part: keeps its size when the design is rescaled.
    void hardwareRequested(ObjectId id, bool on);
    // Simplify: the share of triangles to keep (0.01..1).
    void simplifyKeepRequested(ObjectId id, double keep);
    // Smooth: how strongly (1..10).
    void smoothStrengthRequested(ObjectId id, int strength);

private:
    QDoubleSpinBox* makeLength(const QString& tip);
    void commitName();
    void commitSpec(std::size_t field);
    void commitPlacement();
    void commitWords();
    void commitLetterHeight();
    void updateRadiusYEnabled();

    std::optional<Body> m_body;  // copy of what is shown

    QStackedWidget* m_pages = nullptr;
    QLabel* m_emptyHint = nullptr;
    QLineEdit* m_name = nullptr;
    QLabel* m_type = nullptr;
    QLabel* m_context = nullptr;
    QLabel* m_selectionHint = nullptr;
    QWidget* m_sizeGroup = nullptr;
    QWidget* m_sitButton = nullptr;
    QWidget* m_separateButton = nullptr;
    QWidget* m_editCutButton = nullptr;
    QWidget* m_editEmbossButton = nullptr;
    QWidget* m_editEdgesButton = nullptr;
    QCheckBox* m_throughAll = nullptr;
    QCheckBox* m_hardware = nullptr;
    QWidget* m_keepRow = nullptr;
    QWidget* m_keepLabel = nullptr;
    QDoubleSpinBox* m_keep = nullptr;
    QSpinBox* m_keepCount = nullptr;  // the same, as a number of triangles
    QLabel* m_keepCountLabel = nullptr;
    QLabel* m_keepSize = nullptr;     // what that means for an STL file
    std::size_t m_simplifyFrom = 0;
    QLabel* m_strengthLabel = nullptr;
    QSpinBox* m_strength = nullptr;
    QWidget* m_textGroup = nullptr;  // the words of a Text shape
    WordsEditor* m_words = nullptr;
    QDoubleSpinBox* m_letterHeight = nullptr;

    QFormLayout* m_sizeForm = nullptr;
    std::array<QDoubleSpinBox*, 5> m_sizeFields{};  // the shape's lengths (specFields order)
    std::array<QLabel*, 5> m_sizeLabels{};
    QCheckBox* m_round = nullptr;
    QCheckBox* m_uniform = nullptr;

    QDoubleSpinBox* m_x = nullptr;
    QDoubleSpinBox* m_y = nullptr;
    QDoubleSpinBox* m_z = nullptr;
    QDoubleSpinBox* m_rotX = nullptr;
    QDoubleSpinBox* m_rotY = nullptr;
    QDoubleSpinBox* m_rotZ = nullptr;
};

}  // namespace forge::app
