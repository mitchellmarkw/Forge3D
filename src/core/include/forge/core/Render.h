// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

// Pictures for sharing (slice 80): a clean, lit picture of the model (smooth shading, its
// colours and paint, a soft shadow on the ground) from any way round, and a turntable
// GIF of it going once round.
namespace forge {

class Document;

namespace render {

struct Item {
    Mesh mesh;                            // world coordinates
    std::uint32_t colour = 0x8a9bb0;      // its own colour
    std::vector<std::uint32_t> painted;   // per triangle (paint::kOwnColour: its own); may be empty
};

enum class Background { White, Light, Dark, Clear };
inline constexpr int kBackgroundCount = 4;
const char* title(Background b);

struct Settings {
    int width = 1200, height = 1200;
    double turn = 30.0;   // degrees round from the front (+ toward the figure's left... the camera moves to +X)
    double tilt = 20.0;   // degrees above level
    double fill = 0.85;   // how much of the picture the model fills (0.3 .. 1)
    Background background = Background::Light;
    bool shadow = true;
    int smooth = 2;       // drawn this many times bigger and averaged (1..3)
};
void check(const Settings& s);

struct Image {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels;  // 0xAARRGGBB, row by row (not premultiplied)
};

// The models in `items`, seen as `s` says. The camera is framed on the whole of them,
// the same for every turn (a turntable doesn't jump).
Image picture(const std::vector<Item>& items, const Settings& s);

// The shapes as they are in the document (world coordinates, colours and paint), each
// then moved by `moved` if it's there (as Pose Test has them posed).
std::vector<Item> itemsOf(const Document& doc, const std::vector<ObjectId>& ids,
                          const std::map<ObjectId, Transform>& moved = {});

// ---- GIF ----
// An animated GIF of `frames` (all the same size), each shown `delay` hundredths of a
// second, looping for ever. One palette for them all. `progress` (0..1) may be empty.
std::vector<std::uint8_t> gif(const std::vector<Image>& frames, int delay,
                              const std::function<void(double)>& progress = {});

}  // namespace render
}  // namespace forge
