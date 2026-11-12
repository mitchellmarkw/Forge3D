// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Document.h"

#include "forge/core/Edges.h"
#include "forge/core/Emboss.h"

#include "forge/core/Export.h"
#include "forge/core/Primitives.h"
#include "forge/core/Tessellator.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS.hxx>
#include <Bnd_Box.hxx>
#include <TopLoc_Location.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <set>
#include <stdexcept>
#include <type_traits>

namespace forge {

// ---- Free helpers ---------------------------------------------------------------

void validate(const PrimitiveSpec& spec)
{
    const auto& fields = specFields(spec);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const double v = specValue(spec, i);
        if (!std::isfinite(v) || v < 0.0 || (v == 0.0 && !fields[i].allowZero))
            throw std::invalid_argument(std::string(fields[i].label) +
                                        (fields[i].allowZero ? " must not be negative" : " must be a positive number"));
    }
    if (const auto* mesh = std::get_if<MeshSpec>(&spec); mesh && (!mesh->data || mesh->data->indices.empty()))
        throw std::invalid_argument("The imported shape has no triangles");
    if (const auto* cone = std::get_if<ConeSpec>(&spec)) {
        const auto endOk = [](double x, double y) { return (x == 0.0) == (y == 0.0); };
        if (!endOk(cone->bottomRadiusX, cone->bottomRadiusY) || !endOk(cone->topRadiusX, cone->topRadiusY))
            throw std::invalid_argument("Each end of a cone needs both radii above 0, or both 0 for a point");
        if (cone->bottomRadiusX == 0.0 && cone->topRadiusX == 0.0)
            throw std::invalid_argument("A cone can't be a point at both ends");
    }
    if (const auto* torus = std::get_if<TorusSpec>(&spec)) {
        const double small = std::min(torus->ringRadiusX, torus->ringRadiusY);
        const double big = std::max(torus->ringRadiusX, torus->ringRadiusY);
        if (!(torus->tubeRadius < small * small / big))
            throw std::invalid_argument(small == big ? "Tube radius must be less than the ring radius"
                                                     : "Tube radius is too big for this oval ring: the hole would "
                                                       "close up at its narrow ends");
    }
    if (const auto* tube = std::get_if<TubeSpec>(&spec);
        tube && !(tube->innerRadiusX < tube->outerRadiusX && tube->innerRadiusY < tube->outerRadiusY))
        throw std::invalid_argument("Inner radius must be less than the outer radius (X and Y)");
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
    const BooleanOp op = body.boolean->op;
    if (op == BooleanOp::Edges) {
        if (ops.size() != 1)
            throw std::invalid_argument("Edges works on exactly one shape");
        if (!body.boolean->edges || body.boolean->edges->edges.empty())
            throw std::invalid_argument("Pick at least one edge");
    } else if (op == BooleanOp::Split || op == BooleanOp::Mirror || op == BooleanOp::Emboss) {
        if (op == BooleanOp::Emboss && !body.boolean->emboss)
            throw std::invalid_argument("Emboss needs words");
        if (ops.size() != 1)
            throw std::invalid_argument(toString(op) + " works on exactly one shape");
        const auto& n = body.boolean->planeNormal;
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (!(len > 1e-9) || !std::isfinite(len))
            throw std::invalid_argument("Split needs a plane direction");
        for (double v : body.boolean->planePoint)
            if (!std::isfinite(v))
                throw std::invalid_argument("Split has an invalid plane");
    } else if (ops.size() < 2) {
        throw std::invalid_argument(toString(op) + " needs at least two shapes");
    }
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
    m_solidCache.clear();
    m_meshCache.clear();
    m_problems.clear();
    m_lastPreview.reset();
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

void Document::setBoolean(ObjectId id, const BooleanSpec& spec)
{
    Body& body = require(id);
    if (!body.isBoolean())
        throw std::invalid_argument("'" + body.name + "' is not a combined shape");
    if (*body.boolean == spec)
        return;
    const auto swappable = [](BooleanOp op) {
        return op == BooleanOp::Merge || op == BooleanOp::Subtract || op == BooleanOp::Intersect;
    };
    const bool opOk = body.boolean->op == spec.op || (swappable(body.boolean->op) && swappable(spec.op));
    if (!opOk || body.boolean->operands != spec.operands)
        throw std::invalid_argument("Changing which parts a combined shape holds needs Separate and Combine");
    Body check = body;
    check.boolean = spec;
    validateBoolean(check, m_parentOf, m_bodies);
    body.boolean = spec;
    invalidate(id, true);
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
        m_solidCache.erase(id);
        m_problems.erase(id);
    }
    // Every result above this body depends on it.
    std::set<ObjectId> visited{id};
    for (ObjectId p = parentOf(id); p != kInvalidId && visited.insert(p).second; p = parentOf(p)) {
        m_shapeCache.erase(p);
        m_meshCache.erase(p);
        m_solidCache.erase(p);
        m_problems.erase(p);
        notify(ChangeKind::Modified, p);
    }
    if (m_lastPreview)
        for (ObjectId part : m_lastPreview->spec.operands)
            if (visited.count(part)) {
                m_lastPreview.reset();
                break;
            }
}

bool Document::partsUnchanged(const LastPreview& p) const
{
    for (const Body& was : p.parts) {
        const Body* now = find(was.id);
        if (!now || !(now->spec == was.spec) || !(now->boolean == was.boolean)
            || !(now->placement == was.placement))
            return false;
    }
    return true;
}

// ---- Document: geometry ---------------------------------------------------------

TopoDS_Shape Document::combine(const BooleanSpec& spec) const
{
    const BooleanOp op = spec.op;
    const std::vector<ObjectId>& operands = spec.operands;
    TopoDS_Shape result;
    try {
        if (op == BooleanOp::Emboss)
            throw GeometryError("Emboss is worked out by the mesh engine");
        if (op == BooleanOp::Edges) {
            if (!spec.edges)
                throw GeometryError("No edges were picked");
            const ObjectId part = operands.at(0);
            const TopoDS_Shape placed =
                BRepBuilderAPI_Transform(shape(part), toTrsf(localTransform(part)), /*copy=*/true).Shape();
            return edges::apply(placed, *spec.edges);
        }
        if (op == BooleanOp::Mirror) {
            const ObjectId part = operands.at(0);
            gp_Trsf mirror;
            mirror.SetMirror(gp_Ax2(gp_Pnt(spec.planePoint[0], spec.planePoint[1], spec.planePoint[2]),
                                    gp_Dir(spec.planeNormal[0], spec.planeNormal[1], spec.planeNormal[2])));
            const gp_Trsf placed = mirror * toTrsf(localTransform(part));
            return BRepBuilderAPI_Transform(shape(part), placed, /*copy=*/true).Shape();
        }
        if (op == BooleanOp::Group) {
            // Side by side, not merged: a compound of the placed parts.
            BRep_Builder builder;
            TopoDS_Compound group;
            builder.MakeCompound(group);
            for (ObjectId part : operands)
                builder.Add(group,
                            BRepBuilderAPI_Transform(shape(part), toTrsf(localTransform(part)), /*copy=*/true).Shape());
            return group;
        }
        if (op == BooleanOp::Split) {
            const ObjectId part = operands.at(0);
            const TopoDS_Shape placed =
                BRepBuilderAPI_Transform(shape(part), toTrsf(localTransform(part)), /*copy=*/true).Shape();
            // A huge box on the kept side of the plane; keep what the part shares with it.
            const gp_Pnt p(spec.planePoint[0], spec.planePoint[1], spec.planePoint[2]);
            const gp_Dir n(spec.planeNormal[0], spec.planeNormal[1], spec.planeNormal[2]);
            const Bounds b = shapeBounds(placed);
            const auto size = b.size();
            const double extent =
                std::max({double(size[0]), double(size[1]), double(size[2]), 1.0}) * 10.0 +
                p.Distance(gp_Pnt(b.center()[0], b.center()[1], b.center()[2])) * 2.0;
            const gp_Ax2 frame(p, n);
            const gp_Pnt corner = p.Translated(gp_Vec(frame.XDirection()) * -extent + gp_Vec(frame.YDirection()) * -extent);
            const TopoDS_Shape keep =
                BRepPrimAPI_MakeBox(gp_Ax2(corner, n, frame.XDirection()), 2 * extent, 2 * extent, extent).Shape();
            BRepAlgoAPI_Common common(placed, keep);
            if (!common.IsDone() || common.HasErrors())
                throw GeometryError("Split failed");
            result = common.Shape();
            ShapeUpgrade_UnifySameDomain unify(result, true, true, false);
            unify.Build();
            return unify.Shape();
        }
        bool first = true;
        Bounds reach;
        for (ObjectId part : operands) {
            // Each part in the result's coordinates: its own shape, moved by its placement.
            TopoDS_Shape placed =
                BRepBuilderAPI_Transform(shape(part), toTrsf(localTransform(part)), /*copy=*/true).Shape();
            if (first) {
                result = placed;
                first = false;
                reach = shapeBounds(placed);
                continue;
            }
            if (op == BooleanOp::Subtract && spec.throughAll)
                if (const auto stretched = throughTool(part, reach))
                    placed = BRepBuilderAPI_Transform(primitives::buildShape(stretched->first),
                                                      toTrsf(stretched->second), true)
                                 .Shape();
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
            case BooleanOp::Split:
            case BooleanOp::Mirror:
            case BooleanOp::Group:
            case BooleanOp::Emboss:
            case BooleanOp::Edges:
                break;  // handled above / by the mesh engine
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

    TopoDS_Shape built;
    if (body->isBoolean() && usesMeshEngine(id)) {
        // Rarely needed (everything else asks the mesh engine directly): faces from its triangles.
        auto data = std::make_shared<MeshData>();
        meshkernel::triangles(meshSolid(id), data->positions, data->indices);
        if (data->indices.empty())
            return TopoDS_Shape();
        built = primitives::makeMeshSolid(*data, {1.0, 1.0, 1.0});
    } else {
        if (!body->isBoolean()) {
            built = primitives::buildShape(body->spec);
        } else {
            try {
                built = combine(effectiveSpec(*body));
                m_problems.erase(id);
            } catch (const GeometryError& e) {
                // A step that can't be built (an earlier change left it nothing to work
                // on): note why, and carry on as if it were switched off.
                if (body->isOff())
                    throw;
                m_problems[id] = e.what();
                built = combine(passThrough(*body->boolean));
            }
        }
    }
    m_shapeCache[id] = built;
    return built;
}

std::shared_ptr<const Mesh> Document::mesh(ObjectId id) const
{
    const auto cached = m_meshCache.find(id);
    if (cached != m_meshCache.end())
        return cached->second;
    const Body* body = find(id);
    std::shared_ptr<const Mesh> built;
    if (body && !body->isBoolean() && std::holds_alternative<MeshSpec>(body->spec))
        built = std::make_shared<const Mesh>(displayMesh(std::get<MeshSpec>(body->spec)));  // no B-rep needed
    else if (body && body->isBoolean() && usesMeshEngine(id))
        built = std::make_shared<const Mesh>(meshkernel::displayMesh(meshSolid(id)));
    else
        built = std::make_shared<const Mesh>(tessellate(shape(id)));
    m_meshCache[id] = built;
    return built;
}

TopoDS_Shape Document::previewCombine(BooleanOp op, const std::vector<ObjectId>& operands) const
{
    BooleanSpec spec;
    spec.op = op;
    spec.operands = operands;
    return previewCombine(spec);
}

TopoDS_Shape Document::previewCombine(const BooleanSpec& spec) const
{
    for (ObjectId part : spec.operands)
        if (!find(part))
            throw std::out_of_range("No body with id " + std::to_string(part));
    if ((spec.op == BooleanOp::Split || spec.op == BooleanOp::Mirror || spec.op == BooleanOp::Emboss ||
         spec.op == BooleanOp::Edges) &&
        spec.operands.size() != 1)
        throw std::invalid_argument(toString(spec.op) + " works on exactly one shape");
    return combine(spec);
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

namespace {

// True if any face is a free-form surface (B-spline, Bezier, swept...), where
// OpenCASCADE's optimal box can come out a little too small.
bool hasFreeFormFaces(const TopoDS_Shape& shape)
{
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
        switch (BRepAdaptor_Surface(TopoDS::Face(it.Current()), /*restriction=*/false).GetType()) {
        case GeomAbs_Plane:
        case GeomAbs_Cylinder:
        case GeomAbs_Cone:
        case GeomAbs_Sphere:
        case GeomAbs_Torus:
            break;
        default:
            return true;
        }
    }
    return false;
}

}  // namespace

// ---- Document: the mesh engine -------------------------------------------------------

bool Document::usesMeshEngine(ObjectId id) const
{
    const Body* body = find(id);
    if (!body)
        return false;
    if (!body->isBoolean())
        return std::holds_alternative<MeshSpec>(body->spec);
    return specUsesMesh(effectiveSpec(*body));
}

BooleanSpec Document::passThrough(const BooleanSpec& spec)
{
    BooleanSpec pass = spec;
    pass.op = BooleanOp::Group;
    pass.emboss.reset();
    pass.edges.reset();
    switch (spec.op) {
    case BooleanOp::Merge:
    case BooleanOp::Intersect:
    case BooleanOp::Group: break;  // all parts, side by side
    default: pass.operands.resize(1); break;  // just the first part
    }
    return pass;
}

BooleanSpec Document::effectiveSpec(const Body& body) const
{
    if (!body.isBoolean())
        throw std::invalid_argument("'" + body.name + "' is not a combined shape");
    return body.isOff() ? passThrough(*body.boolean) : *body.boolean;
}

std::optional<std::pair<PrimitiveSpec, Transform>> Document::throughTool(ObjectId tool, const Bounds& reach) const
{
    const Body* body = find(tool);
    if (!body || body->isBoolean())
        return std::nullopt;
    double* height = nullptr;
    PrimitiveSpec spec = body->spec;
    if (auto* b = std::get_if<BoxSpec>(&spec))
        height = &b->height;
    else if (auto* c = std::get_if<CylinderSpec>(&spec))
        height = &c->height;
    else if (auto* t = std::get_if<TubeSpec>(&spec))
        height = &t->height;
    else if (auto* x = std::get_if<TextSpec>(&spec))
        height = &x->height;
    if (!height)
        return std::nullopt;
    // How far the first part reaches along the tool's own height (its local Z).
    const Transform place = localTransform(tool);
    const Transform toTool = place.inverse();
    double lo = 1e300, hi = -1e300;
    for (int k = 0; k < 8; ++k) {
        const Vec3 corner{k & 1 ? reach.max[0] : reach.min[0], k & 2 ? reach.max[1] : reach.min[1],
                          k & 4 ? reach.max[2] : reach.min[2]};
        const double z = toTool.apply(corner)[2];
        lo = std::min(lo, z);
        hi = std::max(hi, z);
    }
    if (!(hi > lo))
        return std::nullopt;
    const double margin = 1.0 + 0.01 * (hi - lo);
    *height = hi - lo + 2.0 * margin;
    Transform lift;
    lift.m[2][3] = lo - margin;  // the stretched tool's base, below the first part
    return std::pair{spec, place * lift};
}

std::optional<std::string> Document::problemWith(ObjectId id) const
{
    const Body* body = find(id);
    if (!body || !body->isBoolean())
        return std::nullopt;
    try {
        if (usesMeshEngine(id))
            (void)meshSolid(id);
        else
            (void)shape(id);
    } catch (const std::exception& e) {
        return std::string(e.what());
    }
    if (const auto it = m_problems.find(id); it != m_problems.end())
        return it->second;
    return std::nullopt;
}

void Document::setStepState(ObjectId id, bool suppressed, bool rolledBack)
{
    Body& body = require(id);
    if (body.suppressed == suppressed && body.rolledBack == rolledBack)
        return;
    if (!body.isBoolean() && (suppressed || rolledBack))
        throw std::invalid_argument("Only a combine step can be switched off");
    body.suppressed = suppressed;
    body.rolledBack = rolledBack;
    invalidate(id, true);
    notify(ChangeKind::Modified, id);
}

void Document::insertStep(Body step, ObjectId above)
{
    if (!step.isBoolean() || step.boolean->operands.empty())
        throw std::invalid_argument("A step needs parts");
    Body& parent = require(above);
    if (!parent.isBoolean() || parent.boolean->operands.empty())
        throw std::invalid_argument("'" + parent.name + "' is not a combined shape");
    const ObjectId below = step.boolean->operands.front();
    auto& slots = parent.boolean->operands;
    const auto slot = std::find(slots.begin(), slots.end(), below);
    if (slot == slots.end())
        throw std::invalid_argument("The new step's first part must be a part of '" + parent.name + "'");
    if (find(step.id) || step.id == kInvalidId)
        throw std::invalid_argument("The new step needs a new id");
    // Check it as if `below` were free.
    std::map<ObjectId, ObjectId> parents = m_parentOf;
    parents.erase(below);
    validateBoolean(step, parents, m_bodies);
    requireFinite(step.placement, "Body '" + step.name + "'");
    m_nextId = std::max(m_nextId, step.id + 1);
    const ObjectId id = step.id;
    *slot = id;
    m_bodies.push_back(std::move(step));
    rebuildParents();
    invalidate(id, true);
    notify(ChangeKind::Added, id);
}

Body Document::removeStep(ObjectId id)
{
    const Body& step = require(id);
    if (!step.isBoolean())
        throw std::invalid_argument("'" + step.name + "' is not a step");
    const ObjectId above = parentOf(id);
    if (above == kInvalidId)
        throw std::invalid_argument("'" + step.name + "' is on the plate; use Separate");
    Body taken = step;
    Body& parent = require(above);
    std::replace(parent.boolean->operands.begin(), parent.boolean->operands.end(), id,
                 taken.boolean->operands.front());
    m_bodies.erase(std::remove_if(m_bodies.begin(), m_bodies.end(), [id](const Body& b) { return b.id == id; }),
                   m_bodies.end());
    m_shapeCache.erase(id);
    m_meshCache.erase(id);
    m_solidCache.erase(id);
    rebuildParents();
    invalidate(above, true);
    notify(ChangeKind::Removed, id);
    for (std::size_t i = 1; i < taken.boolean->operands.size(); ++i)
        notify(ChangeKind::Modified, taken.boolean->operands[i]);
    return taken;
}

bool Document::specUsesMesh(const BooleanSpec& spec) const
{
    if (spec.op == BooleanOp::Emboss)
        return true;  // letters following a surface only exist as triangles
    for (ObjectId part : spec.operands)
        if (usesMeshEngine(part))
            return true;
    return false;
}

MeshSolid Document::meshSolid(ObjectId id) const
{
    if (const auto cached = m_solidCache.find(id); cached != m_solidCache.end())
        return cached->second;
    const Body* body = find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    MeshSolid built;
    if (body->isBoolean() && usesMeshEngine(id)) {
        if (!body->isOff() && m_lastPreview && m_lastPreview->spec == *body->boolean &&
            partsUnchanged(*m_lastPreview))
            built = m_lastPreview->solid;
        else {
            try {
                built = combineMeshes(effectiveSpec(*body));
                m_problems.erase(id);
            } catch (const GeometryError& e) {
                if (body->isOff())
                    throw;
                m_problems[id] = e.what();  // as in shape(): note it, carry on without the step
                built = combineMeshes(passThrough(*body->boolean));
            }
        }
        m_lastPreview.reset();
    } else if (const auto* mesh = std::get_if<MeshSpec>(&body->spec)) {
        built = meshkernel::fromMeshData(*mesh->data, {mesh->width / mesh->data->size[0],
                                                       mesh->depth / mesh->data->size[1],
                                                       mesh->height / mesh->data->size[2]});
    } else {
        // A primitive working with a mesh: its shape as fine, shared-point triangles.
        const ExportMesh tri = exportMeshes(*this, {id}, exportQuality()).front();
        // exportMeshes places it on the plate; bring it back to its own coordinates.
        const Transform back = worldTransform(id).inverse();
        std::vector<float> local(tri.positions.size());
        for (std::size_t i = 0; i < tri.positions.size(); i += 3) {
            const Vec3 p = back.apply({tri.positions[i], tri.positions[i + 1], tri.positions[i + 2]});
            local[i] = float(p[0]);
            local[i + 1] = float(p[1]);
            local[i + 2] = float(p[2]);
        }
        built = meshkernel::fromTriangles(local, tri.indices);
    }
    m_solidCache[id] = built;
    return built;
}

MeshSolid Document::combineMeshes(const BooleanSpec& spec) const
{
    std::vector<MeshSolid> parts;
    for (ObjectId part : spec.operands)
        parts.push_back(meshkernel::transformed(meshSolid(part), localTransform(part)));
    if (parts.empty())
        throw GeometryError("Nothing to combine");
    switch (spec.op) {
    case BooleanOp::Merge: return meshkernel::merged(parts);
    case BooleanOp::Subtract: {
        std::vector<MeshSolid> tools(parts.begin() + 1, parts.end());
        if (spec.throughAll) {
            const Bounds reach = meshkernel::bounds(parts.front());
            for (std::size_t i = 1; i < spec.operands.size(); ++i)
                if (const auto stretched = throughTool(spec.operands[i], reach)) {
                    // As triangles: the stretched tool on its own, then placed.
                    Document scratch;
                    Body b;
                    b.id = 1;
                    b.name = "tool";
                    b.spec = stretched->first;
                    scratch.insertBody(b);
                    const ExportMesh m = exportMeshes(scratch, {1}, exportQuality()).front();
                    tools[i - 1] = meshkernel::transformed(meshkernel::fromTriangles(m.positions, m.indices),
                                                           stretched->second);
                }
        }
        return meshkernel::subtracted(parts.front(), tools);
    }
    case BooleanOp::Intersect: return meshkernel::intersected(parts);
    case BooleanOp::Split: return meshkernel::trimmed(parts.front(), spec.planePoint, spec.planeNormal);
    case BooleanOp::Mirror: return meshkernel::mirrored(parts.front(), spec.planePoint, spec.planeNormal);
    case BooleanOp::Group: return meshkernel::together(parts);
    case BooleanOp::Edges:
        throw GeometryError("Rounding and chamfering work on solid shapes, not on imported meshes or shapes made "
                            "with them.");
    case BooleanOp::Emboss: {
        if (!spec.emboss)
            throw GeometryError("Emboss has no words");
        std::vector<float> pos;
        std::vector<std::uint32_t> idx;
        meshkernel::triangles(parts.front(), pos, idx);
        const MeshSolid words = emboss::letters(pos, idx, spec.planePoint, spec.planeNormal, *spec.emboss);
        return spec.emboss->raised ? meshkernel::merged({parts.front(), words})
                                   : meshkernel::subtracted(parts.front(), {words});
    }
    }
    throw GeometryError("Unknown combine");
}

Bounds Document::localBounds(ObjectId id) const
{
    const Body* body = find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (usesMeshEngine(id))
        return body->isBoolean() ? meshkernel::bounds(meshSolid(id)) : mesh(id)->bounds();
    return shapeBounds(shape(id));
}

Document::Preview Document::previewResult(const BooleanSpec& spec) const
{
    Preview out;
    if (specUsesMesh(spec)) {
        for (ObjectId part : spec.operands)
            if (!find(part))
                throw std::out_of_range("No body with id " + std::to_string(part));
        const MeshSolid solid = combineMeshes(spec);
        LastPreview keep{spec, {}, solid};
        for (ObjectId part : spec.operands)
            keep.parts.push_back(*find(part));
        m_lastPreview = std::move(keep);
        out.empty = meshkernel::isEmpty(solid);
        out.bounds = meshkernel::bounds(solid);
    } else {
        const TopoDS_Shape s = previewCombine(spec);
        out.empty = isEmptySolid(s);
        out.bounds = shapeBounds(s);
    }
    return out;
}

std::shared_ptr<const Mesh> Document::previewMesh(const BooleanSpec& spec) const
{
    for (ObjectId part : spec.operands)
        if (!find(part))
            throw std::out_of_range("No body with id " + std::to_string(part));
    if (!specUsesMesh(spec))
        return std::make_shared<const Mesh>(tessellate(previewCombine(spec)));
    MeshSolid solid;
    if (m_lastPreview && m_lastPreview->spec == spec && partsUnchanged(*m_lastPreview)) {
        solid = m_lastPreview->solid;
    } else {
        solid = combineMeshes(spec);
        LastPreview keep{spec, {}, solid};
        for (ObjectId part : spec.operands)
            keep.parts.push_back(*find(part));
        m_lastPreview = std::move(keep);
    }
    return std::make_shared<const Mesh>(meshkernel::displayMesh(solid));
}

std::shared_ptr<const Mesh> Document::previewWords(const BooleanSpec& spec) const
{
    if (spec.op != BooleanOp::Emboss || !spec.emboss || spec.operands.size() != 1)
        throw std::invalid_argument("previewWords is for Emboss");
    const ObjectId part = spec.operands.front();
    if (!find(part))
        throw std::out_of_range("No body with id " + std::to_string(part));
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(meshkernel::transformed(meshSolid(part), localTransform(part)), pos, idx);
    const MeshSolid words = emboss::letters(pos, idx, spec.planePoint, spec.planeNormal, *spec.emboss);
    return std::make_shared<const Mesh>(meshkernel::displayMesh(words));
}

Bounds shapeBounds(const TopoDS_Shape& shape, const Transform& t)
{
    Bounds out;
    if (shape.IsNull())
        return out;
    const TopoDS_Shape moved = shape.Moved(TopLoc_Location(toTrsf(t)));
    Bnd_Box box;
    BRepBndLib::AddOptimal(moved, box, /*useTriangulation=*/false, /*useShapeTolerance=*/false);
    if (box.IsVoid())
        return out;
    std::array<double, 3> lo{}, hi{};
    box.Get(lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);

    if (hasFreeFormFaces(moved)) {
        // Check each side against the true surface: the point of the shape nearest
        // to a far-away point beyond that side is (very nearly) its outermost point.
        // It lies on the shape, so it can only push the box out to the truth.
        const double size = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1.0});
        const double far = size * 1000.0;
        const gp_Pnt c((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2);
        for (int axis = 0; axis < 3; ++axis) {
            for (int sign : {-1, 1}) {
                gp_Pnt p = c;
                p.SetCoord(axis + 1, c.Coord(axis + 1) + sign * far);
                BRepExtrema_DistShapeShape dist(BRepBuilderAPI_MakeVertex(p).Vertex(), moved);
                if (!dist.IsDone() || dist.NbSolution() == 0)
                    continue;
                for (int i = 1; i <= dist.NbSolution(); ++i) {
                    const double v = dist.PointOnShape2(i).Coord(axis + 1);
                    lo[static_cast<std::size_t>(axis)] = std::min(lo[static_cast<std::size_t>(axis)], v);
                    hi[static_cast<std::size_t>(axis)] = std::max(hi[static_cast<std::size_t>(axis)], v);
                }
            }
        }
    }
    out.expand(static_cast<float>(lo[0]), static_cast<float>(lo[1]), static_cast<float>(lo[2]));
    out.expand(static_cast<float>(hi[0]), static_cast<float>(hi[1]), static_cast<float>(hi[2]));
    return out;
}

Mesh displayMesh(const MeshSpec& spec)
{
    // Flat shading: each triangle gets its own three points and its face normal.
    Mesh m;
    const MeshData& d = *spec.data;
    const float sx = float(spec.width / d.size[0]), sy = float(spec.depth / d.size[1]),
                sz = float(spec.height / d.size[2]);
    m.positions.reserve(d.indices.size() * 3);
    m.normals.reserve(d.indices.size() * 3);
    m.indices.reserve(d.indices.size());
    for (std::size_t t = 0; t + 2 < d.indices.size(); t += 3) {
        float p[3][3];
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t i = d.indices[t + k];
            p[k][0] = d.positions[i * 3] * sx;
            p[k][1] = d.positions[i * 3 + 1] * sy;
            p[k][2] = d.positions[i * 3 + 2] * sz;
        }
        const float ux = p[1][0] - p[0][0], uy = p[1][1] - p[0][1], uz = p[1][2] - p[0][2];
        const float wx = p[2][0] - p[0][0], wy = p[2][1] - p[0][1], wz = p[2][2] - p[0][2];
        float n[3] = {uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx};
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 0.0f)
            for (float& c : n)
                c /= len;
        for (int k = 0; k < 3; ++k) {
            m.indices.push_back(static_cast<std::uint32_t>(m.positions.size() / 3));
            m.positions.insert(m.positions.end(), {p[k][0], p[k][1], p[k][2]});
            m.normals.insert(m.normals.end(), {n[0], n[1], n[2]});
        }
    }
    return m;
}

Bounds Document::worldBounds(ObjectId id) const
{
    if (const Body* body = find(id); body && body->isBoolean() && usesMeshEngine(id))
        return meshkernel::bounds(meshSolid(id), worldTransform(id));
    if (const Body* body = find(id); body && !body->isBoolean() && std::holds_alternative<MeshSpec>(body->spec)) {
        // Exact and quick: the corners of a flat-faced shape are its points.
        const MeshSpec& spec = std::get<MeshSpec>(body->spec);
        const MeshData& d = *spec.data;
        const Transform t = worldTransform(id);
        const Vec3 s{spec.width / d.size[0], spec.depth / d.size[1], spec.height / d.size[2]};
        Bounds out;
        for (std::size_t i = 0; i + 2 < d.positions.size(); i += 3) {
            const Vec3 p = t.apply({d.positions[i] * s[0], d.positions[i + 1] * s[1], d.positions[i + 2] * s[2]});
            out.expand(float(p[0]), float(p[1]), float(p[2]));
        }
        return out;
    }
    return shapeBounds(shape(id), worldTransform(id));
}

Vec3 Document::visibleCentre(ObjectId id) const
{
    const Body* body = find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!body->isBoolean())
        return pivotOf(body->spec);
    const Bounds b = localBounds(id);
    if (!b.valid)
        return pivotOf(*body);
    const auto c = b.center();
    return {c[0], c[1], c[2]};
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
