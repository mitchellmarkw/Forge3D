// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 80: pictures for sharing
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Paint.h"
#include "forge/core/Render.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>

using namespace forge;

namespace {

render::Item box(double w, double d, double h, double x, double y, std::uint32_t colour)
{
    render::Item it;
    it.colour = colour;
    const double xs[2] = {x - w / 2, x + w / 2}, ys[2] = {y - d / 2, y + d / 2}, zs[2] = {0, h};
    for (int k = 0; k < 8; ++k)
        it.mesh.positions.insert(it.mesh.positions.end(), {float(xs[k & 1]), float(ys[(k >> 1) & 1]), float(zs[(k >> 2) & 1])});
    // Faces, outward (anticlockwise from outside).
    const std::uint32_t f[12][3] = {{0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6}, {0, 1, 4}, {1, 5, 4},
                                    {2, 6, 3}, {3, 6, 7}, {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}};
    for (const auto& t : f)
        it.mesh.indices.insert(it.mesh.indices.end(), {t[0], t[1], t[2]});
    return it;
}

std::uint32_t at(const render::Image& im, int x, int y)
{
    return im.pixels[std::size_t(y) * std::size_t(im.width) + std::size_t(x)];
}

int brightness(std::uint32_t c)
{
    return int((c >> 16) & 0xFF) + int((c >> 8) & 0xFF) + int(c & 0xFF);
}

// A GIF read back the way browsers read it: each frame's palette indices.
struct Decoded {
    int width = 0, height = 0;
    std::vector<std::array<std::uint8_t, 3>> palette;
    std::vector<std::vector<std::uint8_t>> frames;
    bool loops = false;
};

Decoded readGif(const std::vector<std::uint8_t>& g)
{
    Decoded d;
    REQUIRE(g.size() > 13);
    REQUIRE(std::string(g.begin(), g.begin() + 6) == "GIF89a");
    d.width = g[6] | (g[7] << 8);
    d.height = g[8] | (g[9] << 8);
    std::size_t p = 13;
    REQUIRE((g[10] & 0x80));
    const int size = 1 << ((g[10] & 7) + 1);
    for (int i = 0; i < size; ++i, p += 3)
        d.palette.push_back({g[p], g[p + 1], g[p + 2]});
    while (p < g.size()) {
        const std::uint8_t b = g[p++];
        if (b == 0x3B)
            break;
        if (b == 0x21) {
            const std::uint8_t label = g[p++];
            if (label == 0xFF && g[p] == 11 && std::string(g.begin() + std::ptrdiff_t(p) + 1, g.begin() + std::ptrdiff_t(p) + 12) == "NETSCAPE2.0")
                d.loops = true;
            while (g[p] != 0)
                p += g[p] + 1u;
            ++p;
            continue;
        }
        REQUIRE(b == 0x2C);
        p += 9;  // where, size, flags (no local table)
        const int minSize = g[p++];
        std::vector<std::uint8_t> data;
        while (g[p] != 0) {
            data.insert(data.end(), g.begin() + std::ptrdiff_t(p) + 1, g.begin() + std::ptrdiff_t(p) + 1 + g[p]);
            p += g[p] + 1u;
        }
        ++p;
        // LZW.
        const int clear = 1 << minSize, end = clear + 1;
        int codeSize = minSize + 1, avail = clear + 2, old = -1;
        std::vector<std::vector<std::uint8_t>> table(4096);
        for (int i = 0; i < clear; ++i)
            table[std::size_t(i)] = {std::uint8_t(i)};
        std::vector<std::uint8_t> out;
        std::size_t bit = 0;
        const auto read = [&]() -> int {
            int v = 0;
            for (int k = 0; k < codeSize; ++k, ++bit) {
                if (bit / 8 >= data.size())
                    return end;
                v |= ((data[bit / 8] >> (bit % 8)) & 1) << k;
            }
            return v;
        };
        for (;;) {
            const int code = read();
            if (code == clear) {
                codeSize = minSize + 1;
                avail = clear + 2;
                old = -1;
                continue;
            }
            if (code == end)
                break;
            std::vector<std::uint8_t> entry;
            if (old < 0) {
                entry = table[std::size_t(code)];
            } else {
                if (code < avail)
                    entry = table[std::size_t(code)];
                else {
                    REQUIRE(code == avail);
                    entry = table[std::size_t(old)];
                    entry.push_back(table[std::size_t(old)][0]);
                }
                if (avail < 4096) {
                    table[std::size_t(avail)] = table[std::size_t(old)];
                    table[std::size_t(avail)].push_back(entry[0]);
                    ++avail;
                    if (avail == (1 << codeSize) && codeSize < 12)
                        ++codeSize;
                }
            }
            out.insert(out.end(), entry.begin(), entry.end());
            old = code;
        }
        d.frames.push_back(std::move(out));
    }
    return d;
}

}  // namespace

TEST_CASE("Render: a lit box on a light background, with its shadow", "[render]")
{
    const std::vector<render::Item> items{box(20, 20, 30, 0, 0, 0x3366CC)};
    render::Settings s;
    s.width = 200;
    s.height = 200;
    s.turn = 30;
    s.tilt = 20;
    const auto im = render::picture(items, s);
    REQUIRE(im.pixels.size() == 200u * 200u);
    // The middle is the box (blue), the corners the backdrop (light grey).
    const std::uint32_t mid = at(im, 100, 100);
    CHECK((mid & 0xFF) > ((mid >> 16) & 0xFF));
    const std::uint32_t corner = at(im, 2, 2);
    CHECK((corner >> 24) == 0xFF);
    CHECK(brightness(corner) > 3 * 230);
    // The box's sides are lit differently (smooth or not, it has form).
    std::map<int, int> shades;
    for (int y = 40; y < 160; y += 3)
        for (int x = 40; x < 160; x += 3)
            if ((at(im, x, y) & 0xFF) > ((at(im, x, y) >> 16) & 0xFF) + 40)
                ++shades[brightness(at(im, x, y)) / 30];
    CHECK(shades.size() >= 3);
    // Darker on the ground just under it than out at the side.
    // The first backdrop pixel under the box.
    int y = 199;
    while (y > 100 && (at(im, 100, y - 1) & 0xFF) <= ((at(im, 100, y - 1) >> 16) & 0xFF) + 10)
        --y;
    const int below = brightness(at(im, 100, y + 1));
    CHECK(below < brightness(corner) - 20);

    // No backdrop: see-through round it.
    s.background = render::Background::Clear;
    s.shadow = false;
    const auto clear = render::picture(items, s);
    CHECK((at(clear, 2, 2) >> 24) == 0);
    CHECK((at(clear, 100, 100) >> 24) == 0xFF);

    render::Settings bad;
    bad.fill = 2.0;
    CHECK_THROWS_AS(render::picture(items, bad), std::invalid_argument);
}

TEST_CASE("Render: turning round, and paint", "[render]")
{
    // A tall thin wall across X: wide from the front, narrow from the side.
    render::Item wall = box(40, 4, 30, 0, 0, 0xCC3333);
    render::Settings s;
    s.width = s.height = 160;
    s.tilt = 0;
    s.shadow = false;
    s.background = render::Background::Clear;
    const auto width = [](const render::Image& im) {
        int lo = im.width, hi = -1;
        for (int x = 0; x < im.width; ++x)
            if ((at(im, x, im.height / 2) >> 24) > 0) {
                lo = std::min(lo, x);
                hi = std::max(hi, x);
            }
        return hi - lo;
    };
    s.turn = 0;
    const int front = width(render::picture({wall}, s));
    s.turn = 90;
    const int side = width(render::picture({wall}, s));
    CHECK(front > side * 4);
    // Painted triangles show their paint.
    wall.painted.assign(wall.mesh.indices.size() / 3, 0x22AA22);
    s.turn = 0;
    const auto im = render::picture({wall}, s);
    const std::uint32_t c = at(im, 80, 80);
    CHECK(((c >> 8) & 0xFF) > ((c >> 16) & 0xFF) + 30);
}

TEST_CASE("Render: an animated GIF reads back", "[render]")
{
    std::vector<render::Image> frames;
    for (int n = 0; n < 3; ++n) {
        render::Image im;
        im.width = 97;
        im.height = 61;
        for (int y = 0; y < im.height; ++y)
            for (int x = 0; x < im.width; ++x)
                im.pixels.push_back(0xFF000000u | (std::uint32_t((x * 255) / 96) << 16) | (std::uint32_t((y * 255) / 60) << 8) |
                                    std::uint32_t(n * 100));
        frames.push_back(std::move(im));
    }
    double last = 0;
    const auto bytes = render::gif(frames, 5, [&](double p) { last = p; });
    CHECK(last == 1.0);
    const Decoded d = readGif(bytes);
    CHECK(d.width == 97);
    CHECK(d.height == 61);
    CHECK(d.loops);
    REQUIRE(d.frames.size() == 3);
    for (std::size_t n = 0; n < 3; ++n) {
        REQUIRE(d.frames[n].size() == 97u * 61u);
        // Each pixel near its colour (256 colours for a smooth ramp, with a little dither).
        int worst = 0;
        for (std::size_t i = 0; i < d.frames[n].size(); ++i) {
            const auto& pc = d.palette[d.frames[n][i]];
            const std::uint32_t c = frames[n].pixels[i];
            worst = std::max({worst, std::abs(int(pc[0]) - int((c >> 16) & 0xFF)),
                              std::abs(int(pc[1]) - int((c >> 8) & 0xFF)), std::abs(int(pc[2]) - int(c & 0xFF))});
        }
        CHECK(worst < 48);
    }
    if (const char* folder = std::getenv("FORGE_DUMP")) {
        std::ofstream(std::string(folder) + "/ramp.gif", std::ios::binary)
            .write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    }
    CHECK_THROWS_AS(render::gif({}, 5), std::invalid_argument);
}

TEST_CASE("Render: shapes from the document, painted", "[render]")
{
    Document doc;
    CommandStack stack{doc};
    Body cyl;
    cyl.id = doc.allocateId();
    cyl.name = "Post";
    cyl.spec = CylinderSpec{5.0, 20.0};
    stack.execute(std::make_unique<AddBodyCommand>(cyl));
    Body painted;
    painted.id = doc.allocateId();
    painted.name = "Post";
    painted.boolean = paintSpec(doc, cyl.id, {{{0, -5, 10}, 4.0, 0xFF0000, false}});
    stack.execute(std::make_unique<CombineCommand>(painted));
    const auto items = render::itemsOf(doc, doc.topLevelIds());
    REQUIRE(items.size() == 1);
    REQUIRE_FALSE(items[0].painted.empty());
    std::size_t red = 0;
    for (auto c : items[0].painted)
        red += c == 0xFF0000;
    CHECK(red > 0);
    render::Settings s;
    s.width = s.height = 120;
    s.turn = 0;
    s.tilt = 0;
    const auto im = render::picture(items, s);
    const std::uint32_t c = at(im, 60, 60);  // the front, painted red
    CHECK(((c >> 16) & 0xFF) > ((c >> 8) & 0xFF) + 60);
    if (const char* folder = std::getenv("FORGE_DUMP")) {
        render::Settings big;
        big.width = big.height = 400;
        std::vector<render::Image> turns;
        for (int k = 0; k < 4; ++k) {
            big.turn = k * 90;
            turns.push_back(render::picture(items, big));
        }
        const auto g = render::gif(turns, 30);
        std::ofstream(std::string(folder) + "/post.gif", std::ios::binary)
            .write(reinterpret_cast<const char*>(g.data()), std::streamsize(g.size()));
    }
}
