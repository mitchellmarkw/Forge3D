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

#include <cstdlib>
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
