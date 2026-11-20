// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Mannequin.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace forge::mannequin {

namespace {

constexpr const char* kKeys[kMJointCount] = {"neck",          "waist",     "leftShoulder", "rightShoulder",
                                             "leftElbow",     "rightElbow", "leftHip",     "rightHip",
                                             "leftKnee",      "rightKnee"};
constexpr const char* kTitles[kMJointCount] = {"Neck",        "Waist",       "Left shoulder", "Right shoulder",
                                               "Left elbow",  "Right elbow", "Left hip",      "Right hip",
                                               "Left knee",   "Right knee"};

int index(MJoint j) { return int(j); }

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 scale(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 0 ? scale(a, 1.0 / l) : Vec3{0, 0, 1};
}

Transform T(double x, double y, double z) { return Transform::translation({x, y, z}); }
Transform Rx(double a) { return Transform::rotationXYZ(a, 0, 0); }
Transform Ry(double a) { return Transform::rotationXYZ(0, a, 0); }
Transform Rz(double a) { return Transform::rotationXYZ(0, 0, a); }

// The figure's measurements, from its height and heads.
struct Body {
    double H, R, top;
    double neckZ, shoulderZ, waistZ, hipZ, kneeZ, ankleZ;
    double shoulderX, hipX;
    double upperArm, forearm, hand, thigh, shin;
};

Body measure(const Mannequin& m)
{
    Body b;
    b.H = m.height / m.heads;
    b.R = m.height - b.H;
    b.top = m.height;
    b.neckZ = b.top - b.H;
    b.shoulderZ = b.top - b.H - 0.05 * b.R;
    b.waistZ = b.top - b.H - 0.29 * b.R;
    b.hipZ = b.top - b.H - 0.40 * b.R;
    b.kneeZ = 0.28 * b.R;
    b.ankleZ = 0.05 * b.R;
    b.shoulderX = 0.9 * b.H;
    b.hipX = 0.42 * b.H;
    b.upperArm = 0.20 * b.R;
    b.forearm = 0.17 * b.R;
    b.hand = 0.09 * b.R;
    b.thigh = b.hipZ - b.kneeZ;
    b.shin = b.kneeZ - b.ankleZ;
    return b;
}

// The foot, in the knee's frame.
Vec3 footCentre(const Body& b) { return {0, -0.3 * b.H, -b.shin - 0.4 * b.ankleZ}; }
Vec3 footRadii(const Body& b) { return {0.2 * b.H, 0.55 * b.H, 0.6 * b.ankleZ}; }

// Each part's frame on the plate, as posed.
struct Frames {
    Transform root, waist, neck, shoulder[2], elbow[2], hip[2], knee[2];
};

Frames frames(const Mannequin& m, const Body& b, double lift)
{
    const auto& p = m.pose;
    Frames f;
    f.root = T(m.x, m.y, lift) * Rz(m.turn);
    const auto& w = p[index(MJoint::Waist)];
    f.waist = f.root * T(0, 0, b.waistZ) * Rz(w[1]) * Rx(w[0]);
    const auto& n = p[index(MJoint::Neck)];
    f.neck = f.waist * T(0, 0, b.neckZ - b.waistZ) * Rz(n[1]) * Rx(n[0]);
    for (int side = 0; side < 2; ++side) {
        const double sign = side == 0 ? 1.0 : -1.0;  // left is +X (the figure faces -Y)
        const auto& s = p[index(side == 0 ? MJoint::LeftShoulder : MJoint::RightShoulder)];
        f.shoulder[side] = f.waist * T(sign * b.shoulderX, 0, b.shoulderZ - b.waistZ) * Rx(-s[0]) * Ry(-sign * s[1]);
        const auto& e = p[index(side == 0 ? MJoint::LeftElbow : MJoint::RightElbow)];
        f.elbow[side] = f.shoulder[side] * T(0, 0, -b.upperArm) * Rx(-e[0]);
        const auto& h = p[index(side == 0 ? MJoint::LeftHip : MJoint::RightHip)];
        f.hip[side] = f.root * T(sign * b.hipX, 0, b.hipZ) * Rx(-h[0]) * Ry(-sign * h[1]);
        const auto& k = p[index(side == 0 ? MJoint::LeftKnee : MJoint::RightKnee)];
        f.knee[side] = f.hip[side] * T(0, 0, -b.thigh) * Rx(k[0]);
    }
    return f;
}

// Posed, standing on the plate: the lowest foot rests on it (bent legs come down, a
// raised leg doesn't lift the figure).
Frames frames(const Mannequin& m, const Body& b)
{
    const Frames first = frames(m, b, 0.0);
    double lowest = 1e300;
    for (int side = 0; side < 2; ++side) {
        // The foot's lowest point (an ellipsoid turned with the shin).
        const Transform& k = first.knee[side];
        const Vec3 c = k.apply(footCentre(b));
        const Vec3 r = footRadii(b);
        double reach = 0.0;
        for (int i = 0; i < 3; ++i)
            reach += (k.m[2][std::size_t(i)] * r[std::size_t(i)]) * (k.m[2][std::size_t(i)] * r[std::size_t(i)]);
        lowest = std::min(lowest, c[2] - std::sqrt(reach));
    }
    return frames(m, b, -lowest);
}

constexpr int kAround = 16;  // segments round each shape
constexpr int kRings = 10;

// An ellipsoid: `centre` and `radii` in the frame `w`.
void ellipsoid(Mesh& out, const Transform& w, const Vec3& centre, const Vec3& radii)
{
    const auto base = std::uint32_t(out.vertexCount());
    for (int r = 0; r <= kRings; ++r) {
        const double v = std::numbers::pi * r / kRings;
        for (int a = 0; a < kAround; ++a) {
            const double u = 2 * std::numbers::pi * a / kAround;
            const Vec3 s{std::sin(v) * std::cos(u), std::sin(v) * std::sin(u), std::cos(v)};
            const Vec3 p = w.apply(add(centre, {s[0] * radii[0], s[1] * radii[1], s[2] * radii[2]}));
            const Vec3 n = w.applyDirection(unit({s[0] / radii[0], s[1] / radii[1], s[2] / radii[2]}));
            for (int k = 0; k < 3; ++k) {
                out.positions.push_back(float(p[k]));
                out.normals.push_back(float(n[k]));
            }
        }
    }
    for (int r = 0; r < kRings; ++r)
        for (int a = 0; a < kAround; ++a) {
            const std::uint32_t i0 = base + std::uint32_t(r * kAround + a);
            const std::uint32_t i1 = base + std::uint32_t(r * kAround + (a + 1) % kAround);
            const std::uint32_t i2 = i0 + kAround, i3 = i1 + kAround;
            out.indices.insert(out.indices.end(), {i0, i2, i1, i1, i2, i3});
        }
}

// A rounded rod from `a` to `b` (frame `w`), radius ra at a and rb at b.
void rod(Mesh& out, const Transform& w, const Vec3& a, const Vec3& b, double ra, double rb)
{
    ellipsoid(out, w, a, {ra, ra, ra});
    ellipsoid(out, w, b, {rb, rb, rb});
    const Vec3 d = unit(sub(b, a));
    const Vec3 side = unit(cross(d, std::abs(d[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}));
    const Vec3 other = cross(d, side);
    const auto base = std::uint32_t(out.vertexCount());
    for (int end = 0; end < 2; ++end)
        for (int k = 0; k < kAround; ++k) {
            const double u = 2 * std::numbers::pi * k / kAround;
            const Vec3 radial = add(scale(side, std::cos(u)), scale(other, std::sin(u)));
            const Vec3 p = w.apply(add(end == 0 ? a : b, scale(radial, end == 0 ? ra : rb)));
            const Vec3 n = w.applyDirection(radial);
            for (int c = 0; c < 3; ++c) {
                out.positions.push_back(float(p[c]));
                out.normals.push_back(float(n[c]));
            }
        }
    for (int k = 0; k < kAround; ++k) {
        const std::uint32_t i0 = base + std::uint32_t(k), i1 = base + std::uint32_t((k + 1) % kAround);
        const std::uint32_t j0 = i0 + kAround, j1 = i1 + kAround;
        out.indices.insert(out.indices.end(), {i0, i1, j0, i1, j1, j0});
    }
}

}  // namespace

const char* key(MJoint joint) { return kKeys[index(joint)]; }

std::optional<MJoint> fromKey(const std::string& k)
{
    for (int i = 0; i < kMJointCount; ++i)
        if (k == kKeys[i])
            return MJoint(i);
    return std::nullopt;
}

const char* title(MJoint joint) { return kTitles[index(joint)]; }

int angleCount(MJoint joint)
{
    switch (joint) {
    case MJoint::LeftElbow:
    case MJoint::RightElbow:
    case MJoint::LeftKnee:
    case MJoint::RightKnee:
        return 1;
    default:
        return 2;
    }
}

const char* angleTitle(MJoint joint, int which)
{
    switch (joint) {
    case MJoint::Neck:
        return which == 0 ? "Nod" : "Turn";
    case MJoint::Waist:
        return which == 0 ? "Bend" : "Twist";
    case MJoint::LeftElbow:
    case MJoint::RightElbow:
    case MJoint::LeftKnee:
    case MJoint::RightKnee:
        return "Bend";
    default:
        return which == 0 ? "Forward" : "Out";
    }
}

std::array<double, 2> angleRange(MJoint joint, int which)
{
    switch (joint) {
    case MJoint::Neck:
        return which == 0 ? std::array<double, 2>{-60, 60} : std::array<double, 2>{-90, 90};
    case MJoint::Waist:
        return which == 0 ? std::array<double, 2>{-45, 90} : std::array<double, 2>{-60, 60};
    case MJoint::LeftShoulder:
    case MJoint::RightShoulder:
        return which == 0 ? std::array<double, 2>{-90, 180} : std::array<double, 2>{-30, 180};
    case MJoint::LeftHip:
    case MJoint::RightHip:
        return which == 0 ? std::array<double, 2>{-60, 130} : std::array<double, 2>{-30, 90};
    default:  // elbows, knees
        return which == 0 ? std::array<double, 2>{0, 150} : std::array<double, 2>{0, 0};
    }
}

void validate(const Mannequin& m)
{
    if (!(m.height >= 10.0 && m.height <= 2000.0))
        throw std::invalid_argument("The figure guide's height must be from 10 mm to 2 m.");
    if (!(m.heads >= 3.0 && m.heads <= 12.0))
        throw std::invalid_argument("The figure guide must be from 3 to 12 heads tall.");
    if (!std::isfinite(m.x) || !std::isfinite(m.y) || !std::isfinite(m.turn))
        throw std::invalid_argument("The figure guide's place is broken.");
    for (int j = 0; j < kMJointCount; ++j)
        for (int k = 0; k < angleCount(MJoint(j)); ++k) {
            const auto r = angleRange(MJoint(j), k);
            const double a = m.pose[std::size_t(j)][std::size_t(k)];
            if (!(a >= r[0] - 1e-9 && a <= r[1] + 1e-9))
                throw std::invalid_argument(std::string("The figure guide's ") + title(MJoint(j)) + " (" +
                                            angleTitle(MJoint(j), k) + ") is out of range.");
        }
}

const char* title(PosePreset p)
{
    switch (p) {
    case PosePreset::Standing:
        return "Standing";
    case PosePreset::APose:
        return "A-pose";
    case PosePreset::TPose:
        return "T-pose";
    case PosePreset::Walking:
        return "Walking";
    case PosePreset::Action:
        return "Action";
    }
    return "";
}

std::array<std::array<double, 2>, kMJointCount> preset(PosePreset p)
{
    std::array<std::array<double, 2>, kMJointCount> a{};
    const auto set = [&a](MJoint j, double first, double second = 0.0) { a[std::size_t(j)] = {first, second}; };
    switch (p) {
    case PosePreset::Standing:
        set(MJoint::LeftShoulder, 0, 6);
        set(MJoint::RightShoulder, 0, 6);
        break;
    case PosePreset::APose:
        set(MJoint::LeftShoulder, 0, 40);
        set(MJoint::RightShoulder, 0, 40);
        set(MJoint::LeftHip, 0, 8);
        set(MJoint::RightHip, 0, 8);
        break;
    case PosePreset::TPose:
        set(MJoint::LeftShoulder, 0, 90);
        set(MJoint::RightShoulder, 0, 90);
        break;
    case PosePreset::Walking:
        set(MJoint::LeftShoulder, -20, 6);
        set(MJoint::RightShoulder, 20, 6);
        set(MJoint::LeftElbow, 10);
        set(MJoint::RightElbow, 25);
        set(MJoint::LeftHip, 25);
        set(MJoint::RightHip, -15);
        set(MJoint::LeftKnee, 5);
        set(MJoint::RightKnee, 20);
        break;
    case PosePreset::Action:
        set(MJoint::Waist, 10, 15);
        set(MJoint::Neck, 0, -20);
        set(MJoint::LeftShoulder, 60, 30);
        set(MJoint::RightShoulder, -30, 40);
        set(MJoint::LeftElbow, 90);
        set(MJoint::RightElbow, 60);
        set(MJoint::LeftHip, 50, 10);
        set(MJoint::RightHip, -20, 15);
        set(MJoint::LeftKnee, 70);
        set(MJoint::RightKnee, 30);
        break;
    }
    return a;
}

Landmarks landmarks(const Mannequin& m)
{
    const Body b = measure(m);
    const Frames f = frames(m, b);
    Landmarks l;
    l.top = f.neck.apply({0, 0, b.H});
    l.chin = f.neck.apply({0, 0, 0});
    l.leftShoulder = f.shoulder[0].apply({0, 0, 0});
    l.rightShoulder = f.shoulder[1].apply({0, 0, 0});
    l.leftElbow = f.elbow[0].apply({0, 0, 0});
    l.rightElbow = f.elbow[1].apply({0, 0, 0});
    l.leftWrist = f.elbow[0].apply({0, 0, -b.forearm});
    l.rightWrist = f.elbow[1].apply({0, 0, -b.forearm});
    l.leftHip = f.hip[0].apply({0, 0, 0});
    l.rightHip = f.hip[1].apply({0, 0, 0});
    l.leftKnee = f.knee[0].apply({0, 0, 0});
    l.rightKnee = f.knee[1].apply({0, 0, 0});
    l.leftAnkle = f.knee[0].apply({0, 0, -b.shin});
    l.rightAnkle = f.knee[1].apply({0, 0, -b.shin});
    return l;
}

Mesh mesh(const Mannequin& m)
{
    const Body b = measure(m);
    const Frames f = frames(m, b);
    const double H = b.H, R = b.R;
    Mesh out;
    // Head and neck.
    ellipsoid(out, f.neck, {0, -0.04 * H, 0.5 * H}, {0.36 * H, 0.44 * H, 0.5 * H});
    rod(out, f.waist, {0, 0, b.shoulderZ - b.waistZ - 0.04 * R}, {0, 0, b.neckZ - b.waistZ + 0.1 * H}, 0.2 * H,
        0.17 * H);
    // Chest and belly (they turn at the waist), hips (they don't).
    const double chestZ = b.shoulderZ - b.waistZ - 0.08 * R;
    ellipsoid(out, f.waist, {0, 0, chestZ}, {0.82 * H, 0.48 * H, 0.12 * R});
    rod(out, f.waist, {0, 0, 0}, {0, 0, chestZ}, 0.55 * H, 0.62 * H);
    rod(out, f.root, {0, 0, b.hipZ + 0.04 * R}, {0, 0, b.waistZ}, 0.6 * H, 0.55 * H);
    ellipsoid(out, f.root, {0, 0, b.hipZ + 0.02 * R}, {0.72 * H, 0.44 * H, 0.08 * R});
    for (int side = 0; side < 2; ++side) {
        // Arm: upper arm, forearm, hand.
        rod(out, f.shoulder[side], {0, 0, 0}, {0, 0, -b.upperArm}, 0.22 * H, 0.16 * H);
        rod(out, f.elbow[side], {0, 0, 0}, {0, 0, -b.forearm}, 0.16 * H, 0.12 * H);
        ellipsoid(out, f.elbow[side], {0, 0, -b.forearm - 0.5 * b.hand}, {0.1 * H, 0.2 * H, 0.5 * b.hand});
        // Leg: thigh, shin, foot.
        rod(out, f.hip[side], {0, 0, 0}, {0, 0, -b.thigh}, 0.3 * H, 0.2 * H);
        rod(out, f.knee[side], {0, 0, 0}, {0, 0, -b.shin}, 0.2 * H, 0.12 * H);
        ellipsoid(out, f.knee[side], footCentre(b), footRadii(b));
    }
    return out;
}

}  // namespace forge::mannequin
