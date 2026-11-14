// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Body.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace forge {

namespace {

// Per-type description: names, and the length fields in display order.
template <class T>
struct Traits;

template <class T>
struct Field {
    double T::*member;
    SpecField info;
};

template <>
struct Traits<BoxSpec> {
    static constexpr const char* name = "Box";
    static constexpr const char* file = "box";
    static const std::vector<Field<BoxSpec>>& fields()
    {
        static const std::vector<Field<BoxSpec>> f{
            {&BoxSpec::width, {"width", "Width (X)", "Size along X", false}},
            {&BoxSpec::depth, {"depth", "Depth (Y)", "Size along Y", false}},
            {&BoxSpec::height, {"height", "Height (Z)", "Size along Z (up)", false}},
        };
        return f;
    }
};

template <>
struct Traits<CylinderSpec> {
    static constexpr const char* name = "Cylinder";
    static constexpr const char* file = "cylinder";
    static const std::vector<Field<CylinderSpec>>& fields()
    {
        static const std::vector<Field<CylinderSpec>> f{
            {&CylinderSpec::radiusX, {"radiusX", "Radius X", "Distance from the centre line to the side, along X", false, 1}},
            {&CylinderSpec::radiusY, {"radiusY", "Radius Y", "Distance from the centre line to the side, along Y", false, 1}},
            {&CylinderSpec::height, {"height", "Height (Z)", "Size along Z (up)", false, 0}},
        };
        return f;
    }
};

template <>
struct Traits<SphereSpec> {
    static constexpr const char* name = "Sphere";
    static constexpr const char* file = "sphere";
    static const std::vector<Field<SphereSpec>>& fields()
    {
        static const std::vector<Field<SphereSpec>> f{
            {&SphereSpec::radiusX, {"radiusX", "Radius X", "Distance from the centre to the surface, along X", false, 1}},
            {&SphereSpec::radiusY, {"radiusY", "Radius Y", "Distance from the centre to the surface, along Y", false, 1}},
            {&SphereSpec::radiusZ, {"radiusZ", "Radius Z (up)", "Distance from the centre to the top", false, 1}},
        };
        return f;
    }
};

template <>
struct Traits<ConeSpec> {
    static constexpr const char* name = "Cone";
    static constexpr const char* file = "cone";
    static const std::vector<Field<ConeSpec>>& fields()
    {
        static const std::vector<Field<ConeSpec>> f{
            {&ConeSpec::bottomRadiusX, {"bottomRadiusX", "Bottom radius X", "Radius of the base along X. Both bottom radii 0 make a point at the bottom", true, 1}},
            {&ConeSpec::bottomRadiusY, {"bottomRadiusY", "Bottom radius Y", "Radius of the base along Y", true, 1}},
            {&ConeSpec::topRadiusX, {"topRadiusX", "Top radius X", "Radius of the top along X. Both top radii 0 make a point; more cuts the tip off flat", true, 2}},
            {&ConeSpec::topRadiusY, {"topRadiusY", "Top radius Y", "Radius of the top along Y", true, 2}},
            {&ConeSpec::height, {"height", "Height (Z)", "Size along Z (up)", false, 0}},
        };
        return f;
    }
};

template <>
struct Traits<TorusSpec> {
    static constexpr const char* name = "Torus";
    static constexpr const char* file = "torus";
    static const std::vector<Field<TorusSpec>>& fields()
    {
        static const std::vector<Field<TorusSpec>> f{
            {&TorusSpec::ringRadiusX, {"ringRadiusX", "Ring radius X", "From the centre of the hole to the middle of the ring, along X", false, 1}},
            {&TorusSpec::ringRadiusY, {"ringRadiusY", "Ring radius Y", "From the centre of the hole to the middle of the ring, along Y", false, 1}},
            {&TorusSpec::tubeRadius, {"tubeRadius", "Tube radius", "Half the width of the ring, across it (flat)", false, 2}},
            {&TorusSpec::tubeRadiusZ, {"tubeRadiusZ", "Tube radius Z (up)", "Half the thickness of the ring, up", false, 2}},
        };
        return f;
    }
};

template <>
struct Traits<TubeSpec> {
    static constexpr const char* name = "Tube";
    static constexpr const char* file = "tube";
    static const std::vector<Field<TubeSpec>>& fields()
    {
        static const std::vector<Field<TubeSpec>> f{
            {&TubeSpec::outerRadiusX, {"outerRadiusX", "Outer radius X", "Radius of the outside along X", false, 1}},
            {&TubeSpec::outerRadiusY, {"outerRadiusY", "Outer radius Y", "Radius of the outside along Y", false, 1}},
            {&TubeSpec::innerRadiusX, {"innerRadiusX", "Inner radius X", "Radius of the hole along X. Must be less than outer radius X", false, 2}},
            {&TubeSpec::innerRadiusY, {"innerRadiusY", "Inner radius Y", "Radius of the hole along Y. Must be less than outer radius Y", false, 2}},
            {&TubeSpec::height, {"height", "Height (Z)", "Size along Z (up)", false, 0}},
        };
        return f;
    }
};

template <>
struct Traits<WedgeSpec> {
    static constexpr const char* name = "Wedge";
    static constexpr const char* file = "wedge";
    static const std::vector<Field<WedgeSpec>>& fields()
    {
        static const std::vector<Field<WedgeSpec>> f{
            {&WedgeSpec::width, {"width", "Width (X)", "Length of the slope along X", false}},
            {&WedgeSpec::depth, {"depth", "Depth (Y)", "Size along Y", false}},
            {&WedgeSpec::height, {"height", "Height (Z)", "Height of the tall end (at -X)", false}},
        };
        return f;
    }
};

template <>
struct Traits<PyramidSpec> {
    static constexpr const char* name = "Pyramid";
    static constexpr const char* file = "pyramid";
    static const std::vector<Field<PyramidSpec>>& fields()
    {
        static const std::vector<Field<PyramidSpec>> f{
            {&PyramidSpec::baseWidth, {"baseWidth", "Base width (X)", "Size of the base along X", false, 0}},
            {&PyramidSpec::baseDepth, {"baseDepth", "Base depth (Y)", "Size of the base along Y", false, 0}},
            {&PyramidSpec::topWidth, {"topWidth", "Top width (X)", "Size of the top along X. Top width and depth both 0 make a point", true, 0}},
            {&PyramidSpec::topDepth, {"topDepth", "Top depth (Y)", "Size of the top along Y. Only one of them 0 makes a ridge, like a roof", true, 0}},
            {&PyramidSpec::height, {"height", "Height (Z)", "Size along Z (up)", false, 0}},
        };
        return f;
    }
};

template <>
struct Traits<TextSpec> {
    static constexpr const char* name = "Text";
    static constexpr const char* file = "text";
    static const std::vector<Field<TextSpec>>& fields()
    {
        static const std::vector<Field<TextSpec>> f{
            {&TextSpec::width, {"width", "Width (X)", "Length of the text along X", false, 0}},
            {&TextSpec::depth, {"depth", "Depth (Y)", "Size of the text along Y: from the lowest letter to the highest", false, 0}},
            {&TextSpec::height, {"height", "Thickness (Z)", "How thick the letters are (up)", false, 0}},
        };
        return f;
    }
};

template <>
struct Traits<PadSpec> {
    static constexpr const char* name = "Pad";
    static constexpr const char* file = "pad";
    static const std::vector<Field<PadSpec>>& fields()
    {
        static const std::vector<Field<PadSpec>> f{
            {&PadSpec::length, {"length", "Length (Z)", "How far the sketch is pushed up", false, 0}},
        };
        return f;
    }
};

template <>
struct Traits<RevolveSpec> {
    static constexpr const char* name = "Revolve";
    static constexpr const char* file = "revolve";
    static const std::vector<Field<RevolveSpec>>& fields()
    {
        // Its size is all in the sketch (the angle isn't a length).
        static const std::vector<Field<RevolveSpec>> f{};
        return f;
    }
};

template <>
struct Traits<HoleSpec> {
    static constexpr const char* name = "Hole";
    static constexpr const char* file = "hole";
    static const std::vector<Field<HoleSpec>>& fields()
    {
        static const std::vector<Field<HoleSpec>> f{
            {&HoleSpec::diameter, {"diameter", "Diameter", "How wide the holes are", false, 0}},
            {&HoleSpec::depth, {"depth", "Depth", "How deep the holes go", false, 0}},
        };
        return f;
    }
};

template <>
struct Traits<MeshSpec> {
    static constexpr const char* name = "Mesh";
    static constexpr const char* file = "mesh";
    static const std::vector<Field<MeshSpec>>& fields()
    {
        static const std::vector<Field<MeshSpec>> f{
            {&MeshSpec::width, {"width", "Width (X)", "Size along X", false, 0}},
            {&MeshSpec::depth, {"depth", "Depth (Y)", "Size along Y", false, 0}},
            {&MeshSpec::height, {"height", "Height (Z)", "Size along Z (up)", false, 0}},
        };
        return f;
    }
};

template <class T>
const std::vector<SpecField>& infos()
{
    static const std::vector<SpecField> list = [] {
        std::vector<SpecField> out;
        for (const auto& f : Traits<T>::fields())
            out.push_back(f.info);
        return out;
    }();
    return list;
}

template <std::size_t... I>
PrimitiveSpec defaultAt(std::size_t kind, std::index_sequence<I...>)
{
    PrimitiveSpec out;
    const bool found = ((kind == I ? (out = std::variant_alternative_t<I, PrimitiveSpec>{}, true) : false) || ...);
    if (!found)
        throw std::out_of_range("No primitive kind " + std::to_string(kind));
    return out;
}

}  // namespace

PrimitiveSpec defaultSpec(std::size_t kind)
{
    return defaultAt(kind, std::make_index_sequence<kPrimitiveKindCount>{});
}

std::string typeName(const PrimitiveSpec& spec)
{
    return std::visit([](const auto& p) -> std::string { return Traits<std::decay_t<decltype(p)>>::name; }, spec);
}

std::string fileTypeName(const PrimitiveSpec& spec)
{
    return std::visit([](const auto& p) -> std::string { return Traits<std::decay_t<decltype(p)>>::file; }, spec);
}

const std::vector<SpecField>& specFields(const PrimitiveSpec& spec)
{
    return std::visit([](const auto& p) -> const std::vector<SpecField>& { return infos<std::decay_t<decltype(p)>>(); },
                      spec);
}

double specValue(const PrimitiveSpec& spec, std::size_t field)
{
    return std::visit(
        [field](const auto& p) -> double {
            const auto& f = Traits<std::decay_t<decltype(p)>>::fields();
            if (field >= f.size())
                throw std::out_of_range("specValue: no such field");
            return p.*(f[field].member);
        },
        spec);
}

void setSpecValue(PrimitiveSpec& spec, std::size_t field, double value)
{
    std::visit(
        [field, value](auto& p) {
            const auto& f = Traits<std::decay_t<decltype(p)>>::fields();
            if (field >= f.size())
                throw std::out_of_range("setSpecValue: no such field");
            p.*(f[field].member) = value;
        },
        spec);
}

PrimitiveSpec scaled(const PrimitiveSpec& spec, double factor)
{
    PrimitiveSpec out = spec;
    for (std::size_t i = 0; i < specFields(spec).size(); ++i)
        setSpecValue(out, i, specValue(spec, i) * factor);
    if (auto* pad = std::get_if<PadSpec>(&out))
        sketch::scale(pad->sketch, factor, factor);
    if (auto* revolve = std::get_if<RevolveSpec>(&out))
        sketch::scale(revolve->sketch, factor, factor);
    if (auto* hole = std::get_if<HoleSpec>(&out)) {
        sketch::scale(hole->sketch, factor, factor);
        hole->headDiameter *= factor;
        hole->headDepth *= factor;
        hole->countersinkDiameter *= factor;
        if (factor != 1.0)
            hole->size.clear();  // no longer a standard size
    }
    return out;
}

bool isSketched(const PrimitiveSpec& spec)
{
    return sketchOf(spec) != nullptr;
}

const sketch::Sketch* sketchOf(const PrimitiveSpec& spec)
{
    if (const auto* p = std::get_if<PadSpec>(&spec))
        return &p->sketch;
    if (const auto* r = std::get_if<RevolveSpec>(&spec))
        return &r->sketch;
    if (const auto* h = std::get_if<HoleSpec>(&spec))
        return &h->sketch;
    return nullptr;
}

bool isRoundFromAbove(const PrimitiveSpec& spec)
{
    return hasRoundGroups(spec);
}

bool hasRoundGroups(const PrimitiveSpec& spec)
{
    for (const SpecField& f : specFields(spec))
        if (f.roundGroup != 0)
            return true;
    return false;
}

namespace {
// Index of the first field in the same round group (the leader), or npos.
std::size_t leaderOf(const std::vector<SpecField>& fields, std::size_t i)
{
    if (fields[i].roundGroup == 0)
        return static_cast<std::size_t>(-1);
    for (std::size_t j = 0; j < fields.size(); ++j)
        if (fields[j].roundGroup == fields[i].roundGroup)
            return j;
    return i;
}
}  // namespace

bool isRound(const PrimitiveSpec& spec)
{
    const auto& fields = specFields(spec);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const std::size_t lead = leaderOf(fields, i);
        if (lead != static_cast<std::size_t>(-1) && specValue(spec, i) != specValue(spec, lead))
            return false;
    }
    return true;
}

PrimitiveSpec madeRound(const PrimitiveSpec& spec)
{
    PrimitiveSpec out = spec;
    const auto& fields = specFields(spec);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const std::size_t lead = leaderOf(fields, i);
        if (lead != static_cast<std::size_t>(-1))
            setSpecValue(out, i, specValue(spec, lead));
    }
    return out;
}

std::string toString(BooleanOp op)
{
    switch (op) {
    case BooleanOp::Merge: return "Merge";
    case BooleanOp::Subtract: return "Subtract";
    case BooleanOp::Intersect: return "Intersect";
    case BooleanOp::Split: return "Split";
    case BooleanOp::Mirror: return "Mirror";
    case BooleanOp::Group: return "Group";
    case BooleanOp::Emboss: return "Emboss";
    case BooleanOp::Edges: return "Edges";
    case BooleanOp::Shell: return "Shell";
    case BooleanOp::Follow: return "Follow";
    case BooleanOp::Pattern: return "Pattern";
    case BooleanOp::Sculpt: return "Sculpt";
    }
    return "Combine";
}

std::string typeName(const Body& body)
{
    return body.isBoolean() ? toString(body.boolean->op) : typeName(body.spec);
}

}  // namespace forge
