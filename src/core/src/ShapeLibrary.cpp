// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/ShapeLibrary.h"

#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <system_error>

namespace forge::library {

namespace fs = std::filesystem;

namespace {

// A path from a name in UTF-8 (as names are kept everywhere else).
fs::path pathOf(const std::string& utf8)
{
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

// The shapes' combined box on the plate.
Bounds boundsOf(const Document& doc, const std::vector<ObjectId>& ids)
{
    Bounds all;
    for (ObjectId id : ids) {
        const Bounds b = doc.worldBounds(id);
        if (b.valid) {
            all.expand(b.min[0], b.min[1], b.min[2]);
            all.expand(b.max[0], b.max[1], b.max[2]);
        }
    }
    return all;
}

// `base` if no shape has that name, else "base 2", "base 3"...
std::string freeName(const Document& doc, const std::string& base, const std::set<std::string>& taken)
{
    const auto used = [&](const std::string& n) {
        if (taken.count(n))
            return true;
        return std::any_of(doc.bodies().begin(), doc.bodies().end(), [&](const Body& b) { return b.name == n; });
    };
    if (!used(base))
        return base;
    for (int n = 2;; ++n) {
        std::string candidate = base + " " + std::to_string(n);
        if (!used(candidate))
            return candidate;
    }
}

}  // namespace

std::vector<Entry> list(const fs::path& folder)
{
    std::vector<Entry> out;
    std::error_code ec;
    if (!fs::is_directory(folder, ec))
        return out;
    for (const auto& item : fs::directory_iterator(folder, ec)) {
        if (!item.is_regular_file(ec) || lower(item.path().extension().string()) != file::kExtension)
            continue;
        const auto stem = item.path().stem().u8string();
        out.push_back({std::string(stem.begin(), stem.end()), item.path()});
    }
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return lower(a.name) < lower(b.name); });
    return out;
}

std::string fileNameFor(const std::string& name)
{
    std::string out;
    for (const char ch : name) {
        const auto c = static_cast<unsigned char>(ch);
        const bool bad = c < 32 || std::string_view("<>:\"/\\|?*").find(ch) != std::string_view::npos;
        out.push_back(bad ? '-' : ch);
    }
    const auto trimmed = [](char c) { return c == ' ' || c == '.'; };
    while (!out.empty() && trimmed(out.back()))
        out.pop_back();
    std::size_t start = 0;
    while (start < out.size() && trimmed(out[start]))
        ++start;
    out = out.substr(start);
    if (out.size() > 80) {
        out.resize(80);
        // Not part way through a character of more than one byte (UTF-8).
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80)
            out.pop_back();
        if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0x80))
            out.pop_back();
        while (!out.empty() && trimmed(out.back()))
            out.pop_back();
    }
    if (out.empty())
        throw std::invalid_argument("A custom shape needs a name.");
    return out + file::kExtension;
}

fs::path save(const Document& doc, const std::vector<ObjectId>& ids, const fs::path& folder,
              const std::string& name, const std::string& createdBy)
{
    if (ids.empty())
        throw std::invalid_argument("Select the shapes to save first.");
    for (ObjectId id : ids) {
        if (!doc.find(id))
            throw std::out_of_range("No body with id " + std::to_string(id));
        if (!doc.isTopLevel(id))
            throw std::invalid_argument("'" + doc.find(id)->name + "' is part of another shape: select the whole shape.");
    }
    const fs::path target = folder / pathOf(fileNameFor(name));
    const Bounds box = boundsOf(doc, ids);
    if (!box.valid)
        throw std::invalid_argument("There's nothing to save in these shapes.");
    // Centred on the plate's middle, resting on it.
    const auto c = box.center();
    const Vec3 shift{-c[0], -c[1], -double(box.min[2])};

    Document scratch;
    for (ObjectId id : ids) {
        BodyTree tree = captureTree(doc, id);
        Body& root = tree.back();
        root.placement.x += shift[0];
        root.placement.y += shift[1];
        root.placement.z += shift[2];
        if (ids.size() == 1)
            root.name = name;
        for (Body& b : tree)
            scratch.insertBody(std::move(b));
        // Results already worked out (they don't depend on where the shape sits).
        if (const auto solid = doc.cachedSolid(id))
            scratch.seedSolid(id, *solid);
    }
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec)
        throw std::runtime_error("The custom shapes folder can't be made: " + ec.message());
    file::save(scratch, target, createdBy);
    return target;
}

std::vector<ObjectId> add(CommandStack& stack, Document& doc, const fs::path& path)
{
    Document scratch;
    scratch.replaceAll(file::load(path));
    file::useSavedResults(scratch, path);
    std::vector<ObjectId> roots;
    for (const Body& b : scratch.bodies())
        if (scratch.isTopLevel(b.id))
            roots.push_back(b.id);
    if (roots.empty())
        throw std::runtime_error("This custom shape is empty.");
    const Bounds box = boundsOf(scratch, roots);
    // Beside what's on the plate, as Add Shape does (its middle where a new shape's goes).
    Vec3 offset{0.0, 0.0, 0.0};
    if (box.valid) {
        const auto c = box.center();
        const Placement p = suggestPlacement(doc, double(box.max[0] - box.min[0]));
        offset = {p.x - c[0], p.y - c[1], 0.0};
    }
    const auto stem = path.stem().u8string();
    const std::string shapeName(stem.begin(), stem.end());
    std::vector<ObjectId> made;
    stack.beginMacro("Add " + shapeName);
    try {
        std::set<std::string> taken;
        for (ObjectId id : roots) {
            const BodyTree tree = captureTree(scratch, id);
            BodyTree copy = cloneTree(doc, tree, offset);
            // The names it was saved with, not "... copy".
            for (std::size_t i = 0; i < copy.size(); ++i) {
                // One shape is called what the library calls it (even after a rename).
                const bool named = roots.size() == 1 && i + 1 == copy.size();
                copy[i].name = freeName(doc, named ? shapeName : tree[i].name, taken);
                taken.insert(copy[i].name);
            }
            for (const Body& b : copy)
                stack.execute(std::make_unique<AddBodyCommand>(b));
            if (const auto solid = scratch.cachedSolid(id))
                doc.seedSolid(copy.back().id, *solid);
            made.push_back(copy.back().id);
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

fs::path rename(const fs::path& path, const std::string& newName)
{
    const fs::path target = path.parent_path() / pathOf(fileNameFor(newName));
    std::error_code ec;
    if (fs::exists(target, ec) && !fs::equivalent(target, path, ec))
        throw std::runtime_error("There's already a custom shape called '" + newName + "'.");
    fs::rename(path, target, ec);
    if (ec)
        throw std::runtime_error("The custom shape couldn't be renamed: " + ec.message());
    return target;
}

}  // namespace library
