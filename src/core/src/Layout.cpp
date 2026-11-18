// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Layout.h"

#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace forge {

namespace {

constexpr double kMargin = 2.0;  // kept clear round the edge of a plate

Vec3 unitOf(const Vec3& v)
{
    const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return l > 1e-12 ? Vec3{v[0] / l, v[1] / l, v[2] / l} : Vec3{0, 0, -1};
}

// How much of a surface touches the plate when it's turned so `down` faces straight
// down: the area of the triangles lying flat at the lowest level (plate coordinates).
double contactArea(const std::vector<Vec3>& pts, const std::vector<std::uint32_t>& tris, const Vec3& down)
{
    const Vec3 d = unitOf(down);
    double lowest = 1e300;
    for (const Vec3& p : pts)
        lowest = std::min(lowest, -(p[0] * d[0] + p[1] * d[1] + p[2] * d[2]));
    double area = 0.0;
    for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
        const Vec3& a = pts[tris[t]];
        const Vec3& b = pts[tris[t + 1]];
        const Vec3& c = pts[tris[t + 2]];
        bool low = true;
        for (const Vec3* p : {&a, &b, &c})
            low = low && -((*p)[0] * d[0] + (*p)[1] * d[1] + (*p)[2] * d[2]) - lowest < 0.05;
        if (!low)
            continue;
        const Vec3 e1{b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const Vec3 n{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const double twice = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (twice > 0 && (n[0] * d[0] + n[1] * d[1] + n[2] * d[2]) / twice > 0.999)
            area += twice / 2;
    }
    return area;
}

// The box round the shape if it were turned so `down` faces down (in a frame where
// that's -Z): its height and footprint.
std::array<double, 3> sizeFacing(const std::vector<Vec3>& pts, const Vec3& down)
{
    const Vec3 z = unitOf({-down[0], -down[1], -down[2]});
    const Vec3 helper = std::abs(z[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    Vec3 x{helper[1] * z[2] - helper[2] * z[1], helper[2] * z[0] - helper[0] * z[2], helper[0] * z[1] - helper[1] * z[0]};
    x = unitOf(x);
    const Vec3 y{z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (const Vec3& p : pts) {
        const double v[3] = {p[0] * x[0] + p[1] * x[1] + p[2] * x[2], p[0] * y[0] + p[1] * y[1] + p[2] * y[2],
                             p[0] * z[0] + p[1] * z[1] + p[2] * z[2]};
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], v[k]);
            hi[k] = std::max(hi[k], v[k]);
        }
    }
    return {hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]};
}

}  // namespace

double plateCentreX(int index, double plateWidth)
{
    return double(index) * (plateWidth + kPlateGap);
}

int plateHolding(const Bounds& box, const std::array<double, 3>& plate, int most)
{
    if (!box.valid)
        return -1;
    const double cx = (double(box.min[0]) + box.max[0]) / 2;
    const int k = int(std::lround(cx / (plate[0] + kPlateGap)));
    if (k < 0 || k >= most)
        return -1;
    const double left = plateCentreX(k, plate[0]) - plate[0] / 2, right = left + plate[0];
    const double eps = 0.01;
    if (box.min[0] < left - eps || box.max[0] > right + eps || box.min[1] < -plate[1] / 2 - eps ||
        box.max[1] > plate[1] / 2 + eps || box.max[2] > plate[2] + eps)
        return -1;
    return k;
}

Vec3 flattestSide(const Document& doc, ObjectId id, const std::array<double, 3>& plate, const std::vector<Vec3>& favour)
{
    const auto mesh = doc.mesh(id);
    if (!mesh || mesh->empty())
        return {0, 0, -1};
    const Transform world = doc.worldTransform(id);
    std::vector<Vec3> pts(mesh->vertexCount());
    for (std::size_t i = 0; i < pts.size(); ++i)
        pts[i] = world.apply({mesh->positions[i * 3], mesh->positions[i * 3 + 1], mesh->positions[i * 3 + 2]});
    // The large flat areas: triangles grouped by the way they face.
    std::map<std::array<long, 3>, std::pair<double, Vec3>> facing;  // rounded direction -> area, summed direction
    for (std::size_t t = 0; t + 2 < mesh->indices.size(); t += 3) {
        const Vec3& a = pts[mesh->indices[t]];
        const Vec3& b = pts[mesh->indices[t + 1]];
        const Vec3& c = pts[mesh->indices[t + 2]];
        const Vec3 e1{b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const Vec3 n{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const double twice = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (twice <= 1e-12)
            continue;
        const std::array<long, 3> key{std::lround(n[0] / twice * 60), std::lround(n[1] / twice * 60),
                                      std::lround(n[2] / twice * 60)};
        auto& [area, sum] = facing[key];
        area += twice / 2;
        for (int k = 0; k < 3; ++k)
            sum[std::size_t(k)] += n[std::size_t(k)] / 2;
    }
    std::vector<std::pair<double, Vec3>> sides;
    for (const auto& [key, value] : facing)
        sides.push_back({value.first, unitOf(value.second)});
    std::sort(sides.begin(), sides.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    if (sides.size() > 12)
        sides.resize(12);

    const auto fits = [&plate](const std::array<double, 3>& s) {
        const double x = plate[0] - 2 * kMargin, y = plate[1] - 2 * kMargin;
        return ((s[0] <= x && s[1] <= y) || (s[1] <= x && s[0] <= y)) && s[2] <= plate[2];
    };
    // A favoured side counts for three times its area (if a fair part of it touches).
    const auto favoured = [&favour](const Vec3& side) {
        for (const Vec3& f : favour) {
            const Vec3 u = unitOf(f);
            if (std::abs(u[0] * side[0] + u[1] * side[1] + u[2] * side[2]) > 0.996)  // within 5°
                return true;
        }
        return false;
    };
    const Vec3 now{0, 0, -1};
    const double nowArea = contactArea(pts, mesh->indices, now) * (favoured(now) ? 3.0 : 1.0);
    const bool nowFits = fits(sizeFacing(pts, now));
    Vec3 best = now;
    double bestScore = nowArea * 1.5 + (nowFits ? 1e9 : 0.0);  // kept unless clearly beaten
    for (const auto& [area, side] : sides) {
        const double touching = contactArea(pts, mesh->indices, side) * (favoured(side) ? 3.0 : 1.0);
        const double score = touching + (fits(sizeFacing(pts, side)) ? 1e9 : 0.0);
        if (score > bestScore) {
            bestScore = score;
            best = side;
        }
    }
    return best;
}

LayoutResult layOut(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids, const LayoutOptions& options)
{
    for (ObjectId id : ids)
        if (!doc.find(id) || !doc.isTopLevel(id))
            throw std::invalid_argument("Only shapes on the plate can be laid out");
    LayoutResult result;
    result.plateOf.assign(ids.size(), -1);
    if (ids.empty())
        return result;
    const double width = options.plate[0] - 2 * kMargin, depth = options.plate[1] - 2 * kMargin;
    stack.beginMacro(ids.size() == 1 ? "Lay out " + doc.find(ids.front())->name : "Lay out for printing");
    try {
        // Each onto its flattest side, then turned a quarter if that's the only way it
        // fits, or if it then lies longer side along X (rows pack better).
        struct Piece {
            std::size_t index;
            ObjectId id;
            double w, d;
        };
        std::vector<Piece> pieces;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            const ObjectId id = ids[i];
            const Placement before = doc.find(id)->placement;
            Placement next = before;
            if (options.turnOver)
                next = laidOnFace(doc, id, flattestSide(doc, id, options.plate, options.favour));
            else
                next = sitOnPlate(doc, id);
            if (!(next == before))
                stack.execute(std::make_unique<SetPlacementCommand>(id, next, false));
            auto size = doc.worldBounds(id).size();
            const bool fitsAsIs = size[0] <= width && size[1] <= depth;
            const bool fitsTurned = size[1] <= width && size[0] <= depth;
            if ((!fitsAsIs && fitsTurned) || (fitsAsIs && fitsTurned && size[1] > size[0] + 1e-6)) {
                Placement turned = doc.find(id)->placement;
                turned.rotationZ += 90.0;
                stack.execute(std::make_unique<SetPlacementCommand>(id, turned, false));
                stack.execute(std::make_unique<SetPlacementCommand>(id, sitOnPlate(doc, id), false));
                size = doc.worldBounds(id).size();
            }
            if (size[0] > width + 1e-6 || size[1] > depth + 1e-6 || size[2] > options.plate[2] + 1e-6) {
                result.tooBig.push_back(id);
                continue;
            }
            pieces.push_back({i, id, double(size[0]), double(size[1])});
        }
        // Rows, deepest pieces first: across the plate, then a new row behind, then a
        // new plate.
        std::sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) {
            return a.d != b.d ? a.d > b.d : a.w > b.w;
        });
        int plate = 0;
        double x = 0.0, y = 0.0, rowDepth = 0.0;
        for (const Piece& p : pieces) {
            if (x > 0.0 && x + p.w > width + 1e-6) {  // the row is full
                x = 0.0;
                y += rowDepth + options.gap;
                rowDepth = 0.0;
            }
            if (y > 0.0 && y + p.d > depth + 1e-6) {  // the plate is full
                ++plate;
                x = y = rowDepth = 0.0;
            }
            // Its front-left corner at (x, y) from the plate's front-left corner.
            const Bounds b = doc.worldBounds(p.id);
            const double left = plateCentreX(plate, options.plate[0]) - width / 2 + x;
            const double front = -depth / 2 + y;
            Placement moved = doc.find(p.id)->placement;
            moved.x += left - b.min[0];
            moved.y += front - b.min[1];
            stack.execute(std::make_unique<SetPlacementCommand>(p.id, moved, false));
            result.plateOf[p.index] = plate;
            result.plates = std::max(result.plates, plate + 1);
            x += p.w + options.gap;
            rowDepth = std::max(rowDepth, p.d);
        }
    } catch (...) {
        stack.abortMacro();  // undo what was done of it
        throw;
    }
    stack.endMacro();
    return result;
}

}  // namespace forge
