// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QVector3D>

#include <optional>
#include <vector>

class QPainter;

namespace forge::app {

// The view cube in the corner of the 3D view (like FreeCAD's): it turns with the view and
// says which side faces you. Each face is split 3 x 3: the middle looks straight at that
// side, an edge piece from between two sides, a corner piece from between three.
class ViewCube {
public:
    struct Piece {
        QPolygonF outline;   // on screen
        QVector3D lookFrom;  // unit: where the eye goes, from the middle of the model
    };

    // The pieces facing the viewer, for this view (its rotation is all that counts),
    // drawn in `area` (a square).
    static std::vector<Piece> pieces(const QMatrix4x4& view, const QRectF& area);
    // The piece under a point, if any.
    static std::optional<int> pieceAt(const std::vector<Piece>& pieces, const QPointF& point);
    static void paint(QPainter& painter, const QMatrix4x4& view, const QRectF& area, std::optional<int> hovered);
    // The camera angles (yaw, pitch, in degrees) that look from `lookFrom`.
    static std::pair<float, float> anglesFor(const QVector3D& lookFrom);
};

}  // namespace forge::app
