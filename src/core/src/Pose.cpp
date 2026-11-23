// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Pose.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/Hinge.h"
#include "forge/core/History.h"
#include "forge/core/Socket.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <exception>
#include <mutex>
#include <numbers>
#include <set>
#include <thread>

namespace forge::pose {
namespace {

constexpr double kPi = std::numbers::pi;

Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 scaled(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit(const Vec3& a)
{
    const double l = std::sqrt(dot(a, a));
    return l > 1e-12 ? scaled(a, 1.0 / l) : Vec3{0, 0, 1};
}

Vec3 squareTo(const Vec3& axis) { return unit(cross(emboss::upFor(axis), axis)); }

bool contains(const Bounds& b, const Vec3& p, double slack)
{
    if (!b.valid)
        return false;
    for (int i = 0; i < 3; ++i)
        if (p[i] < b.min[i] - slack || p[i] > b.max[i] + slack)
            return false;
    return true;
}

double boxVolume(const Bounds& b)
{
    if (!b.valid)
        return 0.0;
    const auto s = b.size();
    return double(s[0]) * s[1] * s[2];
}

bool same(const Transform& a, const Transform& b)
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            if (std::abs(a.m[r][c] - b.m[r][c]) > 1e-9)
                return false;
    return true;
}

bool overlapping(const Bounds& a, const Bounds& b)
{
    if (!a.valid || !b.valid)
        return false;
    for (int i = 0; i < 3; ++i)
        if (a.max[i] < b.min[i] || b.max[i] < a.min[i])
            return false;
    return true;
}

// The other part round a pivot: the biggest one there that isn't the holder.
ObjectId partAround(const Document& doc, const Vec3& pivot, ObjectId holder)
{
    ObjectId best = kInvalidId;
    double biggest = -1.0;
    for (ObjectId id : doc.topLevelIds()) {
        if (id == holder)
            continue;
        const Bounds b = doc.worldBounds(id);
        if (!contains(b, pivot, 0.01))
            continue;
        const double v = boxVolume(b);
        if (v > biggest) {
            biggest = v;
            best = id;
        }
    }
    return best;
}

}  // namespace

std::vector<Joint> findJoints(const Document& doc)
{
    std::vector<Joint> out;
    for (const Body& b : doc.bodies()) {
        if (!b.isBoolean())
            continue;
        const BooleanSpec& spec = *b.boolean;
        const bool hinged = spec.op == BooleanOp::Hinge && spec.hinge && spec.hinge->half == HingeHalf::Fork;
        const bool socketed = spec.op == BooleanOp::Socket && spec.socket;
        if (!hinged && !socketed)
            continue;
        const ObjectId holder = history::objectOf(doc, b.id);
        if (holder == kInvalidId || !doc.find(holder))
            continue;
        Joint j;
        j.step = b.id;
        j.holder = holder;
        const std::string holderName = doc.find(holder)->name;
        try {
            if (hinged) {
                const HingeFinish& f = *spec.hinge;
                const emboss::SurfacePoint spot = hingeSpot(doc, b.id);
                const Vec3 n = unit(spot.normal);
                if (f.kind == HingeKind::Swivel) {
                    j.kind = JointKind::Swivel;
                    j.pivot = sub(spot.point, scaled(n, f.barrel / 2.0));
                    j.axis = n;
                    j.name = "Swivel in " + holderName;
                } else {
                    j.kind = JointKind::Hinge;
                    j.pivot = sub(spot.point, scaled(n, hinge::pinDepth(f)));
                    j.axis = hinge::pinAxis(f, n);
                    j.name = (f.kind == HingeKind::Double ? "Double hinge in " : "Hinge in ") + holderName;
                }
                j.across = squareTo(j.axis);
                j.clicks = f.clicks;
                const ObjectId partner = hingePartner(doc, b.id);
                if (partner != kInvalidId) {
                    const Body* p = doc.find(partner);
                    if (p && p->boolean && p->boolean->hinge && p->boolean->hinge->half == HingeHalf::Tab)
                        j.other = history::objectOf(doc, partner);
                }
            } else {
                const SocketFinish& f = *spec.socket;
                const emboss::SurfacePoint spot = socketSpot(doc, b.id);
                const Vec3 n = unit(spot.normal);
                j.axis = n;
                j.across = squareTo(n);
                if (f.kind == SocketKind::Ball) {
                    j.kind = JointKind::Ball;
                    j.pivot = sub(spot.point, scaled(n, socket::ballSeat(f).centreDepth));
                    j.name = "Ball socket in " + holderName;
                } else if (f.kind == SocketKind::Magnet) {
                    j.kind = JointKind::Fixed;  // holds a swappable part on
                    j.pivot = sub(spot.point, scaled(n, f.cover + f.depth / 2.0));
                    j.name = "Magnet in " + holderName;
                    // The part held on sits against the face, just outside it.
                    j.other = partAround(doc, sub(spot.point, scaled(n, -0.3)), holder);
                } else {
                    j.kind = f.shape == PegShape::Round ? JointKind::Swivel : JointKind::Fixed;
                    j.pivot = sub(spot.point, scaled(n, f.depth / 2.0));
                    j.name = (f.shape == PegShape::Round ? "Peg socket in " : "Keyed peg socket in ") + holderName;
                }
            }
        } catch (const std::exception&) {
            continue;
        }
        if (j.other == kInvalidId)
            j.other = partAround(doc, j.pivot, holder);
        if (j.other == holder)
            j.other = kInvalidId;
        out.push_back(std::move(j));
    }
    return out;
}

Rig build(const std::vector<Joint>& joints, const std::map<ObjectId, double>& sizes,
          const std::map<ObjectId, Bounds>& bounds, ObjectId hold)
{
    Rig rig;
    // Who's joined to whom.
    std::map<ObjectId, std::vector<std::pair<ObjectId, std::size_t>>> links;
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const Joint& j = joints[i];
        if (j.other == kInvalidId || !sizes.count(j.holder) || !sizes.count(j.other))
            continue;
        links[j.holder].push_back({j.other, i});
        links[j.other].push_back({j.holder, i});
    }
    const auto sizeOf = [&sizes](ObjectId id) {
        const auto it = sizes.find(id);
        return it == sizes.end() ? 0.0 : it->second;
    };
    std::set<ObjectId> reached;
    const auto spread = [&](ObjectId root) {
        std::deque<ObjectId> todo{root};
        reached.insert(root);
        rig.order.push_back(root);
        while (!todo.empty()) {
            const ObjectId at = todo.front();
            todo.pop_front();
            for (const auto& [next, joint] : links[at]) {
                if (reached.count(next))
                    continue;
                reached.insert(next);
                rig.parent[next] = at;
                rig.joint[next] = joint;
                rig.order.push_back(next);
                todo.push_back(next);
            }
        }
    };
    if (hold == kInvalidId || !links.count(hold)) {
        // The biggest jointed part (a figure's body), or what was asked if it's on the plate.
        if (hold == kInvalidId || !sizes.count(hold)) {
            hold = kInvalidId;
            for (const auto& [id, l] : links)
                if (hold == kInvalidId || sizeOf(id) > sizeOf(hold))
                    hold = id;
        }
    }
    rig.hold = hold;
    if (hold != kInvalidId)
        spread(hold);
    // Other figures on the plate: each held by its biggest part.
    for (;;) {
        ObjectId root = kInvalidId;
        for (const auto& [id, l] : links)
            if (!reached.count(id) && (root == kInvalidId || sizeOf(id) > sizeOf(root)))
                root = id;
        if (root == kInvalidId)
            break;
        spread(root);
    }
    // Pins and the like: a joint's pivot inside them; they move with its parent side.
    for (const auto& [id, size] : sizes) {
        if (reached.count(id))
            continue;
        const auto b = bounds.find(id);
        if (b == bounds.end())
            continue;
        for (std::size_t i = 0; i < joints.size(); ++i) {
            const Joint& j = joints[i];
            if (j.other == kInvalidId || !contains(b->second, j.pivot, 0.01))
                continue;
            const auto child = rig.joint.find(j.other);
            const bool otherIsChild = child != rig.joint.end() && child->second == i;
            const ObjectId with = otherIsChild ? j.holder : j.other;
            if (reached.count(with)) {
                rig.rides[id] = with;
                break;
            }
        }
    }
    return rig;
}

double nearestClick(const Joint& joint, double degrees)
{
    if (joint.clicks <= 0 || (joint.kind != JointKind::Hinge && joint.kind != JointKind::Swivel))
        return degrees;
    const double step = 360.0 / joint.clicks;
    return std::round(degrees / step) * step;
}

Transform rotationAbout(const Vec3& point, const Vec3& axisIn, double degrees)
{
    const Vec3 k = unit(axisIn);
    const double a = degrees * kPi / 180.0, c = std::cos(a), s = std::sin(a), t = 1.0 - c;
    Transform r;
    r.m[0] = {t * k[0] * k[0] + c, t * k[0] * k[1] - s * k[2], t * k[0] * k[2] + s * k[1], 0.0};
    r.m[1] = {t * k[0] * k[1] + s * k[2], t * k[1] * k[1] + c, t * k[1] * k[2] - s * k[0], 0.0};
    r.m[2] = {t * k[0] * k[2] - s * k[1], t * k[1] * k[2] + s * k[0], t * k[2] * k[2] + c, 0.0};
    return Transform::translation(point) * r * Transform::translation(scaled(point, -1.0));
}

Transform turn(const Joint& joint, const std::array<double, 3>& angles)
{
    switch (joint.kind) {
    case JointKind::Hinge:
    case JointKind::Swivel:
        return rotationAbout(joint.pivot, joint.axis, angles[0]);
    case JointKind::Ball: {
        const Vec3 e1 = joint.across, e3 = joint.axis, e2 = unit(cross(e3, e1));
        return rotationAbout(joint.pivot, e1, angles[0]) * rotationAbout(joint.pivot, e2, angles[1]) *
               rotationAbout(joint.pivot, e3, angles[2]);
    }
    case JointKind::Fixed:
        break;
    }
    return Transform{};
}

std::array<double, 3> ballAngles(const Joint& joint, const Transform& rotation)
{
    const Vec3 e[3] = {joint.across, unit(cross(joint.axis, joint.across)), joint.axis};
    // The rotation in the ball's own frame: M = E^T R E = Rx(a) Ry(b) Rz(c).
    double m[3][3];
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 3; ++k) {
            const Vec3 re = rotation.applyDirection(e[k]);
            m[i][k] = dot(e[i], re);
        }
    const double b = std::asin(std::clamp(m[0][2], -1.0, 1.0));
    double a, c;
    if (std::abs(m[0][2]) < 1.0 - 1e-9) {
        c = std::atan2(-m[0][1], m[0][0]);
        a = std::atan2(-m[1][2], m[2][2]);
    } else {  // straight up or down the middle: the twist and bend are the same turn
        c = 0.0;
        a = std::atan2(m[2][1], m[1][1]);
    }
    return {a * 180.0 / kPi, b * 180.0 / kPi, c * 180.0 / kPi};
}

std::map<ObjectId, Transform> transforms(const std::vector<Joint>& joints, const Rig& rig,
                                         const std::map<ObjectId, std::array<double, 3>>& angles)
{
    std::map<ObjectId, Transform> at;
    for (ObjectId id : rig.order) {
        const auto parent = rig.parent.find(id);
        if (parent == rig.parent.end()) {
            at[id] = Transform{};  // held still
            continue;
        }
        const Joint& j = joints[rig.joint.at(id)];
        const auto a = angles.find(j.step);
        const std::array<double, 3> turnBy = a == angles.end() ? std::array<double, 3>{0, 0, 0} : a->second;
        at[id] = at[parent->second] * turn(j, turnBy);
    }
    for (const auto& [id, with] : rig.rides)
        if (at.count(with))
            at[id] = at[with];
    std::map<ObjectId, Transform> moved;
    for (const auto& [id, t] : at)
        if (!same(t, Transform{}))
            moved[id] = t;
    return moved;
}

namespace {

std::vector<Hit> hitsBetween(const std::vector<PosedPart>& parts, const std::vector<MeshSolid>& placed,
                             const std::vector<Bounds>& boxes, const std::set<std::size_t>* onlyWith,
                             double smallest, bool firstOnly)
{
    std::vector<Hit> hits;
    for (std::size_t i = 0; i < parts.size(); ++i)
        for (std::size_t k = i + 1; k < parts.size(); ++k) {
            if (same(parts[i].moved, parts[k].moved))
                continue;  // they move together
            if (onlyWith && onlyWith->count(i) == onlyWith->count(k))
                continue;
            if (!overlapping(boxes[i], boxes[k]))
                continue;
            const MeshSolid both = meshkernel::intersected({placed[i], placed[k]});
            const double v = meshkernel::volume(both);
            if (v <= smallest)
                continue;
            Hit h;
            h.a = parts[i].id;
            h.b = parts[k].id;
            h.volume = v;
            if (!firstOnly)
                meshkernel::triangles(both, h.overlap.positions, h.overlap.indices);
            hits.push_back(std::move(h));
            if (firstOnly)
                return hits;
        }
    return hits;
}

}  // namespace

std::vector<Hit> collisions(const std::vector<PosedPart>& parts, double smallest)
{
    std::vector<MeshSolid> placed;
    std::vector<Bounds> boxes;
    for (const PosedPart& p : parts) {
        placed.push_back(meshkernel::transformed(p.solid, p.moved));
        boxes.push_back(meshkernel::bounds(placed.back()));
    }
    return hitsBetween(parts, placed, boxes, nullptr, smallest, false);
}

std::array<double, 2> range(const std::vector<Joint>& joints, const Rig& rig,
                            std::map<ObjectId, std::array<double, 3>> angles, std::size_t joint,
                            const std::vector<PosedPart>& parts, double smallest)
{
    const Joint& j = joints.at(joint);
    // The parts this joint moves: those whose way back to the held part goes through it.
    std::set<ObjectId> moving;
    for (const auto& [id, parent] : rig.parent) {
        for (ObjectId at = id;;) {
            const auto p = rig.parent.find(at);
            if (p == rig.parent.end())
                break;
            if (rig.joint.at(at) == joint) {
                moving.insert(id);
                break;
            }
            at = p->second;
        }
    }
    for (const auto& [id, with] : rig.rides)
        if (moving.count(with))
            moving.insert(id);

    const auto clear = [&](double angle) {
        angles[j.step] = {angle, 0.0, 0.0};
        const auto moved = transforms(joints, rig, angles);
        std::vector<PosedPart> posed = parts;
        std::vector<MeshSolid> placed;
        std::vector<Bounds> boxes;
        std::set<std::size_t> inSet;
        for (std::size_t i = 0; i < posed.size(); ++i) {
            const auto m = moved.find(posed[i].id);
            posed[i].moved = m == moved.end() ? Transform{} : m->second;
            placed.push_back(meshkernel::transformed(posed[i].solid, posed[i].moved));
            boxes.push_back(meshkernel::bounds(placed.back()));
            if (moving.count(posed[i].id))
                inSet.insert(i);
        }
        return hitsBetween(posed, placed, boxes, &inSet, smallest, true).empty();
    };

    std::array<double, 2> out{0.0, 0.0};
    for (int side = 0; side < 2; ++side) {
        const double s = side == 0 ? -1.0 : 1.0;
        double ok = 0.0;
        // A joint with clicks only rests on a click: try those.
        const bool clicky = j.clicks > 0;
        double step = clicky ? 360.0 / j.clicks : 5.0;
        double a = step;
        bool hit = false;
        while (a <= 180.0 + 1e-9) {
            if (!clear(s * a)) {
                hit = true;
                break;
            }
            ok = a;
            a += step;
        }
        if (hit && !clicky) {
            // Closer: a degree at a time from the last clear angle.
            for (double b = ok + 1.0; b < a - 1e-9; b += 1.0) {
                if (!clear(s * b))
                    break;
                ok = b;
            }
        }
        out[side] = s * ok;
    }
    return out;
}

std::vector<Room> room(const std::vector<Joint>& joints, const Rig& rig,
                       std::map<ObjectId, std::array<double, 3>> angles, std::size_t joint, int which,
                       double target, const std::vector<PosedPart>& parts, bool fromTurning, double clearance)
{
    const Joint& j = joints.at(joint);
    if (which < 0 || which > 2)
        throw std::invalid_argument("Unknown angle to turn.");
    if (!std::isfinite(target) || std::abs(target) > 360.0)
        throw std::invalid_argument("The angle to turn to must be from -360° to 360°.");
    if (!(clearance >= 0.0 && clearance <= 10.0))
        throw std::invalid_argument("The clearance must be from 0 to 10 mm.");
    // The parts this joint moves (as in range): those whose way back to the held part
    // goes through it, and what rides with them.
    std::set<ObjectId> moving;
    for (const auto& [id, parent] : rig.parent) {
        for (ObjectId at = id;;) {
            const auto p = rig.parent.find(at);
            if (p == rig.parent.end())
                break;
            if (rig.joint.at(at) == joint) {
                moving.insert(id);
                break;
            }
            at = p->second;
        }
    }
    for (const auto& [id, with] : rig.rides)
        if (moving.count(with))
            moving.insert(id);
    if (moving.empty())
        throw std::invalid_argument("Nothing turns in this joint.");

    std::array<double, 3> now = angles.count(j.step) ? angles[j.step] : std::array<double, 3>{0, 0, 0};
    const double start = now[std::size_t(which)];
    const auto posedAt = [&](double angle) {
        std::array<double, 3> a = now;
        a[std::size_t(which)] = angle;
        angles[j.step] = a;
        return transforms(joints, rig, angles);
    };
    const auto movedOf = [](const std::map<ObjectId, Transform>& moved, ObjectId id) {
        const auto m = moved.find(id);
        return m == moved.end() ? Transform{} : m->second;
    };

    // The pairs that touch already, as they are: left alone.
    std::set<std::pair<ObjectId, ObjectId>> touching;
    {
        const auto moved = posedAt(start);
        std::vector<PosedPart> posed = parts;
        for (PosedPart& p : posed)
            p.moved = movedOf(moved, p.id);
        for (const Hit& h : collisions(posed))
            touching.insert({std::min(h.a, h.b), std::max(h.a, h.b)});
    }
    std::vector<std::size_t> turning, still;
    for (std::size_t i = 0; i < parts.size(); ++i)
        (moving.count(parts[i].id) ? turning : still).push_back(i);

    // Steps small enough that the scallops left between them are under a micron deep
    // where the turning parts reach farthest from the pivot.
    double reach = 1.0;
    for (std::size_t t : turning) {
        const Bounds b = meshkernel::bounds(parts[t].solid);
        for (int c = 0; c < 8; ++c) {
            const Vec3 d{((c & 1) ? b.max[0] : b.min[0]) - j.pivot[0], ((c & 2) ? b.max[1] : b.min[1]) - j.pivot[1],
                         ((c & 4) ? b.max[2] : b.min[2]) - j.pivot[2]};
            reach = std::max(reach, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
        }
    }
    const double stepDegrees = std::clamp(2.0 * std::sqrt(2.0 * 0.0002 / reach) * 180.0 / 3.14159265358979, 0.1, 1.0);
    const double way = target >= start ? 1.0 : -1.0;
    const double span = std::abs(target - start);
    const int steps = std::max(1, int(std::ceil(span / stepDegrees)));
    // As far as it could go: the sweep, and the most the clearance could add.
    const double most = start + way * (span + 20.0);
    std::vector<double> sweepAngles;
    for (int i = 1; i <= steps; ++i)
        sweepAngles.push_back(start + way * span * i / steps);

    const auto corners = [](const Bounds& b) {
        std::array<Vec3, 8> out;
        for (int c = 0; c < 8; ++c)
            out[std::size_t(c)] = {(c & 1) ? b.max[0] : b.min[0], (c & 2) ? b.max[1] : b.min[1], (c & 4) ? b.max[2] : b.min[2]};
        return out;
    };
    const auto boxSolid = [](const Bounds& b) {
        std::vector<float> p;
        for (int c = 0; c < 8; ++c)
            p.insert(p.end(), {float((c & 1) ? b.max[0] : b.min[0]), float((c & 2) ? b.max[1] : b.min[1]),
                               float((c & 4) ? b.max[2] : b.min[2])});
        const std::vector<std::uint32_t> idx{0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
                                             2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5};
        return meshkernel::fromTriangles(p, idx);
    };
    const auto grow = [](Bounds b, double by) {
        for (std::size_t k = 0; k < 3; ++k) {
            b.min[k] -= float(by);
            b.max[k] += float(by);
        }
        return b;
    };
    const auto overlaps = [](const Bounds& a, const Bounds& b) {
        for (std::size_t k = 0; k < 3; ++k)
            if (a.max[k] < b.min[k] || b.max[k] < a.min[k])
                return false;
        return true;
    };
    // The angles to look at for how far things could go (the ends, and every few degrees).
    std::vector<double> coarse;
    for (int i = 0; i <= 72; ++i)
        coarse.push_back(start + (most - start) * i / 72.0);

    // The still parts don't move as this joint turns: placed once, and cut down to where
    // the turning parts ever get to (their boxes, turned all the way).
    const auto firstMoved = posedAt(start);
    std::vector<MeshSolid> placedStill(parts.size());
    std::vector<Bounds> boxStill(parts.size());
    std::vector<bool> stillReached(parts.size(), false), turningNear(parts.size(), false);
    Bounds swept;
    bool anySwept = false;
    for (double a : coarse) {
        const auto moved = posedAt(a);
        for (std::size_t t : turning) {
            const Transform tm = movedOf(moved, parts[t].id);
            for (const Vec3& c : corners(meshkernel::bounds(parts[t].solid))) {
                const Vec3 w = tm.apply(c);
                swept.expand(float(w[0]), float(w[1]), float(w[2]));
                anySwept = true;
            }
        }
    }
    // Turning a box's corners by a few degrees between the looks can bulge past them:
    // a margin for that.
    const double bulge = reach * (1.0 - std::cos((std::abs(most - start) / 72.0) * 3.14159265358979 / 180.0)) + 0.2;
    swept = grow(swept, bulge);
    for (std::size_t s : still) {
        MeshSolid placed = meshkernel::transformed(parts[s].solid, movedOf(firstMoved, parts[s].id));
        const Bounds box = meshkernel::bounds(placed);
        if (anySwept && overlaps(box, swept)) {
            Bounds keep;
            for (std::size_t k = 0; k < 3; ++k) {
                keep.min[k] = std::max(box.min[k], swept.min[k]);
                keep.max[k] = std::min(box.max[k], swept.max[k]);
            }
            keep.valid = true;
            placed = meshkernel::intersected({placed, boxSolid(grow(keep, 0.01))});
            if (!meshkernel::isEmpty(placed)) {
                placedStill[s] = placed;
                boxStill[s] = meshkernel::bounds(placed);  // (worked out now, before the threads share it)
                stillReached[s] = true;
            }
        }
    }
    // Likewise the turning parts: only what comes near the still parts that are left.
    Bounds stillBox;
    for (std::size_t s : still)
        if (stillReached[s])
            for (const Vec3& c : corners(boxStill[s]))
                stillBox.expand(float(c[0]), float(c[1]), float(c[2]));
    std::vector<MeshSolid> turningSolid(parts.size());
    for (std::size_t t : turning) {
        Bounds near;
        if (stillBox.valid)
            for (double a : coarse) {
                const Transform back = movedOf(posedAt(a), parts[t].id).inverse();
                for (const Vec3& c : corners(stillBox)) {
                    const Vec3 w = back.apply(c);
                    near.expand(float(w[0]), float(w[1]), float(w[2]));
                }
            }
        if (!near.valid)
            continue;
        turningSolid[t] = meshkernel::intersected({parts[t].solid, boxSolid(grow(near, bulge))});
        if (!meshkernel::isEmpty(turningSolid[t])) {
            (void)meshkernel::bounds(turningSolid[t]);  // worked out now, before the threads share it
            turningNear[t] = true;
        }
    }

    // Where the turning parts go into still ones, at each angle, mapped back to the part
    // being cut: the angles shared among the processor's cores.
    struct Found {
        std::map<ObjectId, std::vector<MeshSolid>> pieces;
        double farthest = 0.0;  // from the pivot, of anything that went in
    };
    const auto sweepOne = [&](double angle, const std::map<ObjectId, Transform>& moved, Found& found) {
        for (std::size_t t : turning) {
            if (!turningNear[t])
                continue;
            const Transform tm = movedOf(moved, parts[t].id);
            const MeshSolid placed = meshkernel::transformed(turningSolid[t], tm);
            const Bounds box = meshkernel::bounds(placed);
            for (std::size_t s : still) {
                if (touching.count({std::min(parts[t].id, parts[s].id), std::max(parts[t].id, parts[s].id)}))
                    continue;
                if (!stillReached[s] || !overlaps(box, boxStill[s]))
                    continue;
                const MeshSolid overlap = meshkernel::intersected({placed, placedStill[s]});
                if (meshkernel::isEmpty(overlap) || meshkernel::volume(overlap) < 1e-6)
                    continue;
                for (const Vec3& corner : corners(meshkernel::bounds(overlap))) {
                    const Vec3 d{corner[0] - j.pivot[0], corner[1] - j.pivot[1], corner[2] - j.pivot[2]};
                    found.farthest = std::max(found.farthest, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
                }
                if (fromTurning)
                    found.pieces[parts[t].id].push_back(meshkernel::transformed(overlap, tm.inverse()));
                else
                    found.pieces[parts[s].id].push_back(
                        meshkernel::transformed(overlap, movedOf(moved, parts[s].id).inverse()));
            }
        }
        (void)angle;
    };
    std::map<ObjectId, std::vector<MeshSolid>> pieces;
    double farthest = 0.0;
    const auto sweepAll = [&](const std::vector<double>& angles) {
        // The joints' transforms worked out here (they share the angles map), the
        // booleans in the threads.
        std::vector<std::map<ObjectId, Transform>> moves;
        for (double a : angles)
            moves.push_back(posedAt(a));
        const unsigned cores = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
        std::vector<Found> found(cores);
        std::atomic<std::size_t> next{0};
        std::exception_ptr failed;
        std::mutex failing;
        const auto work = [&](unsigned w) {
            try {
                for (std::size_t i = next++; i < angles.size(); i = next++)
                    sweepOne(angles[i], moves[i], found[w]);
            } catch (...) {
                const std::lock_guard<std::mutex> lock(failing);
                failed = std::current_exception();
            }
        };
        std::vector<std::thread> threads;
        for (unsigned w = 1; w < cores; ++w)
            threads.emplace_back(work, w);
        work(0);
        for (auto& th : threads)
            th.join();
        if (failed)
            std::rethrow_exception(failed);
        for (Found& f : found) {
            farthest = std::max(farthest, f.farthest);
            for (auto& [id, list] : f.pieces)
                pieces[id].insert(pieces[id].end(), list.begin(), list.end());
        }
    };
    sweepAll(sweepAngles);
    // A little past it, so there's `clearance` to spare where the parts meet.
    if (clearance > 0.0 && farthest > 0.0) {
        const double extra = std::clamp(clearance / farthest * 180.0 / 3.14159265358979, 0.5, 20.0);
        const int more = std::max(1, int(std::ceil(extra / stepDegrees)));
        std::vector<double> past;
        for (int i = 1; i <= more; ++i)
            past.push_back(target + way * extra * i / more);
        sweepAll(past);
    }

    std::vector<Room> out;
    for (auto& [id, list] : pieces) {
        Room r;
        r.part = id;
        r.cut = list.size() == 1 ? list.front() : meshkernel::merged(list);
        r.volume = meshkernel::volume(r.cut);
        if (r.volume > 1e-6)
            out.push_back(std::move(r));
    }
    return out;
}

}  // namespace forge::pose

namespace forge {

std::vector<ObjectId> makeRoom(CommandStack& stack, Document& doc, const std::vector<pose::Room>& rooms,
                               const std::string& what)
{
    if (rooms.empty())
        throw std::invalid_argument("Nothing is in the way: it turns that far already.");
    for (const pose::Room& r : rooms)
        if (!doc.find(r.part) || !doc.isTopLevel(r.part))
            throw std::invalid_argument("A part to cut isn't on the plate any more.");
    std::vector<ObjectId> made;
    stack.beginMacro("Make room for " + what);
    try {
        for (const pose::Room& r : rooms) {
            Mesh cut;
            meshkernel::triangles(r.cut, cut.positions, cut.indices);
            Body cutter = hinge::pieceBody(cut, "Room for " + what);
            cutter.id = doc.allocateId();
            stack.execute(std::make_unique<AddBodyCommand>(cutter));
            BooleanSpec spec;
            spec.op = BooleanOp::Subtract;
            spec.operands = {r.part, cutter.id};
            const auto preview = doc.previewResult(spec);
            const auto c = preview.bounds.center();
            spec.pivot = {c[0], c[1], c[2]};
            Body result;
            result.id = doc.allocateId();
            result.name = doc.find(r.part)->name;
            result.boolean = std::move(spec);
            made.push_back(result.id);
            stack.execute(std::make_unique<CombineCommand>(std::move(result)));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

}  // namespace forge
