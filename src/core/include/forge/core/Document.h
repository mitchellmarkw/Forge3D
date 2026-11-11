// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace forge {

// Thrown when OpenCASCADE cannot build a shape (e.g. a combine that fails).
class GeometryError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// The model being edited: an ordered list of bodies plus cached geometry.
//
// Bodies are either top-level (on the plate) or parts inside a combine result.
// Which is which is worked out from the results' operand lists, so it can never
// disagree with them.
//
// The UI should not call the mutating functions directly; it goes through
// commands (Commands.h) on a CommandStack so every change can be undone.
// Mutators validate their input and throw std::invalid_argument /
// std::out_of_range *before* changing anything, so a failed call leaves the
// document untouched.
class Document {
public:
    enum class ChangeKind { Added, Removed, Modified };
    struct Change {
        ChangeKind kind;
        ObjectId id;
    };
    using Listener = std::function<void(const Change&)>;

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    Document() = default;
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // ---- Identity and naming ----
    ObjectId allocateId() { return m_nextId++; }
    // "Box 1", "Box 2", ...: the lowest number not already used with this base.
    std::string uniqueName(const std::string& base) const;

    // ---- Bodies ----
    const std::vector<Body>& bodies() const { return m_bodies; }
    std::size_t bodyCount() const { return m_bodies.size(); }
    bool empty() const { return m_bodies.empty(); }
    const Body* find(ObjectId id) const;
    std::size_t indexOf(ObjectId id) const;  // npos if absent

    // The combine result a body is a part of, or kInvalidId if it is on the plate.
    ObjectId parentOf(ObjectId id) const;
    bool isTopLevel(ObjectId id) const { return parentOf(id) == kInvalidId; }
    // Bodies on the plate, in list order.
    std::vector<ObjectId> topLevelIds() const;
    // The body, its parts, their parts... (depth first, the body first).
    std::vector<ObjectId> withDescendants(ObjectId id) const;

    // Inserts at index (or at the end if index is npos or past the end).
    // Throws if the id is invalid or already used, the spec is invalid, or (for a
    // combine result) a part is missing or already in another result.
    void insertBody(Body body, std::size_t index = npos);
    // Removes and returns the body. Removing a combine result frees its parts
    // (they become top-level). A part that is still inside a result cannot be
    // removed: remove the result first. Throws std::out_of_range if absent.
    Body removeBody(ObjectId id);
    // Swaps in a whole new set of bodies (File > Open, File > New). Every body is
    // checked first; on any problem it throws and the document is unchanged.
    // Not undoable: callers clear the undo history afterwards.
    void replaceAll(std::vector<Body> bodies);

    void setPlacement(ObjectId id, const Placement& placement);
    void setSpec(ObjectId id, const PrimitiveSpec& spec);
    void rename(ObjectId id, const std::string& name);

    // ---- Geometry ----
    // The exact solid in the body's own coordinates (placement not applied).
    // Cached until the body or one of its parts changes. Throws GeometryError if
    // OpenCASCADE cannot build it.
    TopoDS_Shape shape(ObjectId id) const;
    // Triangles of shape(id). The returned pointer stays valid after the cache
    // is rebuilt, and a new pointer means new geometry, so viewers can compare
    // pointers to spot changes.
    std::shared_ptr<const Mesh> mesh(ObjectId id) const;
    // Body coordinates -> its parent's coordinates (the plate, for top-level bodies).
    Transform localTransform(ObjectId id) const;
    // Body coordinates -> plate coordinates, through every parent.
    Transform worldTransform(ObjectId id) const;
    // Bounding box of the placed body, in plate coordinates. Tight even when the
    // body is tilted (built from the mesh points, not the corners of a box).
    Bounds worldBounds(ObjectId id) const;
    // Bounding box of everything on the plate; invalid if nothing is.
    Bounds worldBounds() const;
    // The middle of what you can see of the body, in its own coordinates: the
    // centre of its solid's bounding box. For a primitive this is its pivot; for a
    // combined shape it ignores cut-away parts (a cylinder subtracted from a cube
    // does not count). Rotation turns about this point.
    Vec3 visibleCentre(ObjectId id) const;

    // Builds what a combine would produce from these (top-level) bodies, without
    // changing the document. Throws GeometryError if OpenCASCADE fails.
    TopoDS_Shape previewCombine(BooleanOp op, const std::vector<ObjectId>& operands) const;

    // ---- Change notification ----
    int addListener(Listener listener);
    void removeListener(int handle);

private:
    Body& require(ObjectId id);
    void validateBoolean(const Body& body, const std::map<ObjectId, ObjectId>& parents,
                         const std::vector<Body>& all) const;
    void rebuildParents();
    void invalidate(ObjectId id, bool includeSelf);
    TopoDS_Shape combine(BooleanOp op, const std::vector<ObjectId>& operands) const;
    void notify(ChangeKind kind, ObjectId id);

    std::vector<Body> m_bodies;
    std::map<ObjectId, ObjectId> m_parentOf;  // part -> result
    ObjectId m_nextId = 1;

    mutable std::map<ObjectId, TopoDS_Shape> m_shapeCache;
    mutable std::map<ObjectId, std::shared_ptr<const Mesh>> m_meshCache;

    std::map<int, Listener> m_listeners;
    int m_nextListener = 1;
};

// Validates a primitive's parameters; throws std::invalid_argument if not buildable.
void validate(const PrimitiveSpec& spec);

// True if the shape has no solid in it (e.g. Intersect of shapes that don't touch).
// Exact bounding box of a solid (from its true surfaces, not its mesh), after
// moving it by t.
Bounds shapeBounds(const TopoDS_Shape& shape, const Transform& t = Transform::identity());
bool isEmptySolid(const TopoDS_Shape& shape);

// Where to put a new body of the given footprint so it does not overlap the
// existing ones: to the right of everything, with a gap. Empty plate: the centre.
Placement suggestPlacement(const Document& doc, double footprintWidth, double gap = 10.0);

// The body's placement moved up or down so its lowest point touches the plate (Z = 0).
Placement sitOnPlate(const Document& doc, ObjectId id);

}  // namespace forge
