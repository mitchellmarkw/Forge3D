// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"

#include <array>
#include <string>
#include <vector>

// Parts list: for every part its size, volume, and the estimated print weight and
// material cost, as a table (for planning, quoting, or a kit's instructions).
namespace forge {

class Document;

namespace parts {

struct Material {
    std::string name;
    double density = 1.24;  // g/cm³ (resin: g/ml, the same)
    double fill = 1.0;      // how much of the inside is printed solid (resin: all of it)
    bool resin = false;
};
// Common filaments and resins.
const std::vector<Material>& materials();

struct Settings {
    double density = 1.24;    // g/cm³
    double fill = 1.0;        // 0..1
    double waste = 0.10;      // extra for supports, brims and waste, 0..
    double pricePerKg = 20.0;
    bool inside = false;      // list the parts inside joined shapes (a kit's parts) too
};

struct Row {
    ObjectId id = kInvalidId;
    // The part's number (1, 2...), the same on an instruction sheet: every row that's a
    // piece to print, not a joined shape listed with its parts after it. 0: none.
    int number = 0;
    std::string name;
    int depth = 0;                      // 0: a shape on the plate; 1: a part inside it
    std::array<double, 3> size{0, 0, 0};  // mm
    double volume = 0.0;                // mm³
    double grams = 0.0;
    double cost = 0.0;
};

// The rows for shapes `ids` (on the plate), in order, with the parts inside each joined
// shape (Merge or Group) after it if asked. Throws std::invalid_argument for bad settings.
std::vector<Row> rows(const Document& doc, const std::vector<ObjectId>& ids, const Settings& s);
// The totals of the shapes on the plate (the parts inside are already in them).
Row total(const std::vector<Row>& rows);

// The table as comma-separated values (a spreadsheet opens it), and as a web page
// (printed or saved as PDF). `currency`: shown before costs ("$", "£"...).
std::string csv(const std::vector<Row>& rows, const std::string& currency);
std::string html(const std::vector<Row>& rows, const std::string& currency, const std::string& title,
                 const std::string& note);

}  // namespace parts
}  // namespace forge
