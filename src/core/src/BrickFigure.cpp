// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/BrickFigure.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FigureStyle.h"
#include "forge/core/Hinge.h"
#include "forge/core/LimbCut.h"
#include "forge/core/MaskSplit.h"
#include "forge/core/MeshKernel.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace forge::brickfig {

namespace {

double len3(double x, double y, double z) { return std::sqrt(x * x + y * y + z * z); }

// A box (centre, half sizes), its edges rounded by r. Negative inside.
double box(const Vec3& p, const Vec3& c, const Vec3& half, double r)
{
    const double q[3] = {std::abs(p[0] - c[0]) - half[0] + r, std::abs(p[1] - c[1]) - half[1] + r,
                         std::abs(p[2] - c[2]) - half[2] + r};
    return len3(std::max(q[0], 0.0), std::max(q[1], 0.0), std::max(q[2], 0.0)) +
           std::min(std::max({q[0], q[1], q[2]}), 0.0) - r;
}

// A rod with round ends from a to b.
double capsule(const Vec3& p, const Vec3& a, const Vec3& b, double r)
{
    const Vec3 pa{p[0] - a[0], p[1] - a[1], p[2] - a[2]}, ba{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double l2 = ba[0] * ba[0] + ba[1] * ba[1] + ba[2] * ba[2];
    const double h = l2 > 0 ? std::clamp((pa[0] * ba[0] + pa[1] * ba[1] + pa[2] * ba[2]) / l2, 0.0, 1.0) : 0.0;
    return len3(pa[0] - ba[0] * h, pa[1] - ba[1] * h, pa[2] - ba[2] * h) - r;
}

// A cylinder along X (from x0 to x1) or Z, flat ended.
double cylinderX(const Vec3& p, double y, double z, double r, double x0, double x1)
{
    const double d = std::hypot(p[1] - y, p[2] - z) - r;
    const double e = std::max(x0 - p[0], p[0] - x1);
    return std::max(d, e);
}
double cylinderZ(const Vec3& p, double x, double y, double r, double z0, double z1)
{
    const double d = std::hypot(p[0] - x, p[1] - y) - r;
    const double e = std::max(z0 - p[2], p[2] - z1);
    return std::max(d, e);
}

double smoothUnion(double a, double b, double k)
{
    const double h = std::clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return b * (1.0 - h) + a * h - k * h * (1.0 - h);
}

using Field = std::function<double(const Vec3&)>;

Mesh meshOf(const Field& f, const Vec3& lo, const Vec3& hi, double edge)
{
    Bounds b;
    b.expand(float(lo[0] - 2 * edge), float(lo[1] - 2 * edge), float(lo[2] - 2 * edge));
    b.expand(float(hi[0] + 2 * edge), float(hi[1] + 2 * edge), float(hi[2] + 2 * edge));
    const MeshSolid solid = meshkernel::levelSet([&](const Vec3& p) { return -f(p); }, b, edge, 0.0);
    Mesh m;
    meshkernel::triangles(solid, m.positions, m.indices);
    return m;
}

// The body's sizes, as shares of the whole height T, in the build of the familiar brick
// toy figures (the generic, compatible kind): a torso narrower at the shoulders than the
// waist, hips with an axle the legs hang from, square legs with flat fronts, arms that
// angle out and bend forward, C-shaped hands. The head, its neck included, is the top 30%.
struct Sizes {
    double T;
    double legW, legD, axleZ, footH, footForward;  // each leg; the axle it turns on
    double gap;                                     // between the legs (the hips' lug)
    double hipW, hipBottom, hipTop;
    double torsoBottomW, torsoTopW, torsoBottomD, torsoTopD, torsoTop;
    double shoulderZ, armR, foreR, handR, handH;
    double pegR, pegL;
    double postR, postH;
    double round;
};

Sizes sizesFor(const Settings& s)
{
    const double T = s.height;
    Sizes z{};
    z.T = T;
    z.round = 0.012 * T;
    z.hipW = 0.39 * T;
    z.hipBottom = 0.30 * T;
    z.hipTop = 0.38 * T;
    z.gap = 0.03 * T;
    z.legW = (z.hipW - z.gap) / 2;
    z.legD = 0.20 * T;
    z.axleZ = z.hipBottom - z.legD / 2;  // the leg's round top just under the hips (less the fit)
    z.footH = 0.06 * T;
    z.footForward = 0.015 * T;
    z.torsoBottomW = 0.39 * T;
    z.torsoTopW = 0.29 * T;
    z.torsoBottomD = 0.20 * T;
    z.torsoTopD = 0.18 * T;
    z.torsoTop = 0.70 * T;
    z.shoulderZ = 0.635 * T;
    z.armR = 0.05 * T;
    z.foreR = 0.045 * T;
    z.handR = 0.055 * T;
    z.handH = 0.065 * T;
    z.pegR = std::max(0.9, 0.025 * T);
    z.pegL = 0.045 * T;
    z.postR = std::max(1.0, 0.04 * T);
    z.postH = 0.05 * T;
    return z;
}

// The torso's half width and half depth at height zz (narrowing to the shoulders).
double torsoHalfW(const Sizes& z, double zz)
{
    const double t = std::clamp((zz - z.hipTop) / (z.torsoTop - z.hipTop), 0.0, 1.0);
    return (z.torsoBottomW + (z.torsoTopW - z.torsoBottomW) * t) / 2;
}
double torsoHalfD(const Sizes& z, double zz)
{
    const double t = std::clamp((zz - z.hipTop) / (z.torsoTop - z.hipTop), 0.0, 1.0);
    return (z.torsoBottomD + (z.torsoTopD - z.torsoBottomD) * t) / 2;
}

// x: +1 the figure's left (+X), -1 its right.
Field legField(const Sizes& z, double side, double fit, bool joints = true)
{
    return [=](const Vec3& p) {
        const double inner = z.gap / 2 + fit, outer = z.hipW / 2;
        const double cx = side * (inner + outer) / 2, hw = (outer - inner) / 2;
        const double axle = z.axleZ - fit;
        // A square leg up to the axle, its top round about it (so it turns under the hips),
        // the foot a little forward at the bottom.
        double d = box(p, {cx, 0, axle / 2}, {hw, z.legD / 2, axle / 2}, z.round);
        d = std::min(d, cylinderX(p, 0, axle, z.legD / 2, side > 0 ? inner : -outer, side > 0 ? outer : -inner) +
                            0.0);
        d = std::min(d, box(p, {cx, -z.footForward / 2, z.footH / 2}, {hw, z.legD / 2 + z.footForward / 2, z.footH / 2},
                            z.round));
        // A step in at the back of the leg, above the heel (as the toys have).
        d = std::max(d, -box(p, {cx, z.legD / 2, (z.footH + axle) / 2 - 0.02 * z.T},
                              {hw + 1.0, 0.02 * z.T, (axle - z.footH) / 2 - 0.05 * z.T}, 0.0));
        if (!joints)
            return d;
        // The hole for the axle's stub, from the inside only.
        const double reach = z.gap / 2 + z.pegL + fit;
        return std::max(d, -cylinderX(p, 0, axle, z.pegR + fit, side > 0 ? -z.T : -reach, side > 0 ? reach : z.T));
    };
}

// Where an arm's parts are: the shoulder (on the peg), the elbow (out and down, along the
// torso's slanting side), the wrist (forward) and the hand's middle.
struct ArmLine {
    Vec3 shoulder, elbow, wrist, hand;
};
ArmLine armLine(const Sizes& z, double side, double fit)
{
    const double sx = torsoHalfW(z, z.shoulderZ) + fit + z.armR;
    ArmLine a;
    a.shoulder = {side * sx, 0, z.shoulderZ};
    a.elbow = {side * (sx + 0.035 * z.T), 0, 0.48 * z.T};
    a.wrist = {side * (sx + 0.04 * z.T), -0.07 * z.T, 0.43 * z.T};
    a.hand = {side * (sx + 0.045 * z.T), -0.07 * z.T - z.handR, 0.41 * z.T};
    return a;
}

Field armField(const Sizes& z, double side, double fit, double grip, bool joints = true)
{
    return [=](const Vec3& p) {
        const ArmLine a = armLine(z, side, fit);
        double d = capsule(p, a.shoulder, a.elbow, z.armR);
        d = smoothUnion(d, capsule(p, a.elbow, a.wrist, z.foreR), z.foreR * 0.4);
        // The hand: a C, its hole up and down through it, open at the front.
        const double ring = cylinderZ(p, a.hand[0], a.hand[1], z.handR, a.hand[2] - z.handH / 2, a.hand[2] + z.handH / 2);
        double hand = ring;
        if (grip > 0) {
            hand = std::max(hand, -cylinderZ(p, a.hand[0], a.hand[1], grip / 2 + fit, -z.T, z.T));
            const double slot = 0.75 * grip / 2;
            hand = std::max(hand, -box(p, {a.hand[0], a.hand[1] - z.handR, a.hand[2]}, {slot, z.handR, z.T}, 0.0));
        }
        d = std::min(d, std::max(capsule(p, a.wrist, a.hand, z.foreR * 0.7),
                                 -cylinderZ(p, a.hand[0], a.hand[1], grip > 0 ? grip / 2 + fit : 0.0, -z.T, z.T)));
        d = std::min(d, hand);
        if (joints) {
            const double reach = std::abs(a.shoulder[0]) + fit;  // the peg reaches the shoulder's middle
            d = std::max(d, -cylinderX(p, 0, z.shoulderZ, z.pegR + fit, side > 0 ? -z.T : -reach, side > 0 ? reach : z.T));
        }
        return d;
    };
}

Field torsoField(const Sizes& z, bool pegs)
{
    return [=](const Vec3& p) {
        // The torso: narrower and a little thinner at the shoulders than at the waist.
        const double hw = torsoHalfW(z, p[2]), hd = torsoHalfD(z, p[2]);
        const double r = 0.02 * z.T;
        const double qx = std::abs(p[0]) - hw + r, qy = std::abs(p[1]) - hd + r;
        const double qz = std::max(z.hipTop - p[2], p[2] - z.torsoTop) + r;
        double d = len3(std::max(qx, 0.0), std::max(qy, 0.0), std::max(qz, 0.0)) +
                   std::min(std::max({qx, qy, qz}), 0.0) - r;
        // The hips, and the lug between the legs that carries their axle.
        d = std::min(d, box(p, {0, 0, (z.hipBottom + z.hipTop) / 2 + 0.005 * z.T},
                            {z.hipW / 2, z.torsoBottomD / 2, (z.hipTop - z.hipBottom) / 2 + 0.005 * z.T}, z.round));
        // The post the head sits on.
        d = std::min(d, cylinderZ(p, 0, 0, z.postR, z.torsoTop - z.round, z.torsoTop + z.postH));
        if (pegs) {
            const double lugR = z.pegR + 0.012 * z.T;
            d = std::min(d, box(p, {0, 0, (z.axleZ + z.hipBottom) / 2}, {z.gap / 2 - 0.15, lugR, (z.hipBottom - z.axleZ) / 2 + lugR / 2},
                                0.0));
            d = std::min(d, cylinderX(p, 0, z.axleZ, z.pegR, -(z.gap / 2 + z.pegL), z.gap / 2 + z.pegL));
            // The shoulders' pegs, out to the arms' middles.
            const double sx = torsoHalfW(z, z.shoulderZ) + z.armR;
            d = std::min(d, cylinderX(p, 0, z.shoulderZ, z.pegR, -sx, sx));
        }
        return d;
    };
}

void moveBy(Mesh& m, const Vec3& by)
{
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3)
        for (int k = 0; k < 3; ++k)
            m.positions[i + std::size_t(k)] += float(by[std::size_t(k)]);
}

}  // namespace

void check(const Settings& s)
{
    if (!(s.height >= kLeastHeight && s.height <= kMostHeight))
        throw std::invalid_argument("The brick figure must be from 20 to 400 mm tall.");
    if (!(s.headSize >= kLeastHeadSize && s.headSize <= kMostHeadSize))
        throw std::invalid_argument("The head's size must be from 60% to 160%.");
    if (!(std::abs(s.headForward) <= 0.2 * s.height))
        throw std::invalid_argument("The head can be moved at most a fifth of the height forward or back.");
    if (!(s.fit >= 0.0 && s.fit <= 1.0))
        throw std::invalid_argument("The fit must be from 0 to 1 mm.");
    const Sizes z = sizesFor(s);
    if (!(s.grip >= 0.0 && s.grip / 2 + s.fit <= 0.7 * z.handR))
        throw std::invalid_argument("The grip hole is too big for the hands at this height: make it smaller, or the "
                                    "figure taller.");
}

Seat seatFor(const Settings& s)
{
    const Sizes z = sizesFor(s);
    return {z.torsoTop, z.postR, z.postH};
}

std::vector<Part> body(const Settings& s, double coarseness)
{
    check(s);
    const Sizes z = sizesFor(s);
    const double T = z.T;
    const double edge = std::clamp(T / 220.0 * coarseness, 0.1, 3.0);
    const double grip = s.grip;
    const double W = torsoHalfW(z, z.shoulderZ) + 0.12 * T + 2 * z.handR;
    const Vec3 lo{-W, -0.25 * T, -0.01 * T}, hi{W, 0.15 * T, z.torsoTop + z.postH + 0.01 * T};
    if (s.build == Build::OnePiece) {
        const Field torso = torsoField(z, false);
        const Field left = legField(z, 1, 0, false), right = legField(z, -1, 0, false);
        const Field la = armField(z, 1, 0, grip, false), ra = armField(z, -1, 0, grip, false);
        // The legs joined to the hips (the axle's room filled).
        const Field fill = [&](const Vec3& p) {
            return box(p, {0, 0, (z.axleZ + z.hipBottom) / 2}, {z.hipW / 2, z.legD / 2 - z.round, (z.hipBottom - z.axleZ) / 2 + 0.01 * T},
                       z.round);
        };
        const Field all = [&](const Vec3& p) {
            return std::min({torso(p), left(p), right(p), la(p), ra(p), fill(p)});
        };
        return {{"Body", meshOf(all, lo, hi, edge)}};
    }
    std::vector<Part> out;
    out.push_back({"Torso", meshOf(torsoField(z, true), {-W, -0.15 * T, z.axleZ - 0.05 * T}, hi, edge)});
    out.push_back({"Left arm", meshOf(armField(z, 1, s.fit, grip), {0, lo[1], 0.3 * T}, hi, edge)});
    out.push_back({"Right arm", meshOf(armField(z, -1, s.fit, grip), {-W, lo[1], 0.3 * T}, {0, hi[1], hi[2]}, edge)});
    out.push_back({"Left leg", meshOf(legField(z, 1, s.fit), {0, lo[1], lo[2]}, {W, hi[1], z.hipBottom + z.round}, edge)});
    out.push_back({"Right leg", meshOf(legField(z, -1, s.fit), {-W, lo[1], lo[2]}, {0, hi[1], z.hipBottom + z.round},
                                       edge)});
    for (const Part& p : out)
        if (p.mesh.empty())
            throw std::invalid_argument("The " + p.name + " came out empty: make the figure taller.");
    return out;
}

namespace {

// The head as it comes off the figure (plate coordinates): the solid, the middle of its
// cut, the way up into it, and the neck's thickness there.
struct Cut {
    MeshSolid head;
    Vec3 point{0, 0, 0};
    Vec3 up{0, 0, 1};
    double neck = 0.0;
};

Cut headOf(const Document& doc, ObjectId figure, const rig::Points& points, const Settings& s)
{
    const Transform toPlate = doc.worldTransform(figure);
    Cut out;
    if (!s.headMask.empty()) {
        // What's masked, cut flat through the mask's edge (joined-up triangles, so the edge
        // can be followed round).
        std::vector<float> pos;
        std::vector<std::uint32_t> idx;
        meshkernel::triangles(doc.meshSolid(figure), pos, idx);
        for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
            const Vec3 p = toPlate.apply(Vec3{pos[i], pos[i + 1], pos[i + 2]});
            for (std::size_t k = 0; k < 3; ++k)
                pos[i + k] = float(p[k]);
        }
        const mask::Pieces pieces = mask::split(pos, idx, s.headMask, MaskCut::Flat);
        if (pieces.keySpots.empty())
            throw std::invalid_argument("The mask's edge must go round the neck once: paint the whole head.");
        const mask::KeySpot spot = *std::max_element(pieces.keySpots.begin(), pieces.keySpots.end(),
                                                     [](const auto& a, const auto& b) { return a.reach < b.reach; });
        out.head = meshkernel::fromTriangles(pieces.masked.positions, pieces.masked.indices);
        out.point = spot.point;
        out.neck = spot.room;
        Vec3 c{0, 0, 0};
        const std::size_t count = pieces.masked.positions.size() / 3;
        for (std::size_t i = 0; i < count; ++i)
            for (int k = 0; k < 3; ++k)
                c[std::size_t(k)] += pieces.masked.positions[3 * i + std::size_t(k)] / double(count);
        // The head is up above the shoulders: a mask anywhere else isn't on it.
        const Vec3 waist = points[std::size_t(rig::Point::Waist)], neck = points[std::size_t(rig::Point::Neck)];
        const Vec3 rise{neck[0] - waist[0], neck[1] - waist[1], neck[2] - waist[2]};
        const double r2 = rise[0] * rise[0] + rise[1] * rise[1] + rise[2] * rise[2];
        if (r2 > 1e-12 && ((c[0] - waist[0]) * rise[0] + (c[1] - waist[1]) * rise[1] + (c[2] - waist[2]) * rise[2]) / r2 < 0.6)
            throw std::invalid_argument("The mask isn't over the head: paint the whole head, down to where it should "
                                        "come off the neck.");
        const double l = len3(spot.normal[0], spot.normal[1], spot.normal[2]);
        out.up = l > 1e-9 ? Vec3{spot.normal[0] / l, spot.normal[1] / l, spot.normal[2] / l} : Vec3{0, 0, 1};
        if ((c[0] - out.point[0]) * out.up[0] + (c[1] - out.point[1]) * out.up[1] + (c[2] - out.point[2]) * out.up[2] < 0)
            out.up = {-out.up[0], -out.up[1], -out.up[2]};
    } else {
        // What's beyond the neck's cut (see figurestyle::neckCut).
        Mesh surface = *doc.mesh(figure);
        for (std::size_t i = 0; i + 2 < surface.positions.size(); i += 3) {
            const Vec3 p = toPlate.apply(Vec3{surface.positions[i], surface.positions[i + 1], surface.positions[i + 2]});
            for (std::size_t k = 0; k < 3; ++k)
                surface.positions[i + k] = float(p[k]);
        }
        const figurestyle::NeckCut nc = figurestyle::neckCut(surface, points, s.raise, s.tilt);
        limbcut::Tilt tilt;
        tilt.forward = nc.tilt;
        limbcut::Cut cut = limbcut::plan(surface, points, rig::Joint::Neck, tilt);
        for (int k = 0; k < 3; ++k) {
            cut.finish.point[std::size_t(k)] = nc.point[std::size_t(k)];
            cut.finish.normal[std::size_t(k)] = nc.normal[std::size_t(k)];
        }
        for (LimbRod& r : cut.finish.reach)
            r.radius *= 1.3;  // all the head, hair and all
        cut.finish.limb = true;
        cut.finish.gap = 0.0;
        out.head = limbcut::apply(meshkernel::transformed(doc.meshSolid(figure), toPlate), cut.finish);
        out.point = nc.point;
        out.up = nc.normal;
        out.neck = nc.radius;
    }
    if (meshkernel::isEmpty(out.head))
        throw std::invalid_argument("The head couldn't be found: check the figure stands up straight, facing the "
                                    "front, or paint a mask over the head.");
    return out;
}

}  // namespace

std::vector<Part> build(const Document& doc, ObjectId figure, const rig::Points& points, const Settings& s,
                        bool preview)
{
    check(s);
    if (!doc.find(figure) || !doc.isTopLevel(figure))
        throw std::invalid_argument("Choose a figure on the plate.");
    Cut cut = headOf(doc, figure, points, s);
    MeshSolid head = cut.head;

    // Sized and set on the body: its cut face down on the torso's top, upright, facing the
    // front as it did.
    const Sizes z = sizesFor(s);
    const Vec3 n = cut.up;
    // Rotation taking n to +Z (Rodrigues), then the scale, about the cut's middle.
    const Vec3 up{0, 0, 1};
    const Vec3 k{n[1] * up[2] - n[2] * up[1], n[2] * up[0] - n[0] * up[2], n[0] * up[1] - n[1] * up[0]};
    const double sinA = len3(k[0], k[1], k[2]), cosA = n[2];
    double R[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    if (sinA > 1e-9) {
        const Vec3 a{k[0] / sinA, k[1] / sinA, k[2] / sinA};
        const double K[3][3] = {{0, -a[2], a[1]}, {a[2], 0, -a[0]}, {-a[1], a[0], 0}};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c) {
                double kk = 0;
                for (int q = 0; q < 3; ++q)
                    kk += K[r][q] * K[q][c];
                R[r][c] += sinA * K[r][c] + (1 - cosA) * kk;
            }
    }
    Transform turn;
    for (int r = 0; r < 3; ++r) {
        double o = 0;
        for (int c = 0; c < 3; ++c) {
            turn.m[std::size_t(r)][std::size_t(c)] = R[r][c];
            o += R[r][c] * cut.point[std::size_t(c)];
        }
        turn.m[std::size_t(r)][3] = -o;  // the cut's middle to the origin
    }
    head = meshkernel::transformed(head, turn);
    const Bounds hb = meshkernel::bounds(head);
    const double tall = std::max(1e-6, double(hb.max[2]));
    const double scale = 0.30 * z.T * s.headSize / tall;  // the torso's top is at 70%
    Transform grow;
    for (int r = 0; r < 3; ++r)
        grow.m[std::size_t(r)][std::size_t(r)] = scale;
    grow.m[1][3] = -s.headForward;  // the figure faces -Y
    grow.m[2][3] = z.torsoTop;
    head = meshkernel::transformed(head, grow);
    // The socket for the post, the fit all round and above it (one piece: none, the post
    // goes in solid).
    if (s.build == Build::Parts && !preview) {
        const double r = z.postR + s.fit;
        // A neck too thin round the socket gets a collar (as the toys' heads have).
        const double wall = std::max(0.8, 0.02 * z.T);
        if (cut.neck * scale < r + wall) {
            const double cr = r + wall, ctop = z.torsoTop + z.postH + s.fit + wall;
            const Field collar = [&](const Vec3& p) { return cylinderZ(p, 0, 0, cr, z.torsoTop, ctop); };
            const Mesh cm = meshOf(collar, {-cr, -cr, z.torsoTop}, {cr, cr, ctop}, std::max(0.05, cr / 14.0));
            head = meshkernel::merged({head, meshkernel::fromTriangles(cm.positions, cm.indices)});
        }
        const double top = z.torsoTop + z.postH + s.fit;
        const Field socket = [&](const Vec3& p) { return cylinderZ(p, 0, 0, r, z.torsoTop - 1.0, top); };
        const Mesh sm = meshOf(socket, {-r, -r, z.torsoTop - 1.0}, {r, r, top}, std::max(0.05, r / 12.0));
        head = meshkernel::subtracted(head, {meshkernel::fromTriangles(sm.positions, sm.indices)});
    }
    Mesh headMesh;
    meshkernel::triangles(head, headMesh.positions, headMesh.indices);

    std::vector<Part> parts = body(s, preview ? 3.0 : 1.0);
    parts.insert(parts.begin(), Part{"Head", std::move(headMesh)});
    if (s.build == Build::OnePiece && !preview) {
        // The head on the body as one shape too.
        const MeshSolid joined = meshkernel::merged(
            {meshkernel::fromTriangles(parts[0].mesh.positions, parts[0].mesh.indices),
             meshkernel::fromTriangles(parts[1].mesh.positions, parts[1].mesh.indices)});
        Mesh m;
        meshkernel::triangles(joined, m.positions, m.indices);
        parts = {Part{"Brick figure", std::move(m)}};
    }
    // Beside the figure, to its right on the plate.
    const Bounds fb = doc.worldBounds(figure);
    const Vec3 at{double(fb.max[0]) + 0.3 * z.T + 5.0, double(fb.center()[1]), 0.0};
    for (Part& p : parts)
        moveBy(p.mesh, at);
    return parts;
}

Mesh preview(const Document& doc, ObjectId figure, const rig::Points& points, const Settings& s)
{
    Mesh out;
    for (const Part& part : build(doc, figure, points, s, true)) {
        // Shaded (with its facings), as a shape is shown.
        const Mesh p = meshkernel::displayMesh(meshkernel::fromTriangles(part.mesh.positions, part.mesh.indices));
        const auto base = std::uint32_t(out.positions.size() / 3);
        out.positions.insert(out.positions.end(), p.positions.begin(), p.positions.end());
        out.normals.insert(out.normals.end(), p.normals.begin(), p.normals.end());
        for (std::uint32_t i : p.indices)
            out.indices.push_back(base + i);
    }
    return out;
}

Made make(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points, const Settings& s)
{
    std::vector<Part> parts = build(doc, figure, points, s);
    Made made;
    stack.beginMacro("Brick Figure");
    try {
        for (Part& p : parts) {
            const std::string name = parts.size() == 1 ? p.name : "Brick " + [&] {
                std::string lower = p.name;
                lower[0] = char(std::tolower(static_cast<unsigned char>(lower[0])));
                return lower;
            }();
            Body b = hinge::pieceBody(p.mesh, name);
            b.id = doc.allocateId();
            b.name = doc.uniqueName(name);
            made.parts.push_back(b.id);
            stack.execute(std::make_unique<AddBodyCommand>(std::move(b)));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

}  // namespace forge::brickfig
