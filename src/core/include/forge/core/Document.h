// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace forge {

// The model being edited: an ordered list of bodies plus cached geometry.
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

    // Inserts at index (or at the end if index is npos or past the end).
    // Throws if the id is invalid or already used, or the spec is invalid.
    void insertBody(Body body, std::size_t index = npos);
    // Removes and returns the body. Throws std::out_of_range if absent.
    Body removeBody(ObjectId id);
    // Swaps in a whole new set of bodies (File > Open, File > New). Every body is
    // checked first; on any problem it throws and the document is unchanged.
    // Not undoable: callers clear the undo history afterwards.
    void replaceAll(std::vector<Body> bodies);

    void setPlacement(ObjectId id, const Placement& placement);
    void setSpec(ObjectId id, const PrimitiveSpec& spec);
    void rename(ObjectId id, const std::string& name);

    // ---- Geometry ----
    // Triangles of the body in its own coordinates (placement not applied).
    // Built on first request and cached until the spec changes. The returned
    // pointer stays valid even after the cache is rebuilt, and a new pointer
    // means new geometry, so viewers can compare pointers to spot changes.
    std::shared_ptr<const Mesh> mesh(ObjectId id) const;
    // Bounding box of the placed body, in plate coordinates.
    Bounds worldBounds(ObjectId id) const;
    // Bounding box of everything; invalid if the document is empty.
    Bounds worldBounds() const;

    // ---- Change notification ----
    int addListener(Listener listener);
    void removeListener(int handle);

private:
    Body& require(ObjectId id);
    void notify(ChangeKind kind, ObjectId id);

    std::vector<Body> m_bodies;
    ObjectId m_nextId = 1;

    mutable std::map<ObjectId, std::shared_ptr<const Mesh>> m_meshCache;

    std::map<int, Listener> m_listeners;
    int m_nextListener = 1;
};

// Validates a primitive's parameters; throws std::invalid_argument if not buildable.
void validate(const PrimitiveSpec& spec);

// Where to put a new body of the given footprint so it does not overlap the
// existing ones: to the right of everything, with a gap. Empty plate: the centre.
Placement suggestPlacement(const Document& doc, double footprintWidth, double gap = 10.0);

// The body's placement moved up or down so its lowest point touches the plate (Z = 0).
Placement sitOnPlate(const Document& doc, ObjectId id);

}  // namespace forge
