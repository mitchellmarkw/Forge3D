// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Rock.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"
#include "RockField.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <stdexcept>
#include <vector>

namespace forge::rock {

namespace {

using namespace rockfield;

}  // namespace

const char* title(RockKind k)
{
    switch (k) {
    case RockKind::Boulder: return "Boulder";
    case RockKind::Crag: return "Crag (broken rock)";
    case RockKind::Slab: return "Slab";
    case RockKind::Pile: return "Pile of rocks";
    }
    return "";
}

RockSpec defaults(RockKind k, const RockSpec& from)
{
    RockSpec s = from;
    s.kind = k;
    switch (k) {
    case RockKind::Boulder: s.length = 1500, s.width = 1100, s.height = 900, s.roughness = 0.4, s.sunk = 0.2; break;
    case RockKind::Crag: s.length = 2000, s.width = 1400, s.height = 1600, s.roughness = 0.5, s.sunk = 0.15; break;
    case RockKind::Slab: s.length = 1800, s.width = 1200, s.height = 300, s.roughness = 0.3, s.sunk = 0.3; break;
    case RockKind::Pile: s.length = 2500, s.width = 2000, s.height = 900, s.roughness = 0.5, s.sunk = 0.1; break;
    }
    return s;
}

void check(const RockSpec& s)
{
    if (!(s.scale >= 1.0 && s.scale <= 1000.0))
        throw std::invalid_argument("The scale must be from 1:1 to 1:1000.");
    if (!(s.length >= 10 && s.width >= 10 && s.height >= 10 && s.length <= 200000 && s.width <= 200000 &&
          s.height <= 200000))
        throw std::invalid_argument("A rock must be from 10 mm to 200 m each way.");
    if (!(s.roughness >= 0 && s.roughness <= 1))
        throw std::invalid_argument("Roughness must be from 0 to 100%.");
    if (!(s.sunk >= 0 && s.sunk <= 0.8))
        throw std::invalid_argument("A rock can be sunk at most 80% into the ground.");
    if (s.layers && !(s.layerThickness >= 5))
        throw std::invalid_argument("The layers must be at least 5 mm thick.");
    if (!(s.count >= 2 && s.count <= 40))
        throw std::invalid_argument("A pile has from 2 to 40 rocks.");
    if (!(s.smallest >= 0.02 && s.smallest <= 2))
        throw std::invalid_argument("The smallest detail must be from 0.02 to 2 mm.");
    const double k = 1.0 / s.scale;
    if (std::min({s.length, s.width, s.height}) * k < 2 * s.smallest)
        throw std::invalid_argument("The rock is too small to print at this scale: make it bigger, or the smallest "
                                    "detail finer.");
}

Mesh make(const RockSpec& s, bool quick)
{
    check(s);
    const double k = 1.0 / s.scale;
    const Vec3 half{s.length * k / 2, s.width * k / 2, s.height * k / 2};
    const double ground = s.sunk * 2 * half[2];  // the rock's base this far up its height
    // Rocks, centred on X = Y = 0, standing on Z = -ground (cut at 0).
    Field field;
    if (s.kind == RockKind::Pile) {
        std::mt19937 rng(s.seed);
        std::uniform_real_distribution<double> uni(0.0, 1.0);
        struct One {
            Field f;
            Vec3 c;
            double reach;  // nothing of it further than this from c
            double ratio;  // its field is at least (distance - reach) x this out there
        };
        std::vector<One> rocks;
        for (int i = 0; i < s.count; ++i) {
            // Bigger rocks lower and in the middle, smaller ones round and on them.
            const double t = double(i) / std::max(1, s.count - 1);
            // Smaller rocks the more there are, so each is still its own rock.
            const double size = (0.42 - 0.2 * t) * std::sqrt(6.0 / std::max(6, s.count)) * (0.8 + 0.4 * uni(rng));
            const double a = uni(rng) * 2 * kPi, rr = std::sqrt(uni(rng)) * (0.7 + 0.3 * t);
            const Vec3 r{half[0] * size * (0.8 + 0.4 * uni(rng)), half[1] * size * (0.8 + 0.4 * uni(rng)),
                         std::min(half[2], half[0] * size) * (0.6 + 0.3 * uni(rng))};
            const Vec3 c{std::cos(a) * rr * (half[0] - r[0]), std::sin(a) * rr * (half[1] - r[1]),
                         -ground + r[2] * 0.8 + (1 - rr) * (2 * half[2] - 2 * r[2]) * 0.55 * (1 - t * 0.3)};
            rocks.push_back({oneRock(c, r, uni(rng) < 0.6, s.roughness, s.seed * 31u + std::uint32_t(i), 0), c,
                             std::max({r[0], r[1], r[2]}) * (1.0 + 0.3 * s.roughness),
                             0.5 * std::min({r[0], r[1], r[2]}) / std::max({r[0], r[1], r[2]})});
        }
        const double blend = 0.03 * std::min(half[0], half[1]);
        field = [rocks, blend](const Vec3& p) {
            double d = 1e9;
            for (const auto& one : rocks) {
                // Far from this one (further than it reaches, and the blend): it can't matter.
                // (A lower bound of its field, so leaving it out changes nothing: no seams.)
                const double far = (std::sqrt((p[0] - one.c[0]) * (p[0] - one.c[0]) + (p[1] - one.c[1]) * (p[1] - one.c[1]) +
                                              (p[2] - one.c[2]) * (p[2] - one.c[2])) -
                                    one.reach) *
                                   one.ratio;
                if (far > d + blend)
                    continue;
                d = smoothMin(d, one.f(p), blend);
            }
            return d;
        };
    } else {
        const Vec3 c{0, 0, half[2] - ground};
        field = oneRock(c, half, s.kind == RockKind::Crag || s.kind == RockKind::Slab, s.roughness, s.seed,
                        s.kind == RockKind::Slab ? 1.0 : 0.0);
    }
    // Bedding: grooves across it, a layer apart.
    if (s.layers) {
        const double t = s.layerThickness * k;
        const double depth = std::max(0.25 * t, s.smallest * 0.8);
        if (t >= 2 * s.smallest) {
            const Field base = field;
            field = [base, t, depth, ground](const Vec3& p) {
                const double f = (p[2] + ground) / t;
                const double g = std::abs(f - std::round(f));  // 0 on a groove's line
                return base(p) + depth * std::max(0.0, 1.0 - g * 6.0);
            };
        }
    }
    // Cut flat at the ground.
    const Field shape = [field](const Vec3& p) { return std::max(field(p), -p[2]); };
    const double extent = std::max({half[0], half[1], half[2]}) * 1.35;
    // Fine enough to print what's there; not finer (a rock's surface is noise: every
    // step finer costs eight times as much).
    double edge = std::max({s.smallest * 0.75, std::min({half[0], half[1], half[2]}) / 18.0, extent / 90.0});
    if (quick)
        edge *= 2.5;
    Bounds b;
    b.expand(float(-extent), float(-extent), float(-0.01));
    b.expand(float(extent), float(extent), float(2 * half[2] * 1.35));
    const MeshSolid solid = meshkernel::levelSet([&](const Vec3& p) { return -shape(p); }, b, edge, 0.0);
    Mesh m;
    meshkernel::triangles(solid, m.positions, m.indices);
    if (m.indices.empty())
        throw std::invalid_argument("The rock came out empty: sink it less, or make it bigger.");
    // Centred on X = Y = 0 (its lumps may have moved it).
    const Bounds mb = m.bounds();
    const float cx = mb.center()[0], cy = mb.center()[1];
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
        m.positions[i] -= cx;
        m.positions[i + 1] -= cy;
    }
    return m;
}

Body body(const RockSpec& spec, const Mesh& made, const std::string& name, double x, double y)
{
    RockSpec s = spec;
    Body b = hinge::pieceBody(made, name);
    s.offset = {b.placement.x, b.placement.y, b.placement.z};
    b.rock = s;
    b.colour = s.kind == RockKind::Slab ? 0x8E8A84 : 0x8A8680;
    b.placement.x += x;
    b.placement.y += y;
    return b;
}

ObjectId add(CommandStack& stack, Document& doc, RockSpec s, const std::string& name, double x, double y)
{
    Body b = body(s, make(s), doc.uniqueName(name), x, y);
    b.id = doc.allocateId();
    const ObjectId id = b.id;
    stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
    return id;
}

void update(CommandStack& stack, Document& doc, ObjectId id, RockSpec s)
{
    const Body* old = doc.find(id);
    if (!old || !old->rock)
        throw std::invalid_argument("That shape isn't a rock.");
    const double bx = old->placement.x - old->rock->offset[0];
    const double by = old->placement.y - old->rock->offset[1];
    const Body before = *old;
    Body after = body(s, make(s), before.name, bx, by);
    after.id = id;
    after.colour = before.colour;
    after.placement.z = before.placement.z - before.rock->offset[2] + after.rock->offset[2];
    after.placement.rotationX = before.placement.rotationX;
    after.placement.rotationY = before.placement.rotationY;
    after.placement.rotationZ = before.placement.rotationZ;
    stack.execute(std::make_unique<EditBodyCommand>(before, after, "Change Rock"));
}

}  // namespace forge::rock
