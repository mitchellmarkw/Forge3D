// SPDX-License-Identifier: GPL-3.0-or-later
// A file-loading fuzzer (hidden: run with "[.fuzz]" and FORGE3D_FUZZ_CORPUS set to a
// folder of document JSON files, with any meshes beside them as "<n>-mesh-<id>.bin").
// Each file is loaded with one value at a time changed to something odd (zero,
// negative, huge, the wrong type, an empty list...), then every body is built. A
// damaged file must give an error, never a crash or a hang.
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

using namespace forge;
using nlohmann::json;

namespace {

void leaves(const json& j, const json::json_pointer& at, std::vector<json::json_pointer>& out)
{
    if (j.is_object()) {
        out.push_back(at);
        for (auto it = j.begin(); it != j.end(); ++it)
            leaves(it.value(), at / it.key(), out);
    } else if (j.is_array()) {
        out.push_back(at);
        for (std::size_t i = 0; i < j.size(); ++i)
            leaves(j[i], at / i, out);
    } else {
        out.push_back(at);
    }
}

std::vector<json> oddValues(const json& v)
{
    std::vector<json> out{json(nullptr), json("x"), json::array(), json::object()};
    if (v.is_number()) {
        const double d = v.get<double>();
        for (double x : {0.0, -1.0, -d, 1e-12, 1e9, 1e300, -1e300, d * 1000.0, 0.5})
            out.push_back(x);
        if (v.is_number_integer())
            for (long long x : {0LL, -1LL, 2147483647LL, -2147483648LL, 9223372036854775807LL})
                out.push_back(x);
    } else if (v.is_array()) {
        if (!v.empty()) {
            json shorter = v;
            shorter.erase(shorter.size() - 1);
            out.push_back(shorter);
            json longer = v;
            longer.push_back(v.back());
            out.push_back(longer);
            json doubled = v;
            for (const auto& e : v)
                doubled.push_back(e);
            out.push_back(doubled);
        }
    } else if (v.is_string()) {
        out.push_back("");
        out.push_back(std::string(5000, 'W'));  // refused: too many letters
        out.push_back(std::string(120, 'W'));   // the most allowed
        out.push_back("meshes/../../etc/passwd");
    } else if (v.is_boolean()) {
        out.push_back(!v.get<bool>());
        out.push_back(1);
    }
    return out;
}

}  // namespace

TEST_CASE("fuzz: damaged files give errors, not crashes", "[.fuzz]")
{
    const char* dir = std::getenv("FORGE3D_FUZZ_CORPUS");
    REQUIRE(dir);
    const int perFile = std::getenv("FORGE3D_FUZZ_PER_FILE") ? std::atoi(std::getenv("FORGE3D_FUZZ_PER_FILE")) : 80;
    std::mt19937 rng(12345);
    int files = 0, cases = 0, refused = 0, built = 0, buildErrors = 0, foreign = 0, unopened = 0;
    std::vector<std::filesystem::path> paths;
    for (const auto& e : std::filesystem::directory_iterator(dir))
        if (e.path().extension() == ".json")
            paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        const json original = json::parse(text.str());
        const std::string stem = path.stem().string();
        const auto readEntry = [&](const std::string& name) -> std::string {
            // "meshes/7.bin" -> "<stem>-mesh-7.bin"
            const auto slash = name.rfind('/');
            const std::string id = name.substr(slash + 1, name.size() - slash - 5);
            std::ifstream m(std::filesystem::path(dir) / (stem + "-mesh-" + id + ".bin"), std::ios::binary);
            if (!m)
                throw std::runtime_error("no entry " + name);
            std::stringstream b;
            b << m.rdbuf();
            return b.str();
        };
        ++files;
        try {  // the file as saved must still open
            (void)file::fromJson(original.dump(), readEntry);
        } catch (const std::exception& e) {
            ++unopened;
            std::cerr << "THE ORIGINAL DIDN'T OPEN: " << path.filename().string() << ": " << e.what() << std::endl;
        }
        std::vector<json::json_pointer> all;
        leaves(original, json::json_pointer(), all);
        std::shuffle(all.begin(), all.end(), rng);
        if (int(all.size()) > perFile)
            all.resize(std::size_t(perFile));
        for (const auto& at : all) {
            for (const json& odd : oddValues(original.at(at))) {
                json changed = original;
                if (at.empty())
                    continue;
                changed[at] = odd;
                ++cases;
                const std::string label = path.filename().string() + " " + at.to_string() + " = " + odd.dump().substr(0, 60);
                if (std::getenv("FORGE3D_FUZZ_VERBOSE"))
                    std::cerr << label << std::endl;
                if (const char* trace = std::getenv("FORGE3D_FUZZ_TRACE"))  // the case in hand, if it hangs
                    std::ofstream(trace, std::ios::trunc) << label << std::endl;
                const auto start = std::chrono::steady_clock::now();
                std::vector<Body> bodies;
                try {
                    bodies = file::fromJson(changed.dump(), readEntry);
                } catch (const std::exception&) {
                    ++refused;
                    continue;
                } catch (...) {
                    ++foreign;
                    std::cerr << "NOT A std::exception (opening): " << label << std::endl;
                    continue;
                }
                Document doc;
                try {
                    doc.replaceAll(std::move(bodies));
                } catch (const std::exception&) {
                    ++refused;
                    continue;
                } catch (...) {
                    ++foreign;
                    std::cerr << "NOT A std::exception (replaceAll): " << label << std::endl;
                    continue;
                }
                ++built;
                for (const Body& b : doc.bodies()) {
                    try {
                        (void)doc.mesh(b.id);
                        (void)doc.worldBounds(b.id);
                    } catch (const std::exception&) {
                        ++buildErrors;
                    } catch (...) {
                        ++foreign;
                        std::cerr << "NOT A std::exception (building): " << label << std::endl;
                    }
                }
                const double seconds =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                if (seconds > 20.0)
                    std::cerr << "SLOW (" << seconds << " s): " << label << std::endl;
            }
        }
    }
    std::cerr << "fuzz: " << files << " files, " << cases << " cases: " << refused << " refused, " << built
              << " opened (" << buildErrors << " body errors), " << foreign << " escaped as unknown exceptions"
              << std::endl;
    CHECK(files > 0);
    CHECK(foreign == 0);
    CHECK(unopened == 0);
}
