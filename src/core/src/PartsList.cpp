// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/PartsList.h"

#include "forge/core/Document.h"
#include "forge/core/MeshKernel.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace forge::parts {

namespace {

std::string fixed(double v, int places)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", places, v);
    return buf;
}

std::string escapeCsv(const std::string& s)
{
    if (s.find_first_of(",\"\n") == std::string::npos)
        return s;
    std::string out = "\"";
    for (char c : s)
        out += c == '"' ? std::string("\"\"") : std::string(1, c);
    return out + "\"";
}

std::string escapeHtml(const std::string& s)
{
    std::string out;
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c;
        }
    }
    return out;
}

std::string sizeText(const std::array<double, 3>& s)
{
    return fixed(s[0], 1) + " \xC3\x97 " + fixed(s[1], 1) + " \xC3\x97 " + fixed(s[2], 1);
}

Row rowFor(const Document& doc, ObjectId id, int depth, const Settings& s)
{
    const Body& b = *doc.find(id);
    Row r;
    r.id = id;
    r.name = b.name;
    r.depth = depth;
    const Bounds box = doc.worldBounds(id);
    if (box.valid)
        r.size = {double(box.max[0] - box.min[0]), double(box.max[1] - box.min[1]), double(box.max[2] - box.min[2])};
    r.volume = std::abs(meshkernel::volume(doc.meshSolid(id)));
    r.grams = r.volume / 1000.0 * s.density * s.fill * (1.0 + s.waste);
    r.cost = r.grams / 1000.0 * s.pricePerKg;
    return r;
}

}  // namespace

const std::vector<Material>& materials()
{
    static const std::vector<Material> list{
        {"PLA", 1.24, 0.40, false},           {"PETG", 1.27, 0.40, false},
        {"ABS", 1.04, 0.40, false},           {"ASA", 1.07, 0.40, false},
        {"TPU", 1.21, 0.40, false},           {"Standard resin", 1.12, 1.0, true},
        {"ABS-like resin", 1.15, 1.0, true},  {"Tough resin", 1.18, 1.0, true},
    };
    return list;
}

std::vector<Row> rows(const Document& doc, const std::vector<ObjectId>& ids, const Settings& s)
{
    if (!(s.density > 0.1 && s.density < 25.0))
        throw std::invalid_argument("The material's density must be from 0.1 to 25 g/cm\xC2\xB3.");
    if (!(s.fill > 0.0 && s.fill <= 1.0))
        throw std::invalid_argument("The fill must be more than 0 and at most 100 %.");
    if (!(s.waste >= 0.0 && s.waste <= 5.0) || !(s.pricePerKg >= 0.0))
        throw std::invalid_argument("The waste and price can't be less than nothing.");
    std::vector<Row> out;
    for (ObjectId id : ids) {
        const Body* b = doc.find(id);
        if (!b)
            continue;
        out.push_back(rowFor(doc, id, 0, s));
        if (s.inside && b->isBoolean() &&
            (b->boolean->op == BooleanOp::Merge || b->boolean->op == BooleanOp::Group))
            for (ObjectId part : b->boolean->operands)
                if (doc.find(part))
                    out.push_back(rowFor(doc, part, 1, s));
    }
    int next = 1;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const bool hasParts = out[i].depth == 0 && i + 1 < out.size() && out[i + 1].depth > 0;
        out[i].number = hasParts ? 0 : next++;
    }
    return out;
}

Row total(const std::vector<Row>& rows)
{
    Row t;
    t.name = "Total";
    for (const Row& r : rows)
        if (r.depth == 0) {
            t.volume += r.volume;
            t.grams += r.grams;
            t.cost += r.cost;
        }
    return t;
}

std::string csv(const std::vector<Row>& rows, const std::string& currency)
{
    std::string out = "#,Part,Width (mm),Depth (mm),Height (mm),Volume (cm3),Weight (g),Cost (" + currency + ")\n";
    const auto line = [&](const Row& r, bool sized) {
        out += (r.number > 0 ? std::to_string(r.number) : std::string()) + ",";
        out += escapeCsv((r.depth > 0 ? "  " : "") + r.name) + ",";
        out += sized ? fixed(r.size[0], 1) + "," + fixed(r.size[1], 1) + "," + fixed(r.size[2], 1) + "," : ",,,";
        out += fixed(r.volume / 1000.0, 2) + "," + fixed(r.grams, 1) + "," + fixed(r.cost, 2) + "\n";
    };
    for (const Row& r : rows)
        line(r, true);
    line(total(rows), false);
    return out;
}

std::string html(const std::vector<Row>& rows, const std::string& currency, const std::string& title,
                 const std::string& note)
{
    std::string out = "<html><head><meta charset=\"utf-8\"><title>" + escapeHtml(title) +
                      "</title></head><body style=\"font-family: sans-serif;\">";
    out += "<h2>" + escapeHtml(title) + "</h2>";
    if (!note.empty())
        out += "<p>" + escapeHtml(note) + "</p>";
    out += "<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\" style=\"border-collapse: collapse;\">"
           "<tr><th>#</th><th align=\"left\">Part</th><th>Size (mm)</th><th>Volume (cm\xC2\xB3)</th><th>Weight (g)</th>"
           "<th>Cost</th></tr>";
    for (const Row& r : rows) {
        out += "<tr><td align=\"right\">" + (r.number > 0 ? std::to_string(r.number) : std::string()) + "</td><td>" +
               std::string(r.depth > 0 ? "&nbsp;&nbsp;&nbsp;&nbsp;" : "") + escapeHtml(r.name) +
               "</td><td align=\"center\">" + sizeText(r.size) + "</td><td align=\"right\">" +
               fixed(r.volume / 1000.0, 2) + "</td><td align=\"right\">" + fixed(r.grams, 1) +
               "</td><td align=\"right\">" + escapeHtml(currency) + fixed(r.cost, 2) + "</td></tr>";
    }
    const Row t = total(rows);
    out += "<tr><td></td><td><b>Total</b></td><td></td><td align=\"right\"><b>" + fixed(t.volume / 1000.0, 2) +
           "</b></td><td align=\"right\"><b>" + fixed(t.grams, 1) + "</b></td><td align=\"right\"><b>" +
           escapeHtml(currency) + fixed(t.cost, 2) + "</b></td></tr></table></body></html>";
    return out;
}

}  // namespace forge::parts
