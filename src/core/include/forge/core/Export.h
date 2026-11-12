// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Tessellator.h"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace forge {

class Document;

// One shape, as triangles on the plate (millimetres), ready to write. Points are
// shared between neighbouring triangles ("welded"), as slicers expect.
struct ExportMesh {
    std::string name;
    std::vector<float> positions;       // 3 per point
    std::vector<std::uint32_t> indices;  // 3 per triangle, anticlockwise seen from outside
    std::size_t openEdges = 0;          // edges not shared by exactly two triangles (0 = watertight)

    std::size_t triangleCount() const { return indices.size() / 3; }
    double volume() const;  // mm^3; negative if inside out
};

enum class ExportFormat { Stl, ThreeMf, Obj };

// "Fine" is the default: 0.01 mm from the true surface and at most ~8.6 degrees between
// neighbouring facets on curves. "Normal" is coarser and smaller (faster slicing).
TessellationSettings exportQuality(bool fine = true);

// Triangles for each of these shapes (normally shapes on the plate), placed where
// they are on the plate.
std::vector<ExportMesh> exportMeshes(const Document& doc, const std::vector<ObjectId>& ids,
                                     const TessellationSettings& quality = exportQuality());

class ExportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Writes the meshes to `path` in `format` (STL: one binary STL holding everything;
// 3MF: one object per shape; OBJ: one "o" per shape). Writes to a temporary file
// first and swaps it in, so a failed export never leaves half a file. Throws
// ExportError with a message for the user.
void writeMeshes(const std::vector<ExportMesh>& meshes, const std::filesystem::path& path, ExportFormat format);

// The format implied by a file name's extension (.stl / .3mf / .obj, any case).
// Throws ExportError for anything else.
ExportFormat formatForPath(const std::filesystem::path& path);

}  // namespace forge
