// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Export.h"

#include "forge/core/Document.h"
#include "forge/core/Transform.h"

#include <BRepBuilderAPI_Copy.hxx>
#include <miniz.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

namespace forge {

namespace {

// Joins points that are the same to within 1e-5 mm.
struct Welder {
    struct Key {
        std::int64_t x, y, z;
        bool operator==(const Key&) const = default;
    };
    struct Hash {
        std::size_t operator()(const Key& k) const
        {
            return std::hash<std::int64_t>()(k.x * 73856093LL ^ k.y * 19349663LL ^ k.z * 83492791LL);
        }
    };
    std::unordered_map<Key, std::uint32_t, Hash> index;
    std::vector<float>& positions;

    explicit Welder(std::vector<float>& out) : positions(out) {}

    std::uint32_t add(const Vec3& p)
    {
        constexpr double grid = 1e5;  // 1e-5 mm
        const Key key{std::llround(p[0] * grid), std::llround(p[1] * grid), std::llround(p[2] * grid)};
        const auto [it, added] = index.try_emplace(key, static_cast<std::uint32_t>(positions.size() / 3));
        if (added)
            positions.insert(positions.end(), {float(p[0]), float(p[1]), float(p[2])});
        return it->second;
    }
};

std::size_t countOpenEdges(const std::vector<std::uint32_t>& indices)
{
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> uses;
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3)
        for (int e = 0; e < 3; ++e) {
            std::uint32_t a = indices[t + e], b = indices[t + (e + 1) % 3];
            if (a > b)
                std::swap(a, b);
            ++uses[{a, b}];
        }
    std::size_t open = 0;
    for (const auto& [edge, n] : uses)
        if (n != 2)
            ++open;
    return open;
}

std::string number(float v)
{
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(v));
    return buf;
}

std::string xmlEscape(const std::string& s)
{
    std::string out;
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default: out += c;
        }
    }
    return out;
}

std::string stlBytes(const std::vector<ExportMesh>& meshes)
{
    std::uint32_t count = 0;
    for (const ExportMesh& m : meshes)
        count += static_cast<std::uint32_t>(m.triangleCount());
    std::string out(80, '\0');
    const char header[] = "Forge3D binary STL, millimetres";
    std::memcpy(out.data(), header, sizeof header - 1);
    out.append(reinterpret_cast<const char*>(&count), 4);  // little-endian on Windows and Linux x86
    for (const ExportMesh& m : meshes) {
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            std::array<std::array<float, 3>, 3> v{};
            for (int k = 0; k < 3; ++k)
                for (int c = 0; c < 3; ++c)
                    v[k][c] = m.positions[m.indices[t + k] * 3 + c];
            const float ux = v[1][0] - v[0][0], uy = v[1][1] - v[0][1], uz = v[1][2] - v[0][2];
            const float wx = v[2][0] - v[0][0], wy = v[2][1] - v[0][1], wz = v[2][2] - v[0][2];
            std::array<float, 3> n{uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx};
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (len > 0.0f)
                for (float& c : n)
                    c /= len;
            out.append(reinterpret_cast<const char*>(n.data()), 12);
            for (const auto& p : v)
                out.append(reinterpret_cast<const char*>(p.data()), 12);
            out.append(2, '\0');
        }
    }
    return out;
}

std::string objText(const std::vector<ExportMesh>& meshes)
{
    std::ostringstream out;
    out << "# Forge3D OBJ export, millimetres\n";
    std::size_t base = 1;  // OBJ counts points from 1, across the whole file
    for (const ExportMesh& m : meshes) {
        std::string name = m.name;
        std::replace(name.begin(), name.end(), ' ', '_');
        out << "o " << name << "\n";
        for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
            out << "v " << number(m.positions[i]) << ' ' << number(m.positions[i + 1]) << ' '
                << number(m.positions[i + 2]) << "\n";
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
            out << "f " << base + m.indices[t] << ' ' << base + m.indices[t + 1] << ' ' << base + m.indices[t + 2]
                << "\n";
        base += m.positions.size() / 3;
    }
    return out.str();
}

std::string threeMfModel(const std::vector<ExportMesh>& meshes)
{
    std::ostringstream out;
    out << R"(<?xml version="1.0" encoding="UTF-8"?>)" << "\n"
        << R"(<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">)"
        << "\n <metadata name=\"Application\">Forge3D</metadata>\n <resources>\n";
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const ExportMesh& m = meshes[i];
        out << "  <object id=\"" << i + 1 << "\" type=\"model\" name=\"" << xmlEscape(m.name) << "\">\n"
            << "   <mesh>\n    <vertices>\n";
        for (std::size_t p = 0; p + 2 < m.positions.size(); p += 3)
            out << "     <vertex x=\"" << number(m.positions[p]) << "\" y=\"" << number(m.positions[p + 1])
                << "\" z=\"" << number(m.positions[p + 2]) << "\"/>\n";
        out << "    </vertices>\n    <triangles>\n";
        for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3)
            out << "     <triangle v1=\"" << m.indices[t] << "\" v2=\"" << m.indices[t + 1] << "\" v3=\""
                << m.indices[t + 2] << "\"/>\n";
        out << "    </triangles>\n   </mesh>\n  </object>\n";
    }
    out << " </resources>\n <build>\n";
    for (std::size_t i = 0; i < meshes.size(); ++i)
        out << "  <item objectid=\"" << i + 1 << "\"/>\n";
    out << " </build>\n</model>\n";
    return out.str();
}

std::string threeMfBytes(const std::vector<ExportMesh>& meshes)
{
    const std::string types =
        R"(<?xml version="1.0" encoding="UTF-8"?>)"
        R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
        R"(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)"
        R"(<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>)"
        R"(</Types>)";
    const std::string rels =
        R"(<?xml version="1.0" encoding="UTF-8"?>)"
        R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
        R"(<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>)"
        R"(</Relationships>)";
    const std::string model = threeMfModel(meshes);

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 64 * 1024))
        throw ExportError("Could not prepare the 3MF file (out of memory?).");
    const auto add = [&zip](const char* name, const std::string& text) {
        return mz_zip_writer_add_mem(&zip, name, text.data(), text.size(),
                                     static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION));
    };
    void* buffer = nullptr;
    size_t size = 0;
    const bool ok = add("[Content_Types].xml", types) && add("_rels/.rels", rels) && add("3D/3dmodel.model", model) &&
                    mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    std::string bytes;
    if (ok)
        bytes.assign(static_cast<const char*>(buffer), size);
    mz_zip_writer_end(&zip);
    if (!ok)
        throw ExportError("Could not prepare the 3MF file.");
    return bytes;
}

void writeFileSafely(const std::filesystem::path& path, const std::string& bytes)
{
    std::filesystem::path temp = path;
    temp += ".saving";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            throw ExportError("Could not write the file. Check that the folder exists, that you have permission "
                              "to save there, and that the disk is not full.");
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        throw ExportError("Could not replace the file (is it open in another program?): " + ec.message());
    }
}

}  // namespace

double ExportMesh::volume() const
{
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        const float* a = &positions[indices[t] * 3];
        const float* b = &positions[indices[t + 1] * 3];
        const float* c = &positions[indices[t + 2] * 3];
        v += (double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) -
              double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0]) +
              double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0])) /
             6.0;
    }
    return v;
}

TessellationSettings exportQuality(bool fine)
{
    TessellationSettings s;
    s.linearDeflection = fine ? 0.01 : 0.05;
    s.angularDeflection = fine ? 0.15 : 0.35;
    s.relative = false;
    return s;
}

std::vector<ExportMesh> exportMeshes(const Document& doc, const std::vector<ObjectId>& ids,
                                     const TessellationSettings& quality)
{
    std::vector<ExportMesh> out;
    for (ObjectId id : ids) {
        const Body* body = doc.find(id);
        if (!body)
            throw std::out_of_range("No body with id " + std::to_string(id));
        // A fresh copy, so the finer triangles don't replace the ones used for display.
        const TopoDS_Shape shape = BRepBuilderAPI_Copy(doc.shape(id), /*copyGeom=*/true, /*copyMesh=*/false).Shape();
        const Mesh mesh = tessellate(shape, quality);
        const Transform toPlate = doc.worldTransform(id);
        ExportMesh m;
        m.name = body->name;
        Welder welder(m.positions);
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            std::array<std::uint32_t, 3> tri{};
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t i = mesh.indices[t + k];
                tri[k] = welder.add(toPlate.apply(
                    {mesh.positions[i * 3], mesh.positions[i * 3 + 1], mesh.positions[i * 3 + 2]}));
            }
            if (tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2])
                continue;  // a sliver that collapsed to a line
            m.indices.insert(m.indices.end(), tri.begin(), tri.end());
        }
        m.openEdges = countOpenEdges(m.indices);
        out.push_back(std::move(m));
    }
    return out;
}

ExportFormat formatForPath(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (ext == ".stl")
        return ExportFormat::Stl;
    if (ext == ".3mf")
        return ExportFormat::ThreeMf;
    if (ext == ".obj")
        return ExportFormat::Obj;
    throw ExportError("Forge3D can export .stl, .3mf and .obj files.");
}

void writeMeshes(const std::vector<ExportMesh>& meshes, const std::filesystem::path& path, ExportFormat format)
{
    if (meshes.empty())
        throw ExportError("There is nothing to export.");
    switch (format) {
    case ExportFormat::Stl: writeFileSafely(path, stlBytes(meshes)); break;
    case ExportFormat::ThreeMf: writeFileSafely(path, threeMfBytes(meshes)); break;
    case ExportFormat::Obj: writeFileSafely(path, objText(meshes)); break;
    }
}

}  // namespace forge
