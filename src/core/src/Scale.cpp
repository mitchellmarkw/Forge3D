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
               std::vector<Body>& out, const ScaleOptions& o);

// A joint part kept at its size: moved so its own origin goes where the scaling takes it.
// `local`: its transform in the parent frame being scaled.
Body movedRigidly(const Body& body, const Transform& local, const Vec3& factors, const Vec3& anchor)
{
    const Vec3 q = local.apply(Vec3{0, 0, 0});
    Body next = body;
    for (std::size_t k = 0; k < 3; ++k) {
        const double d = factors[k] * (q[k] - anchor[k]) + anchor[k] - q[k];
        (k == 0 ? next.placement.x : k == 1 ? next.placement.y : next.placement.z) += d;
    }
    return next;
}

// Body `id` lives in a parent frame that is being scaled by `factors` about
// `anchor` (parent coordinates). Updates its spec / placement (or, for a result,
// its parts) so its geometry follows.
void scaleChild(const Document& doc, ObjectId id, const Vec3& factors, const Vec3& anchor, std::vector<Body>& out,
                const ScaleOptions& o)
{
    const Body* body = doc.find(id);
    const Transform local = doc.localTransform(id);
    if (o.keepJoints && body->hardware && even(factors)) {
        out.push_back(movedRigidly(*body, local, factors, anchor));
        return;
    }
    Vec3 bodyFactors = factors;  // the same factors, along the body's own axes
    if (!even(factors) && !bodyFactorsFor(local, factors, bodyFactors))
        throw NotScalable("'" + body->name + "' is turned at an angle, so this shape can only be scaled evenly "
                          "in that direction");

    if (body->isBoolean()) {
        // Its placement stays; scale its contents about the same point, seen from inside it.
        scaleInto(doc, id, bodyFactors, local.inverse().apply(anchor), /*moveSelf=*/false, out, o);
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
               std::vector<Body>& out, const ScaleOptions& o)
{
    const bool keepJoints = o.keepJoints && even(factors);
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (!body->isBoolean()) {
        if (moveSelf)
            scaleChild(doc, id, factors, anchor, out, o);
        return;
    }
    for (ObjectId part : body->boolean->operands)
        scaleChild(doc, part, factors, anchor, out, o);
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
        // The strokes are kept in the part's own coordinates: they go where its surface
        // goes (seen from here, then back into the part as it is after scaling).
        // Evenly scaled, the brushes grow too.
        const ObjectId part = body->boolean->operands.front();
        const Transform before = doc.localTransform(part);
        Transform after = before;
        for (const Body& b : out)
            if (b.id == part)
                after = placementTransform(b.placement, pivotOf(b));
        const Transform back = after.inverse();
        Body next = *body;
        for (SculptStroke& st : next.boolean->sculpt->strokes) {
            for (auto& d : st.dabs) {
                const Vec3 q = before.apply(Vec3{d[0], d[1], d[2]});
                const Vec3 r = back.apply(Vec3{factors[0] * (q[0] - anchor[0]) + anchor[0],
                                               factors[1] * (q[1] - anchor[1]) + anchor[1],
                                               factors[2] * (q[2] - anchor[2]) + anchor[2]});
                d = {float(r[0]), float(r[1]), float(r[2])};
            }
            const Vec3 g = before.applyDirection(Vec3{st.grab[0], st.grab[1], st.grab[2]});
            const Vec3 h = back.applyDirection(Vec3{factors[0] * g[0], factors[1] * g[1], factors[2] * g[2]});
            st.grab = {float(h[0]), float(h[1]), float(h[2])};
            if (even(factors)) {
                st.radius *= factors[0];
                st.finest *= factors[0];
                st.tile *= factors[0];
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
    if (body->boolean->op == BooleanOp::Thread && body->boolean->thread) {
        // The picked face moves with the shape; evenly scaled, the thread grows too.
        Body next = *body;
        ThreadFinish& t = *next.boolean->thread;
        t.face.point = {factors[0] * (t.face.point[0] - anchor[0]) + anchor[0],
                        factors[1] * (t.face.point[1] - anchor[1]) + anchor[1],
                        factors[2] * (t.face.point[2] - anchor[2]) + anchor[2]};
        if (even(factors) && !keepJoints) {
            t.size *= factors[0];
            t.pitch *= factors[0];
        }
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::MaskSplit && body->boolean->maskSplit) {
        // The mask is painted on the shape: it moves and stretches with it (each dab's
        // reach grows only when scaled evenly).
        Body next = *body;
        for (MaskDab& d : next.boolean->maskSplit->dabs) {
            for (std::size_t k = 0; k < 3; ++k)
                d.centre[k] = factors[k] * (d.centre[k] - anchor[k]) + anchor[k];
            if (even(factors))
                d.radius *= factors[0];
        }
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::Hollow && body->boolean->hollow) {
        // The holes move with the shape; evenly scaled, the walls and holes grow too
        // (unless kept: the holes take the plugs, which are joint parts).
        Body next = *body;
        HollowFinish& h = *next.boolean->hollow;
        for (DrainHole& d : h.holes) {
            for (std::size_t k = 0; k < 3; ++k)
                d.point[k] = factors[k] * (d.point[k] - anchor[k]) + anchor[k];
            Vec3 n{d.normal[0] / factors[0], d.normal[1] / factors[1], d.normal[2] / factors[2]};
            const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            d.normal = {n[0] / len, n[1] / len, n[2] / len};
        }
        if (even(factors) && !o.keepWalls)
            h.thickness *= factors[0];
        if (even(factors) && !keepJoints)
            h.holeSize *= factors[0];
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
        if (even(factors) && !(o.keepWalls))
            next.boolean->shell->thickness *= factors[0];
        out.push_back(std::move(next));
        return;
    }
    if (body->boolean->op == BooleanOp::Split || body->boolean->op == BooleanOp::Mirror ||
        body->boolean->op == BooleanOp::Emboss || body->boolean->op == BooleanOp::ExtrudeDown ||
        body->boolean->op == BooleanOp::Socket || body->boolean->op == BooleanOp::Hinge) {
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
        if (next.boolean->socket && even(factors) && !keepJoints) {
            // Scaled evenly, the socket grows with the shape (so does its ball, scaled with
            // it); the fit for printing stays as it is.
            SocketFinish& sf = *next.boolean->socket;
            sf.size *= factors[0];
            sf.depth *= factors[0];
            if (sf.reliefDepth > 0.0)
                sf.reliefDepth *= factors[0];
        }
        if (next.boolean->hinge && even(factors) && !keepJoints) {
            // Scaled evenly, the joint grows with the shape; a filament pin or a rod
            // doesn't (it's bought, not printed), nor does the fit for printing.
            HingeFinish& h = *next.boolean->hinge;
            h.width *= factors[0];
            h.barrel *= factors[0];
            h.reach *= factors[0];
            h.gap *= factors[0];
            h.lip *= factors[0];
            if (h.pin == PinKind::Printed)
                h.pinSize *= factors[0];
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
                if (p.taper != 0.0) {
                    // The sides keep their shape: the lean changes when it's stretched one way.
                    if (std::abs(f[0] - f[1]) > 1e-12 * std::abs(f[0]))
                        throw NotScalable("A tapered shape can only be resized evenly across (its height can "
                                          "change freely)");
                    const double pi = 3.14159265358979323846;
                    p.taper = std::clamp(std::atan(std::tan(p.taper * pi / 180) * f[0] / f[2]) * 180 / pi, -60.0, 60.0);
                }
                p.length *= f[2];
            } else if constexpr (std::is_same_v<T, LoftSpec> || std::is_same_v<T, SweepSpec>) {
                if (std::abs(f[0] - f[1]) > 1e-12 || std::abs(f[0] - f[2]) > 1e-12)
                    throw NotScalable("A loft or a sweep can only be resized evenly");
                p = std::get<T>(scaled(PrimitiveSpec{p}, f[0]));
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

std::vector<Body> scaledTree(const Document& doc, ObjectId id, const Vec3& factors, const Vec3& anchor,
                             const ScaleOptions& options)
{
    for (double v : factors)
        if (!(v > 0.0) || !std::isfinite(v))
            throw std::invalid_argument("Scale factors must be positive");
    std::vector<Body> out;
    const Body* body = doc.find(id);
    if (!body)
        throw std::out_of_range("No body with id " + std::to_string(id));
    if (body->isBoolean()) {
        scaleInto(doc, id, factors, anchor, false, out, options);
    } else if (options.keepJoints && body->hardware && even(factors)) {
        const Transform t = placementTransform(body->placement, pivotOf(body->spec));
        out.push_back(movedRigidly(*body, t, factors, t.apply(anchor)));
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

namespace {

void countJoints(const Document& doc, ObjectId id, int& n)
{
    const Body* b = doc.find(id);
    if (!b)
        return;
    if (b->hardware) {
        ++n;
        return;
    }
    if (!b->isBoolean())
        return;
    const BooleanSpec& s = *b->boolean;
    if ((s.op == BooleanOp::Socket && s.socket) || (s.op == BooleanOp::Hinge && s.hinge) ||
        (s.op == BooleanOp::Thread && s.thread))
        ++n;
    for (ObjectId part : s.operands)
        countJoints(doc, part, n);
}

}  // namespace

std::vector<Body> rescaledDesign(const Document& doc, const std::vector<ObjectId>& ids, double factor,
                                 const Vec3& anchor, const ScaleOptions& options)
{
    if (!(factor > 0.0) || !std::isfinite(factor))
        throw std::invalid_argument("The new scale must be bigger than nothing");
    std::vector<Body> out;
    const Vec3 f{factor, factor, factor};
    for (ObjectId id : ids) {
        const Body* body = doc.find(id);
        if (!body)
            throw std::out_of_range("No body with id " + std::to_string(id));
        const Transform world = doc.worldTransform(id);
        if (options.keepJoints && body->hardware) {
            // Moved whole, by its own origin.
            const Vec3 q = world.apply(Vec3{0, 0, 0});
            Body next = *body;
            next.placement.x += factor * (q[0] - anchor[0]) + anchor[0] - q[0];
            next.placement.y += factor * (q[1] - anchor[1]) + anchor[1] - q[1];
            next.placement.z += factor * (q[2] - anchor[2]) + anchor[2] - q[2];
            out.push_back(std::move(next));
            continue;
        }
        auto changed = scaledTree(doc, id, f, world.inverse().apply(anchor), options);
        out.insert(out.end(), std::make_move_iterator(changed.begin()), std::make_move_iterator(changed.end()));
    }
    return out;
}

int jointCount(const Document& doc, const std::vector<ObjectId>& ids)
{
    int n = 0;
    for (ObjectId id : ids)
        countJoints(doc, id, n);
    return n;
}

}  // namespace forge
