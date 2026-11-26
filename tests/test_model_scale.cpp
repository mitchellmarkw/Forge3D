// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 110: model scales (Forge3D Scenery).
#include "forge/core/ModelScale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinAbs;

TEST_CASE("Model scales: reading, writing and working out sizes", "[scale]")
{
    const modelscale::Scale s{35};
    // A standard brick, 194 x 92 x 57 mm, at 1:35.
    CHECK_THAT(s.toModel(194.0), WithinAbs(5.543, 0.001));
    CHECK_THAT(s.toModel(57.0), WithinAbs(1.629, 0.001));
    CHECK_THAT(s.toReal(s.toModel(92.0)), WithinAbs(92.0, 1e-9));
    for (const char* text : {"1:35", "1/35", "35", " 1 : 35 ", "1:35 (whatever)"}) {
        const auto p = modelscale::parse(text);
        REQUIRE(p);
        CHECK(*p == s);
    }
    CHECK(modelscale::parse("1:87.1")->ratio == 87.1);
    CHECK(modelscale::format({87.1}) == "1:87.1");
    CHECK(modelscale::format({35}) == "1:35");
    for (const char* bad : {"", "2:35", "1:0.5", "1:5000", "abc", "1:"})
        CHECK_FALSE(modelscale::parse(bad));
    // The usual ones read back as themselves.
    for (const auto& n : modelscale::common())
        CHECK(*modelscale::parse(n.name) == n.scale);
}
