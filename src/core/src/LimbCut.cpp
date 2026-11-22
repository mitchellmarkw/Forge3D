// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/LimbCut.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Hinge.h"
#include "forge/core/Socket.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <string>

namespace forge {

namespace limbcut {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a, const Vec3& fallback = {0, 0, 1})
{
    const double l = length(a);
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}

double segmentDistance(const Vec3& q, const Vec3& a, const Vec3& b)
{
    const Vec3 d = sub(b, a);
    const double dd = dot(d, d);
    const double t = dd > 0 ? std::clamp(dot(sub(q, a), d) / dd, 0.0, 1.0) : 0.0;
    return length(sub(q, add(a, mul(d, t))));
}

bool isLimbJoint(rig::Joint j)
{
    switch (j) {
    case rig::Joint::LeftElbow:
    case rig::Joint::RightElbow:
    case rig::Joint::LeftKnee:
    case rig::Joint::RightKnee:
    case rig::Joint::LeftWrist:
    case rig::Joint::RightWrist:
    case rig::Joint::LeftAnkle:
    case rig::Joint::RightAnkle: return true;
    default: return false;
    }
}

Vec3 boneDirection(const rig::Points& points, int bone)
{
    const auto segs = rig::boneSegments(points, bone);
    return segs.empty() ? Vec3{0, 0, 1} : unit(sub(segs.front()[1], segs.front()[0]));
}

Cut planOne(const Mesh& figure, const rig::Points& points, rig::Joint joint, const std::vector<int>& nearest)
{
    const int bone = rig::boneOf(joint);
    const Vec3 p = points[std::size_t(rig::pivot(joint))];
    // Cuts on the body go the body's way, whichever way the limb hangs: the shoulders
    // straight up and down (square to the sides), the neck and waist level, the hips level.
    // Cuts along a limb go square to it, halfway between its two bones.
    Vec3 normal = boneDirection(points, bone);
    switch (joint) {
    case rig::Joint::Neck:
    case rig::Joint::Waist: normal = {0, 0, 1}; break;
    case rig::Joint::LeftShoulder: normal = {1, 0, 0}; break;  // the figure's left is +X
    case rig::Joint::RightShoulder: normal = {-1, 0, 0}; break;
    case rig::Joint::LeftHip:
    case rig::Joint::RightHip: normal = {0, 0, -1}; break;
    default:
        if (isLimbJoint(joint))
            normal = unit(add(normal, boneDirection(points, rig::parentBone(bone))), normal);
    }

    // How far each bone's surface reaches from it.
    const std::vector<int> below = rig::boneAndBelow(bone);
    std::vector<double> reach(rig::kBoneCount, 0.0);
    const std::size_t n = figure.vertexCount();
    std::vector<std::vector<std::array<Vec3, 2>>> segs(rig::kBoneCount);
    for (int b = 0; b < rig::kBoneCount; ++b)
        segs[std::size_t(b)] = rig::boneSegments(points, b);
    const int parent = rig::parentBone(bone);
    std::vector<double> ring;  // the limb's surface round it, near the cut: how far from the bone
    const Vec3 boneDir = boneDirection(points, bone);
    const auto boneSegs = rig::boneSegments(points, bone);
    const double boneLength =
        boneSegs.empty() ? 0.0 : length(sub(boneSegs.front()[1], boneSegs.front()[0]));
    // At the cut, but a little way into a limb cut off the body at a slant (the head and
    // chest widen past the neck and waist: right at the cut for those).
    const bool body = joint == rig::Joint::Neck || joint == rig::Joint::Waist;
    const Vec3 across = body ? p : add(p, mul(boneDir, 0.25 * boneLength));
    std::vector<std::vector<double>> distances(rig::kBoneCount);
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3 q{figure.positions[3 * i], figure.positions[3 * i + 1], figure.positions[3 * i + 2]};
        const int b = nearest[i];
        // Only the limb's side of the cut counts: the body beside a shoulder, nearest the
        // upper arm though it is, mustn't fatten the arm's reach.
        const bool beyond = dot(sub(q, p), normal) > 0.0;
        if (beyond && std::find(below.begin(), below.end(), b) != below.end()) {
            double d = std::numeric_limits<double>::max();
            for (const auto& s : segs[std::size_t(b)])
                d = std::min(d, segmentDistance(q, s[0], s[1]));
            distances[std::size_t(b)].push_back(d);
        }
        // The limb's thickness at the cut: the nearest surface in the cut's plane.
        const Vec3 d = sub(q, p);
        const double along = dot(d, normal);
        (void)along;
        // The limb's thickness a little way along its bone, across it (a cut at a slant
        // across an arm is longer than the arm is thick).
        if (b == bone) {
            const Vec3 e = sub(q, across);
            const double t = dot(e, boneDir);
            if (std::abs(t) < 0.75)
                ring.push_back(length(sub(e, mul(boneDir, t))));
        }
        (void)parent;
    }
    // How far each bone's surface reaches: nearly all of it (a few points of another part
    // of the body that happen to be nearest, say the crotch to a thigh, don't count).
    for (int b : below) {
        auto& d = distances[std::size_t(b)];
        if (d.empty())
            continue;
        const std::size_t at = std::min(d.size() - 1, d.size() * 97 / 100);
        std::nth_element(d.begin(), d.begin() + std::ptrdiff_t(at), d.end());
        reach[std::size_t(b)] = d[at];
    }
    double widest = 0.0;
    for (int b : below)
        widest = std::max(widest, reach[std::size_t(b)]);
    if (widest <= 0.0)
        throw std::invalid_argument(std::string("There's no ") + rig::boneTitle(bone) +
                                    " to cut off at the " + rig::title(joint) + ": check the skeleton's balls.");

    Cut cut;
    cut.finish.joint = int(joint);
    cut.finish.point = p;
    cut.finish.normal = normal;
    // The limb's thickness at the cut: how far its surface round the cut is from the bone,
    // typically.
    if (!ring.empty()) {
        std::nth_element(ring.begin(), ring.begin() + std::ptrdiff_t(ring.size() / 2), ring.end());
        cut.limbRadius = ring[ring.size() / 2];
    } else {
        cut.limbRadius = 0.6 * widest;
    }
    for (int b : below) {
        // A bone with no surface of its own (a hand inside a fist) takes the widest.
        double r = (reach[std::size_t(b)] > 0 ? reach[std::size_t(b)] : widest) * 1.1 + 0.3;
        // Where a limb is cut off the body, it's no fatter than it is there: its reach can't
        // swell into the body beside it. (Not the head or the chest, which widen past the
        // neck and waist.)
        if (b == bone && !ring.empty() && joint != rig::Joint::Neck && joint != rig::Joint::Waist)
            r = std::min(r, 1.4 * cut.limbRadius + 0.5);
        for (const auto& s : segs[std::size_t(b)])
            cut.finish.reach.push_back({s[0], s[1], r});
    }
    return cut;
}

}  // namespace

std::vector<rig::Joint> cuttable()
{
    using J = rig::Joint;
    return {J::Neck,      J::Waist,      J::LeftShoulder, J::RightShoulder, J::LeftHip,    J::RightHip,
            J::LeftElbow, J::RightElbow, J::LeftKnee,     J::RightKnee,     J::LeftWrist,  J::RightWrist,
            J::LeftAnkle, J::RightAnkle};
}

Cut plan(const Mesh& figure, const rig::Points& points, rig::Joint joint)
{
    return planOne(figure, points, joint, rig::nearestBones(figure, points));
}

std::vector<Cut> plan(const Mesh& figure, const rig::Points& points, const std::vector<rig::Joint>& joints)
{
    const std::vector<int> nearest = rig::nearestBones(figure, points);
    std::vector<Cut> out;
    for (rig::Joint j : joints)
        out.push_back(planOne(figure, points, j, nearest));
    return out;
}

void check(const LimbCutFinish& f)
{
    if (f.joint < 0 || f.joint >= kRigJointCount)
        throw std::invalid_argument("The cut isn't at a joint.");
    for (double v : {f.point[0], f.point[1], f.point[2], f.normal[0], f.normal[1], f.normal[2], f.gap})
        if (!std::isfinite(v))
            throw std::invalid_argument("The cut's place isn't a number.");
    if (length(f.normal) < 1e-9)
        throw std::invalid_argument("The cut has no direction.");
    if (f.reach.empty())
        throw std::invalid_argument("The cut has no limb to take off.");
    for (const LimbRod& r : f.reach)
        if (!(r.radius > 0.0) || !std::isfinite(r.radius))
            throw std::invalid_argument("The limb's reach has no size.");
    if (f.gap < 0.0 || f.gap > 20.0)
        throw std::invalid_argument("The gap at a cut must be from 0 to 20 mm.");
}

std::string describe(const LimbCutFinish& f)
{
    const auto joint = rig::Joint(std::clamp(f.joint, 0, kRigJointCount - 1));
    std::string at = rig::title(joint);
    std::transform(at.begin(), at.end(), at.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (!f.limb)
        return "the rest, cut at the " + at;
    std::string limb = rig::pieceName(rig::boneAndBelow(rig::boneOf(joint)));
    std::transform(limb.begin(), limb.end(), limb.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return "the " + limb + ", cut at the " + at;
}

LimbCutFinish moved(const LimbCutFinish& f, const Transform& t)
{
    LimbCutFinish out = f;
    out.point = t.apply(Vec3{f.point[0], f.point[1], f.point[2]});
    out.normal = unit(t.applyDirection(Vec3{f.normal[0], f.normal[1], f.normal[2]}));
    for (LimbRod& r : out.reach) {
        r.a = t.apply(Vec3{r.a[0], r.a[1], r.a[2]});
        r.b = t.apply(Vec3{r.b[0], r.b[1], r.b[2]});
    }
    return out;
}

Vec3 limbFace(const LimbCutFinish& f)
{
    return add(f.point, mul(unit(f.normal), 0.5 * f.gap));
}

Vec3 restFace(const LimbCutFinish& f)
{
    return sub(f.point, mul(unit(f.normal), 0.5 * f.gap));
}

Mesh rod(const Vec3& a, const Vec3& b, double radius, int around)
{
    Vec3 axis = sub(b, a);
    const double len = length(axis);
    axis = unit(axis, {0, 0, 1});
    const Vec3 x = unit(cross(std::abs(axis[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}, axis));
    const Vec3 y = cross(axis, x);
    constexpr int rings = 6;  // per end, the equator included
    Mesh m;
    const auto point = [&m](const Vec3& p) {
        for (int k = 0; k < 3; ++k)
            m.positions.push_back(float(p[std::size_t(k)]));
        return std::uint32_t(m.positions.size() / 3 - 1);
    };
    const std::uint32_t bottom = point(sub(a, mul(axis, radius)));
    std::vector<std::uint32_t> firstOfRing;
    // Rings from the bottom up: the bottom end's (below a), then the top end's (above b).
    for (int end = 0; end < 2; ++end)
        for (int k = 0; k < rings; ++k) {
            const double phi = end == 0 ? -std::numbers::pi / 2 + std::numbers::pi / 2 * (k + 1) / rings
                                        : std::numbers::pi / 2 * k / rings;
            const Vec3 c = add(end == 0 ? a : b, mul(axis, radius * std::sin(phi)));
            const double rr = radius * std::cos(phi);
            firstOfRing.push_back(std::uint32_t(m.positions.size() / 3));
            for (int i = 0; i < around; ++i) {
                const double t = 2 * std::numbers::pi * i / around;
                point(add(c, add(mul(x, rr * std::cos(t)), mul(y, rr * std::sin(t)))));
            }
        }
    const std::uint32_t top = point(add(b, mul(axis, radius)));
    (void)len;
    const auto at = [&](std::size_t ring, int i) { return firstOfRing[ring] + std::uint32_t(i % around); };
    for (int i = 0; i < around; ++i)
        m.indices.insert(m.indices.end(), {bottom, at(0, i + 1), at(0, i)});
    for (std::size_t r = 0; r + 1 < firstOfRing.size(); ++r)
        for (int i = 0; i < around; ++i) {
            m.indices.insert(m.indices.end(), {at(r, i), at(r, i + 1), at(r + 1, i + 1)});
            m.indices.insert(m.indices.end(), {at(r, i), at(r + 1, i + 1), at(r + 1, i)});
        }
    const std::size_t last = firstOfRing.size() - 1;
    for (int i = 0; i < around; ++i)
        m.indices.insert(m.indices.end(), {at(last, i), at(last, i + 1), top});
    return m;
}

MeshSolid apply(const MeshSolid& part, const LimbCutFinish& f)
{
    check(f);
    std::vector<MeshSolid> rods;
    for (const LimbRod& r : f.reach) {
        const Mesh m = rod(r.a, r.b, r.radius);
        rods.push_back(meshkernel::fromTriangles(m.positions, m.indices));
    }
    const Vec3 face = f.limb ? limbFace(f) : restFace(f);
    const MeshSolid limbReach = meshkernel::trimmed(meshkernel::merged(rods), face, unit(f.normal));
    if (f.limb)
        return meshkernel::intersected({part, limbReach});
    return meshkernel::subtracted(part, {limbReach});
}


SocketFinish ballFor(double limbRadius)
{
    SocketFinish f;
    f.kind = SocketKind::Ball;
    f.size = std::clamp(std::round(limbRadius * 2.0) / 2.0, 2.0, 20.0);  // as wide as the limb is thick
    f.snap = true;
    f.grip = 10.0;
    return f;
}

HingeFinish hingeFor(double limbRadius, HingeKind kind)
{
    const auto half = [](double v) { return std::round(v * 2.0) / 2.0; };
    HingeFinish f;
    f.kind = kind;
    if (kind == HingeKind::Swivel) {
        f.width = std::clamp(half(limbRadius), 2.0, 15.0);  // the peg's diameter
        f.barrel = std::clamp(half(limbRadius * 1.2), 2.0, 20.0);  // how far it goes in
    } else {
        f.width = std::clamp(half(limbRadius * 0.8), 1.5, 15.0);   // the tab's thickness
        f.barrel = std::clamp(half(limbRadius * 1.4), 3.0, 25.0);  // its rounded end
        f.pinSize = std::clamp(half(limbRadius * 0.35), 1.0, 4.0);
    }
    return f;
}

double gapFor(const Choice& c)
{
    switch (c.kind) {
    case JointKind::Ball: return std::clamp(0.12 * c.ball.size, 0.4, 2.0);
    case JointKind::Hinge: return c.hinge.kind == HingeKind::Double ? c.hinge.gap : 0.0;
    default: return 0.0;
    }
}

Mesh ballOnNeck(const SocketFinish& f, const Vec3& spot, const Vec3& normalOutOfLimb, const Vec3& restFace)
{
    const Vec3 into = mul(unit(normalOutOfLimb), -1.0);  // from the opening into the limb
    const socket::BallSeat seat = socket::ballSeat(f);
    const double r = f.size / 2.0;
    const Vec3 centre = add(spot, mul(into, seat.centreDepth));
    // The neck: thinner than the opening, so the ball can tilt; a millimetre into the other piece.
    const double neck = std::max(0.4, std::min(0.3 * f.size, 0.42 * seat.opening));
    const Vec3 start = sub(restFace, mul(into, 1.0));
    const Mesh ball = rod(sub(centre, mul(into, 1e-3)), add(centre, mul(into, 1e-3)), r, 28);
    const Mesh stem = rod(start, centre, neck, 20);
    std::vector<float> pos;
    std::vector<std::uint32_t> idx;
    meshkernel::triangles(meshkernel::merged({meshkernel::fromTriangles(ball.positions, ball.indices),
                                              meshkernel::fromTriangles(stem.positions, stem.indices)}),
                          pos, idx);
    Mesh out;
    out.positions = std::move(pos);
    out.indices = std::move(idx);
    return out;
}

Made cutIntoParts(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points,
                  std::vector<Choice> choices, std::uint32_t firstLink)
{
    if (!doc.find(figure) || !doc.isTopLevel(figure))
        throw std::invalid_argument("Choose a figure on the plate to cut.");
    if (choices.empty())
        throw std::invalid_argument("Choose where to cut first.");
    // From the middle out, so each cut finds its limb still on one piece.
    const std::vector<rig::Joint> order = cuttable();
    std::sort(choices.begin(), choices.end(), [&](const Choice& a, const Choice& b) {
        return std::find(order.begin(), order.end(), a.joint) < std::find(order.begin(), order.end(), b.joint);
    });
    // The figure's surface on the plate, to plan the cuts on.
    Mesh surface = *doc.mesh(figure);
    const Transform toPlate = doc.worldTransform(figure);
    for (std::size_t i = 0; i + 2 < surface.positions.size(); i += 3) {
        const Vec3 p = toPlate.apply(Vec3{surface.positions[i], surface.positions[i + 1], surface.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            surface.positions[i + k] = float(p[k]);
    }
    std::vector<rig::Joint> joints;
    for (const Choice& c : choices)
        joints.push_back(c.joint);
    std::vector<Cut> cuts = plan(surface, points, joints);

    std::map<int, ObjectId> owner;  // each bone's piece
    for (int b = 0; b < rig::kBoneCount; ++b)
        owner[b] = figure;
    const auto bonesOf = [&](ObjectId id) {
        std::vector<int> out;
        for (const auto& [b, o] : owner)
            if (o == id)
                out.push_back(b);
        return out;
    };
    const auto nameOf = [&](ObjectId id) { return rig::pieceName(bonesOf(id)); };
    const auto handOver = [&](ObjectId from, ObjectId to) {
        for (auto& [b, o] : owner)
            if (o == from)
                o = to;
    };

    Made made;
    stack.beginMacro("Cut into Parts");
    try {
        // 1. The cuts.
        for (std::size_t i = 0; i < choices.size(); ++i) {
            Cut& cut = cuts[i];
            cut.finish.gap = gapFor(choices[i]);
            const int bone = rig::boneOf(choices[i].joint);
            const ObjectId piece = owner[bone];
            LimbCutFinish rest = cut.finish, limb = cut.finish;
            rest.limb = false;
            limb.limb = true;
            BooleanSpec restSpec = limbCutSpec(doc, piece, rest);
            BooleanSpec limbSpec = limbCutSpec(doc, piece, limb);
            const auto restShape = doc.previewResult(restSpec), limbShape = doc.previewResult(limbSpec);
            std::string at = rig::title(choices[i].joint);
            if (limbShape.empty || restShape.empty)
                throw std::invalid_argument("The cut at the " + at +
                                            " doesn't cut anything off: check the skeleton's balls there.");
            const std::vector<Body> copies = duplicateTree(doc, piece);
            for (const Body& c : copies)
                stack.execute(std::make_unique<AddBodyCommand>(c));
            // The copy is the same shape: handed over, so it isn't worked out again from the start.
            doc.seedSolid(copies.back().id, doc.meshSolid(piece));
            limbSpec.operands = {copies.back().id};
            Body r, l;
            r.id = doc.allocateId();
            l.id = doc.allocateId();
            r.boolean = std::move(restSpec);
            l.boolean = std::move(limbSpec);
            // The limb takes its bones from the piece; the rest keeps the others.
            const std::vector<int> below = rig::boneAndBelow(bone);
            for (int b : below)
                if (owner[b] == piece)
                    owner[b] = l.id;
            handOver(piece, r.id);
            r.name = nameOf(r.id);
            l.name = nameOf(l.id);
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
            stack.execute(std::make_unique<CombineCommand>(std::move(l)));
        }
        // 2. The joints, on the finished pieces.
        std::uint32_t link = firstLink;
        for (std::size_t i = 0; i < choices.size(); ++i) {
            const Choice& c = choices[i];
            if (c.kind == JointKind::Cut)
                continue;
            const int bone = rig::boneOf(c.joint);
            const ObjectId a = owner[rig::parentBone(bone)], b = owner[bone];
            const Vec3 n = unit(cuts[i].finish.normal);
            const Vec3 restAt = restFace(cuts[i].finish), limbAt = limbFace(cuts[i].finish);
            const auto step = [&](BooleanSpec spec, ObjectId of) {
                Body s;
                s.id = doc.allocateId();
                s.name = nameOf(of);
                s.boolean = std::move(spec);
                const ObjectId id = s.id;
                stack.execute(std::make_unique<CombineCommand>(std::move(s)));
                handOver(of, id);
                return id;
            };
            if (c.kind == JointKind::Ball) {
                // The socket in the limb, the ball on its neck merged onto the other piece.
                step(socketSpec(doc, b, {limbAt, mul(n, -1.0)}, c.ball), b);
                Body ball = hinge::pieceBody(ballOnNeck(c.ball, limbAt, mul(n, -1.0), restAt), "Ball");
                ball.id = doc.allocateId();
                const ObjectId ballId = ball.id;
                stack.execute(std::make_unique<AddBodyCommand>(std::move(ball)));
                BooleanSpec merge;
                merge.op = BooleanOp::Merge;
                merge.operands = {a, ballId};
                const auto centre = doc.previewResult(merge).bounds.center();
                merge.pivot = {centre[0], centre[1], centre[2]};
                step(std::move(merge), a);
            } else {
                HingeFinish f = c.hinge;
                f.half = HingeHalf::Fork;
                f.link = link++;
                // The pin square to the limb and to the front, so the limb bends forwards and back
                // (an arm hanging down: the pin runs left to right; held out to the side: up and down).
                HingeFinish straight = f;
                straight.turn = 0.0;
                const Vec3 a0 = hinge::pinAxis(straight, n);
                Vec3 want = cross(n, Vec3{0, -1, 0});
                if (length(want) < 0.2)
                    want = sub(Vec3{1, 0, 0}, mul(n, n[0]));
                want = unit(want);
                // The slot and pin hole just through the limb, not on into the rest of the figure.
                const double r = cuts[i].limbRadius;
                if (f.reach <= 0.0 && f.kind != HingeKind::Swivel)
                    f.reach = std::max(1.25 * r, 0.5 * f.width + 1.0);
                f.turn = std::atan2(dot(cross(a0, want), n), dot(a0, want)) * 180.0 / std::numbers::pi;
                hinge::check(f);
                const hinge::Half other = hinge::otherHalf(f, restAt, n);
                step(hingeSpec(doc, a, {restAt, n}, f), a);
                step(hingeSpec(doc, b, {other.point, other.normal}, other.finish), b);
                const double span = std::max(0.95 * r, 0.5 * f.width + 0.5);
                for (const hinge::Piece& piece : hinge::pieces(f, restAt, n, false, {span, span})) {
                    Body extra = hinge::pieceBody(piece.mesh, piece.name);
                    extra.id = doc.allocateId();
                    extra.hardware = true;
                    made.extras.push_back(extra.id);
                    stack.execute(std::make_unique<AddBodyCommand>(std::move(extra)));
                }
            }
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    for (const auto& [b, o] : owner)
        if (std::find(made.pieces.begin(), made.pieces.end(), o) == made.pieces.end())
            made.pieces.push_back(o);
    return made;
}

}  // namespace limbcut

BooleanSpec limbCutSpec(const Document& doc, ObjectId target, const LimbCutFinish& inPlate)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    BooleanSpec spec;
    spec.op = BooleanOp::LimbCut;
    spec.operands = {target};
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    spec.limbCut = limbcut::moved(inPlate, toStep);
    limbcut::check(*spec.limbCut);
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

}  // namespace forge
