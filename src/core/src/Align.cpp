// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Align.h"

#include "forge/core/Document.h"

#include <array>
#include <cmath>
#include <stdexcept>

namespace forge {

std::string toString(AlignMode mode)
{
    switch (mode) {
    case AlignMode::MeetAtFace: return "Meet At Face";
    case AlignMode::FaceCentreXYZ: return "Align Face Center XYZ";
    case AlignMode::FaceCentreX: return "Align Face Center X";
    case AlignMode::FaceCentreY: return "Align Face Center Y";
    case AlignMode::FaceCentreZ: return "Align Face Center Z";
    case AlignMode::CentreXYZ: return "Center XYZ";
    case AlignMode::CentreX: return "Center X";
    case AlignMode::CentreY: return "Center Y";
    case AlignMode::CentreZ: return "Center Z";
    }
    return "Align";
}

namespace {

struct Box {
    std::array<double, 3> min, max;
    double centre(int a) const { return (min[a] + max[a]) / 2.0; }
    double half(int a) const { return (max[a] - min[a]) / 2.0; }
};

Box toBox(const Bounds& b)
{
    return {{b.min[0], b.min[1], b.min[2]}, {b.max[0], b.max[1], b.max[2]}};
}

// Shape 2 goes on the side of shape 1 where its centre already is (above when level).
bool onPlusSide(const Box& fixed, const Box& moving, int axis)
{
    return moving.centre(axis) >= fixed.centre(axis);
}

}  // namespace

int facingAxis(const Bounds& fixedBounds, const Bounds& movingBounds)
{
    const Box f = toBox(fixedBounds), m = toBox(movingBounds);
    int best = 2;
    double bestScore = -1.0;
    for (int a : {2, 1, 0}) {  // Z first so it wins ties
        const double span = f.half(a) + m.half(a);
        const double score = span > 0.0 ? std::abs(m.centre(a) - f.centre(a)) / span : 0.0;
        if (score > bestScore + 1e-9) {
            bestScore = score;
            best = a;
        }
    }
    return best;
}

Placement alignedPlacement(const Document& doc, ObjectId fixedId, ObjectId movingId, AlignMode mode)
{
    const Body* moving = doc.find(movingId);
    if (!moving || !doc.find(fixedId))
        throw std::out_of_range("No body with that id");
    if (!doc.isTopLevel(movingId) || !doc.isTopLevel(fixedId))
        throw std::invalid_argument("Only shapes on the plate can be aligned");

    const Bounds fb = doc.worldBounds(fixedId), mb = doc.worldBounds(movingId);
    Placement p = moving->placement;
    if (!fb.valid || !mb.valid)
        return p;
    const Box f = toBox(fb), m = toBox(mb);

    std::array<double, 3> delta{0.0, 0.0, 0.0};
    const auto centreOn = [&](int a) { delta[a] = f.centre(a) - m.centre(a); };
    const auto centreOnFace = [&](int a) {  // halfway in
        delta[a] = (onPlusSide(f, m, a) ? f.max[a] : f.min[a]) - m.centre(a);
    };

    switch (mode) {
    case AlignMode::MeetAtFace: {
        const int a = facingAxis(fb, mb);
        delta[a] = onPlusSide(f, m, a) ? f.max[a] - m.min[a] : f.min[a] - m.max[a];
        break;
    }
    case AlignMode::FaceCentreXYZ: {
        const int a = facingAxis(fb, mb);
        for (int i = 0; i < 3; ++i) {
            if (i == a)
                centreOnFace(i);
            else
                centreOn(i);
        }
        break;
    }
    case AlignMode::FaceCentreX: centreOnFace(0); break;
    case AlignMode::FaceCentreY: centreOnFace(1); break;
    case AlignMode::FaceCentreZ: centreOnFace(2); break;
    case AlignMode::CentreXYZ:
        for (int i = 0; i < 3; ++i)
            centreOn(i);
        break;
    case AlignMode::CentreX: centreOn(0); break;
    case AlignMode::CentreY: centreOn(1); break;
    case AlignMode::CentreZ: centreOn(2); break;
    }

    // A top-level body's position is a plain world offset, so the move adds on.
    // Round away float noise from the bounds (they are stored as floats).
    // Axes that don't move keep their exact value.
    const auto shift = [](double v, double d) { return d == 0.0 ? v : std::round((v + d) * 1e4) / 1e4; };
    p.x = shift(p.x, delta[0]);
    p.y = shift(p.y, delta[1]);
    p.z = shift(p.z, delta[2]);
    return p;
}

}  // namespace forge
