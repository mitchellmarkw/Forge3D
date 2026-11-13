// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Pattern.h"
#include "forge/core/Document.h"
#include "forge/core/History.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace {

Vec3 unit(const Vec3& v)
{
    const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return l > 1e-12 ? Vec3{v[0] / l, v[1] / l, v[2] / l} : Vec3{0, 0, 1};
}

// Turning by `degrees` about the line through `centre` along `axis` (Rodrigues).
Transform turnAbout(const Vec3& centre, const Vec3& axisIn, double degrees)
{
    const Vec3 k = unit(axisIn);
    const double a = degrees * std::numbers::pi / 180.0, c = std::cos(a), s = std::sin(a), t = 1 - c;
    Transform r;
    r.m[0] = {t * k[0] * k[0] + c, t * k[0] * k[1] - s * k[2], t * k[0] * k[2] + s * k[1], 0};
    r.m[1] = {t * k[0] * k[1] + s * k[2], t * k[1] * k[1] + c, t * k[1] * k[2] - s * k[0], 0};
    r.m[2] = {t * k[0] * k[2] - s * k[1], t * k[1] * k[2] + s * k[0], t * k[2] * k[2] + c, 0};
    return Transform::translation(centre) * r * Transform::translation({-centre[0], -centre[1], -centre[2]});
}

PatternFinish moved(const PatternFinish& f, const Transform& t)
{
    PatternFinish out = f;
    out.dir1 = unit(t.applyDirection(f.dir1));
    out.dir2 = unit(t.applyDirection(f.dir2));
    out.axis = unit(t.applyDirection(f.axis));
    out.centre = t.apply(f.centre);
    return out;
}

const Body& patternResult(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Pattern || !body->boolean->pattern)
        throw std::invalid_argument("Not a Pattern step");
    return *body;
}

}  // namespace

namespace pattern {

int copies(const PatternFinish& f)
{
    switch (f.kind) {
    case PatternKind::Row: return f.count1;
    case PatternKind::Grid: return f.count1 * f.count2;
    case PatternKind::Circle: return f.count;
    }
    return 1;
}

std::vector<Transform> placements(const PatternFinish& f)
{
    if (f.count1 < 1 || f.count2 < 1 || f.count < 1)
        throw std::invalid_argument("A pattern needs at least one copy");
    if (copies(f) > 1000)
        throw GeometryError("That's more than 1000 copies: make fewer.");
    std::vector<Transform> out;
    switch (f.kind) {
    case PatternKind::Row:
    case PatternKind::Grid: {
        const Vec3 a = unit(f.dir1), b = unit(f.dir2);
        const int rows = f.kind == PatternKind::Grid ? f.count2 : 1;
        for (int j = 0; j < rows; ++j)
            for (int i = 0; i < f.count1; ++i)
                out.push_back(Transform::translation({a[0] * f.spacing1 * i + b[0] * f.spacing2 * j,
                                                      a[1] * f.spacing1 * i + b[1] * f.spacing2 * j,
                                                      a[2] * f.spacing1 * i + b[2] * f.spacing2 * j}));
        break;
    }
    case PatternKind::Circle: {
        if (!(f.angle > 0.0) || f.angle > 360.0)
            throw std::invalid_argument("The angle must be more than 0 and at most 360 degrees");
        // All the way round: evenly spaced, the last not on top of the first.
        const double step = f.angle >= 360.0 - 1e-9 ? 360.0 / f.count
                            : f.count > 1           ? f.angle / (f.count - 1)
                                                    : 0.0;
        for (int i = 0; i < f.count; ++i)
            out.push_back(turnAbout(f.centre, f.axis, step * i));
        break;
    }
    }
    return out;
}

std::optional<std::string> featureName(const Document& doc, ObjectId step)
{
    const Body* body = doc.find(step);
    if (!body || !body->isBoolean())
        return std::nullopt;
    const BooleanSpec& spec = *body->boolean;
    if (spec.op == BooleanOp::Follow)
        return body->name;
    if ((spec.op == BooleanOp::Merge || spec.op == BooleanOp::Subtract) && spec.operands.size() == 2)
        if (const Body* tool = doc.find(spec.operands[1]))
            return tool->name;
    return std::nullopt;
}

}  // namespace pattern

PatternFinish startingPattern(const Document& doc, ObjectId target, bool feature)
{
    PatternFinish f;
    const Bounds whole = doc.worldBounds(history::objectOf(doc, target));
    const auto c = whole.center();
    const auto size = whole.size();
    if (feature) {
        // About twice the size of what's repeated, and round the middle of the part.
        double small = 5.0;
        const Body* body = doc.find(target);
        if (body && body->isBoolean() && body->boolean->operands.size() == 2)
            if (const Bounds tool = doc.worldBounds(body->boolean->operands[1]); tool.valid)
                small = std::max(double(std::max(tool.size()[0], tool.size()[1])), 1.0);
        f.spacing1 = f.spacing2 = std::round(std::clamp(small * 2.0, 2.0, 50.0));
        f.centre = {c[0], c[1], c[2]};
    } else {
        // Side by side with a gap; round a point beside it.
        f.spacing1 = std::round(double(size[0]) * 1.25 + 1.0);
        f.spacing2 = std::round(double(size[1]) * 1.25 + 1.0);
        f.centre = {c[0] - double(size[0]) * 1.5, c[1], c[2]};
    }
    f.feature = feature;
    return f;
}

BooleanSpec patternSpec(const Document& doc, ObjectId target, const PatternFinish& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    BooleanSpec spec;
    spec.op = BooleanOp::Pattern;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.pattern = moved(inPlate, toStep);
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

PatternFinish patternInPlate(const Document& doc, ObjectId result)
{
    return moved(*patternResult(doc, result).boolean->pattern, doc.worldTransform(result));
}

Body withPattern(const Document& doc, ObjectId result, const PatternFinish& inPlate)
{
    Body next = patternResult(doc, result);
    next.boolean->pattern = moved(inPlate, doc.worldTransform(result).inverse());
    return next;
}

}  // namespace forge
