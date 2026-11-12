// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/FileFormat.h"

#include "forge/core/Document.h"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <cstring>
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
    case BooleanOp::Emboss: return "emboss";
    case BooleanOp::Edges: return "edges";
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
        if (b.boolean->op == BooleanOp::Split || b.boolean->op == BooleanOp::Mirror ||
            b.boolean->op == BooleanOp::Emboss) {
            j["planePoint"] = b.boolean->planePoint;
            j["planeNormal"] = b.boolean->planeNormal;
        }
        if (const auto& f = b.boolean->edges) {
            static const char* kinds[] = {"rounding", "fillet", "chamfer", "bevel"};
            json picks = json::array();
            for (const EdgeRef& r : f->edges)
                picks.push_back(json{{"point", r.point}, {"facePoint", r.facePoint}});
            j["edges"] = json{{"kind", kinds[int(f->kind)]}, {"size", f->size}, {"angle", f->angle}, {"picks", picks}};
        }
        if (const auto& e = b.boolean->emboss) {
            j["emboss"] = json{{"text", e->text},     {"font", e->font},   {"bold", e->bold},
                               {"italic", e->italic}, {"vertical", e->vertical}, {"letterHeight", e->letterHeight},
                               {"depth", e->depth},   {"raised", e->raised}, {"angle", e->angle}};
        }
        return j;
    }
    j["type"] = fileTypeName(b.spec);
    if (std::holds_alternative<MeshSpec>(b.spec))
        j["data"] = "meshes/" + std::to_string(b.id) + ".bin";
    if (const auto* t = std::get_if<TextSpec>(&b.spec)) {
        j["text"] = t->text;
        j["font"] = t->font;
        j["bold"] = t->bold;
        j["italic"] = t->italic;
        j["vertical"] = t->vertical;
    }
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

Body bodyFromJson(const json& j, std::size_t index, int version,
                  const std::function<std::string(const std::string&)>& readEntry)
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
        else if (op == "emboss" && version >= 10)
            spec.op = BooleanOp::Emboss;
        else if (op == "edges" && version >= 11)
            spec.op = BooleanOp::Edges;
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
        if (spec.op == BooleanOp::Split || spec.op == BooleanOp::Mirror || spec.op == BooleanOp::Emboss) {
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
        if (spec.op == BooleanOp::Edges) {
            const auto e = j.find("edges");
            if (e == j.end() || !e->is_object())
                damaged(where + ": missing the edges");
            EdgeFinish f;
            const std::string kind = e->value("kind", std::string());
            if (kind == "rounding")
                f.kind = EdgeFinishKind::Rounding;
            else if (kind == "fillet")
                f.kind = EdgeFinishKind::Fillet;
            else if (kind == "chamfer")
                f.kind = EdgeFinishKind::Chamfer;
            else if (kind == "bevel")
                f.kind = EdgeFinishKind::Bevel;
            else
                damaged(where + ": unknown edge finish '" + kind + "'");
            f.size = number(*e, "size", where);
            f.angle = e->value("angle", 45.0);
            const auto picks = e->find("picks");
            if (picks == e->end() || !picks->is_array())
                damaged(where + ": missing the picked edges");
            for (const auto& p : *picks) {
                EdgeRef r;
                const auto readPoint = [&](const char* key, std::array<double, 3>& out) {
                    const auto v = p.find(key);
                    if (v == p.end() || !v->is_array() || v->size() != 3)
                        damaged(where + ": a picked edge is damaged");
                    for (std::size_t i = 0; i < 3; ++i)
                        out[i] = (*v)[i].get<double>();
                };
                readPoint("point", r.point);
                readPoint("facePoint", r.facePoint);
                f.edges.push_back(r);
            }
            spec.edges = std::move(f);
        }
        if (spec.op == BooleanOp::Emboss) {
            const auto e = j.find("emboss");
            if (e == j.end() || !e->is_object())
                damaged(where + ": missing emboss words");
            EmbossText words;
            words.text = e->value("text", std::string());
            words.font = e->value("font", std::string());
            words.bold = e->value("bold", true);
            words.italic = e->value("italic", false);
            words.vertical = e->value("vertical", false);
            words.letterHeight = number(*e, "letterHeight", where);
            words.depth = number(*e, "depth", where);
            words.raised = e->value("raised", true);
            words.angle = e->value("angle", 0.0);
            spec.emboss = std::move(words);
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
    } else if (t == "mesh" && version >= 9) {
        MeshSpec mesh;
        mesh.width = num("width");
        mesh.depth = num("depth");
        mesh.height = num("height");
        const std::string entry = j.value("data", std::string());
        if (entry.empty())
            damaged(where + ": missing mesh data");
        if (!readEntry)
            damaged(where + ": mesh data not available");
        mesh.data = meshFromBytes(readEntry(entry));
        b.spec = mesh;
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
            if (auto* text = std::get_if<TextSpec>(&spec)) {
                text->text = j.value("text", std::string());
                text->font = j.value("font", std::string());
                text->bold = j.value("bold", false);
                text->italic = j.value("italic", false);
                text->vertical = j.value("vertical", false);
            }
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

std::vector<Body> fromJson(const std::string& text, const std::function<std::string(const std::string&)>& readEntry)
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
        result.push_back(bodyFromJson((*bodies)[i], i, fileVersion, readEntry));
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
    bool ok = mz_zip_writer_add_mem(&zip, kDocumentEntry, jsonText.data(), jsonText.size(),
                                    static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION));
    for (const Body& b : doc.bodies()) {
        if (!ok)
            break;
        if (const auto* mesh = std::get_if<MeshSpec>(&b.spec); mesh && mesh->data) {
            const std::string name = "meshes/" + std::to_string(b.id) + ".bin";
            const std::string bytes = meshToBytes(*mesh->data);
            ok = mz_zip_writer_add_mem(&zip, name.c_str(), bytes.data(), bytes.size(),
                                       static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION));
        }
    }
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
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

    const auto readEntry = [&zip](const std::string& name) {
        size_t entrySize = 0;
        void* entry = mz_zip_reader_extract_file_to_heap(&zip, name.c_str(), &entrySize, 0);
        if (!entry)
            damaged("missing " + name);
        std::string out(static_cast<const char*>(entry), entrySize);
        mz_free(entry);
        return out;
    };
    try {
        std::vector<Body> bodies = fromJson(jsonText, readEntry);
        mz_zip_reader_end(&zip);
        return bodies;
    } catch (...) {
        mz_zip_reader_end(&zip);
        throw;
    }
}

namespace {
constexpr char kMeshMagic[8] = {'F', '3', 'D', 'M', 'E', 'S', 'H', '1'};
}

std::string meshToBytes(const MeshData& mesh)
{
    std::string out(kMeshMagic, sizeof kMeshMagic);
    const auto put = [&out](const void* p, std::size_t n) { out.append(static_cast<const char*>(p), n); };
    const std::uint32_t points = static_cast<std::uint32_t>(mesh.positions.size() / 3);
    const std::uint32_t tris = static_cast<std::uint32_t>(mesh.indices.size() / 3);
    const std::uint8_t closed = mesh.closed ? 1 : 0;
    put(&points, 4);
    put(&tris, 4);
    put(mesh.size.data(), 12);
    put(&closed, 1);
    put(mesh.positions.data(), mesh.positions.size() * sizeof(float));
    put(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    return out;
}

std::shared_ptr<const MeshData> meshFromBytes(const std::string& bytes)
{
    std::size_t at = 0;
    const auto take = [&](void* p, std::size_t n) {
        if (at + n > bytes.size())
            damaged("mesh data is cut short");
        std::memcpy(p, bytes.data() + at, n);
        at += n;
    };
    char magic[8];
    take(magic, 8);
    if (std::memcmp(magic, kMeshMagic, 8) != 0)
        damaged("mesh data is not in a known format");
    auto mesh = std::make_shared<MeshData>();
    std::uint32_t points = 0, tris = 0;
    std::uint8_t closed = 1;
    take(&points, 4);
    take(&tris, 4);
    take(mesh->size.data(), 12);
    take(&closed, 1);
    mesh->closed = closed != 0;
    mesh->positions.resize(std::size_t(points) * 3);
    mesh->indices.resize(std::size_t(tris) * 3);
    take(mesh->positions.data(), mesh->positions.size() * sizeof(float));
    take(mesh->indices.data(), mesh->indices.size() * sizeof(std::uint32_t));
    for (std::uint32_t i : mesh->indices)
        if (i >= points)
            damaged("mesh data refers to a missing point");
    return mesh;
}

}  // namespace forge::file
