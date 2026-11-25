// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/FigureStyle.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/LimbCut.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace forge::figurestyle {

namespace {

using rig::Point;
using rig::Proportion;

Vec3 at(const rig::Points& p, Point q) { return p[std::size_t(q)]; }
double dist(const Vec3& a, const Vec3& b)
{
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}
Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 mul(const Vec3& a, double k) { return {a[0] * k, a[1] * k, a[2] * k}; }
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    return l > 1e-12 ? mul(a, 1.0 / l) : Vec3{0, 0, 1};
}

// Each style's shares (head, shoulders, torso, arms, hands, legs, feet, limbs, body).
rig::Proportions sharesOf(Style s)
{
    switch (s) {
    case Style::Bobblehead: return {2.4, 0.95, 0.85, 0.85, 1.1, 0.8, 1.2, 1.1, 1.05};
    case Style::Vinyl: return {2.7, 0.9, 0.6, 0.6, 0.9, 0.5, 1.4, 1.6, 1.3};
    case Style::Caricature: return {1.6, 1.0, 1.0, 1.0, 1.15, 1.0, 1.1, 1.0, 1.0};
    case Style::Blocky: return {1.15, 1.0, 0.95, 0.95, 1.1, 0.9, 1.1, 1.25, 1.1};
    }
    return rig::asMade();
}

// How a caricature sees the figure: each part's length against the usual figure's (the
// figure guide's, for this height), so what's long is made longer.
rig::Proportions measured(const rig::Points& p)
{
    rig::Proportions k = rig::asMade();
    const double ankle = 0.5 * (at(p, Point::LeftAnkle)[2] + at(p, Point::RightAnkle)[2]);
    const double h = (at(p, Point::Head)[2] - ankle) / 0.9567;
    if (!(h > 1e-6))
        return k;
    const double H = h / 7.5, R = h - H;
    const auto share = [&](double length, double usual) {
        return usual > 1e-9 && length > 1e-9 ? std::clamp(length / usual, 0.5, 2.0) : 1.0;
    };
    const auto both = [&](Point la, Point lb, Point lc, Point ra, Point rb, Point rc) {
        return 0.5 * (dist(at(p, la), at(p, lb)) + dist(at(p, lb), at(p, lc)) + dist(at(p, ra), at(p, rb)) +
                      dist(at(p, rb), at(p, rc)));
    };
    k[std::size_t(Proportion::Head)] = share(at(p, Point::Head)[2] - at(p, Point::Neck)[2], H);
    k[std::size_t(Proportion::Torso)] = share(at(p, Point::Neck)[2] - at(p, Point::Waist)[2], 0.29 * R);
    k[std::size_t(Proportion::Shoulders)] =
        share(dist(at(p, Point::LeftShoulder), at(p, Point::RightShoulder)), 1.8 * H);
    k[std::size_t(Proportion::Arms)] = share(both(Point::LeftShoulder, Point::LeftElbow, Point::LeftWrist,
                                                  Point::RightShoulder, Point::RightElbow, Point::RightWrist),
                                             0.37 * R);
    k[std::size_t(Proportion::Hands)] =
        share(0.5 * (dist(at(p, Point::LeftWrist), at(p, Point::LeftHand)) +
                     dist(at(p, Point::RightWrist), at(p, Point::RightHand))),
              0.09 * R);
    k[std::size_t(Proportion::Legs)] = share(
        both(Point::LeftHip, Point::LeftKnee, Point::LeftAnkle, Point::RightHip, Point::RightKnee, Point::RightAnkle),
        0.55 * R);
    return k;
}

}  // namespace

const char* title(Style s)
{
    switch (s) {
    case Style::Bobblehead: return "Bobblehead";
    case Style::Vinyl: return "Vinyl collectible";
    case Style::Caricature: return "Caricature";
    case Style::Blocky: return "Blocky";
    }
    return "";
}

const char* blurb(Style s)
{
    switch (s) {
    case Style::Bobblehead: return "A big head on a smaller body; the head can be cut off to sit on a spring.";
    case Style::Vinyl: return "A huge head on a short, chunky body with stubby arms and legs.";
    case Style::Caricature: return "What's already big made bigger and what's small made smaller, and a bigger head.";
    case Style::Blocky: return "Squared off all over, like a toy built from blocks.";
    }
    return "";
}

Look look(Style s, const rig::Points& asMade, double strength)
{
    strength = std::clamp(strength, kLeastStrength, kMostStrength);
    rig::Proportions shares = sharesOf(s);
    if (s == Style::Caricature) {
        const rig::Proportions m = measured(asMade);
        for (int i = 0; i < kProportionCount; ++i)
            shares[std::size_t(i)] *= m[std::size_t(i)];
    }
    Look out;
    for (int i = 0; i < kProportionCount; ++i) {
        const double v = std::pow(shares[std::size_t(i)], strength);
        out.proportions[std::size_t(i)] =
            std::clamp(std::round(v * 100.0) / 100.0, rig::kLeastProportion, rig::kMostProportion);
    }
    if (s == Style::Blocky)
        out.blocky = std::clamp(strength, 0.0, 1.0);
    return out;
}

SpringSizes sizesFor(const Spring& s)
{
    SpringSizes out;
    out.post = s.diameter - 2.0 * s.wire - 0.1;
    out.postHeight = 1.2 * s.diameter;
    out.socket = s.diameter + 2.0 * s.fit;
    out.socketDepth = 1.5 * s.diameter;
    return out;
}

void check(const Spring& s)
{
    if (!(s.diameter >= kLeastSpring && s.diameter <= kMostSpring))
        throw std::invalid_argument("The spring must be from 2 to 20 mm across.");
    if (!(s.wire >= 0.1 && s.wire <= s.diameter / 4.0))
        throw std::invalid_argument("The spring's wire must be from 0.1 mm to a quarter of the spring's width.");
    if (!(s.fit >= 0.0 && s.fit <= 1.0))
        throw std::invalid_argument("The room round the spring must be from 0 to 1 mm.");
}

Bobble separateHead(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points,
                    const Spring& spring)
{
    check(spring);
    if (!doc.find(figure) || !doc.isTopLevel(figure))
        throw std::invalid_argument("Choose a figure on the plate.");
    // Where the neck's cut goes (as Cut into Parts will cut it), and how thick it is there.
    Mesh surface = *doc.mesh(figure);
    const Transform toPlate = doc.worldTransform(figure);
    for (std::size_t i = 0; i + 2 < surface.positions.size(); i += 3) {
        const Vec3 p = toPlate.apply(Vec3{surface.positions[i], surface.positions[i + 1], surface.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            surface.positions[i + k] = float(p[k]);
    }
    limbcut::Choice choice;
    choice.joint = rig::Joint::Neck;
    choice.kind = limbcut::JointKind::Cut;
    limbcut::Cut cut = limbcut::plan(surface, points, rig::Joint::Neck);
    cut.finish.gap = limbcut::gapFor(choice);
    // A big head widens the neck just under it: cut where the neck's narrowest, between
    // there and the shoulders, so the head's piece ends in a neck, not a wide lip.
    {
        const Vec3 n = unit(cut.finish.normal);
        const Vec3 p0 = cut.finish.point;
        const Vec3 mid = mul(add(at(points, Point::LeftShoulder), at(points, Point::RightShoulder)), 0.5);
        const Vec3 d{mid[0] - p0[0], mid[1] - p0[1], mid[2] - p0[2]};
        const double down = std::max(0.0, -(d[0] * n[0] + d[1] * n[1] + d[2] * n[2])) * 0.7;
        const double near = 2.5 * cut.limbRadius;
        double best = cut.limbRadius, bestShift = 0.0;
        for (double t = -0.5; t >= -down; t -= 0.5) {
            std::vector<double> ring;
            for (std::size_t i = 0; i + 2 < surface.positions.size(); i += 3) {
                const Vec3 e{surface.positions[i] - p0[0], surface.positions[i + 1] - p0[1],
                             surface.positions[i + 2] - p0[2]};
                const double along = e[0] * n[0] + e[1] * n[1] + e[2] * n[2];
                if (std::abs(along - t) > 0.75)
                    continue;
                const Vec3 side{e[0] - along * n[0], e[1] - along * n[1], e[2] - along * n[2]};
                const double r = std::sqrt(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
                if (r < near)
                    ring.push_back(r);
            }
            if (ring.size() < 8)
                continue;
            std::nth_element(ring.begin(), ring.begin() + std::ptrdiff_t(ring.size() / 2), ring.end());
            const double r = ring[ring.size() / 2];
            if (r < best - 0.05) {
                best = r;
                bestShift = t;
            }
        }
        choice.shift = bestShift;
        cut.limbRadius = best;
        for (int k = 0; k < 3; ++k)
            cut.finish.point[std::size_t(k)] += bestShift * n[std::size_t(k)];
    }
    const SpringSizes z = sizesFor(spring);
    if (z.post / 2.0 > 0.9 * cut.limbRadius)
        throw std::invalid_argument("The neck is too thin for a spring " + std::to_string(int(spring.diameter)) +
                                    " mm across: choose a smaller spring.");
    const Vec3 n = unit(cut.finish.normal);  // up into the head
    const Vec3 headFace = limbcut::limbFace(cut.finish), neckFace = limbcut::restFace(cut.finish);

    Bobble out;
    stack.beginMacro("Bobblehead");
    try {
        const limbcut::Made made = limbcut::cutIntoParts(stack, doc, figure, points, {choice}, 1);
        if (made.pieces.size() < 2)
            throw std::invalid_argument("The neck's cut doesn't cut the head off.");
        // Pieces in bone order: the body (hips and chest) first, then the head.
        ObjectId body = made.pieces[0], head = made.pieces[1];
        // A closed surface merged onto, or taken out of, a piece: the piece's next step.
        const auto withPart = [&](ObjectId piece, const Mesh& mesh, const std::string& name, BooleanOp op) {
            Body part = hinge::pieceBody(mesh, name);
            part.id = doc.allocateId();
            const ObjectId partId = part.id;
            stack.execute(std::make_unique<AddBodyCommand>(std::move(part)));
            BooleanSpec spec;
            spec.op = op;
            spec.operands = {piece, partId};
            const auto centre = doc.previewResult(spec).bounds.center();
            spec.pivot = {centre[0], centre[1], centre[2]};
            Body s;
            s.id = doc.allocateId();
            s.name = doc.find(piece)->name;
            s.boolean = std::move(spec);
            const ObjectId id = s.id;
            stack.execute(std::make_unique<CombineCommand>(std::move(s)));
            return id;
        };
        // The socket up into the head from its cut face; the post up from the neck's.
        const double sink = std::min(2.0, cut.limbRadius);
        head = withPart(head,
                        limbcut::rod(add(headFace, mul(n, -2.0)), add(headFace, mul(n, z.socketDepth)),
                                     z.socket / 2.0, 32),
                        "Spring socket", BooleanOp::Subtract);
        body = withPart(body,
                        limbcut::rod(add(neckFace, mul(n, -sink)), add(neckFace, mul(n, z.postHeight)), z.post / 2.0,
                                     32),
                        "Spring post", BooleanOp::Merge);
        out = {body, head};
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return out;
}

}  // namespace forge::figurestyle
