// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 114: Forge3D Scenery's tree designer.
#include "forge/core/CommandStack.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Tree.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {
double volumeOf(const Mesh& m)
{
    return meshkernel::volume(meshkernel::fromTriangles(m.positions, m.indices));
}
}  // namespace

TEST_CASE("Trees: every kind, to scale, standing flat", "[tree]")
{
    for (TreeKind kind : {TreeKind::Broadleaf, TreeKind::Conifer, TreeKind::Dead, TreeKind::Palm}) {
        INFO(tree::title(kind));
        const TreeSpec s = tree::defaults(kind);
        const double k = 1.0 / s.scale;
        const auto start = std::chrono::steady_clock::now();
        const Mesh m = tree::make(s);
        const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        INFO("made in " << took << " s, " << m.triangleCount() << " triangles");
        CHECK(took < 20.0);
        CHECK(volumeOf(m) > 0);
        const Bounds b = m.bounds();
        CHECK_THAT(double(b.min[2]), WithinAbs(0.0, 0.01));
        CHECK_THAT(double(b.max[2]), WithinRel(s.height * k, 0.08));
        const double across = std::max(b.max[0] - b.min[0], b.max[1] - b.min[1]);
        CHECK(across < s.crownWidth * k * 1.15);
        CHECK(across > s.crownWidth * k * 0.6);
        // Flat on the ground: plenty of the underside at Z = 0.
        int onGround = 0;
        for (std::size_t i = 2; i < m.positions.size(); i += 3)
            onGround += std::abs(m.positions[i]) < 1e-3;
        CHECK(onGround > 10);
        // The quick one: simpler.
        CHECK(tree::make(s, true).triangleCount() > 0);
    }
}

TEST_CASE("Trees: seeds, leaves, branches, lean", "[tree]")
{
    TreeSpec s = tree::defaults(TreeKind::Broadleaf);
    const double a = volumeOf(tree::make(s));
    CHECK_THAT(volumeOf(tree::make(s)), WithinRel(a, 1e-9));
    s.seed = 2;
    CHECK(std::abs(volumeOf(tree::make(s)) - a) > 0.01 * a);
    // Bare: much less.
    TreeSpec bare = s;
    bare.leaves = false;
    CHECK(volumeOf(tree::make(bare)) < 0.5 * volumeOf(tree::make(s)));
    // More branches: more parts in the quick one.
    TreeSpec few = s, many = s;
    few.branches = 0;
    many.branches = 1;
    CHECK(tree::make(many, true).triangleCount() > tree::make(few, true).triangleCount());
    // Leaning: off to one side.
    TreeSpec lean = s;
    lean.lean = 20;
    const Bounds lb = tree::make(lean, true).bounds();
    CHECK(lb.max[0] + lb.min[0] > 0.0);
    // Thin branches are thickened to what prints.
    CHECK_THAT(tree::thinnest(s), WithinAbs(1.0, 1e-9));
    // Checked.
    TreeSpec tiny = s;
    tiny.height = 300;
    tiny.scale = 100;
    CHECK_THROWS_AS(tree::make(tiny), std::invalid_argument);
}

TEST_CASE("Trees: added, changed and saved", "[tree]")
{
    Document doc;
    CommandStack stack(doc);
    const TreeSpec s = tree::defaults(TreeKind::Conifer);
    const ObjectId id = tree::add(stack, doc, s, "Conifer", 20.0, 40.0);
    REQUIRE(doc.find(id));
    CHECK(doc.find(id)->tree);
    CHECK_THAT(double(doc.worldBounds(id).min[2]), WithinAbs(0.0, 0.05));
    // Where its foot is: the same after a change (the crown may differ).
    const auto foot = [&] { return doc.find(id)->placement.x - doc.find(id)->tree->offset[0]; };
    CHECK_THAT(foot(), WithinAbs(20.0, 3.0));
    const double x = foot();
    TreeSpec t = s;
    t.seed = 5;
    t.height = 14000;
    tree::update(stack, doc, id, t);
    CHECK(doc.find(id)->tree->height == 14000);
    CHECK_THAT(foot(), WithinAbs(x, 1e-9));
    stack.undo();
    CHECK(doc.find(id)->tree->height == s.height);
    const auto path = std::filesystem::temp_directory_path() / "forge_tree_test.f3d";
    file::save(doc, path, "test");
    Document other;
    other.replaceAll(file::load(path));
    std::filesystem::remove(path);
    CHECK(other.find(id)->tree == doc.find(id)->tree);
}
