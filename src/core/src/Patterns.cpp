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

// Shared pieces for the textures below: noise and cells that repeat seamlessly every
// `cells` across a picture `size` wide.
double hashAt(int i, int j, int n, int seed)
{
    i = ((i % n) + n) % n;
    j = ((j % n) + n) % n;
    std::uint32_t h = std::uint32_t(i) * 374761393u + std::uint32_t(j) * 668265263u + std::uint32_t(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return double((h ^ (h >> 16)) & 0xffff) / 65535.0;
}

double smoothNoise(double x, double y, int size, int cells, int seed)
{
    const double s = double(size) / cells;
    const double fx = x / s, fy = y / s;
    const int i = int(std::floor(fx)), j = int(std::floor(fy));
    const double tx = fx - i, ty = fy - j;
    const double ux = tx * tx * (3 - 2 * tx), uy = ty * ty * (3 - 2 * ty);
    const double a = hashAt(i, j, cells, seed), b = hashAt(i + 1, j, cells, seed), c = hashAt(i, j + 1, cells, seed),
                 d = hashAt(i + 1, j + 1, cells, seed);
    return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy;
}

// Cells round scattered points (one in each square of a `cells` grid): the distance
// to the nearest point and to the next (in pixels).
std::pair<double, double> cellDistances(double x, double y, int size, int cells, int seed, double jitter = 0.8)
{
    const double s = double(size) / cells;
    const int i = int(std::floor(x / s)), j = int(std::floor(y / s));
    double d1 = 1e18, d2 = 1e18;
    for (int dj = -2; dj <= 2; ++dj)
        for (int di = -2; di <= 2; ++di) {
            const int ci = i + di, cj = j + dj;
            const double px = (ci + 0.5 + (hashAt(ci, cj, cells, seed) - 0.5) * jitter) * s;
            const double py = (cj + 0.5 + (hashAt(ci, cj, cells, seed + 101) - 0.5) * jitter) * s;
            const double d = std::hypot(x - px, y - py);
            if (d < d1) {
                d2 = d1;
                d1 = d;
            } else if (d < d2) {
                d2 = d;
            }
        }
    return {d1, d2};
}

// Chainmail: rows of interlinked rings, every other row half a ring along, each ring
// tipped so it lies over its neighbour on one side and under it on the other.
std::shared_ptr<PictureData> chainmail()
{
    constexpr int kSize = 256;
    constexpr double kStep = 64.0, kR = 38.0, kWire = 9.0;  // four rings across
    auto p = blank(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            double h = 0.0;
            for (int row = -1; row <= kSize / 32 + 1; ++row) {
                const double cy = row * 32.0;
                const double offset = (((row % 2) + 2) % 2) * kStep / 2.0;
                for (int col = -1; col <= kSize / int(kStep) + 1; ++col) {
                    const double cx = col * kStep + offset;
                    for (int wx = -1; wx <= 1; ++wx)
                        for (int wy = -1; wy <= 1; ++wy) {
                            const double dx = x + 0.5 - cx - wx * kSize, dy = y + 0.5 - cy - wy * kSize;
                            const double off = std::abs(std::hypot(dx, dy) - kR);
                            if (off >= kWire)
                                continue;
                            const double round = std::sqrt(1.0 - (off / kWire) * (off / kWire));
                            const double tip = 0.75 + 0.25 * (dx / kR);  // higher on its right side
                            h = std::max(h, round * tip);
                        }
                }
            }
            put(*p, x, y, h);
        }
    return p;
}

// Twill (denim): fine diagonal ribs of threads.
std::shared_ptr<PictureData> twill()
{
    constexpr int kSize = 256;
    auto p = blank(kSize, kSize);
    const double pi = std::numbers::pi;
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const double rib = std::pow(std::abs(std::sin(pi * (x + y + 1.0) / 32.0)), 0.6);
            const double thread = 0.85 + 0.15 * std::abs(std::sin(pi * (y + 0.5) / 8.0));
            put(*p, x, y, rib * thread);
        }
    return p;
}

// Knitting: rows of V-shaped stitches, each two leaning loops side by side.
std::shared_ptr<PictureData> knit()
{
    constexpr int kSize = 256, kW = 32, kH = 32;  // eight stitches across
    auto p = blank(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const double lx = std::fmod(x + 0.5, kW) - kW / 2.0, ly0 = std::fmod(y + 0.5, kH) - kH / 2.0;
            double h = 0.0;
            for (int row = -1; row <= 1; ++row) {  // the stitches above and below reach in
                const double ly = ly0 + row * kH;
                for (int side : {-1, 1}) {
                    // A loop leaning in towards the stitch's middle at the bottom.
                    const double cx = side * 7.5, a = side * 0.45;
                    const double rx = (lx - cx) * std::cos(a) + ly * std::sin(a);
                    const double ry = -(lx - cx) * std::sin(a) + ly * std::cos(a);
                    const double e = (rx / 6.5) * (rx / 6.5) + (ry / 16.0) * (ry / 16.0);
                    if (e < 1.0)
                        h = std::max(h, std::sqrt(1.0 - e));
                }
            }
            put(*p, x, y, h);
        }
    return p;
}

// Leather: a pebbled grain of soft, uneven cells with fine creases between.
std::shared_ptr<PictureData> leather()
{
    constexpr int kSize = 256;
    auto p = blank(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const auto [d1, d2] = cellDistances(x + 0.5, y + 0.5, kSize, 16, 7);
            const double crease = std::clamp((d2 - d1) / 4.0, 0.0, 1.0);
            const double dome = 1.0 - std::clamp(d1 / 14.0, 0.0, 1.0) * 0.4;
            put(*p, x, y, std::sqrt(crease) * dome * (0.85 + 0.15 * smoothNoise(x, y, kSize, 32, 9)));
        }
    return p;
}

// Cracked stone: an uneven surface split by thin, deep cracks.
std::shared_ptr<PictureData> cracks()
{
    constexpr int kSize = 256;
    auto p = blank(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            // The cracks wander a little.
            const double wx = x + 6.0 * (smoothNoise(x, y, kSize, 8, 21) - 0.5);
            const double wy = y + 6.0 * (smoothNoise(x, y, kSize, 8, 22) - 0.5);
            const auto [d1, d2] = cellDistances(wx, wy, kSize, 5, 23, 0.9);
            const double crack = std::clamp((d2 - d1) / 3.0, 0.0, 1.0);
            const double rough = 0.6 * smoothNoise(x, y, kSize, 16, 24) + 0.4 * smoothNoise(x, y, kSize, 32, 25);
            put(*p, x, y, crack * (0.55 + 0.45 * rough));
        }
    return p;
}

// Wood grain: long wavy lines along the picture's width.
std::shared_ptr<PictureData> wood()
{
    constexpr int kSize = 256;
    auto p = blank(kSize, kSize);
    const double pi = std::numbers::pi;
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const double warp = 2.2 * smoothNoise(x, y, kSize, 4, 31) + 0.6 * smoothNoise(x, y, kSize, 16, 32);
            const double line = 0.5 + 0.5 * std::sin(2.0 * pi * ((y + 0.5) / 16.0 + warp));
            put(*p, x, y, 0.3 + 0.7 * std::pow(line, 1.6) + 0.08 * smoothNoise(x, y, kSize, 64, 33));
        }
    return p;
}

// Pores: fine, soft skin with tiny pits scattered in it.
std::shared_ptr<PictureData> pores()
{
    constexpr int kSize = 256;
    auto p = blank(kSize, kSize);
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) {
            const double base = 0.6 * smoothNoise(x, y, kSize, 16, 41) + 0.4 * smoothNoise(x, y, kSize, 32, 42);
            const auto [d1, d2] = cellDistances(x + 0.5, y + 0.5, kSize, 24, 43);
            (void)d2;
            const double pit = std::clamp(d1 / 3.2, 0.0, 1.0);
            put(*p, x, y, (0.65 + 0.35 * base) * (0.35 + 0.65 * std::sqrt(pit)));
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
    static const std::vector<std::string> names{"scales", "hexagons", "weave",  "skin",   "bricks", "chainmail",
                                                "twill",  "knit",     "leather", "cracks", "wood",   "pores"};
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
    else if (pattern.name == "chainmail")
        image = fullRange(chainmail());
    else if (pattern.name == "twill")
        image = fullRange(twill());
    else if (pattern.name == "knit")
        image = fullRange(knit());
    else if (pattern.name == "leather")
        image = fullRange(leather());
    else if (pattern.name == "cracks")
        image = fullRange(cracks());
    else if (pattern.name == "wood")
        image = fullRange(wood());
    else if (pattern.name == "pores")
        image = fullRange(pores());
    else
        throw std::invalid_argument("No built-in pattern called '" + pattern.name + "'");
    made.emplace(pattern.name, image);
    return image;
}

double stampRadius(const PictureData& image, double tile)
{
    const double w = tile, h = image.width > 0 ? tile * double(image.height) / double(image.width) : tile;
    return std::hypot(w, h) / 2.0 * 1.02;
}

double patternDepth(const SculptStroke& stroke)
{
    // A stamp is one big picture: shallower for its size than a pattern's small repeats.
    return (stroke.invert ? -1.0 : 1.0) * std::clamp(stroke.strength, 0.0, 1.0) * stroke.tile *
           (stroke.once ? 0.06 : 0.3);
}

}  // namespace forge::sculpt
