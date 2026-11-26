// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Updates.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace forge::updates;

TEST_CASE("Updates: comparing versions", "[updates]")
{
    CHECK(compareVersions("0.25.0", "0.25.0") == 0);
    CHECK(compareVersions("v0.25.0", "0.25") == 0);
    CHECK(compareVersions("0.25.1", "0.25.0") == 1);
    CHECK(compareVersions("0.9.0", "0.25.0") == -1);  // numbers, not text
    CHECK(compareVersions("1.0", "0.99.9") == 1);
    CHECK(compareVersions("0.26.0-beta", "0.25.3") == 1);
}

TEST_CASE("Updates: the newest release from GitHub's list", "[updates]")
{
    const std::string list = R"([
        {"tag_name": "v0.27.0", "name": "Not yet", "draft": true, "html_url": "https://example/draft"},
        {"tag_name": "v0.25.0", "name": "Forge3D 0.25 Beta", "draft": false, "html_url": "https://example/0.25"},
        {"tag_name": "v0.26.0", "name": "Forge3D 0.26 Beta", "draft": false, "prerelease": true,
         "html_url": "https://example/0.26"},
        {"tag_name": "nightly", "name": "Ignored", "draft": false}
    ])";
    const auto newer = newerRelease(list, "0.25.0");
    REQUIRE(newer);
    CHECK(newer->version == "0.26.0");
    CHECK(newer->name == "Forge3D 0.26 Beta");
    CHECK(newer->url == "https://example/0.26");
    CHECK_FALSE(newerRelease(list, "0.26.0"));
    CHECK_FALSE(newerRelease("[]", "0.25.0"));
    CHECK_THROWS_AS(newerRelease("{\"message\": \"rate limited\"}", "0.25.0"), std::runtime_error);
    CHECK_THROWS_AS(newerRelease("not json", "0.25.0"), std::runtime_error);
}

TEST_CASE("Updates: Forge3D and Scenery each see only their own releases", "[updates]")
{
    const std::string list = R"([
        {"tag_name": "scenery-v0.2.0", "name": "Forge3D Scenery 0.2 Beta", "draft": false, "html_url": "https://example/s0.2"},
        {"tag_name": "v0.118.0", "name": "Forge3D 0.118 Beta", "draft": false, "html_url": "https://example/0.118"},
        {"tag_name": "scenery-v0.1.0", "name": "", "draft": false, "html_url": "https://example/s0.1"},
        {"tag_name": "V9.0.0", "name": "Wrong case", "draft": false}
    ])";
    const auto forge = newerRelease(list, "0.117.0");
    REQUIRE(forge);
    CHECK(forge->version == "0.118.0");
    CHECK_FALSE(newerRelease(list, "0.118.0"));  // Scenery's 0.2 isn't Forge3D's
    const auto scenery = newerRelease(list, "0.1.0", kSceneryTags, "Forge3D Scenery");
    REQUIRE(scenery);
    CHECK(scenery->version == "0.2.0");
    CHECK(scenery->url == "https://example/s0.2");
    const auto named = newerRelease(list, "0.0.1", kSceneryTags, "Forge3D Scenery");
    CHECK(named->version == "0.2.0");
    CHECK_FALSE(newerRelease(list, "0.2.0", kSceneryTags, "Forge3D Scenery"));
    // A release with no name is called after the app.
    const auto unnamed = newerRelease(R"([{"tag_name": "scenery-v0.1.0", "draft": false}])", "0.0.1", kSceneryTags,
                                      "Forge3D Scenery");
    REQUIRE(unnamed);
    CHECK(unnamed->name == "Forge3D Scenery 0.1.0");
}
