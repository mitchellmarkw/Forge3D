// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/PrintCheck.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <thread>

namespace forge::printcheck {

namespace {

using V = std::array<double, 3>;

V sub(const V& a, const V& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V cross(const V& a, const V& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const V& a, const V& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double length(const V& a) { return std::sqrt(dot(a, a)); }

V vertex(const Mesh& m, std::uint32_t i)
{
    return {m.positions[3 * std::size_t(i)], m.positions[3 * std::size_t(i) + 1], m.positions[3 * std::size_t(i) + 2]};
}

// Möller–Trumbore; t along the ray, or negative for a miss.
double rayTriangle(const V& o, const V& d, const V& a, const V& b, const V& c)
{
    const V e1 = sub(b, a), e2 = sub(c, a);
    const V pv = cross(d, e2);
    const double det = dot(e1, pv);
    if (std::abs(det) < 1e-14)
        return -1.0;
    const double inv = 1.0 / det;
    const V s = sub(o, a);
    const double u = dot(s, pv) * inv;
    if (u < 0.0 || u > 1.0)
        return -1.0;
    const V q = cross(s, e1);
    const double v = dot(d, q) * inv;
    if (v < 0.0 || u + v > 1.0)
        return -1.0;
    return dot(e2, q) * inv;
}

// The outward normal of each triangle (unit; zero for a sliver), following the mesh's
// own normals if the winding disagrees with them.
std::vector<V> faceNormals(const Mesh& m)
{
    const bool haveNormals = m.normals.size() == m.positions.size();
    std::vector<V> out(m.triangleCount(), V{0, 0, 0});
    for (std::size_t t = 0; t < out.size(); ++t) {
        const std::uint32_t* i = &m.indices[3 * t];
        const V a = vertex(m, i[0]);
        V n = cross(sub(vertex(m, i[1]), a), sub(vertex(m, i[2]), a));
        const double len = length(n);
        if (len < 1e-12)
            continue;
        n = {n[0] / len, n[1] / len, n[2] / len};
        if (haveNormals) {
            V vn{0, 0, 0};
            for (int k = 0; k < 3; ++k)
                for (int c = 0; c < 3; ++c)
                    vn[c] += m.normals[3 * std::size_t(i[k]) + c];
            if (dot(vn, n) < 0.0)
                n = {-n[0], -n[1], -n[2]};
        }
        out[t] = n;
    }
    return out;
}

// Triangles sorted into a grid of equal cubes, to follow a ray through.
struct Grid {
    V origin{0, 0, 0};
    double cell = 1.0;
    std::array<int, 3> n{1, 1, 1};
    std::vector<std::uint32_t> start;  // per cell, into `items` (one extra at the end)
    std::vector<std::uint32_t> items;  // triangle numbers

    std::size_t index(int x, int y, int z) const { return (std::size_t(z) * n[1] + std::size_t(y)) * n[0] + x; }
    int clampAxis(double v, int axis) const
    {
        return std::clamp(int(std::floor((v - origin[axis]) / cell)), 0, n[axis] - 1);
    }
};

Grid buildGrid(const Mesh& m)
{
    Grid g;
    const Bounds b = m.bounds();
    const std::size_t tris = m.triangleCount();
    double area = 0.0;
    for (std::size_t t = 0; t < tris; ++t) {
        const std::uint32_t* i = &m.indices[3 * t];
        const V a = vertex(m, i[0]);
        area += 0.5 * length(cross(sub(vertex(m, i[1]), a), sub(vertex(m, i[2]), a)));
    }
    V size{double(b.max[0] - b.min[0]), double(b.max[1] - b.min[1]), double(b.max[2] - b.min[2])};
    const double biggest = std::max({size[0], size[1], size[2], 1e-3});
    // About two triangles across a cell...
    double cell = std::max(2.0 * std::sqrt(area / double(std::max<std::size_t>(tris, 1))), biggest / 512.0);
    // ...but not more than about four million cells.
    const auto cells = [&](double c) {
        return (std::floor(size[0] / c) + 1) * (std::floor(size[1] / c) + 1) * (std::floor(size[2] / c) + 1);
    };
    while (cells(cell) > 4.0e6)
        cell *= 1.25;
    g.cell = cell;
    g.origin = {double(b.min[0]), double(b.min[1]), double(b.min[2])};
    for (int a = 0; a < 3; ++a)
        g.n[a] = int(std::floor(size[a] / cell)) + 1;
    const std::size_t count = std::size_t(g.n[0]) * g.n[1] * g.n[2];
    std::vector<std::uint32_t> perCell(count + 1, 0);
    const auto box = [&](std::size_t t, std::array<int, 3>& lo, std::array<int, 3>& hi) {
        const std::uint32_t* i = &m.indices[3 * t];
        for (int a = 0; a < 3; ++a) {
            double mn = 1e300, mx = -1e300;
            for (int k = 0; k < 3; ++k) {
                const double v = m.positions[3 * std::size_t(i[k]) + a];
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
            lo[a] = g.clampAxis(mn, a);
            hi[a] = g.clampAxis(mx, a);
        }
    };
    std::array<int, 3> lo{}, hi{};
    for (std::size_t t = 0; t < tris; ++t) {
        box(t, lo, hi);
        for (int z = lo[2]; z <= hi[2]; ++z)
            for (int y = lo[1]; y <= hi[1]; ++y)
                for (int x = lo[0]; x <= hi[0]; ++x)
                    ++perCell[g.index(x, y, z) + 1];
    }
    for (std::size_t c = 0; c < count; ++c)
        perCell[c + 1] += perCell[c];
    g.start = perCell;
    g.items.resize(perCell[count]);
    for (std::size_t t = 0; t < tris; ++t) {
        box(t, lo, hi);
        for (int z = lo[2]; z <= hi[2]; ++z)
            for (int y = lo[1]; y <= hi[1]; ++y)
                for (int x = lo[0]; x <= hi[0]; ++x)
                    g.items[perCell[g.index(x, y, z)]++] = std::uint32_t(t);
    }
    return g;
}

// The nearest triangle (not `self`) along the ray within `upTo`; kNoWall if none.
double cast(const Mesh& m, const Grid& g, const V& o, const V& d, double upTo, std::size_t self)
{
    std::array<int, 3> cell{}, step{};
    V tMax{}, tDelta{};
    for (int a = 0; a < 3; ++a) {
        cell[a] = g.clampAxis(o[a], a);
        if (d[a] > 1e-12) {
            step[a] = 1;
            tMax[a] = (g.origin[a] + (cell[a] + 1) * g.cell - o[a]) / d[a];
            tDelta[a] = g.cell / d[a];
        } else if (d[a] < -1e-12) {
            step[a] = -1;
            tMax[a] = (g.origin[a] + cell[a] * g.cell - o[a]) / d[a];
            tDelta[a] = -g.cell / d[a];
        } else {
            step[a] = 0;
            tMax[a] = 1e300;
            tDelta[a] = 1e300;
        }
    }
    double best = 1e300;
    double tCell = 0.0;  // where the ray came into this cell
    while (tCell <= upTo && tCell < best) {
        const std::size_t c = g.index(cell[0], cell[1], cell[2]);
        for (std::uint32_t k = g.start[c]; k < g.start[c + 1]; ++k) {
            const std::uint32_t t = g.items[k];
            if (t == self)
                continue;
            const std::uint32_t* i = &m.indices[3 * std::size_t(t)];
            const double hit = rayTriangle(o, d, vertex(m, i[0]), vertex(m, i[1]), vertex(m, i[2]));
            if (hit > 1e-6 && hit < best)
                best = hit;
        }
        // On to the next cell.
        int a = 0;
        if (tMax[1] < tMax[a])
            a = 1;
        if (tMax[2] < tMax[a])
            a = 2;
        if (step[a] == 0)
            break;
        tCell = tMax[a];
        cell[a] += step[a];
        if (cell[a] < 0 || cell[a] >= g.n[a])
            break;
        tMax[a] += tDelta[a];
    }
    return best <= upTo ? best : double(kNoWall);
}

}  // namespace

std::vector<float> faceThickness(const Mesh& mesh, double upTo)
{
    const std::size_t tris = mesh.triangleCount();
    std::vector<float> out(tris, kNoWall);
    if (tris == 0)
        return out;
    const std::vector<V> normals = faceNormals(mesh);
    const Grid grid = buildGrid(mesh);
    const auto work = [&](std::size_t from, std::size_t to) {
        for (std::size_t t = from; t < to; ++t) {
            const V& n = normals[t];
            if (n[0] == 0.0 && n[1] == 0.0 && n[2] == 0.0)
                continue;
            const std::uint32_t* i = &mesh.indices[3 * t];
            const V a = vertex(mesh, i[0]), b = vertex(mesh, i[1]), c = vertex(mesh, i[2]);
            const V middle{(a[0] + b[0] + c[0]) / 3.0, (a[1] + b[1] + c[1]) / 3.0, (a[2] + b[2] + c[2]) / 3.0};
            out[t] = float(cast(mesh, grid, middle, {-n[0], -n[1], -n[2]}, upTo, t));
        }
    };
    const std::size_t threads =
        tris < 20000 ? 1 : std::clamp<std::size_t>(std::thread::hardware_concurrency(), 1, 32);
    if (threads == 1) {
        work(0, tris);
        return out;
    }
    std::vector<std::thread> pool;
    const std::size_t chunk = (tris + threads - 1) / threads;
    for (std::size_t k = 0; k < threads; ++k) {
        const std::size_t from = k * chunk, to = std::min(tris, from + chunk);
        if (from < to)
            pool.emplace_back(work, from, to);
    }
    for (auto& th : pool)
        th.join();
    return out;
}

Report classify(const Mesh& mesh, const std::vector<float>& thickness, const Transform& place, const Settings& s,
                Mesh* shown)
{
    Report r;
    const std::size_t tris = mesh.triangleCount();
    const std::vector<V> normals = faceNormals(mesh);
    const bool haveThickness = thickness.size() == tris;
    // How low the part goes on the plate: faces lying there rest on it.
    double bottom = 1e300;
    std::vector<double> plateZ(mesh.vertexCount());
    for (std::size_t v = 0; v < plateZ.size(); ++v) {
        plateZ[v] = place.apply(vertex(mesh, std::uint32_t(v)))[2];
        bottom = std::min(bottom, plateZ[v]);
    }
    const double leanLimit = -std::sin(std::clamp(s.angle, 0.0, 90.0) * std::numbers::pi / 180.0);
    const bool haveNormals = mesh.normals.size() == mesh.positions.size();
    if (shown) {
        *shown = Mesh{};
        shown->positions.reserve(9 * tris);
        shown->normals.reserve(9 * tris);
        shown->indices.reserve(3 * tris);
        shown->shade.reserve(3 * tris);
    }
    for (std::size_t t = 0; t < tris; ++t) {
        const std::uint32_t* i = &mesh.indices[3 * t];
        const V a = vertex(mesh, i[0]);
        const double area = 0.5 * length(cross(sub(vertex(mesh, i[1]), a), sub(vertex(mesh, i[2]), a)));
        r.area += area;
        float shade = kShadeOk;
        if (s.overhangs) {
            const V n = place.applyDirection(normals[t]);
            const bool resting = std::max({plateZ[i[0]], plateZ[i[1]], plateZ[i[2]]}) < bottom + 0.05;
            if (n[2] < leanLimit && !resting) {
                r.overhangArea += area;
                shade = kShadeOverhang;
            }
        }
        if (haveThickness && thickness[t] != kNoWall) {
            r.thinnest = std::min(r.thinnest, double(thickness[t]));
            if (s.thin && thickness[t] < s.minWall) {
                r.thinArea += area;
                shade = kShadeThin;
            }
        }
        if (shown) {
            for (int k = 0; k < 3; ++k) {
                for (int c = 0; c < 3; ++c) {
                    shown->positions.push_back(mesh.positions[3 * std::size_t(i[k]) + c]);
                    shown->normals.push_back(haveNormals ? mesh.normals[3 * std::size_t(i[k]) + c]
                                                         : float(normals[t][c]));
                }
                shown->indices.push_back(std::uint32_t(3 * t + k));
                shown->shade.push_back(shade);
            }
        }
    }
    return r;
}

}  // namespace forge::printcheck
