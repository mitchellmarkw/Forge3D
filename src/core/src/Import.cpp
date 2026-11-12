// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Import.h"

#include "forge/core/Arrange.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"

#include <miniz.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace forge {

namespace {

// ---- small helpers ------------------------------------------------------------------

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

std::string readWholeFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw ImportError("Could not open the file. Check that it exists and isn't open in another program.");
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

// ---- STL ------------------------------------------------------------------------------

std::vector<RawMesh> readStlBinary(const std::string& bytes, const std::string& name)
{
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + 80, 4);
    RawMesh m;
    m.name = name;
    m.positions.reserve(std::size_t(count) * 9);
    m.indices.reserve(std::size_t(count) * 3);
    for (std::uint32_t t = 0; t < count; ++t) {
        const char* rec = bytes.data() + 84 + std::size_t(t) * 50 + 12;  // skip the normal
        for (int k = 0; k < 3; ++k) {
            float p[3];
            std::memcpy(p, rec + k * 12, 12);
            m.indices.push_back(static_cast<std::uint32_t>(m.positions.size() / 3));
            m.positions.insert(m.positions.end(), {p[0], p[1], p[2]});
        }
    }
    return {std::move(m)};
}

std::vector<RawMesh> readStlText(const std::string& text, const std::string& name)
{
    RawMesh m;
    m.name = name;
    std::istringstream in(text);
    std::string word;
    while (in >> word) {
        if (lower(word) != "vertex")
            continue;
        float x = 0, y = 0, z = 0;
        if (!(in >> x >> y >> z))
            throw ImportError("This STL file is damaged (a point has fewer than three numbers).");
        m.indices.push_back(static_cast<std::uint32_t>(m.positions.size() / 3));
        m.positions.insert(m.positions.end(), {x, y, z});
    }
    if (m.indices.size() % 3 != 0)
        throw ImportError("This STL file is damaged (a triangle has fewer than three corners).");
    return {std::move(m)};
}

// ---- 3MF: a very small XML reader ------------------------------------------------------

struct XmlTag {
    std::string name;  // without a namespace prefix
    std::map<std::string, std::string> attrs;
    bool closing = false;      // </name>
    bool selfClosing = false;  // <name ... />
};

// Calls `visit` for every tag in order (comments, declarations and text are skipped).
void scanXml(const std::string& xml, const std::function<void(const XmlTag&)>& visit)
{
    std::size_t at = 0;
    while ((at = xml.find('<', at)) != std::string::npos) {
        if (xml.compare(at, 4, "<!--") == 0) {
            const std::size_t end = xml.find("-->", at);
            at = end == std::string::npos ? xml.size() : end + 3;
            continue;
        }
        const std::size_t end = xml.find('>', at);
        if (end == std::string::npos)
            break;
        std::string body = xml.substr(at + 1, end - at - 1);
        at = end + 1;
        if (body.empty() || body[0] == '?' || body[0] == '!')
            continue;
        XmlTag tag;
        if (body[0] == '/') {
            tag.closing = true;
            body.erase(0, 1);
        }
        if (!body.empty() && body.back() == '/') {
            tag.selfClosing = true;
            body.pop_back();
        }
        std::size_t i = 0;
        while (i < body.size() && !std::isspace(static_cast<unsigned char>(body[i])))
            ++i;
        tag.name = body.substr(0, i);
        if (const auto colon = tag.name.find(':'); colon != std::string::npos)
            tag.name = tag.name.substr(colon + 1);
        // attributes: name="value" or name='value'
        while (i < body.size()) {
            while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i])))
                ++i;
            const std::size_t eq = body.find('=', i);
            if (eq == std::string::npos)
                break;
            std::string key = body.substr(i, eq - i);
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back())))
                key.pop_back();
            std::size_t q = eq + 1;
            while (q < body.size() && std::isspace(static_cast<unsigned char>(body[q])))
                ++q;
            if (q >= body.size() || (body[q] != '"' && body[q] != '\''))
                break;
            const char quote = body[q];
            const std::size_t close = body.find(quote, q + 1);
            if (close == std::string::npos)
                break;
            if (const auto colon = key.find(':'); colon != std::string::npos && key.substr(0, colon) != "xmlns")
                key = key.substr(colon + 1);
            tag.attrs[key] = body.substr(q + 1, close - q - 1);
            i = close + 1;
        }
        visit(tag);
    }
}

std::string unescape(std::string s)
{
    const std::pair<const char*, const char*> entities[] = {
        {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&amp;", "&"}};
    for (const auto& [from, to] : entities)
        for (std::size_t p = s.find(from); p != std::string::npos; p = s.find(from, p + std::strlen(to)))
            s.replace(p, std::strlen(from), to);
    return s;
}

// A 3MF transform: 12 numbers, rows of a 4 x 3 matrix applied as p' = p * M.
using Affine = std::array<double, 12>;
constexpr Affine kIdentity{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};

Affine parseTransform(const std::string& text)
{
    if (text.empty())
        return kIdentity;
    std::istringstream in(text);
    Affine m{};
    for (double& v : m)
        if (!(in >> v))
            throw ImportError("This 3MF file has a damaged transform.");
    return m;
}

Affine combine(const Affine& first, const Affine& then)  // apply `first`, then `then`
{
    Affine r{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col) {
            double v = row == 3 ? then[9 + col] : 0.0;
            for (int k = 0; k < 3; ++k)
                v += first[row * 3 + k] * then[k * 3 + col];
            r[row * 3 + col] = v;
        }
    return r;
}

struct MfObject {
    std::string name;
    std::vector<float> positions;
    std::vector<std::uint32_t> indices;
    std::vector<std::pair<std::string, Affine>> components;  // object id, transform
};

}  // namespace

// ---- readers -------------------------------------------------------------------------

std::vector<RawMesh> readStl(const std::string& bytes, const std::string& name)
{
    if (bytes.size() >= 84) {
        std::uint32_t count = 0;
        std::memcpy(&count, bytes.data() + 80, 4);
        if (std::size_t(84) + std::size_t(count) * 50 == bytes.size())
            return readStlBinary(bytes, name);
    }
    if (lower(bytes.substr(0, 5)) == "solid")
        return readStlText(bytes, name);
    throw ImportError("This doesn't look like an STL file (or it is damaged).");
}

std::vector<RawMesh> readObj(const std::string& text, const std::string& name)
{
    std::vector<std::array<float, 3>> points;  // every "v" in the file
    std::vector<RawMesh> out;
    RawMesh current;
    current.name = name;
    std::unordered_map<std::uint32_t, std::uint32_t> local;  // file point -> current mesh point
    const auto finish = [&] {
        if (!current.indices.empty())
            out.push_back(std::move(current));
        current = RawMesh{};
        local.clear();
    };

    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::istringstream ls(line);
        std::string kind;
        ls >> kind;
        if (kind == "v") {
            std::array<float, 3> p{};
            if (!(ls >> p[0] >> p[1] >> p[2]))
                throw ImportError("This OBJ file is damaged (line " + std::to_string(lineNo) + ").");
            points.push_back(p);
        } else if (kind == "o" || kind == "g") {
            std::string label;
            std::getline(ls, label);
            label.erase(0, label.find_first_not_of(" \t"));
            finish();
            current.name = label.empty() ? name : label;
        } else if (kind == "f") {
            std::vector<std::uint32_t> poly;
            std::string token;
            while (ls >> token) {
                const long long i = std::stoll(token.substr(0, token.find('/')));
                const long long n = static_cast<long long>(points.size());
                const long long index = i > 0 ? i - 1 : n + i;  // negative counts back from the last point
                if (index < 0 || index >= n)
                    throw ImportError("This OBJ file refers to a point that doesn't exist (line " +
                                      std::to_string(lineNo) + ").");
                const auto u = static_cast<std::uint32_t>(index);
                auto [it, added] = local.try_emplace(u, static_cast<std::uint32_t>(current.positions.size() / 3));
                if (added)
                    current.positions.insert(current.positions.end(), points[u].begin(), points[u].end());
                poly.push_back(it->second);
            }
            for (std::size_t k = 1; k + 1 < poly.size(); ++k)  // a fan of triangles
                current.indices.insert(current.indices.end(), {poly[0], poly[k], poly[k + 1]});
        }
    }
    finish();
    if (out.empty())
        throw ImportError("This OBJ file has no faces (only points or lines).");
    if (out.size() == 1 && out[0].name.empty())
        out[0].name = name;
    return out;
}

std::vector<RawMesh> read3mf(const std::string& bytes, const std::string& name)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        throw ImportError("This doesn't look like a 3MF file (it isn't a zip archive).");
    const auto entry = [&zip](const std::string& path) -> std::string {
        std::string p = path;
        if (!p.empty() && p[0] == '/')
            p.erase(0, 1);
        size_t size = 0;
        void* data = mz_zip_reader_extract_file_to_heap(&zip, p.c_str(), &size, 0);
        if (!data)
            return {};
        std::string out(static_cast<const char*>(data), size);
        mz_free(data);
        return out;
    };
    // The model part: named in _rels/.rels, usually 3D/3dmodel.model.
    std::string modelPath = "3D/3dmodel.model";
    scanXml(entry("_rels/.rels"), [&](const XmlTag& t) {
        if (t.name == "Relationship" && t.attrs.count("Type") &&
            t.attrs.at("Type").find("3dmodel") != std::string::npos && t.attrs.count("Target"))
            modelPath = t.attrs.at("Target");
    });
    const std::string model = entry(modelPath);
    mz_zip_reader_end(&zip);
    if (model.empty())
        throw ImportError("This 3MF file has no model inside.");

    double unit = 1.0;  // to millimetres
    std::map<std::string, MfObject> objects;
    std::vector<std::pair<std::string, Affine>> items;
    MfObject* current = nullptr;
    std::string currentId;
    const auto number = [](const XmlTag& t, const char* key) {
        const auto it = t.attrs.find(key);
        if (it == t.attrs.end())
            throw ImportError(std::string("This 3MF file is damaged (missing ") + key + ").");
        return std::stod(it->second);
    };
    scanXml(model, [&](const XmlTag& t) {
        if (t.closing) {
            if (t.name == "object")
                current = nullptr;
            return;
        }
        if (t.name == "model" && t.attrs.count("unit")) {
            const std::string u = t.attrs.at("unit");
            unit = u == "micron" ? 0.001 : u == "centimeter" ? 10.0 : u == "inch" ? 25.4 : u == "foot" ? 304.8
                 : u == "meter" ? 1000.0 : 1.0;
        } else if (t.name == "object") {
            currentId = t.attrs.count("id") ? t.attrs.at("id") : std::string();
            current = &objects[currentId];
            current->name = t.attrs.count("name") ? unescape(t.attrs.at("name")) : std::string();
            if (t.selfClosing)
                current = nullptr;
        } else if (current && t.name == "vertex") {
            current->positions.insert(current->positions.end(),
                                      {float(number(t, "x")), float(number(t, "y")), float(number(t, "z"))});
        } else if (current && t.name == "triangle") {
            current->indices.insert(current->indices.end(),
                                    {std::uint32_t(number(t, "v1")), std::uint32_t(number(t, "v2")),
                                     std::uint32_t(number(t, "v3"))});
        } else if (current && t.name == "component") {
            if (t.attrs.count("objectid"))
                current->components.emplace_back(t.attrs.at("objectid"),
                                                 parseTransform(t.attrs.count("transform") ? t.attrs.at("transform")
                                                                                           : std::string()));
        } else if (t.name == "item" && t.attrs.count("objectid")) {
            items.emplace_back(t.attrs.at("objectid"),
                               parseTransform(t.attrs.count("transform") ? t.attrs.at("transform") : std::string()));
        }
    });

    std::vector<RawMesh> out;
    std::function<void(const std::string&, const Affine&, RawMesh&, int)> addObject =
        [&](const std::string& id, const Affine& m, RawMesh& into, int depth) {
            const auto it = objects.find(id);
            if (it == objects.end() || depth > 16)
                return;
            const MfObject& o = it->second;
            const auto base = static_cast<std::uint32_t>(into.positions.size() / 3);
            for (std::size_t p = 0; p + 2 < o.positions.size(); p += 3) {
                const double x = o.positions[p], y = o.positions[p + 1], z = o.positions[p + 2];
                for (int c = 0; c < 3; ++c)
                    into.positions.push_back(float((x * m[c] + y * m[3 + c] + z * m[6 + c] + m[9 + c]) * unit));
            }
            const auto count = static_cast<std::uint32_t>(o.positions.size() / 3);
            for (std::uint32_t i : o.indices) {
                if (i >= count)
                    throw ImportError("This 3MF file refers to a point that doesn't exist.");
                into.indices.push_back(base + i);
            }
            for (const auto& [child, cm] : o.components)
                addObject(child, combine(cm, m), into, depth + 1);
        };
    if (items.empty())  // no build list: take every object with triangles
        for (const auto& [id, o] : objects)
            if (!o.indices.empty())
                items.emplace_back(id, kIdentity);
    int n = 0;
    for (const auto& [id, m] : items) {
        RawMesh mesh;
        const auto it = objects.find(id);
        ++n;
        mesh.name = it != objects.end() && !it->second.name.empty() ? it->second.name
                    : items.size() == 1                            ? name
                                                                   : name + " " + std::to_string(n);
        addObject(id, m, mesh, 0);
        if (!mesh.indices.empty())
            out.push_back(std::move(mesh));
    }
    if (out.empty())
        throw ImportError("This 3MF file has no triangles to import.");
    return out;
}

std::vector<RawMesh> readMeshFile(const std::filesystem::path& path)
{
    const std::string ext = lower(path.extension().string());
    const std::string name = path.stem().string();
    if (ext == ".stl")
        return readStl(readWholeFile(path), name);
    if (ext == ".obj")
        return readObj(readWholeFile(path), name);
    if (ext == ".3mf")
        return read3mf(readWholeFile(path), name);
    throw ImportError("Forge3D can import .stl, .3mf and .obj files.");
}

// ---- repair --------------------------------------------------------------------------

namespace {

}  // namespace

std::string RepairReport::summary() const
{
    std::vector<std::string> done;
    const auto note = [&done](std::size_t n, const char* one, const char* many) {
        if (n == 1)
            done.push_back(one);
        else if (n > 1)
            done.push_back(std::to_string(n) + " " + many);
    };
    note(pointsMerged, "1 duplicate point joined", "duplicate points joined");
    note(degenerateRemoved, "1 empty triangle removed", "empty triangles removed");
    note(duplicatesRemoved, "1 repeated triangle removed", "repeated triangles removed");
    note(trianglesFlipped, "1 triangle turned round", "triangles turned round");
    note(partsTurnedOut, "1 inside-out piece turned the right way", "inside-out pieces turned the right way");
    note(holesFilled, "1 hole filled", "holes filled");
    std::string s;
    if (done.empty()) {
        s = "No repairs needed.";
    } else {
        s = "Repaired: ";
        for (std::size_t i = 0; i < done.size(); ++i)
            s += (i ? (i + 1 == done.size() ? " and " : ", ") : "") + done[i];
        s += ".";
    }
    if (openEdges > 0 || nonManifoldEdges > 0)
        s += " Still not watertight (" + std::to_string(openEdges + nonManifoldEdges) +
             " edges): it can be moved, resized and exported, but not combined with other shapes.";
    return s;
}

namespace {

// Every triangle edge, sorted so that the uses of one edge sit together.
// For corner k of triangle t (edge from corner k to corner k+1), `slot` = t*3+k.
struct EdgeTable {
    std::vector<std::pair<std::uint64_t, std::uint32_t>> uses;  // (undirected edge key, slot), sorted
    std::vector<std::int64_t> neighbour;  // per slot: the other slot sharing the edge (exactly two uses), else -1
    std::vector<std::uint32_t> useCount;  // per slot: how many triangles use its edge

    static std::uint64_t key(std::uint32_t a, std::uint32_t b)
    {
        return a < b ? (std::uint64_t(a) << 32) | b : (std::uint64_t(b) << 32) | a;
    }

    void build(const std::vector<std::array<std::uint32_t, 3>>& tris)
    {
        uses.clear();
        uses.reserve(tris.size() * 3);
        for (std::size_t t = 0; t < tris.size(); ++t)
            for (int k = 0; k < 3; ++k)
                uses.emplace_back(key(tris[t][k], tris[t][(k + 1) % 3]), static_cast<std::uint32_t>(t * 3 + k));
        std::sort(uses.begin(), uses.end());
        neighbour.assign(tris.size() * 3, -1);
        useCount.assign(tris.size() * 3, 0);
        for (std::size_t i = 0; i < uses.size();) {
            std::size_t j = i;
            while (j < uses.size() && uses[j].first == uses[i].first)
                ++j;
            const auto n = static_cast<std::uint32_t>(j - i);
            for (std::size_t k = i; k < j; ++k)
                useCount[uses[k].second] = n;
            if (n == 2) {
                neighbour[uses[i].second] = uses[i + 1].second;
                neighbour[uses[i + 1].second] = uses[i].second;
            }
            i = j;
        }
    }
};

}  // namespace

std::shared_ptr<const MeshData> repairMesh(const RawMesh& raw, RepairReport& report, Vec3& offset)
{
    report = RepairReport{};
    report.trianglesIn = raw.indices.size() / 3;
    if (raw.indices.empty())
        throw ImportError("'" + raw.name + "' has no triangles.");
    const std::size_t rawPoints = raw.positions.size() / 3;
    for (std::uint32_t i : raw.indices)
        if (i >= rawPoints)
            throw ImportError("'" + raw.name + "' refers to a point that doesn't exist.");

    // 1. Join points closer than a millionth of the size (sort by grid cell).
    std::array<double, 3> lo{1e300, 1e300, 1e300}, hi{-1e300, -1e300, -1e300};
    for (std::size_t p = 0; p < rawPoints; ++p)
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::min(lo[c], double(raw.positions[p * 3 + c]));
            hi[c] = std::max(hi[c], double(raw.positions[p * 3 + c]));
        }
    const double diag = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) +
                                  (hi[2] - lo[2]) * (hi[2] - lo[2]));
    const double grid = std::max(diag * 1e-6, 1e-7);
    std::vector<std::pair<std::array<long long, 3>, std::uint32_t>> cells(rawPoints);
    for (std::size_t p = 0; p < rawPoints; ++p)
        cells[p] = {{std::llround(raw.positions[p * 3] / grid), std::llround(raw.positions[p * 3 + 1] / grid),
                     std::llround(raw.positions[p * 3 + 2] / grid)},
                    static_cast<std::uint32_t>(p)};
    std::sort(cells.begin(), cells.end());
    std::vector<double> pts;  // welded points
    pts.reserve(rawPoints * 3);
    std::vector<std::uint32_t> remap(rawPoints);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i == 0 || cells[i].first != cells[i - 1].first) {
            const std::uint32_t p = cells[i].second;
            pts.insert(pts.end(), {raw.positions[p * 3], raw.positions[p * 3 + 1], raw.positions[p * 3 + 2]});
        }
        remap[cells[i].second] = static_cast<std::uint32_t>(pts.size() / 3 - 1);
    }
    cells = {};
    // Only count points that really were shared between triangles (STL repeats every corner).
    std::vector<char> usedRaw(rawPoints, 0);
    std::size_t usedCount = 0;
    for (std::uint32_t i : raw.indices)
        if (!usedRaw[i]) {
            usedRaw[i] = 1;
            ++usedCount;
        }
    const bool stlLike = rawPoints == raw.indices.size();  // every corner its own point: joining is normal
    report.pointsMerged = !stlLike && usedCount > pts.size() / 3 ? usedCount - pts.size() / 3 : 0;

    const auto point = [&pts](std::uint32_t i) { return std::array<double, 3>{pts[i * 3], pts[i * 3 + 1], pts[i * 3 + 2]}; };
    const auto areaTwice = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        const auto A = point(a), B = point(b), C = point(c);
        const double ux = B[0] - A[0], uy = B[1] - A[1], uz = B[2] - A[2];
        const double vx = C[0] - A[0], vy = C[1] - A[1], vz = C[2] - A[2];
        const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        return std::sqrt(nx * nx + ny * ny + nz * nz);
    };

    // 2. Drop triangles with no area, then repeated triangles (same three corners).
    std::vector<std::array<std::uint32_t, 3>> tris;
    tris.reserve(raw.indices.size() / 3);
    for (std::size_t t = 0; t + 2 < raw.indices.size(); t += 3) {
        const std::uint32_t a = remap[raw.indices[t]], b = remap[raw.indices[t + 1]], c = remap[raw.indices[t + 2]];
        if (a == b || b == c || a == c || areaTwice(a, b, c) < grid * grid) {
            ++report.degenerateRemoved;
            continue;
        }
        tris.push_back({a, b, c});
    }
    {
        std::vector<std::pair<std::array<std::uint32_t, 3>, std::uint32_t>> keys(tris.size());
        for (std::size_t t = 0; t < tris.size(); ++t) {
            auto k = tris[t];
            std::sort(k.begin(), k.end());
            keys[t] = {k, static_cast<std::uint32_t>(t)};
        }
        std::sort(keys.begin(), keys.end());
        std::vector<char> keep(tris.size(), 1);
        for (std::size_t i = 1; i < keys.size(); ++i)
            if (keys[i].first == keys[i - 1].first) {
                keep[keys[i].second] = 0;  // keeps the first of each kind (sort is stable on the index)
                ++report.duplicatesRemoved;
            }
        std::size_t w = 0;
        for (std::size_t t = 0; t < tris.size(); ++t)
            if (keep[t])
                tris[w++] = tris[t];
        tris.resize(w);
    }
    if (tris.empty())
        throw ImportError("'" + raw.name + "' has no usable triangles.");

    // 3. Make neighbours face the same way, piece by piece (through edges shared by two).
    EdgeTable edges;
    edges.build(tris);
    const auto directedAt = [&tris](std::uint32_t slot) {
        const auto& t = tris[slot / 3];
        return std::pair{t[slot % 3], t[(slot % 3 + 1) % 3]};
    };
    std::vector<int> piece(tris.size(), -1);
    std::vector<char> flipped(tris.size(), 0);
    int pieces = 0;
    std::vector<std::size_t> stack;
    for (std::size_t start = 0; start < tris.size(); ++start) {
        if (piece[start] >= 0)
            continue;
        stack.assign(1, start);
        piece[start] = pieces;
        while (!stack.empty()) {
            const std::size_t t = stack.back();
            stack.pop_back();
            for (int k = 0; k < 3; ++k) {
                const std::int64_t other = edges.neighbour[t * 3 + k];
                if (other < 0)
                    continue;  // an edge of a hole, or shared by too many
                const std::size_t n = std::size_t(other) / 3;
                if (piece[n] >= 0)
                    continue;
                // Both run the same way (as they are now, after earlier flips)?
                auto mine = directedAt(static_cast<std::uint32_t>(t * 3 + k));
                if (flipped[t])
                    std::swap(mine.first, mine.second);
                const auto theirs = directedAt(static_cast<std::uint32_t>(other));
                if (mine == theirs) {
                    flipped[n] = 1;
                    ++report.trianglesFlipped;
                }
                piece[n] = pieces;
                stack.push_back(n);
            }
        }
        ++pieces;
    }
    for (std::size_t t = 0; t < tris.size(); ++t)
        if (flipped[t])
            std::swap(tris[t][1], tris[t][2]);

    // 4. Fill holes: walk each loop of edges used once, and close it with a fan.
    edges.build(tris);
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> next;  // hole edge b -> a (reverse of a -> b)
    for (std::size_t slot = 0; slot < tris.size() * 3; ++slot)
        if (edges.useCount[slot] == 1) {
            const auto [a, b] = directedAt(static_cast<std::uint32_t>(slot));
            next[b].push_back(a);
        }
    std::unordered_set<std::uint32_t> visited;
    const std::size_t before = tris.size();
    for (const auto& [start, outs] : next) {
        if (visited.count(start) || outs.size() != 1)
            continue;
        std::vector<std::uint32_t> loop{start};
        bool closed = false;
        for (std::uint32_t at = outs[0]; loop.size() <= next.size();) {
            if (at == start) {
                closed = true;
                break;
            }
            const auto it = next.find(at);
            if (it == next.end() || it->second.size() != 1 || visited.count(at))
                break;  // a messy boundary: leave it
            loop.push_back(at);
            at = it->second[0];
        }
        for (std::uint32_t v : loop)
            visited.insert(v);
        if (!closed || loop.size() < 3)
            continue;
        if (loop.size() == 3) {
            tris.push_back({loop[0], loop[1], loop[2]});
        } else {
            std::array<double, 3> c{0, 0, 0};
            for (std::uint32_t v : loop)
                for (int k = 0; k < 3; ++k)
                    c[k] += pts[v * 3 + k] / double(loop.size());
            const auto centre = static_cast<std::uint32_t>(pts.size() / 3);
            pts.insert(pts.end(), c.begin(), c.end());
            for (std::size_t i = 0; i < loop.size(); ++i)
                tris.push_back({loop[i], loop[(i + 1) % loop.size()], centre});
        }
        ++report.holesFilled;
    }
    if (tris.size() != before) {
        edges.build(tris);
        // Pieces may have joined: label them again.
        piece.assign(tris.size(), -1);
        pieces = 0;
        for (std::size_t s0 = 0; s0 < tris.size(); ++s0) {
            if (piece[s0] >= 0)
                continue;
            stack.assign(1, s0);
            piece[s0] = pieces;
            while (!stack.empty()) {
                const std::size_t t = stack.back();
                stack.pop_back();
                for (int k = 0; k < 3; ++k)
                    if (const std::int64_t o = edges.neighbour[t * 3 + k]; o >= 0 && piece[std::size_t(o) / 3] < 0) {
                        piece[std::size_t(o) / 3] = pieces;
                        stack.push_back(std::size_t(o) / 3);
                    }
            }
            ++pieces;
        }
    }

    // 5. Each piece faces outwards: turn over any that encloses a negative volume.
    std::vector<double> volume(static_cast<std::size_t>(pieces), 0.0);
    for (std::size_t t = 0; t < tris.size(); ++t) {
        const auto A = point(tris[t][0]), B = point(tris[t][1]), C = point(tris[t][2]);
        volume[static_cast<std::size_t>(piece[t])] +=
            (A[0] * (B[1] * C[2] - B[2] * C[1]) - A[1] * (B[0] * C[2] - B[2] * C[0]) + A[2] * (B[0] * C[1] - B[1] * C[0])) / 6.0;
    }
    for (double v : volume)
        if (v < 0.0)
            ++report.partsTurnedOut;
    for (std::size_t t = 0; t < tris.size(); ++t)
        if (volume[static_cast<std::size_t>(piece[t])] < 0.0)
            std::swap(tris[t][1], tris[t][2]);

    for (std::size_t i = 0; i < edges.uses.size();) {
        std::size_t j = i;
        while (j < edges.uses.size() && edges.uses[j].first == edges.uses[i].first)
            ++j;
        if (j - i == 1)
            ++report.openEdges;
        else if (j - i > 2)
            ++report.nonManifoldEdges;
        i = j;
    }
    report.trianglesOut = tris.size();

    // 6. Centre it like a primitive: middle of X / Y at 0, lowest point at Z = 0.
    lo = {1e300, 1e300, 1e300};
    hi = {-1e300, -1e300, -1e300};
    std::vector<char> used(pts.size() / 3, 0);
    for (const auto& t : tris)
        for (std::uint32_t v : t)
            used[v] = 1;
    for (std::size_t p = 0; p < used.size(); ++p)
        if (used[p])
            for (int c = 0; c < 3; ++c) {
                lo[c] = std::min(lo[c], pts[p * 3 + c]);
                hi[c] = std::max(hi[c], pts[p * 3 + c]);
            }
    offset = {(lo[0] + hi[0]) / 2.0, (lo[1] + hi[1]) / 2.0, lo[2]};
    auto out = std::make_shared<MeshData>();
    std::vector<std::uint32_t> compact(used.size(), 0);
    out->positions.reserve(used.size() * 3);
    for (std::size_t p = 0; p < used.size(); ++p) {
        if (!used[p])
            continue;
        compact[p] = static_cast<std::uint32_t>(out->positions.size() / 3);
        out->positions.insert(out->positions.end(), {float(pts[p * 3] - offset[0]), float(pts[p * 3 + 1] - offset[1]),
                                                     float(pts[p * 3 + 2] - offset[2])});
    }
    out->indices.reserve(tris.size() * 3);
    for (const auto& t : tris)
        out->indices.insert(out->indices.end(), {compact[t[0]], compact[t[1]], compact[t[2]]});
    out->size = {float(std::max(hi[0] - lo[0], 1e-6)), float(std::max(hi[1] - lo[1], 1e-6)),
                 float(std::max(hi[2] - lo[2], 1e-6))};
    out->closed = report.openEdges == 0 && report.nonManifoldEdges == 0;
    return out;
}

ImportResult importFile(CommandStack& stack, Document& doc, const std::filesystem::path& path,
                        const std::function<double(double largestSide)>& askScale)
{
    const std::vector<RawMesh> raws = readMeshFile(path);
    ImportResult result;
    std::vector<Body> bodies;
    Bounds all;  // of the whole file, in its own coordinates
    for (const RawMesh& raw : raws) {
        RepairReport report;
        Vec3 offset;
        const auto data = repairMesh(raw, report, offset);
        Body b;
        // The name from the file ("bracket"), unless a shape already has it ("bracket 2").
        const std::string base = raw.name.empty() ? "Mesh" : raw.name;
        const auto taken = [&](const std::string& n) {
            if (std::any_of(doc.bodies().begin(), doc.bodies().end(), [&](const Body& o) { return o.name == n; }))
                return true;
            return std::any_of(bodies.begin(), bodies.end(), [&](const Body& o) { return o.name == n; });
        };
        b.name = base;
        for (int n = 2; taken(b.name); ++n)
            b.name = base + " " + std::to_string(n);
        MeshSpec spec;
        spec.data = data;
        spec.width = data->size[0];
        spec.depth = data->size[1];
        spec.height = data->size[2];
        b.spec = spec;
        b.placement.x = offset[0];
        b.placement.y = offset[1];
        b.placement.z = offset[2];
        all.expand(float(offset[0] - spec.width / 2), float(offset[1] - spec.depth / 2), float(offset[2]));
        all.expand(float(offset[0] + spec.width / 2), float(offset[1] + spec.depth / 2), float(offset[2] + spec.height));
        bodies.push_back(std::move(b));
        result.reports.push_back(report);
        result.names.push_back(raw.name);
    }
    // No units in STL / OBJ: a tiny result was probably saved in metres or inches.
    const std::string ext = lower(path.extension().string());
    const double largest = std::max({all.size()[0], all.size()[1], all.size()[2]});
    if (askScale && ext != ".3mf" && largest < kTinyImport) {
        const double s = askScale(largest);
        if (s > 0.0 && s != 1.0) {
            for (Body& b : bodies) {
                auto& spec = std::get<MeshSpec>(b.spec);
                spec.width *= s;
                spec.depth *= s;
                spec.height *= s;
                b.placement.x *= s;
                b.placement.y *= s;
                b.placement.z *= s;
            }
            all = Bounds{};
            for (const Body& b : bodies) {
                const auto& spec = std::get<MeshSpec>(b.spec);
                all.expand(float(b.placement.x - spec.width / 2), float(b.placement.y - spec.depth / 2),
                           float(b.placement.z));
                all.expand(float(b.placement.x + spec.width / 2), float(b.placement.y + spec.depth / 2),
                           float(b.placement.z + spec.height));
            }
        }
    }

    // Together: lowest point on the plate; centred on the plate if it's empty, else to
    // the right of what's there.
    Vec3 shift{-(all.min[0] + all.max[0]) / 2.0, -(all.min[1] + all.max[1]) / 2.0, -double(all.min[2])};
    if (!doc.empty())
        shift[0] = clearOfPlate(doc, all.min[0])[0];
    stack.beginMacro("Import " + path.filename().string());
    try {
        for (Body& b : bodies) {
            b.id = doc.allocateId();
            b.placement.x += shift[0];
            b.placement.y += shift[1];
            b.placement.z += shift[2];
            stack.execute(std::make_unique<AddBodyCommand>(b));
            result.bodies.push_back(b.id);
        }
    } catch (...) {
        stack.endMacro();
        stack.undo();
        throw;
    }
    stack.endMacro();
    return result;
}

}  // namespace forge
