// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace forge {

class CommandStack;
class Document;

namespace emboss {

// A spot on a surface and the way the surface faces there (unit, pointing out).
struct SurfacePoint {
    Vec3 point{0.0, 0.0, 0.0};
    Vec3 normal{0.0, 0.0, 1.0};
};

// Where a ray first meets the triangles, and the surface's facing there averaged
// over `radius` (so a bumpy scan gives a steady direction). Nothing if it misses.
std::optional<SurfacePoint> surfaceAt(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices,
                                      const Vec3& origin, const Vec3& direction, double radius);

// Which way is "up" for words on a surface facing `normal`: towards +Z where the
// surface is a side, towards +Y (the back) on a top or bottom.
Vec3 upFor(const Vec3& normal);

// The letters as a closed solid lying on the surface (positions / indices: a closed
// triangle surface) around `point`, read from outside along `normal`. Raised letters
// stand out `depth` and reach a little into the surface (so merging joins them);
// engraved ones go `depth` in and start a little above it (so subtracting cuts cleanly).
// Throws GeometryError if part of the words would hang off the surface.
MeshSolid letters(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices, const Vec3& point,
                  const Vec3& normal, const EmbossText& words);

// Centring the words on the face they're on: across, up and down, or both, as the
// words read (their turn included).
enum class Center { Both, TopBottom, LeftRight };

// The spot moved to the middle of the face under it: the triangles joined smoothly to
// the one at the spot (bending less than 35° from one to the next), so a box's face or
// a cylinder's curved side. Nothing if the middle isn't on that face.
std::optional<SurfacePoint> centeredOnFace(const std::vector<float>& positions,
                                           const std::vector<std::uint32_t>& indices, const SurfacePoint& spot,
                                           double angleDegrees, Center how, double radius);

}  // namespace emboss

// The Emboss result for `target` (a shape on the plate) with the words at a spot on
// it, given in plate coordinates. Not added: preview it with Document::previewMesh,
// then add it with CombineCommand.
Body embossBody(Document& doc, ObjectId target, const emboss::SurfacePoint& where, const EmbossText& words);

// centeredOnFace for a shape in the document (or the part an Emboss result works on),
// with the spot in plate coordinates.
std::optional<emboss::SurfacePoint> centeredSpot(const Document& doc, ObjectId id, const emboss::SurfacePoint& spot,
                                                 const EmbossText& words, emboss::Center how);
// Just its recipe (for previews): changes nothing in the document.
BooleanSpec embossSpec(const Document& doc, ObjectId target, const emboss::SurfacePoint& where,
                       const EmbossText& words);

// Where the words of an Emboss result are, in plate coordinates.
emboss::SurfacePoint embossSpot(const Document& doc, ObjectId result);
// The Emboss result with new words and / or a new spot (plate coordinates).
Body withEmboss(const Document& doc, ObjectId result, const emboss::SurfacePoint& where, const EmbossText& words);

// Where a ray from the plate meets the surface of `id` (a shape, or the part being
// embossed in an Emboss result), in plate coordinates.
std::optional<emboss::SurfacePoint> surfaceUnder(const Document& doc, ObjectId id, const Vec3& origin,
                                                 const Vec3& direction, double radius);

}  // namespace forge
