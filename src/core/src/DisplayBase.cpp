// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/DisplayBase.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"
#include "forge/core/Emboss.h"
#include "forge/core/Hinge.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Socket.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <stdexcept>

namespace forge {

namespace {

constexpr double kPi = std::numbers::pi;

void put(Mesh& m, double x, double y, double z)
{
    m.positions.insert(m.positions.end(), {float(x), float(y), float(z)});
}

// A closed shape of stacked outlines (each a ring of the same number of points,
// anticlockwise seen from above, at its height), capped top and bottom.
Mesh stacked(const std::vector<std::pair<std::vector<std::array<double, 2>>, double>>& rings)
{
    Mesh m;
    const auto n = std::uint32_t(rings.front().first.size());
    for (const auto& [ring, z] : rings)
        for (const auto& p : ring)
            put(m, p[0], p[1], z);
    const auto last = std::uint32_t(rings.size() - 1);
    double cx = 0, cy = 0;
    for (const auto& p : rings.front().first) {
        cx += p[0];
        cy += p[1];
    }
    cx /= n;
    cy /= n;
    const auto c0 = std::uint32_t(m.positions.size() / 3);
    put(m, cx, cy, rings.front().second);
    double tx = 0, ty = 0;
    for (const auto& p : rings.back().first) {
        tx += p[0];
        ty += p[1];
    }
    put(m, tx / n, ty / n, rings.back().second);
    for (std::uint32_t k = 0; k < last; ++k)
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint32_t j = (i + 1) % n, a = k * n, b = (k + 1) * n;
            m.indices.insert(m.indices.end(), {a + i, a + j, b + j, a + i, b + j, b + i});
        }
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t j = (i + 1) % n;
        m.indices.insert(m.indices.end(), {c0, j, i, c0 + 1, last * n + i, last * n + j});
    }
    return m;
}

// A round rod straight up, from z0 to z1, its top edge taken off by `tip`.
Mesh rod(double x, double y, double r, double z0, double z1, double tip)
{
    const int n = 48;
    std::vector<std::pair<std::vector<std::array<double, 2>>, double>> rings;
    const auto ring = [&](double radius) {
        std::vector<std::array<double, 2>> out;
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * kPi * i / n;
            out.push_back({x + radius * std::cos(a), y + radius * std::sin(a)});
        }
        return out;
    };
    rings.push_back({ring(r), z0});
    if (tip > 0.0) {
        rings.push_back({ring(r), z1 - tip});
        rings.push_back({ring(r - tip), z1});
    } else {
        rings.push_back({ring(r), z1});
    }
    return stacked(rings);
}

}  // namespace

namespace displaybase {

void check(const Settings& s)
{
    if (!(s.margin >= 0.0 && s.margin <= 200.0))
        throw std::invalid_argument("The base's margin must be from 0 to 200 mm.");
    if (!(s.thickness >= 1.0 && s.thickness <= 100.0))
        throw std::invalid_argument("The base must be from 1 to 100 mm thick.");
    if (!(s.bevel >= 0.0 && s.bevel <= s.thickness * 0.8))
        throw std::invalid_argument("The base's bevel must be less than its thickness.");
    if (!(s.pegSize >= 0.5 && s.pegSize <= 30.0))
        throw std::invalid_argument("The pegs must be from 0.5 to 30 mm across.");
    if (!(s.pegHeight >= 0.5 && s.pegHeight <= 50.0))
        throw std::invalid_argument("The pegs must stand from 0.5 to 50 mm.");
    if (!s.pegs && !(s.pegHeight < s.thickness))
        throw std::invalid_argument("The holes must be less deep than the base is thick.");
    if (!(s.fit >= 0.0 && s.fit <= 1.0))
        throw std::invalid_argument("The fit must be from 0 to 1 mm.");
}

const char* title(Shape s)
{
    switch (s) {
    case Shape::Oval: return "Oval";
    case Shape::Square: return "Square";
    case Shape::Hexagon: return "Hexagon";
    default: return "Round";
    }
}

Mesh make(const Settings& s, const balance::Balance& b, const std::vector<Vec3>& spots)
{
    check(s);
    // What it must reach round: everything on the ground and the centre of weight.
    std::vector<std::array<double, 2>> reach = b.support;
    reach.push_back({b.centre[0], b.centre[1]});
    for (const Vec3& p : spots)
        reach.push_back({p[0], p[1]});
    double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
    for (const auto& p : reach) {
        x0 = std::min(x0, p[0]);
        x1 = std::max(x1, p[0]);
        y0 = std::min(y0, p[1]);
        y1 = std::max(y1, p[1]);
    }
    const double cx = (x0 + x1) / 2.0, cy = (y0 + y1) / 2.0;
    double far = 0.0;  // from the middle
    for (const auto& p : reach)
        far = std::max(far, std::hypot(p[0] - cx, p[1] - cy));
    const double hx = (x1 - x0) / 2.0 + s.margin, hy = (y1 - y0) / 2.0 + s.margin, r = far + s.margin;

    // The outline at an inset `in` (for the bevel).
    const auto outline = [&](double in) {
        std::vector<std::array<double, 2>> out;
        switch (s.shape) {
        case Shape::Round:
        case Shape::Oval: {
            // An oval wide enough to hold the corners of the box round the feet.
            const double ex = (s.shape == Shape::Round ? r : (hx - s.margin) * std::sqrt(2.0) + s.margin) - in;
            const double ey = (s.shape == Shape::Round ? r : (hy - s.margin) * std::sqrt(2.0) + s.margin) - in;
            const int n = 128;
            for (int i = 0; i < n; ++i) {
                const double a = 2.0 * kPi * i / n;
                out.push_back({cx + ex * std::cos(a), cy + ey * std::sin(a)});
            }
            break;
        }
        case Shape::Square: {
            const double w = hx - in, d = hy - in;
            out = {{cx - w, cy - d}, {cx + w, cy - d}, {cx + w, cy + d}, {cx - w, cy + d}};
            break;
        }
        case Shape::Hexagon: {
            const double R = r / std::cos(kPi / 6.0) - in / std::cos(kPi / 6.0);
            for (int i = 0; i < 6; ++i) {
                const double a = 2.0 * kPi * i / 6.0;
                out.push_back({cx + R * std::cos(a), cy + R * std::sin(a)});
            }
            break;
        }
        }
        return out;
    };
    const double top = b.ground, bottom = top - s.thickness;
    std::vector<std::pair<std::vector<std::array<double, 2>>, double>> rings{{outline(0.0), bottom}};
    if (s.bevel > 0.0) {
        rings.push_back({outline(0.0), top - s.bevel});
        rings.push_back({outline(s.bevel), top});
    } else {
        rings.push_back({outline(0.0), top});
    }
    const Mesh plate = stacked(rings);
    MeshSolid solid = meshkernel::fromTriangles(plate.positions, plate.indices);
    std::vector<MeshSolid> pegs;
    for (const Vec3& p : spots) {
        const Mesh m = s.pegs ? rod(p[0], p[1], s.pegSize / 2.0, top - 1.0, top + s.pegHeight,
                                    std::min(0.3, s.pegSize * 0.1))
                              : rod(p[0], p[1], s.pegSize / 2.0 + s.fit, top - s.pegHeight, top + 1.0, 0.0);
        pegs.push_back(meshkernel::fromTriangles(m.positions, m.indices));
    }
    if (!pegs.empty()) {
        if (s.pegs) {
            pegs.insert(pegs.begin(), solid);
            solid = meshkernel::merged(pegs);
        } else {
            solid = meshkernel::subtracted(solid, pegs);
        }
    }
    Mesh out;
    meshkernel::triangles(solid, out.positions, out.indices);
    return out;
}

}  // namespace displaybase

std::vector<Vec3> footSpots(const balance::Balance& b)
{
    std::vector<Vec3> out;
    for (const balance::Foot& f : b.feet)
        if (f.area >= 1.0)
            out.push_back(f.centre);
    return out;
}

std::size_t addFootHoles(CommandStack& stack, Document& doc, const std::vector<balance::PegSpot>& spots,
                         const SocketFinish& peg)
{
    socket::check(peg);
    // Where each part is now: a second hole in the same part goes into the step the first made.
    std::map<ObjectId, ObjectId> top;
    const auto already = [&](ObjectId part, const Vec3& p) {
        for (ObjectId id : doc.withDescendants(part)) {
            const Body* step = doc.find(id);
            if (!step || !step->isBoolean() || step->boolean->op != BooleanOp::Socket)
                continue;
            const emboss::SurfacePoint at = socketSpot(doc, id);
            if (std::hypot(at.point[0] - p[0], at.point[1] - p[1], at.point[2] - p[2]) < 0.6)
                return true;
        }
        return false;
    };
    std::vector<const balance::PegSpot*> todo;
    for (const balance::PegSpot& spot : spots) {
        if (already(spot.part, spot.point))
            continue;
        // Refuse before changing anything.
        doc.previewMesh(socketSpec(doc, spot.part, emboss::SurfacePoint{spot.point, spot.normal}, peg));
        todo.push_back(&spot);
    }
    for (const balance::PegSpot* spot : todo) {
        const auto found = top.find(spot->part);
        const ObjectId into = found == top.end() ? spot->part : found->second;
        Body r;
        r.id = doc.allocateId();
        r.name = doc.find(into)->name;
        r.boolean = socketSpec(doc, into, emboss::SurfacePoint{spot->point, spot->normal}, peg);
        stack.execute(std::make_unique<CombineCommand>(r));
        top[spot->part] = r.id;
    }
    return todo.size();
}

bool isDisplayBase(const Document& doc, ObjectId id)
{
    const Body* b = doc.find(id);
    return b && b->name.rfind("Display base", 0) == 0;
}

ObjectId addDisplayBase(CommandStack& stack, Document& doc, const std::vector<pose::PosedPart>& posed,
                        const displaybase::Settings& s, const std::optional<SocketFinish>& holesInFeet)
{
    displaybase::check(s);
    std::vector<pose::PosedPart> parts;  // the figure, not a base made before
    for (const pose::PosedPart& p : posed)
        if (!isDisplayBase(doc, p.id))
            parts.push_back(p);
    if (parts.empty())
        throw std::invalid_argument("There's no figure to make a base for.");
    const balance::Balance b = balance::of(parts);
    const std::vector<Vec3> spots = footSpots(b);
    const Mesh mesh = displaybase::make(s, b, spots);
    if (mesh.indices.empty())
        throw std::invalid_argument("The base came out empty.");
    Body base = hinge::pieceBody(mesh, "Display base");
    base.id = doc.allocateId();

    std::vector<balance::PegSpot> holeSpots;
    if (holesInFeet) {
        socket::check(*holesInFeet);
        holeSpots = balance::pegSpots(parts, b);
    }
    stack.beginMacro("Make a display base");
    try {
        stack.execute(std::make_unique<AddBodyCommand>(base));
        if (holesInFeet)
            addFootHoles(stack, doc, holeSpots, *holesInFeet);
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return base.id;
}

}  // namespace forge
