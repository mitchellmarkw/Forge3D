// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Reshape.h"

#include "forge/core/Document.h"
#include "forge/core/Sculpt.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace forge {

namespace reshape {

namespace {

// Bernstein polynomials of degree n at t.
void bernstein(int n, double t, double* out)
{
    // n <= 3: binomials 1; 1 1; 1 2 1; 1 3 3 1
    static constexpr double kBinomial[4][4] = {{1, 0, 0, 0}, {1, 1, 0, 0}, {1, 2, 1, 0}, {1, 3, 3, 1}};
    for (int i = 0; i <= n; ++i)
        out[i] = kBinomial[n][i] * std::pow(t, i) * std::pow(1.0 - t, n - i);
}

double diagonal(const ReshapeFinish& f)
{
    const double dx = f.max[0] - f.min[0], dy = f.max[1] - f.min[1], dz = f.max[2] - f.min[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

void check(const ReshapeFinish& f)
{
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(f.min[std::size_t(k)]) || !std::isfinite(f.max[std::size_t(k)]) ||
            !(f.max[std::size_t(k)] - f.min[std::size_t(k)] > 1e-6))
            throw std::invalid_argument("The reshape box has no size.");
        if (f.count[std::size_t(k)] < kLeastHandles || f.count[std::size_t(k)] > kMostHandles)
            throw std::invalid_argument("The reshape box must have 2 to 4 handles along each side.");
    }
    if (int(f.moves.size()) != handleCount(f))
        throw std::invalid_argument("The reshape box's handles don't match its grid.");
    for (const auto& m : f.moves)
        if (!std::isfinite(m[0]) || !std::isfinite(m[1]) || !std::isfinite(m[2]))
            throw std::invalid_argument("A reshape handle's move isn't a number.");
}

std::string describe(const ReshapeFinish& f)
{
    int moved = 0;
    for (const auto& m : f.moves)
        moved += (std::abs(m[0]) + std::abs(m[1]) + std::abs(m[2])) > 1e-9;
    std::ostringstream s;
    s << f.count[0] << " × " << f.count[1] << " × " << f.count[2] << " handles, " << moved
      << (moved == 1 ? " moved" : " moved");
    return s.str();
}

int handleCount(const ReshapeFinish& f)
{
    return f.count[0] * f.count[1] * f.count[2];
}

int handleIndex(const ReshapeFinish& f, int i, int j, int k)
{
    return i + f.count[0] * (j + f.count[1] * k);
}

ReshapeFinish around(const Bounds& b, int handles)
{
    ReshapeFinish f;
    const double pad = std::max(0.02 * double(b.radius()), 0.05);
    for (int k = 0; k < 3; ++k) {
        f.min[std::size_t(k)] = double(b.min[std::size_t(k)]) - pad;
        f.max[std::size_t(k)] = double(b.max[std::size_t(k)]) + pad;
    }
    handles = std::clamp(handles, kLeastHandles, kMostHandles);
    f.count = {handles, handles, handles};
    f.moves.assign(std::size_t(handleCount(f)), {0.0, 0.0, 0.0});
    return f;
}

ReshapeFinish withHandles(const ReshapeFinish& f, int handles)
{
    ReshapeFinish g = f;
    handles = std::clamp(handles, kLeastHandles, kMostHandles);
    g.count = {handles, handles, handles};
    g.moves.assign(std::size_t(handleCount(g)), {0.0, 0.0, 0.0});
    return g;
}

Vec3 rest(const ReshapeFinish& f, int index)
{
    const int i = index % f.count[0], j = (index / f.count[0]) % f.count[1], k = index / (f.count[0] * f.count[1]);
    const int ijk[3] = {i, j, k};
    Vec3 p;
    for (int a = 0; a < 3; ++a)
        p[std::size_t(a)] = f.min[std::size_t(a)] + (f.max[std::size_t(a)] - f.min[std::size_t(a)]) *
                                                        double(ijk[a]) / double(f.count[std::size_t(a)] - 1);
    return p;
}

Vec3 at(const ReshapeFinish& f, int index)
{
    const Vec3 r = rest(f, index);
    const auto& m = f.moves[std::size_t(index)];
    return {r[0] + m[0], r[1] + m[1], r[2] + m[2]};
}

int mirrored(const ReshapeFinish& f, int index)
{
    const int i = index % f.count[0], rest = index / f.count[0];
    return (f.count[0] - 1 - i) + f.count[0] * rest;
}

Vec3 moved(const ReshapeFinish& f, const Vec3& p)
{
    double t[3];
    for (int a = 0; a < 3; ++a)
        t[a] = std::clamp((p[std::size_t(a)] - f.min[std::size_t(a)]) / (f.max[std::size_t(a)] - f.min[std::size_t(a)]),
                          0.0, 1.0);
    double bx[4], by[4], bz[4];
    bernstein(f.count[0] - 1, t[0], bx);
    bernstein(f.count[1] - 1, t[1], by);
    bernstein(f.count[2] - 1, t[2], bz);
    Vec3 d{0, 0, 0};
    int index = 0;
    for (int k = 0; k < f.count[2]; ++k)
        for (int j = 0; j < f.count[1]; ++j)
            for (int i = 0; i < f.count[0]; ++i, ++index) {
                const auto& m = f.moves[std::size_t(index)];
                if (m[0] == 0.0 && m[1] == 0.0 && m[2] == 0.0)
                    continue;
                const double w = bx[i] * by[j] * bz[k];
                for (int a = 0; a < 3; ++a)
                    d[std::size_t(a)] += w * m[std::size_t(a)];
            }
    return {p[0] + d[0], p[1] + d[1], p[2] + d[2]};
}

Mesh finer(const Mesh& surface, const ReshapeFinish& f)
{
    const Bounds b = surface.bounds();
    const Mesh shared = sculpt::welded(surface, std::max(1e-6, double(b.radius()) * 1e-6));
    return sculpt::refined(shared, std::max(diagonal(f) / 60.0, 0.02), 600'000);
}

Mesh movedMesh(const Mesh& fine, const ReshapeFinish& f)
{
    Mesh out;
    out.indices = fine.indices;
    out.positions.resize(fine.positions.size());
    for (std::size_t i = 0; i + 2 < fine.positions.size(); i += 3) {
        const Vec3 q = moved(f, {fine.positions[i], fine.positions[i + 1], fine.positions[i + 2]});
        for (std::size_t a = 0; a < 3; ++a)
            out.positions[i + a] = float(q[a]);
    }
    return out;
}

Mesh reshaped(const Mesh& surface, const ReshapeFinish& f)
{
    check(f);
    return movedMesh(finer(surface, f), f);
}

}  // namespace reshape

BooleanSpec reshapeSpec(const Document& doc, ObjectId target, const ReshapeFinish& f)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    reshape::check(f);
    BooleanSpec spec;
    spec.op = BooleanOp::Reshape;
    spec.operands = {target};
    spec.reshape = f;
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

}  // namespace forge
