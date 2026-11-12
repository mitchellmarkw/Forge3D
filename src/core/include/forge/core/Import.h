// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace forge {

class CommandStack;
class Document;

// Triangles as read from a file, before repair (millimetres, file coordinates).
struct RawMesh {
    std::string name;
    std::vector<float> positions;       // 3 per point
    std::vector<std::uint32_t> indices;  // 3 per triangle
};

class ImportError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Readers. STL: binary or text, one mesh. OBJ: one mesh per "o" / "g" group with
// faces (polygons are split into triangles). 3MF: one mesh per build item (components
// and transforms applied, units converted to millimetres). Throw ImportError.
std::vector<RawMesh> readStl(const std::string& bytes, const std::string& name);
std::vector<RawMesh> readObj(const std::string& text, const std::string& name);
std::vector<RawMesh> read3mf(const std::string& bytes, const std::string& name);
// By extension (.stl / .obj / .3mf).
std::vector<RawMesh> readMeshFile(const std::filesystem::path& path);

// What repair() did, for the user.
struct RepairReport {
    std::size_t trianglesIn = 0;
    std::size_t trianglesOut = 0;
    std::size_t pointsMerged = 0;         // duplicate points joined
    std::size_t degenerateRemoved = 0;    // triangles with no area
    std::size_t duplicatesRemoved = 0;    // the same triangle twice
    std::size_t trianglesFlipped = 0;     // turned to face the same way as their neighbours
    std::size_t partsTurnedOut = 0;       // whole pieces that were inside out
    std::size_t holesFilled = 0;
    std::size_t openEdges = 0;            // left after repair (0 = watertight)
    std::size_t nonManifoldEdges = 0;     // edges shared by more than two triangles

    bool changedAnything() const
    {
        return pointsMerged || degenerateRemoved || duplicatesRemoved || trianglesFlipped || partsTurnedOut ||
               holesFilled;
    }
    std::string summary() const;  // one or two plain sentences
};

// Repairs triangles for printing: joins duplicate points, removes empty and repeated
// triangles, makes neighbours face the same way and every piece face outwards, and
// fills holes. The result is centred on X / Y with its lowest point at Z = 0;
// `offset` gets where that origin was in the file's coordinates.
std::shared_ptr<const MeshData> repairMesh(const RawMesh& raw, RepairReport& report, Vec3& offset);

struct ImportResult {
    std::vector<ObjectId> bodies;          // the new shapes on the plate
    std::vector<RepairReport> reports;     // one per shape
    std::vector<std::string> names;
};

// Reads, repairs and adds a file's meshes as one undo step ("Import <file>"). Shapes
// from one file keep their places relative to each other; together they sit on the
// plate, to the right of anything already there.
ImportResult importFile(CommandStack& stack, Document& doc, const std::filesystem::path& path);

}  // namespace forge
