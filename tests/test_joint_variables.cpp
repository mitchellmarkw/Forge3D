// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 81: joint sizes that follow the design variables
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Port.h"
#include "forge/core/Socket.h"
#include "forge/core/Variables.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

struct Bench {
    Document doc;
    CommandStack stack{doc};
    ObjectId box(double z)
    {
        Body b;
        b.id = doc.allocateId();
        b.spec = BoxSpec{20, 20, 20};
        b.name = doc.uniqueName("Box");
        b.placement.z = z;
        stack.execute(std::make_unique<AddBodyCommand>(b));
        return b.id;
    }
    ObjectId step(BooleanSpec spec, std::map<std::string, std::string> formulas, const std::string& name)
    {
        Body r;
        r.id = doc.allocateId();
        r.name = doc.uniqueName(name);
        r.boolean = std::move(spec);
        r.formulas = std::move(formulas);
        stack.execute(std::make_unique<CombineCommand>(r));
        return r.id;
    }
    void variables(std::vector<Variable> table)
    {
        stack.execute(std::make_unique<SetVariablesCommand>(std::move(table), std::map<std::string, std::string>{},
                                                            "Variables"));
    }
};

const emboss::SurfacePoint kTop{{0, 0, 20}, {0, 0, 1}};

}  // namespace

TEST_CASE("Joint variables: a socket's size follows PegDia", "[jointvars]")
{
    Bench b;
    b.variables({{"PegDia", "3", ""}, {"Fit", "0.1", ""}});
    const ObjectId box = b.box(0);
    if (const char* folder = std::getenv("FORGE_DUMP"))
        file::save(b.doc, std::string(folder) + "/jointvars.f3d", "test");
    SocketFinish f;
    f.kind = SocketKind::Peg;
    f.size = 3;
    f.depth = 4;
    const ObjectId sock = b.step(socketSpec(b.doc, box, kTop, f),
                                 {{"socket.size", "PegDia"}, {"socket.fit", "Fit"}, {"socket.depth", "PegDia + 1"}},
                                 "Socketed");
    const double before = meshkernel::volume(b.doc.meshSolid(sock));
    CHECK(variables::hasField(*b.doc.find(sock), "socket.size"));
    CHECK_FALSE(variables::hasField(*b.doc.find(sock), "hinge.width"));
    CHECK(variables::fieldLabel(*b.doc.find(sock), "socket.size") == "Socket size");

    b.variables({{"PegDia", "4", ""}, {"Fit", "0.15", ""}});
    const SocketFinish& now = *b.doc.find(sock)->boolean->socket;
    CHECK(now.size == 4.0);
    CHECK(now.depth == 5.0);
    CHECK(now.fit == 0.15);
    CHECK(meshkernel::volume(b.doc.meshSolid(sock)) < before - 20.0);  // a bigger hole
    b.stack.undo();
    CHECK(b.doc.find(sock)->boolean->socket->size == 3.0);

    // A size the socket can't have is refused, naming it.
    try {
        b.variables({{"PegDia", "-1", ""}, {"Fit", "0.1", ""}});
        FAIL("refused");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).find("Socketed") != std::string::npos);
    }
    CHECK(b.doc.find(sock)->boolean->socket->size == 3.0);
}

TEST_CASE("Joint variables: a double hinge's second fork moves with the gap", "[jointvars]")
{
    Bench b;
    b.variables({{"Gap", "1", ""}, {"Pin", "2", ""}});
    const ObjectId lower = b.box(0);
    const ObjectId upper = b.box(21);
    HingeFinish f;
    f.kind = HingeKind::Double;
    f.gap = 1.0;
    f.link = 5;
    const std::map<std::string, std::string> formulas{{"hinge.gap", "Gap"}, {"hinge.pinSize", "Pin"}};
    const ObjectId fork = b.step(hingeSpec(b.doc, lower, kTop, f), formulas, "Hinged");
    const hinge::Half other = hinge::otherHalf(f, kTop.point, kTop.normal);
    const ObjectId fork2 = b.step(hingeSpec(b.doc, upper, {other.point, other.normal}, other.finish), formulas, "Hinged");
    REQUIRE(hingePartner(b.doc, fork) == fork2);
    CHECK_THAT(hingeSpot(b.doc, fork2).point[2], WithinAbs(21.0, 1e-6));

    b.variables({{"Gap", "1.5", ""}, {"Pin", "1.75", ""}});
    CHECK(b.doc.find(fork)->boolean->hinge->gap == 1.5);
    CHECK(b.doc.find(fork2)->boolean->hinge->gap == 1.5);
    CHECK(b.doc.find(fork2)->boolean->hinge->pinSize == 1.75);
    CHECK(b.doc.find(fork2)->boolean->hinge->half == HingeHalf::Fork);
    CHECK(b.doc.find(fork2)->boolean->hinge->link == 5);
    // The second fork's face is the new gap out from the first.
    CHECK_THAT(hingeSpot(b.doc, fork2).point[2], WithinAbs(21.5, 1e-6));
    CHECK(b.doc.find(fork2)->formulas == formulas);
    b.stack.undo();
    CHECK_THAT(hingeSpot(b.doc, fork2).point[2], WithinAbs(21.0, 1e-6));
}

TEST_CASE("Joint variables: a hand port's peg", "[jointvars]")
{
    Bench b;
    b.variables({{"PegDia", "3", ""}});
    const ObjectId box = b.box(0);
    PortFinish f;
    f.size = 3;
    f.length = 4;
    const ObjectId port = b.step(portSpec(b.doc, box, kTop, f), {{"port.size", "PegDia"}, {"port.length", "PegDia * 2"}},
                                 "Port");
    b.variables({{"PegDia", "2.5", ""}});
    CHECK(b.doc.find(port)->boolean->port->size == 2.5);
    CHECK(b.doc.find(port)->boolean->port->length == 5.0);
    CHECK(variables::fieldLabel(*b.doc.find(port), "port.length") == "Peg length");
}

TEST_CASE("Hinge pieces: the tab and pin are remade when the hinge's sizes change", "[jointvars]")
{
    Bench b;
    b.variables({{"Tab", "3", ""}, {"Pin", "2", ""}});
    const ObjectId box = b.box(0);
    HingeFinish f;
    f.kind = HingeKind::Single;
    f.width = 3;
    f.pinSize = 2;
    const ObjectId step =
        b.step(hingeSpec(b.doc, box, kTop, f), {{"hinge.width", "Tab"}, {"hinge.pinSize", "Pin"}}, "Hinged");
    // The pieces, as Quick Hinge makes them.
    const std::array<double, 2> span{8.0, 8.0};
    const auto pieces = hinge::pieces(f, kTop.point, kTop.normal, true, span);
    REQUIRE(pieces.size() == 2);  // the tab and the pin
    std::vector<ObjectId> ids;
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        Body piece = hinge::pieceBody(pieces[i].mesh, pieces[i].name);
        piece.id = b.doc.allocateId();
        piece.hardware = true;
        piece.jointPiece = JointPiece{step, int(i), true, span,
                                      {piece.placement.x, piece.placement.y, piece.placement.z}, pieces[i].name};
        if (i == 1)
            piece.name += " 2";  // as when there's one of that name already
        b.stack.execute(std::make_unique<AddBodyCommand>(piece));
        ids.push_back(piece.id);
    }
    // The pin is laid out to print somewhere else.
    {
        Body moved = *b.doc.find(ids[1]);
        moved.placement.x += 40.0;
        b.stack.execute(std::make_unique<EditBodyCommand>(*b.doc.find(ids[1]), moved, "Move"));
    }
    const auto sizeOf = [&](ObjectId id) { return b.doc.worldBounds(id).size(); };
    const auto tabBefore = sizeOf(ids[0]);
    const double pinX = b.doc.find(ids[1])->placement.x;
    const double tabZ = b.doc.find(ids[0])->placement.z;

    b.variables({{"Tab", "4", ""}, {"Pin", "2.5", ""}});
    CHECK(b.doc.find(step)->boolean->hinge->width == 4.0);
    // The tab: thicker along the pin, left in place (so it moved with the joint).
    const auto tabAfter = sizeOf(ids[0]);
    CHECK(std::max({tabAfter[0], tabAfter[1]}) > std::max({tabBefore[0], tabBefore[1]}) - 1e-6);
    CHECK(std::abs((tabAfter[0] + tabAfter[1]) - (tabBefore[0] + tabBefore[1])) > 0.5);
    CHECK_THAT(b.doc.find(ids[0])->placement.z, WithinAbs(tabZ, 1.0));
    CHECK(b.doc.find(ids[0])->name.rfind("Hinge tab 4", 0) == 0);
    // The pin: fatter, but still where it was put, its " 2" kept.
    const auto pinAfter = sizeOf(ids[1]);
    CHECK_THAT(std::min({pinAfter[0], pinAfter[1], pinAfter[2]}), WithinAbs(2.5, 0.05));
    CHECK(b.doc.find(ids[1])->placement.x == pinX);
    CHECK(b.doc.find(ids[1])->name.find(" 2") != std::string::npos);
    CHECK(b.doc.find(ids[1])->name.find("2.5") != std::string::npos);
    // Saved and read back, it still knows its hinge.
    {
        const auto path = std::filesystem::temp_directory_path() / "forge3d-jointpiece.f3d";
        file::save(b.doc, path, "test");
        const auto loaded = file::load(path);
        std::filesystem::remove(path);
        const auto it = std::find_if(loaded.begin(), loaded.end(), [&](const Body& x) { return x.id == ids[1]; });
        REQUIRE(it != loaded.end());
        REQUIRE(it->jointPiece);
        CHECK(*it->jointPiece == *b.doc.find(ids[1])->jointPiece);
    }
    // One undo step puts all of it back.
    b.stack.undo();
    CHECK(b.doc.find(step)->boolean->hinge->width == 3.0);
    CHECK_THAT(std::min({sizeOf(ids[1])[0], sizeOf(ids[1])[1], sizeOf(ids[1])[2]}), WithinAbs(2.0, 0.05));
}
