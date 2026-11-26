// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/BrickFigure.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/FigureStyle.h"
#include "forge/core/Hinge.h"
#include "forge/core/LimbCut.h"
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

// The body's sizes, as shares of the whole height T (the head, its neck included, the top
// 30% of it, as usual).
struct Sizes {
    double T;
    double legW, legD, legTop;        // each leg; its top (under the hips)
    double footD, footH;              // the foot reaching forward
    double hipW, hipBottom, hipTop;   // the hips, between the legs' side plates
    double plate;                     // the legs' side plates, up beside the hips
    double torsoW, torsoD, torsoTop;  // the torso
    double armR, shoulderZ, elbowZ, handR, handForward, handZ;
    double pegR, pegL;                // the pegs (hips and shoulders)
    double postR, postH;
    double round;                     // edges rounded
};

Sizes sizesFor(const Settings& s)
{
    const double T = s.height;
    // The body is 70% of the height with a head of the usual size; a bigger head leaves the
    // body as it is (the figure taller).
    Sizes z{};
    z.T = T;
    z.round = 0.02 * T;
    z.legW = 0.15 * T;
    z.legD = 0.14 * T;
    z.legTop = 0.32 * T;
    z.footD = 0.19 * T;
    z.footH = 0.06 * T;
    z.hipW = 0.20 * T;
    z.hipBottom = 0.30 * T;
    z.hipTop = 0.40 * T;
    z.plate = 0.05 * T;
    z.torsoW = 0.36 * T;
    z.torsoD = 0.16 * T;
    z.torsoTop = 0.70 * T;
    z.armR = 0.045 * T;
    z.shoulderZ = 0.64 * T;
    z.elbowZ = 0.50 * T;
    z.handR = 0.055 * T;
    z.handForward = 0.09 * T;
    z.handZ = 0.45 * T;
    z.pegR = std::max(0.9, 0.025 * T);
    z.pegL = 0.045 * T;
    z.postR = std::max(1.0, 0.04 * T);
    z.postH = 0.05 * T;
    return z;
}

// x: +1 the figure's left (+X), -1 its right.
Field legField(const Sizes& z, double side, double fit, bool joints = true)
{
    return [=](const Vec3& p) {
        const double cx = side * (z.hipW / 2 + z.legW / 2 - z.plate);
        // The leg, its foot forward, and its side plate up beside the hips (outside them).
        double d = box(p, {cx, 0, z.legTop / 2}, {z.legW / 2, z.legD / 2, z.legTop / 2}, z.round);
        d = std::min(d, box(p, {cx, -(z.footD - z.legD) / 2, z.footH / 2}, {z.legW / 2, z.footD / 2, z.footH / 2},
                            z.round));
        // (from the hips' side, the fit away, to the leg's outside; up to just under the torso)
        const double in = z.hipW / 2 + fit, out = std::abs(cx) + z.legW / 2;
        const double top = z.hipTop - 0.01 * z.T - fit, bottom = z.legTop - 2 * z.round;
        d = std::min(d, box(p, {side * (in + out) / 2, 0, (top + bottom) / 2}, {(out - in) / 2, z.legD / 2, (top - bottom) / 2},
                            z.round));
        // Kept clear of the hips (they turn under it) and its hole for the hip's peg.
        const double clear = -box(p, {0, 0, (z.hipBottom + z.hipTop) / 2 + z.T},
                                  {z.hipW / 2 + fit, z.legD, (z.hipTop - z.hipBottom) / 2 + z.T}, 0.0);
        d = std::max(d, clear);
        // (only as deep as the peg reaches, so it doesn't show outside)
        const double reach = z.hipW / 2 + z.pegL + fit;
        if (!joints)
            return d;
        const double hole = -cylinderX(p, 0, (z.hipBottom + z.hipTop) / 2, z.pegR + fit, side > 0 ? -z.T : -reach,
                                       side > 0 ? reach : z.T);
        return std::max(d, hole);
    };
}

Field armField(const Sizes& z, double side, double fit, double grip, bool joints = true)
{
    return [=](const Vec3& p) {
        const double x = side * (z.torsoW / 2 + fit + z.armR);
        const Vec3 shoulder{x, 0, z.shoulderZ}, elbow{x, 0, z.elbowZ}, hand{x, -z.handForward, z.handZ};
        double d = capsule(p, shoulder, elbow, z.armR);
        d = smoothUnion(d, capsule(p, elbow, hand, z.armR * 0.9), z.armR * 0.3);
        d = std::min(d, capsule(p, {x, 0, z.shoulderZ}, {x, 0, z.shoulderZ + 0.02 * z.T}, z.armR * 1.15));
        d = smoothUnion(d, len3(p[0] - hand[0], p[1] - hand[1], p[2] - (hand[2] - z.handR * 0.6)) - z.handR,
                        z.armR * 0.3);
        // The hole for the shoulder's peg, and the grip hole down through the hand.
        const double reach = std::abs(x) + fit;  // the peg reaches the arm's middle
        if (joints)
            d = std::max(d, -cylinderX(p, 0, z.shoulderZ, z.pegR + fit, side > 0 ? -z.T : -reach, side > 0 ? reach : z.T));
        if (grip > 0)
            d = std::max(d, -cylinderZ(p, hand[0], hand[1], grip / 2 + fit, -z.T, z.T));
        return d;
    };
}

Field torsoField(const Sizes& z, bool pegs)
{
    return [=](const Vec3& p) {
        // The torso a little narrower at the top than at the waist, its shoulders rounded.
        const double t = std::clamp((p[2] - z.hipTop) / (z.torsoTop - z.hipTop), 0.0, 1.0);
        const double halfW = z.torsoW / 2 * (1.0 - 0.06 * t);
        double d = box(p, {0, 0, (z.hipTop + z.torsoTop) / 2}, {halfW, z.torsoD / 2, (z.torsoTop - z.hipTop) / 2},
                       0.06 * z.T);
        d = std::min(d, box(p, {0, 0, (z.hipBottom + z.hipTop) / 2 + 0.01 * z.T},
                            {z.hipW / 2, z.legD / 2, (z.hipTop - z.hipBottom) / 2 + 0.01 * z.T}, z.round));
        // The neck's post the head sits on.
        d = std::min(d, cylinderZ(p, 0, 0, z.postR, z.torsoTop - z.round, z.torsoTop + z.postH));
        if (pegs) {
            // Pegs out of the hips' sides, and the shoulders'.
            const double hz = (z.hipBottom + z.hipTop) / 2;
            d = std::min(d, cylinderX(p, 0, hz, z.pegR, -(z.hipW / 2 + z.pegL), z.hipW / 2 + z.pegL));
            const double sx = z.torsoW / 2 - 0.02 * z.T;
            d = std::min(d, cylinderX(p, 0, z.shoulderZ, z.pegR, -(sx + 0.02 * z.T + z.pegL), sx + 0.02 * z.T + z.pegL));
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
    if (!(s.fit >= 0.0 && s.fit <= 1.0))
        throw std::invalid_argument("The fit must be from 0 to 1 mm.");
    const Sizes z = sizesFor(s);
    if (!(s.grip >= 0.0 && s.grip + 2 * s.fit <= 1.4 * z.handR))
        throw std::invalid_argument("The grip hole is too big for the hands at this height: make it smaller, or the "
                                    "figure taller.");
}

Seat seatFor(const Settings& s)
{
    const Sizes z = sizesFor(s);
    return {z.torsoTop, z.postR, z.postH};
}

std::vector<Part> body(const Settings& s)
{
    check(s);
    const Sizes z = sizesFor(s);
    const double T = z.T;
    const double edge = std::clamp(T / 220.0, 0.1, 1.0);
    const double grip = s.grip;
    const double W = z.torsoW / 2 + 2 * z.armR + 0.05 * T;
    const Vec3 lo{-W, -z.footD, -0.01 * T}, hi{W, z.torsoD, z.torsoTop + z.postH + 0.01 * T};
    if (s.build == Build::OnePiece) {
        const Field torso = torsoField(z, false);
        const Field left = legField(z, 1, 0, false), right = legField(z, -1, 0, false);
        const Field la = armField(z, 1, 0, grip, false), ra = armField(z, -1, 0, grip, false);
        const Field all = [&](const Vec3& p) {
            return std::min({torso(p), left(p), right(p), la(p), ra(p)});
        };
        return {{"Body", meshOf(all, lo, hi, edge)}};
    }
    std::vector<Part> out;
    out.push_back({"Torso", meshOf(torsoField(z, true), {-W, -z.torsoD, z.hipBottom - z.round}, hi, edge)});
    out.push_back({"Left arm", meshOf(armField(z, 1, s.fit, grip), {0, -z.T * 0.2, z.handZ - 2 * z.handR}, hi, edge)});
    out.push_back({"Right arm", meshOf(armField(z, -1, s.fit, grip), {-W, -z.T * 0.2, z.handZ - 2 * z.handR},
                                       {0, hi[1], hi[2]}, edge)});
    out.push_back({"Left leg", meshOf(legField(z, 1, s.fit), {0, lo[1], lo[2]}, {W, hi[1], z.hipTop + z.round}, edge)});
    out.push_back({"Right leg", meshOf(legField(z, -1, s.fit), {-W, lo[1], lo[2]}, {0, hi[1], z.hipTop + z.round},
                                       edge)});
    for (const Part& p : out)
        if (p.mesh.empty())
            throw std::invalid_argument("The " + p.name + " came out empty: make the figure taller.");
    return out;
}

Made make(CommandStack& stack, Document& doc, ObjectId figure, const rig::Points& points, const Settings& s)
{
    check(s);
    if (!doc.find(figure) || !doc.isTopLevel(figure))
        throw std::invalid_argument("Choose a figure on the plate.");
    // The figure on the plate, and its head: what's beyond the neck's narrowest point.
    Mesh surface = *doc.mesh(figure);
    const Transform toPlate = doc.worldTransform(figure);
    for (std::size_t i = 0; i + 2 < surface.positions.size(); i += 3) {
        const Vec3 p = toPlate.apply(Vec3{surface.positions[i], surface.positions[i + 1], surface.positions[i + 2]});
        for (std::size_t k = 0; k < 3; ++k)
            surface.positions[i + k] = float(p[k]);
    }
    const figurestyle::NeckCut nc = figurestyle::neckCut(surface, points);
    limbcut::Cut cut = limbcut::plan(surface, points, rig::Joint::Neck);
    for (int k = 0; k < 3; ++k)
        cut.finish.point[std::size_t(k)] = nc.point[std::size_t(k)];
    cut.finish.limb = true;
    cut.finish.gap = 0.0;
    const MeshSolid whole = meshkernel::transformed(doc.meshSolid(figure), toPlate);
    MeshSolid head = limbcut::apply(whole, cut.finish);
    if (meshkernel::isEmpty(head))
        throw std::invalid_argument("The head couldn't be found: check the figure stands up straight, facing the front.");

    // Sized and set on the body: its cut face down on the torso's top, upright, facing the
    // front as it did.
    const Sizes z = sizesFor(s);
    const Vec3 n = nc.normal;
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
            o += R[r][c] * nc.point[std::size_t(c)];
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
    grow.m[2][3] = z.torsoTop;
    head = meshkernel::transformed(head, grow);
    // The socket for the post, the fit all round and above it (one piece: none, the post
    // goes in solid).
    if (s.build == Build::Parts) {
        const double r = z.postR + s.fit;
        const double top = z.torsoTop + z.postH + s.fit;
        const Field socket = [&](const Vec3& p) { return cylinderZ(p, 0, 0, r, z.torsoTop - 1.0, top); };
        const Mesh sm = meshOf(socket, {-r, -r, z.torsoTop - 1.0}, {r, r, top}, std::max(0.05, r / 12.0));
        head = meshkernel::subtracted(head, {meshkernel::fromTriangles(sm.positions, sm.indices)});
    }
    const double neckWidth = 2.0 * nc.radius * scale;
    if (neckWidth < 2.0 * z.postR + 2.0 * s.fit + 0.8)
        throw std::invalid_argument("The head's neck is too thin round the post at this size: make the head bigger, "
                                    "or the figure taller.");
    Mesh headMesh;
    meshkernel::triangles(head, headMesh.positions, headMesh.indices);

    // Beside the figure, to its right on the plate.
    const Bounds fb = doc.worldBounds(figure);
    const Vec3 at{double(fb.max[0]) + 0.3 * z.T + 5.0, double(fb.center()[1]), 0.0};
    std::vector<Part> parts = body(s);
    parts.insert(parts.begin(), Part{"Head", std::move(headMesh)});
    if (s.build == Build::OnePiece) {
        // The head on the body as one shape too.
        const MeshSolid joined = meshkernel::merged(
            {meshkernel::fromTriangles(parts[0].mesh.positions, parts[0].mesh.indices),
             meshkernel::fromTriangles(parts[1].mesh.positions, parts[1].mesh.indices)});
        Mesh m;
        meshkernel::triangles(joined, m.positions, m.indices);
        parts = {Part{"Brick figure", std::move(m)}};
    }
    Made made;
    stack.beginMacro("Brick Figure");
    try {
        for (Part& p : parts) {
            moveBy(p.mesh, at);
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
