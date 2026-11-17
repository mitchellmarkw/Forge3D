// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/MeshSteps.h"

#include "forge/core/Remesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <unordered_map>
#include <stdexcept>
#include <utility>

namespace forge::meshsteps {

namespace {

double diagonalOf(const Mesh& m)
{
    const Bounds b = m.bounds();
    const auto s = b.size();
    return std::sqrt(double(s[0]) * s[0] + double(s[1]) * s[1] + double(s[2]) * s[2]);
}

double meanEdge(const Mesh& m)
{
    double sum = 0.0;
    std::size_t n = 0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t a = m.indices[t + k], b = m.indices[t + (k + 1) % 3];
            const double dx = m.positions[a * 3] - m.positions[b * 3], dy = m.positions[a * 3 + 1] - m.positions[b * 3 + 1],
                         dz = m.positions[a * 3 + 2] - m.positions[b * 3 + 2];
            sum += std::sqrt(dx * dx + dy * dy + dz * dz);
            ++n;
        }
    return n ? sum / double(n) : 0.0;
}

// A closed solid under a patch of surface: the patch turned over on top (lifted `lift`
// along up), its shadow on the plane (pushed `under` past it) below, and walls round
// its edge. Corners of the same point in neighbouring triangles are one point of the
// solid; where two parts of the patch touch only at a point, that point is split in
// two, so the solid never pinches.
Mesh solidBelow(const Mesh& patch, const Vec3& planePoint, const Vec3& up, double lift, double under)
{
    const std::size_t tris = patch.indices.size() / 3;
    const auto key = [](std::uint32_t a, std::uint32_t b) { return (std::uint64_t(a) << 32) | b; };
    std::unordered_map<std::uint64_t, std::uint32_t> edgeOf;  // directed edge -> triangle
    edgeOf.reserve(tris * 3);
    for (std::size_t t = 0; t < tris; ++t)
        for (int k = 0; k < 3; ++k)
            edgeOf[key(patch.indices[t * 3 + k], patch.indices[t * 3 + (k + 1) % 3])] = std::uint32_t(t);
    std::vector<std::uint32_t> parent(tris * 3);
    for (std::uint32_t i = 0; i < parent.size(); ++i)
        parent[i] = i;
    const auto find = [&parent](std::uint32_t x) {
        while (parent[x] != x)
            x = parent[x] = parent[parent[x]];
        return x;
    };
    std::vector<std::pair<std::uint32_t, std::uint32_t>> rim;  // edges round the outside, as corners
    for (std::uint32_t i = 0; i < tris; ++i) {
        const std::uint32_t* f = &patch.indices[std::size_t(i) * 3];
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t a = f[k], b = f[(k + 1) % 3];
            const auto other = edgeOf.find(key(b, a));
            if (other == edgeOf.end()) {
                rim.emplace_back(i * 3 + std::uint32_t(k), i * 3 + std::uint32_t((k + 1) % 3));
                continue;
            }
            const std::uint32_t j = other->second;
            const std::uint32_t* g = &patch.indices[std::size_t(j) * 3];
            for (int m = 0; m < 3; ++m) {
                if (g[m] == a)
                    parent[find(i * 3 + std::uint32_t(k))] = find(j * 3 + std::uint32_t(m));
                if (g[m] == b)
                    parent[find(i * 3 + std::uint32_t((k + 1) % 3))] = find(j * 3 + std::uint32_t(m));
            }
        }
    }
    std::unordered_map<std::uint32_t, std::uint32_t> local;  // root corner -> point of the solid
    std::vector<std::uint32_t> source;                       // point of the solid -> patch point
    const auto point = [&](std::uint32_t corner) {
        auto [it, added] = local.try_emplace(find(corner), std::uint32_t(source.size()));
        if (added)
            source.push_back(patch.indices[corner]);
        return it->second;
    };
    std::vector<std::array<std::uint32_t, 3>> faces;
    for (std::uint32_t i = 0; i < tris; ++i)
        faces.push_back({point(i * 3), point(i * 3 + 1), point(i * 3 + 2)});
    const std::uint32_t n = std::uint32_t(source.size());
    Mesh out;
    out.positions.assign(std::size_t(n) * 6, 0.0f);
    for (std::uint32_t i = 0; i < n; ++i) {
        const float* p = &patch.positions[std::size_t(source[i]) * 3];
        const double h = (p[0] - planePoint[0]) * up[0] + (p[1] - planePoint[1]) * up[1] + (p[2] - planePoint[2]) * up[2];
        for (int c = 0; c < 3; ++c) {
            out.positions[std::size_t(i) * 3 + c] = float(p[c] + lift * up[c]);
            out.positions[std::size_t(n + i) * 3 + c] = float(p[c] - (h + under) * up[c]);
        }
    }
    for (const auto& f : faces) {
        out.indices.insert(out.indices.end(), {f[0], f[2], f[1]});              // top, turned over
        out.indices.insert(out.indices.end(), {n + f[0], n + f[1], n + f[2]});  // bottom, facing down
    }
    for (const auto& [cp, cq] : rim) {
        const std::uint32_t p = point(cp), q = point(cq);
        out.indices.insert(out.indices.end(), {p, q, n + q, p, n + q, n + p});
    }
    return out;
}

// Solids whose boxes are well apart, put together as one mesh (several separate pieces):
// a curved underside breaks into thousands of small patches, and merging thousands of
// solids one by one is slow. Solids that come near each other stay in different meshes.
std::vector<Mesh> packed(std::vector<Mesh> solids, double margin)
{
    std::sort(solids.begin(), solids.end(),
              [](const Mesh& a, const Mesh& b) { return a.indices.size() > b.indices.size(); });
    struct Pack {
        Mesh mesh;
        std::vector<Bounds> boxes;
    };
    std::vector<Pack> packs;
    const auto apart = [margin](const Bounds& a, const Bounds& b) {
        for (int c = 0; c < 3; ++c)
            if (a.max[c] + margin < b.min[c] || b.max[c] + margin < a.min[c])
                return true;
        return false;
    };
    for (Mesh& solid : solids) {
        const Bounds box = solid.bounds();
        Pack* home = nullptr;
        for (Pack& p : packs)
            if (std::all_of(p.boxes.begin(), p.boxes.end(), [&](const Bounds& b) { return apart(box, b); })) {
                home = &p;
                break;
            }
        if (!home) {
            packs.emplace_back();
            home = &packs.back();
        }
        const auto base = std::uint32_t(home->mesh.positions.size() / 3);
        home->mesh.positions.insert(home->mesh.positions.end(), solid.positions.begin(), solid.positions.end());
        for (std::uint32_t i : solid.indices)
            home->mesh.indices.push_back(base + i);
        home->boxes.push_back(box);
    }
    std::vector<Mesh> out;
    for (Pack& p : packs)
        out.push_back(std::move(p.mesh));
    return out;
}

// The same triangles with corners at the same place made one point (a mesh from the
// mesh engine can repeat a point where its faces meet), and triangles that collapsed
// to a line or a point left out.
Mesh welded(const Mesh& m)
{
    Mesh out;
    std::map<std::array<float, 3>, std::uint32_t> at;
    std::vector<std::uint32_t> remap(m.positions.size() / 3);
    for (std::size_t v = 0; v < remap.size(); ++v) {
        const std::array<float, 3> p{m.positions[v * 3], m.positions[v * 3 + 1], m.positions[v * 3 + 2]};
        auto [it, added] = at.try_emplace(p, std::uint32_t(out.positions.size() / 3));
        if (added)
            out.positions.insert(out.positions.end(), p.begin(), p.end());
        remap[v] = it->second;
    }
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const std::uint32_t a = remap[m.indices[t]], b = remap[m.indices[t + 1]], c = remap[m.indices[t + 2]];
        if (a != b && b != c && a != c)
            out.indices.insert(out.indices.end(), {a, b, c});
    }
    return out;
}

}  // namespace

namespace {

// `rounds` times, every point moved half way to the average of its neighbours. Points on
// open edges stay put. Returns false if there were any (the surface isn't closed).
bool relax(Mesh& m, int rounds)
{
    const std::size_t n = m.positions.size() / 3;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
    edges.reserve(m.indices.size());
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
        for (int k = 0; k < 3; ++k)
            edges.emplace_back(m.indices[t + k], m.indices[t + (k + 1) % 3]);
    std::vector<char> pinned(n, 0);
    bool closedSurface = true;
    {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> sorted = edges;
        for (auto& e : sorted)
            if (e.first > e.second)
                std::swap(e.first, e.second);
        std::sort(sorted.begin(), sorted.end());
        for (std::size_t i = 0; i < sorted.size();) {
            std::size_t j = i;
            while (j < sorted.size() && sorted[j] == sorted[i])
                ++j;
            if (j - i != 2) {
                pinned[sorted[i].first] = pinned[sorted[i].second] = 1;
                closedSurface = false;
            }
            i = j;
        }
    }
    // Each directed edge a->b makes b a neighbour of a; on a closed surface every edge
    // shows up both ways, so every neighbour is counted once.
    std::vector<std::uint32_t> start(n + 1, 0), list(edges.size());
    for (const auto& e : edges)
        ++start[e.first + 1];
    for (std::size_t i = 0; i < n; ++i)
        start[i + 1] += start[i];
    {
        std::vector<std::uint32_t> fill(start.begin(), start.end() - 1);
        for (const auto& e : edges)
            list[fill[e.first]++] = e.second;
    }
    std::vector<double> p(m.positions.begin(), m.positions.end()), moved(p.size());
    for (int r = 0; r < rounds; ++r) {
        for (std::size_t v = 0; v < n; ++v) {
            const std::uint32_t from = start[v], to = start[v + 1];
            if (pinned[v] || from == to) {
                for (int c = 0; c < 3; ++c)
                    moved[v * 3 + c] = p[v * 3 + c];
                continue;
            }
            double avg[3] = {0, 0, 0};
            for (std::uint32_t k = from; k < to; ++k)
                for (int c = 0; c < 3; ++c)
                    avg[c] += p[std::size_t(list[k]) * 3 + c];
            for (int c = 0; c < 3; ++c)
                moved[v * 3 + c] = 0.5 * p[v * 3 + c] + 0.5 * avg[c] / double(to - from);
        }
        std::swap(p, moved);
    }
    m.positions.assign(p.begin(), p.end());
    return closedSurface;
}

double volumeOf(const Mesh& m)
{
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &m.positions[std::size_t(m.indices[t]) * 3];
        const float* b = &m.positions[std::size_t(m.indices[t + 1]) * 3];
        const float* c = &m.positions[std::size_t(m.indices[t + 2]) * 3];
        v += double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) -
             double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
             double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0]);
    }
    return v / 6.0;
}

}  // namespace

Mesh smoothed(const Mesh& closed, int strength)
{
    if (strength < 1 || strength > 10)
        throw std::invalid_argument("Smooth needs a strength from 1 to 10");
    if (closed.indices.empty())
        return closed;
    Mesh m = welded(closed);
    const double diag = diagonalOf(m);
    const double before = volumeOf(m);
    // Small even triangles first (a box's 12 triangles have nothing to smooth with), and
    // evened out again every few rounds: points drift along the surface as it's smoothed,
    // bunching up in places, and bunched points make lumps.
    const bool remesh = m.triangleCount() < 150'000;
    const double edge = std::max(std::min(diag / 60.0, std::max(meanEdge(m), 1e-6)), diag / 400.0);
    if (remesh)
        m = remesh::remeshed(m, edge, 3, 1'500'000);
    // Rounding reaches about `reach` in from edges and corners; spreading one triangle's
    // width per round, that takes about (reach / edge)^2 rounds.
    const double reach = double(strength) * diag / 60.0;
    const double meanNow = std::max(meanEdge(m), 1e-9);
    int rounds = int(std::clamp(std::lround((reach / meanNow) * (reach / meanNow)), long(strength), 300L));
    bool closedSurface = true;
    while (rounds > 0) {
        const int now = std::min(rounds, 12);
        closedSurface = relax(m, now) && closedSurface;
        rounds -= now;
        if (rounds > 0 && remesh)
            m = remesh::remeshed(m, edge, 2, 1'500'000);
    }
    // Smoothing shrinks a shape: grow it back about its middle to the volume it had.
    const double after = volumeOf(m);
    if (closedSurface && before > 0.0 && after > 0.0) {
        const std::size_t n = m.positions.size() / 3;
        double centre[3] = {0, 0, 0};
        for (std::size_t v = 0; v < n; ++v)
            for (int c = 0; c < 3; ++c)
                centre[c] += m.positions[v * 3 + c] / double(n);
        const double k = std::cbrt(before / after);
        for (std::size_t v = 0; v < n; ++v)
            for (int c = 0; c < 3; ++c)
                m.positions[v * 3 + c] = float(centre[c] + (m.positions[v * 3 + c] - centre[c]) * k);
    }
    Mesh out;
    out.positions = std::move(m.positions);
    out.indices = std::move(m.indices);
    return out;
}

std::vector<Mesh> columnsBelow(const Mesh& closed, const Vec3& planePoint, const Vec3& upIn)
{
    const double len = std::sqrt(upIn[0] * upIn[0] + upIn[1] * upIn[1] + upIn[2] * upIn[2]);
    if (!(len > 1e-12))
        throw std::invalid_argument("Extrude Down needs a direction");
    const Vec3 up{upIn[0] / len, upIn[1] / len, upIn[2] / len};
    // Columns reach this far below the plane, so none is flat (zero tall) anywhere.
    const double tiny = std::max(diagonalOf(closed) * 1e-6, 1e-6);
    const std::size_t points = closed.positions.size() / 3;
    const std::size_t tris = closed.indices.size() / 3;
    std::vector<double> height(points);
    for (std::size_t v = 0; v < points; ++v)
        height[v] = (closed.positions[v * 3] - planePoint[0]) * up[0] +
                    (closed.positions[v * 3 + 1] - planePoint[1]) * up[1] +
                    (closed.positions[v * 3 + 2] - planePoint[2]) * up[2];

    // The triangles that face down and are above the plane (not lying on it).
    std::vector<char> down(tris, 0);
    for (std::size_t t = 0; t < tris; ++t) {
        const std::uint32_t a = closed.indices[t * 3], b = closed.indices[t * 3 + 1], c = closed.indices[t * 3 + 2];
        Vec3 va, vb, vc;
        for (int k = 0; k < 3; ++k) {
            va[k] = closed.positions[a * 3 + k];
            vb[k] = closed.positions[b * 3 + k];
            vc[k] = closed.positions[c * 3 + k];
        }
        const Vec3 e1{vb[0] - va[0], vb[1] - va[1], vb[2] - va[2]};
        const Vec3 e2{vc[0] - va[0], vc[1] - va[1], vc[2] - va[2]};
        const Vec3 n{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const double area2 = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        const double facing = (n[0] * up[0] + n[1] * up[1] + n[2] * up[2]) / std::max(area2, 1e-300);
        const double lo = std::min({height[a], height[b], height[c]});
        const double hi = std::max({height[a], height[b], height[c]});
        down[t] = facing < -1e-3 && hi > tiny && lo > -tiny / 2 ? 1 : 0;
    }

    // Which triangle is across each edge (closed surfaces: one each way).
    std::unordered_map<std::uint64_t, std::uint32_t> across;  // directed edge a->b -> its triangle
    across.reserve(tris * 3);
    const auto key = [](std::uint32_t a, std::uint32_t b) { return (std::uint64_t(a) << 32) | b; };
    for (std::size_t t = 0; t < tris; ++t)
        if (down[t])
            for (int k = 0; k < 3; ++k)
                across[key(closed.indices[t * 3 + k], closed.indices[t * 3 + (k + 1) % 3])] = std::uint32_t(t);

    // Each connected patch of down-facing triangles becomes one solid (far quicker to
    // merge than a column per triangle).
    std::vector<Mesh> out;
    std::vector<char> seen(tris, 0);
    for (std::size_t start = 0; start < tris; ++start) {
        if (!down[start] || seen[start])
            continue;
        std::vector<std::uint32_t> group{std::uint32_t(start)}, todo{std::uint32_t(start)};
        seen[start] = 1;
        while (!todo.empty()) {
            const std::uint32_t t = todo.back();
            todo.pop_back();
            for (int k = 0; k < 3; ++k) {
                const auto other = across.find(
                    key(closed.indices[std::size_t(t) * 3 + (k + 1) % 3], closed.indices[std::size_t(t) * 3 + k]));
                if (other != across.end() && !seen[other->second]) {
                    seen[other->second] = 1;
                    group.push_back(other->second);
                    todo.push_back(other->second);
                }
            }
        }
        // The patch on its own.
        Mesh patch;
        {
            std::unordered_map<std::uint32_t, std::uint32_t> local;
            for (std::uint32_t t : group)
                for (int k = 0; k < 3; ++k) {
                    const std::uint32_t v = closed.indices[std::size_t(t) * 3 + k];
                    auto [it, added] = local.try_emplace(v, std::uint32_t(local.size()));
                    if (added)
                        for (int c = 0; c < 3; ++c)
                            patch.positions.push_back(closed.positions[std::size_t(v) * 3 + c]);
                    patch.indices.push_back(it->second);
                }
        }
        // A big patch (a dense scan's underside) is thinned first, and its top lifted
        // into the shape by a little more than the thinning moved it, so merging stays
        // quick and leaves no gap.
        double lift = tiny;
        constexpr std::size_t kMostTriangles = 40'000;
        if (patch.triangleCount() > kMostTriangles) {
            lift = 2.0 * meanEdge(patch);
            patch = remesh::simplified(patch, kMostTriangles);
        }
        out.push_back(solidBelow(patch, planePoint, up, lift, tiny));
    }
    return packed(std::move(out), 2.0 * tiny);
}

}  // namespace forge::meshsteps
