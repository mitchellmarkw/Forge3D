// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <filesystem>
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
inline constexpr int kFormatVersion = 2;
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
std::vector<Body> fromJson(const std::string& json);

}  // namespace forge::file
