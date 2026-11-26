// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/FigureStyle.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/LimbCut.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Split.h"

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
    }
    return "";
}

const char* blurb(Style s)
{
    switch (s) {
    case Style::Bobblehead: return "A big head on a smaller body; the head can be cut off to sit on a spring.";
    case Style::Vinyl: return "A big head on a short, chunky body with stubby arms and legs.";
    case Style::Caricature: return "What's already big made bigger and what's small made smaller, and a bigger head.";
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
    // Vinyl and caricature at 100% are as 0.113.0 made them at 50% (that was too far).
    const double power = s == Style::Bobblehead ? strength : 0.5 * strength;
    Look out;
    for (int i = 0; i < kProportionCount; ++i) {
        const double v = std::pow(shares[std::size_t(i)], power);
        out.proportions[std::size_t(i)] =
            std::clamp(std::round(v * 100.0) / 100.0, rig::kLeastProportion, rig::kMostProportion);
    }
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

namespace {

Mesh onPlate(const Document& doc, ObjectId id)
{
    Mesh surface = *doc.mesh(id);
    const Transform toPlate = doc.worldTransform(id);
    for (std::size_t i = 0; i + 2 < surface.positions.size(); i += 3) {
        const Vec3 p = toPlate.apply(Vec3{surface.positions[i], surface.positions[i + 1], surface.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            surface.positions[i + k] = float(p[k]);
    }
    return surface;
}

// A closed surface merged onto, or taken out of, a piece: the piece's next step, named.
ObjectId withPart(CommandStack& stack, Document& doc, ObjectId piece, const Mesh& mesh, const std::string& partName,
                  BooleanOp op, const std::string& name)
{
    Body part = hinge::pieceBody(mesh, partName);
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
    s.name = name;
    s.boolean = std::move(spec);
    const ObjectId id = s.id;
    stack.execute(std::make_unique<CombineCommand>(std::move(s)));
    return id;
}

void checkRoom(const Spring& spring, double neck)
{
    if (sizesFor(spring).post / 2.0 > 0.9 * neck)
        throw std::invalid_argument("The neck is too thin there for a spring " + std::to_string(int(spring.diameter)) +
                                    " mm across: choose a smaller spring, or cut where the neck is thicker.");
}

// The socket up into the head from the cut at `p` (`up` into the head), the post up from
// the neck's face.
Bobble springIn(CommandStack& stack, Document& doc, ObjectId head, ObjectId body, const Vec3& headFace,
                const Vec3& neckFace, const Vec3& up, double neck, const Spring& spring)
{
    const SpringSizes z = sizesFor(spring);
    const double sink = std::min(2.0, neck);
    Bobble out;
    out.head = withPart(stack, doc, head,
                        limbcut::rod(add(headFace, mul(up, -2.0)), add(headFace, mul(up, z.socketDepth)),
                                     z.socket / 2.0, 32),
                        "Spring socket", BooleanOp::Subtract, "Head");
    out.body = withPart(stack, doc, body,
                        limbcut::rod(add(neckFace, mul(up, -sink)), add(neckFace, mul(up, z.postHeight)),
                                     z.post / 2.0, 32),
                        "Spring post", BooleanOp::Merge, "Body");
    return out;
}

}  // namespace

namespace {

// A slice across the line through `p` along `n`, `s` along it: the surface's points in it
// no further than `near` from the line; their middle (in the slice) and how far they are
// from it, typically. Radius 0 if too little is there.
struct Slice {
    Vec3 centre{0, 0, 0};
    double radius = 0.0;
};
Slice sliceAt(const Mesh& surface, const Vec3& p, const Vec3& n, double s, double near)
{
    // Where the triangles' edges cross the slice's plane (the surface's outline there).
    const Vec3 at = add(p, mul(n, s));
    const auto& P = surface.positions;
    const auto height = [&](std::uint32_t v) {
        return (P[3 * v] - at[0]) * n[0] + (P[3 * v + 1] - at[1]) * n[1] + (P[3 * v + 2] - at[2]) * n[2];
    };
    std::vector<Vec3> in;
    const auto& I = surface.indices;
    for (std::size_t t = 0; t + 2 < I.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t va = I[t + std::size_t(k)], vb = I[t + std::size_t((k + 1) % 3)];
            if (va > vb)
                continue;  // each edge once (shared edges come both ways round)
            const double ha = height(va), hb = height(vb);
            if ((ha > 0) == (hb > 0))
                continue;
            const double f = ha / (ha - hb);
            Vec3 e;
            for (int c = 0; c < 3; ++c)
                e[std::size_t(c)] = P[3 * va + std::size_t(c)] + f * (P[3 * vb + std::size_t(c)] - P[3 * va + std::size_t(c)]) -
                                    at[std::size_t(c)];
            if (e[0] * e[0] + e[1] * e[1] + e[2] * e[2] < near * near)
                in.push_back(e);
        }
    Slice out;
    if (in.size() < 8)
        return out;
    Vec3 c{0, 0, 0};
    for (const Vec3& v : in)
        c = add(c, v);
    c = mul(c, 1.0 / double(in.size()));
    std::vector<double> r;
    r.reserve(in.size());
    for (const Vec3& v : in)
        r.push_back(std::sqrt((v[0] - c[0]) * (v[0] - c[0]) + (v[1] - c[1]) * (v[1] - c[1]) +
                              (v[2] - c[2]) * (v[2] - c[2])));
    std::nth_element(r.begin(), r.begin() + std::ptrdiff_t(r.size() / 2), r.end());
    out.centre = add(at, c);
    out.radius = r[r.size() / 2];
    return out;
}

}  // namespace

NeckCut neckCut(const Mesh& surface, const rig::Points& points, double raise, double tilt)
{
    raise = std::clamp(raise, -kMostRaise, kMostRaise);
    tilt = std::clamp(tilt, -kMostTilt, kMostTilt);
    limbcut::Tilt t;
    t.forward = tilt;
    const limbcut::Cut cut = limbcut::plan(surface, points, rig::Joint::Neck, t);
    const Vec3 n = unit(cut.finish.normal);
    const Vec3 p0 = cut.finish.point;
    const auto along = [&](const Vec3& q) {
        return (q[0] - p0[0]) * n[0] + (q[1] - p0[1]) * n[1] + (q[2] - p0[2]) * n[2];
    };
    // Down from the top of the head: wider and wider through the head, then narrower (the
    // neck under the chin), the narrowest found until it widens again (the shoulders). The
    // skeleton's neck ball may be up in a big head, so it isn't trusted for the height.
    double top = along(at(points, Point::Head));
    for (std::size_t i = 0; i + 2 < surface.positions.size(); i += 3) {
        const Vec3 q{surface.positions[i], surface.positions[i + 1], surface.positions[i + 2]};
        const Vec3 e{q[0] - p0[0], q[1] - p0[1], q[2] - p0[2]};
        const double a = along(q);
        const Vec3 side{e[0] - a * n[0], e[1] - a * n[1], e[2] - a * n[2]};
        if (side[0] * side[0] + side[1] * side[1] + side[2] * side[2] < 4.0 * cut.limbRadius * cut.limbRadius)
            top = std::max(top, a);
    }
    const double span = std::max(1.0, top - along(at(points, Point::Waist)));
    const double near = 0.6 * span;
    const double step = std::max(0.25, span / 160.0);
    double shift = 0.0;
    Slice found;
    found.radius = cut.limbRadius;
    found.centre = p0;
    double widest = 0.0;
    bool inNeck = false, seen = false;
    for (double s = top - step; s >= top - 0.85 * span; s -= step) {
        const Slice here = sliceAt(surface, p0, n, s, near);
        if (here.radius <= 0)
            continue;
        if (!inNeck) {
            widest = std::max(widest, here.radius);
            if (here.radius < 0.8 * widest)
                inNeck = true;  // narrowing under the head
            else
                continue;
        }
        if (!seen || here.radius < found.radius - 0.02) {
            found = here;
            shift = s;
            seen = true;
        } else if (here.radius > 1.4 * found.radius) {
            break;  // the shoulders
        }
    }
    if (!seen)
        found.centre = p0;  // no neck to be seen: where the skeleton has it
    NeckCut out;
    out.normal = n;
    out.tilt = tilt;
    out.shift = shift + raise;
    out.radius = found.radius;
    out.point = add(found.centre, mul(n, raise));
    if (raise != 0.0) {
        const Slice there = sliceAt(surface, p0, n, out.shift, std::max(near, 3.0 * cut.limbRadius));
        if (there.radius > 0) {
            out.radius = there.radius;
            out.point = there.centre;
        }
    }
    return out;
}

Mesh cutDisc(const NeckCut& cut, double radius)
{
    // Square to the cut: two ways across it.
    const Vec3 n = unit(cut.normal);
    Vec3 a = std::abs(n[0]) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    const double k = a[0] * n[0] + a[1] * n[1] + a[2] * n[2];
    a = unit(Vec3{a[0] - k * n[0], a[1] - k * n[1], a[2] - k * n[2]});
    const Vec3 b{n[1] * a[2] - n[2] * a[1], n[2] * a[0] - n[0] * a[2], n[0] * a[1] - n[1] * a[0]};
    const int around = 48;
    const double half = 0.25;
    Mesh m;
    const auto put = [&](const Vec3& p) {
        for (int c = 0; c < 3; ++c)
            m.positions.push_back(float(p[std::size_t(c)]));
        return std::uint32_t(m.positions.size() / 3 - 1);
    };
    const std::uint32_t top = put(add(cut.point, mul(n, half))), bottom = put(add(cut.point, mul(n, -half)));
    for (int i = 0; i < around; ++i) {
        const double ang = 2.0 * 3.14159265358979 * i / around;
        const Vec3 r = add(mul(a, radius * std::cos(ang)), mul(b, radius * std::sin(ang)));
        put(add(add(cut.point, r), mul(n, half)));
        put(add(add(cut.point, r), mul(n, -half)));
    }
    for (int i = 0; i < around; ++i) {
        const std::uint32_t u0 = 2 + 2 * std::uint32_t(i), l0 = u0 + 1;
        const std::uint32_t u1 = 2 + 2 * std::uint32_t((i + 1) % around), l1 = u1 + 1;
        for (std::uint32_t v : {top, u0, u1, bottom, l1, l0, u0, l0, l1, u0, l1, u1})
            m.indices.push_back(v);
    }
    m.normals = std::vector<float>(m.positions.size(), 0.0f);
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const float* p0 = &m.positions[3 * m.indices[t]];
        const float* p1 = &m.positions[3 * m.indices[t + 1]];
        const float* p2 = &m.positions[3 * m.indices[t + 2]];
        const float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        const float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        const float c[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        for (int k = 0; k < 3; ++k)
            for (int q = 0; q < 3; ++q)
                m.normals[3 * m.indices[t + std::size_t(k)] + std::size_t(q)] += c[q];
    }
    for (std::size_t i = 0; i + 2 < m.normals.size(); i += 3) {
        const double l = std::sqrt(double(m.normals[i]) * m.normals[i] + double(m.normals[i + 1]) * m.normals[i + 1] +
                                   double(m.normals[i + 2]) * m.normals[i + 2]);
        if (l > 0)
            for (int q = 0; q < 3; ++q)
                m.normals[i + std::size_t(q)] = float(m.normals[i + std::size_t(q)] / l);
    }
    return m;
}

Bobble separateHead(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points,
                    const Spring& spring, double raise, double tilt)
{
    check(spring);
    if (!doc.find(figure) || !doc.isTopLevel(figure))
        throw std::invalid_argument("Choose a figure on the plate.");
    const Mesh surface = onPlate(doc, figure);
    const NeckCut nc = neckCut(surface, points, raise, tilt);
    checkRoom(spring, nc.radius);
    limbcut::Choice choice;
    choice.joint = rig::Joint::Neck;
    choice.kind = limbcut::JointKind::Cut;
    choice.tilt.forward = nc.tilt;
    choice.shift = nc.shift;
    // The faces as Cut into Parts leaves them (its gap, if any, split either side).
    LimbCutFinish f;
    f.point = {nc.point[0], nc.point[1], nc.point[2]};
    f.normal = {nc.normal[0], nc.normal[1], nc.normal[2]};
    f.gap = limbcut::gapFor(choice);
    const Vec3 headFace = limbcut::limbFace(f), neckFace = limbcut::restFace(f);

    Bobble out;
    stack.beginMacro("Bobblehead");
    try {
        const limbcut::Made made = limbcut::cutIntoParts(stack, doc, figure, points, {choice}, 1);
        if (made.pieces.size() < 2)
            throw std::invalid_argument("The neck's cut doesn't cut the head off.");
        // Pieces in bone order: the body (hips and chest) first, then the head.
        out = springIn(stack, doc, made.pieces[1], made.pieces[0], headFace, neckFace, nc.normal, nc.radius, spring);
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return out;
}

Bobble separateHeadByMask(CommandStack& stack, Document& doc, ObjectId figure, const std::vector<MaskDab>& dabs,
                          const Spring& spring)
{
    check(spring);
    if (!doc.find(figure) || !doc.isTopLevel(figure))
        throw std::invalid_argument("Choose a figure on the plate.");
    if (dabs.empty())
        throw std::invalid_argument("Paint over the head first.");
    // Where the flat cut goes: the cap's middle and the way across it, and how much room.
    // Joined-up triangles (the mask's edge is followed round from one to the next).
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(doc.meshSolid(figure), pos, idx);
    const Transform toPlate = doc.worldTransform(figure);
    for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
        const Vec3 p = toPlate.apply(Vec3{pos[i], pos[i + 1], pos[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            pos[i + k] = float(p[k]);
    }
    const mask::Pieces pieces = mask::split(pos, idx, dabs, MaskCut::Flat);
    if (pieces.keySpots.empty())
        throw std::invalid_argument("The mask's edge must go round the neck once: paint the whole head.");
    // The main cap (specks and gaps in the paint make little ones).
    const mask::KeySpot spot = *std::max_element(pieces.keySpots.begin(), pieces.keySpots.end(),
                                                 [](const auto& a, const auto& b) { return a.reach < b.reach; });
    checkRoom(spring, spot.room);
    // Up: from the cut towards the head.
    Vec3 hc{0, 0, 0};
    const std::size_t count = pieces.masked.positions.size() / 3;
    for (std::size_t i = 0; i < count; ++i)
        for (int k = 0; k < 3; ++k)
            hc[std::size_t(k)] += pieces.masked.positions[3 * i + std::size_t(k)] / double(count);
    Vec3 up = unit(spot.normal);
    const Vec3 toHead{hc[0] - spot.point[0], hc[1] - spot.point[1], hc[2] - spot.point[2]};
    if (toHead[0] * up[0] + toHead[1] * up[1] + toHead[2] * up[2] < 0)
        up = mul(up, -1.0);

    Bobble out;
    stack.beginMacro("Bobblehead");
    try {
        const std::vector<ObjectId> made =
            splitByMask(stack, doc, figure, dabs, SplitKeep::Both, -1.0, MaskCut::Flat);
        if (made.size() < 2)
            throw std::invalid_argument("The mask doesn't cut the head off.");
        out = springIn(stack, doc, made[0], made[1], spot.point, spot.point, up, spot.room, spring);
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return out;
}

}  // namespace forge::figurestyle
