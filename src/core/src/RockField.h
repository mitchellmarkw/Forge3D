// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Rock fields, shared by the rock designer (slice 112) and the terrain's crags (slice 113):
// value noise, a smooth minimum, and one rock's signed field. Private to the core.

#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <vector>

namespace forge::rockfield {

constexpr double kPi = 3.14159265358979323846;

// Smooth value noise: a pseudo-random value at each whole-number point of space, blended
// smoothly between them; `fbm` adds octaves of it (bumps on bumps), from about -1 to 1.
struct Noise {
    std::uint32_t seed;

    double at(int x, int y, int z) const
    {
        std::uint32_t h = seed * 0x9E3779B1u ^ std::uint32_t(x) * 0x85EBCA77u ^ std::uint32_t(y) * 0xC2B2AE3Du ^
                          std::uint32_t(z) * 0x27D4EB2Fu;
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        h *= 0x297A2D39u;
        h ^= h >> 15;
        return double(h & 0xFFFFFF) / double(0xFFFFFF) * 2.0 - 1.0;
    }
    double value(double x, double y, double z) const
    {
        const int ix = int(std::floor(x)), iy = int(std::floor(y)), iz = int(std::floor(z));
        const double fx = x - ix, fy = y - iy, fz = z - iz;
        const auto s = [](double t) { return t * t * (3 - 2 * t); };
        const double u = s(fx), v = s(fy), w = s(fz);
        const auto lerp = [](double a, double b, double t) { return a + (b - a) * t; };
        const double c00 = lerp(at(ix, iy, iz), at(ix + 1, iy, iz), u);
        const double c10 = lerp(at(ix, iy + 1, iz), at(ix + 1, iy + 1, iz), u);
        const double c01 = lerp(at(ix, iy, iz + 1), at(ix + 1, iy, iz + 1), u);
        const double c11 = lerp(at(ix, iy + 1, iz + 1), at(ix + 1, iy + 1, iz + 1), u);
        return lerp(lerp(c00, c10, v), lerp(c01, c11, v), w);
    }
    double fbm(double x, double y, double z, int octaves) const
    {
        double sum = 0, amp = 0.5, f = 1;
        for (int i = 0; i < octaves; ++i) {
            sum += amp * value(x * f + i * 17.3, y * f - i * 9.1, z * f + i * 3.7);
            f *= 2.03;
            amp *= 0.5;
        }
        return sum;
    }
};

using Field = std::function<double(const Vec3&)>;  // negative inside

// One rock's field: an ellipsoid (rounded), or a block cut by random planes (broken), its
// surface roughened; centred at c, half sizes r.
inline Field oneRock(const Vec3& c, const Vec3& r, bool broken, double roughness, std::uint32_t seed, double flatten)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> uni(-1.0, 1.0);
    std::vector<std::pair<Vec3, double>> planes;  // unit normal (in the unit sphere's space), offset
    if (broken) {
        const int n = 9 + int(rng() % 6);
        for (int i = 0; i < n; ++i) {
            Vec3 d{uni(rng), uni(rng), uni(rng) * 0.8};
            const double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (l < 1e-6)
                continue;
            d = {d[0] / l, d[1] / l, d[2] / l};
            planes.push_back({d, 0.72 + 0.2 * (uni(rng) * 0.5 + 0.5)});
        }
    }
    const Noise noise{seed * 7919u + 13u};
    const double small = std::min({r[0], r[1], r[2]});
    const double feature = std::max(small, 1e-6);
    return [=](const Vec3& p) {
        // In the unit sphere's space.
        const double x = (p[0] - c[0]) / r[0], y = (p[1] - c[1]) / r[1], z = (p[2] - c[2]) / r[2];
        double d;
        if (broken) {
            // A rounded block, cut by the planes: flat faces, sharp edges.
            const double q = std::pow(std::pow(std::abs(x), 4) + std::pow(std::abs(y), 4) + std::pow(std::abs(z), 4), 0.25);
            d = q - 1.0;
            for (const auto& [n, off] : planes)
                d = std::max(d, x * n[0] + y * n[1] + z * n[2] - off);
        } else {
            d = std::sqrt(x * x + y * y + z * z) - 1.0;
        }
        d *= small;
        // Lumps (big, gentle) and roughness (small): in model mm, as shares of the rock.
        const double s = 2.2 / feature;
        d += roughness * small * (0.18 * noise.fbm(p[0] * s * 0.5, p[1] * s * 0.5, p[2] * s * 0.5, 2) +
                                  0.07 * noise.fbm(p[0] * s * 2.5, p[1] * s * 2.5, p[2] * s * 2.5, 3));
        if (flatten > 0)  // a slab's top and bottom flatter
            d = std::max(d, std::abs(p[2] - c[2]) - r[2] * (1.0 - 0.15 * flatten));
        return d;
    };
}

inline double smoothMin(double a, double b, double k)
{
    const double h = std::clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return b * (1 - h) + a * h - k * h * (1 - h);
}

}  // namespace forge::rockfield
