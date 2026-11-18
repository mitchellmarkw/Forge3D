// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 41: PLY (scans) and glTF / GLB (downloads) importers.
#include "forge/core/Import.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <optional>
#include <limits>
#include <sstream>

namespace forge {

namespace {

constexpr std::size_t kMostCorners = 90'000'000;  // 30 million triangles, as for 3MF

// ---- PLY ------------------------------------------------------------------------

enum class PlyType { Int8, UInt8, Int16, UInt16, Int32, UInt32, Float32, Float64 };

PlyType plyType(const std::string& word)
{
    if (word == "char" || word == "int8")
        return PlyType::Int8;
    if (word == "uchar" || word == "uint8")
        return PlyType::UInt8;
    if (word == "short" || word == "int16")
        return PlyType::Int16;
    if (word == "ushort" || word == "uint16")
        return PlyType::UInt16;
    if (word == "int" || word == "int32")
        return PlyType::Int32;
    if (word == "uint" || word == "uint32")
        return PlyType::UInt32;
    if (word == "float" || word == "float32")
        return PlyType::Float32;
    if (word == "double" || word == "float64")
        return PlyType::Float64;
    throw ImportError("This PLY file uses a kind of number Forge3D doesn't know ('" + word + "').");
}

std::size_t plySize(PlyType t)
{
    switch (t) {
    case PlyType::Int8:
    case PlyType::UInt8: return 1;
    case PlyType::Int16:
    case PlyType::UInt16: return 2;
    case PlyType::Int32:
    case PlyType::UInt32:
    case PlyType::Float32: return 4;
    case PlyType::Float64: return 8;
    }
    return 4;
}

struct PlyProperty {
    std::string name;
    PlyType type = PlyType::Float32;
    bool list = false;
    PlyType countType = PlyType::UInt8;
};

struct PlyElement {
    std::string name;
    std::size_t count = 0;
    std::vector<PlyProperty> properties;
};

// Reads numbers one at a time from the body of a PLY file, text or binary.
class PlyReader {
public:
    PlyReader(const std::string& bytes, std::size_t at, int format)  // 0 text, 1 little-endian, 2 big-endian
        : m_bytes(bytes), m_at(at), m_format(format)
    {
        if (format == 0)
            m_text.str(bytes.substr(at));
    }

    double read(PlyType t)
    {
        if (m_format == 0) {
            std::string word;
            if (!(m_text >> word))
                throw ImportError("This PLY file ends too soon (it may be cut short).");
            try {
                std::size_t used = 0;
                const double v = std::stod(word, &used);
                if (used != word.size())
                    throw std::invalid_argument(word);
                return v;
            } catch (const std::logic_error&) {
                throw ImportError("This PLY file is damaged ('" + word + "' where a number should be).");
            }
        }
        const std::size_t n = plySize(t);
        if (m_at + n > m_bytes.size())
            throw ImportError("This PLY file ends too soon (it may be cut short).");
        unsigned char b[8];
        std::memcpy(b, m_bytes.data() + m_at, n);
        m_at += n;
        if (m_format == 2)
            std::reverse(b, b + n);  // big-endian: Windows PCs are little-endian
        switch (t) {
        case PlyType::Int8: return double(static_cast<std::int8_t>(b[0]));
        case PlyType::UInt8: return double(b[0]);
        case PlyType::Int16: {
            std::int16_t v;
            std::memcpy(&v, b, 2);
            return v;
        }
        case PlyType::UInt16: {
            std::uint16_t v;
            std::memcpy(&v, b, 2);
            return v;
        }
        case PlyType::Int32: {
            std::int32_t v;
            std::memcpy(&v, b, 4);
            return v;
        }
        case PlyType::UInt32: {
            std::uint32_t v;
            std::memcpy(&v, b, 4);
            return v;
        }
        case PlyType::Float32: {
            float v;
            std::memcpy(&v, b, 4);
            return v;
        }
        case PlyType::Float64: {
            double v;
            std::memcpy(&v, b, 8);
            return v;
        }
        }
        return 0.0;
    }

    // Bytes left (binary), for refusing counts the file can't hold.
    std::size_t left() const { return m_format == 0 ? m_bytes.size() : m_bytes.size() - m_at; }

private:
    const std::string& m_bytes;
    std::size_t m_at;
    int m_format;
    std::istringstream m_text;
};

// ---- glTF -----------------------------------------------------------------------

using nlohmann::json;
using Matrix = std::array<double, 16>;  // column-major, as glTF writes them

Matrix identity()
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

Matrix multiply(const Matrix& a, const Matrix& b)
{
    Matrix r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            double s = 0.0;
            for (int k = 0; k < 4; ++k)
                s += a[std::size_t(k * 4 + row)] * b[std::size_t(c * 4 + k)];
            r[std::size_t(c * 4 + row)] = s;
        }
    return r;
}

double number(const json& j)
{
    if (!j.is_number())
        throw ImportError("This glTF file is damaged (a number is missing).");
    const double v = j.get<double>();
    if (!std::isfinite(v))
        throw ImportError("This glTF file is damaged (a number is not a number).");
    return v;
}

Matrix nodeMatrix(const json& node)
{
    if (const auto m = node.find("matrix"); m != node.end()) {
        if (!m->is_array() || m->size() != 16)
            throw ImportError("This glTF file is damaged (a matrix doesn't have 16 numbers).");
        Matrix out{};
        for (std::size_t i = 0; i < 16; ++i)
            out[i] = number((*m)[i]);
        return out;
    }
    const auto three = [&node](const char* key, std::array<double, 3> fallback) {
        const auto it = node.find(key);
        if (it == node.end())
            return fallback;
        if (!it->is_array() || it->size() != 3)
            throw ImportError(std::string("This glTF file is damaged (") + key + ").");
        return std::array<double, 3>{number((*it)[0]), number((*it)[1]), number((*it)[2])};
    };
    const auto t = three("translation", {0, 0, 0});
    const auto s = three("scale", {1, 1, 1});
    std::array<double, 4> q{0, 0, 0, 1};  // x, y, z, w
    if (const auto it = node.find("rotation"); it != node.end()) {
        if (!it->is_array() || it->size() != 4)
            throw ImportError("This glTF file is damaged (rotation).");
        for (std::size_t i = 0; i < 4; ++i)
            q[i] = number((*it)[i]);
        const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (len > 1e-12)
            for (double& v : q)
                v /= len;
    }
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    const double r[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w),     2 * (x * z - y * w),
                         2 * (x * y - z * w),     1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                         2 * (x * z + y * w),     2 * (y * z - x * w),     1 - 2 * (x * x + y * y)};
    Matrix m{};
    for (int c = 0; c < 3; ++c)
        for (int row = 0; row < 3; ++row)
            m[std::size_t(c * 4 + row)] = r[c * 3 + row] * s[std::size_t(c)];
    m[12] = t[0];
    m[13] = t[1];
    m[14] = t[2];
    m[15] = 1;
    return m;
}

std::string decodeBase64(const std::string& text)
{
    static const std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(text.size() * 3 / 4);
    unsigned int buffer = 0;
    int bits = 0;
    for (char c : text) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ')
            continue;
        const auto at = letters.find(c);
        if (at == std::string::npos)
            throw ImportError("This glTF file is damaged (its embedded data isn't base64).");
        buffer = (buffer << 6) | unsigned(at);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(char((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

class Gltf {
public:
    Gltf(json doc, std::vector<std::string> buffers) : m_doc(std::move(doc)), m_buffers(std::move(buffers)) {}

    // An accessor's values, `width` numbers each (3 for points, 1 for indices).
    std::vector<double> values(std::size_t index, std::size_t width) const
    {
        const json& accessors = m_doc.value("accessors", json::array());
        if (index >= accessors.size())
            throw ImportError("This glTF file refers to data that isn't there.");
        const json& a = accessors[index];
        if (a.contains("sparse"))
            throw ImportError("This glTF file uses sparse data, which Forge3D can't read yet.");
        const std::size_t count = a.value("count", std::size_t(0));
        const std::string type = a.value("type", std::string());
        const std::size_t parts = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : 0;
        if (parts != width)
            throw ImportError("This glTF file is damaged (data of the wrong shape).");
        const int component = a.value("componentType", 0);
        const std::size_t size = component == 5120 || component == 5121   ? 1
                                 : component == 5122 || component == 5123 ? 2
                                 : component == 5125 || component == 5126 ? 4
                                                                          : 0;
        if (size == 0)
            throw ImportError("This glTF file is damaged (unknown kind of number).");
        if (count > kMostCorners)
            throw ImportError("This glTF file is too big to open (over 30 million triangles).");
        std::vector<double> out;
        if (!a.contains("bufferView")) {  // all zeros
            out.assign(count * width, 0.0);
            return out;
        }
        const json& views = m_doc.value("bufferViews", json::array());
        const std::size_t viewIndex = a["bufferView"].get<std::size_t>();
        if (viewIndex >= views.size())
            throw ImportError("This glTF file refers to data that isn't there.");
        const json& view = views[viewIndex];
        const std::size_t bufferIndex = view.value("buffer", std::size_t(0));
        if (bufferIndex >= m_buffers.size())
            throw ImportError("This glTF file refers to data that isn't there.");
        const std::string& buffer = m_buffers[bufferIndex];
        const std::size_t viewStart = view.value("byteOffset", std::size_t(0));
        const std::size_t viewLength = view.value("byteLength", std::size_t(0));
        const std::size_t element = size * width;
        const std::size_t stride = view.value("byteStride", element);
        const std::size_t start = a.value("byteOffset", std::size_t(0));
        bool fits = viewStart <= buffer.size() && viewLength <= buffer.size() - viewStart && stride >= element;
        if (fits && count > 0)
            fits = start <= viewLength && element <= viewLength - start &&
                   (count - 1) <= (viewLength - start - element) / stride;
        if (!fits)
            throw ImportError("This glTF file is damaged (its data runs past the end).");
        out.resize(count * width);
        const char* base = buffer.data() + viewStart + start;
        for (std::size_t i = 0; i < count; ++i)
            for (std::size_t k = 0; k < width; ++k) {
                const char* p = base + i * stride + k * size;
                double v = 0.0;
                switch (component) {
                case 5120: v = double(static_cast<std::int8_t>(*p)); break;
                case 5121: v = double(static_cast<std::uint8_t>(*p)); break;
                case 5122: {
                    std::int16_t x;
                    std::memcpy(&x, p, 2);
                    v = x;
                    break;
                }
                case 5123: {
                    std::uint16_t x;
                    std::memcpy(&x, p, 2);
                    v = x;
                    break;
                }
                case 5125: {
                    std::uint32_t x;
                    std::memcpy(&x, p, 4);
                    v = x;
                    break;
                }
                default: {
                    float x;
                    std::memcpy(&x, p, 4);
                    v = x;
                }
                }
                out[i * width + k] = v;
            }
        return out;
    }

    std::vector<RawMesh> meshes(const std::string& fileName) const
    {
        for (const char* compressed : {"KHR_draco_mesh_compression", "EXT_meshopt_compression"})
            for (const auto& used : m_doc.value("extensionsRequired", json::array()))
                if (used.is_string() && used.get<std::string>() == compressed)
                    throw ImportError("This glTF file is compressed (" + std::string(compressed) +
                                      "), which Forge3D can't read yet. Export it again without compression.");
        const json& nodes = m_doc.value("nodes", json::array());
        std::vector<std::size_t> roots;
        const json& scenes = m_doc.value("scenes", json::array());
        const std::size_t sceneIndex = m_doc.value("scene", std::size_t(0));
        if (sceneIndex < scenes.size()) {
            for (const auto& n : scenes[sceneIndex].value("nodes", json::array()))
                roots.push_back(n.get<std::size_t>());
        } else {
            // No scene: every node nobody holds.
            std::vector<char> held(nodes.size(), 0);
            for (const auto& n : nodes)
                for (const auto& c : n.value("children", json::array()))
                    if (c.get<std::size_t>() < held.size())
                        held[c.get<std::size_t>()] = 1;
            for (std::size_t i = 0; i < nodes.size(); ++i)
                if (!held[i])
                    roots.push_back(i);
        }
        std::vector<RawMesh> out;
        std::size_t corners = 0;
        std::function<void(std::size_t, const Matrix&, int)> visit = [&](std::size_t index, const Matrix& parent,
                                                                         int depth) {
            if (index >= nodes.size() || depth > 64)
                return;
            const json& node = nodes[index];
            const Matrix world = multiply(parent, nodeMatrix(node));
            if (const auto mesh = node.find("mesh"); mesh != node.end() && mesh->is_number_unsigned()) {
                RawMesh raw = meshAt(mesh->get<std::size_t>(), world, corners);
                if (!raw.indices.empty()) {
                    if (const auto name = node.find("name"); name != node.end() && name->is_string() &&
                                                             !name->get<std::string>().empty())
                        raw.name = name->get<std::string>();
                    if (raw.name.empty())
                        raw.name = fileName;
                    out.push_back(std::move(raw));
                }
            }
            for (const auto& c : node.value("children", json::array()))
                if (c.is_number_unsigned())
                    visit(c.get<std::size_t>(), world, depth + 1);
        };
        for (std::size_t r : roots)
            visit(r, identity(), 0);
        if (out.empty())
            throw ImportError("This glTF file has no triangles to import.");
        return out;
    }

private:
    RawMesh meshAt(std::size_t index, const Matrix& m, std::size_t& corners) const
    {
        const json& meshes = m_doc.value("meshes", json::array());
        if (index >= meshes.size())
            throw ImportError("This glTF file refers to a mesh that isn't there.");
        const json& mesh = meshes[index];
        RawMesh raw;
        raw.name = mesh.value("name", std::string());
        for (const auto& prim : mesh.value("primitives", json::array())) {
            const int mode = prim.value("mode", 4);
            if (mode != 4 && mode != 5 && mode != 6)
                continue;  // points and lines: nothing to print
            const auto attributes = prim.find("attributes");
            if (attributes == prim.end() || !attributes->contains("POSITION"))
                continue;
            const std::vector<double> p = values((*attributes)["POSITION"].get<std::size_t>(), 3);
            const std::size_t n = p.size() / 3;
            std::vector<std::uint32_t> order;
            if (prim.contains("indices")) {
                for (double v : values(prim["indices"].get<std::size_t>(), 1)) {
                    if (v < 0 || v >= double(n))
                        throw ImportError("This glTF file refers to a point that doesn't exist.");
                    order.push_back(std::uint32_t(v));
                }
            } else {
                order.resize(n);
                for (std::size_t i = 0; i < n; ++i)
                    order[i] = std::uint32_t(i);
            }
            std::vector<std::uint32_t> tris;
            if (mode == 4) {
                tris.assign(order.begin(), order.begin() + std::ptrdiff_t(order.size() / 3 * 3));
            } else {
                for (std::size_t i = 2; i < order.size(); ++i) {
                    if (mode == 5)  // strip: every other triangle turned round
                        tris.insert(tris.end(), i % 2 ? std::initializer_list<std::uint32_t>{order[i - 1], order[i - 2], order[i]}
                                                       : std::initializer_list<std::uint32_t>{order[i - 2], order[i - 1], order[i]});
                    else  // fan
                        tris.insert(tris.end(), {order[0], order[i - 1], order[i]});
                }
            }
            corners += tris.size();
            if (corners > kMostCorners)
                throw ImportError("This glTF file is too big to open (over 30 million triangles).");
            // Placed by its node, then turned to Forge3D's way up (glTF's up is Y, ours is
            // Z) and from metres to millimetres.
            const auto base = std::uint32_t(raw.positions.size() / 3);
            for (std::size_t i = 0; i < n; ++i) {
                const double x = p[i * 3], y = p[i * 3 + 1], z = p[i * 3 + 2];
                const double wx = m[0] * x + m[4] * y + m[8] * z + m[12];
                const double wy = m[1] * x + m[5] * y + m[9] * z + m[13];
                const double wz = m[2] * x + m[6] * y + m[10] * z + m[14];
                raw.positions.push_back(float(wx * 1000.0));
                raw.positions.push_back(float(-wz * 1000.0));
                raw.positions.push_back(float(wy * 1000.0));
            }
            // A mirroring node turns triangles inside out: turn them back.
            const double det = m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) +
                               m[8] * (m[1] * m[6] - m[5] * m[2]);
            for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
                raw.indices.push_back(base + tris[t]);
                raw.indices.push_back(base + (det < 0 ? tris[t + 2] : tris[t + 1]));
                raw.indices.push_back(base + (det < 0 ? tris[t + 1] : tris[t + 2]));
            }
            if (!raw.colour && prim.contains("material"))
                raw.colour = colourOf(prim["material"].get<std::size_t>());
        }
        return raw;
    }

    std::optional<std::uint32_t> colourOf(std::size_t material) const
    {
        const json& materials = m_doc.value("materials", json::array());
        if (material >= materials.size())
            return std::nullopt;
        const json& pbr = materials[material].value("pbrMetallicRoughness", json::object());
        const auto f = pbr.find("baseColorFactor");
        if (f == pbr.end() || !f->is_array() || f->size() < 3)
            return std::nullopt;
        std::uint32_t rgb = 0;
        for (std::size_t i = 0; i < 3; ++i) {
            const double linear = std::clamp(number((*f)[i]), 0.0, 1.0);
            // glTF colours are linear; screens (and Forge3D) use sRGB.
            const double srgb = linear <= 0.0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
            rgb = (rgb << 8) | std::uint32_t(std::lround(std::clamp(srgb, 0.0, 1.0) * 255.0));
        }
        return rgb;
    }

    json m_doc;
    std::vector<std::string> m_buffers;
};

json parseJson(const std::string& text)
{
    try {
        json j = json::parse(text);
        if (!j.is_object())
            throw ImportError("This doesn't look like a glTF file.");
        if (const auto asset = j.find("asset"); asset != j.end() && asset->is_object()) {
            const std::string version = asset->value("version", std::string("2.0"));
            if (!version.empty() && version[0] == '1')
                throw ImportError("This is an old (version 1) glTF file, which Forge3D can't read. "
                                  "Export it again as glTF 2.");
        }
        return j;
    } catch (const json::exception&) {
        throw ImportError("This glTF file is damaged (its description can't be read).");
    }
}

}  // namespace

std::vector<RawMesh> readPly(const std::string& bytes, const std::string& name)
{
    if (bytes.compare(0, 3, "ply") != 0)
        throw ImportError("This doesn't look like a PLY file (or it is damaged).");
    const auto headerEnd = bytes.find("end_header");
    if (headerEnd == std::string::npos)
        throw ImportError("This PLY file is damaged (its header has no end).");
    std::size_t bodyAt = bytes.find('\n', headerEnd);
    if (bodyAt == std::string::npos)
        throw ImportError("This PLY file has no points.");
    ++bodyAt;
    std::istringstream header(bytes.substr(0, headerEnd));
    std::string line;
    int format = -1;
    std::vector<PlyElement> elements;
    while (std::getline(header, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream words(line);
        std::string word;
        words >> word;
        if (word == "format") {
            std::string kind;
            words >> kind;
            format = kind == "ascii" ? 0 : kind == "binary_little_endian" ? 1 : kind == "binary_big_endian" ? 2 : -1;
            if (format < 0)
                throw ImportError("This PLY file is in a format Forge3D doesn't know ('" + kind + "').");
        } else if (word == "element") {
            PlyElement e;
            long long count = -1;
            words >> e.name >> count;
            if (count < 0 || std::size_t(count) > kMostCorners)
                throw ImportError("This PLY file is damaged (a count is wrong or far too big).");
            e.count = std::size_t(count);
            elements.push_back(std::move(e));
        } else if (word == "property") {
            if (elements.empty())
                throw ImportError("This PLY file is damaged (a property before any element).");
            PlyProperty p;
            std::string type;
            words >> type;
            if (type == "list") {
                std::string countType, itemType;
                words >> countType >> itemType >> p.name;
                p.list = true;
                p.countType = plyType(countType);
                p.type = plyType(itemType);
            } else {
                p.type = plyType(type);
                words >> p.name;
            }
            elements.back().properties.push_back(p);
        }
    }
    if (format < 0)
        throw ImportError("This PLY file is damaged (it doesn't say its format).");

    PlyReader in(bytes, bodyAt, format);
    RawMesh mesh;
    mesh.name = name;
    std::size_t points = 0;
    double red = 0, green = 0, blue = 0;
    std::size_t coloured = 0;
    for (const PlyElement& e : elements) {
        if (format != 0) {
            // Refuse counts the file can't hold before making room for them.
            std::size_t least = 0;
            for (const PlyProperty& p : e.properties)
                least += p.list ? plySize(p.countType) : plySize(p.type);
            if (least > 0 && e.count > in.left() / least)
                throw ImportError("This PLY file ends too soon (it may be cut short).");
        }
        const bool vertex = e.name == "vertex";
        const bool face = e.name == "face";
        if (vertex) {
            points = e.count;
            mesh.positions.reserve(points * 3);
        }
        for (std::size_t i = 0; i < e.count; ++i) {
            double xyz[3] = {0, 0, 0};
            double rgb[3] = {-1, -1, -1};
            for (const PlyProperty& p : e.properties) {
                if (p.list) {
                    const double n = in.read(p.countType);
                    if (n < 0 || n > 1000 || n != std::floor(n))
                        throw ImportError("This PLY file is damaged (a face with " + std::to_string(n) + " corners).");
                    std::vector<std::uint32_t> corners;
                    for (int k = 0; k < int(n); ++k) {
                        const double v = in.read(p.type);
                        if (face && (p.name == "vertex_indices" || p.name == "vertex_index")) {
                            if (v < 0 || v >= double(points) || v != std::floor(v))
                                throw ImportError("This PLY file refers to a point that doesn't exist.");
                            corners.push_back(std::uint32_t(v));
                        }
                    }
                    for (std::size_t k = 2; k < corners.size(); ++k)  // polygons: a fan of triangles
                        mesh.indices.insert(mesh.indices.end(), {corners[0], corners[k - 1], corners[k]});
                    if (mesh.indices.size() > kMostCorners)
                        throw ImportError("This PLY file is too big to open (over 30 million triangles).");
                    continue;
                }
                const double v = in.read(p.type);
                if (vertex) {
                    if (p.name == "x")
                        xyz[0] = v;
                    else if (p.name == "y")
                        xyz[1] = v;
                    else if (p.name == "z")
                        xyz[2] = v;
                    else if (p.name == "red" || p.name == "diffuse_red")
                        rgb[0] = p.type == PlyType::Float32 || p.type == PlyType::Float64 ? v * 255 : v;
                    else if (p.name == "green" || p.name == "diffuse_green")
                        rgb[1] = p.type == PlyType::Float32 || p.type == PlyType::Float64 ? v * 255 : v;
                    else if (p.name == "blue" || p.name == "diffuse_blue")
                        rgb[2] = p.type == PlyType::Float32 || p.type == PlyType::Float64 ? v * 255 : v;
                }
            }
            if (vertex) {
                for (double c : xyz)
                    mesh.positions.push_back(float(c));
                if (rgb[0] >= 0 && rgb[1] >= 0 && rgb[2] >= 0) {
                    red += rgb[0];
                    green += rgb[1];
                    blue += rgb[2];
                    ++coloured;
                }
            }
        }
    }
    if (mesh.indices.empty())
        throw ImportError(points > 0 ? "This PLY file has points but no faces (a point cloud): there's no surface "
                                       "to import. Turn it into a mesh in your scanning software first."
                                     : "This PLY file has no triangles to import.");
    // A scan coloured point by point: the shape takes their average colour.
    if (coloured > 0) {
        const auto c = [coloured](double sum) { return std::uint32_t(std::clamp(std::lround(sum / double(coloured)), 0L, 255L)); };
        mesh.colour = (c(red) << 16) | (c(green) << 8) | c(blue);
    }
    return {mesh};
}

std::vector<RawMesh> readGltf(const std::string& bytes, const std::string& name, const std::filesystem::path& folder)
{
    std::vector<std::string> buffers;
    json doc;
    if (bytes.size() >= 12 && bytes.compare(0, 4, "glTF") == 0) {
        // GLB: a header, then chunks: the description (JSON), then the data.
        std::uint32_t version = 0, length = 0;
        std::memcpy(&version, bytes.data() + 4, 4);
        std::memcpy(&length, bytes.data() + 8, 4);
        if (version != 2)
            throw ImportError("This is an old (version " + std::to_string(version) +
                              ") GLB file, which Forge3D can't read. Export it again as glTF 2.");
        std::size_t at = 12;
        std::string description, data;
        while (at + 8 <= bytes.size()) {
            std::uint32_t chunkLength = 0, kind = 0;
            std::memcpy(&chunkLength, bytes.data() + at, 4);
            std::memcpy(&kind, bytes.data() + at + 4, 4);
            at += 8;
            if (chunkLength > bytes.size() - at)
                throw ImportError("This GLB file ends too soon (it may be cut short).");
            if (kind == 0x4E4F534Au && description.empty())
                description = bytes.substr(at, chunkLength);
            else if (kind == 0x004E4942u && data.empty())
                data = bytes.substr(at, chunkLength);
            at += chunkLength;
        }
        if (description.empty())
            throw ImportError("This GLB file is damaged (it has no description).");
        doc = parseJson(description);
        for (const auto& b : doc.value("buffers", json::array())) {
            if (b.contains("uri") && b["uri"].is_string())
                buffers.push_back(std::string());  // filled in below
            else
                buffers.push_back(data);
        }
    } else {
        doc = parseJson(bytes);
        for (std::size_t i = 0; i < doc.value("buffers", json::array()).size(); ++i)
            buffers.push_back(std::string());
    }
    const json& list = doc.value("buffers", json::array());
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (!list[i].contains("uri") || !list[i]["uri"].is_string())
            continue;
        const std::string uri = list[i]["uri"].get<std::string>();
        if (uri.rfind("data:", 0) == 0) {
            const auto comma = uri.find(',');
            if (comma == std::string::npos || uri.find(";base64") == std::string::npos)
                throw ImportError("This glTF file is damaged (its embedded data can't be read).");
            buffers[i] = decodeBase64(uri.substr(comma + 1));
            continue;
        }
        // A file beside it (spaces may be written %20).
        std::string file;
        for (std::size_t k = 0; k < uri.size(); ++k) {
            if (uri[k] == '%' && k + 2 < uri.size() && std::isxdigit(static_cast<unsigned char>(uri[k + 1])) &&
                std::isxdigit(static_cast<unsigned char>(uri[k + 2]))) {
                file.push_back(char(std::stoi(uri.substr(k + 1, 2), nullptr, 16)));
                k += 2;
            } else {
                file.push_back(uri[k]);
            }
        }
        const std::filesystem::path path = folder / std::filesystem::path(std::u8string(file.begin(), file.end()));
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw ImportError("This glTF file keeps its data in '" + file +
                              "', which isn't in the same folder. Keep the two together, or export it as one GLB file.");
        std::ostringstream all;
        all << in.rdbuf();
        buffers[i] = all.str();
    }
    return Gltf(std::move(doc), std::move(buffers)).meshes(name);
}

}  // namespace forge
