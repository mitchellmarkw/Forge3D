// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>

// Is there a newer Forge3D? Worked out from GitHub's list of releases.
namespace forge::updates {

// Where the list comes from (GitHub's API) and where people download from.
inline constexpr const char* kReleasesApi = "https://api.github.com/repos/mitchellmarkw/Forge3D/releases?per_page=20";
inline constexpr const char* kReleasesPage = "https://github.com/mitchellmarkw/Forge3D/releases";

struct Release {
    std::string version;  // "0.26.0" (the tag without its "v")
    std::string name;     // "Forge3D 0.26 Beta"
    std::string url;      // its page, to download from
};

// -1, 0 or 1 as version a is older than, the same as, or newer than b. Versions are
// dot-separated numbers ("0.25.0", a leading "v" is ignored); missing parts count as 0.
int compareVersions(const std::string& a, const std::string& b);

// The newest published release in GitHub's release list (JSON), if it's newer than
// `current`. Drafts are skipped. Throws std::runtime_error if the list can't be read.
std::optional<Release> newerRelease(const std::string& releasesJson, const std::string& current);

}  // namespace forge::updates
