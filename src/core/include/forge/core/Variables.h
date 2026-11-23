// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

// The design variables table (slice 77): named values (PegDia = 3) that sizes and
// positions can follow (a shape's width = PegDia + 0.4), so changing one changes every
// shape that uses it. A value can be a formula of others (PegHole = PegDia + Fit).
namespace forge {

struct Variable {
    std::string name;
    std::string formula;  // "3", "PegDia + Fit", "max(2, Wall * 2)"
    std::string note;     // what it's for (shown in the table)
    bool operator==(const Variable&) const = default;
};

namespace variables {

// Letters, digits and _, starting with a letter; not a function's name (or pi).
bool validName(const std::string& name);
// Throws std::invalid_argument (in plain words) if the name can't be used.
void checkName(const std::string& name);

// The value of a formula: numbers, + - * / ^, brackets, names (looked up; nullopt: not
// known), pi, and min(a, b, ...), max, abs, sqrt, round, floor, ceil. A leading "="
// is allowed. Throws std::invalid_argument in plain words.
double evaluate(const std::string& formula, const std::function<std::optional<double>(const std::string&)>& lookup);
// The names a formula uses (each once, in order).
std::vector<std::string> namesIn(const std::string& formula);
// The formula with one name changed to another (whole names only).
std::string renamed(const std::string& formula, const std::string& from, const std::string& to);
// " = " dropped from the front, spaces trimmed.
std::string cleaned(const std::string& formula);

struct Values {
    std::map<std::string, double> value;        // the ones that work out
    std::map<std::string, std::string> problem;  // the ones that don't, and why
};
// Every variable's value (formulas may use each other, in any order; a loop is a problem).
Values compute(const std::vector<Variable>& table);
// Throws std::invalid_argument if two have the same name or a name can't be used.
void checkTable(const std::vector<Variable>& table);

// ---- Fields a formula can drive ----
// A primitive's lengths by their file key ("width", "radiusX"...), the placement as
// "at.x", "at.y", "at.z", "at.rotationX", "at.rotationY", "at.rotationZ", and a joint
// step's sizes: "socket.size", "socket.depth", "socket.fit", "socket.cover",
// "hinge.width", "hinge.barrel", "hinge.pinSize", "hinge.fit", "hinge.gap", "hinge.lip",
// "port.size", "port.length", "port.fit", "port.ball".
bool hasField(const Body& body, const std::string& key);
double fieldValue(const Body& body, const std::string& key);
void setFieldValue(Body& body, const std::string& key, double value);
// "Width", "X position"...
std::string fieldLabel(const Body& body, const std::string& key);

// The body with each of its formulas worked out from `values` and put in. A formula that
// doesn't work out leaves its field as it is (and is listed in `problems`, if given).
Body applied(const Body& body, const Values& values, std::vector<std::string>* problems = nullptr);

}  // namespace variables
}  // namespace forge
