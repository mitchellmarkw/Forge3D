// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace forge {
class Document;
}

// The .f3d file: a zip archive holding document.json (and, later, sketches,
// sculpt layers and a preview model). A zip is easy to inspect and lets us add
// parts without breaking older files.
namespace forge::file {

// Version history (readers must keep accepting every older version):
//   1  slice 3a: box and cylinder, placement x/y/z + rotationZ
//   2  rotationX and rotationY added (rotation about all three axes)
//   3  cylinders store radiusX and radiusY (elliptical cylinders)
//   4  combine results (type "combine": operation, operands, pivot)
//   5  sphere, cone, torus, tube, wedge
//   6  oval sphere / cone / torus / tube (X / Y radii), pyramid
//   7  split results (operation "split", planePoint, planeNormal)
//   8  mirror results (operation "mirror", plane) and groups (operation "group")
//   9  imported meshes (type "mesh", width / depth / height, "data": zip entry name)
//  10  text (type "text": text, font, bold, italic) and emboss results (operation
//      "emboss", planePoint / planeNormal, "emboss": the words)
//  11  edge finishes (operation "edges"; "edges": kind, size, angle, picks)
//  12  steps switched off: "suppressed" / "rolledBack" on results (missing = false)
inline constexpr int kFormatVersion = 12;
inline constexpr const char* kExtension = ".f3d";
inline constexpr const char* kDocumentEntry = "document.json";

// Thrown for anything the user should be told about; what() is a plain-language
// message that can be shown in a dialog as it is.
class FileError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Writes the document. The old file is replaced only once the new one is
// complete, so a failed save never destroys the previous version.
void save(const Document& doc, const std::filesystem::path& path, const std::string& createdBy);

// Reads a document's bodies. Use Document::replaceAll() to show them.
std::vector<Body> load(const std::filesystem::path& path);

// The JSON part on its own (for tests and tools).
std::string toJson(const Document& doc, const std::string& createdBy);
// Imported meshes keep their triangles in separate zip entries ("meshes/7.bin");
// `readEntry` returns an entry's bytes. Without it, a file with meshes is refused.
std::vector<Body> fromJson(const std::string& json,
                           const std::function<std::string(const std::string&)>& readEntry = {});

// The binary form of an imported mesh's triangles (the "meshes/<id>.bin" entries).
std::string meshToBytes(const MeshData& mesh);
std::shared_ptr<const MeshData> meshFromBytes(const std::string& bytes);

}  // namespace forge::file
