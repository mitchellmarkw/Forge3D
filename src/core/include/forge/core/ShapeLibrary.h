// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <filesystem>
#include <string>
#include <vector>

namespace forge {

class CommandStack;
class Document;

// Custom shapes: shapes saved once and added again and again at their exact size.
// Each is a small .f3d file in a library folder (named after the shape), holding the
// shapes with their whole history, so an added one can still be changed.
namespace library {

struct Entry {
    std::string name;             // as shown in the menu (the file's name)
    std::filesystem::path file;
};

// The shapes in `folder`, sorted by name (ignoring case). None if it doesn't exist.
std::vector<Entry> list(const std::filesystem::path& folder);

// The file name for a shape called `name`: characters a file name can't hold become
// '-', spaces and dots trimmed from the ends, at most 80 characters, then ".f3d".
// Throws std::invalid_argument if nothing is left.
std::string fileNameFor(const std::string& name);

// Saves shapes `ids` (on the plate) as `name` in `folder` (made if needed), replacing a
// shape of that name. They keep their sizes and the way they sit together; the group of
// them is centred on the plate's middle, resting on it. One shape is renamed `name`.
// Finished results already worked out are kept too, so a big sculpted shape comes in
// quickly. Returns the file.
std::filesystem::path save(const Document& doc, const std::vector<ObjectId>& ids,
                           const std::filesystem::path& folder, const std::string& name,
                           const std::string& createdBy);

// Adds the shapes saved in `file` to `doc` as one undo step ("Add <name>"), beside
// what's already on the plate, at their saved size. A single shape is named after the
// file ("Ball joint 3mm", then "Ball joint 3mm 2"...); several keep their own names.
// Returns the new shapes.
std::vector<ObjectId> add(CommandStack& stack, Document& doc, const std::filesystem::path& file);

// Renames a saved shape (its file). Throws std::runtime_error if the new name is taken.
std::filesystem::path rename(const std::filesystem::path& file, const std::string& newName);

}  // namespace library
}  // namespace forge
