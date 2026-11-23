// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Render.h"

#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Paint.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <unordered_map>

namespace forge::render {

namespace {

using V3 = std::array<double, 3>;

V3 add(const V3& a, const V3& b)
{
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
V3 sub(const V3& a, const V3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V3 mul(const V3& a, double k)
{
    return {a[0] * k, a[1] * k, a[2] * k};
}
double dot(const V3& a, const V3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V3 cross(const V3& a, const V3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
V3 unit(const V3& a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 0 ? mul(a, 1.0 / l) : V3{0, 0, 1};
}

double linear(std::uint32_t c, int shift)
{
    const double v = ((c >> shift) & 0xFF) / 255.0;
    return v * v;  // close enough to sRGB's curve for lighting
}

std::uint32_t pack(double a, double r, double g, double b)
{
    const auto byte = [](double v) { return std::uint32_t(std::clamp(v, 0.0, 1.0) * 255.0 + 0.5); };
    return (byte(a) << 24) | (byte(r) << 16) | (byte(g) << 8) | byte(b);
}

// Each corner's normal: the faces round its point that meet at less than `crease`
// (smooth), area weighted; sharp edges stay sharp.
std::vector<V3> cornerNormals(const Mesh& m)
{
    const std::size_t tris = m.indices.size() / 3;
    std::vector<V3> face(tris);
    for (std::size_t t = 0; t < tris; ++t) {
        V3 p[3];
        for (int k = 0; k < 3; ++k) {
            const std::size_t v = m.indices[t * 3 + std::size_t(k)];
            p[k] = {m.positions[v * 3], m.positions[v * 3 + 1], m.positions[v * 3 + 2]};
        }
        face[t] = cross(sub(p[1], p[0]), sub(p[2], p[0]));  // area weighted
    }
    // Points in the same place are one (a mesh may repeat them along its seams).
    std::unordered_map<std::uint64_t, std::uint32_t> where;
    std::vector<std::uint32_t> same(m.positions.size() / 3);
    for (std::size_t v = 0; v < same.size(); ++v) {
        const auto q = [&](int k) { return std::uint64_t(std::int64_t(std::llround(m.positions[v * 3 + std::size_t(k)] * 1e4)) & 0x1FFFFF); };
        const std::uint64_t key = q(0) | (q(1) << 21) | (q(2) << 42);
        same[v] = where.try_emplace(key, std::uint32_t(where.size())).first->second;
    }
    std::vector<std::vector<std::uint32_t>> around(where.size());
    for (std::size_t t = 0; t < tris; ++t)
        for (int k = 0; k < 3; ++k)
            around[same[m.indices[t * 3 + std::size_t(k)]]].push_back(std::uint32_t(t));
    const double crease = std::cos(40.0 * std::numbers::pi / 180.0);
    std::vector<V3> out(tris * 3);
    for (std::size_t t = 0; t < tris; ++t) {
        const V3 mine = unit(face[t]);
        for (int k = 0; k < 3; ++k) {
            V3 sum{0, 0, 0};
            for (std::uint32_t o : around[same[m.indices[t * 3 + std::size_t(k)]]])
                if (dot(unit(face[o]), mine) >= crease)
                    sum = add(sum, face[o]);
            out[t * 3 + std::size_t(k)] = dot(sum, sum) > 0 ? unit(sum) : mine;
        }
    }
    return out;
}

}  // namespace

const char* title(Background b)
{
    switch (b) {
    case Background::White: return "White";
    case Background::Dark: return "Dark";
    case Background::Clear: return "Clear (no background)";
    default: return "Light grey";
    }
}

void check(const Settings& s)
{
    if (s.width < 32 || s.height < 32 || s.width > 6000 || s.height > 6000)
        throw std::invalid_argument("The picture must be from 32 to 6000 pixels each way.");
    if (!(s.tilt >= -60.0 && s.tilt <= 89.0))
        throw std::invalid_argument("The tilt must be from -60 to 89 degrees.");
    if (!(s.fill >= 0.3 && s.fill <= 1.0))
        throw std::invalid_argument("The model must fill from 30% to 100% of the picture.");
    if (s.smooth < 1 || s.smooth > 3)
        throw std::invalid_argument("Smoothing must be 1, 2 or 3.");
}

Image picture(const std::vector<Item>& items, const Settings& s)
{
    check(s);
    const int S = s.smooth;
    const int W = s.width * S, H = s.height * S;

    // What's to be framed.
    V3 lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
    for (const Item& it : items)
        for (std::size_t i = 0; i + 2 < it.mesh.positions.size(); i += 3)
            for (int k = 0; k < 3; ++k) {
                lo[std::size_t(k)] = std::min(lo[std::size_t(k)], double(it.mesh.positions[i + std::size_t(k)]));
                hi[std::size_t(k)] = std::max(hi[std::size_t(k)], double(it.mesh.positions[i + std::size_t(k)]));
            }
    const bool empty = lo[0] > hi[0];
    const V3 centre = empty ? V3{0, 0, 0} : mul(add(lo, hi), 0.5);
    double radius = 1.0;
    if (!empty) {
        radius = 1e-6;
        for (const Item& it : items)
            for (std::size_t i = 0; i + 2 < it.mesh.positions.size(); i += 3) {
                const V3 d = sub({it.mesh.positions[i], it.mesh.positions[i + 1], it.mesh.positions[i + 2]}, centre);
                radius = std::max(radius, std::sqrt(dot(d, d)));
            }
    }
    const double ground = empty ? 0.0 : lo[2];

    // The camera: round the middle, far enough for the whole of it to fit whichever way.
    const double fov = 28.0 * std::numbers::pi / 180.0;
    const double aspect = double(W) / double(H);
    const double half = std::min(fov / 2.0, std::atan(std::tan(fov / 2.0) * aspect));
    const double distance = radius / (std::sin(half) * s.fill);
    const double turn = s.turn * std::numbers::pi / 180.0, tilt = s.tilt * std::numbers::pi / 180.0;
    const V3 eye = add(centre, mul(V3{std::sin(turn) * std::cos(tilt), -std::cos(turn) * std::cos(tilt), std::sin(tilt)},
                                   distance));
    const V3 forward = unit(sub(centre, eye));
    const V3 right = unit(cross(forward, V3{0, 0, 1}));
    const V3 up = cross(right, forward);
    const double focal = (H / 2.0) / std::tan(fov / 2.0);

    // Studio lights, fixed to the camera (the model turns under them).
    const V3 toCamera = mul(forward, -1.0);
    const V3 key = unit(add(add(mul(right, -0.85), mul(up, 0.6)), mul(toCamera, 0.45)));
    const V3 fillLight = unit(add(add(mul(right, 0.8), mul(up, 0.05)), mul(toCamera, 0.45)));
    const V3 rim = unit(add(add(mul(right, 0.25), mul(up, 0.45)), mul(forward, 0.85)));
    const V3 half1 = unit(add(key, toCamera));

    std::vector<float> nearness(std::size_t(W) * std::size_t(H), 0.0f);  // 1/depth: bigger is nearer
    std::vector<std::uint32_t> colour(std::size_t(W) * std::size_t(H), 0u);

    for (const Item& it : items) {
        const auto& P = it.mesh.positions;
        const auto& I = it.mesh.indices;
        const std::vector<V3> normals = cornerNormals(it.mesh);
        for (std::size_t t = 0; t * 3 + 2 < I.size(); ++t) {
            double sx[3], sy[3], iz[3];
            V3 n[3];
            bool behind = false;
            for (int k = 0; k < 3; ++k) {
                const std::size_t v = I[t * 3 + std::size_t(k)];
                const V3 rel = sub({P[v * 3], P[v * 3 + 1], P[v * 3 + 2]}, eye);
                const double z = dot(rel, forward);
                if (z <= 1e-6)
                    behind = true;
                iz[k] = 1.0 / std::max(z, 1e-6);
                sx[k] = W / 2.0 + dot(rel, right) * iz[k] * focal;
                sy[k] = H / 2.0 - dot(rel, up) * iz[k] * focal;
                n[k] = normals[t * 3 + std::size_t(k)];
            }
            if (behind)
                continue;
            const std::uint32_t own =
                t < it.painted.size() && it.painted[t] != paint::kOwnColour ? it.painted[t] : it.colour;
            const double ar = linear(own, 16), ag = linear(own, 8), ab = linear(own, 0);
            const double area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (std::abs(area) < 1e-12)
                continue;
            const int x0 = std::max(0, int(std::floor(std::min({sx[0], sx[1], sx[2]}))));
            const int x1 = std::min(W - 1, int(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
            const int y0 = std::max(0, int(std::floor(std::min({sy[0], sy[1], sy[2]}))));
            const int y1 = std::min(H - 1, int(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const double px = x + 0.5, py = y + 0.5;
                    const double b0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
                    const double b1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
                    const double b2 = 1.0 - b0 - b1;
                    if (b0 < -1e-9 || b1 < -1e-9 || b2 < -1e-9)
                        continue;
                    const double w0 = b0 * iz[0], w1 = b1 * iz[1], w2 = b2 * iz[2];
                    const double near = w0 + w1 + w2;
                    const std::size_t at = std::size_t(y) * std::size_t(W) + std::size_t(x);
                    if (near <= nearness[at])
                        continue;
                    nearness[at] = float(near);
                    V3 nn = unit(add(add(mul(n[0], w0 / near), mul(n[1], w1 / near)), mul(n[2], w2 / near)));
                    if (dot(nn, toCamera) < 0)
                        nn = mul(nn, -1.0);  // seen from inside: lit as its outside
                    const double ambient = 0.13 + 0.07 * nn[2];
                    const double light = ambient + 1.0 * std::max(0.0, dot(nn, key)) +
                                         0.22 * std::max(0.0, dot(nn, fillLight)) +
                                         0.5 * std::pow(std::max(0.0, dot(nn, rim)), 2.0);
                    const double shine = 0.22 * std::pow(std::max(0.0, dot(nn, half1)), 48.0);
                    const auto toned = [&](double a) { return std::sqrt(std::clamp(a * light + shine, 0.0, 1.0)); };
                    colour[at] = pack(1.0, toned(ar), toned(ag), toned(ab));
                }
        }
    }

    // Hairline cracks between triangles (where a shape's mesh has a T-junction) filled from
    // a side: a gap a sample wide with the model either side of it.
    {
        std::vector<float> nearFixed = nearness;
        std::vector<std::uint32_t> colourFixed = colour;
        for (int y = 1; y + 1 < H; ++y)
            for (int x = 1; x + 1 < W; ++x) {
                const std::size_t at = std::size_t(y) * std::size_t(W) + std::size_t(x);
                if (nearness[at] > 0.0f)
                    continue;
                const std::size_t l = at - 1, r = at + 1, u = at - std::size_t(W), d = at + std::size_t(W);
                std::size_t from = at;
                if (nearness[l] > 0.0f && nearness[r] > 0.0f)
                    from = nearness[l] > nearness[r] ? l : r;
                else if (nearness[u] > 0.0f && nearness[d] > 0.0f)
                    from = nearness[u] > nearness[d] ? u : d;
                if (from != at) {
                    nearFixed[at] = nearness[from];
                    colourFixed[at] = colour[from];
                }
            }
        nearness.swap(nearFixed);
        colour.swap(colourFixed);
    }

    // The ground's soft shadow: where the model is above it, darker the closer it is.
    constexpr int kGrid = 256;
    const double span = radius * 2.6;
    const double gx0 = centre[0] - span / 2, gy0 = centre[1] - span / 2, cell = span / kGrid;
    std::vector<float> shade(kGrid * kGrid, 0.0f);
    if (s.shadow && !empty) {
        std::vector<float> lowest(kGrid * kGrid, 1e30f);
        for (const Item& it : items) {
            const auto& P = it.mesh.positions;
            const auto& I = it.mesh.indices;
            for (std::size_t t = 0; t * 3 + 2 < I.size(); ++t) {
                double cx[3], cy[3], cz[3];
                for (int k = 0; k < 3; ++k) {
                    const std::size_t v = I[t * 3 + std::size_t(k)];
                    cx[k] = (P[v * 3] - gx0) / cell;
                    cy[k] = (P[v * 3 + 1] - gy0) / cell;
                    cz[k] = P[v * 3 + 2];
                }
                const double area = (cx[1] - cx[0]) * (cy[2] - cy[0]) - (cx[2] - cx[0]) * (cy[1] - cy[0]);
                const int x0 = std::max(0, int(std::floor(std::min({cx[0], cx[1], cx[2]}))));
                const int x1 = std::min(kGrid - 1, int(std::ceil(std::max({cx[0], cx[1], cx[2]}))));
                const int y0 = std::max(0, int(std::floor(std::min({cy[0], cy[1], cy[2]}))));
                const int y1 = std::min(kGrid - 1, int(std::ceil(std::max({cy[0], cy[1], cy[2]}))));
                for (int y = y0; y <= y1; ++y)
                    for (int x = x0; x <= x1; ++x) {
                        double z = std::min({cz[0], cz[1], cz[2]});
                        if (std::abs(area) > 1e-12) {
                            const double px = x + 0.5, py = y + 0.5;
                            const double b0 = ((cx[1] - px) * (cy[2] - py) - (cx[2] - px) * (cy[1] - py)) / area;
                            const double b1 = ((cx[2] - px) * (cy[0] - py) - (cx[0] - px) * (cy[2] - py)) / area;
                            const double b2 = 1.0 - b0 - b1;
                            if (b0 < -1e-9 || b1 < -1e-9 || b2 < -1e-9)
                                continue;
                            z = b0 * cz[0] + b1 * cz[1] + b2 * cz[2];
                        } else if (x != int(cx[0]) || y != int(cy[0])) {
                            continue;
                        }
                        float& l = lowest[std::size_t(y * kGrid + x)];
                        l = std::min(l, float(z));
                    }
            }
        }
        for (std::size_t i = 0; i < shade.size(); ++i)
            if (lowest[i] < 1e29f)
                shade[i] = float(std::exp(-(lowest[i] - ground) / (0.35 * radius)));
        // Softened: three box blurs each way.
        const int r = std::max(1, int(0.06 * radius / cell));
        std::vector<float> tmp(shade.size());
        for (int pass = 0; pass < 3; ++pass)
            for (int dir = 0; dir < 2; ++dir) {
                for (int y = 0; y < kGrid; ++y)
                    for (int x = 0; x < kGrid; ++x) {
                        double sum = 0;
                        int count = 0;
                        for (int d = -r; d <= r; ++d) {
                            const int xx = dir == 0 ? x + d : x, yy = dir == 0 ? y : y + d;
                            if (xx < 0 || yy < 0 || xx >= kGrid || yy >= kGrid) {
                                ++count;
                                continue;
                            }
                            sum += shade[std::size_t(yy * kGrid + xx)];
                            ++count;
                        }
                        tmp[std::size_t(y * kGrid + x)] = float(sum / count);
                    }
                shade.swap(tmp);
            }
    }
    const auto shadowAt = [&](double wx, double wy) {
        const double fx = (wx - gx0) / cell - 0.5, fy = (wy - gy0) / cell - 0.5;
        const int x = int(std::floor(fx)), y = int(std::floor(fy));
        const double tx = fx - x, ty = fy - y;
        const auto get = [&](int xx, int yy) {
            return xx < 0 || yy < 0 || xx >= kGrid || yy >= kGrid ? 0.0 : double(shade[std::size_t(yy * kGrid + xx)]);
        };
        return (get(x, y) * (1 - tx) + get(x + 1, y) * tx) * (1 - ty) +
               (get(x, y + 1) * (1 - tx) + get(x + 1, y + 1) * tx) * ty;
    };

    // The background (and the shadow on it), then averaged down.
    Image out;
    out.width = s.width;
    out.height = s.height;
    out.pixels.assign(std::size_t(s.width) * std::size_t(s.height), 0u);
    const auto backdrop = [&](double v) -> std::array<double, 3> {  // v: 0 top .. 1 bottom
        switch (s.background) {
        case Background::White: return {1.0, 1.0, 1.0};
        case Background::Dark: return {0.23 - 0.11 * v, 0.25 - 0.12 * v, 0.27 - 0.13 * v};
        case Background::Clear: return {0.0, 0.0, 0.0};
        default: return {0.957 - 0.09 * v, 0.965 - 0.08 * v, 0.973 - 0.07 * v};
        }
    };
    const double strength = s.background == Background::Dark ? 0.6 : 0.5;
    for (int y = 0; y < s.height; ++y)
        for (int x = 0; x < s.width; ++x) {
            double a = 0, r = 0, g = 0, b = 0;
            for (int j = 0; j < S; ++j)
                for (int i = 0; i < S; ++i) {
                    const int X = x * S + i, Y = y * S + j;
                    const std::size_t at = std::size_t(Y) * std::size_t(W) + std::size_t(X);
                    if (nearness[at] > 0.0f) {
                        const std::uint32_t c = colour[at];
                        a += 1;
                        r += ((c >> 16) & 0xFF) / 255.0;
                        g += ((c >> 8) & 0xFF) / 255.0;
                        b += (c & 0xFF) / 255.0;
                        continue;
                    }
                    double dark = 0;
                    if (s.shadow) {
                        const V3 ray = unit(add(add(forward, mul(right, (X + 0.5 - W / 2.0) / focal)),
                                                mul(up, -(Y + 0.5 - H / 2.0) / focal)));
                        if (ray[2] < -1e-9) {
                            const double tHit = (ground - eye[2]) / ray[2];
                            if (tHit > 0)
                                dark = strength * shadowAt(eye[0] + ray[0] * tHit, eye[1] + ray[1] * tHit);
                        }
                    }
                    const auto bg = backdrop(double(Y) / H);
                    if (s.background == Background::Clear) {
                        a += dark;  // the shadow alone, as see-through black
                    } else {
                        a += 1;
                        r += bg[0] * (1 - dark);
                        g += bg[1] * (1 - dark);
                        b += bg[2] * (1 - dark);
                    }
                }
            const double k = double(S * S);
            if (a <= 0) {
                out.pixels[std::size_t(y) * std::size_t(s.width) + std::size_t(x)] = 0u;
                continue;
            }
            out.pixels[std::size_t(y) * std::size_t(s.width) + std::size_t(x)] = pack(a / k, r / a, g / a, b / a);
        }
    return out;
}

std::vector<Item> itemsOf(const Document& doc, const std::vector<ObjectId>& ids)
{
    std::vector<Item> out;
    for (ObjectId id : ids) {
        if (!doc.find(id))
            continue;
        Item it;
        it.colour = doc.colourOf(id);
        meshkernel::triangles(doc.meshSolid(id), it.mesh.positions, it.mesh.indices);
        const std::vector<PaintDab> dabs = paint::objectPaint(doc, id);
        if (!dabs.empty())
            it.painted = paint::triangleColours(it.mesh.positions, it.mesh.indices, paint::Field(dabs));
        const Transform world = doc.worldTransform(id);
        for (std::size_t i = 0; i + 2 < it.mesh.positions.size(); i += 3) {
            const Vec3 p = world.apply({it.mesh.positions[i], it.mesh.positions[i + 1], it.mesh.positions[i + 2]});
            it.mesh.positions[i] = float(p[0]);
            it.mesh.positions[i + 1] = float(p[1]);
            it.mesh.positions[i + 2] = float(p[2]);
        }
        out.push_back(std::move(it));
    }
    return out;
}

// ---- GIF ----

namespace {

struct Box {
    std::vector<std::array<std::uint8_t, 3>> colours;
};

// Up to `count` colours that stand for `samples` (median cut).
std::vector<std::array<std::uint8_t, 3>> palette(std::vector<std::array<std::uint8_t, 3>> samples, std::size_t count)
{
    std::vector<std::vector<std::array<std::uint8_t, 3>>> boxes;
    boxes.push_back(std::move(samples));
    while (boxes.size() < count) {
        // Split the box with the widest spread (by its size), along that.
        std::size_t best = boxes.size();
        int bestAxis = 0;
        double bestScore = 0;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if (boxes[i].size() < 2)
                continue;
            for (int axis = 0; axis < 3; ++axis) {
                int lo = 255, hi = 0;
                for (const auto& c : boxes[i]) {
                    lo = std::min(lo, int(c[std::size_t(axis)]));
                    hi = std::max(hi, int(c[std::size_t(axis)]));
                }
                const double score = double(hi - lo) * std::sqrt(double(boxes[i].size()));
                if (score > bestScore) {
                    bestScore = score;
                    best = i;
                    bestAxis = axis;
                }
            }
        }
        if (best == boxes.size() || bestScore <= 0)
            break;
        auto& box = boxes[best];
        const std::size_t mid = box.size() / 2;
        std::nth_element(box.begin(), box.begin() + std::ptrdiff_t(mid), box.end(),
                         [&](const auto& a, const auto& b) { return a[std::size_t(bestAxis)] < b[std::size_t(bestAxis)]; });
        std::vector<std::array<std::uint8_t, 3>> other(box.begin() + std::ptrdiff_t(mid), box.end());
        box.resize(mid);
        boxes.push_back(std::move(other));
    }
    std::vector<std::array<std::uint8_t, 3>> out;
    for (const auto& box : boxes) {
        if (box.empty())
            continue;
        double r = 0, g = 0, b = 0;
        for (const auto& c : box) {
            r += c[0];
            g += c[1];
            b += c[2];
        }
        const double n = double(box.size());
        out.push_back({std::uint8_t(r / n + 0.5), std::uint8_t(g / n + 0.5), std::uint8_t(b / n + 0.5)});
    }
    return out;
}

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& out) : m_out(out) {}
    void put(std::uint32_t code, int bits)
    {
        m_bits |= std::uint64_t(code) << m_count;
        m_count += bits;
        while (m_count >= 8) {
            byte(std::uint8_t(m_bits & 0xFF));
            m_bits >>= 8;
            m_count -= 8;
        }
    }
    void finish()
    {
        if (m_count > 0)
            byte(std::uint8_t(m_bits & 0xFF));
        m_bits = 0;
        m_count = 0;
        flush();
        m_out.push_back(0);  // no more blocks
    }

private:
    void byte(std::uint8_t b)
    {
        m_block.push_back(b);
        if (m_block.size() == 255)
            flush();
    }
    void flush()
    {
        if (m_block.empty())
            return;
        m_out.push_back(std::uint8_t(m_block.size()));
        m_out.insert(m_out.end(), m_block.begin(), m_block.end());
        m_block.clear();
    }
    std::vector<std::uint8_t>& m_out;
    std::vector<std::uint8_t> m_block;
    std::uint64_t m_bits = 0;
    int m_count = 0;
};

void lzw(const std::vector<std::uint8_t>& pixels, std::vector<std::uint8_t>& out)
{
    constexpr int kMin = 8;
    constexpr std::uint32_t kClear = 1u << kMin, kEnd = kClear + 1;
    out.push_back(kMin);
    BitWriter w(out);
    // (prefix, byte) -> code, stamped with the round it was made in (no clearing).
    static thread_local std::vector<std::uint32_t> table(4096u * 256u, 0u);
    static thread_local std::uint32_t round = 0;
    const auto reset = [&] {
        if (++round >= (1u << 20)) {
            std::fill(table.begin(), table.end(), 0u);
            round = 1;
        }
    };
    reset();
    std::uint32_t next = kEnd + 1;
    int bits = kMin + 1;
    w.put(kClear, bits);
    if (pixels.empty()) {
        w.put(kEnd, bits);
        w.finish();
        return;
    }
    std::uint32_t prefix = pixels[0];
    for (std::size_t i = 1; i < pixels.size(); ++i) {
        const std::uint8_t c = pixels[i];
        const std::uint32_t entry = table[std::size_t(prefix) * 256 + c];
        if ((entry >> 12) == round) {
            prefix = entry & 0xFFF;
            continue;
        }
        w.put(prefix, bits);
        if (next < 4096) {
            table[std::size_t(prefix) * 256 + c] = (round << 12) | next;
            if (next == (1u << bits) && bits < 12)
                ++bits;
            ++next;
        } else {
            w.put(kClear, bits);
            reset();
            next = kEnd + 1;
            bits = kMin + 1;
        }
        prefix = c;
    }
    w.put(prefix, bits);
    w.put(kEnd, bits);
    w.finish();
}

void word(std::vector<std::uint8_t>& out, int v)
{
    out.push_back(std::uint8_t(v & 0xFF));
    out.push_back(std::uint8_t((v >> 8) & 0xFF));
}

}  // namespace

std::vector<std::uint8_t> gif(const std::vector<Image>& frames, int delay, const std::function<void(double)>& progress)
{
    if (frames.empty())
        throw std::invalid_argument("There are no pictures to make a GIF of.");
    const int width = frames[0].width, height = frames[0].height;
    for (const Image& f : frames)
        if (f.width != width || f.height != height)
            throw std::invalid_argument("The pictures for a GIF must all be the same size.");
    // One palette for them all (no flicker), from a sample of every frame.
    bool clear = false;
    std::vector<std::array<std::uint8_t, 3>> samples;
    const std::size_t total = std::size_t(width) * std::size_t(height) * frames.size();
    const std::size_t stride = std::max<std::size_t>(1, total / 300000);
    std::size_t counter = 0;
    for (const Image& f : frames)
        for (std::uint32_t c : f.pixels) {
            if ((c >> 24) < 128) {
                clear = true;
                continue;
            }
            if (counter++ % stride == 0)
                samples.push_back({std::uint8_t(c >> 16), std::uint8_t(c >> 8), std::uint8_t(c)});
        }
    if (samples.empty())
        samples.push_back({255, 255, 255});
    std::vector<std::array<std::uint8_t, 3>> colours = palette(std::move(samples), clear ? 255 : 256);
    const std::uint8_t see = std::uint8_t(colours.size());  // the see-through index (if any)
    if (clear)
        colours.push_back({0, 0, 0});
    while (colours.size() < 256)
        colours.push_back({0, 0, 0});
    // Nearest palette colour, by 15-bit colour, worked out as needed.
    std::vector<int> nearest(32768, -1);
    const auto lookup = [&](int r, int g, int b) {
        const int key = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
        if (nearest[std::size_t(key)] < 0) {
            int best = 0;
            std::int64_t bestD = std::numeric_limits<std::int64_t>::max();
            const int cr = ((r >> 3) << 3) + 4, cg = ((g >> 3) << 3) + 4, cb = ((b >> 3) << 3) + 4;
            for (int i = 0; i < (clear ? int(see) : 256); ++i) {
                const std::int64_t dr = cr - colours[std::size_t(i)][0], dg = cg - colours[std::size_t(i)][1],
                                   db = cb - colours[std::size_t(i)][2];
                const std::int64_t d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
                if (d < bestD) {
                    bestD = d;
                    best = i;
                }
            }
            nearest[std::size_t(key)] = best;
        }
        return std::uint8_t(nearest[std::size_t(key)]);
    };
    static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

    std::vector<std::uint8_t> out{'G', 'I', 'F', '8', '9', 'a'};
    word(out, width);
    word(out, height);
    out.push_back(0xF7);  // a global table of 256 colours
    out.push_back(0);
    out.push_back(0);
    for (const auto& c : colours)
        out.insert(out.end(), {c[0], c[1], c[2]});
    // Loop for ever.
    out.insert(out.end(), {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00});
    std::vector<std::uint8_t> indices(std::size_t(width) * std::size_t(height));
    for (std::size_t n = 0; n < frames.size(); ++n) {
        const Image& f = frames[n];
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const std::uint32_t c = f.pixels[std::size_t(y) * std::size_t(width) + std::size_t(x)];
                if (clear && (c >> 24) < 128) {
                    indices[std::size_t(y) * std::size_t(width) + std::size_t(x)] = see;
                    continue;
                }
                const int d = (bayer[y & 3][x & 3] - 8);  // a little dither: smooth shading without bands
                indices[std::size_t(y) * std::size_t(width) + std::size_t(x)] =
                    lookup(std::clamp(int((c >> 16) & 0xFF) + d, 0, 255), std::clamp(int((c >> 8) & 0xFF) + d, 0, 255),
                           std::clamp(int(c & 0xFF) + d, 0, 255));
            }
        // How long it shows, and (see-through) the last one wiped first.
        out.insert(out.end(), {0x21, 0xF9, 0x04, std::uint8_t(clear ? 0x09 : 0x04)});
        word(out, std::clamp(delay, 1, 65535));
        out.push_back(clear ? see : 0);
        out.push_back(0);
        out.push_back(0x2C);
        word(out, 0);
        word(out, 0);
        word(out, width);
        word(out, height);
        out.push_back(0);
        lzw(indices, out);
        if (progress)
            progress(double(n + 1) / double(frames.size()));
    }
    out.push_back(0x3B);
    return out;
}

}  // namespace forge::render
