// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Paint.h"

#include "forge/core/Document.h"
#include "forge/core/History.h"
#include "forge/core/Sculpt.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>

namespace forge {

namespace paint {

namespace {

std::uint64_t cellKey(std::int64_t i, std::int64_t j, std::int64_t k)
{
    const auto q = [](std::int64_t v) { return std::uint64_t(v + (1 << 20)) & 0x1FFFFF; };
    return q(i) | q(j) << 21 | q(k) << 42;
}

double dist2(const Vec3& a, const std::array<double, 3>& b)
{
    const double x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return x * x + y * y + z * z;
}

// Dabs much bigger than most (Fill All) are kept apart, checked everywhere, so the grid
// stays fine for the brush strokes.
constexpr double kBigFactor = 4.0;

double typicalRadius(const std::vector<PaintDab>& dabs)
{
    if (dabs.empty())
        return 1.0;
    std::vector<double> r;
    r.reserve(dabs.size());
    for (const PaintDab& d : dabs)
        r.push_back(d.radius);
    const std::size_t at = std::min(r.size() - 1, r.size() * 9 / 10);
    std::nth_element(r.begin(), r.begin() + std::ptrdiff_t(at), r.end());
    return r[at];
}

constexpr std::size_t kMostDabs = 2'000'000;
constexpr std::size_t kFineTriangles = 60'000;

}  // namespace

// The big dabs live under a key no cell uses.
constexpr std::uint64_t kBigKey = ~std::uint64_t{0};

Field::Field(const std::vector<PaintDab>& dabs) : m_dabs(dabs)
{
    const double typical = typicalRadius(dabs);
    double biggestSmall = 0.0;
    for (const PaintDab& d : dabs)
        if (d.radius <= kBigFactor * typical)
            biggestSmall = std::max(biggestSmall, d.radius);
    m_cell = std::max(biggestSmall, 1e-3);
    for (std::uint32_t i = 0; i < m_dabs.size(); ++i) {
        const PaintDab& d = m_dabs[i];
        if (d.radius > kBigFactor * typical) {
            m_cells[kBigKey].push_back(i);
            continue;
        }
        std::int64_t lo[3], hi[3];
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::int64_t(std::floor((d.centre[std::size_t(k)] - d.radius) / m_cell));
            hi[k] = std::int64_t(std::floor((d.centre[std::size_t(k)] + d.radius) / m_cell));
        }
        for (std::int64_t a = lo[0]; a <= hi[0]; ++a)
            for (std::int64_t b = lo[1]; b <= hi[1]; ++b)
                for (std::int64_t c = lo[2]; c <= hi[2]; ++c)
                    m_cells[cellKey(a, b, c)].push_back(i);  // in order, so later dabs come later
    }
}

std::uint32_t Field::colourAt(const Vec3& p) const
{
    // The last dab covering p decides: the highest number among the cell's and the big ones.
    std::int64_t last = -1;
    const auto look = [&](std::uint64_t key) {
        const auto found = m_cells.find(key);
        if (found == m_cells.end())
            return;
        const auto& list = found->second;
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            if (std::int64_t(*it) <= last)
                break;
            const PaintDab& d = m_dabs[*it];
            if (dist2(p, d.centre) <= d.radius * d.radius) {
                last = std::int64_t(*it);
                break;
            }
        }
    };
    look(cellKey(std::int64_t(std::floor(p[0] / m_cell)), std::int64_t(std::floor(p[1] / m_cell)),
                 std::int64_t(std::floor(p[2] / m_cell))));
    look(kBigKey);
    if (last < 0)
        return kOwnColour;
    const PaintDab& d = m_dabs[std::size_t(last)];
    return d.erase ? kOwnColour : (d.colour & 0xFFFFFFu);
}

void check(const PaintFinish& f)
{
    if (f.dabs.size() > kMostDabs)
        throw std::invalid_argument("That's more paint than Forge3D can keep: Fill All, then paint over it.");
    for (const PaintDab& d : f.dabs) {
        if (!std::isfinite(d.centre[0]) || !std::isfinite(d.centre[1]) || !std::isfinite(d.centre[2]))
            throw std::invalid_argument("A dab of paint isn't anywhere.");
        if (!(d.radius > 0.0) || !std::isfinite(d.radius))
            throw std::invalid_argument("A dab of paint has no size.");
    }
}

std::vector<std::uint32_t> colours(const std::vector<PaintDab>& dabs)
{
    std::vector<std::uint32_t> out;
    for (const PaintDab& d : dabs)
        if (!d.erase && std::find(out.begin(), out.end(), d.colour & 0xFFFFFFu) == out.end())
            out.push_back(d.colour & 0xFFFFFFu);
    return out;
}

std::string describe(const PaintFinish& f)
{
    const std::size_t n = colours(f.dabs).size();
    if (n == 0)
        return f.dabs.empty() ? "no paint" : "rubbed out";
    return n == 1 ? "1 colour" : std::to_string(n) + " colours";
}

std::vector<std::uint32_t> triangleColours(const std::vector<float>& positions,
                                           const std::vector<std::uint32_t>& indices, const Field& field,
                                           const Transform& toPaint)
{
    const std::size_t n = indices.size() / 3;
    std::vector<std::uint32_t> out(n, kOwnColour);
    if (field.empty())
        return out;
    const auto work = [&](std::size_t from, std::size_t to) {
        for (std::size_t t = from; t < to; ++t) {
            Vec3 c{0, 0, 0};
            for (int k = 0; k < 3; ++k) {
                const std::size_t v = indices[3 * t + std::size_t(k)];
                for (std::size_t a = 0; a < 3; ++a)
                    c[a] += positions[3 * v + a] / 3.0;
            }
            out[t] = field.colourAt(toPaint.apply(c));
        }
    };
    const std::size_t threads = std::clamp<std::size_t>(std::thread::hardware_concurrency(), 1, 16);
    if (n < 50'000 || threads == 1) {
        work(0, n);
        return out;
    }
    std::vector<std::thread> pool;
    const std::size_t chunk = (n + threads - 1) / threads;
    for (std::size_t i = 0; i < threads; ++i) {
        const std::size_t from = i * chunk, to = std::min(n, from + chunk);
        if (from < to)
            pool.emplace_back(work, from, to);
    }
    for (auto& t : pool)
        t.join();
    return out;
}

void colourMesh(Mesh& mesh, const std::vector<std::uint32_t>& triangleColours)
{
    mesh.paint.assign(mesh.vertexCount() * 4, 0.0f);
    for (std::size_t t = 0; t < triangleColours.size() && 3 * t + 2 < mesh.indices.size(); ++t) {
        const std::uint32_t c = triangleColours[t];
        if (c == kOwnColour)
            continue;
        const float rgb[3] = {float((c >> 16) & 0xFF) / 255.f, float((c >> 8) & 0xFF) / 255.f,
                              float(c & 0xFF) / 255.f};
        for (int k = 0; k < 3; ++k) {
            const std::size_t v = mesh.indices[3 * t + std::size_t(k)];
            mesh.paint[4 * v] = rgb[0];
            mesh.paint[4 * v + 1] = rgb[1];
            mesh.paint[4 * v + 2] = rgb[2];
            mesh.paint[4 * v + 3] = 1.0f;
        }
    }
}

Mesh paintSurface(const Mesh& mesh)
{
    if (mesh.triangleCount() >= kFineTriangles || mesh.empty())
        return mesh;
    const Mesh welded = sculpt::welded(mesh, 1e-4);
    return sculpt::refined(welded, sculpt::edgeForTriangles(welded, kFineTriangles), 4 * kFineTriangles);
}

MeshSolid apply(const MeshSolid& solid, const PaintFinish& f)
{
    check(f);
    Mesh m;
    meshkernel::triangles(solid, m.positions, m.indices);
    if (m.triangleCount() >= kFineTriangles)
        return solid;
    const Mesh fine = paintSurface(m);
    return meshkernel::fromTriangles(fine.positions, fine.indices);
}

std::vector<MaskDab> maskFor(const std::vector<PaintDab>& dabs, std::uint32_t colour)
{
    std::vector<MaskDab> out;
    out.reserve(dabs.size());
    for (const PaintDab& d : dabs)
        out.push_back({d.centre, d.radius, d.erase || (d.colour & 0xFFFFFFu) != (colour & 0xFFFFFFu)});
    // Leading rub-outs change nothing.
    const auto first = std::find_if(out.begin(), out.end(), [](const MaskDab& m) { return !m.erase; });
    out.erase(out.begin(), first);
    return out;
}

std::vector<PaintDab> moved(const std::vector<PaintDab>& dabs, const Transform& t)
{
    std::vector<PaintDab> out = dabs;
    for (PaintDab& d : out) {
        const Vec3 c = t.apply(Vec3{d.centre[0], d.centre[1], d.centre[2]});
        d.centre = {c[0], c[1], c[2]};
    }
    return out;
}

std::vector<PaintDab> objectPaint(const Document& doc, ObjectId top)
{
    std::vector<PaintDab> out;
    const Transform toTop = doc.worldTransform(top).inverse();
    for (ObjectId id : history::steps(doc, top)) {
        const Body* b = doc.find(id);
        if (!b || !b->isBoolean() || b->isOff() || b->boolean->op != BooleanOp::Paint || !b->boolean->paint)
            continue;
        const auto dabs = moved(b->boolean->paint->dabs, toTop * doc.worldTransform(id));
        out.insert(out.end(), dabs.begin(), dabs.end());
    }
    return out;
}

}  // namespace paint

namespace {

const Body& paintStep(const Document& doc, ObjectId result)
{
    const Body* body = doc.find(result);
    if (!body || !body->isBoolean() || body->boolean->op != BooleanOp::Paint || !body->boolean->paint)
        throw std::invalid_argument("That isn't a Paint step.");
    return *body;
}

}  // namespace

BooleanSpec paintSpec(const Document& doc, ObjectId target, const std::vector<PaintDab>& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    BooleanSpec spec;
    spec.op = BooleanOp::Paint;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    PaintFinish f;
    f.dabs = paint::moved(inPlate, toStep);
    paint::check(f);
    spec.paint = std::move(f);
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

std::vector<PaintDab> paintInPlate(const Document& doc, ObjectId result)
{
    const Body& body = paintStep(doc, result);
    return paint::moved(body.boolean->paint->dabs, doc.worldTransform(result));
}

Body withPaint(const Document& doc, ObjectId result, const std::vector<PaintDab>& inPlate)
{
    const Body& body = paintStep(doc, result);
    Body next = body;
    next.boolean->paint->dabs = paint::moved(inPlate, doc.worldTransform(result).inverse());
    paint::check(*next.boolean->paint);
    return next;
}

}  // namespace forge
