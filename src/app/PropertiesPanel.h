// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <QWidget>

#include <optional>

class QDoubleSpinBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QStackedWidget;

namespace forge::app {

// Shows the selected body's name, size and position, and turns edits into
// requests. It never changes the document itself: the main window turns each
// request into an undoable command, then calls setBody() with the result.
class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    // nullptr = nothing selected.
    void setBody(const Body* body);

signals:
    void renameRequested(ObjectId id, const QString& name);
    void specRequested(ObjectId id, const PrimitiveSpec& spec);
    void placementRequested(ObjectId id, const Placement& placement);
    void sitOnPlateRequested(ObjectId id);

private:
    QDoubleSpinBox* makeLength(const QString& tip);
    void commitName();
    void commitSpec();
    void commitPlacement();

    std::optional<Body> m_body;  // copy of what is shown

    QStackedWidget* m_pages = nullptr;
    QLabel* m_emptyHint = nullptr;
    QLineEdit* m_name = nullptr;
    QLabel* m_type = nullptr;

    QStackedWidget* m_sizePages = nullptr;
    QDoubleSpinBox* m_boxWidth = nullptr;
    QDoubleSpinBox* m_boxDepth = nullptr;
    QDoubleSpinBox* m_boxHeight = nullptr;
    QDoubleSpinBox* m_cylRadius = nullptr;
    QDoubleSpinBox* m_cylHeight = nullptr;

    QDoubleSpinBox* m_x = nullptr;
    QDoubleSpinBox* m_y = nullptr;
    QDoubleSpinBox* m_z = nullptr;
    QDoubleSpinBox* m_rotX = nullptr;
    QDoubleSpinBox* m_rotY = nullptr;
    QDoubleSpinBox* m_rotZ = nullptr;
};

}  // namespace forge::app
