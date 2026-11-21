// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Tolerance.h"

#include <optional>
#include <string>
#include <vector>

// Joint standards: a figure line's joint sizes, kept by name ("5-inch line": 6 mm balls,
// 3 mm pegs, 4 mm hand pegs...), so every figure in the line takes the same joints. Each
// standard is a list of named joints, each a Quick Socket or a Quick Hinge with its sizes.
// The fit isn't part of it: that belongs to the printer (the tolerance test's).
namespace forge::standards {

struct Joint {
    std::string name;                  // "Shoulder ball", "Wrist peg"...
    std::optional<SocketFinish> socket;  // one of these two
    std::optional<HingeFinish> hinge;
    bool operator==(const Joint&) const = default;
};

struct Standard {
    std::string name;
    std::string notes;
    std::vector<Joint> joints;
    bool operator==(const Standard&) const = default;
};

// Throws std::invalid_argument (in plain words) if it can't be used: no name, a joint
// with no name, two joints of one name, sizes a socket or hinge can't have.
void check(const Standard& s);
void check(const Joint& j);

// "Ball socket Ø6 (snap-in)", "Single hinge, tab 3 × Ø5, pin Ø2"...
std::string describe(const Joint& j);

// The joint's sizes on a socket or hinge being made: everything but where it is, which
// way it turns, the printer and its fit, and which half it is.
SocketFinish applied(const SocketFinish& joint, const SocketFinish& now);
HingeFinish applied(const HingeFinish& joint, const HingeFinish& now);
// The sizes to keep from a socket or hinge (the rest set back to the usual).
Joint jointFrom(const std::string& name, const SocketFinish& f);
Joint jointFrom(const std::string& name, const HingeFinish& f);

// The tolerance test's sizes from the standard: its first ball, first peg and first hinge
// pin (whichever it has); the rest as they are.
tolerance::Settings toleranceSizes(const Standard& s, tolerance::Settings now);

// The standards as text (a library, or one to share), and back. Reading throws
// std::invalid_argument (in plain words) if it isn't one or is damaged.
std::string toJson(const std::vector<Standard>& standards);
std::vector<Standard> fromJson(const std::string& json);

// A name not used by any of these: `base`, or "base 2", "base 3"...
std::string uniqueName(const std::vector<Standard>& standards, const std::string& base);

}  // namespace forge::standards
