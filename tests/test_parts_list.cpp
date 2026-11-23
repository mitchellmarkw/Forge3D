// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 63: the parts list.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/PartsList.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace forge;
using Catch::Matchers::WithinRel;

namespace {

ObjectId add(CommandStack& stack, Document& doc, PrimitiveSpec spec, const std::string& name, double x = 0)
{
    Body b;
    b.id = doc.allocateId();
    b.spec = spec;
    b.name = name;
    b.placement.x = x;
    stack.execute(std::make_unique<AddBodyCommand>(b));
    return b.id;
}

}  // namespace

TEST_CASE("Parts list: sizes, volumes, weights and costs", "[parts]")
{
    Document doc;
    CommandStack stack(doc);
    const ObjectId box = add(stack, doc, BoxSpec{10, 20, 30}, "Torso");
    const ObjectId cube = add(stack, doc, BoxSpec{10, 10, 10}, "Head, big", 40);
    parts::Settings s;
    s.density = 1.2;
    s.fill = 1.0;
    s.waste = 0.1;
    s.pricePerKg = 30.0;
    auto rows = parts::rows(doc, {box, cube}, s);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].name == "Torso");
    CHECK_THAT(rows[0].size[2], WithinRel(30.0, 1e-6));
    CHECK_THAT(rows[0].volume, WithinRel(6000.0, 1e-6));
    CHECK_THAT(rows[0].grams, WithinRel(6.0 * 1.2 * 1.1, 1e-6));
    CHECK_THAT(rows[0].cost, WithinRel(6.0 * 1.2 * 1.1 * 0.03, 1e-6));
    const parts::Row t = parts::total(rows);
    CHECK_THAT(t.volume, WithinRel(7000.0, 1e-6));

    // Filament at 40 %.
    s.fill = 0.4;
    CHECK_THAT(parts::rows(doc, {box}, s)[0].grams, WithinRel(6.0 * 1.2 * 0.4 * 1.1, 1e-6));

    // Joined: the parts inside listed under it, not counted twice.
    Body merged;
    merged.id = doc.allocateId();
    merged.name = "Kit";
    merged.boolean = BooleanSpec{};
    merged.boolean->op = BooleanOp::Group;
    merged.boolean->operands = {box, cube};
    stack.execute(std::make_unique<CombineCommand>(merged));
    s.inside = true;
    rows = parts::rows(doc, {merged.id}, s);
    REQUIRE(rows.size() == 3);
    CHECK(rows[1].depth == 1);
    // Numbered as pieces to print: the group listed with its parts isn't one.
    CHECK(rows[0].number == 0);
    CHECK(rows[1].number == 1);
    CHECK(rows[2].number == 2);
    CHECK(rows[2].id == cube);
    CHECK_THAT(parts::total(rows).volume, WithinRel(7000.0, 1e-6));

    const std::string table = parts::csv(rows, "$");
    CHECK(table.find("2,\"  Head, big\"") != std::string::npos);  // a comma in a name is quoted
    CHECK(table.find("Total,,,,7.00") != std::string::npos);
    const std::string page = parts::html(rows, "$", "Kit <1>", "PLA");
    CHECK(page.find("Kit &lt;1&gt;") != std::string::npos);

    s.density = 0;
    CHECK_THROWS_AS(parts::rows(doc, {box}, s), std::invalid_argument);
    CHECK(!parts::materials().empty());
}
