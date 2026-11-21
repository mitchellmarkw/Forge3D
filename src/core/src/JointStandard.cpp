// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/JointStandard.h"

#include "forge/core/Hinge.h"
#include "forge/core/Socket.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>

namespace forge::standards {

using json = nlohmann::json;

namespace {

constexpr const char* kFormat = "forge3d-joint-standards";
constexpr int kVersion = 1;

const char* const kSocketKinds[] = {"ball", "peg", "magnet"};
const char* const kPegShapes[] = {"round", "square", "hexagon", "octagon"};
const char* const kHingeKinds[] = {"single", "double", "swivel"};
const char* const kPins[] = {"printed", "filament", "rod"};
const char* const kSwings[] = {"both", "forward", "back"};

template <std::size_t N>
int indexOf(const char* const (&names)[N], const std::string& name, const char* what)
{
    for (std::size_t i = 0; i < N; ++i)
        if (name == names[i])
            return int(i);
    throw std::invalid_argument(std::string("unknown ") + what + " '" + name + "'");
}

json socketJson(const SocketFinish& f)
{
    return json{{"kind", kSocketKinds[int(f.kind)]},
                {"size", f.size},
                {"depth", f.depth},
                {"snap", f.snap},
                {"grip", f.grip},
                {"shape", kPegShapes[int(f.shape)]},
                {"reliefCuts", f.reliefCuts},
                {"reliefWidth", f.reliefWidth},
                {"reliefDepth", f.reliefDepth},
                {"ridges", f.ridges},
                {"ridgeHeight", f.ridgeHeight},
                {"cover", f.cover}};
}

SocketFinish socketFrom(const json& j)
{
    SocketFinish f;
    f.kind = SocketKind(indexOf(kSocketKinds, j.at("kind").get<std::string>(), "socket"));
    f.size = j.at("size").get<double>();
    f.depth = j.value("depth", f.depth);
    f.snap = j.value("snap", f.snap);
    f.grip = j.value("grip", f.grip);
    f.shape = PegShape(indexOf(kPegShapes, j.value("shape", std::string("round")), "peg shape"));
    f.reliefCuts = j.value("reliefCuts", f.reliefCuts);
    f.reliefWidth = j.value("reliefWidth", f.reliefWidth);
    f.reliefDepth = j.value("reliefDepth", f.reliefDepth);
    f.ridges = j.value("ridges", f.ridges);
    f.ridgeHeight = j.value("ridgeHeight", f.ridgeHeight);
    f.cover = j.value("cover", f.cover);
    return f;
}

json hingeJson(const HingeFinish& f)
{
    return json{{"kind", kHingeKinds[int(f.kind)]},
                {"width", f.width},
                {"barrel", f.barrel},
                {"pin", kPins[int(f.pin)]},
                {"pinSize", f.pinSize},
                {"stiff", f.stiff},
                {"swing", kSwings[int(f.swing)]},
                {"reach", f.reach},
                {"gap", f.gap},
                {"lip", f.lip},
                {"clicks", f.clicks},
                {"clickDepth", f.clickDepth}};
}

HingeFinish hingeFrom(const json& j)
{
    HingeFinish f;
    f.kind = HingeKind(indexOf(kHingeKinds, j.at("kind").get<std::string>(), "hinge"));
    f.width = j.at("width").get<double>();
    f.barrel = j.at("barrel").get<double>();
    f.pin = PinKind(indexOf(kPins, j.value("pin", std::string("printed")), "pin"));
    f.pinSize = j.value("pinSize", f.pinSize);
    f.stiff = j.value("stiff", f.stiff);
    f.swing = HingeSwing(indexOf(kSwings, j.value("swing", std::string("both")), "swing"));
    f.reach = j.value("reach", f.reach);
    f.gap = j.value("gap", f.gap);
    f.lip = j.value("lip", f.lip);
    f.clicks = j.value("clicks", f.clicks);
    f.clickDepth = j.value("clickDepth", f.clickDepth);
    return f;
}

}  // namespace

void check(const Joint& j)
{
    if (j.name.empty())
        throw std::invalid_argument("A joint needs a name.");
    if (bool(j.socket) == bool(j.hinge))
        throw std::invalid_argument("The joint '" + j.name + "' must be a socket or a hinge.");
    try {
        if (j.socket)
            socket::check(*j.socket);
        else
            hinge::check(*j.hinge);
    } catch (const std::exception& e) {
        throw std::invalid_argument("The joint '" + j.name + "': " + e.what());
    }
}

void check(const Standard& s)
{
    if (s.name.empty())
        throw std::invalid_argument("A joint standard needs a name.");
    std::set<std::string> names;
    for (const Joint& j : s.joints) {
        check(j);
        if (!names.insert(j.name).second)
            throw std::invalid_argument("'" + s.name + "' has two joints called '" + j.name + "'.");
    }
}

std::string describe(const Joint& j)
{
    if (j.socket)
        return socket::describe(*j.socket);
    if (j.hinge)
        return hinge::describe(*j.hinge);
    return {};
}

SocketFinish applied(const SocketFinish& joint, const SocketFinish& now)
{
    SocketFinish out = joint;
    out.fit = now.fit;
    out.resin = now.resin;
    out.turn = now.turn;
    return out;
}

HingeFinish applied(const HingeFinish& joint, const HingeFinish& now)
{
    HingeFinish out = joint;
    out.half = now.half;
    out.fit = now.fit;
    out.resin = now.resin;
    out.turn = now.turn;
    out.link = now.link;
    return out;
}

Joint jointFrom(const std::string& name, const SocketFinish& f)
{
    Joint j;
    j.name = name;
    j.socket = applied(f, SocketFinish{});
    return j;
}

Joint jointFrom(const std::string& name, const HingeFinish& f)
{
    Joint j;
    j.name = name;
    j.hinge = applied(f, HingeFinish{});
    return j;
}

tolerance::Settings toleranceSizes(const Standard& s, tolerance::Settings now)
{
    bool ball = false, peg = false, pin = false;
    for (const Joint& j : s.joints) {
        if (j.socket && j.socket->kind == SocketKind::Ball && !ball) {
            now.ballSize = j.socket->size;
            if (j.socket->snap)
                now.grip = j.socket->grip;
            ball = true;
        } else if (j.socket && j.socket->kind == SocketKind::Peg && j.socket->shape == PegShape::Round && !peg) {
            now.pegSize = j.socket->size;
            now.pegDepth = j.socket->depth;
            peg = true;
        } else if (j.hinge && j.hinge->kind != HingeKind::Swivel && j.hinge->pin == PinKind::Printed && !pin) {
            now.pinSize = j.hinge->pinSize;
            pin = true;
        }
    }
    return now;
}

std::string toJson(const std::vector<Standard>& standards)
{
    json list = json::array();
    for (const Standard& s : standards) {
        json joints = json::array();
        for (const Joint& j : s.joints) {
            json item{{"name", j.name}};
            if (j.socket)
                item["socket"] = socketJson(*j.socket);
            if (j.hinge)
                item["hinge"] = hingeJson(*j.hinge);
            joints.push_back(std::move(item));
        }
        list.push_back(json{{"name", s.name}, {"notes", s.notes}, {"joints", std::move(joints)}});
    }
    return json{{"format", kFormat}, {"version", kVersion}, {"standards", std::move(list)}}.dump(2);
}

std::vector<Standard> fromJson(const std::string& text)
{
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception&) {
        throw std::invalid_argument("This isn't a joint standards file (it can't be read).");
    }
    if (!root.is_object() || root.value("format", std::string()) != kFormat)
        throw std::invalid_argument("This isn't a joint standards file.");
    if (root.value("version", 0) > kVersion)
        throw std::invalid_argument("These joint standards were made by a newer Forge3D. Please update it.");
    std::vector<Standard> out;
    try {
        for (const json& sj : root.at("standards")) {
            Standard s;
            s.name = sj.at("name").get<std::string>();
            s.notes = sj.value("notes", std::string());
            for (const json& jj : sj.at("joints")) {
                Joint j;
                j.name = jj.at("name").get<std::string>();
                if (const auto so = jj.find("socket"); so != jj.end())
                    j.socket = socketFrom(*so);
                if (const auto hi = jj.find("hinge"); hi != jj.end())
                    j.hinge = hingeFrom(*hi);
                s.joints.push_back(std::move(j));
            }
            check(s);
            out.push_back(std::move(s));
        }
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("The joint standards file is damaged: ") + e.what());
    } catch (const std::exception&) {
        throw std::invalid_argument("The joint standards file is damaged.");
    }
    return out;
}

std::string uniqueName(const std::vector<Standard>& standards, const std::string& base)
{
    const auto taken = [&](const std::string& n) {
        return std::any_of(standards.begin(), standards.end(), [&](const Standard& s) { return s.name == n; });
    };
    if (!taken(base))
        return base;
    for (int i = 2;; ++i) {
        const std::string n = base + " " + std::to_string(i);
        if (!taken(n))
            return n;
    }
}

}  // namespace forge::standards
