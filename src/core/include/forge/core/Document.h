// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
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
    // Changes a combine result's settings that don't change which parts it holds
    // (its pivot, and a Split's plane). Throws if the operation or parts differ.
    void setBoolean(ObjectId id, const BooleanSpec& spec);
    void rename(ObjectId id, const std::string& name);
    // Switches a combine step off or on (see Body::suppressed / rolledBack).
    void setStepState(ObjectId id, bool suppressed, bool rolledBack);
    // Puts a new step between `above` and its first part: `step` takes that part as its
    // first part (any others must be on the plate), and `above` takes `step` instead.
    void insertStep(Body step, ObjectId above);
    // Takes a step out from inside a result: its first part goes back in its place, any
    // other parts go back on the plate. Returns the step, for undo.
    Body removeStep(ObjectId step);
    // What a step is built from right now: its own recipe, or (switched off) a
    // pass-through of its parts.
    BooleanSpec effectiveSpec(const Body& body) const;
    // A recipe that just passes its parts through: the first one (one-part steps,
    // Subtract), or all side by side (Merge, Intersect, Group).
    static BooleanSpec passThrough(const BooleanSpec& spec);
    // Why a step can't be built right now (an earlier change left it nothing to work
    // on), or nothing if it's fine. A step like that is left out: the object carries
    // on as if it were switched off.
    std::optional<std::string> problemWith(ObjectId id) const;

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
    // The same for any combine, including Split (its plane in plate coordinates,
    // since a new result starts with no placement).
    TopoDS_Shape previewCombine(const BooleanSpec& spec) const;

    // ---- The mesh engine --------------------------------------------------------
    // Imported meshes, and every combined shape with one inside, are worked out by the
    // mesh engine (fast with many triangles); everything else by the B-rep engine.
    bool usesMeshEngine(ObjectId id) const;
    // The body as a triangle solid, in its own coordinates (cached like shape()).
    // Throws MeshEngineError (e.g. an imported mesh that isn't watertight).
    MeshSolid meshSolid(ObjectId id) const;
    // Bounds of the body in its own coordinates, from whichever engine it uses.
    Bounds localBounds(ObjectId id) const;

    // What a combine would give, from whichever engine applies: empty or not, and its
    // bounds (plate coordinates, for a new result with no placement).
    struct Preview {
        bool empty = true;
        Bounds bounds;
    };
    Preview previewResult(const BooleanSpec& spec) const;
    // What a combine (e.g. Emboss, Edges) would look like, for drawing before it is
    // made: triangles in the result's coordinates. Kept, so making the
    // result straight after doesn't work it out again.
    std::shared_ptr<const Mesh> previewMesh(const BooleanSpec& spec) const;
    // Just the embossed words of an Emboss recipe (result coordinates), without
    // joining them to the shape: a quick preview for shapes too big to re-cut on
    // every change.
    std::shared_ptr<const Mesh> previewWords(const BooleanSpec& spec) const;

    // ---- Change notification ----
    int addListener(Listener listener);
    void removeListener(int handle);

private:
    Body& require(ObjectId id);
    void validateBoolean(const Body& body, const std::map<ObjectId, ObjectId>& parents,
                         const std::vector<Body>& all) const;
    void rebuildParents();
    void invalidate(ObjectId id, bool includeSelf);
    TopoDS_Shape combine(const BooleanSpec& spec) const;
    MeshSolid combineMeshes(const BooleanSpec& spec) const;
    bool specUsesMesh(const BooleanSpec& spec) const;
    void notify(ChangeKind kind, ObjectId id);

    std::vector<Body> m_bodies;
    std::map<ObjectId, ObjectId> m_parentOf;  // part -> result
    ObjectId m_nextId = 1;

    mutable std::map<ObjectId, TopoDS_Shape> m_shapeCache;
    mutable std::map<ObjectId, std::shared_ptr<const Mesh>> m_meshCache;
    mutable std::map<ObjectId, MeshSolid> m_solidCache;
    mutable std::map<ObjectId, std::string> m_problems;  // steps that couldn't be built, and why
    // The last mesh-engine preview, so making that result straight after doesn't
    // work it out a second time. Kept only while its parts are unchanged.
    struct LastPreview {
        BooleanSpec spec;
        std::vector<Body> parts;
        MeshSolid solid;
    };
    mutable std::optional<LastPreview> m_lastPreview;
    bool partsUnchanged(const LastPreview& p) const;

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
// Triangles to draw an imported mesh (flat shaded), stretched to its size.
Mesh displayMesh(const MeshSpec& spec);

// Where to put a new body of the given footprint so it does not overlap the
// existing ones: to the right of everything, with a gap. Empty plate: the centre.
Placement suggestPlacement(const Document& doc, double footprintWidth, double gap = 10.0);

// The body's placement moved up or down so its lowest point touches the plate (Z = 0).
Placement sitOnPlate(const Document& doc, ObjectId id);

}  // namespace forge
