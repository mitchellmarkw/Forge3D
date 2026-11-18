// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 41: PLY and glTF / GLB.
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/Import.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace forge;
using Catch::Matchers::WithinAbs;
namespace fs = std::filesystem;

namespace {

// A 10 mm cube: 8 corners, 6 square faces (outward).
const float kCorners[8][3] = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0},
                              {0, 0, 10}, {10, 0, 10}, {10, 10, 10}, {0, 10, 10}};
const int kQuads[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};

std::string plyText()
{
    std::string s = "ply\nformat ascii 1.0\ncomment made by hand\nelement vertex 8\nproperty float x\nproperty float y\n"
                    "property float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\n"
                    "element face 6\nproperty list uchar int vertex_indices\nend_header\n";
    for (const auto& c : kCorners)
        s += std::to_string(c[0]) + " " + std::to_string(c[1]) + " " + std::to_string(c[2]) + " 200 100 50\n";
    for (const auto& q : kQuads)
        s += "4 " + std::to_string(q[0]) + " " + std::to_string(q[1]) + " " + std::to_string(q[2]) + " " +
             std::to_string(q[3]) + "\n";
    return s;
}

template <typename T>
void put(std::string& s, T v, bool big)
{
    char b[sizeof(T)];
    std::memcpy(b, &v, sizeof v);
    if (big)
        std::reverse(b, b + sizeof v);
    s.append(b, sizeof v);
}

std::string plyBinary(bool big)
{
    std::string s = std::string("ply\nformat ") + (big ? "binary_big_endian" : "binary_little_endian") +
                    " 1.0\nelement vertex 8\nproperty double x\nproperty double y\nproperty double z\n"
                    "element face 6\nproperty list uchar uint vertex_index\nelement extra 2\nproperty short n\n"
                    "end_header\n";
    for (const auto& c : kCorners)
        for (float v : c)
            put<double>(s, v, big);
    for (const auto& q : kQuads) {
        put<std::uint8_t>(s, 4, big);
        for (int i : q)
            put<std::uint32_t>(s, std::uint32_t(i), big);
    }
    put<std::int16_t>(s, 7, big);
    put<std::int16_t>(s, 8, big);
    return s;
}

// The cube as glTF data: 8 points (in metres, Y up) and 36 indices.
std::string cubeData()
{
    std::string d;
    for (const auto& c : kCorners) {  // our (x, y, z) Z-up mm -> glTF (x, z, -y) Y-up m
        put<float>(d, c[0] / 1000.0f, false);
        put<float>(d, c[2] / 1000.0f, false);
        put<float>(d, -c[1] / 1000.0f, false);
    }
    for (const auto& q : kQuads)
        for (int i : {q[0], q[1], q[2], q[0], q[2], q[3]})
            put<std::uint16_t>(d, std::uint16_t(i), false);
    return d;
}

std::string gltfJson(const std::string& bufferUri, bool withUri, const std::string& nodeExtra = "")
{
    return std::string(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],)") +
           R"("nodes":[{"name":"Holder","translation":[0.02,0,0],"children":[1]},{"name":"Cube","mesh":0)" + nodeExtra +
           R"(}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],)" +
           R"("materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0.2158605,0,1]}}],)" +
           R"("accessors":[{"bufferView":0,"componentType":5126,"count":8,"type":"VEC3"},)" +
           R"({"bufferView":1,"componentType":5123,"count":36,"type":"SCALAR"}],)" +
           R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":96},{"buffer":0,"byteOffset":96,"byteLength":72}],)" +
           R"("buffers":[{"byteLength":168)" + (withUri ? R"(,"uri":")" + bufferUri + "\"" : std::string()) + "}]}";
}

std::string glb(const std::string& description, const std::string& data)
{
    std::string json = description;
    while (json.size() % 4)
        json += ' ';
    std::string bin = data;
    while (bin.size() % 4)
        bin += '\0';
    std::string s = "glTF";
    put<std::uint32_t>(s, 2, false);
    put<std::uint32_t>(s, std::uint32_t(12 + 8 + json.size() + 8 + bin.size()), false);
    put<std::uint32_t>(s, std::uint32_t(json.size()), false);
    put<std::uint32_t>(s, 0x4E4F534Au, false);
    s += json;
    put<std::uint32_t>(s, std::uint32_t(bin.size()), false);
    put<std::uint32_t>(s, 0x004E4942u, false);
    s += bin;
    return s;
}

std::string base64(const std::string& in)
{
    static const char* letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    unsigned int buffer = 0;
    int bits = 0;
    for (unsigned char c : in) {
        buffer = (buffer << 8) | c;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out += letters[(buffer >> bits) & 63];
        }
    }
    if (bits > 0)
        out += letters[(buffer << (6 - bits)) & 63];
    while (out.size() % 4)
        out += '=';
    return out;
}

void checkCube(const RawMesh& m, double dx = 0.0)
{
    REQUIRE(m.indices.size() == 36);
    double lo[3] = {1e9, 1e9, 1e9}, hi[3] = {-1e9, -1e9, -1e9};
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::min(lo[c], double(m.positions[i + c]));
            hi[c] = std::max(hi[c], double(m.positions[i + c]));
        }
    CHECK_THAT(lo[0], WithinAbs(dx, 1e-4));
    CHECK_THAT(hi[0], WithinAbs(dx + 10, 1e-4));
    CHECK_THAT(lo[1], WithinAbs(0, 1e-4));
    CHECK_THAT(hi[1], WithinAbs(10, 1e-4));
    CHECK_THAT(lo[2], WithinAbs(0, 1e-4));
    CHECK_THAT(hi[2], WithinAbs(10, 1e-4));
    // Facing outward: the volume comes out positive.
    double v = 0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &m.positions[m.indices[t] * 3];
        const float* b = &m.positions[m.indices[t + 1] * 3];
        const float* c = &m.positions[m.indices[t + 2] * 3];
        v += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
    }
    CHECK_THAT(v / 6.0, WithinAbs(1000.0, 0.01));
}

}  // namespace

TEST_CASE("PLY: text and binary, squares split, colours averaged", "[import][ply]")
{
    const auto text = readPly(plyText(), "scan");
    REQUIRE(text.size() == 1);
    checkCube(text[0]);
    CHECK(text[0].colour == std::optional<std::uint32_t>(0xC86432));
    for (bool big : {false, true}) {
        const auto bin = readPly(plyBinary(big), "scan");
        REQUIRE(bin.size() == 1);
        checkCube(bin[0]);
        CHECK_FALSE(bin[0].colour.has_value());
    }
    // Damaged and unusable files.
    CHECK_THROWS_AS(readPly("hello", "x"), ImportError);
    CHECK_THROWS_AS(readPly(plyBinary(false).substr(0, 300), "x"), ImportError);  // cut short
    std::string wrong = plyText();
    wrong.replace(wrong.rfind("4 3 0 4 7"), 9, "4 3 0 4 99");
    CHECK_THROWS_AS(readPly(wrong, "x"), ImportError);
    CHECK_THROWS_AS(readPly("ply\nformat ascii 1.0\nelement vertex 1\nproperty float x\nproperty float y\n"
                            "property float z\nend_header\n0 0 0\n",
                            "cloud"),
                    ImportError);  // a point cloud: no surface
    CHECK_THROWS_AS(readPly("ply\nformat binary_little_endian 1.0\nelement vertex 4000000000\nproperty float x\n"
                            "end_header\n",
                            "x"),
                    ImportError);  // more points than the file holds
}

TEST_CASE("glTF and GLB: placed by their nodes, Y-up metres to Z-up millimetres", "[import][gltf]")
{
    const std::string data = cubeData();
    // The holder node moves the cube 20 mm along X.
    const auto fromGlb = readGltf(glb(gltfJson("", false), data), "file", {});
    REQUIRE(fromGlb.size() == 1);
    CHECK(fromGlb[0].name == "Cube");
    checkCube(fromGlb[0], 20.0);
    CHECK(fromGlb[0].colour == std::optional<std::uint32_t>(0xFF8000));  // linear 0.216 -> sRGB 128

    const auto embedded =
        readGltf(gltfJson("data:application/octet-stream;base64," + base64(data), true), "file", {});
    checkCube(embedded.at(0), 20.0);

    const fs::path dir = fs::temp_directory_path() / ("forge3d-gltf-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    std::ofstream(dir / "cube data.bin", std::ios::binary) << data;
    std::ofstream(dir / "cube.gltf") << gltfJson("cube%20data.bin", true);
    checkCube(readMeshFile(dir / "cube.gltf").at(0), 20.0);
    fs::remove(dir / "cube data.bin");
    CHECK_THROWS_AS(readMeshFile(dir / "cube.gltf"), ImportError);  // its data file is missing
    fs::remove_all(dir);

    // A mirroring node: still facing outward.
    const auto mirrored = readGltf(glb(gltfJson("", false, R"(,"scale":[-1,1,1])"), data), "file", {});
    REQUIRE(mirrored.size() == 1);
    double v = 0;
    const RawMesh& m = mirrored[0];
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* a = &m.positions[m.indices[t] * 3];
        const float* b = &m.positions[m.indices[t + 1] * 3];
        const float* c = &m.positions[m.indices[t + 2] * 3];
        v += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
    }
    CHECK(v > 0);

    // Damaged: data that runs past the end, an index out of range, compression, old versions.
    CHECK_THROWS_AS(readGltf(glb(gltfJson("", false), data.substr(0, 100)), "x", {}), ImportError);
    std::string badIndex = data;
    badIndex[96] = char(50);
    CHECK_THROWS_AS(readGltf(glb(gltfJson("", false), badIndex), "x", {}), ImportError);
    std::string draco = gltfJson("", false);
    draco.insert(1, R"("extensionsRequired":["KHR_draco_mesh_compression"],)");
    CHECK_THROWS_AS(readGltf(glb(draco, data), "x", {}), ImportError);
    CHECK_THROWS_AS(readGltf(R"({"asset":{"version":"1.0"}})", "x", {}), ImportError);
    CHECK_THROWS_AS(readGltf("not json", "x", {}), ImportError);
}

TEST_CASE("Importing a GLB makes a shape in millimetres without asking", "[import][gltf]")
{
    const fs::path dir = fs::temp_directory_path() / ("forge3d-glb-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    std::ofstream(dir / "cube.glb", std::ios::binary) << glb(gltfJson("", false), cubeData());
    Document doc;
    CommandStack stack(doc);
    bool asked = false;
    const auto result = importFile(stack, doc, dir / "cube.glb", [&asked](double) {
        asked = true;
        return 1.0;
    });
    CHECK_FALSE(asked);  // glTF says its units
    REQUIRE(result.bodies.size() == 1);
    const Bounds b = doc.worldBounds(result.bodies[0]);
    CHECK_THAT(b.size()[0], WithinAbs(10.0, 1e-3));
    CHECK(doc.colourOf(result.bodies[0]) == 0xFF8000);
    fs::remove_all(dir);
}
