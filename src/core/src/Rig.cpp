// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Rig.h"

#include "forge/core/Document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace forge::rig {

namespace {

// ---- Little vector and rotation helpers ---------------------------------------------

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
Vec3 lerp(const Vec3& a, const Vec3& b, double t) { return add(a, mul(sub(b, a), t)); }

constexpr double kDeg = std::numbers::pi / 180.0;

Transform Rx(double deg) { return Transform::rotationXYZ(deg, 0, 0); }
Transform Ry(double deg) { return Transform::rotationXYZ(0, deg, 0); }
Transform Rz(double deg) { return Transform::rotationXYZ(0, 0, deg); }
Transform T(const Vec3& t) { return Transform::translation(t); }

// The rotation with these columns (where X, Y and Z go).
Transform columns(const Vec3& x, const Vec3& y, const Vec3& z)
{
    Transform t;
    for (std::size_t r = 0; r < 3; ++r) {
        t.m[r][0] = x[r];
        t.m[r][1] = y[r];
        t.m[r][2] = z[r];
        t.m[r][3] = 0.0;
    }
    return t;
}
Vec3 column(const Transform& t, std::size_t c) { return {t.m[0][c], t.m[1][c], t.m[2][c]}; }
Transform rotationOf(const Transform& t)
{
    Transform r = t;
    for (auto& row : r.m)
        row[3] = 0.0;
    return r;
}
Vec3 translationOf(const Transform& t) { return {t.m[0][3], t.m[1][3], t.m[2][3]}; }

// The smallest turn taking direction a to direction b (both unit).
Transform swing(const Vec3& a, const Vec3& b)
{
    const Vec3 k = cross(a, b);
    const double c = dot(a, b);
    if (c < -1.0 + 1e-9) {
        // Opposite: half a turn about any axis square to a.
        Vec3 axis = cross(a, {1, 0, 0});
        if (length(axis) < 1e-6)
            axis = cross(a, {0, 1, 0});
        axis = unit(axis);
        // R = 2 axis axis^T - I
        Transform t;
        for (std::size_t r = 0; r < 3; ++r)
            for (std::size_t q = 0; q < 3; ++q)
                t.m[r][q] = 2 * axis[r] * axis[q] - (r == q ? 1.0 : 0.0);
        return t;
    }
    // R = I + K + K^2 / (1 + c), K the cross-product matrix of k.
    const double K[3][3] = {{0, -k[2], k[1]}, {k[2], 0, -k[0]}, {-k[1], k[0], 0}};
    Transform t;
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t q = 0; q < 3; ++q) {
            double kk = 0;
            for (std::size_t i = 0; i < 3; ++i)
                kk += K[r][i] * K[i][q];
            t.m[r][q] = (r == q ? 1.0 : 0.0) + K[r][q] + kk / (1.0 + c);
        }
    return t;
}

double clampUnit(double v) { return std::clamp(v, -1.0, 1.0); }

// ---- The skeleton --------------------------------------------------------------------

constexpr const char* kPointKeys[kRigPointCount] = {
    "waist",      "neck",      "head",      "leftShoulder", "rightShoulder", "leftElbow",  "rightElbow",
    "leftWrist",  "rightWrist", "leftHand", "rightHand",    "leftHip",       "rightHip",   "leftKnee",
    "rightKnee",  "leftAnkle", "rightAnkle", "leftToe",     "rightToe"};
constexpr const char* kPointTitles[kRigPointCount] = {
    "Waist",       "Neck",        "Top of the head", "Left shoulder", "Right shoulder", "Left elbow",
    "Right elbow", "Left wrist",  "Right wrist",     "Left hand",     "Right hand",     "Left hip",
    "Right hip",   "Left knee",   "Right knee",      "Left ankle",    "Right ankle",    "Left toes",
    "Right toes"};

enum Bone {
    Hips, Chest, HeadBone, LUpperArm, RUpperArm, LForearm, RForearm, LHandBone, RHandBone, LThigh, RThigh, LShin,
    RShin, LFoot, RFoot, kBoneEnd
};
static_assert(int(kBoneEnd) == kBoneCount);

struct BoneInfo {
    int parent;      // -1: the hips
    int joint;       // the Joint that turns it (-1: none)
    int origin;      // the Point it turns about (-1: between the hips)
    int end;         // the Point it reaches to
};

constexpr BoneInfo kBones[kBoneCount] = {
    {-1, -1, -1, int(Point::Waist)},
    {Hips, int(Joint::Waist), int(Point::Waist), int(Point::Neck)},
    {Chest, int(Joint::Neck), int(Point::Neck), int(Point::Head)},
    {Chest, int(Joint::LeftShoulder), int(Point::LeftShoulder), int(Point::LeftElbow)},
    {Chest, int(Joint::RightShoulder), int(Point::RightShoulder), int(Point::RightElbow)},
    {LUpperArm, int(Joint::LeftElbow), int(Point::LeftElbow), int(Point::LeftWrist)},
    {RUpperArm, int(Joint::RightElbow), int(Point::RightElbow), int(Point::RightWrist)},
    {LForearm, int(Joint::LeftWrist), int(Point::LeftWrist), int(Point::LeftHand)},
    {RForearm, int(Joint::RightWrist), int(Point::RightWrist), int(Point::RightHand)},
    {Hips, int(Joint::LeftHip), int(Point::LeftHip), int(Point::LeftKnee)},
    {Hips, int(Joint::RightHip), int(Point::RightHip), int(Point::RightKnee)},
    {LThigh, int(Joint::LeftKnee), int(Point::LeftKnee), int(Point::LeftAnkle)},
    {RThigh, int(Joint::RightKnee), int(Point::RightKnee), int(Point::RightAnkle)},
    {LShin, int(Joint::LeftAnkle), int(Point::LeftAnkle), int(Point::LeftToe)},
    {RShin, int(Joint::RightAnkle), int(Point::RightAnkle), int(Point::RightToe)},
};

// The bone each point moves with.
constexpr int kPointBone[kRigPointCount] = {
    Chest,     Chest,     HeadBone,  LUpperArm, RUpperArm, LForearm, RForearm, LHandBone, RHandBone, LHandBone,
    RHandBone, LThigh,    RThigh,    LShin,     RShin,     LFoot,    RFoot,    LFoot,     RFoot};

const Vec3& at(const Points& p, Point which) { return p[std::size_t(which)]; }
Vec3 pelvis(const Points& p) { return lerp(at(p, Point::LeftHip), at(p, Point::RightHip), 0.5); }

// The line pieces each bone's surface is weighted to.
struct Segment {
    Vec3 a, b;
};
std::array<std::vector<Segment>, kBoneCount> segments(const Points& p)
{
    std::array<std::vector<Segment>, kBoneCount> s;
    s[Hips] = {{at(p, Point::LeftHip), at(p, Point::RightHip)}, {pelvis(p), at(p, Point::Waist)}};
    s[Chest] = {{at(p, Point::Waist), at(p, Point::Neck)},
                {at(p, Point::Neck), at(p, Point::LeftShoulder)},
                {at(p, Point::Neck), at(p, Point::RightShoulder)}};
    for (int b = HeadBone; b < kBoneCount; ++b)
        s[std::size_t(b)] = {{p[std::size_t(kBones[b].origin)], p[std::size_t(kBones[b].end)]}};
    return s;
}

double segmentDistance(const Vec3& q, const Segment& s)
{
    const Vec3 d = sub(s.b, s.a);
    const double dd = dot(d, d);
    double t = dd > 0 ? dot(sub(q, s.a), d) / dd : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    return length(sub(q, add(s.a, mul(d, t))));
}

// ---- Joints ---------------------------------------------------------------------------

double sideSign(Joint j)
{
    switch (j) {
    case Joint::RightShoulder:
    case Joint::RightElbow:
    case Joint::RightHip:
    case Joint::RightKnee:
    case Joint::RightWrist:
    case Joint::RightAnkle: return -1.0;
    default: return 1.0;
    }
}

enum class Kind { Upright, Limb, Elbow, Knee, Ankle };
Kind kindOf(Joint j)
{
    switch (j) {
    case Joint::Neck:
    case Joint::Waist: return Kind::Upright;
    case Joint::LeftElbow:
    case Joint::RightElbow: return Kind::Elbow;
    case Joint::LeftKnee:
    case Joint::RightKnee: return Kind::Knee;
    case Joint::LeftAnkle:
    case Joint::RightAnkle: return Kind::Ankle;
    default: return Kind::Limb;  // shoulders, hips, wrists
    }
}

// The way a bone points when its joint is at 0 (in its parent's frame).
Vec3 restAxis(Kind k)
{
    switch (k) {
    case Kind::Upright: return {0, 0, 1};
    case Kind::Ankle: return {0, -1, 0};
    default: return {0, 0, -1};
    }
}

// A joint's turn at these angles, as the figure guide turns it.
Transform local(Joint j, const std::array<double, 2>& a)
{
    switch (kindOf(j)) {
    case Kind::Upright: return Rz(a[1]) * Rx(a[0]);
    case Kind::Limb: return Rx(-a[0]) * Ry(-sideSign(j) * a[1]);
    case Kind::Elbow: return Rx(-a[0]);
    case Kind::Knee: return Rx(a[0]);
    case Kind::Ankle: return Rx(a[0]);
    }
    return {};
}

// The angles that best give this turn (exactly, where the joint can).
std::array<double, 2> anglesOf(Joint j, const Transform& r)
{
    const Kind k = kindOf(j);
    if (k == Kind::Upright) {
        const Vec3 x = column(r, 0);
        const Vec3 z = column(r, 2);
        const double tw = std::atan2(x[1], x[0]);
        const double s = z[0] * std::sin(tw) - z[1] * std::cos(tw);
        return {std::atan2(s, z[2]) / kDeg, tw / kDeg};
    }
    const Vec3 d = r.applyDirection(restAxis(k));
    switch (k) {
    case Kind::Limb: {
        const double out = std::asin(clampUnit(sideSign(j) * d[0]));
        // Straight out to the side, forward doesn't matter: call it 0.
        const double fwd = std::abs(std::cos(out)) < 0.1 ? 0.0 : std::atan2(-d[1], -d[2]);
        return {fwd / kDeg, out / kDeg};
    }
    case Kind::Elbow: return {std::atan2(-d[1], -d[2]) / kDeg, 0.0};
    case Kind::Knee: return {std::atan2(d[1], -d[2]) / kDeg, 0.0};
    case Kind::Ankle: return {std::atan2(-d[2], -d[1]) / kDeg, 0.0};
    default: return {0.0, 0.0};
    }
}

// The skeleton as made: each bone's frame on the figure, and its joint's own turn.
struct Rest {
    std::array<Transform, kBoneCount> frame;   // bone -> step coordinates
    std::array<Transform, kBoneCount> turn;    // the joint's turn in its parent's frame
    std::array<Vec3, kBoneCount> offset;       // the joint, in its parent's frame
};

Rest restOf(const Points& p)
{
    Rest r;
    r.frame[Hips] = T(pelvis(p));
    r.turn[Hips] = Transform{};
    r.offset[Hips] = pelvis(p);
    for (int b = Chest; b < kBoneCount; ++b) {
        const BoneInfo& info = kBones[b];
        const Transform& parent = r.frame[std::size_t(info.parent)];
        const Transform toParent = parent.inverse();
        const Vec3 origin = p[std::size_t(info.origin)];
        const Vec3 dir = unit(toParent.applyDirection(sub(p[std::size_t(info.end)], origin)));
        Transform turn;
        if (b == Chest) {
            // Up the spine, its X across the shoulders (left).
            const Vec3 z = dir;
            const Vec3 across =
                toParent.applyDirection(sub(at(p, Point::LeftShoulder), at(p, Point::RightShoulder)));
            Vec3 x = sub(across, mul(z, dot(across, z)));
            x = length(x) > 1e-9 ? unit(x) : unit(cross({0, 1, 0}, z), {1, 0, 0});
            turn = columns(x, cross(z, x), z);
        } else {
            turn = swing(restAxis(kindOf(Joint(info.joint))), dir);
        }
        r.turn[std::size_t(b)] = turn;
        r.offset[std::size_t(b)] = toParent.apply(origin);
        r.frame[std::size_t(b)] = parent * T(r.offset[std::size_t(b)]) * turn;
    }
    return r;
}

// Every joint's angle shows (the arm out at 90°... ) round to a tenth of a degree.
double tidy(double a) { return std::round(a * 10.0) / 10.0; }

std::array<double, 2> restAnglesOf(const Rest& r, Joint j)
{
    for (int b = Chest; b < kBoneCount; ++b)
        if (kBones[b].joint == int(j)) {
            const auto a = anglesOf(j, r.turn[std::size_t(b)]);
            return {tidy(a[0]), tidy(a[1])};
        }
    return {0.0, 0.0};
}

// ---- Dual quaternions ------------------------------------------------------------------

struct Quat {
    double w = 1, x = 0, y = 0, z = 0;
};
Quat qmul(const Quat& a, const Quat& b)
{
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}
Quat fromRotation(const Transform& t)
{
    const auto& m = t.m;
    const double tr = m[0][0] + m[1][1] + m[2][2];
    Quat q;
    if (tr > 0) {
        const double s = std::sqrt(tr + 1.0) * 2;
        q = {0.25 * s, (m[2][1] - m[1][2]) / s, (m[0][2] - m[2][0]) / s, (m[1][0] - m[0][1]) / s};
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        const double s = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2;
        q = {(m[2][1] - m[1][2]) / s, 0.25 * s, (m[0][1] + m[1][0]) / s, (m[0][2] + m[2][0]) / s};
    } else if (m[1][1] > m[2][2]) {
        const double s = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2;
        q = {(m[0][2] - m[2][0]) / s, (m[0][1] + m[1][0]) / s, 0.25 * s, (m[1][2] + m[2][1]) / s};
    } else {
        const double s = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2;
        q = {(m[1][0] - m[0][1]) / s, (m[0][2] + m[2][0]) / s, (m[1][2] + m[2][1]) / s, 0.25 * s};
    }
    const double l = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / l, q.x / l, q.y / l, q.z / l};
}

struct DualQuat {
    Quat real, dual;
};
DualQuat toDual(const Transform& t)
{
    DualQuat d;
    d.real = fromRotation(t);
    const Quat tq{0, t.m[0][3], t.m[1][3], t.m[2][3]};
    const Quat qd = qmul(tq, d.real);
    d.dual = {0.5 * qd.w, 0.5 * qd.x, 0.5 * qd.y, 0.5 * qd.z};
    return d;
}

// ---- Weights ----------------------------------------------------------------------------

constexpr int kInfluences = 4;
struct Weights {
    std::array<std::uint8_t, kInfluences> bone{};
    std::array<float, kInfluences> weight{};
};

template <class F>
void inParallel(std::size_t n, F&& f)
{
    const std::size_t threads = std::clamp<std::size_t>(std::thread::hardware_concurrency(), 1, 16);
    if (n < 20000 || threads == 1) {
        f(std::size_t(0), n);
        return;
    }
    std::vector<std::thread> pool;
    const std::size_t chunk = (n + threads - 1) / threads;
    for (std::size_t t = 0; t < threads; ++t) {
        const std::size_t from = t * chunk;
        const std::size_t to = std::min(n, from + chunk);
        if (from >= to)
            break;
        pool.emplace_back([&f, from, to] { f(from, to); });
    }
    for (auto& th : pool)
        th.join();
}

std::vector<float> smoothNormals(const Mesh& mesh)
{
    std::vector<double> acc(mesh.positions.size(), 0.0);
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t i[3] = {mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
        Vec3 p[3];
        for (int k = 0; k < 3; ++k)
            p[k] = {mesh.positions[3 * i[k]], mesh.positions[3 * i[k] + 1], mesh.positions[3 * i[k] + 2]};
        const Vec3 n = cross(sub(p[1], p[0]), sub(p[2], p[0]));
        for (int k = 0; k < 3; ++k)
            for (int c = 0; c < 3; ++c)
                acc[3 * i[k] + std::size_t(c)] += n[std::size_t(c)];
    }
    std::vector<float> out(acc.size());
    for (std::size_t v = 0; v + 2 < acc.size(); v += 3) {
        const Vec3 n = unit({acc[v], acc[v + 1], acc[v + 2]});
        out[v] = float(n[0]);
        out[v + 1] = float(n[1]);
        out[v + 2] = float(n[2]);
    }
    return out;
}

bool finite(const Vec3& v) { return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }

}  // namespace

// ---- Names ------------------------------------------------------------------------------

const char* key(Point p) { return kPointKeys[int(p)]; }
const char* title(Point p) { return kPointTitles[int(p)]; }

Point mirrored(Point p)
{
    switch (p) {
    case Point::Waist:
    case Point::Neck:
    case Point::Head: return p;
    default: {
        // Left and right come in pairs, left first.
        const int i = int(p);
        const bool left = (i - int(Point::LeftShoulder)) % 2 == 0;
        return Point(left ? i + 1 : i - 1);
    }
    }
}

const char* key(Joint j)
{
    switch (j) {
    case Joint::LeftWrist: return "leftWrist";
    case Joint::RightWrist: return "rightWrist";
    case Joint::LeftAnkle: return "leftAnkle";
    case Joint::RightAnkle: return "rightAnkle";
    default: return mannequin::key(MJoint(int(j)));
    }
}

const char* title(Joint j)
{
    switch (j) {
    case Joint::LeftWrist: return "Left wrist";
    case Joint::RightWrist: return "Right wrist";
    case Joint::LeftAnkle: return "Left ankle";
    case Joint::RightAnkle: return "Right ankle";
    default: return mannequin::title(MJoint(int(j)));
    }
}

int angleCount(Joint j)
{
    switch (j) {
    case Joint::LeftWrist:
    case Joint::RightWrist: return 2;
    case Joint::LeftAnkle:
    case Joint::RightAnkle: return 1;
    default: return mannequin::angleCount(MJoint(int(j)));
    }
}

const char* angleTitle(Joint j, int which)
{
    switch (j) {
    case Joint::LeftWrist:
    case Joint::RightWrist: return which == 0 ? "Bend" : "Sideways";
    case Joint::LeftAnkle:
    case Joint::RightAnkle: return "Point toes";
    default: return mannequin::angleTitle(MJoint(int(j)), which);
    }
}

std::array<double, 2> angleRange(Joint j, int which)
{
    switch (j) {
    case Joint::LeftWrist:
    case Joint::RightWrist: return which == 0 ? std::array<double, 2>{-90, 90} : std::array<double, 2>{-45, 45};
    case Joint::LeftAnkle:
    case Joint::RightAnkle: return {-45, 70};
    case Joint::LeftElbow:
    case Joint::RightElbow:
    case Joint::LeftKnee:
    case Joint::RightKnee: return which == 0 ? std::array<double, 2>{-20, 160} : std::array<double, 2>{0, 0};
    default: return mannequin::angleRange(MJoint(int(j)), which);
    }
}

Point pivot(Joint j)
{
    for (int b = Chest; b < kBoneCount; ++b)
        if (kBones[b].joint == int(j))
            return Point(kBones[b].origin);
    return Point::Waist;
}

// ---- Checks -------------------------------------------------------------------------------

// ---- Proportions ------------------------------------------------------------------------

namespace {

// Each bone's map from the figure as made to its proportions: about where it turns, it's
// made longer along itself (arms, legs, torso), wider (the shoulders, across the chest),
// or bigger all round (head, hands, feet), and carried to where its parent put that
// point. Affine, so held in a Transform's numbers (not inverted).
std::array<Transform, kBoneCount> proportionMaps(const PosedFinish& f)
{
    const Points& p = f.points;
    const auto& k = f.proportions;
    using M3 = std::array<std::array<double, 3>, 3>;
    // x' = to + L (x - origin)
    const auto affine = [](const Vec3& origin, const Vec3& to, const M3& L) {
        Transform t;
        for (int r = 0; r < 3; ++r) {
            double o = 0;
            for (int c = 0; c < 3; ++c) {
                t.m[std::size_t(r)][std::size_t(c)] = L[std::size_t(r)][std::size_t(c)];
                o += L[std::size_t(r)][std::size_t(c)] * origin[std::size_t(c)];
            }
            t.m[std::size_t(r)][3] = to[std::size_t(r)] - o;
        }
        return t;
    };
    // L = round I + (along - round) a a^T + (across - 1) round s s^T: `along` along a,
    // `round` all round it, and along s (square to a) `across` times more.
    const auto scaled = [](const Vec3& a, double along, double round, const Vec3& s, double across) {
        M3 L{};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                L[std::size_t(r)][std::size_t(c)] =
                    (r == c ? round : 0.0) + (along - round) * a[std::size_t(r)] * a[std::size_t(c)] +
                    (across - 1.0) * round * s[std::size_t(r)] * s[std::size_t(c)];
        return L;
    };
    const Vec3 none{0, 0, 0};
    const auto unitOr = [](const Vec3& v, const Vec3& fallback) {
        const double l = length(v);
        return l > 1e-9 ? mul(v, 1.0 / l) : fallback;
    };
    const double body = k[std::size_t(Proportion::BodyWidth)];
    const double limbs = k[std::size_t(Proportion::LimbThickness)];
    const Vec3 waist = at(p, Point::Waist);
    const Vec3 up = unitOr(sub(at(p, Point::Neck), waist), {0, 0, 1});
    std::array<Transform, kBoneCount> m;
    // The hips wider all round the line up through the waist.
    m[Hips] = body == 1.0 ? Transform{} : affine(waist, waist, scaled(up, 1.0, body, none, 1.0));
    {
        // The torso longer from the waist up, wider all round; the shoulders wider across.
        Vec3 across = sub(at(p, Point::LeftShoulder), at(p, Point::RightShoulder));
        across = unitOr(sub(across, mul(up, dot(across, up))), {1, 0, 0});
        m[Chest] = affine(waist, waist,
                          scaled(up, k[std::size_t(Proportion::Torso)], body, across,
                                 k[std::size_t(Proportion::Shoulders)]));
    }
    const auto origin = [&](int b) { return p[std::size_t(kBones[b].origin)]; };
    const auto axisOf = [&](int b) {
        return unitOr(sub(p[std::size_t(kBones[b].end)], origin(b)), {0, 0, 1});
    };
    for (int b = HeadBone; b < kBoneCount; ++b) {
        const Vec3 from = origin(b);
        const Vec3 to = m[std::size_t(kBones[b].parent)].apply(from);  // where the parent took it
        double along = 1.0, round = 1.0, all = 0.0;
        switch (b) {
        case HeadBone: all = k[std::size_t(Proportion::Head)]; break;
        case LHandBone:
        case RHandBone: all = k[std::size_t(Proportion::Hands)]; break;
        case LFoot:
        case RFoot: all = k[std::size_t(Proportion::Feet)]; break;
        case LUpperArm:
        case RUpperArm:
        case LForearm:
        case RForearm:
            along = k[std::size_t(Proportion::Arms)];
            round = limbs;
            break;
        default:  // thighs and shins
            along = k[std::size_t(Proportion::Legs)];
            round = limbs;
            break;
        }
        if (all > 0.0)
            m[std::size_t(b)] = affine(from, to, scaled(axisOf(b), all, all, none, 1.0));
        else
            m[std::size_t(b)] = affine(from, to, scaled(axisOf(b), along, round, none, 1.0));
    }
    return m;
}

namespace {

// Each part's round sides squared off about its bone (`pts` the skeleton as proportioned):
// a point's offset from the bone, seen across it, goes out to the square round the circle
// it's on (by length / the larger of its two sideways parts), its bones' moves blended by
// the skin weights, `amount` of the way.
struct BoneFrame {
    Vec3 origin, axis, u, v;
};
std::array<BoneFrame, kBoneCount> boneFrames(const Points& pts)
{
    std::array<BoneFrame, kBoneCount> out;
    for (int b = 0; b < kBoneCount; ++b) {
        Vec3 o, e;
        if (b == Hips) {
            o = mul(add(at(pts, Point::LeftHip), at(pts, Point::RightHip)), 0.5);
            e = at(pts, Point::Waist);
        } else {
            o = pts[std::size_t(kBones[b].origin)];
            e = pts[std::size_t(kBones[b].end)];
        }
        Vec3 a = sub(e, o);
        const double l = length(a);
        a = l > 1e-9 ? mul(a, 1.0 / l) : Vec3{0, 0, 1};
        // Square to the front (the figure faces -Y) where it can, else to up.
        Vec3 u = sub(Vec3{0, 1, 0}, mul(a, a[1]));
        if (length(u) < 0.3)
            u = sub(Vec3{0, 0, 1}, mul(a, a[2]));
        u = mul(u, 1.0 / length(u));
        out[std::size_t(b)] = {o, a, u, cross(a, u)};
    }
    return out;
}

}  // namespace

}  // namespace

const char* key(Proportion p)
{
    static constexpr const char* keys[kProportionCount] = {"head", "shoulders", "torso", "arms", "hands",
                                                         "legs", "feet",      "limbs", "body"};
    return keys[std::size_t(p)];
}

const char* title(Proportion p)
{
    static constexpr const char* titles[kProportionCount] = {"Head size",   "Shoulder width", "Torso length",
                                                             "Arm length",  "Hand size",      "Leg length",
                                                             "Foot size",   "Limb thickness", "Body width"};
    return titles[std::size_t(p)];
}

Proportions asMade() { return {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}; }
// head, shoulders, torso, arms, hands, legs, feet, limbs, body
Proportions heroic() { return {0.9, 1.15, 1.0, 1.08, 1.1, 1.12, 1.05, 1.05, 1.0}; }
Proportions chibi() { return {1.8, 0.85, 0.75, 0.7, 1.2, 0.6, 1.2, 1.15, 1.1}; }

bool proportioned(const PosedFinish& f)
{
    return std::any_of(f.proportions.begin(), f.proportions.end(), [](double v) { return std::abs(v - 1.0) > 1e-9; });
}

bool shaped(const PosedFinish& f) { return proportioned(f) || f.blocky > 1e-9; }

Points proportionedPoints(const PosedFinish& f)
{
    if (!proportioned(f))
        return f.points;
    const auto maps = proportionMaps(f);
    Points out;
    for (std::size_t i = 0; i < kRigPointCount; ++i)
        out[i] = maps[std::size_t(kPointBone[i])].apply(f.points[i]);
    return out;
}

void check(const PosedFinish& f)
{
    for (const auto& p : f.points)
        if (!finite(p))
            throw std::invalid_argument("The skeleton has a point that isn't a number.");
    for (int b = Chest; b < kBoneCount; ++b)
        if (length(sub(f.points[std::size_t(kBones[b].end)], f.points[std::size_t(kBones[b].origin)])) < 1e-3)
            throw std::invalid_argument(std::string("The skeleton's ") + title(Point(kBones[b].origin)) +
                                        " and " + title(Point(kBones[b].end)) + " are in the same place.");
    if (length(sub(at(f.points, Point::LeftHip), at(f.points, Point::RightHip))) < 1e-3)
        throw std::invalid_argument("The skeleton's hips are in the same place.");
    for (const auto& a : f.angles)
        if (!std::isfinite(a[0]) || !std::isfinite(a[1]) || std::abs(a[0]) > 360 || std::abs(a[1]) > 360)
            throw std::invalid_argument("A joint's angle isn't a number of degrees.");
    if (!(f.blend >= 0.1 && f.blend <= 200.0))
        throw std::invalid_argument("How far the bends spread must be from 0.1 to 200 mm.");
    for (int i = 0; i < kProportionCount; ++i)
        if (!(f.proportions[std::size_t(i)] >= kLeastProportion && f.proportions[std::size_t(i)] <= kMostProportion))
            throw std::invalid_argument(std::string(title(Proportion(i))) + " must be from 30% to 300%.");
    if (!(f.blocky >= 0.0 && f.blocky <= 1.0))
        throw std::invalid_argument("How blocky it is must be from 0% to 100%.");
}

std::string describe(const PosedFinish& f)
{
    const Angles rest = restAngles(f.points);
    int bent = 0;
    for (int j = 0; j < kRigJointCount; ++j) {
        if (f.feetFlat && (Joint(j) == Joint::LeftAnkle || Joint(j) == Joint::RightAnkle))
            continue;
        for (int k = 0; k < angleCount(Joint(j)); ++k)
            if (std::abs(f.angles[std::size_t(j)][std::size_t(k)] - rest[std::size_t(j)][std::size_t(k)]) > 0.5) {
                ++bent;
                break;
            }
    }
    std::ostringstream s;
    if (bent == 0)
        s << "posed, as made";
    else
        s << "posed, " << bent << (bent == 1 ? " joint" : " joints") << " moved";
    if (f.feetFlat)
        s << ", feet flat";
    if (proportioned(f))
        s << ", new proportions";
    if (f.blocky > 1e-9)
        s << ", blocky";
    return s.str();
}

// ---- Fitting --------------------------------------------------------------------------------

double height(const Points& p)
{
    double lo = std::numeric_limits<double>::max(), hi = -lo;
    for (const Vec3& v : p) {
        lo = std::min(lo, v[2]);
        hi = std::max(hi, v[2]);
    }
    return hi - lo;
}

Angles restAngles(const Points& points)
{
    const Rest r = restOf(points);
    Angles a{};
    for (int j = 0; j < kRigJointCount; ++j)
        a[std::size_t(j)] = restAnglesOf(r, Joint(j));
    return a;
}

Angles fromMannequin(const std::array<std::array<double, 2>, kMJointCount>& pose)
{
    Angles a{};
    for (int j = 0; j < kMJointCount; ++j)
        a[std::size_t(j)] = pose[std::size_t(j)];
    return a;
}

PosedFinish fit(const Mesh& mesh)
{
    const std::size_t n = mesh.vertexCount();
    if (n == 0)
        throw std::invalid_argument("There's no surface to put a skeleton in.");
    auto vert = [&](std::size_t i) {
        return Vec3{mesh.positions[3 * i], mesh.positions[3 * i + 1], mesh.positions[3 * i + 2]};
    };
    const Bounds box = mesh.bounds();
    const double z0 = box.min[2];
    const double h = std::max(1e-3, double(box.max[2] - box.min[2]));
    // The figure guide's proportions, 7.5 heads tall.
    const double H = h / 7.5;
    const double R = h - H;
    const double neckZ = z0 + h - H;
    const double shoulderZ = neckZ - 0.05 * R;
    const double waistZ = neckZ - 0.29 * R;
    const double hipZ = neckZ - 0.40 * R;
    const double kneeZ = z0 + 0.28 * R;
    const double ankleZ = z0 + 0.05 * R;
    const double shoulderX = 0.9 * H;
    const double hipX = 0.42 * H;
    const double upperArm = 0.20 * R, forearm = 0.17 * R, hand = 0.09 * R;
    const double arm = upperArm + forearm + hand;

    // The body's middle: the surface round the waist, near the middle of the box.
    const double bcx = 0.5 * (box.min[0] + box.max[0]);
    double cx = 0, cy = 0;
    {
        double sx = 0, sy = 0;
        std::size_t count = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 v = vert(i);
            if (std::abs(v[2] - waistZ) < 0.04 * h && std::abs(v[0] - bcx) < H) {
                sx += v[0];
                sy += v[1];
                ++count;
            }
        }
        cx = count ? sx / double(count) : bcx;
        cy = count ? sy / double(count) : 0.5 * (box.min[1] + box.max[1]);
    }

    Points p{};
    auto set = [&](Point which, const Vec3& v) { p[std::size_t(which)] = v; };
    set(Point::Head, {cx, cy, z0 + h});
    set(Point::Neck, {cx, cy, neckZ});
    set(Point::Waist, {cx, cy, waistZ});

    // Moves a limb's point into the middle of the limb: the surface round it, across the
    // limb (along `along` it stays put).
    auto centredOnce = [&](const Vec3& q, const Vec3& along, double reach) {
        Vec3 sum{0, 0, 0};
        std::size_t count = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 d = sub(vert(i), q);
            if (std::abs(dot(d, along)) < 0.12 * H && length(d) < reach) {
                sum = add(sum, d);
                ++count;
            }
        }
        if (count < 6)
            return q;
        const Vec3 mean = mul(sum, 1.0 / double(count));
        return add(q, sub(mean, mul(along, dot(mean, along))));
    };
    // Then again, reaching just past the limb's own surface, so a limb close by (the other
    // leg, the body beside an arm) doesn't pull it off the middle.
    auto centred = [&](const Vec3& q, const Vec3& along, double reach) {
        Vec3 c = centredOnce(q, along, reach);
        for (int pass = 0; pass < 3; ++pass) {
            std::vector<double> d;
            for (std::size_t i = 0; i < n; ++i) {
                const Vec3 e = sub(vert(i), c);
                const double t = dot(e, along);
                if (std::abs(t) < 0.12 * H && length(e) < reach)
                    d.push_back(length(sub(e, mul(along, t))));
            }
            if (d.size() < 6)
                break;
            std::nth_element(d.begin(), d.begin() + std::ptrdiff_t(d.size() / 4), d.end());
            const double nearSide = d[d.size() / 4];  // the limb's own surface is the nearest
            c = centredOnce(c, along, std::min(reach, 1.6 * nearSide + 0.5));
        }
        return c;
    };

    // How much surface there is round a point of a limb's line (within `within` of it, in
    // a thin slice across the line), and how far it typically is.
    struct Round {
        std::size_t count = 0;
        double typical = 0.0;
    };
    const auto round = [&](const Vec3& at, const Vec3& axis, double within) {
        std::vector<double> d;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 e = sub(vert(i), at);
            const double t = dot(e, axis);
            if (std::abs(t) > 0.08 * H)
                continue;
            const double r = length(sub(e, mul(axis, t)));
            if (r < within)
                d.push_back(r);
        }
        Round out;
        out.count = d.size();
        if (d.size() >= 6) {
            std::nth_element(d.begin(), d.begin() + std::ptrdiff_t(d.size() / 2), d.end());
            out.typical = d[d.size() / 2];
        }
        return out;
    };
    // From `from` along a limb's line (`axis`, towards the body), the last point before
    // what's round it gets much fatter than the limb: where the limb meets the body.
    // `stop`: a point the walk mustn't pass (true there). Nothing found: `from`.
    const auto meetsBody = [&](const Vec3& from, const Vec3& axis, double reachOut, double roomFactor,
                               const std::function<bool(const Vec3&)>& stop) {
        const Round limb = round(from, axis, reachOut);
        if (limb.typical <= 0.0)
            return from;
        const double within = roomFactor * limb.typical;
        const std::size_t limbCount = round(from, axis, within).count;
        const double step = 0.05 * H;
        Vec3 at = from;
        for (int k = 0; k < 80; ++k) {
            const Vec3 next = add(at, mul(axis, step));
            if (stop(next))
                break;
            const Round here = round(next, axis, within);
            // Much more surface round it (the body beside), or much less (inside the body,
            // its surface further off), or further off: past where the limb meets the body.
            if (here.count > 2 * limbCount || 2 * here.count < limbCount || here.typical > 1.5 * limb.typical)
                break;
            at = next;
        }
        return at;
    };

    // The crotch: the lowest surface on the figure's middle line between the legs (if the
    // legs stand apart, so it's well above the knees).
    std::optional<double> crotchZ;
    {
        double lowest = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 v = vert(i);
            if (std::abs(v[0] - cx) < 0.1 * H && std::abs(v[1] - cy) < 0.6 * H)
                lowest = std::min(lowest, v[2]);
        }
        if (lowest > kneeZ + 0.1 * R && lowest < waistZ)
            crotchZ = lowest;
    }

    for (int side = 0; side < 2; ++side) {
        const double s = side == 0 ? 1.0 : -1.0;  // left is +X (the figure faces -Y)
        const bool left = side == 0;
        // Arms: out to the side if something reaches well past the shoulder above the
        // thighs, else hanging down.
        Vec3 shoulder{cx + s * shoulderX, cy, shoulderZ};
        Vec3 tip{cx + s * (shoulderX + 0.05 * H), cy, shoulderZ - arm};
        bool out = false;
        double far = -1;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 v = vert(i);
            if (v[2] < 0.5 * (hipZ + kneeZ))
                continue;
            const double reach = s * (v[0] - cx);
            if (reach > far) {
                far = reach;
                if (reach > shoulderX + 0.35 * arm) {
                    tip = v;
                    out = true;
                }
            }
        }
        if (out) {
            // The shoulder in line with the arm (its height and depth from the arm by it).
            const Vec3 sideways = unit(sub(tip, shoulder));
            shoulder = centred(add(shoulder, mul(sideways, 0.3 * H)), sideways, 0.8 * H);
            shoulder = sub(shoulder, mul(sideways, 0.3 * H));
        }
        const Vec3 along = unit(sub(tip, shoulder));
        const Vec3 elbow = centred(lerp(shoulder, tip, upperArm / arm), along, 0.8 * H);
        const Vec3 wrist = centred(lerp(shoulder, tip, (upperArm + forearm) / arm), along, 0.6 * H);
        if (out) {
            // The shoulder where the arm meets the body: along the arm's line in from the elbow.
            const Vec3 inward = unit(sub(elbow, wrist), mul(along, -1.0));
            shoulder = meetsBody(elbow, inward, 0.8 * H, 3.0,
                                 [&](const Vec3& q) { return s * (q[0] - cx) < 0.2 * H; });
        }
        set(left ? Point::LeftShoulder : Point::RightShoulder, shoulder);
        set(left ? Point::LeftElbow : Point::RightElbow, elbow);
        set(left ? Point::LeftWrist : Point::RightWrist, wrist);
        set(left ? Point::LeftHand : Point::RightHand, tip);

        // Legs: the foot is the surface low down on this side.
        Vec3 hip{cx + s * hipX, cy, hipZ};
        Vec3 sum{0, 0, 0};
        std::size_t count = 0;
        double toeY = std::numeric_limits<double>::max(), heelY = -toeY;
        Vec3 toe{};
        for (std::size_t i = 0; i < n; ++i) {
            const Vec3 v = vert(i);
            if (v[2] > z0 + 0.08 * R || s * (v[0] - cx) < 0)
                continue;
            sum = add(sum, v);
            ++count;
            if (v[1] < toeY) {
                toeY = v[1];
                toe = v;
            }
            heelY = std::max(heelY, v[1]);
        }
        Vec3 ankle{cx + s * hipX, cy, ankleZ};
        Vec3 toes{ankle[0], cy - 0.5 * H, z0 + 0.01 * R};
        if (count > 0) {
            const Vec3 mid = mul(sum, 1.0 / double(count));
            ankle = {mid[0], heelY + 0.25 * (toeY - heelY), ankleZ};
            toes = {toe[0], toeY, z0 + 0.02 * R};
            if (toeY > ankle[1] - 0.05 * H)  // no foot to speak of
                toes[1] = ankle[1] - 0.3 * H;
        }
        const Vec3 legAlong = unit(sub(ankle, hip));
        const double t = (hipZ - kneeZ) / std::max(1e-6, hipZ - ankleZ);
        const Vec3 knee = centred(lerp(hip, ankle, t), legAlong, 0.9 * H);
        // The hip where the leg meets the body: up the leg's line from the knee to the
        // crotch's height.
        if (crotchZ && count > 0) {
            const Vec3 up = unit(sub(knee, ankle), {0, 0, 1});
            if (up[2] > 0.3)
                hip = add(knee, mul(up, (*crotchZ - knee[2]) / up[2]));
        }
        set(left ? Point::LeftHip : Point::RightHip, hip);
        set(left ? Point::LeftKnee : Point::RightKnee, knee);
        set(left ? Point::LeftAnkle : Point::RightAnkle, ankle);
        set(left ? Point::LeftToe : Point::RightToe, toes);
    }

    // No bone left with no length (an arm or leg not found where it was looked for): its
    // end put a little way on, down the limb (or up, for the head), so every ball can be
    // dragged where it belongs.
    const double least = std::max(0.02 * h, 1e-2);
    for (int b = Chest; b < kBoneCount; ++b) {
        Vec3& end = p[std::size_t(kBones[b].end)];
        const Vec3& from = p[std::size_t(kBones[b].origin)];
        if (length(sub(end, from)) >= least)
            continue;
        Vec3 way{0, 0, -1};
        if (b == Chest || b == HeadBone)
            way = {0, 0, 1};
        else if (b == LFoot || b == RFoot)
            way = {0, -1, 0};
        else if (kBones[b].parent >= Chest) {
            const BoneInfo& up = kBones[kBones[b].parent];
            if (up.origin >= 0)
                way = unit(sub(from, p[std::size_t(up.origin)]), {0, 0, -1});
        }
        end = add(from, mul(way, least));
    }
    if (length(sub(at(p, Point::LeftHip), at(p, Point::RightHip))) < least) {
        p[std::size_t(Point::LeftHip)][0] += least / 2.0;
        p[std::size_t(Point::RightHip)][0] -= least / 2.0;
    }

    PosedFinish f;
    f.points = p;
    f.angles = restAngles(p);
    f.blend = std::clamp(std::round(0.2 * H * 10.0) / 10.0, 0.5, 50.0);
    return f;
}

// ---- Posing -----------------------------------------------------------------------------------

Moves moves(const PosedFinish& f)
{
    const Points made = proportionedPoints(f);  // posed from the figure with its proportions
    const Rest r = restOf(made);
    std::array<Transform, kBoneCount> frame;
    frame[Hips] = r.frame[Hips];
    for (int b = Chest; b < kBoneCount; ++b) {
        const BoneInfo& info = kBones[b];
        const Joint j = Joint(info.joint);
        const std::size_t bi = std::size_t(b);
        const Transform base = frame[std::size_t(info.parent)] * T(r.offset[bi]);
        const bool flat = f.feetFlat && (j == Joint::LeftAnkle || j == Joint::RightAnkle);
        if (flat) {
            frame[bi] = T(translationOf(base)) * rotationOf(r.frame[bi]);
            continue;
        }
        // The joint's turn at these angles, carrying whatever the figure-guide angles
        // can't say about how it was made (a twist, a lean), so made angles move nothing.
        const auto rest = restAnglesOf(r, j);
        const Transform correction = local(j, rest).inverse() * r.turn[bi];
        frame[bi] = base * local(j, f.angles[std::size_t(info.joint)]) * correction;
    }
    Moves m;
    m.bones.resize(kBoneCount);
    for (std::size_t b = 0; b < kBoneCount; ++b)
        m.bones[b] = frame[b] * r.frame[b].inverse();
    for (std::size_t i = 0; i < kRigPointCount; ++i)
        m.points[i] = m.bones[std::size_t(kPointBone[i])].apply(made[i]);
    return m;
}

struct Skin::Impl {
    Mesh rest;
    std::vector<Weights> weights;
    Points points{};
    double blend = 0;
    double lowest = 0;
};

Skin::Skin(const Mesh& rest, const PosedFinish& f) : m(std::make_unique<Impl>())
{
    check(f);
    m->rest = rest;
    if (m->rest.normals.size() != m->rest.positions.size())
        m->rest.normals = smoothNormals(m->rest);
    m->points = f.points;
    m->blend = f.blend;
    const std::size_t n = rest.vertexCount();
    m->lowest = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < n; ++i)
        m->lowest = std::min(m->lowest, double(rest.positions[3 * i + 2]));
    m->weights.resize(n);
    const auto segs = segments(f.points);
    const double blend = f.blend;
    inParallel(n, [&](std::size_t from, std::size_t to) {
        std::array<double, kBoneCount> d{};
        for (std::size_t i = from; i < to; ++i) {
            const Vec3 q{rest.positions[3 * i], rest.positions[3 * i + 1], rest.positions[3 * i + 2]};
            double nearest = std::numeric_limits<double>::max();
            for (std::size_t b = 0; b < kBoneCount; ++b) {
                double best = std::numeric_limits<double>::max();
                for (const Segment& s : segs[b])
                    best = std::min(best, segmentDistance(q, s));
                d[b] = best;
                nearest = std::min(nearest, best);
            }
            // The nearest bones, each by how close it is to being the nearest.
            std::array<std::pair<float, std::uint8_t>, kBoneCount> w{};
            for (std::size_t b = 0; b < kBoneCount; ++b) {
                const double t = std::clamp(1.0 - (d[b] - nearest) / blend, 0.0, 1.0);
                w[b] = {float(t * t * (3 - 2 * t)), std::uint8_t(b)};
            }
            std::partial_sort(w.begin(), w.begin() + kInfluences, w.end(),
                              [](const auto& a, const auto& b) { return a.first > b.first; });
            float total = 0;
            for (int k = 0; k < kInfluences; ++k)
                total += w[std::size_t(k)].first;
            Weights& out = m->weights[i];
            for (int k = 0; k < kInfluences; ++k) {
                out.bone[std::size_t(k)] = w[std::size_t(k)].second;
                out.weight[std::size_t(k)] = total > 0 ? w[std::size_t(k)].first / total : (k == 0 ? 1.f : 0.f);
            }
        }
    });
}

Skin::~Skin() = default;
Skin::Skin(Skin&&) noexcept = default;
Skin& Skin::operator=(Skin&&) noexcept = default;

bool Skin::sameWeights(const PosedFinish& f) const { return f.points == m->points && f.blend == m->blend; }

double Skin::lowest() const { return m->lowest; }

Skin::Posed Skin::posed(const PosedFinish& f) const
{
    check(f);
    const Moves mv = moves(f);
    std::array<DualQuat, kBoneCount> dq;
    for (std::size_t b = 0; b < kBoneCount; ++b)
        dq[b] = toDual(mv.bones[b]);

    Posed out;
    out.mesh.indices = m->rest.indices;
    out.mesh.positions.resize(m->rest.positions.size());
    out.mesh.normals.resize(m->rest.positions.size());
    const std::size_t n = m->rest.vertexCount();
    // With new proportions, first the figure as made is reshaped: each point by its bones'
    // maps, blended by the same weights (so the joints stay joined), its facings again.
    Mesh reshaped;
    if (proportioned(f)) {
        const auto maps = proportionMaps(f);
        reshaped.indices = m->rest.indices;
        reshaped.positions.resize(m->rest.positions.size());
        const auto& R = m->rest.positions;
        inParallel(n, [&](std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                const Weights& w = m->weights[i];
                const Vec3 x{R[3 * i], R[3 * i + 1], R[3 * i + 2]};
                Vec3 y{0, 0, 0};
                double total = 0;
                for (int k = 0; k < kInfluences; ++k) {
                    const double wt = w.weight[std::size_t(k)];
                    if (wt <= 0)
                        continue;
                    y = add(y, mul(maps[std::size_t(w.bone[std::size_t(k)])].apply(x), wt));
                    total += wt;
                }
                if (total > 0)
                    y = mul(y, 1.0 / total);
                else
                    y = x;
                for (int c = 0; c < 3; ++c)
                    reshaped.positions[3 * i + std::size_t(c)] = float(y[std::size_t(c)]);
            }
        });
    } else if (shaped(f)) {
        reshaped.indices = m->rest.indices;
        reshaped.positions = m->rest.positions;
    }
    if (f.blocky > 1e-9) {
        // Then squared off about the bones where they now are.
        const auto frames = boneFrames(proportionedPoints(f));
        const double amount = f.blocky;
        auto& X = reshaped.positions;
        inParallel(n, [&](std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; ++i) {
                const Weights& w = m->weights[i];
                const Vec3 x{X[3 * i], X[3 * i + 1], X[3 * i + 2]};
                Vec3 move{0, 0, 0};
                double total = 0;
                for (int k = 0; k < kInfluences; ++k) {
                    const double wt = w.weight[std::size_t(k)];
                    if (wt <= 0)
                        continue;
                    const BoneFrame& fr = frames[std::size_t(w.bone[std::size_t(k)])];
                    const Vec3 d = sub(x, fr.origin);
                    const Vec3 r = sub(d, mul(fr.axis, dot(d, fr.axis)));
                    const double ru = std::abs(dot(r, fr.u)), rv = std::abs(dot(r, fr.v));
                    const double big = std::max(ru, rv);
                    if (big > 1e-9)
                        move = add(move, mul(r, wt * (length(r) / big - 1.0)));
                    total += wt;
                }
                if (total > 0)
                    move = mul(move, amount / total);
                for (int c = 0; c < 3; ++c)
                    X[3 * i + std::size_t(c)] = float(x[std::size_t(c)] + move[std::size_t(c)]);
            }
        });
    }
    if (shaped(f))
        reshaped.normals = smoothNormals(reshaped);
    const auto& P = shaped(f) ? reshaped.positions : m->rest.positions;
    const auto& N = shaped(f) ? reshaped.normals : m->rest.normals;
    inParallel(n, [&](std::size_t from, std::size_t to) {
        for (std::size_t i = from; i < to; ++i) {
            const Weights& w = m->weights[i];
            const DualQuat& first = dq[w.bone[0]];
            Quat r{0, 0, 0, 0}, d{0, 0, 0, 0};
            for (int k = 0; k < kInfluences; ++k) {
                const double wt = w.weight[std::size_t(k)];
                if (wt <= 0)
                    continue;
                const DualQuat& q = dq[w.bone[std::size_t(k)]];
                // The shorter way round from the main bone.
                const double sign = (q.real.w * first.real.w + q.real.x * first.real.x +
                                     q.real.y * first.real.y + q.real.z * first.real.z) < 0
                                        ? -wt
                                        : wt;
                r = {r.w + sign * q.real.w, r.x + sign * q.real.x, r.y + sign * q.real.y, r.z + sign * q.real.z};
                d = {d.w + sign * q.dual.w, d.x + sign * q.dual.x, d.y + sign * q.dual.y, d.z + sign * q.dual.z};
            }
            const double len = std::sqrt(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
            r = {r.w / len, r.x / len, r.y / len, r.z / len};
            d = {d.w / len, d.x / len, d.y / len, d.z / len};
            // Rotation matrix of r; translation 2 (d * conj(r)).
            const Quat t = qmul(d, Quat{r.w, -r.x, -r.y, -r.z});
            const double tx = 2 * t.x, ty = 2 * t.y, tz = 2 * t.z;
            const double xx = r.x * r.x, yy = r.y * r.y, zz = r.z * r.z;
            const double xy = r.x * r.y, xz = r.x * r.z, yz = r.y * r.z;
            const double wx = r.w * r.x, wy = r.w * r.y, wz = r.w * r.z;
            const double M[3][3] = {{1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy)},
                                    {2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx)},
                                    {2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy)}};
            const double p[3] = {P[3 * i], P[3 * i + 1], P[3 * i + 2]};
            const double q[3] = {N[3 * i], N[3 * i + 1], N[3 * i + 2]};
            const double tr[3] = {tx, ty, tz};
            for (int c = 0; c < 3; ++c) {
                out.mesh.positions[3 * i + std::size_t(c)] =
                    float(M[c][0] * p[0] + M[c][1] * p[1] + M[c][2] * p[2] + tr[c]);
                out.mesh.normals[3 * i + std::size_t(c)] = float(M[c][0] * q[0] + M[c][1] * q[1] + M[c][2] * q[2]);
            }
        }
    });
    out.points = mv.points;
    if (f.onGround && n > 0) {
        double low = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < n; ++i)
            low = std::min(low, double(out.mesh.positions[3 * i + 2]));
        const float shift = float(m->lowest - low);
        for (std::size_t i = 0; i < n; ++i)
            out.mesh.positions[3 * i + 2] += shift;
        for (Vec3& p : out.points)
            p[2] += double(shift);
    }
    return out;
}

int boneOf(Joint j)
{
    for (int b = Chest; b < kBoneCount; ++b)
        if (kBones[b].joint == int(j))
            return b;
    return Hips;
}

int parentBone(int bone)
{
    return bone >= 0 && bone < kBoneCount ? kBones[bone].parent : -1;
}

std::vector<int> boneAndBelow(int bone)
{
    std::vector<int> out{bone};
    for (std::size_t i = 0; i < out.size(); ++i)
        for (int b = Chest; b < kBoneCount; ++b)
            if (kBones[b].parent == out[i])
                out.push_back(b);
    return out;
}

const char* boneTitle(int bone)
{
    static const char* const titles[kBoneCount] = {
        "Pelvis",       "Chest",         "Head",        "Left upper arm", "Right upper arm",
        "Left forearm", "Right forearm", "Left hand",   "Right hand",     "Left thigh",
        "Right thigh",  "Left shin",     "Right shin",  "Left foot",      "Right foot"};
    return bone >= 0 && bone < kBoneCount ? titles[bone] : "Part";
}

std::string pieceName(const std::vector<int>& bones)
{
    if (bones.empty())
        return "Part";
    const auto has = [&](int b) { return std::find(bones.begin(), bones.end(), b) != bones.end(); };
    // The top bone: the one whose parent isn't in the piece.
    int top = bones.front();
    for (int b : bones)
        if (!has(parentBone(b)))
            top = b;
    switch (top) {
    case Hips: return has(Chest) ? (bones.size() >= kBoneCount ? "Figure" : "Torso") : "Pelvis";
    case Chest: return has(HeadBone) ? "Upper body" : "Chest";
    case LUpperArm: return has(LForearm) ? "Left arm" : "Left upper arm";
    case RUpperArm: return has(RForearm) ? "Right arm" : "Right upper arm";
    case LThigh: return has(LShin) ? "Left leg" : "Left thigh";
    case RThigh: return has(RShin) ? "Right leg" : "Right thigh";
    case LShin: return has(LFoot) ? "Left lower leg" : "Left shin";
    case RShin: return has(RFoot) ? "Right lower leg" : "Right shin";
    default: return boneTitle(top);
    }
}

std::vector<std::array<Vec3, 2>> boneSegments(const Points& points, int bone)
{
    std::vector<std::array<Vec3, 2>> out;
    if (bone < 0 || bone >= kBoneCount)
        return out;
    const auto all = segments(points);  // kept: looping over a part of a temporary would outlive it
    for (const Segment& s : all[std::size_t(bone)])
        out.push_back({s.a, s.b});
    return out;
}

std::vector<int> nearestBones(const Mesh& mesh, const Points& points)
{
    const auto segs = segments(points);
    const std::size_t n = mesh.vertexCount();
    std::vector<int> out(n, 0);
    inParallel(n, [&](std::size_t from, std::size_t to) {
        for (std::size_t i = from; i < to; ++i) {
            const Vec3 q{mesh.positions[3 * i], mesh.positions[3 * i + 1], mesh.positions[3 * i + 2]};
            double best = std::numeric_limits<double>::max();
            for (std::size_t b = 0; b < kBoneCount; ++b)
                for (const Segment& s : segs[b]) {
                    const double d = segmentDistance(q, s);
                    if (d < best) {
                        best = d;
                        out[i] = int(b);
                    }
                }
        }
    });
    return out;
}

MeshSolid apply(const MeshSolid& solid, const PosedFinish& f)
{
    Mesh rest;
    meshkernel::triangles(solid, rest.positions, rest.indices);
    const Skin skin(rest, f);
    const Skin::Posed p = skin.posed(f);
    return meshkernel::fromTriangles(p.mesh.positions, p.mesh.indices);
}

// ---- Drawing the skeleton ----------------------------------------------------------------------

Mesh skeletonMesh(const Points& points, double ballRadius, int highlight)
{
    Mesh out;
    auto vertex = [&](const Vec3& p, const Vec3& nrm) {
        for (int k = 0; k < 3; ++k) {
            out.positions.push_back(float(p[std::size_t(k)]));
            out.normals.push_back(float(nrm[std::size_t(k)]));
        }
    };
    constexpr int around = 12, rings = 8;
    for (int i = 0; i < kRigPointCount; ++i) {
        const double r = i == highlight ? ballRadius * 1.6 : ballRadius;
        const auto base = std::uint32_t(out.vertexCount());
        for (int ring = 0; ring <= rings; ++ring) {
            const double v = std::numbers::pi * ring / rings;
            for (int a = 0; a < around; ++a) {
                const double u = 2 * std::numbers::pi * a / around;
                const Vec3 s{std::sin(v) * std::cos(u), std::sin(v) * std::sin(u), std::cos(v)};
                vertex(add(points[std::size_t(i)], mul(s, r)), s);
            }
        }
        for (int ring = 0; ring < rings; ++ring)
            for (int a = 0; a < around; ++a) {
                const std::uint32_t p0 = base + std::uint32_t(ring * around + a);
                const std::uint32_t p1 = base + std::uint32_t(ring * around + (a + 1) % around);
                const std::uint32_t p2 = p0 + around, p3 = p1 + around;
                out.indices.insert(out.indices.end(), {p0, p2, p1, p1, p2, p3});
            }
    }
    // Sticks between them.
    const std::pair<Point, Point> sticks[] = {
        {Point::Waist, Point::Neck},          {Point::Neck, Point::Head},
        {Point::Neck, Point::LeftShoulder},   {Point::Neck, Point::RightShoulder},
        {Point::LeftShoulder, Point::LeftElbow}, {Point::RightShoulder, Point::RightElbow},
        {Point::LeftElbow, Point::LeftWrist}, {Point::RightElbow, Point::RightWrist},
        {Point::LeftWrist, Point::LeftHand},  {Point::RightWrist, Point::RightHand},
        {Point::Waist, Point::LeftHip},       {Point::Waist, Point::RightHip},
        {Point::LeftHip, Point::RightHip},    {Point::LeftHip, Point::LeftKnee},
        {Point::RightHip, Point::RightKnee},  {Point::LeftKnee, Point::LeftAnkle},
        {Point::RightKnee, Point::RightAnkle}, {Point::LeftAnkle, Point::LeftToe},
        {Point::RightAnkle, Point::RightToe}};
    const double r = 0.35 * ballRadius;
    for (const auto& [a, b] : sticks) {
        const Vec3 pa = at(points, a), pb = at(points, b);
        const Vec3 axis = sub(pb, pa);
        if (length(axis) < 1e-9)
            continue;
        const Vec3 z = unit(axis);
        const Vec3 x = unit(cross(std::abs(z[2]) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0}, z));
        const Vec3 y = cross(z, x);
        const auto base = std::uint32_t(out.vertexCount());
        for (int end = 0; end < 2; ++end)
            for (int k = 0; k < around; ++k) {
                const double u = 2 * std::numbers::pi * k / around;
                const Vec3 nrm = add(mul(x, std::cos(u)), mul(y, std::sin(u)));
                vertex(add(end == 0 ? pa : pb, mul(nrm, r)), nrm);
            }
        for (int k = 0; k < around; ++k) {
            const std::uint32_t p0 = base + std::uint32_t(k), p1 = base + std::uint32_t((k + 1) % around);
            const std::uint32_t p2 = p0 + around, p3 = p1 + around;
            out.indices.insert(out.indices.end(), {p0, p1, p2, p1, p3, p2});
        }
    }
    return out;
}

}  // namespace forge::rig

namespace forge {

Transform rigFrame(const Document& doc, ObjectId target)
{
    const ObjectId holder = doc.parentOf(target);
    return holder == kInvalidId ? Transform{} : doc.worldTransform(holder);
}

BooleanSpec posedSpec(const Document& doc, ObjectId target, const PosedFinish& f)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    rig::check(f);
    BooleanSpec spec;
    spec.op = BooleanOp::Posed;
    spec.operands = {target};
    spec.posed = f;
    const Transform toStep = rigFrame(doc, target).inverse();
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

Body withPosed(const Document& doc, ObjectId result, const PosedFinish& f)
{
    const Body* body = doc.find(result);
    if (!body || !body->boolean || body->boolean->op != BooleanOp::Posed)
        throw std::invalid_argument("That isn't a Pose Figure step.");
    rig::check(f);
    Body next = *body;
    next.boolean->posed = f;
    return next;
}

}  // namespace forge
