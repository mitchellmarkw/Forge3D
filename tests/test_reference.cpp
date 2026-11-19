// SPDX-License-Identifier: GPL-3.0-or-later
// Slice 49: reference images.
#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FileFormat.h"
#include "forge/core/Reference.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

using namespace forge;
using Catch::Matchers::WithinAbs;

namespace {

ReferenceImage picture(RefView view, int w = 400, int h = 200)
{
    ReferenceImage r;
    r.view = view;
    r.name = "photo.png";
    r.suffix = "png";
    // Any bytes do: the core keeps the file as it was and never looks inside.
    r.data = std::make_shared<const std::string>(std::string("\x89PNG fake picture\0bytes", 23));
    r.pixelsWide = w;
    r.pixelsHigh = h;
    r.mmPerPixel = 0.5;
    return r;
}

void near(const Vec3& a, const Vec3& b)
{
    for (int i = 0; i < 3; ++i)
        CHECK_THAT(a[i], WithinAbs(b[i], 1e-9));
}

}  // namespace

TEST_CASE("A front picture stands behind the model, the right way up", "[reference]")
{
    ReferenceImage r = picture(RefView::Front);  // 200 x 100 mm
    r.depth = 30;
    const auto c = reference::corners(r);
    // Seen from the front (-Y): its top-left is at the left (-X) and up (+Z), behind (+Y).
    near(c[0], {-100, 30, 50});
    near(c[1], {100, 30, 50});
    near(c[2], {100, 30, -50});
    near(c[3], {-100, 30, -50});

    // A ray from the front, looking back (+Y), hits it at the right pixel.
    const auto h = reference::hit(r, {50, -200, 25}, {0, 1, 0});
    REQUIRE(h);
    CHECK_THAT(h->distance, WithinAbs(230, 1e-9));
    CHECK_THAT(h->px, WithinAbs(300, 1e-9));
    CHECK_THAT(h->py, WithinAbs(50, 1e-9));
    // From behind, or beside it, it isn't there.
    CHECK_FALSE(reference::hit(r, {50, 200, 25}, {0, -1, 0}));
    CHECK_FALSE(reference::hit(r, {150, -200, 25}, {0, 1, 0}));
}

TEST_CASE("Every direction's picture faces its own view", "[reference]")
{
    for (int v = 0; v < kRefViewCount; ++v) {
        ReferenceImage r = picture(static_cast<RefView>(v));
        r.depth = 10;
        const auto f = reference::frameOf(r.view);
        // Looking at it from its view, straight at its middle.
        const Vec3 eye{f.toward[0] * 500, f.toward[1] * 500, f.toward[2] * 500};
        const auto h = reference::hit(r, eye, {-f.toward[0], -f.toward[1], -f.toward[2]});
        INFO(reference::title(r.view));
        REQUIRE(h);
        CHECK_THAT(h->px, WithinAbs(200, 1e-9));
        CHECK_THAT(h->py, WithinAbs(100, 1e-9));
        CHECK_THAT(h->distance, WithinAbs(510, 1e-9));
        // Its top-right corner is to the right and up on the screen.
        const Vec3 tr = reference::corners(r)[1];
        const auto dot = [](const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
        CHECK(dot(tr, f.right) > 0);
        CHECK(dot(tr, f.up) > 0);
        CHECK(reference::fromKey(reference::key(r.view)) == r.view);
    }
}

TEST_CASE("Calibrating sizes a picture from two points and keeps the first one still", "[reference]")
{
    ReferenceImage r = picture(RefView::Right, 1000, 800);
    r.turn = 15;
    r.across = 12;
    r.up = 40;
    const std::array<double, 2> a{100, 700}, b{400, 300};  // 500 pixels apart
    const Vec3 before = reference::pointAt(r, a[0], a[1]);

    // 4.5 m at 1:48 is 93.75 mm on the model.
    const double mm = reference::modelLength(4.5, reference::Unit::Metres, 48);
    CHECK_THAT(mm, WithinAbs(93.75, 1e-9));
    const ReferenceImage c = reference::calibrated(r, a, b, mm);
    CHECK_THAT(c.mmPerPixel, WithinAbs(93.75 / 500, 1e-12));
    near(reference::pointAt(c, a[0], a[1]), before);
    const Vec3 pa = reference::pointAt(c, a[0], a[1]), pb = reference::pointAt(c, b[0], b[1]);
    CHECK_THAT(std::hypot(pa[0] - pb[0], pa[1] - pb[1], pa[2] - pb[2]), WithinAbs(93.75, 1e-9));
    CHECK_THAT(c.depth, WithinAbs(r.depth, 1e-12));  // still in the same plane

    // Full size, in inches.
    CHECK_THAT(reference::modelLength(2, reference::Unit::Inches, 1), WithinAbs(50.8, 1e-9));
    // The Scale Calculator's example: a 6 ft person at 1:60 is 30.48 mm.
    CHECK_THAT(reference::modelLength(6, reference::Unit::Feet, 60), WithinAbs(30.48, 1e-9));
    CHECK_THAT(reference::modelLength(1, reference::Unit::Yards, 1), WithinAbs(914.4, 1e-9));
    CHECK_THAT(reference::fromMm(30.48, reference::Unit::Inches), WithinAbs(1.2, 1e-9));
    CHECK_THROWS(reference::calibrated(r, a, a, 10));
    CHECK_THROWS(reference::calibrated(r, a, b, 0));
    CHECK_THROWS(reference::modelLength(1, reference::Unit::Millimetres, 0));
}

TEST_CASE("Clicks go through inactive and hidden pictures", "[reference]")
{
    ReferenceImage near1 = picture(RefView::Front);
    near1.depth = 10;
    ReferenceImage top = picture(RefView::Top);
    std::vector<ReferenceImage> all{near1, top};
    // Looking from the front and a little above: only the front picture is seen.
    CHECK(reference::pick(all, {0, -300, 0}, {0, 1, 0}) == std::size_t(0));
    all[0].active = false;
    CHECK_FALSE(reference::pick(all, {0, -300, 0}, {0, 1, 0}));
    all[0].active = true;
    all[0].shown = false;
    CHECK_FALSE(reference::pick(all, {0, -300, 0}, {0, 1, 0}));
    // From above, the top one.
    reference::Hit where;
    CHECK(reference::pick(all, {10, 10, 300}, {0, 0, -1}, &where) == std::size_t(1));
    CHECK_THAT(where.px, WithinAbs(220, 1e-9));
}

TEST_CASE("A new picture stands on the plate behind the model", "[reference]")
{
    const ReferenceImage front = reference::placed(picture(RefView::Front, 400, 200), 150, 40);
    CHECK_THAT(front.widthMm(), WithinAbs(150, 1e-9));
    CHECK_THAT(reference::corners(front)[3][2], WithinAbs(0, 1e-9));  // bottom on the plate
    CHECK_THAT(reference::corners(front)[3][1], WithinAbs(40, 1e-9));
    const ReferenceImage top = reference::placed(picture(RefView::Top, 400, 200), 150, 5);
    CHECK_THAT(reference::corners(top)[0][2], WithinAbs(-5, 1e-9));  // under the plate
}

TEST_CASE("One picture per direction; changes undo", "[reference]")
{
    Document doc;
    CommandStack stack(doc);
    int told = 0;
    doc.addReferenceListener([&told] { ++told; });

    stack.execute(std::make_unique<SetReferencesCommand>(std::vector{picture(RefView::Front)}, "Add Picture"));
    CHECK(doc.references().size() == 1);
    CHECK(told == 1);
    CHECK_THROWS(doc.setReferences({picture(RefView::Top), picture(RefView::Top)}));
    CHECK(doc.references().size() == 1);

    // Opacity slider moves: one undo step.
    for (double o : {0.5, 0.4, 0.3}) {
        auto changed = doc.references();
        changed[0].opacity = o;
        stack.execute(std::make_unique<SetReferencesCommand>(changed, "Picture Opacity", true));
    }
    CHECK_THAT(doc.references()[0].opacity, WithinAbs(0.3, 1e-12));
    stack.undo();
    CHECK_THAT(doc.references()[0].opacity, WithinAbs(0.6, 1e-12));
    stack.undo();
    CHECK(doc.references().empty());
    stack.redo();
    CHECK(doc.references().size() == 1);
    // The picture file isn't copied for each step.
    CHECK(doc.references()[0].data.use_count() >= 2);
}

TEST_CASE("Reference pictures are saved in the file, and nothing else has them", "[reference]")
{
    Document doc;
    CommandStack stack(doc);
    Body box;
    box.id = doc.allocateId();
    box.name = "Box 1";
    box.spec = BoxSpec{};
    stack.execute(std::make_unique<AddBodyCommand>(box));

    ReferenceImage front = picture(RefView::Front);
    front.across = 3;
    front.up = 4;
    front.turn = -7;
    front.depth = 25;
    front.opacity = 0.35;
    front.active = false;
    ReferenceImage side = picture(RefView::Left, 64, 32);
    side.suffix = "JPG";
    side.shown = false;
    doc.setReferences({front, side});

    const auto path = std::filesystem::temp_directory_path() / "forge3d-test-references.f3d";
    file::save(doc, path, "test");
    const auto bodies = file::load(path);
    CHECK(bodies.size() == 1);  // the pictures aren't shapes
    const auto back = file::loadReferences(path);
    REQUIRE(back.size() == 2);
    const ReferenceImage& f = back[0];
    CHECK(f.view == RefView::Front);
    CHECK(f.name == "photo.png");
    CHECK(*f.data == *front.data);
    CHECK(f.pixelsWide == 400);
    CHECK_THAT(f.mmPerPixel, WithinAbs(0.5, 1e-12));
    CHECK_THAT(f.across, WithinAbs(3, 1e-12));
    CHECK_THAT(f.up, WithinAbs(4, 1e-12));
    CHECK_THAT(f.turn, WithinAbs(-7, 1e-12));
    CHECK_THAT(f.depth, WithinAbs(25, 1e-12));
    CHECK_THAT(f.opacity, WithinAbs(0.35, 1e-12));
    CHECK_FALSE(f.active);
    CHECK(f.shown);
    CHECK(back[1].view == RefView::Left);
    CHECK(back[1].suffix == "jpg");
    CHECK_FALSE(back[1].shown);

    // A file without pictures, or with a broken one, still opens.
    Document plain;
    file::save(plain, path, "test");
    CHECK(file::loadReferences(path).empty());
    const std::string broken = R"({"format":"forge3d-document","formatVersion":28,"bodies":[],
        "references":[{"view":"sideways","file":"references/x.png","pixels":[1,1],"mmPerPixel":1},
                      {"view":"top","file":"references/top.png","pixels":[0,5],"mmPerPixel":1},
                      {"view":"back","file":"references/back.png","pixels":[2,2],"mmPerPixel":1}]})";
    const auto some = file::referencesFromJson(broken, [](const std::string& name) {
        if (name != "references/back.png")
            throw file::FileError("missing");
        return std::string("pic");
    });
    REQUIRE(some.size() == 1);
    CHECK(some[0].view == RefView::Back);
    CHECK(file::fromJson(broken).empty());
    std::filesystem::remove(path);
}
