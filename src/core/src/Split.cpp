// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Split.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace {

constexpr double kDeg = std::numbers::pi / 180.0;

Vec3 unit(const Vec3& v)
{
    const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(len > 1e-12))
        throw std::invalid_argument("The cutting plane needs a direction");
    return {v[0] / len, v[1] / len, v[2] / len};
}

double dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

BooleanSpec splitSpec(ObjectId part, const SplitPlane& plane, bool front)
{
    const Vec3 n = unit(plane.normal);
    BooleanSpec spec;
    spec.op = BooleanOp::Split;
    spec.operands = {part};
    spec.planePoint = plane.point;
    spec.planeNormal = front ? n : Vec3{-n[0], -n[1], -n[2]};
    return spec;
}

}  // namespace

std::string sideName(const Vec3& d)
{
    const double ax = std::abs(d[0]), ay = std::abs(d[1]), az = std::abs(d[2]);
    if (az >= ax && az >= ay)
        return d[2] >= 0 ? "top" : "bottom";
    if (ax >= ay)
        return d[0] >= 0 ? "right" : "left";
    return d[1] >= 0 ? "back" : "front";
}

SplitPlane planeFromTilts(const Vec3& centre, double tiltX, double tiltY, double offset)
{
    const Vec3 n = Transform::rotationXYZ(tiltX, tiltY, 0.0).applyDirection({0.0, 0.0, 1.0});
    return {{centre[0] + n[0] * offset, centre[1] + n[1] * offset, centre[2] + n[2] * offset}, n};
}

void tiltsOfPlane(const SplitPlane& plane, const Vec3& centre, double& tiltX, double& tiltY, double& offset)
{
    // n = Ry(tiltY) Rx(tiltX) (0, 0, 1) = (cos x sin y, -sin x, cos x cos y)
    const Vec3 n = unit(plane.normal);
    tiltX = -std::asin(std::clamp(n[1], -1.0, 1.0)) / kDeg;
    tiltY = (std::abs(n[0]) < 1e-12 && std::abs(n[2]) < 1e-12) ? 0.0 : std::atan2(n[0], n[2]) / kDeg;
    offset = dot({plane.point[0] - centre[0], plane.point[1] - centre[1], plane.point[2] - centre[2]}, n);
    const auto tidy = [](double v) { return std::round(v * 1e6) / 1e6 + 0.0; };
    tiltX = tidy(tiltX);
    tiltY = tidy(tiltY);
    offset = tidy(offset);
}

std::vector<ObjectId> splitBody(CommandStack& stack, Document& doc, ObjectId id, const SplitPlane& plane,
                                SplitKeep keep)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!doc.isTopLevel(id))
        throw std::invalid_argument("Only a shape on the plate can be split");

    // Work out both halves first: refuse before changing anything.
    const BooleanSpec front = splitSpec(id, plane, true), back = splitSpec(id, plane, false);
    const TopoDS_Shape frontShape = doc.previewCombine(front), backShape = doc.previewCombine(back);
    if (isEmptySolid(frontShape) || isEmptySolid(backShape))
        throw std::invalid_argument("The plane doesn't cut through '" + body->name + "'. Move it so it crosses the shape.");

    const std::string baseName = body->name;
    const Vec3 n = unit(plane.normal);
    const auto makeResult = [&](BooleanSpec spec, const TopoDS_Shape& preview, const Vec3& side) {
        const auto c = shapeBounds(preview).center();
        spec.pivot = {c[0], c[1], c[2]};
        Body r;
        r.id = doc.allocateId();
        r.name = baseName + " (" + sideName(side) + ")";
        r.boolean = std::move(spec);
        return r;
    };

    std::vector<ObjectId> made;
    stack.beginMacro("Split " + baseName);
    try {
        if (keep == SplitKeep::Both) {
            const std::vector<Body> copies = duplicateTree(doc, id);
            for (const Body& c : copies)
                stack.execute(std::make_unique<AddBodyCommand>(c));
            const ObjectId copyId = copies.back().id;
            Body a = makeResult(front, frontShape, n);
            BooleanSpec backOnCopy = back;
            backOnCopy.operands = {copyId};
            Body b = makeResult(backOnCopy, backShape, {-n[0], -n[1], -n[2]});
            made = {a.id, b.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(a)));
            stack.execute(std::make_unique<CombineCommand>(std::move(b)));
        } else {
            const bool keepFront = keep == SplitKeep::Front;
            Body r = keepFront ? makeResult(front, frontShape, n) : makeResult(back, backShape, {-n[0], -n[1], -n[2]});
            made = {r.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
        }
    } catch (...) {
        stack.endMacro();
        stack.undo();  // put back whatever part of the split was done
        throw;
    }
    stack.endMacro();
    return made;
}

SplitPlane splitPlaneOf(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Split)
        throw std::invalid_argument("Not a split shape");
    const Transform t = doc.worldTransform(result);
    return {t.apply(body->boolean->planePoint), t.applyDirection(body->boolean->planeNormal)};
}

Body withSplitPlane(const Document& doc, ObjectId result, const SplitPlane& plane)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Split)
        throw std::invalid_argument("Not a split shape");
    const Transform toLocal = doc.worldTransform(result).inverse();
    Body next = *body;
    next.boolean->planePoint = toLocal.apply(plane.point);
    next.boolean->planeNormal = unit(toLocal.applyDirection(plane.normal));
    if (isEmptySolid(doc.previewCombine(*next.boolean)))
        throw std::invalid_argument("The plane doesn't cut through '" + body->name + "'. Move it so it crosses the shape.");
    return next;
}

}  // namespace forge
