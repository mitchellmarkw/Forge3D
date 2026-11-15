// SPDX-License-Identifier: GPL-3.0-or-later
// The Pattern brush's built-in patterns: grey pictures made in code, each repeating
// seamlessly side by side (white highest).
#include "forge/core/Sculpt.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <numbers>
#include <stdexcept>

namespace forge::sculpt {

namespace {

std::shared_ptr<PictureData> blank(int w, int h)
{
    auto p = std::make_shared<PictureData>();
    p->width = w;
    p->height = h;
    p->grey.assign(std::size_t(w) * std::size_t(h), 0);
    return p;
}

void put(PictureData& p, int x, int y, double h)
{
    p.grey[std::size_t(y) * p.width + x] = std::uint8_t(std::lround(std::clamp(h, 0.0, 1.0) * 255.0));
}

// Fish or dragon scales: rows of round scales, each row half a scale along and lying
// over the row above it.
std::shared_ptr<PictureData> scales()
{
    constexpr int kSize = 256, kR = 64;  // two scales across
    auto p = blank(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            double best = -1e9, h = 0.0;
            for (int row = -2; row < kSize / kR + 2; ++row) {
                const double cy = row * kR;
                const double offset = (((row % 2) + 2) % 2) * kR;
                for (int col = -2; col < kSize / (2 * kR) + 2; ++col) {
                    const double cx = col * 2.0 * kR + offset;
                    for (int wrap = -1; wrap <= 1; ++wrap) {
                        const double dx = x + 0.5 - cx - wrap * kSize, dy = y + 0.5 - cy;
                        const double d = std::hypot(dx, dy);
                        if (d >= kR || cy < best)
                            continue;
                        best = cy;  // a lower row lies over the one above
                        h = std::sqrt(1.0 - d / kR) * (0.55 + 0.45 * std::clamp(dy / kR, 0.0, 1.0));
                    }
                }
            }
            put(*p, x, y, h);
        }
    return p;
}

// Hexagons (like a honeycomb or turtle shell): raised cells with grooves between.
std::shared_ptr<PictureData> hexagons()
{
    constexpr int kW = 256, kH = 222;
    constexpr double kCell = 128.0, kRow = kH / 2.0;  // two cells across
    auto p = blank(kW, kH);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            double d1 = 1e9, d2 = 1e9;
            for (int row = 0; row < 2; ++row)  // each centre once; the wraps give the rest
                for (int col = 0; col < 2; ++col) {
                    const double cx = (col + 0.5 * (((row % 2) + 2) % 2)) * kCell, cy = row * kRow;
                    for (int wx = -1; wx <= 1; ++wx)
                        for (int wy = -1; wy <= 1; ++wy) {
                            const double d = std::hypot(x + 0.5 - cx - wx * kW, y + 0.5 - cy - wy * kH);
                            if (d < d1) {
                                d2 = d1;
                                d1 = d;
                            } else if (d < d2) {
                                d2 = d;
                            }
                        }
                }
            const double edge = std::clamp((d2 - d1) / 20.0, 0.0, 1.0);  // 0 on the groove
            put(*p, x, y, std::sqrt(edge) * (0.85 + 0.15 * (1.0 - d1 / kCell)));
        }
    return p;
}

// Woven cloth: threads going over and under each other, a basket weave.
std::shared_ptr<PictureData> weave()
{
    constexpr int kSize = 256, kCell = 128, kThread = 64;  // two by two
    auto p = blank(kSize, kSize);
    const double pi = std::numbers::pi;
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const bool across = ((x / kCell) + (y / kCell)) % 2 == 0;
            const double along = across ? (x % kCell + 0.5) / kCell : (y % kCell + 0.5) / kCell;
            const double side = across ? (y % kThread + 0.5) / kThread : (x % kThread + 0.5) / kThread;
            put(*p, x, y, std::sin(pi * side) * (0.55 + 0.45 * std::sin(pi * along)));
        }
    return p;
}

// Skin: soft, uneven bumps (noise that repeats).
std::shared_ptr<PictureData> skin()
{
    constexpr int kSize = 256;
    auto p = blank(kSize, kSize);
    const auto hash = [](int i, int j, int n, int seed) {
        i = ((i % n) + n) % n;
        j = ((j % n) + n) % n;
        std::uint32_t h = std::uint32_t(i) * 374761393u + std::uint32_t(j) * 668265263u + std::uint32_t(seed) * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return double((h ^ (h >> 16)) & 0xffff) / 65535.0;
    };
    const auto noise = [&](double x, double y, int cells, int seed) {
        const double s = double(kSize) / cells;
        const double fx = x / s, fy = y / s;
        const int i = int(std::floor(fx)), j = int(std::floor(fy));
        const double tx = fx - i, ty = fy - j;
        const double ux = tx * tx * (3 - 2 * tx), uy = ty * ty * (3 - 2 * ty);
        const double a = hash(i, j, cells, seed), b = hash(i + 1, j, cells, seed), c = hash(i, j + 1, cells, seed),
                     d = hash(i + 1, j + 1, cells, seed);
        return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy;
    };
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const double h = 0.55 * noise(x, y, 8, 1) + 0.3 * noise(x, y, 16, 2) + 0.15 * noise(x, y, 32, 3);
            put(*p, x, y, (h - 0.2) / 0.6);
        }
    return p;
}

// Bricks or stone blocks: rows of blocks, every other row half a block along, with
// mortar between and slightly rounded edges.
std::shared_ptr<PictureData> bricks()
{
    constexpr int kSize = 256, kW = 128, kH = 64;  // two bricks across
    constexpr double kMortar = 8.0, kBevel = 18.0;
    auto p = blank(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const int row = y / kH;
            const int shift = (row % 2) * (kW / 2);
            const double bx = std::fmod(x + 0.5 + shift, kW), by = std::fmod(y + 0.5, kH);
            const double toEdge = std::min({bx, kW - bx, by, kH - by}) - kMortar / 2.0;
            put(*p, x, y, toEdge <= 0 ? 0.0 : std::sqrt(std::clamp(toEdge / kBevel, 0.0, 1.0)));
        }
    return p;
}

// Stretched to use every shade, black to white, so each pattern's depth means the same.
std::shared_ptr<PictureData> fullRange(std::shared_ptr<PictureData> p)
{
    const auto [lo, hi] = std::minmax_element(p->grey.begin(), p->grey.end());
    const int a = *lo, b = *hi;
    if (b > a)
        for (auto& g : p->grey)
            g = std::uint8_t((int(g) - a) * 255 / (b - a));
    return p;
}

}  // namespace

const std::vector<std::string>& builtInPatterns()
{
    static const std::vector<std::string> names{"scales", "hexagons", "weave", "skin", "bricks"};
    return names;
}

std::shared_ptr<const PictureData> patternImage(const SculptPattern& pattern)
{
    if (!pattern.builtIn) {
        if (!pattern.image)
            throw std::invalid_argument("The pattern '" + pattern.name + "' has no picture");
        return pattern.image;
    }
    // Made once each, then shared.
    static std::mutex lock;
    static std::map<std::string, std::shared_ptr<const PictureData>> made;
    const std::lock_guard<std::mutex> hold(lock);
    if (const auto found = made.find(pattern.name); found != made.end())
        return found->second;
    std::shared_ptr<const PictureData> image;
    if (pattern.name == "scales")
        image = fullRange(scales());
    else if (pattern.name == "hexagons")
        image = fullRange(hexagons());
    else if (pattern.name == "weave")
        image = fullRange(weave());
    else if (pattern.name == "skin")
        image = fullRange(skin());
    else if (pattern.name == "bricks")
        image = fullRange(bricks());
    else
        throw std::invalid_argument("No built-in pattern called '" + pattern.name + "'");
    made.emplace(pattern.name, image);
    return image;
}

double patternDepth(const SculptStroke& stroke)
{
    return (stroke.invert ? -1.0 : 1.0) * std::clamp(stroke.strength, 0.0, 1.0) * stroke.tile * 0.3;
}

}  // namespace forge::sculpt
