// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/History.h"

#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace forge::history {

namespace {

// 20, 12.5, 0.25: no trailing zeros.
std::string num(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0')
        s.pop_back();
    if (!s.empty() && s.back() == '.')
        s.pop_back();
    return s == "-0" ? "0" : s;
}

std::string radius(double x, double y)
{
    return std::abs(x - y) < 1e-9 ? "R" + num(x) : "R" + num(x) + "/" + num(y);
}

const char* const kTimes = " \xC3\x97 ";  // " × "

std::string names(const Document& doc, const std::vector<ObjectId>& ids, std::size_t from)
{
    std::string out;
    for (std::size_t i = from; i < ids.size(); ++i) {
        const Body* b = doc.find(ids[i]);
        if (!b)
            continue;
        if (!out.empty())
            out += ", ";
        out += b->name;
    }
    return out;
}

std::string describeHoles(const HoleSpec& h)
{
    const std::size_t n = sketch::loosePoints(h.sketch).size();
    std::string out = n == 1 ? "1 hole " : std::to_string(n) + " holes ";
    out += h.size.empty() ? "\xC3\x98" + num(h.diameter) : h.size + (h.tapped ? " tap drill" : "");
    if (h.kind == HoleKind::Counterbored)
        out += " counterbored";
    else if (h.kind == HoleKind::Countersunk)
        out += " countersunk";
    return out + ", " + num(h.depth) + " mm deep";
}

}  // namespace

std::vector<ObjectId> steps(const Document& doc, ObjectId top)
{
    std::vector<ObjectId> line;
    std::set<ObjectId> seen;
    for (ObjectId id = top; id != kInvalidId && seen.insert(id).second;) {
        const Body* b = doc.find(id);
        if (!b)
            break;
        line.push_back(id);
        id = b->isBoolean() && !b->boolean->operands.empty() ? b->boolean->operands.front() : kInvalidId;
    }
    std::reverse(line.begin(), line.end());
    return line;
}

ObjectId objectOf(const Document& doc, ObjectId id)
{
    std::set<ObjectId> seen;
    while (doc.parentOf(id) != kInvalidId && seen.insert(id).second)
        id = doc.parentOf(id);
    return id;
}

std::size_t barAt(const Document& doc, ObjectId top)
{
    const auto line = steps(doc, top);
    std::size_t shown = line.size();
    // The bar sits below the oldest rolled-back step.
    for (std::size_t i = 0; i < line.size(); ++i)
        if (doc.find(line[i])->rolledBack) {
            shown = i;
            break;
        }
    return std::max<std::size_t>(shown, 1);
}

std::vector<Body> rolledTo(const Document& doc, ObjectId top, std::size_t shown)
{
    const auto line = steps(doc, top);
    shown = std::clamp<std::size_t>(shown, 1, line.size());
    std::vector<Body> out;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const Body& b = *doc.find(line[i]);
        const bool back = i >= shown && b.isBoolean();
        if (b.rolledBack != back) {
            Body next = b;
            next.rolledBack = back;
            out.push_back(std::move(next));
        }
    }
    return out;
}

ObjectId tip(const Document& doc, ObjectId top)
{
    const auto line = steps(doc, top);
    return line[barAt(doc, top) - 1];
}

std::string describe(const Document& doc, ObjectId id)
{
    const Body* body = doc.find(id);
    if (!body)
        return {};
    if (!body->isBoolean()) {
        return std::visit(
            [](const auto& p) -> std::string {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, BoxSpec> || std::is_same_v<T, WedgeSpec>)
                    return num(p.width) + kTimes + num(p.depth) + kTimes + num(p.height) + " mm";
                else if constexpr (std::is_same_v<T, CylinderSpec>)
                    return radius(p.radiusX, p.radiusY) + kTimes + num(p.height) + " mm";
                else if constexpr (std::is_same_v<T, SphereSpec>)
                    return std::abs(p.radiusX - p.radiusZ) < 1e-9 && std::abs(p.radiusX - p.radiusY) < 1e-9
                               ? "R" + num(p.radiusX)
                               : "R" + num(p.radiusX) + "/" + num(p.radiusY) + "/" + num(p.radiusZ);
                else if constexpr (std::is_same_v<T, ConeSpec>)
                    return radius(p.bottomRadiusX, p.bottomRadiusY) + " to " + radius(p.topRadiusX, p.topRadiusY) +
                           kTimes + num(p.height) + " mm";
                else if constexpr (std::is_same_v<T, TorusSpec>)
                    return "ring " + radius(p.ringRadiusX, p.ringRadiusY) + ", tube " +
                           radius(p.tubeRadius, p.tubeRadiusZ);
                else if constexpr (std::is_same_v<T, TubeSpec>)
                    return radius(p.outerRadiusX, p.outerRadiusY) + " outside, " +
                           radius(p.innerRadiusX, p.innerRadiusY) + " inside" + kTimes + num(p.height) + " mm";
                else if constexpr (std::is_same_v<T, PyramidSpec>)
                    return num(p.baseWidth) + kTimes + num(p.baseDepth) + " base" + kTimes + num(p.height) + " mm";
                else if constexpr (std::is_same_v<T, TextSpec>)
                    return "\"" + p.text + "\"";
                else if constexpr (std::is_same_v<T, PadSpec>)
                    return std::string("sketch, ") + num(p.length) + " mm" +
                           (p.reversed ? " down" : p.midplane ? " both ways" : "");
                else if constexpr (std::is_same_v<T, RevolveSpec>)
                    return std::string("sketch, spun ") + num(p.angle) + "\xC2\xB0";
                else if constexpr (std::is_same_v<T, HoleSpec>)
                    return describeHoles(p);
                else if constexpr (std::is_same_v<T, MeshSpec>)
                    return p.data ? std::to_string(p.data->indices.size() / 3) + " triangles" : std::string();
                else
                    return std::string();
            },
            body->spec);
    }
    const BooleanSpec& spec = *body->boolean;
    switch (spec.op) {
    case BooleanOp::Merge: return "with " + names(doc, spec.operands, 1);
    case BooleanOp::Subtract:
        if (spec.operands.size() == 2)
            if (const Body* tool = doc.find(spec.operands[1]); tool && !tool->isBoolean())
                if (const auto* hole = std::get_if<HoleSpec>(&tool->spec))
                    return describeHoles(*hole) + (spec.throughAll ? ", through all" : "");
        return "minus " + names(doc, spec.operands, 1) + (spec.throughAll ? " (through all)" : "");
    case BooleanOp::Intersect: return "overlap with " + names(doc, spec.operands, 1);
    case BooleanOp::Split: return "cut in two";
    case BooleanOp::Mirror: return "mirrored";
    case BooleanOp::Group: return "group of " + std::to_string(spec.operands.size());
    case BooleanOp::Emboss:
        if (!spec.emboss)
            return {};
        return "\"" + spec.emboss->text + "\" " + (spec.emboss->raised ? "raised" : "engraved");
    case BooleanOp::Edges: {
        if (!spec.edges)
            return {};
        const auto& f = *spec.edges;
        const std::size_t n = f.edges.size();
        const std::string count = n == 1 ? "1 edge" : std::to_string(n) + " edges";
        switch (f.kind) {
        case EdgeFinishKind::Rounding: return count + " rounded R" + num(f.size);
        case EdgeFinishKind::Fillet: return count + " filleted R" + num(f.size);
        case EdgeFinishKind::Chamfer: return count + " chamfered " + num(f.size) + " mm";
        case EdgeFinishKind::Bevel: return count + " bevelled " + num(f.size) + " mm at " + num(f.angle) + "\xC2\xB0";
        }
        return count;
    }
    case BooleanOp::Shell: {
        if (!spec.shell)
            return {};
        const std::size_t n = spec.shell->open.size();
        return "hollowed, walls " + num(spec.shell->thickness) + " mm" + (spec.shell->outward ? " outside" : "") +
               (n == 0 ? std::string(", closed") : n == 1 ? std::string(", 1 face open")
                                                         : ", " + std::to_string(n) + " faces open");
    }
    }
    return {};
}

}  // namespace forge::history
