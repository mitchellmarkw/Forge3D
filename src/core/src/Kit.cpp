// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Kit.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Tolerance.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace forge::kit {

namespace {

// An upright box from (x0, y0, z0) to (x1, y1, z1).
MeshSolid block(double x0, double y0, double z0, double x1, double y1, double z1)
{
    std::vector<float> pos;
    for (int k = 0; k < 8; ++k) {
        pos.push_back(float((k & 1) ? x1 : x0));
        pos.push_back(float((k & 2) ? y1 : y0));
        pos.push_back(float((k & 4) ? z1 : z0));
    }
    const std::vector<std::uint32_t> idx{0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
                                         2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5};
    return meshkernel::fromTriangles(pos, idx);
}

// A round rod from a to b.
MeshSolid rod(const Vec3& a, const Vec3& b, double radius)
{
    Vec3 d{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    d = {d[0] / len, d[1] / len, d[2] / len};
    Vec3 side = std::abs(d[2]) < 0.9 ? Vec3{-d[1], d[0], 0.0} : Vec3{1.0, 0.0, 0.0};
    const double sl = std::sqrt(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
    side = {side[0] / sl, side[1] / sl, side[2] / sl};
    const Vec3 other{d[1] * side[2] - d[2] * side[1], d[2] * side[0] - d[0] * side[2], d[0] * side[1] - d[1] * side[0]};
    constexpr int kSides = 12;
    std::vector<float> pos;
    for (const Vec3& end : {a, b})
        for (int k = 0; k < kSides; ++k) {
            const double t = 2 * std::numbers::pi * k / kSides;
            for (int c = 0; c < 3; ++c)
                pos.push_back(float(end[std::size_t(c)] + radius * (std::cos(t) * side[std::size_t(c)] +
                                                                    std::sin(t) * other[std::size_t(c)])));
        }
    for (const Vec3& end : {a, b})
        for (int c = 0; c < 3; ++c)
            pos.push_back(float(end[std::size_t(c)]));
    std::vector<std::uint32_t> idx;
    const std::uint32_t ca = 2 * kSides, cb = ca + 1;
    for (std::uint32_t k = 0; k < kSides; ++k) {
        const std::uint32_t p0 = k, p1 = (k + 1) % kSides, q0 = p0 + kSides, q1 = p1 + kSides;
        idx.insert(idx.end(), {p0, p1, q0, p1, q1, q0, ca, p1, p0, cb, q0, q1});
    }
    return meshkernel::fromTriangles(pos, idx);
}

struct Part {
    ObjectId id;
    Bounds box;              // on the plate, now
    std::vector<Vec3> points;  // its surface points on the plate, now (to find where a gate meets it)
};

}  // namespace

void check(const Settings& s)
{
    if (!(s.gap >= 1.0 && s.gap <= 50.0))
        throw std::invalid_argument("The gap must be from 1 to 50 mm.");
    if (!(s.bar >= 0.8 && s.bar <= 10.0))
        throw std::invalid_argument("The frame's bars must be from 0.8 to 10 mm.");
    if (!(s.gate >= 0.3 && s.gate <= s.bar))
        throw std::invalid_argument("The gates must be from 0.3 mm to as thick as the bars.");
    if (s.numbers && !(s.numberHeight >= 1.5 && s.numberHeight <= 10.0))
        throw std::invalid_argument("The numbers must be from 1.5 to 10 mm tall.");
}

Plan plan(const Document& doc, const std::vector<ObjectId>& ids, const Settings& s)
{
    check(s);
    if (ids.empty())
        throw std::invalid_argument("Pick the parts for the kit first.");
    std::vector<Part> parts;
    Bounds before;
    for (ObjectId id : ids) {
        if (!doc.find(id) || !doc.isTopLevel(id))
            throw std::invalid_argument("A kit is made from shapes on the plate.");
        Part p{id, doc.worldBounds(id), {}};
        if (!p.box.valid)
            throw std::invalid_argument("'" + doc.find(id)->name + "' has nothing to it.");
        const Mesh& m = *doc.mesh(id);
        const Transform t = doc.worldTransform(id);
        // Every so many points is enough to find where a gate can meet it.
        const std::size_t step = std::max<std::size_t>(1, m.vertexCount() / 4000);
        for (std::size_t v = 0; v < m.vertexCount(); v += step)
            p.points.push_back(t.apply(Vec3{m.positions[3 * v], m.positions[3 * v + 1], m.positions[3 * v + 2]}));
        before.expand(p.box.min[0], p.box.min[1], 0);
        before.expand(p.box.max[0], p.box.max[1], 0);
        parts.push_back(std::move(p));
    }
    // Biggest first (by depth, then width), in rows about as wide as the kit is deep.
    std::stable_sort(parts.begin(), parts.end(), [](const Part& a, const Part& b) {
        const double da = a.box.max[1] - a.box.min[1], db = b.box.max[1] - b.box.min[1];
        return da > db + 1e-6 || (std::abs(da - db) <= 1e-6 && a.box.max[0] - a.box.min[0] > b.box.max[0] - b.box.min[0]);
    });
    // The tag beside each part needs room in the gap.
    const double tagDepth = s.numbers ? s.numberHeight + 1.5 : 0.0;
    const double gap = std::max(s.gap, s.numbers ? tagDepth + 1.0 : 0.0);
    double area = 0.0, widest = 0.0;
    for (const Part& p : parts) {
        const double w = p.box.max[0] - p.box.min[0], d = p.box.max[1] - p.box.min[1];
        area += (w + gap) * (d + 2 * gap + s.bar);
        widest = std::max(widest, w);
    }
    const double rowWidth = std::max(widest, std::sqrt(area) * 1.1);
    struct Row {
        std::vector<std::size_t> parts;
        double width = 0.0, depth = 0.0;
    };
    std::vector<Row> rows(1);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const double w = parts[i].box.max[0] - parts[i].box.min[0], d = parts[i].box.max[1] - parts[i].box.min[1];
        if (!rows.back().parts.empty() && rows.back().width + gap + w > rowWidth)
            rows.emplace_back();
        Row& r = rows.back();
        r.width += (r.parts.empty() ? 0.0 : gap) + w;
        r.depth = std::max(r.depth, d);
        r.parts.push_back(i);
    }
    double inner = 0.0;
    for (const Row& r : rows)
        inner = std::max(inner, r.width);
    // From the bottom: bar, gap, row, gap, bar, gap, row... bar. Left and right: a bar, then the gap.
    const double frameWidth = inner + 2 * gap + 2 * s.bar;
    double frameDepth = s.bar;
    for (const Row& r : rows)
        frameDepth += 2 * gap + r.depth + s.bar;
    const auto mid = before.center();
    const double x0 = mid[0] - frameWidth / 2, y0 = mid[1] - frameDepth / 2;

    Plan out;
    std::vector<MeshSolid> pieces;
    // Side bars, and a bar under each row (and one over the last).
    pieces.push_back(block(x0, y0, 0, x0 + s.bar, y0 + frameDepth, s.bar));
    pieces.push_back(block(x0 + frameWidth - s.bar, y0, 0, x0 + frameWidth, y0 + frameDepth, s.bar));
    std::vector<double> barY;  // each bar's lower edge
    double y = y0;
    for (std::size_t r = 0; r <= rows.size(); ++r) {
        barY.push_back(y);
        pieces.push_back(block(x0, y, 0, x0 + frameWidth, y + s.bar, s.bar));
        if (r < rows.size())
            y += s.bar + 2 * gap + rows[r].depth;
    }
    int number = 0;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const Row& row = rows[r];
        double x = x0 + s.bar + gap + (inner - row.width) / 2;
        const double rowY = barY[r] + s.bar + gap;
        for (std::size_t i : row.parts) {
            const Part& p = parts[i];
            const double w = p.box.max[0] - p.box.min[0], d = p.box.max[1] - p.box.min[1];
            Slot slot;
            slot.id = p.id;
            slot.number = ++number;
            slot.dx = x - p.box.min[0];
            slot.dy = rowY + (row.depth - d) / 2 - p.box.min[1];
            out.placed.push_back(slot);
            // The gate: from the nearer bar to the part's nearest low point (moved into place).
            const double below = barY[r] + s.bar, above = barY[r + 1];
            Vec3 best{0, 0, 0};
            double score = 1e300;
            bool down = true;
            for (const Vec3& q : p.points) {
                const Vec3 m{q[0] + slot.dx, q[1] + slot.dy, q[2]};
                const double toBelow = m[1] - below, toAbove = above - m[1];
                const double sc = std::min(toBelow, toAbove) + 0.5 * std::max(0.0, m[2] - s.bar);
                if (sc < score) {
                    score = sc;
                    best = m;
                    down = toBelow <= toAbove;
                }
            }
            const double barEdge = down ? below : above;
            const double gz = s.bar / 2;
            const Vec3 from{best[0], barEdge + (down ? -0.3 : 0.3), gz};
            // A little into the part, so it holds.
            Vec3 into{best[0] - from[0], best[1] - from[1], best[2] - from[2]};
            const double il = std::sqrt(into[0] * into[0] + into[1] * into[1] + into[2] * into[2]);
            Vec3 to = il > 1e-6 ? Vec3{best[0] + into[0] / il * 0.4, best[1] + into[1] / il * 0.4,
                                        best[2] + into[2] / il * 0.4}
                                : best;
            to[2] = std::max(to[2], s.gate / 2);  // not through the plate
            pieces.push_back(rod(from, to, s.gate / 2));
            // Its number on a tag beside the gate, on the bar, in the gap.
            if (s.numbers) {
                const std::string text = std::to_string(slot.number);
                const Mesh digits = tolerance::digits(text, s.numberHeight, 0.6);
                const Bounds db = digits.bounds();
                const double tw = double(db.max[0] - db.min[0]) + 1.5;
                double tx = best[0] + s.gate + 0.5;
                if (tx + tw > x0 + frameWidth - s.bar)
                    tx = best[0] - s.gate - 0.5 - tw;  // no room to the right: to the left
                const double ty0 = down ? below - 0.2 : above + 0.2 - tagDepth;
                const double ty1 = down ? below - 0.2 + tagDepth : above + 0.2;
                pieces.push_back(block(tx, ty0, 0, tx + tw, ty1, 1.0));
                std::vector<float> pos = digits.positions;
                const double dx = tx + 0.75 - db.min[0], dy = (ty0 + ty1) / 2, dz = 1.0 - 0.01;
                for (std::size_t k = 0; k < pos.size(); k += 3) {
                    pos[k] = float(pos[k] + dx);
                    pos[k + 1] = float(pos[k + 1] + dy);
                    pos[k + 2] = float(pos[k + 2] + dz);
                }
                pieces.push_back(meshkernel::fromTriangles(pos, digits.indices));
            }
            x += w + gap;
        }
    }
    // Kept as one closed surface (shared corners), so it can be made a solid again.
    meshkernel::triangles(meshkernel::merged(pieces), out.frame.positions, out.frame.indices);
    out.area.expand(float(x0), float(y0), 0);
    out.area.expand(float(x0 + frameWidth), float(y0 + frameDepth), float(s.bar));
    return out;
}

ObjectId make(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids, const Settings& s)
{
    const Plan p = plan(doc, ids, s);
    stack.beginMacro("Make kit");
    try {
        // The parts into place (and numbered).
        std::vector<Body> before, after;
        for (const Slot& slot : p.placed) {
            const Body& b = *doc.find(slot.id);
            before.push_back(b);
            Body moved = b;
            moved.placement = movedOnPlate(b, {slot.dx, slot.dy, 0.0});
            if (s.numbers)
                moved.name = std::to_string(slot.number) + " " + b.name;
            after.push_back(std::move(moved));
        }
        stack.execute(std::make_unique<EditBodiesCommand>(std::move(before), std::move(after), "Lay out kit"));
        // The frame.
        Body frame = hinge::pieceBody(p.frame, doc.uniqueName("Kit frame"));
        frame.id = doc.allocateId();
        const ObjectId frameId = frame.id;
        stack.execute(std::make_unique<AddBodyCommand>(std::move(frame)));
        ObjectId result = frameId;
        if (s.join) {
            Body kit;
            kit.id = doc.allocateId();
            kit.name = doc.uniqueName("Kit");
            kit.boolean = BooleanSpec{};
            kit.boolean->op = BooleanOp::Merge;
            kit.boolean->operands.push_back(frameId);
            for (const Slot& slot : p.placed)
                kit.boolean->operands.push_back(slot.id);
            result = kit.id;
            stack.execute(std::make_unique<CombineCommand>(std::move(kit)));
        }
        stack.endMacro();
        return result;
    } catch (...) {
        stack.abortMacro();
        throw;
    }
}

}  // namespace forge::kit
