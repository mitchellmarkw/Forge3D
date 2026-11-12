// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Sketch.h"

#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <optional>
#include <vector>

namespace forge::app {

// The 2D sketch editor: draws the sketch flat (as if looking straight at its plane),
// lets you draw lines, rectangles, circles, arcs and slots, pick things, drag them, and
// add rules and dimensions to the picked things. Every change is solved at once.
//
// Mouse: left-click draws / picks (Ctrl+click adds to the pick), left-drag moves a
// point or curve, right-click ends a chain of lines, middle- or right-drag pans, wheel
// zooms. Double-click a dimension to type its value.
class SketchView : public QWidget {
    Q_OBJECT

public:
    enum class Tool { Select, Line, Rectangle, Circle, Arc, Slot };

    explicit SketchView(QWidget* parent = nullptr);

    // Starts editing. reference: outlines to show and snap to (the face sketched on),
    // in sketch coordinates.
    void start(const sketch::Sketch& sketch, std::vector<std::vector<QPointF>> reference);
    const sketch::Sketch& sketch() const { return m_sketch; }
    const sketch::Solved& solved() const { return m_solved; }

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }

    // Rules on what's picked. Horizontal / Vertical: lines, or two points. Parallel,
    // Perpendicular, Equal, Tangent: two curves. Coincident: two points (joined into
    // one) or a point and a curve (on it). Symmetric: two points and a line. Fixed:
    // points.
    void addRule(sketch::Rule rule);
    // A dimension on what's picked, then asks for its value. how: 0 = length / distance
    // / radius / angle as suits the pick, 1 = across only, 2 = up only.
    void addDimension(int how);
    void toggleConstruction();
    void deleteSelection();
    void undo();
    void redo();
    void fit();

signals:
    void changed();                                     // the sketch changed (and was solved)
    void message(const QString& text, bool problem);   // for the status line
    void toolChanged(SketchView::Tool tool);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Pick {
        enum Kind { Point, Geo, Rule } kind = Point;
        int index = -1;
        bool operator==(const Pick&) const = default;
    };
    // Where the cursor is, after snapping: an existing point, a reference point, the
    // grid, or just the cursor.
    struct Snap {
        sketch::Point at;
        int point = -1;  // an existing sketch point it snapped to
    };

    QPointF toScreen(const sketch::Point& p) const;
    sketch::Point toSketch(const QPointF& p) const;
    Snap snapAt(const QPointF& pixel) const;
    std::optional<Pick> pickAt(const QPointF& pixel) const;
    double gridStep() const;

    // Changes: snapshot for undo first, then solve after.
    void remember();
    void solveNow();
    // Adds a point at `snap` (or reuses the snapped one).
    int pointFor(const Snap& snap);
    int addLine(int a, int b);
    void finishChain();
    void clickTool(const Snap& snap);
    void editDimension(int rule);

    std::vector<int> picked(Pick::Kind kind) const;
    QString describeRule(const sketch::Constraint& c) const;
    QPointF ruleAnchor(const sketch::Constraint& c, int index) const;

    sketch::Sketch m_sketch;
    sketch::Solved m_solved;
    std::vector<std::vector<QPointF>> m_reference;
    std::vector<sketch::Sketch> m_undo, m_redo;

    // View: millimetres to pixels, and the sketch point at the middle of the widget.
    double m_scale = 10.0;
    QPointF m_centre;

    Tool m_tool = Tool::Select;
    std::vector<Snap> m_clicks;  // points clicked so far with a drawing tool
    int m_chainStart = -1;       // the first point of a chain of lines
    Snap m_cursor;
    QPointF m_cursorPixel;

    std::vector<Pick> m_picks;
    std::optional<Pick> m_hover;
    std::vector<QRectF> m_ruleRects;  // where each rule's label was drawn (for picking)

    // Dragging
    enum class Drag { None, Pan, Move } m_drag = Drag::None;
    QPointF m_dragFrom;
    sketch::Point m_dragStartSketch;
    std::vector<std::pair<int, sketch::Point>> m_dragPoints;  // points being moved, and where they started
    int m_dragRadius = -1;                                     // a circle whose size is being dragged
    bool m_dragMoved = false;
};

}  // namespace forge::app
