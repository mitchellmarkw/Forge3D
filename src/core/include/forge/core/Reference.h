// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Transform.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Reference images: a photo or drawing standing behind the model, to build it to.
// One picture per direction (front, back, top, bottom, left, right). They are kept in
// the .f3d file but are never part of the model: nothing exported or printed has them.
namespace forge {

// Which way a picture faces: the view it is seen from (Front looks from -Y, Z up,
// like the view cube).
enum class RefView { Front, Back, Top, Bottom, Left, Right };
inline constexpr int kRefViewCount = 6;

struct ReferenceImage {
    RefView view = RefView::Front;
    std::string name;    // the file it came from, shown in the list
    std::string suffix;  // the picture file's kind: "png", "jpg"...
    // The picture file exactly as it was read (shared: undo steps don't copy it).
    std::shared_ptr<const std::string> data;
    int pixelsWide = 0, pixelsHigh = 0;
    // Size: millimetres on the model per pixel of the picture.
    double mmPerPixel = 0.1;
    // Where the picture's middle sits, mm, along the view's across (right) and up.
    double across = 0.0, up = 0.0;
    double turn = 0.0;   // degrees, anticlockwise as seen from its view
    double depth = 0.0;  // how far behind the plate's middle, mm (away from the viewer)
    double opacity = 0.6;
    bool shown = true;
    // An inactive picture can't be clicked: clicks go straight through it, to the
    // shapes and the plate behind.
    bool active = true;

    double widthMm() const { return pixelsWide * mmPerPixel; }
    double heightMm() const { return pixelsHigh * mmPerPixel; }
};

namespace reference {

// The view's directions in plate coordinates: right and up on the screen, and from the
// model towards the eye.
struct Frame {
    Vec3 right, up, toward;
};
Frame frameOf(RefView view);
// "front", "back"... (the file's names) and back again.
const char* key(RefView view);
std::optional<RefView> fromKey(const std::string& key);
// "Front", "Back"... for showing.
const char* title(RefView view);

// The picture's own right and up in plate coordinates (the view's, turned by `turn`).
Frame pictureFrame(const ReferenceImage& image);
// The point on the picture plane at a pixel (x right, y down from the top-left
// corner; fractions allowed), in plate mm.
Vec3 pointAt(const ReferenceImage& image, double px, double py);
// Corners in plate mm: top-left, top-right, bottom-right, bottom-left.
std::array<Vec3, 4> corners(const ReferenceImage& image);

// Where a ray (plate mm) meets the picture, from its front (a picture is seen from
// its own side only): how far along the ray, and the pixel. Nothing if it misses.
struct Hit {
    double distance = 0.0;
    double px = 0.0, py = 0.0;
};
std::optional<Hit> hit(const ReferenceImage& image, const Vec3& origin, const Vec3& direction);

// The nearest active, shown picture under a ray. Inactive and hidden ones are passed
// over. Returns its index in `images`.
std::optional<std::size_t> pick(const std::vector<ReferenceImage>& images, const Vec3& origin,
                                const Vec3& direction, Hit* where = nullptr);

// Sizes the picture so the distance between pixels a and b is `mm` on the model, and
// keeps the point under `a` where it was.
// Throws std::invalid_argument if the points are the same or mm isn't positive.
ReferenceImage calibrated(const ReferenceImage& image, std::array<double, 2> a, std::array<double, 2> b, double mm);

// Units a length can be typed in.
enum class Unit { Millimetres, Centimetres, Metres, Inches, Feet };
double toMm(double value, Unit unit);
// The length on the model for a measured length: a real-world size shown at a scale
// of 1 : `scale` (1 = full size).
double modelLength(double value, Unit unit, double scale);

// A picture's starting size and place: its longest side about `fit` mm, centred over
// the plate's middle (sitting on the plate for the side views), `behind` mm back.
ReferenceImage placed(ReferenceImage image, double fit, double behind);

// Checks a set of pictures: one per direction, a size, a picture file. Throws
// std::invalid_argument when not.
void validate(const std::vector<ReferenceImage>& images);

// The picture for a direction, if there is one.
const ReferenceImage* find(const std::vector<ReferenceImage>& images, RefView view);

}  // namespace reference
}  // namespace forge
