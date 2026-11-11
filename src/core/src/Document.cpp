// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Document.h"

#include "forge/core/Primitives.h"
#include "forge/core/Tessellator.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace forge {

// ---- Free helpers ---------------------------------------------------------------

std::string typeName(const PrimitiveSpec& spec)
{
    return std::visit(
        [](const auto& p) -> std::string {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
                return "Box";
            else
                return "Cylinder";
        },
        spec);
}

std::string toString(BooleanOp op)
{
    switch (op) {
    case BooleanOp::Merge: return "Merge";
    case BooleanOp::Subtract: return "Subtract";
    case BooleanOp::Intersect: return "Intersect";
    }
    return "Combine";
}

std::string typeName(const Body& body)
{
    return body.isBoolean() ? toString(body.boolean->op) : typeName(body.spec);
}

void validate(const PrimitiveSpec& spec)
{
    const auto positive = [](double v, const char* what) {
        if (!(v > 0.0) || !std::isfinite(v))
            throw std::invalid_argument(std::string(what) + " must be a positive number");
    };
    std::visit(
        [&](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec>) {
                positive(p.width, "Width");
                positive(p.depth, "Depth");
                positive(p.height, "Height");
            } else {
                positive(p.radiusX, "Radius X");
                positive(p.radiusY, "Radius Y");
                positive(p.height, "Height");
            }
        },
        spec);
}

bool isEmptySolid(const TopoDS_Shape& shape)
{
    return shape.IsNull() || !TopExp_Explorer(shape, TopAbs_SOLID).More();
}

Placement suggestPlacement(const Document& doc, double footprintWidth, double gap)
{
    const Bounds all = doc.worldBounds();
    if (!all.valid)
        return {};
    Placement p;
    p.x = static_cast<double>(all.max[0]) + gap + footprintWidth / 2.0;
    return p;
}

Placement sitOnPlate(const Document& doc, ObjectId id)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    Placement p = body->placement;
    const Bounds wb = doc.worldBounds(id);
    if (wb.valid)
        p.z -= static_cast<double>(wb.min[2]);
    return p;
}

namespace {

void requireFinite(const Placement& p, const std::string& who)
{
    for (double v : {p.x, p.y, p.z, p.rotationX, p.rotationY, p.rotationZ})
        if (!std::isfinite(v))
            throw std::invalid_argument(who + " has an invalid placement");
}

gp_Trsf toTrsf(const Transform& t)
{
    gp_Trsf trsf;
    trsf.SetValues(t.m[0][0], t.m[0][1], t.m[0][2], t.m[0][3],  //
                   t.m[1][0], t.m[1][1], t.m[1][2], t.m[1][3],  //
                   t.m[2][0], t.m[2][1], t.m[2][2], t.m[2][3]);
    return trsf;
}

}  // namespace

// ---- Document: bodies -----------------------------------------------------------

std::string Document::uniqueName(const std::string& base) const
{
    std::set<std::string> used;
    for (const Body& b : m_bodies)
        used.insert(b.name);
    for (int n = 1;; ++n) {
        std::string candidate = base + " " + std::to_string(n);
        if (!used.count(candidate))
            return candidate;
    }
}

const Body* Document::find(ObjectId id) const
{
    const auto it = std::find_if(m_bodies.begin(), m_bodies.end(),
                                 [id](const Body& b) { return b.id == id; });
    return it == m_bodies.end() ? nullptr : &*it;
}

std::size_t Document::indexOf(ObjectId id) const
{
    for (std::size_t i = 0; i < m_bodies.size(); ++i)
        if (m_bodies[i].id == id)
            return i;
    return npos;
}

ObjectId Document::parentOf(ObjectId id) const
{
    const auto it = m_parentOf.find(id);
    return it == m_parentOf.end() ? kInvalidId : it->second;
}

std::vector<ObjectId> Document::topLevelIds() const
{
    std::vector<ObjectId> ids;
    for (const Body& b : m_bodies)
        if (!m_parentOf.count(b.id))
            ids.push_back(b.id);
    return ids;
}

std::vector<ObjectId> Document::withDescendants(ObjectId id) const
{
    std::vector<ObjectId> out;
    std::function<void(ObjectId)> walk = [&](ObjectId current) {
        out.push_back(current);
        if (const Body* b = find(current); b && b->isBoolean())
            for (ObjectId part : b->boolean->operands)
                walk(part);
    };
    if (find(id))
        walk(id);
    return out;
}

Body& Document::require(ObjectId id)
{
    const std::size_t i = indexOf(id);
    if (i == npos)
        throw std::out_of_range("No body with id " + std::to_string(id));
    return m_bodies[i];
}

void Document::validateBoolean(const Body& body, const std::map<ObjectId, ObjectId>& parents,
                               const std::vector<Body>& all) const
{
    const auto& ops = body.boolean->operands;
    if (ops.size() < 2)
        throw std::invalid_argument(toString(body.boolean->op) + " needs at least two shapes");
    std::set<ObjectId> seen;
    for (ObjectId part : ops) {
        if (part == body.id)
            throw std::invalid_argument("A shape cannot be combined with itself");
        if (!seen.insert(part).second)
            throw std::invalid_argument("The same shape is listed twice");
        const bool exists = std::any_of(all.begin(), all.end(), [part](const Body& b) { return b.id == part; });
        if (!exists)
            throw std::invalid_argument("A shape to combine (id " + std::to_string(part) + ") is missing");
        const auto owner = parents.find(part);
        if (owner != parents.end() && owner->second != body.id)
            throw std::invalid_argument("A shape can only be part of one combined result");
    }
}

void Document::insertBody(Body body, std::size_t index)
{
    if (body.id == kInvalidId)
        throw std::invalid_argument("Body id must be non-zero");
    if (find(body.id))
        throw std::invalid_argument("Body id " + std::to_string(body.id) + " is already in use");
    requireFinite(body.placement, "Body '" + body.name + "'");
    if (body.isBoolean()) {
        validateBoolean(body, m_parentOf, m_bodies);
        // No cycles: none of the parts may (through its own parts) contain an ancestor...
        // which cannot happen here because a new id has no ancestors yet.
    } else {
        validate(body.spec);
    }

    // Keep allocateId() ahead of any id inserted from outside (e.g. undo, file load).
    m_nextId = std::max(m_nextId, body.id + 1);

    const ObjectId id = body.id;
    std::vector<ObjectId> parts;
    if (body.isBoolean())
        parts = body.boolean->operands;
    if (index >= m_bodies.size())
        m_bodies.push_back(std::move(body));
    else
        m_bodies.insert(m_bodies.begin() + static_cast<std::ptrdiff_t>(index), std::move(body));
    for (ObjectId part : parts)
        m_parentOf[part] = id;

    notify(ChangeKind::Added, id);
    for (ObjectId part : parts)
        notify(ChangeKind::Modified, part);  // now inside a result
}

Body Document::removeBody(ObjectId id)
{
    const std::size_t i = indexOf(id);
    if (i == npos)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (const ObjectId owner = parentOf(id); owner != kInvalidId)
        throw std::invalid_argument("'" + m_bodies[i].name + "' is part of '" + find(owner)->name +
                                    "'; separate or delete that first");

    invalidate(id, true);
    Body removed = std::move(m_bodies[i]);
    m_bodies.erase(m_bodies.begin() + static_cast<std::ptrdiff_t>(i));
    std::vector<ObjectId> freed;
    if (removed.isBoolean()) {
        for (ObjectId part : removed.boolean->operands) {
            m_parentOf.erase(part);
            freed.push_back(part);
        }
    }
    notify(ChangeKind::Removed, id);
    for (ObjectId part : freed)
        notify(ChangeKind::Modified, part);  // back on the plate
    return removed;
}

void Document::rebuildParents()
{
    m_parentOf.clear();
    for (const Body& b : m_bodies)
        if (b.isBoolean())
            for (ObjectId part : b.boolean->operands)
                m_parentOf[part] = b.id;
}

void Document::replaceAll(std::vector<Body> bodies)
{
    std::set<ObjectId> ids;
    ObjectId highest = 0;
    std::map<ObjectId, ObjectId> parents;
    for (const Body& b : bodies) {
        if (b.id == kInvalidId)
            throw std::invalid_argument("Body id must be non-zero");
        if (!ids.insert(b.id).second)
            throw std::invalid_argument("Body id " + std::to_string(b.id) + " appears twice");
        requireFinite(b.placement, "Body '" + b.name + "'");
        if (!b.isBoolean())
            validate(b.spec);
        highest = std::max(highest, b.id);
    }
    for (const Body& b : bodies) {
        if (!b.isBoolean())
            continue;
        validateBoolean(b, parents, bodies);
        for (ObjectId part : b.boolean->operands)
            parents[part] = b.id;
    }
    // No cycles: following parents from any body must end at the plate.
    for (const Body& b : bodies) {
        std::set<ObjectId> path{b.id};
        for (auto it = parents.find(b.id); it != parents.end(); it = parents.find(it->second))
            if (!path.insert(it->second).second)
                throw std::invalid_argument("Combined shapes contain each other");
    }

    // All checked: now change things.
    std::vector<ObjectId> oldIds;
    for (const Body& b : m_bodies)
        oldIds.push_back(b.id);
    m_bodies.clear();
    m_parentOf.clear();
    m_shapeCache.clear();
    m_meshCache.clear();
    for (ObjectId id : oldIds)
        notify(ChangeKind::Removed, id);

    m_bodies = std::move(bodies);
    rebuildParents();
    m_nextId = std::max<ObjectId>(1, highest + 1);
    for (const Body& b : m_bodies)
        notify(ChangeKind::Added, b.id);
}

void Document::setPlacement(ObjectId id, const Placement& placement)
{
    requireFinite(placement, "The new position");
    Body& body = require(id);
    if (body.placement == placement)
        return;
    body.placement = placement;
    invalidate(id, false);  // a part moved: its result's shape changes; its own shape does not
    notify(ChangeKind::Modified, id);
}

void Document::setSpec(ObjectId id, const PrimitiveSpec& spec)
{
    validate(spec);
    Body& body = require(id);
    if (body.spec == spec)
        return;  // nothing to do (also lets gizmo drags on combined shapes pass through)
    if (body.isBoolean())
        throw std::invalid_argument("'" + body.name + "' is a combined shape; change its parts instead");
    body.spec = spec;
    invalidate(id, true);
    notify(ChangeKind::Modified, id);
}

void Document::rename(ObjectId id, const std::string& name)
{
    if (name.empty())
        throw std::invalid_argument("Name must not be empty");
    Body& body = require(id);
    if (body.name == name)
        return;
    body.name = name;
    notify(ChangeKind::Modified, id);
}

void Document::invalidate(ObjectId id, bool includeSelf)
{
    if (includeSelf) {
        m_shapeCache.erase(id);
        m_meshCache.erase(id);
    }
    // Every result above this body depends on it.
    std::set<ObjectId> visited{id};
    for (ObjectId p = parentOf(id); p != kInvalidId && visited.insert(p).second; p = parentOf(p)) {
        m_shapeCache.erase(p);
        m_meshCache.erase(p);
        notify(ChangeKind::Modified, p);
    }
}

// ---- Document: geometry ---------------------------------------------------------

TopoDS_Shape Document::combine(BooleanOp op, const std::vector<ObjectId>& operands) const
{
    TopoDS_Shape result;
    try {
        bool first = true;
        for (ObjectId part : operands) {
            // Each part in the result's coordinates: its own shape, moved by its placement.
            const TopoDS_Shape placed =
                BRepBuilderAPI_Transform(shape(part), toTrsf(localTransform(part)), /*copy=*/true).Shape();
            if (first) {
                result = placed;
                first = false;
                continue;
            }
            switch (op) {
            case BooleanOp::Merge: {
                BRepAlgoAPI_Fuse fuse(result, placed);
                if (!fuse.IsDone() || fuse.HasErrors())
                    throw GeometryError("Merge failed");
                result = fuse.Shape();
                break;
            }
            case BooleanOp::Subtract: {
                BRepAlgoAPI_Cut cut(result, placed);
                if (!cut.IsDone() || cut.HasErrors())
                    throw GeometryError("Subtract failed");
                result = cut.Shape();
                break;
            }
            case BooleanOp::Intersect: {
                BRepAlgoAPI_Common common(result, placed);
                if (!common.IsDone() || common.HasErrors())
                    throw GeometryError("Intersect failed");
                result = common.Shape();
                break;
            }
            }
        }
        // Merge faces that ended up on the same surface, so the result has clean
        // faces (and tessellates cleanly) instead of a patchwork.
        ShapeUpgrade_UnifySameDomain unify(result, /*UnifyEdges=*/true, /*UnifyFaces=*/true,
                                           /*ConcatBSplines=*/false);
        unify.Build();
        result = unify.Shape();
    } catch (const Standard_Failure& e) {
#if OCC_VERSION_HEX >= 0x080000
        const char* message = e.what();  // OCCT 8: Standard_Failure is a std::exception
#else
        const char* message = e.GetMessageString();
#endif
        throw GeometryError(std::string(toString(op)) + " failed in the geometry kernel: " +
                            (message ? message : "unknown error"));
    }
    return result;
}

TopoDS_Shape Document::shape(ObjectId id) const
{
    const auto cached = m_shapeCache.find(id);
    if (cached != m_shapeCache.end())
        return cached->second;

    const Body* body = find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));

    TopoDS_Shape built = body->isBoolean() ? combine(body->boolean->op, body->boolean->operands)
                                           : primitives::buildShape(body->spec);
    m_shapeCache[id] = built;
    return built;
}

std::shared_ptr<const Mesh> Document::mesh(ObjectId id) const
{
    const auto cached = m_meshCache.find(id);
    if (cached != m_meshCache.end())
        return cached->second;
    auto built = std::make_shared<const Mesh>(tessellate(shape(id)));
    m_meshCache[id] = built;
    return built;
}

TopoDS_Shape Document::previewCombine(BooleanOp op, const std::vector<ObjectId>& operands) const
{
    for (ObjectId part : operands)
        if (!find(part))
            throw std::out_of_range("No body with id " + std::to_string(part));
    return combine(op, operands);
}

Transform Document::localTransform(ObjectId id) const
{
    const Body* body = find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    return placementTransform(body->placement, pivotOf(*body));
}

Transform Document::worldTransform(ObjectId id) const
{
    Transform t = localTransform(id);
    std::set<ObjectId> visited{id};
    for (ObjectId p = parentOf(id); p != kInvalidId && visited.insert(p).second; p = parentOf(p))
        t = localTransform(p) * t;
    return t;
}

Bounds Document::worldBounds(ObjectId id) const
{
    const Bounds local = mesh(id)->bounds();
    Bounds world;
    if (!local.valid)
        return world;

    const Transform toWorld = worldTransform(id);
    for (int corner = 0; corner < 8; ++corner) {
        const Vec3 p = toWorld.apply({(corner & 1) ? local.max[0] : local.min[0],
                                      (corner & 2) ? local.max[1] : local.min[1],
                                      (corner & 4) ? local.max[2] : local.min[2]});
        world.expand(static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2]));
    }
    return world;
}

Bounds Document::worldBounds() const
{
    Bounds all;
    for (ObjectId id : topLevelIds()) {
        const Bounds wb = worldBounds(id);
        if (!wb.valid)
            continue;
        all.expand(wb.min[0], wb.min[1], wb.min[2]);
        all.expand(wb.max[0], wb.max[1], wb.max[2]);
    }
    return all;
}

// ---- Change notification ----------------------------------------------------------

int Document::addListener(Listener listener)
{
    const int handle = m_nextListener++;
    m_listeners.emplace(handle, std::move(listener));
    return handle;
}

void Document::removeListener(int handle)
{
    m_listeners.erase(handle);
}

void Document::notify(ChangeKind kind, ObjectId id)
{
    const Change change{kind, id};
    // Copy so a listener may add or remove listeners while being called.
    const auto listeners = m_listeners;
    for (const auto& [handle, listener] : listeners)
        listener(change);
}

}  // namespace forge
