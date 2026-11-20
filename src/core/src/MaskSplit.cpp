// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/MaskSplit.h"

#include "forge/core/CommandStack.h"
#include "forge/core/Commands.h"
#include "forge/core/Document.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <unordered_set>

namespace forge {
namespace mask {

namespace {

Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot3(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross3(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec3 unit3(const Vec3& a, const Vec3& fallback)
{
    const double l = std::sqrt(dot3(a, a));
    return l > 1e-12 ? mul(a, 1.0 / l) : fallback;
}

// A three-sided prism (its point aiming along +X: not the same turned any other way),
// `size` from its middle to its point, from z0 to z1, set in `frame`.
MeshSolid keyPrism(const Transform& frame, double size, double z0, double z1)
{
    const double c[3][2] = {{size, 0.0}, {-0.5 * size, 0.6 * size}, {-0.5 * size, -0.6 * size}};
    std::vector<float> pos;
    for (double z : {z0, z1})
        for (const auto& xy : c) {
            const Vec3 p = frame.apply(Vec3{xy[0], xy[1], z});
            pos.insert(pos.end(), {float(p[0]), float(p[1]), float(p[2])});
        }
    // Counter-clockwise from above (+Z): bottom faces down, top up, sides out.
    const std::vector<std::uint32_t> idx{0, 2, 1, 3, 4, 5, 0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4, 2, 0, 3, 2, 3, 5};
    return meshkernel::fromTriangles(pos, idx);
}

double dist2(const Vec3& a, const Vec3& b)
{
    const Vec3 d = sub(a, b);
    return d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
}

std::uint64_t edgeKey(std::uint32_t a, std::uint32_t b)
{
    return a < b ? (std::uint64_t(a) << 32 | b) : (std::uint64_t(b) << 32 | a);
}

}  // namespace

Field::Field(const std::vector<MaskDab>& dabs) : m_dabs(dabs)
{
    double biggest = 0.0;
    m_smallest = dabs.empty() ? 1.0 : 1e300;
    for (const MaskDab& d : dabs) {
        biggest = std::max(biggest, d.radius);
        m_smallest = std::min(m_smallest, d.radius);
    }
    m_cell = std::max(biggest, 1e-3);
    for (std::uint32_t i = 0; i < m_dabs.size(); ++i) {
        const MaskDab& d = m_dabs[i];
        std::int64_t lo[3], hi[3];
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::int64_t(std::floor((d.centre[std::size_t(k)] - d.radius) / m_cell));
            hi[k] = std::int64_t(std::floor((d.centre[std::size_t(k)] + d.radius) / m_cell));
        }
        for (std::int64_t a = lo[0]; a <= hi[0]; ++a)
            for (std::int64_t b = lo[1]; b <= hi[1]; ++b)
                for (std::int64_t c = lo[2]; c <= hi[2]; ++c)
                    m_cells[key(a, b, c)].push_back(i);  // in order, so later dabs come later
    }
}

std::uint64_t Field::key(std::int64_t i, std::int64_t j, std::int64_t k) const
{
    const auto q = [](std::int64_t v) { return std::uint64_t(v + (1 << 20)) & 0x1FFFFF; };
    return q(i) | q(j) << 21 | q(k) << 42;
}

bool Field::masked(const Vec3& p) const
{
    const auto found = m_cells.find(key(std::int64_t(std::floor(p[0] / m_cell)), std::int64_t(std::floor(p[1] / m_cell)),
                                        std::int64_t(std::floor(p[2] / m_cell))));
    if (found == m_cells.end())
        return false;
    bool state = false;
    for (std::uint32_t i : found->second) {
        const MaskDab& d = m_dabs[i];
        if (dist2(p, d.centre) <= d.radius * d.radius)
            state = !d.erase;  // the last dab covering it decides
    }
    return state;
}

bool Field::nearEdge(const Vec3& p, double slack) const
{
    const std::int64_t ci = std::int64_t(std::floor(p[0] / m_cell)), cj = std::int64_t(std::floor(p[1] / m_cell)),
                       ck = std::int64_t(std::floor(p[2] / m_cell));
    for (std::int64_t a = ci - 1; a <= ci + 1; ++a)
        for (std::int64_t b = cj - 1; b <= cj + 1; ++b)
            for (std::int64_t c = ck - 1; c <= ck + 1; ++c) {
                const auto found = m_cells.find(key(a, b, c));
                if (found == m_cells.end())
                    continue;
                for (std::uint32_t i : found->second) {
                    const MaskDab& d = m_dabs[i];
                    if (std::abs(std::sqrt(dist2(p, d.centre)) - d.radius) < slack)
                        return true;
                }
            }
    return false;
}

namespace {

// Splits the long edges near the mask's edge (in two, over and over) until they're
// no longer than `longest` there, so the cut can follow the mask's curve. Triangles
// stay joined: every triangle on a split edge is divided.
void refineNearEdge(std::vector<Vec3>& points, std::vector<std::uint32_t>& tris, const Field& field, double longest)
{
    for (int pass = 0; pass < 16; ++pass) {
        std::unordered_map<std::uint64_t, std::uint32_t> middle;
        const double l2 = longest * longest;
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = tris[t + std::size_t(k)], b = tris[t + std::size_t((k + 1) % 3)];
                if (dist2(points[a], points[b]) <= l2 || middle.count(edgeKey(a, b)))
                    continue;
                const Vec3 m = mul(add(points[a], points[b]), 0.5);
                // Near enough to the mask's edge that the cut could cross it.
                const double half = std::sqrt(dist2(points[a], points[b])) / 2.0;
                if (!field.nearEdge(m, std::min(half + longest, field.cellSize())))
                    continue;
                middle.emplace(edgeKey(a, b), std::uint32_t(points.size()));
                points.push_back(m);
            }
        if (middle.empty())
            return;
        std::vector<std::uint32_t> next;
        next.reserve(tris.size() * 2);
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
            const std::uint32_t v[3] = {tris[t], tris[t + 1], tris[t + 2]};
            std::int64_t e[3];
            for (int k = 0; k < 3; ++k) {
                const auto f = middle.find(edgeKey(v[k], v[(k + 1) % 3]));
                e[k] = f == middle.end() ? -1 : std::int64_t(f->second);
            }
            const int splits = (e[0] >= 0) + (e[1] >= 0) + (e[2] >= 0);
            if (splits == 0) {
                next.insert(next.end(), {v[0], v[1], v[2]});
            } else if (splits == 3) {
                const auto a = std::uint32_t(e[0]), b = std::uint32_t(e[1]), c = std::uint32_t(e[2]);
                next.insert(next.end(), {v[0], a, c, a, v[1], b, c, b, v[2], a, b, c});
            } else if (splits == 1) {
                int r = 0;
                while (e[r] < 0)
                    ++r;
                const std::uint32_t a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3], m = std::uint32_t(e[r]);
                next.insert(next.end(), {a, m, c, m, b, c});
            } else {
                // Two split: the unsplit edge is r+2 (from c to a); fan from the corner between them.
                int r = 0;
                while (e[r] < 0 || e[(r + 1) % 3] < 0)
                    ++r;
                const std::uint32_t a = v[r], b = v[(r + 1) % 3], c = v[(r + 2) % 3];
                const auto mab = std::uint32_t(e[r]), mbc = std::uint32_t(e[(r + 1) % 3]);
                next.insert(next.end(), {mab, b, mbc, a, mab, mbc, a, mbc, c});
            }
        }
        tris.swap(next);
    }
}

}  // namespace

Pieces split(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices,
             const std::vector<MaskDab>& dabs)
{
    const Field field(dabs);
    std::vector<Vec3> points(positions.size() / 3);
    for (std::size_t v = 0; v < points.size(); ++v)
        points[v] = {positions[3 * v], positions[3 * v + 1], positions[3 * v + 2]};
    std::vector<std::uint32_t> refined = indices;
    refineNearEdge(points, refined, field, std::max(0.02, field.smallest() / 4.0));
    const std::size_t n0 = points.size();
    std::vector<char> inside(n0, 0);
    std::size_t in = 0;
    for (std::size_t v = 0; v < n0; ++v) {
        inside[v] = field.masked(points[v]) ? 1 : 0;
        in += std::size_t(inside[v]);
    }
    if (in == 0)
        throw std::invalid_argument("Nothing of the shape is masked: paint over the part to cut off.");
    if (in == n0)
        throw std::invalid_argument("All of the shape is masked: leave some of it unpainted.");

    // Where each edge crossing the mask's edge is cut: found by halving from the masked end.
    std::unordered_map<std::uint64_t, std::uint32_t> cuts;
    const auto cutOn = [&](std::uint32_t a, std::uint32_t b) {
        const std::uint64_t k = edgeKey(a, b);
        if (const auto f = cuts.find(k); f != cuts.end())
            return f->second;
        const std::uint32_t from = inside[a] ? a : b, to = inside[a] ? b : a;
        double lo = 0.0, hi = 1.0;
        const Vec3 p = points[from], d = sub(points[to], points[from]);
        for (int i = 0; i < 32; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (field.masked(add(p, mul(d, mid))))
                lo = mid;
            else
                hi = mid;
        }
        const auto id = std::uint32_t(points.size());
        points.push_back(add(p, mul(d, 0.5 * (lo + hi))));
        cuts.emplace(k, id);
        return id;
    };

    std::vector<std::uint32_t> inTris, outTris;
    const auto put = [&](bool masked, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        auto& to = masked ? inTris : outTris;
        to.insert(to.end(), {a, b, c});
    };
    for (std::size_t t = 0; t + 2 < refined.size(); t += 3) {
        const std::uint32_t v[3] = {refined[t], refined[t + 1], refined[t + 2]};
        const int count = inside[v[0]] + inside[v[1]] + inside[v[2]];
        if (count == 0 || count == 3) {
            put(count == 3, v[0], v[1], v[2]);
            continue;
        }
        // The corner on its own side first, the turn kept.
        int odd = 0;
        for (int k = 0; k < 3; ++k)
            if (inside[v[k]] != inside[v[(k + 1) % 3]] && inside[v[k]] != inside[v[(k + 2) % 3]])
                odd = k;
        const std::uint32_t a = v[odd], b = v[(odd + 1) % 3], c = v[(odd + 2) % 3];
        const std::uint32_t ab = cutOn(a, b), ac = cutOn(a, c);
        put(inside[a], a, ab, ac);
        put(!inside[a], ab, b, c);
        put(!inside[a], ab, c, ac);
    }

    // The masked piece's open edges, chained into loops (each edge as the piece runs it).
    std::unordered_set<std::uint64_t> directed;
    for (std::size_t t = 0; t + 2 < inTris.size(); t += 3)
        for (int k = 0; k < 3; ++k)
            directed.insert(std::uint64_t(inTris[t + std::size_t(k)]) << 32 | inTris[t + std::size_t((k + 1) % 3)]);
    std::unordered_map<std::uint32_t, std::uint32_t> next;
    for (std::uint64_t e : directed) {
        const auto u = std::uint32_t(e >> 32), w = std::uint32_t(e & 0xFFFFFFFFu);
        if (!directed.count(std::uint64_t(w) << 32 | u))
            next[u] = w;
    }
    std::vector<std::vector<std::uint32_t>> loops;
    std::unordered_set<std::uint32_t> seen;
    std::vector<std::uint32_t> starts;
    for (const auto& [u, w] : next)
        starts.push_back(u);
    std::sort(starts.begin(), starts.end());  // the same caps every time
    for (std::uint32_t start : starts) {
        if (seen.count(start))
            continue;
        std::vector<std::uint32_t> loop;
        std::uint32_t at = start;
        do {
            if (!seen.insert(at).second)
                throw std::invalid_argument("The mask's edge couldn't be followed round (it may cross itself).");
            loop.push_back(at);
            const auto f = next.find(at);
            if (f == next.end())
                throw std::invalid_argument("The mask's edge couldn't be followed round.");
            at = f->second;
        } while (at != start);
        if (loop.size() >= 3)
            loops.push_back(std::move(loop));
    }

    // A cap for each loop: rings drawn in to the loop's middle, then smoothed like a
    // stretched membrane (each inner point to the middle of its neighbours).
    std::vector<std::uint32_t> caps;
    std::vector<KeySpot> keySpots;
    for (const auto& loop : loops) {
        const std::size_t n = loop.size();
        Vec3 c{0, 0, 0};
        for (std::uint32_t v : loop)
            c = add(c, points[v]);
        c = mul(c, 1.0 / double(n));
        const int rings = std::clamp(int(double(n) / (2.0 * std::numbers::pi)), 2, 40);
        std::vector<std::vector<std::uint32_t>> ring{loop};
        const std::size_t firstInner = points.size();
        for (int k = 1; k < rings; ++k) {
            std::vector<std::uint32_t> r(n);
            const double t = 1.0 - double(k) / rings;
            for (std::size_t i = 0; i < n; ++i) {
                r[i] = std::uint32_t(points.size());
                points.push_back(add(c, mul(sub(points[loop[i]], c), t)));
            }
            ring.push_back(std::move(r));
        }
        const auto centre = std::uint32_t(points.size());
        points.push_back(c);
        std::vector<std::uint32_t> tris;
        for (std::size_t k = 0; k + 1 < ring.size(); ++k)
            for (std::size_t i = 0; i < n; ++i) {
                const std::size_t j = (i + 1) % n;
                const std::uint32_t o0 = ring[k][i], o1 = ring[k][j], i0 = ring[k + 1][i], i1 = ring[k + 1][j];
                // Runs each loop edge the other way from the masked piece.
                tris.insert(tris.end(), {o1, o0, i0, o1, i0, i1});
            }
        for (std::size_t i = 0; i < n; ++i)
            tris.insert(tris.end(), {ring.back()[(i + 1) % n], ring.back()[i], centre});
        // Neighbours of the inner points, then the smoothing.
        const std::size_t inner = points.size() - firstInner;
        std::vector<std::vector<std::uint32_t>> around(inner);
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t a = tris[t + std::size_t(k)], b = tris[t + std::size_t((k + 1) % 3)];
                if (a >= firstInner)
                    around[a - firstInner].push_back(b);
                if (b >= firstInner)
                    around[b - firstInner].push_back(a);
            }
        for (int pass = 0; pass < 300; ++pass)
            for (std::size_t i = 0; i < inner; ++i) {
                Vec3 sum{0, 0, 0};
                for (std::uint32_t o : around[i])
                    sum = add(sum, points[o]);
                if (!around[i].empty())
                    points[firstInner + i] = mul(sum, 1.0 / double(around[i].size()));
            }
        // Where an alignment key could go: the cap's middle point (smoothed with the rest).
        KeySpot spot;
        spot.point = points[centre];
        Vec3 way{0, 0, 0};
        for (std::size_t t = 0; t + 2 < tris.size(); t += 3)
            way = add(way, cross3(sub(points[tris[t + 1]], points[tris[t]]), sub(points[tris[t + 2]], points[tris[t]])));
        spot.normal = unit3(way, {0, 0, 1});
        double room = 1e300;
        for (std::uint32_t v : loop)
            room = std::min(room, std::sqrt(dist2(points[v], spot.point)));
        spot.room = room;
        const Vec3 toEdge = sub(points[loop.front()], spot.point);
        spot.across = unit3(sub(toEdge, mul(spot.normal, dot3(toEdge, spot.normal))), {1, 0, 0});
        keySpots.push_back(spot);
        caps.insert(caps.end(), tris.begin(), tris.end());
    }

    // Each piece with its own points.
    const auto build = [&](const std::vector<std::uint32_t>& sides, bool flipCaps) {
        Mesh m;
        std::unordered_map<std::uint32_t, std::uint32_t> local;
        const auto at = [&](std::uint32_t v) {
            const auto [it, added] = local.emplace(v, std::uint32_t(local.size()));
            if (added)
                for (int k = 0; k < 3; ++k)
                    m.positions.push_back(float(points[v][std::size_t(k)]));
            return it->second;
        };
        for (std::uint32_t v : sides)
            m.indices.push_back(at(v));
        for (std::size_t t = 0; t + 2 < caps.size(); t += 3) {
            if (flipCaps)
                m.indices.insert(m.indices.end(), {at(caps[t]), at(caps[t + 2]), at(caps[t + 1])});
            else
                m.indices.insert(m.indices.end(), {at(caps[t]), at(caps[t + 1]), at(caps[t + 2])});
        }
        return m;
    };
    Pieces out;
    out.masked = build(inTris, false);
    out.rest = build(outTris, true);
    out.caps = int(loops.size());
    out.keySpots = std::move(keySpots);
    return out;
}

double keySize(double room)
{
    const double size = std::min(0.3 * room, 3.0);
    return size >= 0.5 ? size : 0.0;
}

double keyLength(double size)
{
    return std::clamp(1.5 * size, 1.0, 4.0);
}

MeshSolid keyed(const Mesh& piece, const std::vector<KeySpot>& spots, bool masked, double fit)
{
    const MeshSolid solid = meshkernel::fromTriangles(piece.positions, piece.indices);
    std::vector<MeshSolid> keys;
    for (const KeySpot& s : spots) {
        const double size = keySize(s.room);
        if (size <= 0.0)
            continue;
        // The key's frame: X across the cap, Z out of the masked piece into the rest.
        const Vec3 y = cross3(s.normal, s.across);
        Transform frame;
        for (int r = 0; r < 3; ++r) {
            frame.m[r][0] = s.across[std::size_t(r)];
            frame.m[r][1] = y[std::size_t(r)];
            frame.m[r][2] = s.normal[std::size_t(r)];
            frame.m[r][3] = s.point[std::size_t(r)];
        }
        const double length = keyLength(size);
        if (masked)
            keys.push_back(keyPrism(frame, size, -length, length));  // half in, half standing out
        else
            keys.push_back(keyPrism(frame, size + 2.0 * fit, -0.5, length + fit));
    }
    if (keys.empty())
        return solid;
    if (masked) {
        keys.insert(keys.begin(), solid);
        return meshkernel::merged(keys);
    }
    return meshkernel::subtracted(solid, keys);
}

std::string describe(const MaskSplitFinish& f)
{
    const std::size_t n = f.dabs.size();
    return std::string(f.masked ? "the masked part" : "the rest") + " (" + std::to_string(n) +
           (n == 1 ? " dab" : " dabs") + (f.keys ? ", keyed)" : ")");
}

}  // namespace mask

namespace {

const Body& maskStep(const Document& doc, ObjectId result)
{
    const Body* b = doc.find(result);
    if (!b || !b->isBoolean() || b->boolean->op != BooleanOp::MaskSplit || !b->boolean->maskSplit)
        throw std::invalid_argument("Not a shape split by a mask");
    return *b;
}

std::vector<MaskDab> moved(const std::vector<MaskDab>& dabs, const Transform& t)
{
    std::vector<MaskDab> out = dabs;
    for (MaskDab& d : out)
        d.centre = t.apply(d.centre);
    return out;
}

}  // namespace

BooleanSpec maskSplitSpec(const Document& doc, ObjectId target, const std::vector<MaskDab>& dabs, bool masked,
                          double keyFit)
{
    if (!doc.find(target))
        throw std::out_of_range("No body with id " + std::to_string(target));
    if (dabs.empty())
        throw std::invalid_argument("Paint the mask first.");
    const ObjectId holder = doc.parentOf(target);
    const Transform toStep = holder == kInvalidId ? Transform{} : doc.worldTransform(holder).inverse();
    BooleanSpec spec;
    spec.op = BooleanOp::MaskSplit;
    spec.operands = {target};
    spec.maskSplit = MaskSplitFinish{moved(dabs, toStep), masked};
    if (keyFit >= 0.0) {
        if (keyFit > 1.0)
            throw std::invalid_argument("The keys' fit must be from 0 to 1 mm.");
        spec.maskSplit->keys = true;
        spec.maskSplit->keyFit = keyFit;
    }
    const auto c = doc.worldBounds(target).center();
    const Vec3 centre = toStep.apply(Vec3{c[0], c[1], c[2]});
    spec.pivot = {centre[0], centre[1], centre[2]};
    return spec;
}

std::vector<ObjectId> splitByMask(CommandStack& stack, Document& doc, ObjectId id, const std::vector<MaskDab>& dabs,
                                  SplitKeep keep, double keyFit)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!doc.isTopLevel(id))
        throw std::invalid_argument("Only a shape on the plate can be split");
    // Both pieces worked out first: refuse before changing anything.
    BooleanSpec front = maskSplitSpec(doc, id, dabs, true, keyFit), back = maskSplitSpec(doc, id, dabs, false, keyFit);
    const Document::Preview frontShape = doc.previewResult(front), backShape = doc.previewResult(back);
    if (frontShape.empty || backShape.empty)
        throw std::invalid_argument("The mask doesn't cut '" + body->name + "' in two.");
    const std::string base = body->name;
    const auto makeResult = [&](BooleanSpec spec, const Document::Preview& preview, const std::string& side) {
        const auto c = preview.bounds.center();
        spec.pivot = {c[0], c[1], c[2]};
        Body r;
        r.id = doc.allocateId();
        r.name = base + " (" + side + ")";
        r.boolean = std::move(spec);
        return r;
    };
    std::vector<ObjectId> made;
    stack.beginMacro("Split " + base + " by mask");
    try {
        if (keep == SplitKeep::Both) {
            const std::vector<Body> copies = duplicateTree(doc, id);
            for (const Body& c : copies)
                stack.execute(std::make_unique<AddBodyCommand>(c));
            back.operands = {copies.back().id};
            Body a = makeResult(front, frontShape, "masked");
            Body b = makeResult(back, backShape, "rest");
            made = {a.id, b.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(a)));
            stack.execute(std::make_unique<CombineCommand>(std::move(b)));
        } else {
            const bool keepMasked = keep == SplitKeep::Front;
            Body r = keepMasked ? makeResult(front, frontShape, "masked") : makeResult(back, backShape, "rest");
            made = {r.id};
            stack.execute(std::make_unique<CombineCommand>(std::move(r)));
        }
    } catch (...) {
        stack.abortMacro();
        throw;
    }
    stack.endMacro();
    return made;
}

std::vector<MaskDab> maskDabs(const Document& doc, ObjectId result)
{
    const Body& b = maskStep(doc, result);
    return moved(b.boolean->maskSplit->dabs, doc.worldTransform(result));
}

std::vector<Body> withMask(const Document& doc, ObjectId result, const std::vector<MaskDab>& dabs,
                           std::optional<double> keyFit)
{
    const Body& b = maskStep(doc, result);
    if (dabs.empty())
        throw std::invalid_argument("Paint the mask first.");
    const std::vector<MaskDab> local = moved(dabs, doc.worldTransform(result).inverse());
    std::vector<Body> out;
    if (keyFit && *keyFit > 1.0)
        throw std::invalid_argument("The keys' fit must be from 0 to 1 mm.");
    const auto setKeys = [&keyFit](MaskSplitFinish& f) {
        if (!keyFit)
            return;
        f.keys = *keyFit >= 0.0;
        if (f.keys)
            f.keyFit = *keyFit;
    };
    Body next = b;
    next.boolean->maskSplit->dabs = local;
    setKeys(*next.boolean->maskSplit);
    out.push_back(next);
    // Its other piece, if it's still on the plate with the same mask: changed alike (the
    // dabs are in the same place on the part, wherever the piece has been moved since).
    const MaskSplitFinish& old = *b.boolean->maskSplit;
    for (const Body& other : doc.bodies()) {
        if (other.id == result || !doc.isTopLevel(other.id) || !other.isBoolean() ||
            other.boolean->op != BooleanOp::MaskSplit || !other.boolean->maskSplit)
            continue;
        const MaskSplitFinish& f = *other.boolean->maskSplit;
        if (f.masked != old.masked && f.dabs == old.dabs) {
            Body o = other;
            o.boolean->maskSplit->dabs = local;
            setKeys(*o.boolean->maskSplit);
            out.push_back(o);
            break;
        }
    }
    return out;
}

}  // namespace forge
