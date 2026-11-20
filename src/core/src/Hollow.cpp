// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Hollow.h"

#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace forge {
namespace hollow {

namespace {

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 1e-12 ? mul(a, 1.0 / l) : Vec3{0, 0, -1};
}

std::string num(double v)
{
    std::ostringstream s;
    s.precision(3);
    s << v;
    return s.str();
}

constexpr double kMaxCells = 16.0e6;
constexpr float kFar = 1e20f;

// Felzenszwalb & Huttenlocher's 1D squared distance transform: d[i] = min_j (i-j)² + f[j],
// over the j where f[j] is set (not kFar).
void distance1d(const float* f, float* d, int n, std::vector<int>& v, std::vector<float>& z)
{
    int k = -1;
    for (int q = 0; q < n; ++q) {
        if (f[q] >= kFar)
            continue;
        while (true) {
            if (k < 0) {
                k = 0;
                v[0] = q;
                z[0] = -kFar;
                z[1] = kFar;
                break;
            }
            const int p = v[std::size_t(k)];
            const float s = ((f[q] + float(q) * float(q)) - (f[p] + float(p) * float(p))) / (2.0f * float(q - p));
            if (s <= z[std::size_t(k)]) {
                --k;
                continue;
            }
            ++k;
            v[std::size_t(k)] = q;
            z[std::size_t(k)] = s;
            z[std::size_t(k) + 1] = kFar;
            break;
        }
    }
    if (k < 0) {
        for (int q = 0; q < n; ++q)
            d[q] = kFar;
        return;
    }
    k = 0;
    for (int q = 0; q < n; ++q) {
        while (z[std::size_t(k) + 1] < float(q))
            ++k;
        const int p = v[std::size_t(k)];
        d[q] = float(q - p) * float(q - p) + f[p];
    }
}

// Runs `work(from, to)` over [0, n) on the machine's cores.
template <class F>
void parallel(int n, F work)
{
    const int threads = std::clamp(int(std::thread::hardware_concurrency()), 1, 32);
    if (threads == 1 || n < 64) {
        work(0, n);
        return;
    }
    std::vector<std::thread> pool;
    const int chunk = (n + threads - 1) / threads;
    for (int t = 0; t < threads; ++t) {
        const int from = t * chunk, to = std::min(n, from + chunk);
        if (from < to)
            pool.emplace_back(work, from, to);
    }
    for (auto& th : pool)
        th.join();
}

}  // namespace

void check(const HollowFinish& f)
{
    if (!(f.thickness >= 0.5 && f.thickness <= 20.0))
        throw std::invalid_argument("The walls must be from 0.5 to 20 mm thick.");
    if (!(f.holeSize >= 0.5 && f.holeSize <= 30.0))
        throw std::invalid_argument("Drain holes must be from 0.5 to 30 mm across.");
    if (f.holes.size() > 20)
        throw std::invalid_argument("That's too many drain holes (20 at most).");
    for (const DrainHole& h : f.holes)
        for (int k = 0; k < 3; ++k)
            if (!std::isfinite(h.point[std::size_t(k)]) || !std::isfinite(h.normal[std::size_t(k)]))
                throw std::invalid_argument("A drain hole's spot is broken.");
}

std::string describe(const HollowFinish& f)
{
    std::string s = num(f.thickness) + " mm walls";
    if (f.holes.empty())
        return s + ", no drain holes";
    return s + ", " + std::to_string(f.holes.size()) + (f.holes.size() == 1 ? " drain hole" : " drain holes") +
           " \xC3\x98" + num(f.holeSize);
}

double cellFor(const Bounds& box, double thickness)
{
    double h = std::clamp(thickness / 3.0, 0.1, 2.0);
    if (!box.valid)
        return h;
    const auto cells = [&box](double c) {
        double n = 1;
        for (int a = 0; a < 3; ++a)
            n *= (double(box.max[a]) - double(box.min[a])) / c + 5.0;
        return n;
    };
    while (cells(h) > kMaxCells)
        h *= 1.15;
    return h;
}

MeshSolid hollowed(const MeshSolid& solid, double thickness)
{
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(solid, pos, idx);
    const Bounds box = meshkernel::bounds(solid);
    if (!box.valid || idx.empty())
        return solid;
    const double h = cellFor(box, thickness);
    const Vec3 o{box.min[0] - 2 * h, box.min[1] - 2 * h, box.min[2] - 2 * h};
    int n[3];
    for (int a = 0; a < 3; ++a)
        n[a] = int(std::ceil((double(box.max[a]) - double(box.min[a])) / h)) + 5;
    const std::size_t nx = std::size_t(n[0]), ny = std::size_t(n[1]), nz = std::size_t(n[2]);
    const auto at = [nx, ny](std::size_t x, std::size_t y, std::size_t z) { return (z * ny + y) * nx + x; };

    // 1. Inside or out, a column at a time: a ray up each column, counting crossings.
    std::vector<std::vector<std::uint32_t>> bins(nx * ny);
    const std::size_t tris = idx.size() / 3;
    for (std::size_t t = 0; t < tris; ++t) {
        double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
        for (int k = 0; k < 3; ++k)
            for (int a = 0; a < 2; ++a) {
                const double v = pos[3 * std::size_t(idx[3 * t + std::size_t(k)]) + std::size_t(a)];
                lo[a] = std::min(lo[a], v);
                hi[a] = std::max(hi[a], v);
            }
        const int x0 = std::max(0, int(std::floor((lo[0] - o[0]) / h - 0.5)));
        const int x1 = std::min(n[0] - 1, int(std::ceil((hi[0] - o[0]) / h - 0.5)));
        const int y0 = std::max(0, int(std::floor((lo[1] - o[1]) / h - 0.5)));
        const int y1 = std::min(n[1] - 1, int(std::ceil((hi[1] - o[1]) / h - 0.5)));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                bins[std::size_t(y) * nx + std::size_t(x)].push_back(std::uint32_t(t));
    }
    // Squared distance (in cells) to the nearest outside cell: 0 outside, far inside.
    std::vector<float> grid(nx * ny * nz, 0.0f);
    parallel(int(ny), [&](int y0, int y1) {
        std::vector<double> hits;
        for (int y = y0; y < y1; ++y)
            for (std::size_t x = 0; x < nx; ++x) {
                // A little off the cell's middle, so the ray doesn't run along an edge.
                const double px = o[0] + (double(x) + 0.5) * h + 1.234567e-5 * h;
                const double py = o[1] + (double(y) + 0.5) * h + 2.345678e-5 * h;
                hits.clear();
                for (std::uint32_t t : bins[std::size_t(y) * nx + x]) {
                    const float* a = &pos[3 * std::size_t(idx[3 * std::size_t(t)])];
                    const float* b = &pos[3 * std::size_t(idx[3 * std::size_t(t) + 1])];
                    const float* c = &pos[3 * std::size_t(idx[3 * std::size_t(t) + 2])];
                    const double d = (double(b[1]) - c[1]) * (double(a[0]) - c[0]) +
                                     (double(c[0]) - b[0]) * (double(a[1]) - c[1]);
                    if (std::abs(d) < 1e-18)
                        continue;
                    const double u = ((double(b[1]) - c[1]) * (px - c[0]) + (double(c[0]) - b[0]) * (py - c[1])) / d;
                    const double v = ((double(c[1]) - a[1]) * (px - c[0]) + (double(a[0]) - c[0]) * (py - c[1])) / d;
                    const double w = 1.0 - u - v;
                    if (u < 0 || v < 0 || w < 0)
                        continue;
                    hits.push_back(u * a[2] + v * b[2] + w * c[2]);
                }
                if (hits.size() < 2)
                    continue;
                std::sort(hits.begin(), hits.end());
                std::size_t k = 0;
                for (std::size_t z = 0; z < nz; ++z) {
                    const double pz = o[2] + (double(z) + 0.5) * h;
                    while (k < hits.size() && hits[k] < pz)
                        ++k;
                    if (k % 2 == 1)
                        grid[at(x, std::size_t(y), z)] = kFar;
                }
            }
    });
    // 2. The distance transform, along X, then Y, then Z.
    const auto pass = [&](int axis) {
        const std::size_t len = axis == 0 ? nx : axis == 1 ? ny : nz;
        const std::size_t other1 = axis == 0 ? ny : nx;
        const std::size_t other2 = axis == 2 ? ny : nz;
        parallel(int(other2), [&](int from, int to) {
            std::vector<float> f(len), d(len), zs(len + 1);
            std::vector<int> v(len);
            for (int b = from; b < to; ++b)
                for (std::size_t a = 0; a < other1; ++a) {
                    const auto cell = [&](std::size_t i) {
                        return axis == 0 ? at(i, a, std::size_t(b))
                               : axis == 1 ? at(a, i, std::size_t(b))
                                           : at(a, std::size_t(b), i);
                    };
                    for (std::size_t i = 0; i < len; ++i)
                        f[i] = grid[cell(i)];
                    distance1d(f.data(), d.data(), int(len), v, zs);
                    for (std::size_t i = 0; i < len; ++i)
                        grid[cell(i)] = d[i];
                }
        });
    };
    pass(0);
    pass(1);
    pass(2);
    // 3. Distance to the surface, mm (about: a cell's middle is half a cell from it).
    bool deep = false;
    for (float& g : grid) {
        g = g > 0.0f ? float(std::sqrt(double(g)) * h - 0.5 * h) : float(-0.5 * h);
        deep = deep || g > thickness + 0.5 * h;
    }
    if (!deep)
        return solid;  // nowhere thick enough to hollow
    // 4. The inside: where it's further than the wall from the surface.
    const auto field = [&](const Vec3& p) -> double {
        double c[3];
        int i[3];
        for (int a = 0; a < 3; ++a) {
            c[a] = (p[std::size_t(a)] - o[std::size_t(a)]) / h - 0.5;
            i[a] = int(std::floor(c[a]));
            if (i[a] < 0 || i[a] >= n[a] - 1)
                return -h;
            c[a] -= i[a];
        }
        double v = 0.0;
        for (int k = 0; k < 8; ++k) {
            const int dx = k & 1, dy = (k >> 1) & 1, dz = (k >> 2) & 1;
            const double w = (dx ? c[0] : 1 - c[0]) * (dy ? c[1] : 1 - c[1]) * (dz ? c[2] : 1 - c[2]);
            v += w * grid[at(std::size_t(i[0] + dx), std::size_t(i[1] + dy), std::size_t(i[2] + dz))];
        }
        return v;
    };
    Bounds inner;
    inner.expand(float(o[0]), float(o[1]), float(o[2]));
    inner.expand(float(o[0] + double(nx) * h), float(o[1] + double(ny) * h), float(o[2] + double(nz) * h));
    // The inside's surface needn't be fine (it's never seen): a coarser one is much quicker
    // to make and to cut, and makes a lighter file.
    const double edge = std::max(2.0 * h, std::min(thickness, 1.5));
    const MeshSolid cavity = meshkernel::levelSet(field, inner, edge, thickness);
    if (meshkernel::isEmpty(cavity))
        return solid;
    return meshkernel::subtracted(solid, {cavity});
}

Mesh holeCutter(const DrainHole& hole, double size, double thickness)
{
    const Vec3 n = unit(hole.normal);
    const Vec3 side = unit(cross(n, std::abs(n[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}));
    const Vec3 other = cross(n, side);
    const Vec3 top = add(hole.point, mul(n, 1.0));
    const Vec3 bottom = sub(hole.point, mul(n, thickness * 1.5 + 1.0));
    constexpr int kSides = 24;
    const double r = size / 2.0;
    Mesh m;
    const auto put = [&m](const Vec3& p) {
        for (int k = 0; k < 3; ++k)
            m.positions.push_back(float(p[std::size_t(k)]));
    };
    for (int end = 0; end < 2; ++end)
        for (int k = 0; k < kSides; ++k) {
            const double a = 2 * std::numbers::pi * k / kSides;
            put(add(end == 0 ? top : bottom, add(mul(side, r * std::cos(a)), mul(other, r * std::sin(a)))));
        }
    put(top);
    put(bottom);
    const std::uint32_t ct = 2 * kSides, cb = ct + 1;
    for (std::uint32_t k = 0; k < kSides; ++k) {
        const std::uint32_t a = k, b = (k + 1) % kSides, c = a + kSides, d = b + kSides;
        m.indices.insert(m.indices.end(), {a, c, b, b, c, d, ct, a, b, cb, d, c});
    }
    // Facing out whichever way the rod points.
    const float* p0 = &m.positions[0];
    const float* p1 = &m.positions[3 * kSides];
    const float* p2 = &m.positions[3];
    const Vec3 e1{p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]}, e2{p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
    const Vec3 outward = sub(Vec3{p0[0], p0[1], p0[2]}, mul(add(top, bottom), 0.5));
    if (dot(cross(e1, e2), outward) < 0)
        for (std::size_t t = 0; t < m.indices.size(); t += 3)
            std::swap(m.indices[t + 1], m.indices[t + 2]);
    return m;
}

MeshSolid apply(const MeshSolid& solid, const HollowFinish& f)
{
    check(f);
    MeshSolid out = hollowed(solid, f.thickness);
    if (f.holes.empty())
        return out;
    std::vector<MeshSolid> tools;
    for (const DrainHole& h : f.holes) {
        const Mesh c = holeCutter(h, f.holeSize, f.thickness);
        tools.push_back(meshkernel::fromTriangles(c.positions, c.indices));
    }
    return meshkernel::subtracted(out, tools);
}

std::vector<DrainHole> lowSpots(const Mesh& plate, double holeSize, int count)
{
    // Candidates: the middle of each triangle, with its own way out; lowest first, and
    // among equally low ones, those facing most nearly down.
    struct Spot {
        Vec3 point, normal;
    };
    std::vector<Spot> spots;
    for (std::size_t t = 0; t + 2 < plate.indices.size(); t += 3) {
        const auto p = [&](std::size_t k) {
            const std::size_t i = plate.indices[t + k];
            return Vec3{plate.positions[3 * i], plate.positions[3 * i + 1], plate.positions[3 * i + 2]};
        };
        const Vec3 nrm = cross(sub(p(1), p(0)), sub(p(2), p(0)));
        if (dot(nrm, nrm) < 1e-18)
            continue;
        spots.push_back({mul(add(add(p(0), p(1)), p(2)), 1.0 / 3.0), unit(nrm)});
    }
    std::sort(spots.begin(), spots.end(), [](const Spot& a, const Spot& b) {
        if (std::abs(a.point[2] - b.point[2]) > 1e-6)
            return a.point[2] < b.point[2];
        return a.normal[2] < b.normal[2];
    });
    std::vector<DrainHole> out;
    const double apart = 4.0 * holeSize;
    for (const Spot& s : spots) {
        if (int(out.size()) >= count)
            break;
        bool clear = true;
        for (const DrainHole& h : out) {
            const Vec3 d = sub(s.point, Vec3{h.point[0], h.point[1], h.point[2]});
            clear = clear && dot(d, d) >= apart * apart;
        }
        if (!clear)
            continue;
        DrainHole h;
        h.point = {s.point[0], s.point[1], s.point[2]};
        h.normal = {s.normal[0], s.normal[1], s.normal[2]};
        out.push_back(h);
    }
    return out;
}

PrimitiveSpec plug(const HollowFinish& f, double fit)
{
    return CylinderSpec(std::max(0.2, f.holeSize / 2.0 - std::max(0.0, fit)), f.thickness);
}

}  // namespace hollow

namespace {

const Body& hollowStep(const Document& doc, ObjectId result)
{
    const Body* b = doc.find(result);
    if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::Hollow || !b->boolean->hollow)
        throw std::invalid_argument("That isn't a Hollow step.");
    return *b;
}

HollowFinish moved(const HollowFinish& f, const Transform& t)
{
    HollowFinish out = f;
    for (DrainHole& h : out.holes) {
        const Vec3 p = t.apply(Vec3{h.point[0], h.point[1], h.point[2]});
        Vec3 n = t.applyDirection(Vec3{h.normal[0], h.normal[1], h.normal[2]});
        const double l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (l > 1e-12)
            n = {n[0] / l, n[1] / l, n[2] / l};
        h.point = {p[0], p[1], p[2]};
        h.normal = {n[0], n[1], n[2]};
    }
    return out;
}

}  // namespace

BooleanSpec hollowSpec(const Document& doc, ObjectId target, const HollowFinish& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    hollow::check(inPlate);
    BooleanSpec spec;
    spec.op = BooleanOp::Hollow;
    spec.operands = {target};
    // In the coordinates of whatever holds the part (the plate, or the next step up).
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.hollow = moved(inPlate, toStep);
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

HollowFinish hollowInPlate(const Document& doc, ObjectId result)
{
    const Body& body = hollowStep(doc, result);
    return moved(*body.boolean->hollow, doc.worldTransform(result));
}

Body withHollow(const Document& doc, ObjectId result, const HollowFinish& inPlate)
{
    const Body& body = hollowStep(doc, result);
    hollow::check(inPlate);
    Body next = body;
    next.boolean->hollow = moved(inPlate, doc.worldTransform(result).inverse());
    return next;
}

}  // namespace forge
