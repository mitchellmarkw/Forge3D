// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Hand.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Port.h"
#include "forge/core/Socket.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace forge {

namespace {

constexpr double kPi = std::numbers::pi;

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 unit(const Vec3& a)
{
    const double l = norm(a);
    return l > 1e-12 ? mul(a, 1.0 / l) : Vec3{0, 0, 1};
}
double rad(double degrees) { return degrees * kPi / 180.0; }
// `v` turned `angle` radians round unit axis `k`.
Vec3 turn(const Vec3& v, const Vec3& k, double angle)
{
    const double c = std::cos(angle), s = std::sin(angle);
    return add(add(mul(v, c), mul(cross(k, v), s)), mul(k, dot(k, v) * (1.0 - c)));
}

// Signed distances (negative inside), in hand units (the hand's length is 1).
double capsule(const Vec3& p, const Vec3& a, const Vec3& b, double r)
{
    const Vec3 ab = sub(b, a);
    const double t = std::clamp(dot(sub(p, a), ab) / std::max(dot(ab, ab), 1e-12), 0.0, 1.0);
    return norm(sub(p, add(a, mul(ab, t)))) - r;
}

double roundBox(const Vec3& p, const Vec3& centre, const Vec3& half, double r)
{
    Vec3 q;
    for (int k = 0; k < 3; ++k)
        q[std::size_t(k)] = std::abs(p[std::size_t(k)] - centre[std::size_t(k)]) - (half[std::size_t(k)] - r);
    const Vec3 out{std::max(q[0], 0.0), std::max(q[1], 0.0), std::max(q[2], 0.0)};
    return norm(out) + std::min(std::max({q[0], q[1], q[2]}), 0.0) - r;
}

// A smooth join (k: how far the blend reaches).
double smin(double a, double b, double k)
{
    const double h = std::max(k - std::abs(a - b), 0.0) / k;
    return std::min(a, b) - h * h * k * 0.25;
}

// From the segment a-b to the line through `c` along unit `u`: the nearest it comes.
double segmentToLine(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& u)
{
    double best = 1e300;
    for (int i = 0; i <= 12; ++i) {
        const Vec3 p = add(a, mul(sub(b, a), i / 12.0));
        const Vec3 d = sub(p, c);
        best = std::min(best, norm(sub(d, mul(u, dot(d, u)))));
    }
    return best;
}

struct Rod {
    Vec3 a, b;
    double r;
};

// The hand's shape, in hand units, as a right hand.
struct Shape {
    Vec3 palmCentre, palmHalf;
    double palmRound;
    Vec3 wristCentre, wristHalf;
    double wristRound;
    std::vector<Rod> fingers;  // the four fingers' segments
    std::vector<Rod> thumb;    // the thumb, and the ball of it
    bool grip = false;
    Vec3 gripMiddle{0, 0, 0}, gripAlong{1, 0, 0};

    double distance(const Vec3& p) const
    {
        double d = roundBox(p, palmCentre, palmHalf, palmRound);
        d = smin(d, roundBox(p, wristCentre, wristHalf, wristRound), 0.06);
        for (const Rod& r : thumb)
            d = smin(d, capsule(p, r.a, r.b, r.r), 0.04);
        for (const Rod& r : fingers)
            d = smin(d, capsule(p, r.a, r.b, r.r), 0.03);
        return std::max(d, -p[2]);  // the wrist cut flat
    }
};

struct FingerSpec {
    double x, z, length, radius, spread;  // spread: degrees towards the thumb (+X)
};

// Index, middle, ring, little, for a right hand (the thumb towards +X).
constexpr std::array<FingerSpec, 4> kFingers{{{0.15, 0.47, 0.47, 0.040, 4.0},
                                              {0.05, 0.48, 0.52, 0.042, 0.0},
                                              {-0.05, 0.47, 0.49, 0.039, -4.0},
                                              {-0.15, 0.44, 0.38, 0.033, -9.0}}};
constexpr std::array<double, 3> kSegment{0.47, 0.30, 0.23};  // of a finger's length
constexpr std::array<double, 3> kMaxBend{95.0, 110.0, 80.0};

// A finger's joints, bent by these angles (degrees) towards the palm.
std::vector<Vec3> bent(const FingerSpec& f, double spreadExtra, const std::array<double, 3>& bends)
{
    const Vec3 start{f.x, 0.0, f.z};
    const Vec3 dir0 = turn({0, 0, 1}, {0, 1, 0}, -rad(f.spread + spreadExtra));  // +spread towards +X
    const Vec3 k = unit(cross(dir0, {0, -1, 0}));  // turning +: towards the palm side
    std::vector<Vec3> points{start};
    Vec3 dir = dir0;
    for (int j = 0; j < 3; ++j) {
        dir = turn(dir, k, rad(bends[std::size_t(j)]));
        points.push_back(add(points.back(), mul(dir, f.length * kSegment[std::size_t(j)])));
    }
    return points;
}

// A finger wrapped round a rod (middle c, along u, radius R): each joint bent as far as
// it goes before the finger would touch it.
std::vector<Vec3> wrapped(const FingerSpec& f, double radius, const Vec3& c, const Vec3& u, double R)
{
    const Vec3 start{f.x, 0.0, f.z};
    const Vec3 dir0 = turn({0, 0, 1}, {0, 1, 0}, -rad(f.spread));
    const Vec3 k = unit(cross(dir0, {0, -1, 0}));
    std::vector<Vec3> points{start};
    Vec3 dir = dir0;
    for (int j = 0; j < 3; ++j) {
        const double len = f.length * kSegment[std::size_t(j)];
        const double r = radius * (1.0 - 0.06 * j);
        double angle = 0.0;
        for (double a = 2.0; a <= kMaxBend[std::size_t(j)]; a += 2.0) {
            const Vec3 d = turn(dir, k, rad(a));
            if (segmentToLine(points.back(), add(points.back(), mul(d, len)), c, u) < R + r)
                break;
            angle = a;
        }
        dir = turn(dir, k, rad(angle));
        points.push_back(add(points.back(), mul(dir, len)));
    }
    return points;
}

void addFinger(Shape& s, const std::vector<Vec3>& points, double radius)
{
    for (std::size_t j = 0; j + 1 < points.size(); ++j)
        s.fingers.push_back({points[j], points[j + 1], radius * (1.0 - 0.06 * double(j))});
}

Shape shapeOf(const HandSettings& set, double socketR, double socketD, double wall)
{
    const double L = set.length;
    const double full = set.fullness;
    Shape s;
    s.palmCentre = {0.0, 0.0, 0.28};
    s.palmHalf = {0.21, 0.075 * full, 0.20};
    s.palmRound = std::min(0.06, 0.07 * full);
    // The wrist: room for the socket, with a wall round it.
    const double needR = socketR > 0 ? (socketR + wall) / L : 0.0;
    const double needD = socketD > 0 ? (socketD + wall) / L : 0.0;
    s.wristHalf = {std::max(0.15, needR), std::max(0.065 * full, needR), 0.5 * std::max(0.16, needD + 0.02)};
    s.wristCentre = {0.0, 0.0, s.wristHalf[2] - 0.04};
    s.wristRound = std::min({0.05, s.wristHalf[0] * 0.9, s.wristHalf[1] * 0.9});
    // The palm starts above the socket.
    const double palmBottom = std::max(0.08, needD * 0.75);
    s.palmCentre[2] = palmBottom + 0.20;
    const double lift = palmBottom - 0.08;
    const double front = -s.palmHalf[1];  // the palm's face (towards -Y)

    std::array<FingerSpec, 4> fingers = kFingers;
    for (FingerSpec& f : fingers) {
        f.z += lift;
        f.radius *= full;
    }
    // The rod the fingers wrap round: a gripped handle, or a thin core for a fist.
    const auto core = [&](double R) {
        const double a = rad(set.gripAngle);
        const Vec3 along = unit({-std::cos(a), 0.0, -std::sin(a)});  // from the thumb to the little finger
        const Vec3 middle{0.0, front - R, 0.40 + lift};
        return std::pair<Vec3, Vec3>{middle, along};
    };

    switch (set.pose) {
    case HandPose::Open:
        for (int i = 0; i < 4; ++i) {
            const double extra = std::array<double, 4>{4.0, 0.0, -3.0, -6.0}[std::size_t(i)];
            addFinger(s, bent(fingers[std::size_t(i)], extra, {0.0, 3.0, 3.0}), fingers[std::size_t(i)].radius);
        }
        break;
    case HandPose::Relaxed:
        for (int i = 0; i < 4; ++i)
            addFinger(s, bent(fingers[std::size_t(i)], 0.0, {15.0 + 5.0 * i, 25.0 + 6.0 * i, 12.0 + 3.0 * i}),
                      fingers[std::size_t(i)].radius);
        break;
    case HandPose::Fist:
    case HandPose::Grip:
    case HandPose::Point: {
        const double R = set.pose == HandPose::Grip ? (set.handle / 2.0 + set.fit) / L : 0.02;
        const auto [middle, along] = core(R);
        for (int i = 0; i < 4; ++i) {
            const FingerSpec& f = fingers[std::size_t(i)];
            if (set.pose == HandPose::Point && i == 0)
                addFinger(s, bent(f, 0.0, {0.0, 2.0, 2.0}), f.radius);
            else
                addFinger(s, wrapped(f, f.radius, middle, along, R), f.radius);
        }
        if (set.pose == HandPose::Grip) {
            s.grip = true;
            s.gripMiddle = middle;
            s.gripAlong = along;
        }
        break;
    }
    }

    // The thumb, from its root low on the palm's thumb side.
    const double tr = 0.052 * full;
    const Vec3 root{0.16, -0.02, 0.10 + lift};
    s.thumb.push_back({{0.11, -0.02, 0.10 + lift}, {0.14, -0.035, 0.27 + lift}, 0.075 * full});  // the ball of it
    std::vector<Vec3> joints{root};
    if (set.pose == HandPose::Open || set.pose == HandPose::Relaxed) {
        const bool open = set.pose == HandPose::Open;
        const std::array<Vec3, 3> dirs = open ? std::array<Vec3, 3>{unit({0.55, -0.25, 0.80}), unit({0.60, -0.20, 0.77}),
                                                                    unit({0.55, -0.15, 0.82})}
                                              : std::array<Vec3, 3>{unit({0.35, -0.50, 0.79}), unit({0.10, -0.60, 0.79}),
                                                                    unit({-0.10, -0.60, 0.79})};
        const std::array<double, 3> lengths{0.20, 0.16, 0.13};
        for (int j = 0; j < 3; ++j)
            joints.push_back(add(joints.back(), mul(dirs[std::size_t(j)], lengths[std::size_t(j)])));
    } else {
        // Across the front of the curled fingers: the tip over the index finger's front.
        Vec3 frontmost{0.0, 1e300, 0.0};
        for (std::size_t j = 0; j < 3; ++j) {
            const Rod& r = s.fingers[j];  // the index finger's segments come first
            for (const Vec3& p : {r.a, r.b})
                if (j > 0 && p[1] < frontmost[1])
                    frontmost = p;
        }
        if (set.pose == HandPose::Point)  // the index finger is straight: over the middle finger
            for (std::size_t j = 4; j < 6; ++j)
                for (const Vec3& p : {s.fingers[j].a, s.fingers[j].b})
                    if (p[1] < frontmost[1] || frontmost[1] > 1e299)
                        frontmost = p;
        const double y = frontmost[1] - fingers[0].radius - tr * 0.8;
        const double z = std::max(frontmost[2], 0.25 + lift);
        joints.push_back({0.20, std::min(-0.16, y * 0.6), std::min(0.27 + lift, z)});
        joints.push_back({0.12, y, z - 0.01});
        joints.push_back({frontmost[0] - 0.03, y, z});
    }
    for (std::size_t j = 0; j + 1 < joints.size(); ++j)
        s.thumb.push_back({joints[j], joints[j + 1], tr * (1.0 - 0.06 * double(j))});
    return s;
}

}  // namespace

namespace hand {

const char* title(HandPose pose)
{
    switch (pose) {
    case HandPose::Fist: return "Fist";
    case HandPose::Grip: return "Gripping";
    case HandPose::Open: return "Open";
    case HandPose::Relaxed: return "Relaxed";
    case HandPose::Point: return "Pointing";
    }
    return "Hand";
}

void check(const HandSettings& s)
{
    if (!(s.length >= 5.0 && s.length <= 300.0))
        throw std::invalid_argument("A hand must be from 5 to 300 mm long.");
    if (!(s.fullness >= 0.8 && s.fullness <= 2.0))
        throw std::invalid_argument("A hand's fullness must be from 80% to 200%.");
    if (s.pose == HandPose::Grip) {
        if (!(s.handle >= 0.3 && s.handle <= s.length * 0.4))
            throw std::invalid_argument("The handle must be from 0.3 mm to 40% of the hand's length across.");
        if (!(s.fit >= 0.0 && s.fit <= 1.0))
            throw std::invalid_argument("The handle's fit must be from 0 to 1 mm.");
        if (!(s.gripAngle >= -45.0 && s.gripAngle <= 45.0))
            throw std::invalid_argument("The handle's angle must be from -45° to 45°.");
    }
}

double lengthAtScale(double scale)
{
    return kLifeSize / scale;
}

Made make(const HandSettings& s, double socketR, double socketD, double wall)
{
    check(s);
    const Shape shape = shapeOf(s, socketR, socketD, wall);
    const double L = s.length;
    const double mirror = s.left ? -1.0 : 1.0;
    const auto field = [&](const Vec3& p) {
        return -shape.distance({mirror * p[0] / L, p[1] / L, p[2] / L}) * L;
    };
    Bounds box;
    box.min = {float(-0.45 * L), float(-0.75 * L), float(-0.01 * L)};
    box.max = {float(0.45 * L), float(0.35 * L), float(1.15 * L)};
    box.valid = true;
    const MeshSolid solid = meshkernel::levelSet(field, box, L / 75.0, 0.0);
    if (meshkernel::isEmpty(solid))
        throw std::invalid_argument("The hand came out empty.");
    Made out;
    meshkernel::triangles(solid, out.mesh.positions, out.mesh.indices);
    out.grip = shape.grip;
    out.gripMiddle = {mirror * shape.gripMiddle[0] * L, shape.gripMiddle[1] * L, shape.gripMiddle[2] * L};
    out.gripAlong = unit({mirror * shape.gripAlong[0], shape.gripAlong[1], shape.gripAlong[2]});
    out.wristHalfWidth = shape.wristHalf[0] * L;
    out.wristHalfThickness = shape.wristHalf[1] * L;
    return out;
}

double socketRadius(const SocketFinish& f)
{
    if (f.kind == SocketKind::Ball)
        return socket::ballSeat(f).radius;
    const int sides = f.shape == PegShape::Square    ? 4
                      : f.shape == PegShape::Hexagon ? 6
                      : f.shape == PegShape::Octagon ? 8
                                                     : 0;
    const double half = f.size / 2.0 + f.fit;
    return sides == 0 ? half : half / std::cos(kPi / sides);
}

double socketDepth(const SocketFinish& f)
{
    if (f.kind == SocketKind::Ball) {
        const auto seat = socket::ballSeat(f);
        return seat.centreDepth + seat.radius;
    }
    return f.depth + f.fit;
}

std::string nameOf(const HandSettings& s)
{
    std::string pose = title(s.pose);
    for (char& c : pose)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return std::string(s.left ? "Left" : "Right") + " hand (" + pose + ")";
}

std::vector<ObjectId> addHands(CommandStack& stack, Document& doc, const HandSettings& settings,
                               const std::optional<SocketFinish>& wrist, bool pair, double x, double y)
{
    check(settings);
    if (wrist)
        socket::check(*wrist);
    const double sr = wrist ? socketRadius(*wrist) : 0.0, sd = wrist ? socketDepth(*wrist) : 0.0;
    std::vector<HandSettings> hands;
    if (pair) {
        HandSettings l = settings, r = settings;
        l.left = true;
        r.left = false;
        hands = {r, l};
    } else {
        hands = {settings};
    }
    // Everything worked out before anything changes.
    struct Plan {
        Body body;
        std::optional<BooleanSpec> socketStep;
        std::optional<PortFinish> grip;
        Vec3 at;
    };
    std::vector<Plan> plans;
    const double gap = settings.length * 0.9;
    for (std::size_t i = 0; i < hands.size(); ++i) {
        const HandSettings& h = hands[i];
        const Made made = make(h, sr, sd);
        // Pairs: the right hand on the left of the plate as you look (it's the figure's right).
        const Vec3 at{x + (pair ? (i == 0 ? -gap / 2.0 : gap / 2.0) : 0.0), y, 0.0};
        Mesh m = made.mesh;
        for (std::size_t v = 0; v < m.vertexCount(); ++v) {
            m.positions[3 * v] += float(at[0]);
            m.positions[3 * v + 1] += float(at[1]);
        }
        Plan plan;
        plan.body = hinge::pieceBody(m, nameOf(h));
        plan.at = at;
        if (made.grip) {
            PortFinish g;
            g.kind = PortKind::Grip;
            g.size = h.handle;
            g.fit = h.fit;
            const double reach = h.length;
            const Vec3 middle = add(made.gripMiddle, at);
            g.point = add(middle, mul(made.gripAlong, -reach));
            g.direction = mul(made.gripAlong, -1.0);
            g.hasExit = true;
            g.exit = add(middle, mul(made.gripAlong, reach));
            plan.grip = g;
        }
        plans.push_back(std::move(plan));
    }
    std::vector<ObjectId> made;
    stack.beginMacro(pair ? "Make a pair of hands" : "Make a hand");
    try {
        for (Plan& plan : plans) {
            plan.body.id = doc.allocateId();
            ObjectId top = plan.body.id;
            const std::string name = plan.body.name;
            stack.execute(std::make_unique<AddBodyCommand>(plan.body));
            if (wrist) {
                Body r;
                r.id = doc.allocateId();
                r.name = name;
                r.boolean = socketSpec(doc, top, emboss::SurfacePoint{plan.at, {0.0, 0.0, -1.0}}, *wrist);
                top = r.id;
                stack.execute(std::make_unique<CombineCommand>(std::move(r)));
            }
            if (plan.grip) {
                Body r;
                r.id = doc.allocateId();
                r.name = name;
                BooleanSpec spec;
                spec.op = BooleanOp::Port;
                spec.operands = {top};
                spec.port = *plan.grip;
                r.boolean = std::move(spec);
                top = r.id;
                stack.execute(std::make_unique<CombineCommand>(std::move(r)));
            }
            made.push_back(top);
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

}  // namespace hand
}  // namespace forge
