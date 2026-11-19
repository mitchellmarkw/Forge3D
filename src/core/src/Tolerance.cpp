// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Tolerance.h"

#include "forge/core/Body.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Socket.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace forge::tolerance {
namespace {

constexpr double kPi = std::numbers::pi;

void pushPoint(Mesh& m, double x, double y, double z)
{
    m.positions.push_back(float(x));
    m.positions.push_back(float(y));
    m.positions.push_back(float(z));
}

Mesh box(double x0, double x1, double y0, double y1, double z0, double z1)
{
    Mesh m;
    for (int k = 0; k < 8; ++k)
        pushPoint(m, k & 1 ? x1 : x0, k & 2 ? y1 : y0, k & 4 ? z1 : z0);
    // Faces outward (x0 < x1, y0 < y1, z0 < z1).
    m.indices = {0, 2, 3, 0, 3, 1, 4, 5, 7, 4, 7, 6, 0, 1, 5, 0, 5, 4,
                 2, 6, 7, 2, 7, 3, 0, 4, 6, 0, 6, 2, 1, 3, 7, 1, 7, 5};
    return m;
}

int sidesFor(double r) { return std::clamp(int(std::ceil(2.0 * kPi * r / 0.25)), 48, 160); }

// Upright round post, true radius (a peg or pin is never bigger than asked).
Mesh post(double x, double y, double r, double z0, double z1)
{
    const int n = sidesFor(r);
    Mesh m;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * kPi * i / n;
            pushPoint(m, x + r * std::cos(a), y + r * std::sin(a), k == 0 ? z0 : z1);
        }
    pushPoint(m, x, y, z0);
    pushPoint(m, x, y, z1);
    const auto un = std::uint32_t(n);
    for (std::uint32_t i = 0; i < un; ++i) {
        const std::uint32_t j = (i + 1) % un;
        m.indices.insert(m.indices.end(), {i, j, un + j, i, un + j, un + i, 2 * un, j, i, 2 * un + 1, un + i, un + j});
    }
    return m;
}

Mesh sphere(double x, double y, double z, double r)
{
    const int rings = std::clamp(sidesFor(r) / 2, 24, 80), segs = rings * 2;
    Mesh m;
    pushPoint(m, x, y, z - r);  // 0: bottom
    for (int i = 1; i < rings; ++i) {
        const double phi = -kPi / 2.0 + kPi * i / rings;
        for (int k = 0; k < segs; ++k) {
            const double th = 2.0 * kPi * k / segs;
            pushPoint(m, x + r * std::cos(phi) * std::cos(th), y + r * std::cos(phi) * std::sin(th), z + r * std::sin(phi));
        }
    }
    const auto top = std::uint32_t(1 + (rings - 1) * segs);
    pushPoint(m, x, y, z + r);
    const auto at = [segs](int ring, int k) { return std::uint32_t(1 + (ring - 1) * segs + (k % segs)); };
    for (int k = 0; k < segs; ++k)
        m.indices.insert(m.indices.end(), {0, at(1, k + 1), at(1, k)});
    for (int i = 1; i < rings - 1; ++i)
        for (int k = 0; k < segs; ++k)
            m.indices.insert(m.indices.end(),
                             {at(i, k), at(i, k + 1), at(i + 1, k + 1), at(i, k), at(i + 1, k + 1), at(i + 1, k)});
    for (int k = 0; k < segs; ++k)
        m.indices.insert(m.indices.end(), {top, at(rings - 1, k), at(rings - 1, k + 1)});
    return m;
}

MeshSolid solid(const Mesh& m) { return meshkernel::fromTriangles(m.positions, m.indices); }

Mesh meshOf(const MeshSolid& s)
{
    Mesh m;
    meshkernel::triangles(s, m.positions, m.indices);
    return m;
}

Mesh moved(Mesh m, double dx, double dy)
{
    for (std::size_t i = 0; i + 2 < m.positions.size(); i += 3) {
        m.positions[i] += float(dx);
        m.positions[i + 1] += float(dy);
    }
    return m;
}

std::string num(double v)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << v;
    return out.str();
}

// Seven segments: a top, b top right, c bottom right, d bottom, e bottom left,
// f top left, g middle.
const char* segmentsOf(char c)
{
    switch (c) {
    case '0': return "abcdef";
    case '1': return "bc";
    case '2': return "abged";
    case '3': return "abgcd";
    case '4': return "fgbc";
    case '5': return "afgcd";
    case '6': return "afgedc";
    case '7': return "abc";
    case '8': return "abcdefg";
    case '9': return "abcdfg";
    case '-': return "g";
    default: return "";
    }
}

}  // namespace

std::vector<double> steps(double from, double to, double step)
{
    if (!(step > 0.0) || !(to >= from) || !(from >= 0.0))
        throw std::invalid_argument("Give a smallest fit, a biggest fit no smaller, and a step more than 0.");
    std::vector<double> out;
    for (int i = 0;; ++i) {
        const double v = std::round((from + i * step) * 100.0) / 100.0;
        if (v > to + 1e-9)
            break;
        if (out.empty() || v > out.back() + 1e-9)
            out.push_back(v);
        if (out.size() > 12)
            throw std::invalid_argument("That's more than 12 fits: make the step bigger.");
    }
    return out;
}

void check(const Settings& s)
{
    if (!s.pegs && !s.balls && !s.pins)
        throw std::invalid_argument("Choose at least one thing to test.");
    if (s.fits.empty() || s.fits.size() > 12)
        throw std::invalid_argument("Test 1 to 12 fits.");
    for (double f : s.fits)
        if (!(f >= 0.0) || f > 2.0)
            throw std::invalid_argument("Each fit must be from 0 to 2 mm.");
    if (s.pegs && (!(s.pegSize >= 1.0) || !(s.pegDepth >= 1.0)))
        throw std::invalid_argument("The peg must be at least 1 mm across and go in at least 1 mm.");
    if (s.balls && !(s.ballSize >= 2.0))
        throw std::invalid_argument("The ball must be at least 2 mm across.");
    if (s.pins && !(s.pinSize >= 0.5))
        throw std::invalid_argument("The pin must be at least 0.5 mm across.");
}

std::string label(double fit)
{
    std::string t = num(fit);
    if (t.rfind("0.", 0) == 0)
        t = t.substr(1);
    return t;
}

Mesh digits(const std::string& text, double height, double raise)
{
    const double w = height * 0.55, t = std::max(0.4, height * 0.16), gap = height * 0.25;
    const double h2 = height / 2.0;
    std::vector<MeshSolid> parts;
    double x = 0.0;
    for (char c : text) {
        if (c == '.') {
            parts.push_back(solid(box(x, x + t, -h2, -h2 + t, 0.0, raise)));
            x += t + gap;
            continue;
        }
        for (const char* s = segmentsOf(c); *s; ++s) {
            switch (*s) {
            case 'a': parts.push_back(solid(box(x, x + w, h2 - t, h2, 0, raise))); break;
            case 'd': parts.push_back(solid(box(x, x + w, -h2, -h2 + t, 0, raise))); break;
            case 'g': parts.push_back(solid(box(x, x + w, -t / 2, t / 2, 0, raise))); break;
            case 'b': parts.push_back(solid(box(x + w - t, x + w, 0, h2, 0, raise))); break;
            case 'c': parts.push_back(solid(box(x + w - t, x + w, -h2, 0, 0, raise))); break;
            case 'e': parts.push_back(solid(box(x, x + t, -h2, 0, 0, raise))); break;
            case 'f': parts.push_back(solid(box(x, x + t, 0, h2, 0, raise))); break;
            default: break;
            }
        }
        x += w + gap;
    }
    if (parts.empty())
        return {};
    return meshOf(meshkernel::merged(parts));
}

std::vector<Piece> pieces(const Settings& s)
{
    check(s);
    const std::size_t n = s.fits.size();
    const double biggestFit = *std::max_element(s.fits.begin(), s.fits.end());
    const double labelHeight = 3.0, labelRaise = 0.6;
    const std::string range = label(s.fits.front()) + "\xE2\x80\x93" + label(s.fits.back());

    struct Bar {
        Kind kind;
        double size;
    };
    std::vector<Bar> bars;
    if (s.pegs)
        bars.push_back({Kind::Peg, s.pegSize});
    if (s.balls)
        bars.push_back({Kind::Ball, s.ballSize});
    if (s.pins)
        bars.push_back({Kind::Pin, s.pinSize});

    std::vector<Piece> out;
    double y = 0.0;  // rows go back from here
    std::vector<std::pair<Kind, double>> trial;  // what to try in each bar: kind and the row's front
    for (const Bar& bar : bars) {
        const double hole = bar.size + 2.0 * biggestFit;
        const double labelWidth = labelHeight * (0.55 + 0.25) * 3.0;
        const double pitch = std::max(hole + 3.0, labelWidth + 1.5);
        const double length = pitch * double(n) + 4.0;
        const double depth = hole + labelHeight + 6.0;  // front to back
        SocketFinish f;
        f.kind = bar.kind == Kind::Ball ? SocketKind::Ball : SocketKind::Peg;
        f.size = bar.size;
        f.grip = s.grip;
        double height = 0.0;
        if (bar.kind == Kind::Peg) {
            f.depth = s.pegDepth;
            height = s.pegDepth + 2.0;
        } else if (bar.kind == Kind::Ball) {
            f.fit = biggestFit;
            const auto seat = socket::ballSeat(f);
            height = seat.centreDepth + seat.radius + 1.5;
        } else {
            height = 4.0;
            f.depth = height;  // right through
        }
        const double y0 = y, holeY = y0 + depth - 2.0 - hole / 2.0, labelY = y0 + 2.0 + labelHeight / 2.0;
        MeshSolid body = solid(box(-length / 2.0, length / 2.0, y0, y0 + depth, 0.0, height));
        std::vector<MeshSolid> cuts;
        std::vector<MeshSolid> labels;
        for (std::size_t i = 0; i < n; ++i) {
            const double x = -length / 2.0 + 2.0 + pitch * (double(i) + 0.5);
            f.fit = s.fits[i];
            if (bar.kind == Kind::Pin)
                f.depth = height;
            for (const Mesh& c : socket::cutters(f, {x, holeY, height}, {0, 0, 1}))
                cuts.push_back(solid(c));
            const std::string text = label(s.fits[i]);
            const double textWidth = labelHeight * (0.55 + 0.25) * double(text.size()) - labelHeight * 0.25;
            Mesh d = digits(text, labelHeight, labelRaise);
            for (std::size_t k = 2; k + 0 < d.positions.size(); k += 3)
                d.positions[k] += float(height);
            labels.push_back(solid(moved(std::move(d), x - textWidth / 2.0, labelY)));
        }
        body = meshkernel::subtracted(body, cuts);
        labels.insert(labels.begin(), body);
        const char* what = bar.kind == Kind::Peg ? "Peg" : bar.kind == Kind::Ball ? "Ball" : "Pin";
        out.push_back({std::string(what) + " fit test " + range, meshOf(meshkernel::merged(labels))});
        trial.push_back({bar.kind, y0});
        y += depth + 5.0;
    }

    // The pieces to try, in a row in front of the bars.
    double x = 0.0;
    const double front = -6.0;
    for (const Bar& bar : bars) {
        if (bar.kind == Kind::Ball) {
            // A ball on a stalk, to hold it by.
            const double r = bar.size / 2.0, stalk = std::max(0.8, bar.size * 0.3);
            const double cx = x + r, cy = front - r;
            const MeshSolid ball = meshkernel::merged(
                {solid(post(cx, cy, stalk, 0.0, bar.size)), solid(sphere(cx, cy, bar.size + r * 0.6, r))});
            out.push_back({"Test ball \xC3\x98" + num(bar.size), meshOf(ball)});
            x += bar.size + 4.0;
            continue;
        }
        const double r = bar.size / 2.0;
        const double tall = bar.kind == Kind::Peg ? s.pegDepth + 3.0 : 4.0 + 3.0;
        for (int copy = 0; copy < 2; ++copy) {
            const double cx = x + r, cy = front - r;
            out.push_back({std::string(bar.kind == Kind::Peg ? "Test peg \xC3\x98" : "Test pin \xC3\x98") + num(bar.size),
                           post(cx, cy, r, 0.0, tall)});
            x += bar.size + 4.0;
        }
    }
    // The row of pieces centred under the bars.
    const double shift = -(x - 4.0) / 2.0;
    for (std::size_t i = bars.size(); i < out.size(); ++i)
        out[i].mesh = moved(std::move(out[i].mesh), shift, 0.0);
    return out;
}

}  // namespace forge::tolerance
