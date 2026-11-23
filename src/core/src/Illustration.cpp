// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Illustration.h"

#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace forge::illustrate {

namespace {

using V3 = std::array<double, 3>;

V3 sub(const V3& a, const V3& b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
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
    return l > 0 ? V3{a[0] / l, a[1] / l, a[2] / l} : V3{0, 0, 0};
}

struct Camera {
    V3 right, up, dir;  // dir: from the eye into the scene
};

Camera camera(View v)
{
    V3 d{0, 1, 0}, hint{0, 0, 1};
    switch (v) {
    case View::Front: d = {0, 1, 0}; break;
    case View::Back: d = {0, -1, 0}; break;
    case View::Left: d = {1, 0, 0}; break;  // from -X, as the view cube's Left
    case View::Right: d = {-1, 0, 0}; break;
    case View::Top:
        d = {0, 0, -1};
        hint = {0, 1, 0};
        break;
    case View::ThreeQuarter: d = unit({-0.55, 1.0, -0.45}); break;
    }
    Camera c;
    c.dir = unit(d);
    c.right = unit(cross(c.dir, hint));
    c.up = unit(cross(c.right, c.dir));
    return c;
}

constexpr int kSuper = 2;  // drawn this many times bigger, then averaged down (smooth edges)

}  // namespace

const char* title(View v)
{
    switch (v) {
    case View::ThreeQuarter: return "Three-quarter";
    case View::Left: return "Left";
    case View::Right: return "Right";
    case View::Back: return "Back";
    case View::Top: return "Top";
    default: return "Front";
    }
}

Picture render(const std::vector<Item>& items, View view, int width, int height)
{
    if (width < 64 || height < 64 || width > 8000 || height > 8000)
        throw std::invalid_argument("The picture must be from 64 to 8000 pixels each way.");
    Picture pic;
    pic.width = width;
    pic.height = height;
    pic.pixels.assign(std::size_t(width) * std::size_t(height), 0u);
    pic.balloon = std::max(11.0, std::min(width, height) * 0.022);

    const Camera cam = camera(view);
    // Where everything lands on the picture.
    double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300, z0 = 1e300, z1 = -1e300;
    for (const Item& it : items)
        for (std::size_t i = 0; i + 2 < it.mesh.positions.size(); i += 3) {
            const V3 p{it.mesh.positions[i], it.mesh.positions[i + 1], it.mesh.positions[i + 2]};
            const double sx = dot(p, cam.right), sy = dot(p, cam.up), sz = dot(p, cam.dir);
            x0 = std::min(x0, sx), x1 = std::max(x1, sx);
            y0 = std::min(y0, sy), y1 = std::max(y1, sy);
            z0 = std::min(z0, sz), z1 = std::max(z1, sz);
        }
    if (x0 > x1)
        return pic;  // nothing to draw
    const double pad = pic.balloon * 4.0;
    const double spanX = std::max(x1 - x0, 1e-6), spanY = std::max(y1 - y0, 1e-6);
    const double scale = std::min((width - 2 * pad) / spanX, (height - 2 * pad) / spanY);
    if (!(scale > 0))
        throw std::invalid_argument("The picture is too small for the balloons.");
    const double offX = (width - spanX * scale) / 2.0, offY = (height - spanY * scale) / 2.0;

    const int W = width * kSuper, H = height * kSuper;
    std::vector<float> depth(std::size_t(W) * std::size_t(H), std::numeric_limits<float>::infinity());
    std::vector<int> owner(std::size_t(W) * std::size_t(H), -1);
    std::vector<std::uint32_t> colour(std::size_t(W) * std::size_t(H), 0u);
    const V3 light = unit({-cam.right[0] * 0.45 + cam.up[0] * 0.6 - cam.dir[0] * 0.75,
                           -cam.right[1] * 0.45 + cam.up[1] * 0.6 - cam.dir[1] * 0.75,
                           -cam.right[2] * 0.45 + cam.up[2] * 0.6 - cam.dir[2] * 0.75});

    for (std::size_t n = 0; n < items.size(); ++n) {
        const Item& it = items[n];
        const auto& P = it.mesh.positions;
        const auto& I = it.mesh.indices;
        // The part's colour, lightened a little (instructions are drawn light).
        const double cr = ((it.colour >> 16) & 0xFF) / 255.0, cg = ((it.colour >> 8) & 0xFF) / 255.0,
                     cb = (it.colour & 0xFF) / 255.0;
        const double lr = cr * 0.75 + 0.25, lg = cg * 0.75 + 0.25, lb = cb * 0.75 + 0.25;
        for (std::size_t t = 0; t + 2 < I.size(); t += 3) {
            V3 w[3];
            double sx[3], sy[3], sz[3];
            for (int k = 0; k < 3; ++k) {
                const std::size_t v = std::size_t(I[t + std::size_t(k)]) * 3;
                if (v + 2 >= P.size())
                    goto next;
                w[k] = {P[v], P[v + 1], P[v + 2]};
                sx[k] = ((dot(w[k], cam.right) - x0) * scale + offX) * kSuper;
                sy[k] = ((y1 - dot(w[k], cam.up)) * scale + offY) * kSuper;
                sz[k] = dot(w[k], cam.dir);
            }
            {
                V3 nrm = unit(cross(sub(w[1], w[0]), sub(w[2], w[0])));
                if (dot(nrm, cam.dir) > 0)
                    nrm = {-nrm[0], -nrm[1], -nrm[2]};  // seen from either side
                const double shade = 0.42 + 0.58 * std::max(0.0, dot(nrm, light));
                const auto ch = [&](double c) { return std::uint32_t(std::clamp(c * shade, 0.0, 1.0) * 255.0 + 0.5); };
                const std::uint32_t argb = 0xFF000000u | (ch(lr) << 16) | (ch(lg) << 8) | ch(lb);
                const double area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
                if (std::abs(area) < 1e-12)
                    goto next;
                const int bx0 = std::max(0, int(std::floor(std::min({sx[0], sx[1], sx[2]}))));
                const int bx1 = std::min(W - 1, int(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
                const int by0 = std::max(0, int(std::floor(std::min({sy[0], sy[1], sy[2]}))));
                const int by1 = std::min(H - 1, int(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
                for (int y = by0; y <= by1; ++y)
                    for (int x = bx0; x <= bx1; ++x) {
                        const double px = x + 0.5, py = y + 0.5;
                        double b0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
                        double b1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
                        double b2 = 1.0 - b0 - b1;
                        if (b0 < -1e-9 || b1 < -1e-9 || b2 < -1e-9)
                            continue;
                        const float z = float(b0 * sz[0] + b1 * sz[1] + b2 * sz[2]);
                        const std::size_t at = std::size_t(y) * std::size_t(W) + std::size_t(x);
                        if (z < depth[at]) {
                            depth[at] = z;
                            owner[at] = int(n);
                            colour[at] = argb;
                        }
                    }
            }
        next:;
        }
    }

    // Outlines: where one part meets another or the background, or the same part folds
    // in front of itself (a jump in depth).
    const float jump = float(std::max(z1 - z0, std::max(spanX, spanY)) * 0.02);
    const std::uint32_t ink = 0xFF2B2F33u;
    std::vector<std::uint32_t> drawn = colour;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const std::size_t at = std::size_t(y) * std::size_t(W) + std::size_t(x);
            const int me = owner[at];
            bool edge = false;
            for (int r = 1; r <= kSuper && !edge; ++r)
                for (const auto& [dx, dy] : std::array<std::pair<int, int>, 4>{{{r, 0}, {-r, 0}, {0, r}, {0, -r}}}) {
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H)
                        continue;
                    const std::size_t o = std::size_t(ny) * std::size_t(W) + std::size_t(nx);
                    if (owner[o] != me) {
                        // Draw the line on the nearer side only (on the part, not the background).
                        if (me >= 0 && (owner[o] < 0 || depth[at] <= depth[o]))
                            edge = true;
                    } else if (me >= 0 && depth[o] - depth[at] > jump) {
                        edge = true;
                    }
                    if (edge)
                        break;
                }
            if (edge)
                drawn[at] = ink;
        }

    // Averaged down to the picture's size.
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            double a = 0, r = 0, g = 0, b = 0;
            for (int sy = 0; sy < kSuper; ++sy)
                for (int sx = 0; sx < kSuper; ++sx) {
                    const std::uint32_t c = drawn[std::size_t(y * kSuper + sy) * std::size_t(W) +
                                                  std::size_t(x * kSuper + sx)];
                    const double ca = ((c >> 24) & 0xFF) / 255.0;
                    a += ca;
                    r += ca * ((c >> 16) & 0xFF);
                    g += ca * ((c >> 8) & 0xFF);
                    b += ca * (c & 0xFF);
                }
            const double k = kSuper * kSuper;
            if (a <= 0)
                continue;
            const auto byte = [](double v) { return std::uint32_t(std::clamp(v, 0.0, 255.0) + 0.5); };
            pic.pixels[std::size_t(y) * std::size_t(width) + std::size_t(x)] =
                (byte(a / k * 255.0) << 24) | (byte(r / a) << 16) | (byte(g / a) << 8) | byte(b / a);
        }

    // Balloons: each points at a spot on its part that can be seen (near the middle of
    // what shows), and sits out beyond everything, away from the middle of the picture.
    const auto filled = [&](double px, double py) {
        const int x = int(px), y = int(py);
        if (x < 0 || y < 0 || x >= width || y >= height)
            return false;
        return (pic.pixels[std::size_t(y) * std::size_t(width) + std::size_t(x)] >> 24) > 0;
    };
    double cxAll = 0, cyAll = 0;
    {
        double count = 0;
        for (int y = 0; y < H; y += 2)
            for (int x = 0; x < W; x += 2)
                if (owner[std::size_t(y) * std::size_t(W) + std::size_t(x)] >= 0) {
                    cxAll += x;
                    cyAll += y;
                    ++count;
                }
        if (count > 0) {
            cxAll = cxAll / count / kSuper;
            cyAll = cyAll / count / kSuper;
        } else {
            cxAll = width / 2.0;
            cyAll = height / 2.0;
        }
    }
    const double R = pic.balloon;
    // For each part: the middle of what shows of it, then the shown point nearest that
    // (a little in from its edge, so the dot is clearly on it).
    const std::size_t count = items.size();
    std::vector<double> sumX(count, 0.0), sumY(count, 0.0), shown(count, 0.0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (const int o = owner[std::size_t(y) * std::size_t(W) + std::size_t(x)]; o >= 0) {
                sumX[std::size_t(o)] += x;
                sumY[std::size_t(o)] += y;
                shown[std::size_t(o)] += 1.0;
            }
    std::vector<double> best(count, 1e300), bestX(count, 0.0), bestY(count, 0.0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int o = owner[std::size_t(y) * std::size_t(W) + std::size_t(x)];
            if (o < 0)
                continue;
            const std::size_t k = std::size_t(o);
            const double mx = sumX[k] / shown[k], my = sumY[k] / shown[k];
            int inside = 0;
            for (const auto& [dx, dy] : std::array<std::pair<int, int>, 4>{{{3, 0}, {-3, 0}, {0, 3}, {0, -3}}}) {
                const int nx = x + dx, ny = y + dy;
                inside += nx >= 0 && ny >= 0 && nx < W && ny < H &&
                          owner[std::size_t(ny) * std::size_t(W) + std::size_t(nx)] == o;
            }
            const double d = (x - mx) * (x - mx) + (y - my) * (y - my) + (inside == 4 ? 0.0 : 1e12);
            if (d < best[k]) {
                best[k] = d;
                bestX[k] = x;
                bestY[k] = y;
            }
        }
    for (std::size_t n = 0; n < items.size(); ++n) {
        if (items[n].number <= 0 || shown[n] == 0)
            continue;  // no number, or hidden behind the others
        const double ax = bestX[n], ay = bestY[n];
        Label label;
        label.number = items[n].number;
        label.ax = (ax + 0.5) / kSuper;
        label.ay = (ay + 0.5) / kSuper;
        double dx = label.ax - cxAll, dy = label.ay - cyAll;
        if (std::hypot(dx, dy) < 1e-6)
            dx = 0, dy = -1;
        const double base = std::atan2(dy, dx);
        // Each way round: the nearest clear spot that way. The best is short, doesn't
        // cross other parts or other leaders, and leans away from the middle.
        const auto ownerAt = [&](double px, double py) {
            const int x = int(px * kSuper), y = int(py * kSuper);
            if (x < 0 || y < 0 || x >= W || y >= H)
                return -1;
            return owner[std::size_t(y) * std::size_t(W) + std::size_t(x)];
        };
        const auto crosses = [](double ax, double ay, double bx, double by, const Label& o) {
            const auto side = [](double px, double py, double qx, double qy, double rx, double ry) {
                return (qx - px) * (ry - py) - (qy - py) * (rx - px);
            };
            const double d1 = side(ax, ay, bx, by, o.ax, o.ay), d2 = side(ax, ay, bx, by, o.bx, o.by);
            const double d3 = side(o.ax, o.ay, o.bx, o.by, ax, ay), d4 = side(o.ax, o.ay, o.bx, o.by, bx, by);
            return d1 * d2 < 0 && d3 * d4 < 0;
        };
        bool placed = false;
        double bestCost = 1e300;
        constexpr int kWays = 24;
        for (int k = 0; k < kWays; ++k) {
            const double a = base + 2.0 * std::numbers::pi * k / kWays;
            const double ux = std::cos(a), uy = std::sin(a);
            for (double d = R * 1.5; d < std::max(width, height); d += 2.0) {
                const double bx = label.ax + ux * d, by = label.ay + uy * d;
                if (bx < R + 2 || by < R + 2 || bx > width - R - 2 || by > height - R - 2)
                    break;
                bool clear = !filled(bx, by);
                for (const Label& other : pic.labels)
                    if (clear && std::hypot(other.bx - bx, other.by - by) < 2 * R + 4)
                        clear = false;
                for (int j = 0; j < 16 && clear; ++j) {
                    const double t = 2 * std::numbers::pi * j / 16;
                    for (double rr : {R + 3, R * 0.5})
                        if (filled(bx + rr * std::cos(t), by + rr * std::sin(t)))
                            clear = false;
                }
                if (!clear)
                    continue;
                // Over other parts along the way (the leader hides them).
                double over = 0;
                for (double t = 0; t < d - R; t += 2.0) {
                    const int o = ownerAt(label.ax + ux * t, label.ay + uy * t);
                    over += (o >= 0 && o != int(n)) ? 2.0 : 0.0;
                }
                int crossings = 0;
                for (const Label& other : pic.labels)
                    crossings += crosses(label.ax, label.ay, bx, by, other);
                const double cost = d + over * 6.0 + crossings * 4.0 * R * 10.0 +
                                    R * 1.5 * (1.0 - std::cos(a - base));
                if (cost < bestCost) {
                    bestCost = cost;
                    label.bx = bx;
                    label.by = by;
                    placed = true;
                }
                break;  // the nearest clear spot this way
            }
        }
        if (!placed) {  // nowhere clear: just outside the dot
            label.bx = std::clamp(label.ax + R * 2, R, width - R);
            label.by = std::clamp(label.ay - R * 2, R, height - R);
        }
        pic.labels.push_back(label);
    }
    return pic;
}

std::vector<Item> exploded(const Document& doc, const std::vector<parts::Row>& rows, double spread)
{
    if (!(spread >= 0.0 && spread <= 5.0))
        throw std::invalid_argument("The spread must be from 0 to 500%.");
    std::vector<Item> out;
    std::vector<V3> centres;
    for (const parts::Row& r : rows) {
        if (r.number <= 0 || !doc.find(r.id))
            continue;
        Item it;
        it.number = r.number;
        it.colour = doc.colourOf(r.id);
        const MeshSolid solid = meshkernel::transformed(doc.meshSolid(r.id), doc.worldTransform(r.id));
        meshkernel::triangles(solid, it.mesh.positions, it.mesh.indices);
        Bounds b;
        for (std::size_t i = 0; i + 2 < it.mesh.positions.size(); i += 3)
            b.expand(it.mesh.positions[i], it.mesh.positions[i + 1], it.mesh.positions[i + 2]);
        const auto c = b.center();
        centres.push_back({c[0], c[1], c[2]});
        out.push_back(std::move(it));
    }
    if (out.empty())
        return out;
    V3 mid{0, 0, 0};
    for (const V3& c : centres)
        for (int k = 0; k < 3; ++k)
            mid[std::size_t(k)] += c[std::size_t(k)] / double(centres.size());
    for (std::size_t n = 0; n < out.size(); ++n) {
        const V3 move{(centres[n][0] - mid[0]) * spread, (centres[n][1] - mid[1]) * spread,
                      (centres[n][2] - mid[2]) * spread};
        auto& p = out[n].mesh.positions;
        for (std::size_t i = 0; i + 2 < p.size(); i += 3)
            for (int k = 0; k < 3; ++k)
                p[i + std::size_t(k)] = float(p[i + std::size_t(k)] + move[std::size_t(k)]);
    }
    return out;
}

}  // namespace forge::illustrate
