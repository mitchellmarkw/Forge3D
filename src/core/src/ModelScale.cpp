// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/ModelScale.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace forge::modelscale {

const std::vector<Named>& common()
{
    static const std::vector<Named> list = {
        {{6}, "1:6 (playscale)"}, {{12}, "1:12"},      {{18}, "1:18"},       {{24}, "1:24"},
        {{32}, "1:32"},           {{35}, "1:35"},      {{43}, "1:43"},       {{48}, "1:48 (O)"},
        {{56}, "1:56 (28 mm)"},   {{64}, "1:64 (S)"},  {{72}, "1:72"},       {{87}, "1:87 (HO)"},
        {{100}, "1:100"},         {{160}, "1:160 (N)"}, {{220}, "1:220 (Z)"},
    };
    return list;
}

std::optional<Scale> parse(const std::string& text)
{
    std::string t;
    for (char c : text)
        if (!std::isspace(static_cast<unsigned char>(c)))
            t += c;
    // "(HO)" and the like after the number
    if (const auto paren = t.find('('); paren != std::string::npos)
        t = t.substr(0, paren);
    if (const auto sep = t.find_first_of(":/"); sep != std::string::npos) {
        if (t.substr(0, sep) != "1")
            return std::nullopt;
        t = t.substr(sep + 1);
    }
    if (t.empty())
        return std::nullopt;
    char* end = nullptr;
    const double r = std::strtod(t.c_str(), &end);
    if (end != t.c_str() + t.size() || !std::isfinite(r) || r < kLargest || r > kSmallest)
        return std::nullopt;
    return Scale{r};
}

std::string format(const Scale& s)
{
    char buf[32];
    if (std::abs(s.ratio - std::round(s.ratio)) < 1e-6)
        std::snprintf(buf, sizeof buf, "1:%.0f", s.ratio);
    else
        std::snprintf(buf, sizeof buf, "1:%g", s.ratio);
    return buf;
}

}  // namespace forge::modelscale
