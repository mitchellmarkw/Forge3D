// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/MakePart.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/MeshKernel.h"

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace forge {

namespace makepart {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a, const Vec3& fallback = {0, 0, 1})
{
    const double l = std::sqrt(dot(a, a));
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}

std::uint64_t key(std::uint32_t a, std::uint32_t b)
{
    return std::uint64_t(a) << 32 | b;
}

}  // namespace

Mesh slab(const Mesh& patch, double inner, double outer, double shrink)
{
    const std::size_t n = patch.positions.size() / 3;
    if (patch.indices.size() < 3 || n < 3)
        throw std::invalid_argument("Nothing is painted where the part would go.");
    std::vector<Vec3> p(n);
    for (std::size_t i = 0; i < n; ++i)
        p[i] = {patch.positions[3 * i], patch.positions[3 * i + 1], patch.positions[3 * i + 2]};
    // Each point's facing: its triangles' (by area), evened out a little with its
    // neighbours' so the shell's faces don't fold where the surface turns sharply.
    std::vector<Vec3> normal(n, Vec3{0, 0, 0});
    std::vector<std::vector<std::uint32_t>> near(n);
    std::map<std::uint64_t, int> edges;  // directed edge → how many triangles run it
    for (std::size_t t = 0; t + 2 < patch.indices.size(); t += 3) {
        const std::uint32_t v[3] = {patch.indices[t], patch.indices[t + 1], patch.indices[t + 2]};
        const Vec3 f = cross(sub(p[v[1]], p[v[0]]), sub(p[v[2]], p[v[0]]));
        for (int k = 0; k < 3; ++k) {
            normal[v[k]] = add(normal[v[k]], f);
            near[v[k]].push_back(v[(k + 1) % 3]);
            ++edges[key(v[k], v[(k + 1) % 3])];
        }
    }
    for (Vec3& v : normal)
        v = unit(v);
    for (int pass = 0; pass < 3; ++pass) {
        std::vector<Vec3> next(n);
        for (std::size_t i = 0; i < n; ++i) {
            Vec3 s = mul(normal[i], 2.0);
            for (std::uint32_t j : near[i])
                s = add(s, normal[j]);
            next[i] = unit(s, normal[i]);
        }
        normal.swap(next);
    }
    // The edge: directed edges no triangle runs the other way.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> rim;
    std::vector<int> rimCount(n, 0);
    for (const auto& [k, count] : edges) {
        const auto a = std::uint32_t(k >> 32), b = std::uint32_t(k & 0xFFFFFFFFu);
        if (count > 1)
            throw std::invalid_argument("The painted surface folds over itself; paint the area again.");
        if (!edges.count(key(b, a))) {
            rim.emplace_back(a, b);
            ++rimCount[a];
        }
    }
    for (int c : rimCount)
        if (c > 1)
            throw std::invalid_argument("The paint's edge touches itself at a point: paint a little more (or rub a "
                                        "little out) where two painted areas just meet.");
    if (shrink > 0.0) {
        // The edge brought in along the surface: across each rim edge, into the paint.
        std::vector<Vec3> in(n, Vec3{0, 0, 0});
        for (const auto& [a, b] : rim) {
            const Vec3 d = cross(normal[a], sub(p[b], p[a]));  // left of a→b, facing out: inside
            in[a] = add(in[a], d);
            in[b] = add(in[b], cross(normal[b], sub(p[b], p[a])));
        }
        for (std::size_t i = 0; i < n; ++i)
            if (dot(in[i], in[i]) > 0)
                p[i] = add(p[i], mul(unit(in[i]), shrink));
    }
    // Two layers: the outer facing out, the inner facing in, and a wall round the edge.
    Mesh m;
    m.positions.reserve(6 * n);
    for (int layer = 0; layer < 2; ++layer) {
        const double d = layer == 0 ? outer : inner;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 q = add(p[i], mul(normal[i], d));
            m.positions.insert(m.positions.end(), {float(q[0]), float(q[1]), float(q[2])});
        }
    }
    const auto o = [](std::uint32_t v) { return v; };
    const auto in = [n](std::uint32_t v) { return std::uint32_t(v + n); };
    for (std::size_t t = 0; t + 2 < patch.indices.size(); t += 3) {
        const std::uint32_t a = patch.indices[t], b = patch.indices[t + 1], c = patch.indices[t + 2];
        m.indices.insert(m.indices.end(), {o(a), o(b), o(c), in(a), in(c), in(b)});
    }
    for (const auto& [a, b] : rim)
        m.indices.insert(m.indices.end(), {o(b), o(a), in(a), o(b), in(a), in(b)});
    return m;
}

Mesh paintedSurface(const Document& doc, ObjectId id, const std::vector<MaskDab>& dabs)
{
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(doc.meshSolid(id), pos, idx);
    const Transform toPlate = doc.worldTransform(id);
    for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
        const Vec3 q = toPlate.apply(Vec3{pos[i], pos[i + 1], pos[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            pos[i + k] = float(q[k]);
    }
    if (dabs.empty())
        throw std::invalid_argument("Paint where the part goes first.");
    try {
        return mask::split(pos, idx, dabs).surface;
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Paint where the part goes, not the whole shape (the part is made over the "
                                    "painted area only).");
    }
}

Made makePart(CommandStack& stack, Document& doc, ObjectId id, const std::vector<MaskDab>& dabs, const Options& o)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!(o.thickness >= 0.3 && o.thickness <= 20.0))
        throw std::invalid_argument("The part's thickness must be from 0.3 to 20 mm.");
    if (!(o.fit >= 0.0 && o.fit <= 1.0))
        throw std::invalid_argument("The fit must be from 0 to 1 mm.");
    const Mesh surface = paintedSurface(doc, id, dabs);
    const std::string name = body->name;

    // Sitting on top: from the fit gap out. Set in: from under the surface up to it (so
    // the outline stays), its edge brought in by the fit; the body cut back under it, by
    // the fit deeper, and a little past the surface so nothing's left on top.
    const Mesh part = o.setIn ? slab(surface, -o.thickness, 0.0, o.fit) : slab(surface, o.fit, o.fit + o.thickness);
    MeshSolid partSolid;
    try {
        partSolid = meshkernel::fromTriangles(part.positions, part.indices);
    } catch (const std::exception&) {
        throw std::invalid_argument("The part couldn't be made from this paint: paint a smoother area, or make it "
                                    "thinner.");
    }
    if (meshkernel::volume(partSolid) <= 0.0)
        throw std::invalid_argument("The part came out empty: paint a larger area, or make it thicker.");

    Made made;
    stack.beginMacro("Make part from " + name);
    try {
        Body b = hinge::pieceBody(part, name + " part");
        b.id = doc.allocateId();
        // "Torso part", or "Torso part 2" if there's one already.
        b.name = name + " part";
        for (const Body& other : doc.bodies())
            if (other.name == b.name) {
                b.name = doc.uniqueName(name + " part");
                break;
            }
        made.part = b.id;
        stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
        made.body = id;
        if (o.setIn) {
            const Mesh hollow = slab(surface, -(o.thickness + o.fit), 0.5);
            Body tool = hinge::pieceBody(hollow, "Part's place");
            tool.id = doc.allocateId();
            tool.hardware = true;
            const ObjectId toolId = tool.id;
            stack.execute(std::make_unique<AddBodyCommand>(std::move(tool)));
            BooleanSpec spec;
            spec.op = BooleanOp::Subtract;
            spec.operands = {id, toolId};
            const Document::Preview preview = doc.previewResult(spec);
            if (preview.empty)
                throw std::invalid_argument("Setting the part in leaves nothing of '" + name + "'.");
            const auto c = preview.bounds.center();
            spec.pivot = {c[0], c[1], c[2]};
            Body step;
            step.id = doc.allocateId();
            step.name = name;
            step.boolean = std::move(spec);
            made.body = step.id;
            stack.execute(std::make_unique<CombineCommand>(std::move(step)));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

}  // namespace makepart

}  // namespace forge
