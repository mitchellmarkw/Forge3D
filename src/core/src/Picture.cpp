// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Picture.h"

#include "forge/core/Document.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <unordered_map>

namespace forge::picture {

namespace {

using P2 = std::array<double, 2>;

double signedArea(const std::vector<P2>& loop)
{
    double a = 0.0;
    for (std::size_t i = 0, n = loop.size(); i < n; ++i) {
        const P2& p = loop[i];
        const P2& q = loop[(i + 1) % n];
        a += p[0] * q[1] - q[0] * p[1];
    }
    return a / 2.0;
}

bool inside(const std::vector<P2>& loop, const P2& p)
{
    bool in = false;
    for (std::size_t i = 0, n = loop.size(), j = n - 1; i < n; j = i++) {
        const P2& a = loop[i];
        const P2& b = loop[j];
        if ((a[1] > p[1]) != (b[1] > p[1]) && p[0] < (b[0] - a[0]) * (p[1] - a[1]) / (b[1] - a[1]) + a[0])
            in = !in;
    }
    return in;
}

double distanceToSegment(const P2& p, const P2& a, const P2& b)
{
    const double dx = b[0] - a[0], dy = b[1] - a[1];
    const double l2 = dx * dx + dy * dy;
    double t = l2 > 0.0 ? ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2 : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return std::hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy));
}

// Douglas–Peucker on the open run pts[from..to] (both kept), appending the kept points
// after `from` (to included).
void simplifyRun(const std::vector<P2>& pts, std::size_t from, std::size_t to, double tol, std::vector<P2>& out)
{
    std::vector<std::pair<std::size_t, std::size_t>> todo{{from, to}};
    std::vector<char> keep(pts.size(), 0);
    keep[to] = 1;
    while (!todo.empty()) {
        const auto [a, b] = todo.back();
        todo.pop_back();
        double worst = -1.0;
        std::size_t at = a;
        for (std::size_t i = a + 1; i < b; ++i) {
            const double d = distanceToSegment(pts[i], pts[a], pts[b]);
            if (d > worst) {
                worst = d;
                at = i;
            }
        }
        if (worst > tol) {
            keep[at] = 1;
            todo.push_back({a, at});
            todo.push_back({at, b});
        }
    }
    for (std::size_t i = from + 1; i <= to; ++i)
        if (keep[i])
            out.push_back(pts[i]);
}

std::vector<P2> simplifyLoop(const std::vector<P2>& loop, double tol)
{
    if (loop.size() < 4)
        return loop;
    // Split at the point furthest from the first, so both halves are open runs.
    std::size_t far = 0;
    double best = -1.0;
    for (std::size_t i = 1; i < loop.size(); ++i) {
        const double d = std::hypot(loop[i][0] - loop[0][0], loop[i][1] - loop[0][1]);
        if (d > best) {
            best = d;
            far = i;
        }
    }
    std::vector<P2> pts = loop;
    pts.push_back(loop.front());
    std::vector<P2> out{pts.front()};
    simplifyRun(pts, 0, far, tol, out);
    simplifyRun(pts, far, pts.size() - 1, tol, out);
    out.pop_back();  // the first again
    return out;
}

const char* kNothing = "There's nothing to emboss in this picture at this setting. Move the slider, or tick Invert.";
const char* kTooBusy = "This picture has too much fine detail to trace as a logo. Move the slider, use a simpler "
                       "picture, or choose Photo instead.";

}  // namespace

std::shared_ptr<const PictureData> fromRgba(int width, int height, const std::vector<std::uint8_t>& rgba)
{
    if (width <= 0 || height <= 0 || rgba.size() < std::size_t(width) * std::size_t(height) * 4)
        throw std::invalid_argument("The picture is empty");
    std::vector<double> grey(std::size_t(width) * std::size_t(height));
    for (std::size_t i = 0; i < grey.size(); ++i) {
        const double r = rgba[4 * i], g = rgba[4 * i + 1], b = rgba[4 * i + 2], a = rgba[4 * i + 3] / 255.0;
        const double y = 0.299 * r + 0.587 * g + 0.114 * b;
        grey[i] = y * a + 255.0 * (1.0 - a);  // over white
    }
    const int side = std::max(width, height);
    const double scale = side > kMaxSide ? double(kMaxSide) / side : 1.0;
    auto out = std::make_shared<PictureData>();
    out->width = std::max(1, int(std::lround(width * scale)));
    out->height = std::max(1, int(std::lround(height * scale)));
    out->grey.resize(std::size_t(out->width) * std::size_t(out->height));
    // Each new pixel: the average of the old pixels it covers.
    for (int y = 0; y < out->height; ++y) {
        const int y0 = int(std::floor(y / scale)), y1 = std::max(y0 + 1, std::min(height, int(std::floor((y + 1) / scale))));
        for (int x = 0; x < out->width; ++x) {
            const int x0 = int(std::floor(x / scale)),
                      x1 = std::max(x0 + 1, std::min(width, int(std::floor((x + 1) / scale))));
            double sum = 0.0;
            int count = 0;
            for (int yy = y0; yy < y1 && yy < height; ++yy)
                for (int xx = x0; xx < x1 && xx < width; ++xx) {
                    sum += grey[std::size_t(yy) * width + xx];
                    ++count;
                }
            out->grey[std::size_t(y) * out->width + x] = std::uint8_t(std::clamp(std::lround(sum / std::max(count, 1)), 0L, 255L));
        }
    }
    return out;
}

double heightOf(const EmbossPicture& picture)
{
    if (!picture.image || picture.image->width <= 0)
        return picture.width;
    return picture.width * double(picture.image->height) / double(picture.image->width);
}

std::vector<std::vector<P2>> outlines(const PictureData& image, int threshold, bool invert, double tolerance,
                                      double minArea)
{
    const int w = image.width, h = image.height;
    if (w <= 0 || h <= 0 || image.grey.size() < std::size_t(w) * std::size_t(h))
        return {};
    // A value at each pixel middle, above zero inside the shape; outside the picture is
    // outside. Corners run from -1 to w (and -1 to h).
    const double t = std::clamp(threshold, 1, 254) - 0.5;
    const auto value = [&](int x, int y) -> double {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return -255.0;
        const double g = image.grey[std::size_t(y) * w + x];
        return invert ? g - t : t - g;
    };
    const int cw = w + 2;  // corners across, from -1
    // A crossing on an edge between two corners: horizontal (x,y)-(x+1,y) or vertical
    // (x,y)-(x,y+1), keyed by the first corner.
    const auto key = [cw](int x, int y, bool vertical) {
        return (std::int64_t(y + 1) * cw + (x + 1)) * 2 + (vertical ? 1 : 0);
    };
    std::unordered_map<std::int64_t, P2> where;
    std::unordered_map<std::int64_t, std::int64_t> next;
    const auto crossing = [&](int x0, int y0, int x1, int y1) {
        const double a = value(x0, y0), b = value(x1, y1);
        const double f = a / (a - b);
        const std::int64_t k = key(std::min(x0, x1), std::min(y0, y1), x0 == x1);
        where.emplace(k, P2{x0 + 0.5 + f * (x1 - x0), y0 + 0.5 + f * (y1 - y0)});
        return k;
    };
    for (int y = -1; y < h; ++y)
        for (int x = -1; x < w; ++x) {
            // Corners round the cell: (x,y) (x+1,y) (x+1,y+1) (x,y+1).
            const int cx[4] = {x, x + 1, x + 1, x};
            const int cy[4] = {y, y, y + 1, y + 1};
            bool in[4];
            int count = 0;
            for (int k = 0; k < 4; ++k) {
                in[k] = value(cx[k], cy[k]) > 0.0;
                count += in[k];
            }
            if (count == 0 || count == 4)
                continue;
            // Crossings in order round the cell, and which way: into the shape or out of it.
            std::int64_t ks[4];
            bool intoShape[4];
            int n = 0;
            for (int k = 0; k < 4; ++k) {
                const int j = (k + 1) % 4;
                if (in[k] == in[j])
                    continue;
                ks[n] = crossing(cx[k], cy[k], cx[j], cy[j]);
                intoShape[n] = in[j];
                ++n;
            }
            // Each piece of outline runs from where the walk leaves the shape back to
            // where it came in (the shape always on the same side). Where two shape
            // corners face each other across the cell, the middle decides if they join.
            bool joinShape = false;
            if (n == 4) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k)
                    sum += value(cx[k], cy[k]);
                joinShape = sum > 0.0;
            }
            for (int i = 0; i < n; ++i) {
                if (intoShape[i])
                    continue;
                // i: leaving the shape. Its partner: the crossing into the shape before it
                // (cut off the shape corner) or after it (cut off the outside corner).
                const int partner = joinShape ? (i + 1) % n : (i + n - 1) % n;
                next[ks[i]] = ks[partner];
            }
        }
    std::vector<std::vector<P2>> loops;
    std::size_t total = 0;
    while (!next.empty()) {
        const auto first = next.begin()->first;
        std::vector<P2> loop;
        std::int64_t at = first;
        for (;;) {
            const auto it = next.find(at);
            if (it == next.end())
                break;
            loop.push_back(where.at(at));
            at = it->second;
            next.erase(it);
            if (at == first)
                break;
        }
        if (loop.size() < 3 || std::abs(signedArea(loop)) < minArea)
            continue;
        loop = simplifyLoop(loop, tolerance);
        if (loop.size() < 3 || std::abs(signedArea(loop)) < minArea)
            continue;
        total += loop.size();
        if (loops.size() > 5000 || total > 200000)
            throw GeometryError(kTooBusy);
        loops.push_back(std::move(loop));
    }
    return loops;
}

std::vector<text::FlatPiece> flatPieces(const EmbossPicture& picture, double maxEdge)
{
    if (!picture.image || picture.image->width <= 0 || picture.image->height <= 0)
        throw GeometryError("Choose a picture first.");
    if (!(picture.width > 0.0))
        throw std::invalid_argument("The picture must be wider than zero");
    if (!(maxEdge > 0.0))
        throw std::invalid_argument("flatPieces: maxEdge must be above zero");
    const PictureData& image = *picture.image;
    const double w = image.width, h = image.height;
    const double mm = picture.width / w;  // per pixel
    const double a = picture.angle * std::numbers::pi / 180.0, ca = std::cos(a), sa = std::sin(a);

    if (picture.relief) {
        // A sheet of squares, at most 200 along the longer side (and no finer than the
        // pixels), each corner as high as the shade round it.
        const double cell = std::max({mm, std::max(w, h) * mm / 200.0, 1e-6});
        const int nx = std::max(1, int(std::ceil(picture.width / cell)));
        const int ny = std::max(1, int(std::ceil(h * mm / cell)));
        // Sums of the pixels above and left of each, for averages over any rectangle.
        std::vector<double> sum(std::size_t(image.width + 1) * (image.height + 1), 0.0);
        const auto S = [&](int x, int y) -> double& { return sum[std::size_t(y) * (image.width + 1) + x]; };
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x)
                S(x + 1, y + 1) = image.grey[std::size_t(y) * image.width + x] + S(x, y + 1) + S(x + 1, y) - S(x, y);
        const auto shadeAround = [&](double px, double py, double r) {
            const int x0 = std::clamp(int(std::floor(px - r)), 0, image.width - 1);
            const int x1 = std::clamp(int(std::ceil(px + r)), x0 + 1, image.width);
            const int y0 = std::clamp(int(std::floor(py - r)), 0, image.height - 1);
            const int y1 = std::clamp(int(std::ceil(py + r)), y0 + 1, image.height);
            return (S(x1, y1) - S(x0, y1) - S(x1, y0) + S(x0, y0)) / double((x1 - x0) * (y1 - y0));
        };
        text::FlatPiece sheet;
        const double halfW = picture.width / 2.0, halfH = h * mm / 2.0;
        const double radius = cell / mm / 2.0;  // pixels
        for (int j = 0; j <= ny; ++j)
            for (int i = 0; i <= nx; ++i) {
                const double x = std::min(i * cell, picture.width) - halfW;
                const double y = halfH - std::min(j * cell, h * mm);
                sheet.points.push_back({x * ca - y * sa, x * sa + y * ca});
                const double grey = shadeAround((x + halfW) / mm, (halfH - y) / mm, radius) / 255.0;
                sheet.heights.push_back(picture.invert ? grey : 1.0 - grey);
            }
        const auto at = [nx](int i, int j) { return std::uint32_t(j * (nx + 1) + i); };
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                // Rows go down (-y): anticlockwise from +Z is (i,j+1) (i+1,j+1) (i+1,j) ...
                sheet.triangles.insert(sheet.triangles.end(), {at(i, j + 1), at(i + 1, j + 1), at(i + 1, j)});
                sheet.triangles.insert(sheet.triangles.end(), {at(i, j + 1), at(i + 1, j), at(i, j)});
            }
        return {std::move(sheet)};
    }

    // Logo: the outlines in mm (y up), nested: even depth an outside, odd a hole.
    auto loops = outlines(image, picture.threshold, picture.invert);
    if (loops.empty())
        throw GeometryError(kNothing);
    for (auto& loop : loops)
        for (auto& p : loop)
            p = {(p[0] - w / 2.0) * mm, (h / 2.0 - p[1]) * mm};
    const std::size_t n = loops.size();
    std::vector<double> area(n);
    for (std::size_t i = 0; i < n; ++i)
        area[i] = std::abs(signedArea(loops[i]));
    // The parent: the smallest other loop round it.
    std::vector<std::int64_t> parent(n, -1);
    for (std::size_t i = 0; i < n; ++i) {
        double best = 1e300;
        for (std::size_t j = 0; j < n; ++j)
            if (j != i && area[j] > area[i] && area[j] < best && inside(loops[j], loops[i].front())) {
                best = area[j];
                parent[i] = std::int64_t(j);
            }
    }
    std::vector<int> depth(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::int64_t p = parent[i]; p >= 0; p = parent[std::size_t(p)])
            ++depth[i];
    const gp_Pln plane;
    const auto wireOf = [](const std::vector<P2>& loop, bool anticlockwise) {
        std::vector<P2> pts = loop;
        if ((signedArea(pts) > 0.0) != anticlockwise)
            std::reverse(pts.begin(), pts.end());
        BRepBuilderAPI_MakePolygon poly;
        for (const P2& p : pts)
            poly.Add(gp_Pnt(p[0], p[1], 0.0));
        poly.Close();
        return poly.Wire();
    };
    BRep_Builder builder;
    TopoDS_Compound faces;
    builder.MakeCompound(faces);
    bool any = false;
    for (std::size_t i = 0; i < n; ++i) {
        if (depth[i] % 2 != 0)
            continue;
        BRepBuilderAPI_MakeFace face(plane, wireOf(loops[i], true), Standard_True);
        if (!face.IsDone())
            continue;
        for (std::size_t j = 0; j < n; ++j)
            if (parent[j] == std::int64_t(i))
                face.Add(wireOf(loops[j], false));
        if (!face.IsDone())
            continue;
        builder.Add(faces, face.Face());
        any = true;
    }
    if (!any)
        throw GeometryError(kNothing);
    auto pieces = text::flatPieces(faces, 0.0, 0.0, picture.angle, maxEdge, std::max(mm * 0.1, 1e-4));
    if (pieces.empty())
        throw GeometryError(kNothing);
    return pieces;
}

}  // namespace forge::picture
