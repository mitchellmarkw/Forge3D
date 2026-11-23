// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 77: the design variables table
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Variables.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <random>
#include <stdexcept>

using namespace forge;
using Catch::Matchers::WithinAbs;
namespace fs = std::filesystem;

namespace {

double eval(const std::string& f)
{
    return variables::evaluate(f, [](const std::string& n) -> std::optional<double> {
        if (n == "PegDia")
            return 3.0;
        if (n == "Fit")
            return 0.1;
        return std::nullopt;
    });
}

struct Scene {
    Document doc;
    CommandStack stack{doc};
    ObjectId add(const PrimitiveSpec& spec, const std::string& name, std::map<std::string, std::string> formulas = {})
    {
        Body b;
        b.id = doc.allocateId();
        b.spec = spec;
        b.name = name;
        b.formulas = std::move(formulas);
        stack.execute(std::make_unique<AddBodyCommand>(b));
        return b.id;
    }
};

}  // namespace

TEST_CASE("Variables: formulas work out", "[variables]")
{
    CHECK(eval("3") == 3.0);
    CHECK(eval("= 2 + 3 * 4") == 14.0);
    CHECK(eval("(2 + 3) * 4") == 20.0);
    CHECK(eval("2 ^ 3 ^ 2") == 512.0);
    CHECK(eval("-2 ^ 2") == 4.0);  // the minus belongs to the 2
    CHECK_THAT(eval("PegDia + 2 * Fit"), WithinAbs(3.2, 1e-12));
    CHECK(eval("max(1, PegDia, 2)") == 3.0);
    CHECK(eval("min(4, 5)") == 4.0);
    CHECK(eval("round(2.5) + floor(1.9) + ceil(1.1) + abs(-1) + sqrt(16)") == 3 + 1 + 2 + 1 + 4);
    CHECK_THAT(eval("2 * pi"), WithinAbs(6.283185307, 1e-8));
    CHECK(eval("1e1 + .5") == 10.5);
    CHECK_THROWS_AS(eval("PegDiameter"), std::invalid_argument);
    CHECK_THROWS_AS(eval("1 / 0"), std::invalid_argument);
    CHECK_THROWS_AS(eval("(1 + 2"), std::invalid_argument);
    CHECK_THROWS_AS(eval("2 +"), std::invalid_argument);
    CHECK_THROWS_AS(eval("3 mm"), std::invalid_argument);
    CHECK_THROWS_AS(eval(""), std::invalid_argument);
    CHECK_THROWS_AS(eval("sqrt(-1)"), std::invalid_argument);
    CHECK_THROWS_AS(eval("wobble(1)"), std::invalid_argument);
}

TEST_CASE("Variables: names, renaming, and a table", "[variables]")
{
    CHECK(variables::validName("PegDia"));
    CHECK(variables::validName("Wall_2"));
    CHECK_FALSE(variables::validName("2Wall"));
    CHECK_FALSE(variables::validName("Peg Dia"));
    CHECK_FALSE(variables::validName("max"));
    CHECK_FALSE(variables::validName("Pi"));
    CHECK(variables::namesIn("PegDia + Fit * 2 + max(PegDia, 1e3)") == std::vector<std::string>{"PegDia", "Fit"});
    CHECK(variables::renamed("PegDia+PegDia2*PegDia", "PegDia", "Peg") == "Peg+PegDia2*Peg");
    CHECK(variables::cleaned("  = PegDia + 1 ") == "PegDia + 1");

    // In any order; one that uses itself, or a broken one, is a problem without
    // spoiling the others.
    const auto v = variables::compute({{"Hole", "PegDia + 2 * Fit", ""},
                                       {"PegDia", "3", ""},
                                       {"Fit", "0.1", ""},
                                       {"A", "B + 1", ""},
                                       {"B", "A", ""},
                                       {"Bad", "PegDia +", ""}});
    CHECK_THAT(v.value.at("Hole"), WithinAbs(3.2, 1e-12));
    CHECK(v.value.at("PegDia") == 3.0);
    CHECK(v.problem.count("A"));
    CHECK(v.problem.count("B"));
    CHECK(v.problem.count("Bad"));
    CHECK_THROWS_AS(variables::checkTable({{"Peg", "1", ""}, {"peg", "2", ""}}), std::invalid_argument);
    CHECK_THROWS_AS(variables::checkTable({{"1x", "1", ""}}), std::invalid_argument);
}

TEST_CASE("Variables: shapes follow them, in one undo step", "[variables]")
{
    Scene s;
    const ObjectId peg = s.add(CylinderSpec{1.5, 4.0}, "Peg", {{"radiusX", "PegDia / 2"}, {"radiusY", "PegDia / 2"}});
    const ObjectId box = s.add(BoxSpec{10, 10, 10}, "Block", {{"width", "PegDia * 4"}, {"at.x", "Gap + PegDia"}});
    const ObjectId plain = s.add(BoxSpec{5, 5, 5}, "Plain");
    s.stack.execute(std::make_unique<SetVariablesCommand>(
        std::vector<Variable>{{"PegDia", "3", ""}, {"Gap", "10", ""}}, std::map<std::string, std::string>{}, "Variables"));
    CHECK(s.doc.variables().size() == 2);
    CHECK(std::get<CylinderSpec>(s.doc.find(peg)->spec).radiusX == 1.5);
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).width == 12.0);
    CHECK(s.doc.find(box)->placement.x == 13.0);

    s.stack.execute(std::make_unique<SetVariablesCommand>(
        std::vector<Variable>{{"PegDia", "4", ""}, {"Gap", "10", ""}}, std::map<std::string, std::string>{}, "PegDia"));
    CHECK(std::get<CylinderSpec>(s.doc.find(peg)->spec).radiusX == 2.0);
    CHECK(std::get<CylinderSpec>(s.doc.find(peg)->spec).radiusY == 2.0);
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).width == 16.0);
    CHECK(s.doc.find(box)->placement.x == 14.0);
    CHECK(std::get<BoxSpec>(s.doc.find(plain)->spec).width == 5.0);

    s.stack.undo();
    CHECK(s.doc.variables()[0].formula == "3");
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).width == 12.0);
    s.stack.redo();
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).width == 16.0);

    // Renamed: the shapes' formulas go with it.
    s.stack.execute(std::make_unique<SetVariablesCommand>(
        std::vector<Variable>{{"Peg", "4", ""}, {"Gap", "10", ""}},
        std::map<std::string, std::string>{{"PegDia", "Peg"}}, "Rename"));
    CHECK(s.doc.find(box)->formulas.at("width") == "Peg * 4");
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).width == 16.0);

    // Deleted: the sizes stay, and no longer follow it.
    s.stack.execute(std::make_unique<SetVariablesCommand>(std::vector<Variable>{{"Peg", "4", ""}},
                                                          std::map<std::string, std::string>{}, "Delete Gap"));
    CHECK(s.doc.find(box)->formulas.count("at.x") == 0);
    CHECK(s.doc.find(box)->formulas.count("width") == 1);
    CHECK(s.doc.find(box)->placement.x == 14.0);

    // A value that makes a shape impossible is refused, and nothing changes.
    CHECK_THROWS_AS(s.stack.execute(std::make_unique<SetVariablesCommand>(
                        std::vector<Variable>{{"Peg", "-1", ""}}, std::map<std::string, std::string>{}, "Bad")),
                    std::invalid_argument);
    CHECK(s.doc.variables()[0].formula == "4");
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).width == 16.0);
}

TEST_CASE("Variables: a field takes a formula with EditBodyCommand", "[variables]")
{
    Scene s;
    s.stack.execute(std::make_unique<SetVariablesCommand>(std::vector<Variable>{{"Wall", "2", ""}},
                                                          std::map<std::string, std::string>{}, "Variables"));
    const ObjectId box = s.add(BoxSpec{10, 10, 10}, "Box");
    Body after = *s.doc.find(box);
    after.formulas["height"] = "Wall * 3";
    after = variables::applied(after, variables::compute(s.doc.variables()));
    CHECK(std::get<BoxSpec>(after.spec).height == 6.0);
    s.stack.execute(std::make_unique<EditBodyCommand>(*s.doc.find(box), after, "Height = Wall * 3"));
    CHECK(s.doc.find(box)->formulas.at("height") == "Wall * 3");
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).height == 6.0);
    s.stack.undo();
    CHECK(s.doc.find(box)->formulas.empty());
    CHECK(std::get<BoxSpec>(s.doc.find(box)->spec).height == 10.0);
    CHECK(variables::fieldLabel(*s.doc.find(box), "at.z") == "Z position");
    CHECK_FALSE(variables::hasField(*s.doc.find(box), "radiusX"));
}

TEST_CASE("Variables: saved and read back", "[variables]")
{
    std::random_device rd;
    const fs::path dir = fs::temp_directory_path() / ("forge3d-test-" + std::to_string(rd()));
    fs::create_directories(dir);
    const fs::path path = dir / "v.f3d";
    {
        Scene s;
        s.add(BoxSpec{12, 10, 10}, "Box", {{"width", "PegDia * 4"}});
        s.doc.setVariables({{"PegDia", "3", "the peg on every joint"}, {"Fit", "0.1", ""}});
        file::save(s.doc, path, "test");
    }
    const auto table = file::loadVariables(path);
    REQUIRE(table.size() == 2);
    CHECK(table[0].name == "PegDia");
    CHECK(table[0].note == "the peg on every joint");
    CHECK(table[1].formula == "0.1");
    const auto bodies = file::load(path);
    REQUIRE(bodies.size() == 1);
    CHECK(bodies[0].formulas.at("width") == "PegDia * 4");
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("Variables: a size set by hand stops following", "[variables]")
{
    Body before;
    before.spec = BoxSpec{12, 10, 10};
    before.placement.x = 13;
    before.formulas = {{"width", "PegDia * 4"}, {"at.x", "Gap + PegDia"}, {"height", "Wall"}};
    Body after = before;
    std::get<BoxSpec>(after.spec).width = 15;  // dragged with a handle
    after.placement.z = 4;                     // sat on the plate (z has no formula)
    const auto dropped = variables::dropChanged(before, after);
    REQUIRE(dropped.size() == 1);
    CHECK(dropped[0] == "Width (X)");
    CHECK(after.formulas.count("width") == 0);
    CHECK(after.formulas.count("at.x") == 1);
    CHECK(after.formulas.count("height") == 1);
    Body same = before;
    CHECK(variables::dropChanged(before, same).empty());
}
