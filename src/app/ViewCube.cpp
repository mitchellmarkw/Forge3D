// SPDX-License-Identifier: GPL-3.0-or-later
#include "ViewCube.h"

#include <QCoreApplication>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>

namespace forge::app {

namespace {

struct Face {
    QVector3D normal;
    QVector3D across;  // the text's right
    QVector3D up;      // the text's up
    const char* name;
};

// Front looks at the model from -Y (like FreeCAD), with Z up.
const std::array<Face, 6> kFaces{{
    {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, QT_TRANSLATE_NOOP("ViewCube", "Front")},
    {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, QT_TRANSLATE_NOOP("ViewCube", "Right")},
    {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, QT_TRANSLATE_NOOP("ViewCube", "Back")},
    {{-1, 0, 0}, {0, -1, 0}, {0, 0, 1}, QT_TRANSLATE_NOOP("ViewCube", "Left")},
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, QT_TRANSLATE_NOOP("ViewCube", "Top")},
    {{0, 0, -1}, {1, 0, 0}, {0, -1, 0}, QT_TRANSLATE_NOOP("ViewCube", "Bottom")},
}};

// Where the cube's 3x3 split falls on each face (-1..1).
constexpr float kInner = 0.55f;

struct Projector {
    QMatrix4x4 view;
    QPointF centre;
    float scale;
    QPointF operator()(const QVector3D& p) const
    {
        const QVector3D v = view.mapVector(p);
        return {centre.x() + v.x() * scale, centre.y() - v.y() * scale};
    }
    bool facing(const QVector3D& normal) const { return view.mapVector(normal).z() > 0.05f; }
};

Projector projectorFor(const QMatrix4x4& view, const QRectF& area)
{
    // A cube of side 2 turned any way fits in a circle of radius sqrt(3).
    return {view, area.center(), float(area.width() / 2.0 / std::sqrt(3.0) * 0.98)};
}

}  // namespace

std::vector<ViewCube::Piece> ViewCube::pieces(const QMatrix4x4& view, const QRectF& area)
{
    const Projector project = projectorFor(view, area);
    std::vector<Piece> out;
    const float cuts[4] = {-1.0f, -kInner, kInner, 1.0f};
    for (const Face& f : kFaces) {
        if (!project.facing(f.normal))
            continue;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                const auto corner = [&](float a, float b) { return project(f.normal + f.across * a + f.up * b); };
                Piece piece;
                piece.outline << corner(cuts[i], cuts[j]) << corner(cuts[i + 1], cuts[j]) << corner(cuts[i + 1], cuts[j + 1])
                              << corner(cuts[i], cuts[j + 1]);
                piece.lookFrom = (f.normal + f.across * float(i - 1) + f.up * float(j - 1)).normalized();
                out.push_back(piece);
            }
    }
    return out;
}

std::optional<int> ViewCube::pieceAt(const std::vector<Piece>& pieces, const QPointF& point)
{
    for (std::size_t i = 0; i < pieces.size(); ++i)
        if (pieces[i].outline.containsPoint(point, Qt::OddEvenFill))
            return int(i);
    return std::nullopt;
}

std::pair<float, float> ViewCube::anglesFor(const QVector3D& lookFrom)
{
    const QVector3D d = lookFrom.normalized();
    const float pitch = qRadiansToDegrees(std::asin(std::clamp(d.z(), -1.0f, 1.0f)));
    // Straight down or up: keep Front at the bottom of the screen.
    const float yaw = (std::abs(d.x()) < 1e-4f && std::abs(d.y()) < 1e-4f) ? -90.0f
                                                                            : qRadiansToDegrees(std::atan2(d.y(), d.x()));
    return {yaw, pitch};
}

void ViewCube::paint(QPainter& painter, const QMatrix4x4& view, const QRectF& area, std::optional<int> hovered)
{
    const Projector project = projectorFor(view, area);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QColor faceColour(0xe9, 0xee, 0xf5), edgeColour(0x5a, 0x63, 0x6e), hoverColour(0x2b, 0x78, 0xd0);
    const auto all = pieces(view, area);
    int index = 0;
    for (const Face& f : kFaces) {
        if (!project.facing(f.normal))
            continue;
        // The face, a little darker the more it turns away.
        const float facing = std::clamp(view.mapVector(f.normal).z(), 0.0f, 1.0f);
        QPolygonF outline;
        for (const auto& [a, b] : {std::pair{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}})
            outline << project(f.normal + f.across * a + f.up * b);
        painter.setPen(QPen(edgeColour, 1.2));
        painter.setBrush(faceColour.darker(int(100 + (1.0f - facing) * 22)));
        painter.drawPolygon(outline);
        // The hovered piece of it.
        for (int k = 0; k < 9; ++k, ++index)
            if (hovered && *hovered == index) {
                QColor c = hoverColour;
                c.setAlpha(150);
                painter.setPen(Qt::NoPen);
                painter.setBrush(c);
                painter.drawPolygon(all[std::size_t(index)].outline);
            }
        // Its name, laid on the face: the face's square mapped to its outline.
        const QPointF o = project(f.normal);
        const QPointF ax = project(f.normal + f.across) - o;
        const QPointF ay = project(f.normal - f.up) - o;  // screen y runs down
        const QTransform onFace(ax.x(), ax.y(), ay.x(), ay.y(), o.x(), o.y());
        painter.save();
        painter.setTransform(onFace, true);
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        const QString name = QCoreApplication::translate("ViewCube", f.name);
        const QFontMetricsF metrics(font);
        const double s = 1.5 / std::max(metrics.horizontalAdvance(name), metrics.height() * 2.0);
        painter.scale(s, s);
        painter.setPen(QColor(0x2b, 0x2f, 0x33));
        painter.drawText(QRectF(-1.0 / s, -1.0 / s, 2.0 / s, 2.0 / s), Qt::AlignCenter, name);
        painter.restore();
    }
    painter.restore();
}

}  // namespace forge::app
