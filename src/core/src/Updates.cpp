// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Updates.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <stdexcept>
#include <vector>

namespace forge::updates {

namespace {
std::vector<long> parts(const std::string& version)
{
    std::vector<long> out;
    std::size_t i = 0;
    if (i < version.size() && (version[i] == 'v' || version[i] == 'V'))
        ++i;
    long n = 0;
    bool digits = false;
    for (; i < version.size(); ++i) {
        const char c = version[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            n = n * 10 + (c - '0');
            digits = true;
        } else if (c == '.') {
            out.push_back(n);
            n = 0;
            digits = false;
        } else {
            break;  // "-beta" and the like
        }
    }
    if (digits || !out.empty())
        out.push_back(n);
    return out;
}
}  // namespace

int compareVersions(const std::string& a, const std::string& b)
{
    const auto pa = parts(a), pb = parts(b);
    for (std::size_t i = 0; i < std::max(pa.size(), pb.size()); ++i) {
        const long x = i < pa.size() ? pa[i] : 0, y = i < pb.size() ? pb[i] : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

std::optional<Release> newerRelease(const std::string& releasesJson, const std::string& current)
{
    nlohmann::json list;
    try {
        list = nlohmann::json::parse(releasesJson);
    } catch (const std::exception&) {
        throw std::runtime_error("GitHub's answer couldn't be read");
    }
    if (!list.is_array())
        throw std::runtime_error("GitHub's answer wasn't a list of releases");
    std::optional<Release> best;
    for (const auto& r : list) {
        if (!r.is_object() || r.value("draft", false))
            continue;
        const std::string tag = r.value("tag_name", std::string());
        if (parts(tag).empty())
            continue;
        Release release;
        release.version = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
        release.name = r.value("name", std::string());
        if (release.name.empty())
            release.name = "Forge3D " + release.version;
        release.url = r.value("html_url", std::string(kReleasesPage));
        if (!best || compareVersions(release.version, best->version) > 0)
            best = std::move(release);
    }
    if (best && compareVersions(best->version, current) > 0)
        return best;
    return std::nullopt;
}

}  // namespace forge::updates
