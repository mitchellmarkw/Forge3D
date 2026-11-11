// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/FileFormat.h"

#include "forge/core/Document.h"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <system_error>
#include <type_traits>

namespace forge::file {

using nlohmann::json;

namespace {

constexpr const char* kFormatName = "forge3d-document";

[[noreturn]] void damaged(const std::string& detail)
{
    throw FileError("This file is damaged or is not a Forge3D file.\n(" + detail + ")");
}

json placementToJson(const Placement& p)
{
    return json{{"x", p.x}, {"y", p.y}, {"z", p.z}, {"rotationZ", p.rotationZ}};
}

json bodyToJson(const Body& b)
{
    json j{{"id", b.id}, {"name", b.name}, {"placement", placementToJson(b.placement)}};
    std::visit(
        [&j](const auto& spec) {
            using T = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<T, BoxSpec>) {
                j["type"] = "box";
                j["parameters"] = {{"width", spec.width}, {"depth", spec.depth}, {"height", spec.height}};
            } else {
                j["type"] = "cylinder";
                j["parameters"] = {{"radius", spec.radius}, {"height", spec.height}};
            }
        },
        b.spec);
    return j;
}

double number(const json& obj, const char* key, const std::string& where)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number())
        damaged(where + ": missing number '" + key + "'");
    return it->get<double>();
}

Body bodyFromJson(const json& j, std::size_t index)
{
    const std::string where = "body " + std::to_string(index + 1);
    if (!j.is_object())
        damaged(where + " is not an object");

    Body b;
    const auto id = j.find("id");
    if (id == j.end() || !id->is_number_unsigned())
        damaged(where + ": missing id");
    b.id = id->get<ObjectId>();

    const auto name = j.find("name");
    b.name = (name != j.end() && name->is_string()) ? name->get<std::string>() : std::string();

    const auto placement = j.find("placement");
    if (placement == j.end() || !placement->is_object())
        damaged(where + ": missing placement");
    b.placement = Placement{number(*placement, "x", where), number(*placement, "y", where),
                            number(*placement, "z", where), number(*placement, "rotationZ", where)};

    const auto type = j.find("type");
    const auto params = j.find("parameters");
    if (type == j.end() || !type->is_string() || params == j.end() || !params->is_object())
        damaged(where + ": missing type or parameters");

    const std::string t = type->get<std::string>();
    if (t == "box")
        b.spec = BoxSpec{number(*params, "width", where), number(*params, "depth", where),
                         number(*params, "height", where)};
    else if (t == "cylinder")
        b.spec = CylinderSpec{number(*params, "radius", where), number(*params, "height", where)};
    else
        throw FileError("This file contains a shape type this version of Forge3D does not know (\"" + t +
                        "\").\nIt may have been made by a newer version.");

    if (b.name.empty())
        b.name = typeName(b.spec) + " " + std::to_string(b.id);
    return b;
}

std::string readWholeFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw FileError("Could not open the file. It may have been moved, or you may not have permission to read it.");
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

// ---- JSON -----------------------------------------------------------------------

std::string toJson(const Document& doc, const std::string& createdBy)
{
    json bodies = json::array();
    for (const Body& b : doc.bodies())
        bodies.push_back(bodyToJson(b));
    const json root{{"format", kFormatName},
                    {"formatVersion", kFormatVersion},
                    {"createdBy", createdBy},
                    {"units", "mm"},
                    {"bodies", std::move(bodies)}};
    return root.dump(2);
}

std::vector<Body> fromJson(const std::string& text)
{
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error& e) {
        damaged(std::string("unreadable document.json: ") + e.what());
    }
    if (!root.is_object() || root.value("format", std::string()) != kFormatName)
        damaged("document.json is not a Forge3D document");

    const auto version = root.find("formatVersion");
    if (version == root.end() || !version->is_number_integer())
        damaged("missing format version");
    if (version->get<int>() > kFormatVersion)
        throw FileError("This file was made by a newer version of Forge3D (file format " +
                        std::to_string(version->get<int>()) + ").\nPlease update Forge3D to open it.");

    if (root.value("units", std::string("mm")) != "mm")
        damaged("unsupported units");

    const auto bodies = root.find("bodies");
    if (bodies == root.end() || !bodies->is_array())
        damaged("missing list of bodies");

    std::vector<Body> result;
    result.reserve(bodies->size());
    for (std::size_t i = 0; i < bodies->size(); ++i)
        result.push_back(bodyFromJson((*bodies)[i], i));
    return result;
}

// ---- Zip container --------------------------------------------------------------

void save(const Document& doc, const std::filesystem::path& path, const std::string& createdBy)
{
    const std::string jsonText = toJson(doc, createdBy);

    // Build the zip in memory, so file access stays in C++ (wide paths on Windows).
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 64 * 1024))
        throw FileError("Could not prepare the file for saving (out of memory?).");
    void* buffer = nullptr;
    size_t size = 0;
    const bool ok = mz_zip_writer_add_mem(&zip, kDocumentEntry, jsonText.data(), jsonText.size(),
                                          MZ_DEFAULT_COMPRESSION) &&
                    mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    if (!ok) {
        mz_zip_writer_end(&zip);
        throw FileError("Could not prepare the file for saving.");
    }

    // Write next to the target, then swap it in.
    std::filesystem::path temp = path;
    temp += ".saving";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
        out.close();
        mz_zip_writer_end(&zip);  // frees buffer
        if (!out) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            throw FileError("Could not write the file. Check that the folder exists, that you have "
                            "permission to save there, and that the disk is not full.");
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        throw FileError("Could not replace the existing file. It may be open in another program.");
    }
}

std::vector<Body> load(const std::filesystem::path& path)
{
    const std::string bytes = readWholeFile(path);

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        damaged("not a zip archive");

    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, kDocumentEntry, &size, 0);
    if (!data) {
        mz_zip_reader_end(&zip);
        damaged("no document.json inside");
    }
    std::string jsonText(static_cast<const char*>(data), size);
    mz_free(data);
    mz_zip_reader_end(&zip);

    return fromJson(jsonText);
}

}  // namespace forge::file
