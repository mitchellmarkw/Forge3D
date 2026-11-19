// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Reference.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace forge::reference {
namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

struct Names {
    RefView view;
    const char* key;
    const char* title;
};
constexpr Names kNames[] = {
    {RefView::Front, "front", "Front"}, {RefView::Back, "back", "Back"},     {RefView::Top, "top", "Top"},
    {RefView::Bottom, "bottom", "Bottom"}, {RefView::Left, "left", "Left"}, {RefView::Right, "right", "Right"},
};

bool sideView(RefView view) { return view != RefView::Top && view != RefView::Bottom; }

// The picture's middle, plate mm.
Vec3 centreOf(const ReferenceImage& image)
{
    const Frame f = frameOf(image.view);
    return add(add(mul(f.right, image.across), mul(f.up, image.up)), mul(f.toward, -image.depth));
}

}  // namespace

Frame frameOf(RefView view)
{
    // The same directions as the view cube's faces.
    switch (view) {
    case RefView::Front:
        return {{1, 0, 0}, {0, 0, 1}, {0, -1, 0}};
    case RefView::Back:
        return {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}};
    case RefView::Left:
        return {{0, -1, 0}, {0, 0, 1}, {-1, 0, 0}};
    case RefView::Right:
        return {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
    case RefView::Top:
        return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    case RefView::Bottom:
        return {{1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
    }
    return {{1, 0, 0}, {0, 0, 1}, {0, -1, 0}};
}

const char* key(RefView view)
{
    for (const Names& n : kNames)
        if (n.view == view)
            return n.key;
    return "front";
}

std::optional<RefView> fromKey(const std::string& text)
{
    for (const Names& n : kNames)
        if (text == n.key)
            return n.view;
    return std::nullopt;
}

const char* title(RefView view)
{
    for (const Names& n : kNames)
        if (n.view == view)
            return n.title;
    return "Front";
}

Frame pictureFrame(const ReferenceImage& image)
{
    const Frame f = frameOf(image.view);
    const double a = image.turn * std::numbers::pi / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    // Anticlockwise as seen by the viewer, who looks against `toward`.
    return {add(mul(f.right, c), mul(f.up, s)), add(mul(f.right, -s), mul(f.up, c)), f.toward};
}

Vec3 pointAt(const ReferenceImage& image, double px, double py)
{
    const Frame f = pictureFrame(image);
    const double m = image.mmPerPixel;
    return add(centreOf(image), add(mul(f.right, (px - image.pixelsWide / 2.0) * m),
                                     mul(f.up, (image.pixelsHigh / 2.0 - py) * m)));
}

std::array<Vec3, 4> corners(const ReferenceImage& image)
{
    const double w = image.pixelsWide, h = image.pixelsHigh;
    return {pointAt(image, 0, 0), pointAt(image, w, 0), pointAt(image, w, h), pointAt(image, 0, h)};
}

std::optional<Hit> hit(const ReferenceImage& image, const Vec3& origin, const Vec3& direction)
{
    if (image.pixelsWide <= 0 || image.pixelsHigh <= 0 || !(image.mmPerPixel > 0))
        return std::nullopt;
    const Frame f = pictureFrame(image);
    const double facing = dot(direction, f.toward);
    if (facing > -1e-9)
        return std::nullopt;  // seen edge-on or from behind
    const Vec3 centre = centreOf(image);
    const double t = dot(sub(centre, origin), f.toward) / facing;
    if (t < 0)
        return std::nullopt;
    const Vec3 at = sub(add(origin, mul(direction, t)), centre);
    const double px = dot(at, f.right) / image.mmPerPixel + image.pixelsWide / 2.0;
    const double py = image.pixelsHigh / 2.0 - dot(at, f.up) / image.mmPerPixel;
    if (px < 0 || py < 0 || px > image.pixelsWide || py > image.pixelsHigh)
        return std::nullopt;
    return Hit{t, px, py};
}

std::optional<std::size_t> pick(const std::vector<ReferenceImage>& images, const Vec3& origin,
                                const Vec3& direction, Hit* where)
{
    std::optional<std::size_t> best;
    Hit bestHit;
    for (std::size_t i = 0; i < images.size(); ++i) {
        const ReferenceImage& image = images[i];
        if (!image.active || !image.shown)
            continue;
        const auto h = hit(image, origin, direction);
        if (h && (!best || h->distance < bestHit.distance)) {
            best = i;
            bestHit = *h;
        }
    }
    if (best && where)
        *where = bestHit;
    return best;
}

ReferenceImage calibrated(const ReferenceImage& image, std::array<double, 2> a, std::array<double, 2> b, double mm)
{
    const double pixels = std::hypot(b[0] - a[0], b[1] - a[1]);
    if (!(pixels > 1e-6))
        throw std::invalid_argument("Pick two different points on the picture.");
    if (!(mm > 0) || !std::isfinite(mm))
        throw std::invalid_argument("The length must be more than 0.");
    const Vec3 keep = pointAt(image, a[0], a[1]);
    ReferenceImage out = image;
    out.mmPerPixel = mm / pixels;
    // Slide the picture so the point under `a` is back where it was.
    const Vec3 moved = pointAt(out, a[0], a[1]);
    const Frame f = frameOf(out.view);
    const Vec3 shift = sub(keep, moved);
    out.across += dot(shift, f.right);
    out.up += dot(shift, f.up);
    return out;
}

double toMm(double value, Unit unit)
{
    switch (unit) {
    case Unit::Millimetres:
        return value;
    case Unit::Centimetres:
        return value * 10.0;
    case Unit::Metres:
        return value * 1000.0;
    case Unit::Inches:
        return value * 25.4;
    case Unit::Feet:
        return value * 304.8;
    }
    return value;
}

double modelLength(double value, Unit unit, double scale)
{
    if (!(scale > 0))
        throw std::invalid_argument("The scale must be more than 0.");
    return toMm(value, unit) / scale;
}

ReferenceImage placed(ReferenceImage image, double fit, double behind)
{
    const int longest = std::max(image.pixelsWide, image.pixelsHigh);
    if (longest > 0 && fit > 0)
        image.mmPerPixel = fit / longest;
    image.turn = 0.0;
    image.across = 0.0;
    // Standing on the plate when seen from the side; over its middle from above or below.
    image.up = sideView(image.view) ? image.heightMm() / 2.0 : 0.0;
    image.depth = behind;
    return image;
}

void validate(const std::vector<ReferenceImage>& images)
{
    bool seen[kRefViewCount] = {};
    for (const ReferenceImage& image : images) {
        const int v = static_cast<int>(image.view);
        if (v < 0 || v >= kRefViewCount)
            throw std::invalid_argument("A reference image faces no known direction.");
        if (seen[v])
            throw std::invalid_argument(std::string("There is already a ") + title(image.view) + " picture.");
        seen[v] = true;
        if (image.pixelsWide <= 0 || image.pixelsHigh <= 0)
            throw std::invalid_argument("A reference image has no size.");
        if (!(image.mmPerPixel > 0) || !std::isfinite(image.mmPerPixel))
            throw std::invalid_argument("A reference image's scale must be more than 0.");
        if (!image.data || image.data->empty())
            throw std::invalid_argument("A reference image has no picture.");
        for (double d : {image.across, image.up, image.turn, image.depth, image.opacity})
            if (!std::isfinite(d))
                throw std::invalid_argument("A reference image has a broken number.");
        if (image.opacity < 0.0 || image.opacity > 1.0)
            throw std::invalid_argument("A reference image's opacity must be from 0 to 1.");
    }
}

const ReferenceImage* find(const std::vector<ReferenceImage>& images, RefView view)
{
    for (const ReferenceImage& image : images)
        if (image.view == view)
            return &image;
    return nullptr;
}

}  // namespace forge::reference
