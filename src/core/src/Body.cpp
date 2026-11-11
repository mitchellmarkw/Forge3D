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
            {&CylinderSpec::radiusX, {"radiusX", "Radius X", "Distance from the centre line to the side, along X", false}},
            {&CylinderSpec::radiusY, {"radiusY", "Radius Y", "Distance from the centre line to the side, along Y", false}},
            {&CylinderSpec::height, {"height", "Height (Z)", "Size along Z (up)", false}},
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
            {&SphereSpec::radius, {"radius", "Radius", "Distance from the centre to the surface", false}},
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
            {&ConeSpec::radiusBottom, {"radiusBottom", "Bottom radius", "Radius at the base. 0 makes a point at the bottom", true}},
            {&ConeSpec::radiusTop, {"radiusTop", "Top radius", "Radius at the top. 0 makes a point; more than 0 cuts the tip off flat", true}},
            {&ConeSpec::height, {"height", "Height (Z)", "Size along Z (up)", false}},
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
            {&TorusSpec::majorRadius, {"majorRadius", "Ring radius", "From the centre of the hole to the middle of the ring", false}},
            {&TorusSpec::minorRadius, {"minorRadius", "Tube radius", "Half the thickness of the ring. Must be less than the ring radius", false}},
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
            {&TubeSpec::outerRadius, {"outerRadius", "Outer radius", "Radius of the outside", false}},
            {&TubeSpec::innerRadius, {"innerRadius", "Inner radius", "Radius of the hole. Must be less than the outer radius", false}},
            {&TubeSpec::height, {"height", "Height (Z)", "Size along Z (up)", false}},
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
    return out;
}

bool isRoundFromAbove(const PrimitiveSpec& spec)
{
    return !std::holds_alternative<BoxSpec>(spec) && !std::holds_alternative<WedgeSpec>(spec);
}

std::string toString(BooleanOp op)
{
    switch (op) {
    case BooleanOp::Merge: return "Merge";
    case BooleanOp::Subtract: return "Subtract";
    case BooleanOp::Intersect: return "Intersect";
    }
    return "Combine";
}

std::string typeName(const Body& body)
{
    return body.isBoolean() ? toString(body.boolean->op) : typeName(body.spec);
}

}  // namespace forge
