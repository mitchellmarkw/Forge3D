// SPDX-License-Identifier: GPL-3.0-or-later
// Random runs of commands, undo and redo: every undo must give back exactly the
// document before the step, and every redo exactly the one after it.
#include "forge/core/Body.h"
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/PrintSplit.h"
#include "forge/core/Split.h"

#include <catch2/catch_test_macros.hpp>

#include <random>

using namespace forge;

namespace {

// The document as saved, which covers everything a step can change.
std::string state(const Document& doc)
{
    return file::toJson(doc, "test");
}

std::vector<ObjectId> topLevel(const Document& doc)
{
    std::vector<ObjectId> out;
    for (const Body& b : doc.bodies())
        if (doc.isTopLevel(b.id))
            out.push_back(b.id);
    return out;
}

}  // namespace

TEST_CASE("random steps undo and redo exactly", "[command][random]")
{
    for (unsigned seed : {1u, 2u, 3u}) {
        std::mt19937 rng(seed);
        const auto pick = [&](int n) { return int(std::uniform_int_distribution<int>(0, n - 1)(rng)); };
        const auto number = [&](double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng); };
        Document doc;
        CommandStack stack(doc);
        std::vector<std::string> history{state(doc)};  // after each step on the stack
        std::size_t at = 0;                             // where we are in it
        for (int step = 0; step < 40; ++step) {
            const auto tops = topLevel(doc);
            const int what = pick(9);
            bool did = false;
            try {
                if (what == 0 || tops.empty()) {
                    Body b;
                    b.id = doc.allocateId();
                    b.name = doc.uniqueName("Box");
                    b.spec = pick(2) ? PrimitiveSpec{BoxSpec{number(5, 40), number(5, 40), number(5, 40)}}
                                     : PrimitiveSpec{CylinderSpec{number(3, 15), number(5, 40)}};
                    b.placement.x = number(-60, 60);
                    b.placement.y = number(-60, 60);
                    stack.execute(std::make_unique<AddBodyCommand>(b));
                    did = true;
                } else if (what == 1) {
                    Placement p = doc.find(tops[std::size_t(pick(int(tops.size())))])->placement;
                    p.x += number(-20, 20);
                    p.rotationZ += number(-45, 45);
                    stack.execute(std::make_unique<SetPlacementCommand>(tops[0], p, false));
                    did = true;
                } else if (what == 2 && tops.size() >= 2) {
                    Body r;
                    r.id = doc.allocateId();
                    r.name = doc.uniqueName("Combined");
                    BooleanSpec spec;
                    spec.op = pick(2) ? BooleanOp::Merge : BooleanOp::Subtract;
                    spec.operands = {tops[0], tops[1]};
                    if (!doc.previewResult(spec).empty) {
                        r.boolean = spec;
                        stack.execute(std::make_unique<CombineCommand>(r));
                        did = true;
                    }
                } else if (what == 3) {
                    const ObjectId id = tops[std::size_t(pick(int(tops.size())))];
                    const auto c = doc.worldBounds(id).center();
                    const int axis = pick(3);
                    Vec3 n{0, 0, 0};
                    n[std::size_t(axis)] = 1;
                    KeyOptions keys;
                    keys.loosePins = pick(3) == 0;
                    splitWithKeys(stack, doc, id, {{c[0], c[1], c[2]}, n}, keys);
                    did = true;
                } else if (what == 4) {
                    smartSplit(stack, doc, tops[std::size_t(pick(int(tops.size())))], {30, 30, 30}, KeyOptions{});
                    did = true;
                } else if (what == 5 && doc.find(tops[0])->isBoolean()) {
                    stack.execute(std::make_unique<SeparateCommand>(tops[0]));
                    did = true;
                } else if (what == 6) {
                    stack.execute(std::make_unique<RemoveBodyCommand>(tops[std::size_t(pick(int(tops.size())))]));
                    did = true;
                } else if (what == 7 && stack.canUndo()) {
                    stack.undo();
                    REQUIRE(at > 0);
                    --at;
                    INFO("seed " << seed << " step " << step << ": undo");
                    REQUIRE(state(doc) == history[at]);
                    continue;
                } else if (what == 8 && stack.canRedo()) {
                    stack.redo();
                    ++at;
                    INFO("seed " << seed << " step " << step << ": redo");
                    REQUIRE(at < history.size());
                    REQUIRE(state(doc) == history[at]);
                    continue;
                }
            } catch (const std::invalid_argument&) {
                // Refused (nothing to cut, a cut that misses): the document must be untouched.
                REQUIRE(state(doc) == history[at]);
                continue;
            } catch (const GeometryError&) {
                REQUIRE(state(doc) == history[at]);
                continue;
            }
            if (!did)
                continue;
            const std::string now = state(doc);
            if (now == history[at])
                continue;  // an empty step (a part that already fits)
            history.resize(at + 1);
            history.push_back(now);
            ++at;
            // Every body can still be built.
            for (const Body& b : doc.bodies())
                (void)doc.mesh(b.id);
        }
        // All the way back, then all the way forward.
        while (stack.canUndo()) {
            stack.undo();
            --at;
            REQUIRE(state(doc) == history[at]);
        }
        while (stack.canRedo()) {
            stack.redo();
            ++at;
            REQUIRE(state(doc) == history[at]);
        }
    }
}
