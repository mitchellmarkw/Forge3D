// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Arrange.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/Mesh.h"

#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace forge {

namespace {

void requireOnPlate(const Document& doc, const std::vector<ObjectId>& ids)
{
    for (ObjectId id : ids) {
        const Body* b = doc.find(id);
        if (!b)
            throw std::out_of_range("No body with id " + std::to_string(id));
        if (!doc.isTopLevel(id))
            throw std::invalid_argument("'" + b->name + "' is part of another shape");
    }
}

// Runs `work` as one undo step; if it throws, takes back whatever it did.
template <class Work>
void asOneStep(CommandStack& stack, const std::string& label, Work&& work)
{
    stack.beginMacro(label);
    try {
        work();
    } catch (...) {
        stack.endMacro();
        stack.undo();
        throw;
    }
    stack.endMacro();
}

Body resultAround(Document& doc, BooleanSpec spec, const std::string& name)
{
    const auto c = doc.previewResult(spec).bounds.center();
    spec.pivot = {c[0], c[1], c[2]};
    Body r;
    r.id = doc.allocateId();
    r.name = name;
    r.boolean = std::move(spec);
    return r;
}

}  // namespace

BodyTree captureTree(const Document& doc, ObjectId id)
{
    BodyTree out;
    const std::vector<ObjectId> ids = doc.withDescendants(id);  // the shape first
    for (auto it = ids.rbegin(); it != ids.rend(); ++it)        // parts first
        out.push_back(*doc.find(*it));
    return out;
}

BodyTree cloneTree(Document& doc, const BodyTree& tree, const Vec3& offset)
{
    std::set<std::string> taken;
    const auto used = [&](const std::string& n) {
        if (taken.count(n))
            return true;
        for (const Body& other : doc.bodies())
            if (other.name == n)
                return true;
        return false;
    };
    std::map<ObjectId, ObjectId> newId;
    for (const Body& b : tree)
        newId[b.id] = doc.allocateId();
    BodyTree out;
    for (const Body& b : tree) {
        Body c = b;
        c.id = newId.at(b.id);
        std::string name = b.name + " copy";
        for (int n = 2; used(name); ++n)
            name = b.name + " copy " + std::to_string(n);
        taken.insert(name);
        c.name = name;
        if (c.boolean)
            for (ObjectId& part : c.boolean->operands)
                part = newId.at(part);
        out.push_back(std::move(c));
    }
    if (!out.empty()) {
        Placement& p = out.back().placement;
        p.x += offset[0];
        p.y += offset[1];
        p.z += offset[2];
    }
    return out;
}

std::vector<ObjectId> addCopies(CommandStack& stack, Document& doc, const std::vector<BodyTree>& trees,
                                const Vec3& offset, const std::string& label)
{
    std::vector<ObjectId> made;
    asOneStep(stack, label, [&] {
        for (const BodyTree& tree : trees) {
            const BodyTree copy = cloneTree(doc, tree, offset);
            for (const Body& b : copy)
                stack.execute(std::make_unique<AddBodyCommand>(b));
            if (!copy.empty())
                made.push_back(copy.back().id);
        }
    });
    return made;
}

Vec3 besideOffset(const Document& doc, const std::vector<ObjectId>& ids, double gap)
{
    Bounds all;
    for (ObjectId id : ids) {
        const Bounds b = doc.worldBounds(id);
        if (b.valid) {
            all.expand(b.min[0], b.min[1], b.min[2]);
            all.expand(b.max[0], b.max[1], b.max[2]);
        }
    }
    return {all.valid ? all.size()[0] + gap : gap, 0.0, 0.0};
}

Vec3 clearOfPlate(const Document& doc, double minX, double gap)
{
    const Bounds all = doc.worldBounds();
    if (!all.valid)
        return {0.0, 0.0, 0.0};
    return {all.max[0] + gap - minX, 0.0, 0.0};
}

double leftEdge(const Document& doc, const std::vector<ObjectId>& ids)
{
    double left = 0.0;
    bool any = false;
    for (ObjectId id : ids) {
        const Bounds b = doc.worldBounds(id);
        if (b.valid && (!any || b.min[0] < left)) {
            left = b.min[0];
            any = true;
        }
    }
    return left;
}

std::vector<ObjectId> mirrorBodies(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids, int axis)
{
    if (axis < 0 || axis > 2)
        throw std::invalid_argument("axis must be 0, 1 or 2");
    requireOnPlate(doc, ids);
    std::vector<ObjectId> made;
    asOneStep(stack, ids.size() == 1 ? "Mirror " + doc.find(ids.front())->name : "Mirror", [&] {
        for (ObjectId id : ids) {
            // A new result starts with no placement, so plate coordinates are its own.
            const Vec3 centre = doc.worldTransform(id).apply(doc.visibleCentre(id));
            BooleanSpec spec;
            spec.op = BooleanOp::Mirror;
            spec.operands = {id};
            spec.planePoint = centre;
            spec.planeNormal = {axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0};
            Body r = resultAround(doc, spec, doc.find(id)->name + " (mirrored)");
            made.push_back(r.id);
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
        }
    });
    return made;
}

ObjectId groupBodies(CommandStack& stack, Document& doc, const std::vector<ObjectId>& ids)
{
    if (ids.size() < 2)
        throw std::invalid_argument("Group needs two or more shapes");
    requireOnPlate(doc, ids);
    BooleanSpec spec;
    spec.op = BooleanOp::Group;
    spec.operands = ids;
    Body r = resultAround(doc, spec, doc.uniqueName("Group"));
    const ObjectId id = r.id;
    stack.execute(std::make_unique<CombineCommand>(std::move(r)));
    return id;
}

std::optional<std::pair<Vec3, Vec3>> faceUnder(const Document& doc, ObjectId id, const Vec3& origin,
                                               const Vec3& direction)
{
    const auto mesh = doc.mesh(id);
    if (!mesh || mesh->empty())
        return std::nullopt;
    const Transform toPlate = doc.worldTransform(id);
    const Transform toLocal = toPlate.inverse();
    // A small area: a flat face's own facing, not blended with its neighbours.
    const auto hit = emboss::surfaceAt(mesh->positions, mesh->indices, toLocal.apply(origin),
                                       toLocal.applyDirection(direction), 1e-3);
    if (!hit)
        return std::nullopt;
    return std::pair{toPlate.apply(hit->point), toPlate.applyDirection(hit->normal)};
}

Placement laidOnFace(const Document& doc, ObjectId id, const Vec3& normal)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    const double len = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (!(len > 1e-12))
        throw std::invalid_argument("A direction is needed");
    const Vec3 n{normal[0] / len, normal[1] / len, normal[2] / len};
    // The turn taking n to straight down (0, 0, -1): about n x down, by the angle between.
    Vec3 axis{n[1] * -1.0 - n[2] * 0.0, n[2] * 0.0 - n[0] * -1.0, 0.0};  // n x (0,0,-1) = (-n1, n0, 0)
    double s = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1]);
    const double c = -n[2];
    if (s < 1e-12) {
        if (c > 0.0)
            return sitOnPlate(doc, id);  // already facing down
        axis = {1.0, 0.0, 0.0};  // facing straight up: turn over about X
        s = 0.0;
    } else {
        axis = {axis[0] / s, axis[1] / s, 0.0};
    }
    const double t = 1.0 - c;
    const double x = axis[0], y = axis[1], z = axis[2];
    Transform turn;
    turn.m = {{{t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0.0},
               {t * x * y + s * z, t * y * y + c, t * y * z - s * x, 0.0},
               {t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0.0}}};
    const Transform world = doc.worldTransform(id);
    const Vec3 centre = world.apply(doc.visibleCentre(id));
    const Transform about = Transform::translation(centre) * turn *
                            Transform::translation({-centre[0], -centre[1], -centre[2]});
    Transform next = about * world;
    // Down onto the plate: its lowest point at zero.
    const auto mesh = doc.mesh(id);
    double lowest = 1e300;
    if (mesh)
        for (std::size_t i = 0; i + 2 < mesh->positions.size(); i += 3)
            lowest = std::min(lowest, next.apply({mesh->positions[i], mesh->positions[i + 1], mesh->positions[i + 2]})[2]);
    if (lowest < 1e299)
        next = Transform::translation({0.0, 0.0, -lowest}) * next;
    return placementFromTransform(next, pivotOf(*body));
}

}  // namespace forge
