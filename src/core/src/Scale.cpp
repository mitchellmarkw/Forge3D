// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Scale.h"

#include "forge/core/Document.h"

#include <cmath>
#include <type_traits>

namespace forge {

namespace {

bool even(const Vec3& f)
{
    return std::abs(f[0] - f[1]) < 1e-12 && std::abs(f[1] - f[2]) < 1e-12;
}

// The stretch, seen along the body's own axes, if the body's rotation lets it stay
// a plain stretch there: each body axis must lie within parent axes that stretch by
// the same amount (e.g. any turn about Z is fine when only Z stretches).
bool bodyFactorsFor(const Transform& t, const Vec3& factors, Vec3& out)
{
    for (std::size_t body = 0; body < 3; ++body) {
        bool any = false;
        double f = 0.0;
        for (std::size_t parent = 0; parent < 3; ++parent) {
            if (std::abs(t.m[parent][body]) < 1e-9)
                continue;
            if (any && std::abs(factors[parent] - f) > 1e-12)
                return false;
            f = factors[parent];
            any = true;
        }
        out[body] = f;
    }
    return true;
}

Vec3 mul(const Vec3& f, const Vec3& v)
{
    return {f[0] * v[0], f[1] * v[1], f[2] * v[2]};
}

void scaleInto(const Document& doc, ObjectId id, const Vec3& factors, const Vec3& anchor, bool moveSelf,
               std::vector<Body>& out);

// Body `id` lives in a parent frame that is being scaled by `factors` about
// `anchor` (parent coordinates). Updates its spec / placement (or, for a result,
// its parts) so its geometry follows.
void scaleChild(const Document& doc, ObjectId id, const Vec3& factors, const Vec3& anchor, std::vector<Body>& out)
{
    const Body* body = doc.find(id);
    const Transform local = doc.localTransform(id);
    Vec3 bodyFactors = factors;  // the same factors, along the body's own axes
    if (!even(factors) && !bodyFactorsFor(local, factors, bodyFactors))
        throw NotScalable("'" + body->name + "' is turned at an angle, so this shape can only be scaled evenly "
                          "in that direction");

    if (body->isBoolean()) {
        // Its placement stays; scale its contents about the same point, seen from inside it.
        scaleInto(doc, id, bodyFactors, local.inverse().apply(anchor), /*moveSelf=*/false, out);
        return;
    }
    // A primitive: new sizes, and a position that keeps it in step with the stretch.
    //   p' = S (p + c - A) + A - c'   (c, c': pivots before and after; the rotation drops out)
    Body next = *body;
    next.spec = scaledSpec(body->spec, bodyFactors);
    const Vec3 c = pivotOf(body->spec), c2 = pivotOf(next.spec);
    const Placement& p = body->placement;
    const Vec3 moved = mul(factors, {p.x + c[0] - anchor[0], p.y + c[1] - anchor[1], p.z + c[2] - anchor[2]});
    next.placement.x = moved[0] + anchor[0] - c2[0];
    next.placement.y = moved[1] + anchor[1] - c2[1];
    next.placement.z = moved[2] + anchor[2] - c2[2];
    validate(next.spec);
    out.push_back(std::move(next));
}

void scaleInto(const Document& doc, ObjectId id, const Vec3& factors, const Vec3& anchor, bool moveSelf,
               std::vector<Body>& out)
{
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!body->isBoolean()) {
        if (moveSelf)
            scaleChild(doc, id, factors, anchor, out);
        return;
    }
    for (ObjectId part : body->boolean->operands)
        scaleChild(doc, part, factors, anchor, out);
    if (body->boolean->op == BooleanOp::Edges && body->boolean->edges) {
        // The picks move with the shape; evenly scaled, the rounding grows with it.
        Body next = *body;
        const auto move = [&](std::array<double, 3>& q) {
            q = {factors[0] * (q[0] - anchor[0]) + anchor[0], factors[1] * (q[1] - anchor[1]) + anchor[1],
                 factors[2] * (q[2] - anchor[2]) + anchor[2]};
        };
        for (EdgeRef& r : next.boolean->edges->edges) {
            move(r.point);
            move(r.facePoint);
        }
        if (even(factors))
            next.boolean->edges->size *= factors[0];
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::Sculpt && body->boolean->sculpt) {
        // The strokes go where the surface goes; evenly scaled, the brushes grow too.
        Body next = *body;
        for (SculptStroke& st : next.boolean->sculpt->strokes) {
            for (auto& d : st.dabs)
                for (std::size_t k = 0; k < 3; ++k)
                    d[k] = float(factors[k] * (d[k] - anchor[k]) + anchor[k]);
            for (std::size_t k = 0; k < 3; ++k)
                st.grab[k] = float(factors[k] * st.grab[k]);
            if (even(factors)) {
                st.radius *= factors[0];
                st.finest *= factors[0];
            }
        }
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::Pattern && body->boolean->pattern) {
        // The copies keep their places in the stretched shape: spacing grows with it.
        Body next = *body;
        PatternFinish& p = *next.boolean->pattern;
        const auto along = [&](const std::array<double, 3>& d) {
            const double x = factors[0] * d[0], y = factors[1] * d[1], z = factors[2] * d[2];
            return std::sqrt(x * x + y * y + z * z);
        };
        p.spacing1 *= along(p.dir1);
        p.spacing2 *= along(p.dir2);
        for (std::size_t k = 0; k < 3; ++k)
            p.centre[k] = factors[k] * (p.centre[k] - anchor[k]) + anchor[k];
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::Follow && body->boolean->follow) {
        // The path's start moves with the shape; evenly scaled, the path and groove grow too.
        Body next = *body;
        FollowFinish& f = *next.boolean->follow;
        for (std::size_t k = 0; k < 3; ++k)
            f.origin[k] = factors[k] * (f.origin[k] - anchor[k]) + anchor[k];
        if (even(factors)) {
            sketch::scale(f.path, factors[0], factors[0]);
            sketch::scale(f.custom, factors[0], factors[0]);
            f.width *= factors[0];
            f.depth *= factors[0];
        }
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::Shell && body->boolean->shell) {
        // The open faces' picks move with the shape; evenly scaled, the walls grow too.
        Body next = *body;
        for (FaceRef& r : next.boolean->shell->open)
            r.point = {factors[0] * (r.point[0] - anchor[0]) + anchor[0],
                       factors[1] * (r.point[1] - anchor[1]) + anchor[1],
                       factors[2] * (r.point[2] - anchor[2]) + anchor[2]};
        if (even(factors))
            next.boolean->shell->thickness *= factors[0];
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::Split || body->boolean->op == BooleanOp::Mirror ||
        body->boolean->op == BooleanOp::Emboss) {
        // The cutting / mirror plane (or the words' spot) stretches with everything else.
        Body next = *body;
        if (next.boolean->emboss && even(factors)) {
            // Scaled evenly, the words grow with the shape; stretched, they keep their size.
            next.boolean->emboss->letterHeight *= factors[0];
            next.boolean->emboss->depth *= factors[0];
        }
        if (next.boolean->picture && even(factors)) {
            next.boolean->picture->width *= factors[0];
            next.boolean->picture->depth *= factors[0];
        }
        if (body->boolean->op == BooleanOp::Mirror && !even(factors)) {
            // A reflection only survives a stretch along or across its mirror.
            const Vec3& n0 = body->boolean->planeNormal;
            int axes = 0;
            for (double v : n0)
                axes += std::abs(v) > 1e-9 ? 1 : 0;
            if (axes > 1)
                throw NotScalable("'" + body->name + "' is mirrored at an angle, so it can only be scaled evenly");
        }
        Vec3& q = next.boolean->planePoint;
        q = {factors[0] * (q[0] - anchor[0]) + anchor[0], factors[1] * (q[1] - anchor[1]) + anchor[1],
             factors[2] * (q[2] - anchor[2]) + anchor[2]};
        Vec3& n = next.boolean->planeNormal;
        n = {n[0] / factors[0], n[1] / factors[1], n[2] / factors[2]};  // normals stretch the other way
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        n = {n[0] / len, n[1] / len, n[2] / len};
        out.push_back(std::move(next));
    }
}

}  // namespace

PrimitiveSpec scaledSpec(const PrimitiveSpec& spec, const Vec3& f)
{
    if (even(f))
        return scaled(spec, f[0]);
    return std::visit(
        [&f](auto p) -> PrimitiveSpec {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, BoxSpec> || std::is_same_v<T, WedgeSpec> || std::is_same_v<T, MeshSpec> ||
                          std::is_same_v<T, TextSpec>) {
                p.width *= f[0];
                p.depth *= f[1];
                p.height *= f[2];
            } else if constexpr (std::is_same_v<T, PyramidSpec>) {
                p.baseWidth *= f[0];
                p.topWidth *= f[0];
                p.baseDepth *= f[1];
                p.topDepth *= f[1];
                p.height *= f[2];
            } else if constexpr (std::is_same_v<T, CylinderSpec>) {
                p.radiusX *= f[0];
                p.radiusY *= f[1];
                p.height *= f[2];
            } else if constexpr (std::is_same_v<T, SphereSpec>) {
                p.radiusX *= f[0];
                p.radiusY *= f[1];
                p.radiusZ *= f[2];
            } else if constexpr (std::is_same_v<T, ConeSpec>) {
                p.bottomRadiusX *= f[0];
                p.topRadiusX *= f[0];
                p.bottomRadiusY *= f[1];
                p.topRadiusY *= f[1];
                p.height *= f[2];
            } else if constexpr (std::is_same_v<T, TubeSpec>) {
                p.outerRadiusX *= f[0];
                p.innerRadiusX *= f[0];
                p.outerRadiusY *= f[1];
                p.innerRadiusY *= f[1];
                p.height *= f[2];
            } else if constexpr (std::is_same_v<T, PadSpec>) {
                if (!sketch::scale(p.sketch, f[0], f[1]))
                    throw NotScalable("This sketch has curves, slopes or angles, so it can only be resized evenly "
                                      "across (its height can change freely)");
                p.length *= f[2];
            } else if constexpr (std::is_same_v<T, RevolveSpec>) {
                // Spun round: only even resizing keeps it what it is.
                if (std::abs(f[0] - f[1]) > 1e-12 || std::abs(f[0] - f[2]) > 1e-12)
                    throw NotScalable("A spun (revolved) shape can only be resized evenly");
                sketch::scale(p.sketch, f[0], f[0]);
            } else if constexpr (std::is_same_v<T, HoleSpec>) {
                // The spots follow the stretch; the holes stay round (and get deeper with Z).
                if (!sketch::scale(p.sketch, f[0], f[1])) {
                    // Rules that can't stretch (angles): keep the spots, drop the rules.
                    p.sketch.constraints.clear();
                    for (std::size_t i = 1; i < p.sketch.points.size(); ++i) {
                        p.sketch.points[i].x *= f[0];
                        p.sketch.points[i].y *= f[1];
                    }
                }
                p.depth *= f[2];
                p.headDepth *= f[2];
                if (f[0] != 1.0 || f[1] != 1.0 || f[2] != 1.0)
                    p.size.clear();
            } else {
                static_assert(std::is_same_v<T, TorusSpec>, "scaledSpec: unhandled primitive");
                if (std::abs(f[0] - f[1]) > 1e-12)
                    throw NotScalable("A torus can only be stretched evenly across its ring");
                p.ringRadiusX *= f[0];
                p.ringRadiusY *= f[1];
                p.tubeRadius *= f[0];
                p.tubeRadiusZ *= f[2];
            }
            return p;
        },
        spec);
}

std::vector<Body> scaledTree(const Document& doc, ObjectId id, const Vec3& factors, const Vec3& anchor)
{
    for (double v : factors)
        if (!(v > 0.0) || !std::isfinite(v))
            throw std::invalid_argument("Scale factors must be positive");
    std::vector<Body> out;
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (body->isBoolean()) {
        scaleInto(doc, id, factors, anchor, false, out);
    } else {
        // A primitive on its own: scale it in its own frame (placement adjusted).
        Body next = *body;
        next.spec = scaledSpec(body->spec, factors);
        // Keep `anchor` fixed: in body coordinates the shape becomes S x, so move by
        // R (anchor - S anchor) plus the pivot change.
        const Transform before = placementTransform(body->placement, pivotOf(body->spec));
        const Vec3 fixedAt = before.apply(anchor);
        const Transform after = placementTransform(next.placement, pivotOf(next.spec));
        const Vec3 nowAt = after.apply(mul(factors, anchor));
        next.placement.x += fixedAt[0] - nowAt[0];
        next.placement.y += fixedAt[1] - nowAt[1];
        next.placement.z += fixedAt[2] - nowAt[2];
        validate(next.spec);
        out.push_back(std::move(next));
    }
    return out;
}

}  // namespace forge
