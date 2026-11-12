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
    return json{{"x", p.x},
                {"y", p.y},
                {"z", p.z},
                {"rotationX", p.rotationX},
                {"rotationY", p.rotationY},
                {"rotationZ", p.rotationZ}};
}

const char* opName(BooleanOp op)
{
    switch (op) {
    case BooleanOp::Merge: return "merge";
    case BooleanOp::Subtract: return "subtract";
    case BooleanOp::Intersect: return "intersect";
    case BooleanOp::Split: return "split";
    case BooleanOp::Mirror: return "mirror";
    case BooleanOp::Group: return "group";
    }
    return "merge";
}

json bodyToJson(const Body& b)
{
    json j{{"id", b.id}, {"name", b.name}, {"placement", placementToJson(b.placement)}};
    if (b.isBoolean()) {
        j["type"] = "combine";
        j["operation"] = opName(b.boolean->op);
        j["operands"] = b.boolean->operands;
        j["pivot"] = b.boolean->pivot;
        if (b.boolean->op == BooleanOp::Split || b.boolean->op == BooleanOp::Mirror) {
            j["planePoint"] = b.boolean->planePoint;
            j["planeNormal"] = b.boolean->planeNormal;
        }
        return j;
    }
    j["type"] = fileTypeName(b.spec);
    json params = json::object();
    const auto& fields = specFields(b.spec);
    for (std::size_t i = 0; i < fields.size(); ++i)
        params[fields[i].key] = specValue(b.spec, i);
    j["parameters"] = std::move(params);
    return j;
}

double number(const json& obj, const char* key, const std::string& where)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number())
        damaged(where + ": missing number '" + key + "'");
    return it->get<double>();
}

Body bodyFromJson(const json& j, std::size_t index, int version)
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
    b.placement.x = number(*placement, "x", where);
    b.placement.y = number(*placement, "y", where);
    b.placement.z = number(*placement, "z", where);
    b.placement.rotationZ = number(*placement, "rotationZ", where);
    if (version >= 2) {
        b.placement.rotationX = number(*placement, "rotationX", where);
        b.placement.rotationY = number(*placement, "rotationY", where);
    }  // version 1 files only turned about Z; X and Y stay 0

    const auto type = j.find("type");
    if (type == j.end() || !type->is_string())
        damaged(where + ": missing type");
    const std::string t = type->get<std::string>();

    if (t == "combine" && version >= 4) {
        BooleanSpec spec;
        const std::string op = j.value("operation", std::string());
        if (op == "merge")
            spec.op = BooleanOp::Merge;
        else if (op == "subtract")
            spec.op = BooleanOp::Subtract;
        else if (op == "intersect")
            spec.op = BooleanOp::Intersect;
        else if (op == "split" && version >= 7)
            spec.op = BooleanOp::Split;
        else if (op == "mirror" && version >= 8)
            spec.op = BooleanOp::Mirror;
        else if (op == "group" && version >= 8)
            spec.op = BooleanOp::Group;
        else
            damaged(where + ": unknown combine operation '" + op + "'");
        const auto operands = j.find("operands");
        if (operands == j.end() || !operands->is_array())
            damaged(where + ": missing operands");
        for (const auto& operand : *operands) {
            if (!operand.is_number_unsigned())
                damaged(where + ": bad operand id");
            spec.operands.push_back(operand.get<ObjectId>());
        }
        const auto pivot = j.find("pivot");
        if (pivot == j.end() || !pivot->is_array() || pivot->size() != 3)
            damaged(where + ": missing pivot");
        for (std::size_t i = 0; i < 3; ++i) {
            if (!(*pivot)[i].is_number())
                damaged(where + ": bad pivot");
            spec.pivot[i] = (*pivot)[i].get<double>();
        }
        if (spec.op == BooleanOp::Split || spec.op == BooleanOp::Mirror) {
            const auto readVec = [&](const char* key, std::array<double, 3>& out) {
                const auto v = j.find(key);
                if (v == j.end() || !v->is_array() || v->size() != 3)
                    damaged(where + ": missing " + key);
                for (std::size_t i = 0; i < 3; ++i) {
                    if (!(*v)[i].is_number())
                        damaged(where + ": bad " + key);
                    out[i] = (*v)[i].get<double>();
                }
            };
            readVec("planePoint", spec.planePoint);
            readVec("planeNormal", spec.planeNormal);
        }
        b.boolean = std::move(spec);
        if (b.name.empty())
            b.name = typeName(b) + " " + std::to_string(b.id);
        return b;
    }

    const auto params = j.find("parameters");
    if (params == j.end() || !params->is_object())
        damaged(where + ": missing parameters");
    const auto num = [&](const char* key) { return number(*params, key, where); };
    if (t == "cylinder" && version < 3) {
        b.spec = CylinderSpec{num("radius"), num("height")};  // round
    } else if (version < 6 && t == "sphere") {  // 0.6.0 shapes were always round
        b.spec = SphereSpec{num("radius")};
    } else if (version < 6 && t == "cone") {
        b.spec = ConeSpec{num("radiusBottom"), num("radiusTop"), num("height")};
    } else if (version < 6 && t == "torus") {
        b.spec = TorusSpec{num("majorRadius"), num("minorRadius")};
    } else if (version < 6 && t == "tube") {
        b.spec = TubeSpec{num("outerRadius"), num("innerRadius"), num("height")};
    } else {
        // Find the primitive kind with this file name, then read its fields.
        bool known = false;
        for (std::size_t kind = 0; kind < kPrimitiveKindCount && !known; ++kind) {
            PrimitiveSpec spec = defaultSpec(kind);
            if (fileTypeName(spec) != t)
                continue;
            const auto& fields = specFields(spec);
            for (std::size_t i = 0; i < fields.size(); ++i)
                setSpecValue(spec, i, number(*params, fields[i].key, where));
            b.spec = spec;
            known = true;
        }
        if (!known)
            throw FileError("This file contains a shape type this version of Forge3D does not know (\"" + t +
                            "\").\nIt may have been made by a newer version.");
    }

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
    const int fileVersion = version->get<int>();
    if (fileVersion < 1)
        damaged("invalid format version");
    if (fileVersion > kFormatVersion)
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
        result.push_back(bodyFromJson((*bodies)[i], i, fileVersion));
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
                                          static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION)) &&
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
