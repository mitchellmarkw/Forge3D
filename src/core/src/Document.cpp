// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Document.h"

#include "forge/core/Primitives.h"
#include "forge/core/Tessellator.h"

#include <algorithm>
#include <cmath>
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
                positive(p.radius, "Radius");
                positive(p.height, "Height");
            }
        },
        spec);
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

// ---- Document -------------------------------------------------------------------

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

Body& Document::require(ObjectId id)
{
    const std::size_t i = indexOf(id);
    if (i == npos)
        throw std::out_of_range("No body with id " + std::to_string(id));
    return m_bodies[i];
}

void Document::insertBody(Body body, std::size_t index)
{
    if (body.id == kInvalidId)
        throw std::invalid_argument("Body id must be non-zero");
    if (find(body.id))
        throw std::invalid_argument("Body id " + std::to_string(body.id) + " is already in use");
    validate(body.spec);

    // Keep allocateId() ahead of any id inserted from outside (e.g. undo, file load).
    m_nextId = std::max(m_nextId, body.id + 1);

    const ObjectId id = body.id;
    if (index >= m_bodies.size())
        m_bodies.push_back(std::move(body));
    else
        m_bodies.insert(m_bodies.begin() + static_cast<std::ptrdiff_t>(index), std::move(body));
    notify(ChangeKind::Added, id);
}

Body Document::removeBody(ObjectId id)
{
    const std::size_t i = indexOf(id);
    if (i == npos)
        throw std::out_of_range("No body with id " + std::to_string(id));
    Body removed = std::move(m_bodies[i]);
    m_bodies.erase(m_bodies.begin() + static_cast<std::ptrdiff_t>(i));
    m_meshCache.erase(id);
    notify(ChangeKind::Removed, id);
    return removed;
}

void Document::setPlacement(ObjectId id, const Placement& placement)
{
    for (double v : {placement.x, placement.y, placement.z, placement.rotationZ})
        if (!std::isfinite(v))
            throw std::invalid_argument("Placement values must be finite numbers");
    Body& body = require(id);
    if (body.placement == placement)
        return;
    body.placement = placement;
    notify(ChangeKind::Modified, id);
}

void Document::setSpec(ObjectId id, const PrimitiveSpec& spec)
{
    validate(spec);
    Body& body = require(id);
    if (body.spec == spec)
        return;
    body.spec = spec;
    m_meshCache.erase(id);  // geometry must be rebuilt
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

std::shared_ptr<const Mesh> Document::mesh(ObjectId id) const
{
    const auto cached = m_meshCache.find(id);
    if (cached != m_meshCache.end())
        return cached->second;

    const Body* body = find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));

    auto built = std::make_shared<const Mesh>(tessellate(primitives::buildShape(body->spec)));
    m_meshCache[id] = built;
    return built;
}

Bounds Document::worldBounds(ObjectId id) const
{
    const Body* body = find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));

    const Bounds local = mesh(id)->bounds();
    Bounds world;
    if (!local.valid)
        return world;

    const Placement& p = body->placement;
    const double a = p.rotationZ * std::numbers::pi / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    for (int corner = 0; corner < 8; ++corner) {
        const double x = (corner & 1) ? local.max[0] : local.min[0];
        const double y = (corner & 2) ? local.max[1] : local.min[1];
        const double z = (corner & 4) ? local.max[2] : local.min[2];
        world.expand(static_cast<float>(c * x - s * y + p.x),
                     static_cast<float>(s * x + c * y + p.y),
                     static_cast<float>(z + p.z));
    }
    return world;
}

Bounds Document::worldBounds() const
{
    Bounds all;
    for (const Body& b : m_bodies) {
        const Bounds wb = worldBounds(b.id);
        if (!wb.valid)
            continue;
        all.expand(wb.min[0], wb.min[1], wb.min[2]);
        all.expand(wb.max[0], wb.max[1], wb.max[2]);
    }
    return all;
}

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
