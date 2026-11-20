// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/FileFormat.h"

#include "forge/core/Document.h"
#include "forge/core/Hollow.h"
#include "forge/core/Stamp.h"
#include "forge/core/PanelLines.h"
#include "forge/core/MeshKernel.h"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <system_error>
#include <type_traits>

namespace forge::file {

using nlohmann::json;

namespace {

// Pictures go in document.json as base64 of their deflated grey bytes.
std::string toBase64(const std::string& bytes)
{
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t a = std::uint8_t(bytes[i]);
        const std::uint32_t b = i + 1 < bytes.size() ? std::uint8_t(bytes[i + 1]) : 0;
        const std::uint32_t c = i + 2 < bytes.size() ? std::uint8_t(bytes[i + 2]) : 0;
        const std::uint32_t v = a << 16 | b << 8 | c;
        out += table[v >> 18 & 63];
        out += table[v >> 12 & 63];
        out += i + 1 < bytes.size() ? table[v >> 6 & 63] : '=';
        out += i + 2 < bytes.size() ? table[v & 63] : '=';
    }
    return out;
}

std::string fromBase64(const std::string& text)
{
    std::string out;
    std::uint32_t v = 0;
    int bits = 0;
    for (char ch : text) {
        int d = -1;
        if (ch >= 'A' && ch <= 'Z')
            d = ch - 'A';
        else if (ch >= 'a' && ch <= 'z')
            d = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9')
            d = ch - '0' + 52;
        else if (ch == '+')
            d = 62;
        else if (ch == '/')
            d = 63;
        else
            continue;  // '=' and anything else
        v = v << 6 | std::uint32_t(d);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += char(v >> bits & 0xFF);
        }
    }
    return out;
}


const char* const kGeoKinds[] = {"line", "circle", "arc"};
const char* const kRules[] = {"horizontal", "vertical", "parallel", "perpendicular", "equal",  "tangent",
                              "pointOn",    "coincident", "symmetric", "midpoint", "fixed", "distance",
                              "distanceX", "distanceY", "radius", "angle", "gap"};

json sketchToJson(const sketch::Sketch& s)
{
    json points = json::array(), geos = json::array(), rules = json::array();
    for (const auto& p : s.points)
        points.push_back({p.x, p.y});
    for (const auto& g : s.geos)
        geos.push_back(json{{"kind", kGeoKinds[int(g.kind)]}, {"a", g.a}, {"b", g.b}, {"c", g.c}, {"r", g.r},
                            {"construction", g.construction}});
    for (const auto& c : s.constraints)
        rules.push_back(json{{"rule", kRules[int(c.rule)]}, {"a", c.a}, {"b", c.b}, {"c", c.c},
                             {"value", c.value}, {"value2", c.value2}});
    json out{{"points", points}, {"radii", s.radii}, {"geos", geos}, {"rules", rules}};
    if (!s.texts.empty()) {
        json texts = json::array();
        for (const auto& w : s.texts)
            texts.push_back(json{{"text", w.text}, {"font", w.font}, {"bold", w.bold}, {"italic", w.italic},
                                 {"height", w.height}, {"angle", w.angle}, {"at", w.at}});
        out["texts"] = texts;
    }
    return out;
}

sketch::Sketch sketchFromJson(const json& j, const std::string& where)
{
    sketch::Sketch s;
    s.points.clear();
    try {
        for (const auto& p : j.at("points"))
            s.points.push_back({p.at(0).get<double>(), p.at(1).get<double>()});
        s.radii = j.at("radii").get<std::vector<double>>();
        for (const auto& g : j.at("geos")) {
            sketch::Geo geo;
            const std::string kind = g.at("kind").get<std::string>();
            const auto k = std::find(std::begin(kGeoKinds), std::end(kGeoKinds), kind);
            if (k == std::end(kGeoKinds))
                throw std::runtime_error("unknown curve " + kind);
            geo.kind = sketch::GeoKind(k - std::begin(kGeoKinds));
            geo.a = g.at("a").get<int>();
            geo.b = g.at("b").get<int>();
            geo.c = g.at("c").get<int>();
            geo.r = g.at("r").get<int>();
            geo.construction = g.value("construction", false);
            s.geos.push_back(geo);
        }
        for (const auto& r : j.at("rules")) {
            sketch::Constraint c;
            const std::string rule = r.at("rule").get<std::string>();
            const auto k = std::find(std::begin(kRules), std::end(kRules), rule);
            if (k == std::end(kRules))
                throw std::runtime_error("unknown rule " + rule);
            c.rule = sketch::Rule(k - std::begin(kRules));
            c.a = r.at("a").get<int>();
            c.b = r.at("b").get<int>();
            c.c = r.at("c").get<int>();
            c.value = r.at("value").get<double>();
            c.value2 = r.value("value2", 0.0);
            s.constraints.push_back(c);
        }
        if (const auto texts = j.find("texts"); texts != j.end())
            for (const auto& t : *texts) {
                sketch::Words w;
                w.text = t.at("text").get<std::string>();
                w.font = t.value("font", std::string());
                w.bold = t.value("bold", false);
                w.italic = t.value("italic", false);
                w.height = t.at("height").get<double>();
                w.angle = t.value("angle", 0.0);
                w.at = t.at("at").get<int>();
                if (w.at < 0 || std::size_t(w.at) >= s.points.size() || !(w.height > 0.0))
                    throw std::runtime_error("words out of place");
                s.texts.push_back(w);
            }
    } catch (const std::exception& e) {
        throw FileError("The file is damaged (" + where + ": sketch: " + e.what() + ").");
    }
    if (s.points.empty())
        s.points.push_back({});
    if (const std::string wrong = sketch::damage(s); !wrong.empty())
        throw FileError("The file is damaged (" + where + ": sketch: " + wrong + ").");
    return s;
}


constexpr const char* kFormatName = "forge3d-document";

[[noreturn]] void damaged(const std::string& detail)
{
    throw FileError("This file is damaged or is not a Forge3D file.\n(" + detail + ")");
}

// A picture's grey pixels, deflated and in base64.
json pictureToJson(const PictureData& image)
{
    const auto& g = image.grey;
    std::vector<unsigned char> packed(mz_compressBound(mz_ulong(g.size())));
    mz_ulong packedSize = mz_ulong(packed.size());
    if (mz_compress2(packed.data(), &packedSize, g.data(), mz_ulong(g.size()), MZ_BEST_COMPRESSION) != MZ_OK)
        throw FileError("Could not store a picture (out of memory?).");
    return json{{"width", image.width},
                {"height", image.height},
                {"grey", toBase64(std::string(reinterpret_cast<const char*>(packed.data()), packedSize))}};
}

std::shared_ptr<const PictureData> pictureFromJson(const json& im, const std::string& where)
{
    auto data = std::make_shared<PictureData>();
    data->width = im.value("width", 0);
    data->height = im.value("height", 0);
    if (data->width <= 0 || data->height <= 0 || data->width > 100000 || data->height > 100000)
        damaged(where + ": the picture's size is unreadable");
    const std::string packed = fromBase64(im.value("grey", std::string()));
    data->grey.resize(std::size_t(data->width) * std::size_t(data->height));
    mz_ulong size = mz_ulong(data->grey.size());
    if (mz_uncompress(data->grey.data(), &size, reinterpret_cast<const unsigned char*>(packed.data()),
                      mz_ulong(packed.size())) != MZ_OK ||
        size != data->grey.size())
        damaged(where + ": the picture is damaged");
    return data;
}

json placementToJson(const Placement& p)
{
    return json{{"x", p.x},
                {"y", p.y},
                {"z", p.z},
                {"rotationX", p.rotationX},
                {"rotationY", p.rotationY},
                {"rotationZ", p.rotationZ}};
}

const char* opName(BooleanOp op)
{
    switch (op) {
    case BooleanOp::Merge: return "merge";
    case BooleanOp::Subtract: return "subtract";
    case BooleanOp::Intersect: return "intersect";
    case BooleanOp::Split: return "split";
    case BooleanOp::Mirror: return "mirror";
    case BooleanOp::Group: return "group";
    case BooleanOp::Emboss: return "emboss";
    case BooleanOp::Edges: return "edges";
    case BooleanOp::Shell: return "shell";
    case BooleanOp::Follow: return "follow";
    case BooleanOp::Pattern: return "pattern";
    case BooleanOp::Sculpt: return "sculpt";
    case BooleanOp::Simplify: return "simplify";
    case BooleanOp::Smooth: return "smooth";
    case BooleanOp::ExtrudeDown: return "extrudeDown";
    case BooleanOp::Thread: return "thread";
    case BooleanOp::Socket: return "socket";
    case BooleanOp::Hinge: return "hinge";
    case BooleanOp::MaskSplit: return "maskSplit";
    case BooleanOp::Hollow: return "hollow";
    case BooleanOp::Stamp: return "stamp";
    case BooleanOp::PanelLines: return "panelLines";
    }
    return "merge";
}

json bodyToJson(const Body& b)
{
    json j{{"id", b.id}, {"name", b.name}, {"placement", placementToJson(b.placement)}};
    if (b.colour) {
        char hex[8];
        std::snprintf(hex, sizeof hex, "#%06X", unsigned(*b.colour & 0xFFFFFFu));
        j["colour"] = hex;
    }
    if (b.hardware)
        j["hardware"] = true;
    if (b.isBoolean()) {
        j["type"] = "combine";
        j["operation"] = opName(b.boolean->op);
        j["operands"] = b.boolean->operands;
        j["pivot"] = b.boolean->pivot;
        if (b.boolean->op == BooleanOp::Split || b.boolean->op == BooleanOp::Mirror ||
            b.boolean->op == BooleanOp::ExtrudeDown || b.boolean->op == BooleanOp::Socket ||
            b.boolean->op == BooleanOp::Hinge || b.boolean->op == BooleanOp::Emboss) {
            j["planePoint"] = b.boolean->planePoint;
            j["planeNormal"] = b.boolean->planeNormal;
        }
        if (b.boolean->throughAll)
            j["throughAll"] = true;
        if (b.suppressed)
            j["suppressed"] = true;
        if (b.rolledBack)
            j["rolledBack"] = true;
        if (const auto& f = b.boolean->edges) {
            static const char* kinds[] = {"rounding", "fillet", "chamfer", "bevel"};
            json picks = json::array();
            for (const EdgeRef& r : f->edges)
            {
                json pick{{"point", r.point}, {"facePoint", r.facePoint}};
                if (r.located) {
                    pick["relative"] = r.relative;
                    pick["faceNormal"] = r.faceNormal;
                }
                picks.push_back(pick);
            }
            j["edges"] = json{{"kind", kinds[int(f->kind)]}, {"size", f->size}, {"angle", f->angle}, {"picks", picks}};
        }
        if (const auto& sh = b.boolean->shell) {
            json open = json::array();
            for (const FaceRef& r : sh->open) {
                json pick{{"point", r.point}, {"normal", r.normal}};
                if (r.located)
                    pick["relative"] = r.relative;
                open.push_back(pick);
            }
            j["shell"] = json{{"thickness", sh->thickness}, {"outward", sh->outward}, {"open", open}};
        }
        if (const auto& sc = b.boolean->sculpt) {
            json strokes = json::array();
            for (const SculptStroke& st : sc->strokes) {
                std::vector<float> dabs;
                dabs.reserve(st.dabs.size() * 3);
                for (const auto& d : st.dabs)
                    dabs.insert(dabs.end(), d.begin(), d.end());
                json sj{{"brush", st.brush},       {"invert", st.invert},   {"radius", st.radius},
                        {"strength", st.strength}, {"mirrorX", st.mirrorX}, {"mirrorY", st.mirrorY},
                        {"mirrorZ", st.mirrorZ},   {"finest", st.finest},   {"dabs", dabs},
                        {"grab", st.grab}};
                if (st.pattern >= 0) {
                    sj["pattern"] = st.pattern;
                    sj["tile"] = st.tile;
                }
                strokes.push_back(std::move(sj));
            }
            j["sculpt"] = json{{"detail", sc->detail}, {"strokes", strokes}};
            if (!sc->patterns.empty()) {
                json patterns = json::array();
                for (const SculptPattern& p : sc->patterns) {
                    json pj{{"name", p.name}, {"builtIn", p.builtIn}};
                    if (!p.builtIn && p.image)
                        pj["image"] = pictureToJson(*p.image);
                    patterns.push_back(std::move(pj));
                }
                j["sculpt"]["patterns"] = patterns;
            }
        }
        if (const auto& t = b.boolean->thread) {
            json face{{"point", t->face.point}, {"normal", t->face.normal}};
            if (t->face.located)
                face["relative"] = t->face.relative;
            j["thread"] = json{{"face", face},   {"size", t->size},         {"pitch", t->pitch},
                               {"fit", t->fit}, {"leftHand", t->leftHand}};
        }
        if (const auto& ms = b.boolean->maskSplit) {
            json dabs = json::array();
            for (const MaskDab& d : ms->dabs)
                dabs.push_back(json::array({d.centre[0], d.centre[1], d.centre[2], d.radius, d.erase ? 1 : 0}));
            j["mask"] = json{{"piece", ms->masked ? "masked" : "rest"}, {"dabs", dabs}};
            if (ms->keys)
                j["mask"]["keyFit"] = ms->keyFit;  // alignment keys on the cuts
        }
        if (const auto& so = b.boolean->socket) {
            static const char* shapes[] = {"round", "square", "hexagon", "octagon"};
            j["socket"] = json{{"kind", so->kind == SocketKind::Ball     ? "ball"
                                        : so->kind == SocketKind::Magnet ? "magnet"
                                                                         : "peg"},
                               {"size", so->size},
                               {"depth", so->depth},
                               {"fit", so->fit},
                               {"printer", so->resin ? "resin" : "fdm"},
                               {"snap", so->snap},
                               {"grip", so->grip},
                               {"shape", shapes[int(so->shape)]},
                               {"turn", so->turn},
                               {"reliefCuts", so->reliefCuts},
                               {"reliefWidth", so->reliefWidth},
                               {"reliefDepth", so->reliefDepth}};
            if (so->kind == SocketKind::Magnet && so->cover > 0.0)
                j["socket"]["cover"] = so->cover;
            if (so->ridges > 0) {
                j["socket"]["ridges"] = so->ridges;
                j["socket"]["ridgeHeight"] = so->ridgeHeight;
            }
        }
        if (const auto& h = b.boolean->hinge) {
            static const char* kinds[] = {"single", "double", "swivel"};
            static const char* pins[] = {"printed", "filament", "rod"};
            static const char* swings[] = {"both", "forward", "back"};
            j["hinge"] = json{{"kind", kinds[int(h->kind)]},
                              {"half", h->half == HingeHalf::Fork ? "fork" : "tab"},
                              {"width", h->width},
                              {"barrel", h->barrel},
                              {"pin", pins[int(h->pin)]},
                              {"pinSize", h->pinSize},
                              {"fit", h->fit},
                              {"printer", h->resin ? "resin" : "fdm"},
                              {"stiff", h->stiff},
                              {"swing", swings[int(h->swing)]},
                              {"turn", h->turn},
                              {"reach", h->reach},
                              {"gap", h->gap},
                              {"lip", h->lip},
                              {"link", h->link}};
            if (h->clicks > 0) {
                j["hinge"]["clicks"] = h->clicks;
                j["hinge"]["clickDepth"] = h->clickDepth;
            }
        }
        if (const auto& sf = b.boolean->simplify)
            j["simplify"] = json{{"keep", sf->keep}};
        if (const auto& sm = b.boolean->smooth)
            j["smooth"] = json{{"strength", sm->strength}};
        if (const auto& pl = b.boolean->panelLines) {
            json lines = json::array();
            for (const PanelLine& l : pl->lines) {
                json points = json::array();
                for (const PanelPoint& p : l.points)
                    points.push_back(json::array({p.point[0], p.point[1], p.point[2], p.normal[0], p.normal[1],
                                                  p.normal[2]}));
                lines.push_back({{"closed", l.closed}, {"points", std::move(points)}});
            }
            j["panelLines"] = json{{"profile", panel::key(pl->profile)},
                                   {"width", pl->width},
                                   {"depth", pl->depth},
                                   {"lines", std::move(lines)}};
        }
        if (const auto& st = b.boolean->stamp) {
            json spots = json::array();
            for (const StampSpot& sp : st->spots)
                spots.push_back({{"point", sp.point}, {"normal", sp.normal}, {"along", sp.along}});
            j["stamp"] = json{{"kind", stamp::key(st->kind)}, {"size", st->size},   {"height", st->height},
                              {"sunk", st->sunk},              {"spin", st->spin},   {"spacing", st->spacing},
                              {"spots", std::move(spots)}};
        }
        if (const auto& ho = b.boolean->hollow) {
            json holes = json::array();
            for (const DrainHole& h : ho->holes)
                holes.push_back({{"point", h.point}, {"normal", h.normal}});
            j["hollow"] = json{{"thickness", ho->thickness}, {"holeSize", ho->holeSize}, {"holes", std::move(holes)}};
        }
        if (const auto& p = b.boolean->pattern) {
            static const char* kinds[] = {"row", "grid", "circle"};
            j["pattern"] = json{{"kind", kinds[int(p->kind)]}, {"feature", p->feature},  {"dir1", p->dir1},
                                {"dir2", p->dir2},             {"count1", p->count1},    {"count2", p->count2},
                                {"spacing1", p->spacing1},     {"spacing2", p->spacing2}, {"centre", p->centre},
                                {"axis", p->axis},             {"count", p->count},       {"angle", p->angle}};
        }
        if (const auto& f = b.boolean->follow) {
            static const char* profiles[] = {"round", "square", "v", "custom"};
            json fj{{"path", sketchToJson(f->path)},
                    {"origin", f->origin},
                    {"xAxis", f->xAxis},
                    {"normal", f->normal},
                    {"ridge", f->ridge},
                    {"profile", profiles[int(f->profile)]},
                    {"width", f->width},
                    {"depth", f->depth}};
            if (f->located)
                fj["relative"] = f->relative;
            if (f->profile == FollowProfile::Custom)
                fj["custom"] = sketchToJson(f->custom);
            j["follow"] = fj;
        }
        if (const auto& p = b.boolean->picture) {
            json pj{{"name", p->name},   {"relief", p->relief}, {"threshold", p->threshold}, {"invert", p->invert},
                    {"width", p->width}, {"depth", p->depth},   {"raised", p->raised},       {"angle", p->angle}};
            if (p->image)
                pj["image"] = pictureToJson(*p->image);
            j["picture"] = pj;
        }
        if (const auto& e = b.boolean->emboss) {
            j["emboss"] = json{{"text", e->text},     {"font", e->font},   {"bold", e->bold},
                               {"italic", e->italic}, {"vertical", e->vertical}, {"letterHeight", e->letterHeight},
                               {"depth", e->depth},   {"raised", e->raised}, {"angle", e->angle}};
        }
        return j;
    }
    j["type"] = fileTypeName(b.spec);
    if (std::holds_alternative<MeshSpec>(b.spec))
        j["data"] = "meshes/" + std::to_string(b.id) + ".bin";
    if (const auto* pad = std::get_if<PadSpec>(&b.spec)) {
        j["reversed"] = pad->reversed;
        j["midplane"] = pad->midplane;
        if (pad->taper != 0.0)
            j["taper"] = pad->taper;
        j["sketch"] = sketchToJson(pad->sketch);
    }
    if (const auto* l = std::get_if<LoftSpec>(&b.spec)) {
        json sections = json::array();
        for (const LoftSection& s : l->sections)
            sections.push_back(json{{"height", s.height}, {"sketch", sketchToJson(s.sketch)}});
        j["sections"] = sections;
        j["ruled"] = l->ruled;
    }
    if (const auto* w = std::get_if<SweepSpec>(&b.spec)) {
        j["profile"] = sketchToJson(w->profile);
        j["spiral"] = w->spiral;
        if (w->spiral) {
            j["pitch"] = w->pitch;
            j["turns"] = w->turns;
            j["leftHand"] = w->leftHand;
        } else {
            j["path"] = sketchToJson(w->path);
        }
    }
    if (const auto* r = std::get_if<RevolveSpec>(&b.spec)) {
        j["axis"] = r->axis;
        j["angle"] = r->angle;
        j["reversed"] = r->reversed;
        j["midplane"] = r->midplane;
        j["sketch"] = sketchToJson(r->sketch);
    }
    if (const auto* h = std::get_if<HoleSpec>(&b.spec)) {
        static const char* kinds[] = {"plain", "counterbored", "countersunk"};
        j["kind"] = kinds[int(h->kind)];
        j["size"] = h->size;
        j["tapped"] = h->tapped;
        j["headDiameter"] = h->headDiameter;
        j["headDepth"] = h->headDepth;
        j["countersinkDiameter"] = h->countersinkDiameter;
        j["countersinkAngle"] = h->countersinkAngle;
        j["sketch"] = sketchToJson(h->sketch);
    }
    if (const auto* t = std::get_if<TextSpec>(&b.spec)) {
        j["text"] = t->text;
        j["font"] = t->font;
        j["bold"] = t->bold;
        j["italic"] = t->italic;
        j["vertical"] = t->vertical;
    }
    json params = json::object();
    const auto& fields = specFields(b.spec);
    for (std::size_t i = 0; i < fields.size(); ++i)
        params[fields[i].key] = specValue(b.spec, i);
    j["parameters"] = std::move(params);
    return j;
}

double number(const json& obj, const char* key, const std::string& where)
{
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number())
        damaged(where + ": missing number '" + key + "'");
    return it->get<double>();
}

Body bodyFromJson(const json& j, std::size_t index, int version,
                  const std::function<std::string(const std::string&)>& readEntry)
{
    const std::string where = "body " + std::to_string(index + 1);
    if (!j.is_object())
        damaged(where + " is not an object");

    Body b;
    const auto id = j.find("id");
    if (id == j.end() || !id->is_number_unsigned())
        damaged(where + ": missing id");
    b.id = id->get<ObjectId>();

    const auto name = j.find("name");
    b.name = (name != j.end() && name->is_string()) ? name->get<std::string>() : std::string();

    if (const auto colour = j.find("colour"); colour != j.end()) {
        const std::string hex = colour->is_string() ? colour->get<std::string>() : std::string();
        if (hex.size() != 7 || hex[0] != '#' ||
            hex.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos)
            damaged(where + ": colour is not like #RRGGBB");
        b.colour = std::uint32_t(std::stoul(hex.substr(1), nullptr, 16));
    }
    b.hardware = j.value("hardware", false);

    const auto placement = j.find("placement");
    if (placement == j.end() || !placement->is_object())
        damaged(where + ": missing placement");
    b.placement.x = number(*placement, "x", where);
    b.placement.y = number(*placement, "y", where);
    b.placement.z = number(*placement, "z", where);
    b.placement.rotationZ = number(*placement, "rotationZ", where);
    if (version >= 2) {
        b.placement.rotationX = number(*placement, "rotationX", where);
        b.placement.rotationY = number(*placement, "rotationY", where);
    }  // version 1 files only turned about Z; X and Y stay 0

    const auto type = j.find("type");
    if (type == j.end() || !type->is_string())
        damaged(where + ": missing type");
    const std::string t = type->get<std::string>();

    if (t == "combine" && version >= 4) {
        BooleanSpec spec;
        const std::string op = j.value("operation", std::string());
        if (op == "merge")
            spec.op = BooleanOp::Merge;
        else if (op == "subtract")
            spec.op = BooleanOp::Subtract;
        else if (op == "intersect")
            spec.op = BooleanOp::Intersect;
        else if (op == "split" && version >= 7)
            spec.op = BooleanOp::Split;
        else if (op == "mirror" && version >= 8)
            spec.op = BooleanOp::Mirror;
        else if (op == "group" && version >= 8)
            spec.op = BooleanOp::Group;
        else if (op == "emboss" && version >= 10)
            spec.op = BooleanOp::Emboss;
        else if (op == "edges" && version >= 11)
            spec.op = BooleanOp::Edges;
        else if (op == "shell" && version >= 14)
            spec.op = BooleanOp::Shell;
        else if (op == "follow" && version >= 15)
            spec.op = BooleanOp::Follow;
        else if (op == "pattern" && version >= 16)
            spec.op = BooleanOp::Pattern;
        else if (op == "sculpt" && version >= 18)
            spec.op = BooleanOp::Sculpt;
        else if (op == "simplify" && version >= 20)
            spec.op = BooleanOp::Simplify;
        else if (op == "thread" && version >= 22)
            spec.op = BooleanOp::Thread;
        else if (op == "smooth" && version >= 25)
            spec.op = BooleanOp::Smooth;
        else if (op == "extrudeDown" && version >= 25)
            spec.op = BooleanOp::ExtrudeDown;
        else if (op == "socket" && version >= 26)
            spec.op = BooleanOp::Socket;
        else if (op == "maskSplit" && version >= 27)
            spec.op = BooleanOp::MaskSplit;
        else if (op == "hinge" && version >= 29)
            spec.op = BooleanOp::Hinge;
        else if (op == "hollow" && version >= 34)
            spec.op = BooleanOp::Hollow;
        else if (op == "stamp" && version >= 35)
            spec.op = BooleanOp::Stamp;
        else if (op == "panelLines" && version >= 36)
            spec.op = BooleanOp::PanelLines;
        else
            damaged(where + ": unknown combine operation '" + op + "'");
        const auto operands = j.find("operands");
        if (operands == j.end() || !operands->is_array())
            damaged(where + ": missing operands");
        for (const auto& operand : *operands) {
            if (!operand.is_number_unsigned())
                damaged(where + ": bad operand id");
            spec.operands.push_back(operand.get<ObjectId>());
        }
        const auto pivot = j.find("pivot");
        if (pivot == j.end() || !pivot->is_array() || pivot->size() != 3)
            damaged(where + ": missing pivot");
        for (std::size_t i = 0; i < 3; ++i) {
            if (!(*pivot)[i].is_number())
                damaged(where + ": bad pivot");
            spec.pivot[i] = (*pivot)[i].get<double>();
        }
        if (spec.op == BooleanOp::Split || spec.op == BooleanOp::Mirror || spec.op == BooleanOp::Emboss ||
            spec.op == BooleanOp::ExtrudeDown || spec.op == BooleanOp::Socket || spec.op == BooleanOp::Hinge) {
            const auto readVec = [&](const char* key, std::array<double, 3>& out) {
                const auto v = j.find(key);
                if (v == j.end() || !v->is_array() || v->size() != 3)
                    damaged(where + ": missing " + key);
                for (std::size_t i = 0; i < 3; ++i) {
                    if (!(*v)[i].is_number())
                        damaged(where + ": bad " + key);
                    out[i] = (*v)[i].get<double>();
                }
            };
            readVec("planePoint", spec.planePoint);
            readVec("planeNormal", spec.planeNormal);
        }
        if (spec.op == BooleanOp::Edges) {
            const auto e = j.find("edges");
            if (e == j.end() || !e->is_object())
                damaged(where + ": missing the edges");
            EdgeFinish f;
            const std::string kind = e->value("kind", std::string());
            if (kind == "rounding")
                f.kind = EdgeFinishKind::Rounding;
            else if (kind == "fillet")
                f.kind = EdgeFinishKind::Fillet;
            else if (kind == "chamfer")
                f.kind = EdgeFinishKind::Chamfer;
            else if (kind == "bevel")
                f.kind = EdgeFinishKind::Bevel;
            else
                damaged(where + ": unknown edge finish '" + kind + "'");
            f.size = number(*e, "size", where);
            f.angle = e->value("angle", 45.0);
            const auto picks = e->find("picks");
            if (picks == e->end() || !picks->is_array())
                damaged(where + ": missing the picked edges");
            for (const auto& p : *picks) {
                EdgeRef r;
                const auto readPoint = [&](const char* key, std::array<double, 3>& out) {
                    const auto v = p.find(key);
                    if (v == p.end() || !v->is_array() || v->size() != 3)
                        damaged(where + ": a picked edge is damaged");
                    for (std::size_t i = 0; i < 3; ++i)
                        out[i] = (*v)[i].get<double>();
                };
                readPoint("point", r.point);
                readPoint("facePoint", r.facePoint);
                if (p.contains("relative") && p.contains("faceNormal")) {
                    readPoint("relative", r.relative);
                    readPoint("faceNormal", r.faceNormal);
                    r.located = true;
                }
                f.edges.push_back(r);
            }
            spec.edges = std::move(f);
        }
        if (spec.op == BooleanOp::Sculpt) {
            const auto sj = j.find("sculpt");
            if (sj == j.end() || !sj->is_object())
                damaged(where + ": missing the sculpting");
            SculptFinish sc;
            sc.detail = std::clamp(sj->value("detail", 1), 0, 2);
            const auto strokes = sj->find("strokes");
            if (strokes != sj->end() && strokes->is_array())
                for (const auto& s : *strokes) {
                    if (!s.is_object())
                        damaged(where + ": a sculpting stroke is unreadable");
                    SculptStroke st;
                    st.brush = std::clamp(s.value("brush", 0), 0, kLastSculptBrush);
                    st.invert = s.value("invert", false);
                    st.radius = s.value("radius", 1.0);
                    st.strength = s.value("strength", 0.5);
                    st.mirrorX = s.value("mirrorX", false);
                    st.mirrorY = s.value("mirrorY", false);
                    st.mirrorZ = s.value("mirrorZ", false);
                    st.finest = s.value("finest", 0.0);
                    st.pattern = s.value("pattern", -1);
                    st.tile = s.value("tile", 0.0);
                    const auto dabs = s.value("dabs", std::vector<float>{});
                    for (std::size_t i = 0; i + 2 < dabs.size(); i += 3)
                        st.dabs.push_back({dabs[i], dabs[i + 1], dabs[i + 2]});
                    if (const auto g = s.find("grab"); g != s.end() && g->is_array() && g->size() == 3)
                        st.grab = {g->at(0).get<float>(), g->at(1).get<float>(), g->at(2).get<float>()};
                    sc.strokes.push_back(std::move(st));
                }
            if (const auto pats = sj->find("patterns"); pats != sj->end() && pats->is_array())
                for (const auto& pj : *pats) {
                    if (!pj.is_object())
                        damaged(where + ": a sculpting pattern is unreadable");
                    SculptPattern pat;
                    pat.name = pj.value("name", std::string());
                    pat.builtIn = pj.value("builtIn", true);
                    if (!pat.builtIn) {
                        const auto im = pj.find("image");
                        if (im == pj.end() || !im->is_object())
                            damaged(where + ": a sculpting pattern's picture is missing");
                        pat.image = pictureFromJson(*im, where);
                    }
                    sc.patterns.push_back(std::move(pat));
                }
            for (SculptStroke& st : sc.strokes)
                if (st.pattern >= int(sc.patterns.size()))
                    damaged(where + ": a sculpting stroke's pattern is missing");
            spec.sculpt = std::move(sc);
        }
        if (spec.op == BooleanOp::Thread) {
            const auto tj = j.find("thread");
            if (tj == j.end() || !tj->is_object())
                damaged(where + ": missing the thread");
            ThreadFinish t;
            const auto fj = tj->find("face");
            if (fj == tj->end() || !fj->is_object())
                damaged(where + ": the thread's face is missing");
            const auto readPoint = [&](const char* key, std::array<double, 3>& out) {
                const auto v = fj->find(key);
                if (v == fj->end() || !v->is_array() || v->size() != 3)
                    damaged(where + ": the thread's face is damaged");
                for (std::size_t i = 0; i < 3; ++i)
                    out[i] = (*v)[i].get<double>();
            };
            readPoint("point", t.face.point);
            readPoint("normal", t.face.normal);
            if (fj->contains("relative")) {
                readPoint("relative", t.face.relative);
                t.face.located = true;
            }
            t.size = number(*tj, "size", where);
            t.pitch = number(*tj, "pitch", where);
            t.fit = tj->value("fit", 0.2);
            t.leftHand = tj->value("leftHand", false);
            spec.thread = t;
        }
        if (spec.op == BooleanOp::MaskSplit) {
            const auto mj = j.find("mask");
            if (mj == j.end() || !mj->is_object())
                damaged(where + ": missing the mask");
            MaskSplitFinish ms;
            ms.masked = mj->value("piece", std::string("masked")) != "rest";
            const auto dj = mj->find("dabs");
            if (dj == mj->end() || !dj->is_array())
                damaged(where + ": the mask's dabs are missing");
            for (const auto& d : *dj) {
                if (!d.is_array() || d.size() != 5)
                    damaged(where + ": a mask dab is damaged");
                for (std::size_t i = 0; i < 5; ++i)
                    if (!d[i].is_number())
                        damaged(where + ": a mask dab is damaged");
                MaskDab dab;
                dab.centre = {d[0].get<double>(), d[1].get<double>(), d[2].get<double>()};
                dab.radius = d[3].get<double>();
                dab.erase = d[4].get<double>() != 0.0;
                ms.dabs.push_back(dab);
            }
            if (const auto kf = mj->find("keyFit"); kf != mj->end() && kf->is_number()) {
                ms.keys = true;
                ms.keyFit = std::clamp(kf->get<double>(), 0.0, 1.0);
            }
            spec.maskSplit = std::move(ms);
        }
        if (spec.op == BooleanOp::Socket) {
            const auto sj = j.find("socket");
            if (sj == j.end() || !sj->is_object())
                damaged(where + ": missing the socket's settings");
            SocketFinish so;
            const std::string kind = sj->value("kind", std::string("ball"));
            so.kind = kind == "peg" ? SocketKind::Peg : kind == "magnet" ? SocketKind::Magnet : SocketKind::Ball;
            so.cover = sj->value("cover", 0.0);
            so.size = number(*sj, "size", where);
            so.depth = sj->value("depth", so.depth);
            so.fit = sj->value("fit", so.fit);
            so.resin = sj->value("printer", std::string("resin")) != "fdm";
            so.snap = sj->value("snap", true);
            so.grip = sj->value("grip", so.grip);
            const std::string shape = sj->value("shape", std::string("round"));
            so.shape = shape == "square"    ? PegShape::Square
                       : shape == "hexagon" ? PegShape::Hexagon
                       : shape == "octagon" ? PegShape::Octagon
                                            : PegShape::Round;
            so.turn = sj->value("turn", 0.0);
            so.reliefCuts = sj->value("reliefCuts", 0);
            so.reliefWidth = sj->value("reliefWidth", so.reliefWidth);
            so.reliefDepth = sj->value("reliefDepth", 0.0);
            so.ridges = sj->value("ridges", 0);
            so.ridgeHeight = sj->value("ridgeHeight", so.ridgeHeight);
            spec.socket = so;
        }
        if (spec.op == BooleanOp::Hinge) {
            const auto hj = j.find("hinge");
            if (hj == j.end() || !hj->is_object())
                damaged(where + ": missing the hinge's settings");
            HingeFinish h;
            const std::string kind = hj->value("kind", std::string("single"));
            h.kind = kind == "double" ? HingeKind::Double : kind == "swivel" ? HingeKind::Swivel : HingeKind::Single;
            h.half = hj->value("half", std::string("fork")) == "tab" ? HingeHalf::Tab : HingeHalf::Fork;
            h.width = number(*hj, "width", where);
            h.barrel = number(*hj, "barrel", where);
            const std::string pin = hj->value("pin", std::string("printed"));
            h.pin = pin == "filament" ? PinKind::Filament : pin == "rod" ? PinKind::Rod : PinKind::Printed;
            h.pinSize = hj->value("pinSize", h.pinSize);
            h.fit = hj->value("fit", h.fit);
            h.resin = hj->value("printer", std::string("resin")) != "fdm";
            h.stiff = hj->value("stiff", true);
            const std::string swing = hj->value("swing", std::string("both"));
            h.swing = swing == "forward" ? HingeSwing::Forward : swing == "back" ? HingeSwing::Back : HingeSwing::Both;
            h.turn = hj->value("turn", 0.0);
            h.reach = hj->value("reach", 0.0);
            h.gap = hj->value("gap", h.gap);
            h.lip = hj->value("lip", h.lip);
            h.link = hj->value("link", std::uint32_t(0));
            h.clicks = hj->value("clicks", 0);
            h.clickDepth = hj->value("clickDepth", h.clickDepth);
            spec.hinge = h;
        }
        if (spec.op == BooleanOp::PanelLines) {
            const auto pj = j.find("panelLines");
            if (pj == j.end() || !pj->is_object())
                damaged(where + ": missing the panel lines");
            PanelLineFinish pl;
            const auto profile = panel::fromKey(pj->value("profile", std::string()));
            if (!profile)
                damaged(where + ": an unknown groove shape");
            pl.profile = *profile;
            pl.width = number(*pj, "width", where);
            pl.depth = number(*pj, "depth", where);
            if (const auto list = pj->find("lines"); list != pj->end() && list->is_array())
                for (const json& lj : *list) {
                    PanelLine line;
                    try {
                        line.closed = lj.value("closed", false);
                        for (const json& p : lj.at("points")) {
                            PanelPoint q;
                            for (std::size_t k = 0; k < 3; ++k) {
                                q.point[k] = p.at(k).get<double>();
                                q.normal[k] = p.at(k + 3).get<double>();
                            }
                            line.points.push_back(q);
                        }
                    } catch (const std::exception&) {
                        damaged(where + ": a panel line is broken");
                    }
                    pl.lines.push_back(std::move(line));
                }
            try {
                panel::check(pl);
            } catch (const std::exception& e) {
                damaged(where + ": " + e.what());
            }
            spec.panelLines = std::move(pl);
        }
        if (spec.op == BooleanOp::Stamp) {
            const auto sj = j.find("stamp");
            if (sj == j.end() || !sj->is_object())
                damaged(where + ": missing the stamp's settings");
            StampFinish st;
            const auto kind = stamp::fromKey(sj->value("kind", std::string()));
            if (!kind)
                damaged(where + ": an unknown kind of stamp");
            st.kind = *kind;
            st.size = number(*sj, "size", where);
            st.height = number(*sj, "height", where);
            st.sunk = sj->value("sunk", false);
            st.spin = sj->value("spin", 0.0);
            st.spacing = sj->value("spacing", st.spacing);
            if (const auto list = sj->find("spots"); list != sj->end() && list->is_array())
                for (const json& sp : *list) {
                    StampSpot d;
                    try {
                        for (std::size_t k = 0; k < 3; ++k) {
                            d.point[k] = sp.at("point").at(k).get<double>();
                            d.normal[k] = sp.at("normal").at(k).get<double>();
                            d.along[k] = sp.at("along").at(k).get<double>();
                        }
                    } catch (const std::exception&) {
                        damaged(where + ": a stamp's spot is broken");
                    }
                    st.spots.push_back(d);
                }
            try {
                stamp::check(st);
            } catch (const std::exception& e) {
                damaged(where + ": " + e.what());
            }
            spec.stamp = std::move(st);
        }
        if (spec.op == BooleanOp::Hollow) {
            const auto hj = j.find("hollow");
            if (hj == j.end() || !hj->is_object())
                damaged(where + ": missing the hollowing's settings");
            HollowFinish ho;
            ho.thickness = number(*hj, "thickness", where);
            ho.holeSize = hj->value("holeSize", ho.holeSize);
            if (const auto list = hj->find("holes"); list != hj->end() && list->is_array())
                for (const json& h : *list) {
                    DrainHole d;
                    try {
                        const json& p = h.at("point");
                        const json& n = h.at("normal");
                        for (std::size_t k = 0; k < 3; ++k) {
                            d.point[k] = p.at(k).get<double>();
                            d.normal[k] = n.at(k).get<double>();
                        }
                    } catch (const std::exception&) {
                        damaged(where + ": a drain hole's spot is broken");
                    }
                    ho.holes.push_back(d);
                }
            try {
                hollow::check(ho);
            } catch (const std::exception& e) {
                damaged(where + ": " + e.what());
            }
            spec.hollow = std::move(ho);
        }
        if (spec.op == BooleanOp::Smooth) {
            const auto sj = j.find("smooth");
            if (sj == j.end() || !sj->is_object())
                damaged(where + ": missing how strongly to smooth");
            spec.smooth = SmoothFinish{std::clamp(sj->value("strength", 3), 1, 10)};
        }
        if (spec.op == BooleanOp::Simplify) {
            const auto sj = j.find("simplify");
            if (sj == j.end() || !sj->is_object())
                damaged(where + ": missing how much to simplify");
            spec.simplify = SimplifyFinish{std::clamp(sj->value("keep", 0.5), 0.01, 1.0)};
        }
        if (spec.op == BooleanOp::Pattern) {
            const auto pj = j.find("pattern");
            if (pj == j.end() || !pj->is_object())
                damaged(where + ": missing the pattern");
            PatternFinish p;
            const std::string kind = pj->value("kind", std::string("row"));
            p.kind = kind == "grid" ? PatternKind::Grid : kind == "circle" ? PatternKind::Circle : PatternKind::Row;
            p.feature = pj->value("feature", false);
            const auto readVec = [&](const char* key, std::array<double, 3>& out) {
                const auto v = pj->find(key);
                if (v == pj->end() || !v->is_array() || v->size() != 3)
                    damaged(where + ": the pattern is damaged");
                for (std::size_t i = 0; i < 3; ++i)
                    out[i] = (*v)[i].get<double>();
            };
            readVec("dir1", p.dir1);
            readVec("dir2", p.dir2);
            readVec("centre", p.centre);
            readVec("axis", p.axis);
            p.count1 = pj->value("count1", 3);
            p.count2 = pj->value("count2", 2);
            p.count = pj->value("count", 6);
            p.spacing1 = number(*pj, "spacing1", where);
            p.spacing2 = number(*pj, "spacing2", where);
            p.angle = number(*pj, "angle", where);
            spec.pattern = p;
        }
        if (spec.op == BooleanOp::Follow) {
            const auto fj = j.find("follow");
            if (fj == j.end() || !fj->is_object())
                damaged(where + ": missing the path to follow");
            FollowFinish f;
            const auto readVec = [&](const char* key, std::array<double, 3>& out) {
                const auto v = fj->find(key);
                if (v == fj->end() || !v->is_array() || v->size() != 3)
                    damaged(where + ": the path's place is damaged");
                for (std::size_t i = 0; i < 3; ++i)
                    out[i] = (*v)[i].get<double>();
            };
            readVec("origin", f.origin);
            readVec("xAxis", f.xAxis);
            readVec("normal", f.normal);
            if (fj->contains("relative")) {
                readVec("relative", f.relative);
                f.located = true;
            }
            const auto path = fj->find("path");
            if (path == fj->end() || !path->is_object())
                damaged(where + ": missing the path");
            f.path = sketchFromJson(*path, where);
            f.ridge = fj->value("ridge", false);
            const std::string profile = fj->value("profile", std::string("square"));
            f.profile = profile == "round"    ? FollowProfile::Round
                        : profile == "v"      ? FollowProfile::V
                        : profile == "custom" ? FollowProfile::Custom
                                              : FollowProfile::Square;
            f.width = number(*fj, "width", where);
            f.depth = number(*fj, "depth", where);
            if (const auto custom = fj->find("custom"); custom != fj->end() && custom->is_object())
                f.custom = sketchFromJson(*custom, where);
            spec.follow = std::move(f);
        }
        if (spec.op == BooleanOp::Shell) {
            const auto sh = j.find("shell");
            if (sh == j.end() || !sh->is_object())
                damaged(where + ": missing the shell");
            ShellFinish f;
            f.thickness = number(*sh, "thickness", where);
            f.outward = sh->value("outward", false);
            const auto open = sh->find("open");
            if (open != sh->end() && open->is_array())
                for (const auto& p : *open) {
                    FaceRef r;
                    const auto readPoint = [&](const char* key, std::array<double, 3>& out) {
                        const auto v = p.find(key);
                        if (v == p.end() || !v->is_array() || v->size() != 3)
                            damaged(where + ": a picked face is damaged");
                        for (std::size_t i = 0; i < 3; ++i)
                            out[i] = (*v)[i].get<double>();
                    };
                    readPoint("point", r.point);
                    readPoint("normal", r.normal);
                    if (p.contains("relative")) {
                        readPoint("relative", r.relative);
                        r.located = true;
                    }
                    f.open.push_back(r);
                }
            spec.shell = std::move(f);
        }
        if (spec.op == BooleanOp::Emboss && j.contains("picture") && version >= 17) {
            const json& p = j.at("picture");
            if (!p.is_object())
                damaged(where + ": the picture is unreadable");
            EmbossPicture pic;
            pic.name = p.value("name", std::string());
            pic.relief = p.value("relief", false);
            pic.threshold = p.value("threshold", 128);
            pic.invert = p.value("invert", false);
            pic.width = number(p, "width", where);
            pic.depth = number(p, "depth", where);
            pic.raised = p.value("raised", true);
            pic.angle = p.value("angle", 0.0);
            if (const auto im = p.find("image"); im != p.end() && im->is_object())
                pic.image = pictureFromJson(*im, where);
            spec.picture = std::move(pic);
        } else if (spec.op == BooleanOp::Emboss) {
            const auto e = j.find("emboss");
            if (e == j.end() || !e->is_object())
                damaged(where + ": missing emboss words");
            EmbossText words;
            words.text = e->value("text", std::string());
            words.font = e->value("font", std::string());
            words.bold = e->value("bold", true);
            words.italic = e->value("italic", false);
            words.vertical = e->value("vertical", false);
            words.letterHeight = number(*e, "letterHeight", where);
            words.depth = number(*e, "depth", where);
            words.raised = e->value("raised", true);
            words.angle = e->value("angle", 0.0);
            spec.emboss = std::move(words);
        }
        spec.throughAll = j.value("throughAll", false);
        b.boolean = std::move(spec);
        b.suppressed = j.value("suppressed", false);
        b.rolledBack = j.value("rolledBack", false);
        if (b.name.empty())
            b.name = typeName(b) + " " + std::to_string(b.id);
        return b;
    }

    const auto params = j.find("parameters");
    if (params == j.end() || !params->is_object())
        damaged(where + ": missing parameters");
    const auto num = [&](const char* key) { return number(*params, key, where); };
    if (t == "cylinder" && version < 3) {
        b.spec = CylinderSpec{num("radius"), num("height")};  // round
    } else if (t == "mesh" && version >= 9) {
        MeshSpec mesh;
        mesh.width = num("width");
        mesh.depth = num("depth");
        mesh.height = num("height");
        const std::string entry = j.value("data", std::string());
        if (entry.empty())
            damaged(where + ": missing mesh data");
        if (!readEntry)
            damaged(where + ": mesh data not available");
        mesh.data = meshFromBytes(readEntry(entry));
        b.spec = mesh;
    } else if (t == "pad" && version >= 13) {
        PadSpec pad;
        pad.length = number(*params, "length", where);
        pad.reversed = j.value("reversed", false);
        pad.midplane = j.value("midplane", false);
        pad.taper = std::clamp(j.value("taper", 0.0), -60.0, 60.0);
        const auto sk = j.find("sketch");
        if (sk == j.end() || !sk->is_object())
            damaged(where + ": missing sketch");
        pad.sketch = sketchFromJson(*sk, where);
        b.spec = pad;
    } else if (t == "loft" && version >= 23) {
        LoftSpec l;
        l.ruled = j.value("ruled", false);
        const auto sections = j.find("sections");
        if (sections == j.end() || !sections->is_array())
            damaged(where + ": missing the loft's outlines");
        for (const auto& s : *sections) {
            const auto sk = s.find("sketch");
            if (!s.is_object() || sk == s.end() || !sk->is_object())
                damaged(where + ": a loft outline is damaged");
            l.sections.push_back({sketchFromJson(*sk, where), s.value("height", 0.0)});
        }
        b.spec = l;
    } else if (t == "sweep" && version >= 23) {
        SweepSpec w;
        const auto profile = j.find("profile");
        if (profile == j.end() || !profile->is_object())
            damaged(where + ": missing the sweep's outline");
        w.profile = sketchFromJson(*profile, where);
        w.spiral = j.value("spiral", false);
        if (w.spiral) {
            w.pitch = j.value("pitch", 5.0);
            w.turns = j.value("turns", 3.0);
            w.leftHand = j.value("leftHand", false);
        } else {
            const auto path = j.find("path");
            if (path == j.end() || !path->is_object())
                damaged(where + ": missing the sweep's path");
            w.path = sketchFromJson(*path, where);
        }
        b.spec = w;
    } else if (t == "revolve" && version >= 14) {
        RevolveSpec r;
        r.axis = j.value("axis", -1);
        r.angle = j.value("angle", 360.0);
        r.reversed = j.value("reversed", false);
        r.midplane = j.value("midplane", false);
        const auto sk = j.find("sketch");
        if (sk == j.end() || !sk->is_object())
            damaged(where + ": missing sketch");
        r.sketch = sketchFromJson(*sk, where);
        b.spec = r;
    } else if (t == "hole" && version >= 14) {
        HoleSpec h;
        h.diameter = number(*params, "diameter", where);
        h.depth = number(*params, "depth", where);
        const std::string kind = j.value("kind", std::string("plain"));
        h.kind = kind == "counterbored" ? HoleKind::Counterbored
                 : kind == "countersunk" ? HoleKind::Countersunk
                                         : HoleKind::Plain;
        h.size = j.value("size", std::string());
        h.tapped = j.value("tapped", false);
        h.headDiameter = j.value("headDiameter", h.headDiameter);
        h.headDepth = j.value("headDepth", h.headDepth);
        h.countersinkDiameter = j.value("countersinkDiameter", h.countersinkDiameter);
        h.countersinkAngle = j.value("countersinkAngle", h.countersinkAngle);
        const auto sk = j.find("sketch");
        if (sk == j.end() || !sk->is_object())
            damaged(where + ": missing sketch");
        h.sketch = sketchFromJson(*sk, where);
        b.spec = h;
    } else if (version < 6 && t == "sphere") {  // 0.6.0 shapes were always round
        b.spec = SphereSpec{num("radius")};
    } else if (version < 6 && t == "cone") {
        b.spec = ConeSpec{num("radiusBottom"), num("radiusTop"), num("height")};
    } else if (version < 6 && t == "torus") {
        b.spec = TorusSpec{num("majorRadius"), num("minorRadius")};
    } else if (version < 6 && t == "tube") {
        b.spec = TubeSpec{num("outerRadius"), num("innerRadius"), num("height")};
    } else {
        // Find the primitive kind with this file name, then read its fields.
        bool known = false;
        for (std::size_t kind = 0; kind < kPrimitiveKindCount && !known; ++kind) {
            PrimitiveSpec spec = defaultSpec(kind);
            if (fileTypeName(spec) != t)
                continue;
            const auto& fields = specFields(spec);
            for (std::size_t i = 0; i < fields.size(); ++i)
                setSpecValue(spec, i, number(*params, fields[i].key, where));
            if (auto* text = std::get_if<TextSpec>(&spec)) {
                text->text = j.value("text", std::string());
                text->font = j.value("font", std::string());
                text->bold = j.value("bold", false);
                text->italic = j.value("italic", false);
                text->vertical = j.value("vertical", false);
            }
            b.spec = spec;
            known = true;
        }
        if (!known)
            throw FileError("This file contains a shape type this version of Forge3D does not know (\"" + t +
                            "\").\nIt may have been made by a newer version.");
    }

    if (b.name.empty())
        b.name = typeName(b.spec) + " " + std::to_string(b.id);
    return b;
}

std::string readWholeFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw FileError("Could not open the file. It may have been moved, or you may not have permission to read it.");
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The zip entry holding a reference picture's file: "references/front.png".
std::string referenceEntry(const ReferenceImage& r)
{
    std::string suffix;
    for (char c : r.suffix)
        if (std::isalnum(static_cast<unsigned char>(c)) && suffix.size() < 5)
            suffix += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (suffix.empty())
        suffix = "img";
    return std::string("references/") + reference::key(r.view) + "." + suffix;
}

}  // namespace

// ---- JSON -----------------------------------------------------------------------

std::string toJson(const Document& doc, const std::string& createdBy)
{
    json bodies = json::array();
    for (const Body& b : doc.bodies())
        bodies.push_back(bodyToJson(b));
    json root{{"format", kFormatName},
                    {"formatVersion", kFormatVersion},
                    {"createdBy", createdBy},
                    {"units", "mm"},
                    {"bodies", std::move(bodies)}};
    if (const auto& m = doc.mannequin()) {
        json joints = json::object();
        for (int j = 0; j < kMJointCount; ++j)
            joints[mannequin::key(MJoint(j))] = {m->pose[std::size_t(j)][0], m->pose[std::size_t(j)][1]};
        root["mannequin"] = {{"height", m->height}, {"heads", m->heads}, {"x", m->x},      {"y", m->y},
                             {"turn", m->turn},     {"shown", m->shown}, {"joints", joints}};
    }
    if (!doc.poses().empty()) {
        json poses = json::array();
        for (const SavedPose& p : doc.poses()) {
            json joints = json::array();
            for (const auto& [step, a] : p.angles)
                joints.push_back({{"step", step}, {"angles", {a[0], a[1], a[2]}}});
            poses.push_back({{"name", p.name}, {"hold", p.hold}, {"joints", std::move(joints)}});
        }
        root["poses"] = std::move(poses);
    }
    if (!doc.references().empty()) {
        json pictures = json::array();
        for (const ReferenceImage& r : doc.references())
            pictures.push_back({{"view", reference::key(r.view)},
                                {"name", r.name},
                                {"file", referenceEntry(r)},
                                {"pixels", {r.pixelsWide, r.pixelsHigh}},
                                {"mmPerPixel", r.mmPerPixel},
                                {"across", r.across},
                                {"up", r.up},
                                {"turn", r.turn},
                                {"depth", r.depth},
                                {"opacity", r.opacity},
                                {"shown", r.shown},
                                {"active", r.active}});
        root["references"] = std::move(pictures);
    }
    return root.dump(2);
}

std::vector<ReferenceImage> referencesFromJson(const std::string& text,
                                               const std::function<std::string(const std::string&)>& readEntry)
{
    std::vector<ReferenceImage> out;
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error&) {
        return out;
    }
    const auto list = root.is_object() ? root.find("references") : root.end();
    if (!root.is_object() || list == root.end() || !list->is_array() || !readEntry)
        return out;
    // A picture that can't be read is left out: it isn't part of the model, so it's no
    // reason to refuse the file.
    for (const json& j : *list) {
        try {
            if (!j.is_object())
                continue;
            const auto view = reference::fromKey(j.value("view", std::string()));
            if (!view || reference::find(out, *view))
                continue;
            ReferenceImage r;
            r.view = *view;
            r.name = j.value("name", std::string());
            const std::string file = j.value("file", std::string());
            if (file.rfind("references/", 0) != 0)
                continue;
            const auto dot = file.rfind('.');
            r.suffix = dot == std::string::npos ? std::string() : file.substr(dot + 1);
            const json& pixels = j.at("pixels");
            r.pixelsWide = pixels.at(0).get<int>();
            r.pixelsHigh = pixels.at(1).get<int>();
            r.mmPerPixel = j.at("mmPerPixel").get<double>();
            r.across = j.value("across", 0.0);
            r.up = j.value("up", 0.0);
            r.turn = j.value("turn", 0.0);
            r.depth = j.value("depth", 0.0);
            r.opacity = std::clamp(j.value("opacity", 0.6), 0.0, 1.0);
            r.shown = j.value("shown", true);
            r.active = j.value("active", true);
            r.data = std::make_shared<const std::string>(readEntry(file));
            std::vector<ReferenceImage> one{r};
            reference::validate(one);
            out.push_back(std::move(r));
        } catch (const std::exception&) {
            continue;
        }
    }
    return out;
}

std::vector<Body> fromJson(const std::string& text, const std::function<std::string(const std::string&)>& readEntry)
{
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error& e) {
        damaged(std::string("unreadable document.json: ") + e.what());
    }
    if (!root.is_object() || root.value("format", std::string()) != kFormatName)
        damaged("document.json is not a Forge3D document");

    const auto version = root.find("formatVersion");
    if (version == root.end() || !version->is_number_integer())
        damaged("missing format version");
    const int fileVersion = version->get<int>();
    if (fileVersion < 1)
        damaged("invalid format version");
    if (fileVersion > kFormatVersion)
        throw FileError("This file was made by a newer version of Forge3D (file format " +
                        std::to_string(version->get<int>()) + ").\nPlease update Forge3D to open it.");

    if (root.value("units", std::string("mm")) != "mm")
        damaged("unsupported units");

    // No number in a Forge3D document is near 10 km (10,000,000 mm), or not a number at all:
    // such a value is damage, and building shapes from it can take for ever.
    std::function<void(const json&)> sensible = [&sensible](const json& j) {
        if (j.is_number()) {
            const double v = j.get<double>();
            if (!std::isfinite(v) || std::abs(v) > 1e7)
                damaged("a number is far too big");
        } else if (j.is_structured()) {
            for (const auto& v : j)
                sensible(v);
        }
    };
    sensible(root);

    const auto bodies = root.find("bodies");
    if (bodies == root.end() || !bodies->is_array())
        damaged("missing list of bodies");

    std::vector<Body> result;
    result.reserve(bodies->size());
    for (std::size_t i = 0; i < bodies->size(); ++i)
        result.push_back(bodyFromJson((*bodies)[i], i, fileVersion, readEntry));
    return result;
}

// ---- Saved results ------------------------------------------------------------------

namespace {

// Bump when a step's result is worked out differently, so old saved results aren't used.
constexpr int kResultsVersion = 2;  // 2: sculpting numbers points in space order (0.53)
constexpr char kResultMagic[8] = {'F', '3', 'D', 'R', 'E', 'S', 'U', '1'};

void collect(const Document& doc, ObjectId id, std::vector<ObjectId>& out)
{
    const Body* b = doc.find(id);
    if (!b || std::find(out.begin(), out.end(), id) != out.end())
        return;
    out.push_back(id);
    if (b->boolean)
        for (ObjectId part : b->boolean->operands)
            collect(doc, part, out);
}

// Top-level shapes whose result is the mesh engine's, worth keeping in the file.
bool keepsResult(const Document& doc, const Body& b)
{
    return b.isBoolean() && doc.isTopLevel(b.id) && doc.usesMeshEngine(b.id);
}

}  // namespace

std::string resultKey(const Document& doc, ObjectId id)
{
    std::vector<ObjectId> all;
    collect(doc, id, all);
    std::sort(all.begin(), all.end());
    std::string text = "v" + std::to_string(kResultsVersion) + "/" + std::to_string(id);
    for (ObjectId part : all) {
        json j = bodyToJson(*doc.find(part));
        j.erase("name");  // renaming doesn't change the shape
        j.erase("colour");
        j.erase("hardware");  // nor does marking it a joint part
        if (part == id)
            j.erase("placement");  // nor does moving the whole thing
        text += "|" + j.dump();
    }
    std::uint64_t h = 1469598103934665603ULL;  // FNV-1a
    for (unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h));
    return std::string(hex) + "-" + std::to_string(text.size());
}

std::size_t useSavedResults(const Document& doc, const std::filesystem::path& path)
{
    std::string bytes;
    try {
        bytes = readWholeFile(path);
    } catch (const std::exception&) {
        return 0;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        return 0;
    std::size_t used = 0;
    for (const Body& b : doc.bodies()) {
        if (!keepsResult(doc, b) || doc.cachedSolid(b.id))
            continue;
        const std::string name = "results/" + std::to_string(b.id) + ".bin";
        size_t size = 0;
        void* data = mz_zip_reader_extract_file_to_heap(&zip, name.c_str(), &size, 0);
        if (!data)
            continue;
        const std::string entry(static_cast<const char*>(data), size);
        mz_free(data);
        try {
            if (entry.size() < sizeof kResultMagic + 4 || entry.compare(0, sizeof kResultMagic, kResultMagic,
                                                                        sizeof kResultMagic) != 0)
                continue;
            std::uint32_t keyLength = 0;
            std::memcpy(&keyLength, entry.data() + sizeof kResultMagic, 4);
            const std::size_t at = sizeof kResultMagic + 4;
            if (keyLength > 200 || entry.size() < at + keyLength)
                continue;
            if (entry.compare(at, keyLength, resultKey(doc, b.id)) != 0)
                continue;  // made from something else: work it out again
            const auto mesh = meshFromBytes(entry.substr(at + keyLength));
            doc.seedSolid(b.id, meshkernel::fromMeshData(*mesh, {1.0, 1.0, 1.0}));
            ++used;
        } catch (const std::exception&) {
            // Damaged: it's worked out again when it's needed.
        }
    }
    mz_zip_reader_end(&zip);
    return used;
}

// ---- Zip container --------------------------------------------------------------

void save(const Document& doc, const std::filesystem::path& path, const std::string& createdBy)
{
    const std::string jsonText = toJson(doc, createdBy);

    // Build the zip in memory, so file access stays in C++ (wide paths on Windows).
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 64 * 1024))
        throw FileError("Could not prepare the file for saving (out of memory?).");
    void* buffer = nullptr;
    size_t size = 0;
    bool ok = mz_zip_writer_add_mem(&zip, kDocumentEntry, jsonText.data(), jsonText.size(),
                                    static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION));
    for (const Body& b : doc.bodies()) {
        if (!ok)
            break;
        if (const auto* mesh = std::get_if<MeshSpec>(&b.spec); mesh && mesh->data) {
            const std::string name = "meshes/" + std::to_string(b.id) + ".bin";
            const std::string bytes = meshToBytes(*mesh->data);
            ok = mz_zip_writer_add_mem(&zip, name.c_str(), bytes.data(), bytes.size(),
                                       static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION));
        }
    }
    // Reference pictures, as they were read (already squeezed: stored as they are).
    for (const ReferenceImage& r : doc.references()) {
        if (!ok)
            break;
        const std::string name = referenceEntry(r);
        ok = mz_zip_writer_add_mem(&zip, name.c_str(), r.data->data(), r.data->size(),
                                   static_cast<mz_uint>(MZ_NO_COMPRESSION));
    }
    // The finished results of the slow steps, so opening the file is quick.
    for (const Body& b : doc.bodies()) {
        if (!ok)
            break;
        if (!keepsResult(doc, b))
            continue;
        const auto solid = doc.cachedSolid(b.id);
        if (!solid)
            continue;  // not worked out yet: nothing to save time on
        {
            // A step that couldn't be built should say so again when the file is opened.
            std::vector<ObjectId> all;
            collect(doc, b.id, all);
            if (std::any_of(all.begin(), all.end(), [&doc](ObjectId id) { return doc.knownProblem(id); }))
                continue;
        }
        MeshData mesh;
        meshkernel::triangles(*solid, mesh.positions, mesh.indices);
        Bounds box;
        for (std::size_t i = 0; i + 2 < mesh.positions.size(); i += 3)
            box.expand(mesh.positions[i], mesh.positions[i + 1], mesh.positions[i + 2]);
        mesh.size = box.size();
        const std::string key = resultKey(doc, b.id);
        std::string entry(kResultMagic, sizeof kResultMagic);
        const auto keyLength = std::uint32_t(key.size());
        entry.append(reinterpret_cast<const char*>(&keyLength), 4);
        entry += key;
        entry += meshToBytes(mesh);
        const std::string name = "results/" + std::to_string(b.id) + ".bin";
        ok = mz_zip_writer_add_mem(&zip, name.c_str(), entry.data(), entry.size(),
                                   static_cast<mz_uint>(MZ_BEST_SPEED));
    }
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    if (!ok) {
        mz_zip_writer_end(&zip);
        throw FileError("Could not prepare the file for saving.");
    }

    // Write next to the target, then swap it in.
    std::filesystem::path temp = path;
    temp += ".saving";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
        out.close();
        mz_zip_writer_end(&zip);  // frees buffer
        if (!out) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            throw FileError("Could not write the file. Check that the folder exists, that you have "
                            "permission to save there, and that the disk is not full.");
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        throw FileError("Could not replace the existing file. It may be open in another program.");
    }
}

std::vector<Body> load(const std::filesystem::path& path)
{
    const std::string bytes = readWholeFile(path);

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        damaged("not a zip archive");

    size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, kDocumentEntry, &size, 0);
    if (!data) {
        mz_zip_reader_end(&zip);
        damaged("no document.json inside");
    }
    std::string jsonText(static_cast<const char*>(data), size);
    mz_free(data);

    const auto readEntry = [&zip](const std::string& name) {
        size_t entrySize = 0;
        void* entry = mz_zip_reader_extract_file_to_heap(&zip, name.c_str(), &entrySize, 0);
        if (!entry)
            damaged("missing " + name);
        std::string out(static_cast<const char*>(entry), entrySize);
        mz_free(entry);
        return out;
    };
    try {
        std::vector<Body> bodies = fromJson(jsonText, readEntry);
        mz_zip_reader_end(&zip);
        return bodies;
    } catch (...) {
        mz_zip_reader_end(&zip);
        throw;
    }
}

std::vector<SavedPose> posesFromJson(const std::string& text)
{
    std::vector<SavedPose> out;
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error&) {
        return out;
    }
    if (!root.is_object())
        return out;
    const auto list = root.find("poses");
    if (list == root.end() || !list->is_array())
        return out;
    for (const json& j : *list) {
        try {
            SavedPose p;
            p.name = j.at("name").get<std::string>();
            p.hold = j.value("hold", kInvalidId);
            for (const json& a : j.at("joints")) {
                const json& v = a.at("angles");
                p.angles[a.at("step").get<ObjectId>()] = {v.at(0).get<double>(), v.at(1).get<double>(),
                                                          v.at(2).get<double>()};
            }
            if (p.name.empty() || std::any_of(out.begin(), out.end(), [&p](const SavedPose& q) { return q.name == p.name; }))
                continue;
            out.push_back(std::move(p));
        } catch (const std::exception&) {
            continue;
        }
    }
    return out;
}

std::optional<Mannequin> mannequinFromJson(const std::string& text)
{
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error&) {
        return std::nullopt;
    }
    if (!root.is_object())
        return std::nullopt;
    const auto j = root.find("mannequin");
    if (j == root.end() || !j->is_object())
        return std::nullopt;
    try {
        Mannequin m;
        m.height = j->value("height", m.height);
        m.heads = j->value("heads", m.heads);
        m.x = j->value("x", 0.0);
        m.y = j->value("y", 0.0);
        m.turn = j->value("turn", 0.0);
        m.shown = j->value("shown", true);
        if (const auto joints = j->find("joints"); joints != j->end() && joints->is_object())
            for (const auto& [name, a] : joints->items())
                if (const auto joint = mannequin::fromKey(name))
                    m.pose[std::size_t(*joint)] = {a.at(0).get<double>(), a.at(1).get<double>()};
        mannequin::validate(m);
        return m;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<Mannequin> loadMannequin(const std::filesystem::path& path)
{
    std::string bytes;
    try {
        bytes = readWholeFile(path);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        return std::nullopt;
    std::optional<Mannequin> out;
    size_t size = 0;
    if (void* data = mz_zip_reader_extract_file_to_heap(&zip, kDocumentEntry, &size, 0)) {
        out = mannequinFromJson(std::string(static_cast<const char*>(data), size));
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    return out;
}

std::vector<SavedPose> loadPoses(const std::filesystem::path& path)
{
    std::string bytes;
    try {
        bytes = readWholeFile(path);
    } catch (const std::exception&) {
        return {};
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        return {};
    std::vector<SavedPose> out;
    size_t size = 0;
    if (void* data = mz_zip_reader_extract_file_to_heap(&zip, kDocumentEntry, &size, 0)) {
        out = posesFromJson(std::string(static_cast<const char*>(data), size));
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    return out;
}

std::vector<ReferenceImage> loadReferences(const std::filesystem::path& path)
{
    std::string bytes;
    try {
        bytes = readWholeFile(path);
    } catch (const std::exception&) {
        return {};
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0))
        return {};
    std::vector<ReferenceImage> out;
    size_t size = 0;
    if (void* data = mz_zip_reader_extract_file_to_heap(&zip, kDocumentEntry, &size, 0)) {
        const std::string jsonText(static_cast<const char*>(data), size);
        mz_free(data);
        out = referencesFromJson(jsonText, [&zip](const std::string& name) {
            size_t entrySize = 0;
            void* entry = mz_zip_reader_extract_file_to_heap(&zip, name.c_str(), &entrySize, 0);
            if (!entry)
                throw FileError("missing " + name);
            std::string got(static_cast<const char*>(entry), entrySize);
            mz_free(entry);
            return got;
        });
    }
    mz_zip_reader_end(&zip);
    return out;
}

namespace {
constexpr char kMeshMagic[8] = {'F', '3', 'D', 'M', 'E', 'S', 'H', '1'};
}

std::string meshToBytes(const MeshData& mesh)
{
    std::string out(kMeshMagic, sizeof kMeshMagic);
    const auto put = [&out](const void* p, std::size_t n) { out.append(static_cast<const char*>(p), n); };
    const std::uint32_t points = static_cast<std::uint32_t>(mesh.positions.size() / 3);
    const std::uint32_t tris = static_cast<std::uint32_t>(mesh.indices.size() / 3);
    const std::uint8_t closed = mesh.closed ? 1 : 0;
    put(&points, 4);
    put(&tris, 4);
    put(mesh.size.data(), 12);
    put(&closed, 1);
    put(mesh.positions.data(), mesh.positions.size() * sizeof(float));
    put(mesh.indices.data(), mesh.indices.size() * sizeof(std::uint32_t));
    return out;
}

std::shared_ptr<const MeshData> meshFromBytes(const std::string& bytes)
{
    std::size_t at = 0;
    const auto take = [&](void* p, std::size_t n) {
        if (at + n > bytes.size())
            damaged("mesh data is cut short");
        std::memcpy(p, bytes.data() + at, n);
        at += n;
    };
    char magic[8];
    take(magic, 8);
    if (std::memcmp(magic, kMeshMagic, 8) != 0)
        damaged("mesh data is not in a known format");
    auto mesh = std::make_shared<MeshData>();
    std::uint32_t points = 0, tris = 0;
    std::uint8_t closed = 1;
    take(&points, 4);
    take(&tris, 4);
    take(mesh->size.data(), 12);
    take(&closed, 1);
    mesh->closed = closed != 0;
    mesh->positions.resize(std::size_t(points) * 3);
    mesh->indices.resize(std::size_t(tris) * 3);
    take(mesh->positions.data(), mesh->positions.size() * sizeof(float));
    take(mesh->indices.data(), mesh->indices.size() * sizeof(std::uint32_t));
    for (std::uint32_t i : mesh->indices)
        if (i >= points)
            damaged("mesh data refers to a missing point");
    return mesh;
}

}  // namespace forge::file
