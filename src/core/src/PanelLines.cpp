// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/PanelLines.h"

#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace forge {
namespace panel {

namespace {

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a, const Vec3& fallback)
{
    const double l = length(a);
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}
Vec3 v(const std::array<double, 3>& a) { return {a[0], a[1], a[2]}; }

std::string num(double x)
{
    std::ostringstream s;
    s.precision(3);
    s << x;
    return s.str();
}

// The groove's section: (across, up) points round it, top first.
std::vector<std::pair<double, double>> section(const PanelLineFinish& f)
{
    const double w = f.width / 2, d = f.depth, lift = std::max(0.3, f.depth);
    switch (f.profile) {
    case GrooveProfile::V: return {{-w, lift}, {w, lift}, {w, 0.0}, {0.0, -d}, {-w, 0.0}};
    case GrooveProfile::Square: return {{-w, lift}, {w, lift}, {w, -d}, {-w, -d}};
    case GrooveProfile::Round: {
        std::vector<std::pair<double, double>> s{{-w, lift}, {w, lift}};
        constexpr int kSteps = 8;
        for (int k = 0; k <= kSteps; ++k) {
            const double a = std::numbers::pi * k / kSteps;
            s.push_back({w * std::cos(a), -d * std::sin(a)});
        }
        return s;
    }
    }
    return {};
}

// One straight piece of groove from a to b (normals na, nb), each end `reach` longer.
MeshSolid piece(const Vec3& a, const Vec3& b, const Vec3& na, const Vec3& nb,
                const std::vector<std::pair<double, double>>& s, double reach)
{
    const Vec3 t = unit(sub(b, a), {1, 0, 0});
    const Vec3 ends[2] = {sub(a, mul(t, reach)), add(b, mul(t, reach))};
    const Vec3 normals[2] = {na, nb};
    Mesh m;
    for (int e = 0; e < 2; ++e) {
        // Upright to the surface, square to the line.
        Vec3 n = sub(normals[e], mul(t, dot(normals[e], t)));
        n = unit(n, unit(cross(t, std::abs(t[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}), {0, 0, 1}));
        const Vec3 side = cross(t, n);
        for (const auto& [x, y] : s) {
            const Vec3 p = add(ends[e], add(mul(side, x), mul(n, y)));
            for (int k = 0; k < 3; ++k)
                m.positions.push_back(float(p[std::size_t(k)]));
        }
    }
    const auto k = std::uint32_t(s.size());
    for (std::uint32_t i = 0; i < k; ++i) {
        const std::uint32_t a0 = i, a1 = (i + 1) % k, b0 = i + k, b1 = (i + 1) % k + k;
        m.indices.insert(m.indices.end(), {a0, a1, b0, a1, b1, b0});
    }
    for (std::uint32_t i = 1; i + 1 < k; ++i) {
        m.indices.insert(m.indices.end(), {0, i + 1, i});
        m.indices.insert(m.indices.end(), {k, k + i, k + i + 1});
    }
    // Outward, whichever way the section went round.
    double volume = 0.0;
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const auto p = [&](std::size_t j) {
            const std::size_t q = m.indices[i + j];
            return Vec3{m.positions[3 * q], m.positions[3 * q + 1], m.positions[3 * q + 2]};
        };
        volume += dot(p(0), cross(p(1), p(2)));
    }
    if (volume < 0)
        for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3)
            std::swap(m.indices[i + 1], m.indices[i + 2]);
    return meshkernel::fromTriangles(m.positions, m.indices);
}

}  // namespace

const char* title(GrooveProfile p)
{
    switch (p) {
    case GrooveProfile::V: return "V";
    case GrooveProfile::Square: return "Square";
    case GrooveProfile::Round: return "Round";
    }
    return "";
}

const char* key(GrooveProfile p)
{
    switch (p) {
    case GrooveProfile::V: return "v";
    case GrooveProfile::Square: return "square";
    case GrooveProfile::Round: return "round";
    }
    return "";
}

std::optional<GrooveProfile> fromKey(const std::string& k)
{
    for (GrooveProfile p : {GrooveProfile::V, GrooveProfile::Square, GrooveProfile::Round})
        if (k == key(p))
            return p;
    return std::nullopt;
}

void check(const PanelLineFinish& f)
{
    if (!(f.width >= 0.1 && f.width <= 20.0))
        throw std::invalid_argument("A panel line must be from 0.1 to 20 mm wide.");
    if (!(f.depth >= 0.05 && f.depth <= 20.0))
        throw std::invalid_argument("A panel line must be from 0.05 to 20 mm deep.");
    std::size_t points = 0;
    for (const PanelLine& l : f.lines) {
        if (l.points.size() < 2)
            throw std::invalid_argument("A panel line needs at least two points.");
        points += l.points.size();
        for (const PanelPoint& p : l.points)
            for (std::size_t k = 0; k < 3; ++k)
                if (!std::isfinite(p.point[k]) || !std::isfinite(p.normal[k]))
                    throw std::invalid_argument("A panel line's point is broken.");
    }
    if (points > 100000)
        throw std::invalid_argument("Those lines are too long to cut in one step.");
}

std::string describe(const PanelLineFinish& f)
{
    const std::size_t n = f.lines.size();
    return (n == 1 ? std::string("1 panel line") : std::to_string(n) + " panel lines") + ", " + title(f.profile) +
           " " + num(f.width) + " \xC3\x97 " + num(f.depth) + " mm";
}

double pointSpacing(double width)
{
    return std::clamp(width, 0.25, 1.0);
}

std::vector<MeshSolid> cutters(const PanelLine& line, const PanelLineFinish& f)
{
    std::vector<MeshSolid> out;
    const auto s = section(f);
    const std::size_t n = line.points.size();
    const std::size_t pieces = line.closed && n > 2 ? n : n - 1;
    for (std::size_t i = 0; i < pieces; ++i) {
        const PanelPoint& a = line.points[i];
        const PanelPoint& b = line.points[(i + 1) % n];
        const Vec3 pa = v(a.point), pb = v(b.point);
        const double len = length(sub(pb, pa));
        if (len < 1e-6)
            continue;
        // Long enough past each end to fill the outside of a bend.
        const double reach = std::min(f.width * 0.5, len * 0.5) + 0.02;
        out.push_back(piece(pa, pb, unit(v(a.normal), {0, 0, 1}), unit(v(b.normal), {0, 0, 1}), s, reach));
    }
    return out;
}

Mesh shown(const PanelLineFinish& f)
{
    std::vector<MeshSolid> all;
    for (const PanelLine& l : f.lines)
        if (l.points.size() >= 2) {
            auto c = cutters(l, f);
            all.insert(all.end(), std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
        }
    return meshkernel::displayMesh(meshkernel::together(all));
}

MeshSolid apply(const MeshSolid& part, const PanelLineFinish& f)
{
    check(f);
    std::vector<MeshSolid> all;
    for (const PanelLine& l : f.lines) {
        auto c = cutters(l, f);
        all.insert(all.end(), std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
    }
    if (all.empty())
        return part;
    return meshkernel::subtracted(part, all);
}

}  // namespace panel

namespace {

const Body& panelStep(const Document& doc, ObjectId result)
{
    const Body* b = doc.find(result);
    if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::PanelLines || !b->boolean->panelLines)
        throw std::invalid_argument("That isn't a Panel Lines step.");
    return *b;
}

PanelLineFinish moved(const PanelLineFinish& f, const Transform& t)
{
    PanelLineFinish out = f;
    for (PanelLine& l : out.lines)
        for (PanelPoint& p : l.points) {
            const Vec3 q = t.apply(Vec3{p.point[0], p.point[1], p.point[2]});
            Vec3 n = t.applyDirection(Vec3{p.normal[0], p.normal[1], p.normal[2]});
            const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 1e-12)
                n = {n[0] / len, n[1] / len, n[2] / len};
            p.point = {q[0], q[1], q[2]};
            p.normal = {n[0], n[1], n[2]};
        }
    return out;
}

}  // namespace

BooleanSpec panelLineSpec(const Document& doc, ObjectId target, const PanelLineFinish& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    panel::check(inPlate);
    BooleanSpec spec;
    spec.op = BooleanOp::PanelLines;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.panelLines = moved(inPlate, toStep);
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

PanelLineFinish panelLinesInPlate(const Document& doc, ObjectId result)
{
    const Body& body = panelStep(doc, result);
    return moved(*body.boolean->panelLines, doc.worldTransform(result));
}

Body withPanelLines(const Document& doc, ObjectId result, const PanelLineFinish& inPlate)
{
    const Body& body = panelStep(doc, result);
    panel::check(inPlate);
    Body next = body;
    next.boolean->panelLines = moved(inPlate, doc.worldTransform(result).inverse());
    return next;
}

}  // namespace forge
