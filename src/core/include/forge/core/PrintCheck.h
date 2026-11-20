// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <cstddef>
#include <limits>
#include <vector>

// Print Check: where a part is too thin to print well (fingers, cape edges, weapon
// tips), and which faces hang over the air the way it sits on the plate (they'll need
// supports). Nothing is changed: the answers are shown as colours.
namespace forge::printcheck {

// The thickest wall the check can be asked about (mm). Thickness is only measured up to
// this, so changing the setting doesn't mean measuring again.
constexpr double kMaxWall = 5.0;
constexpr float kNoWall = std::numeric_limits<float>::infinity();

// For each triangle: how thick the part is straight in from the middle of it (mm), up
// to `upTo`; thicker (or nothing on the other side, as in an open mesh) is kNoWall.
// Uses every core of the machine; a million triangles takes about a second.
std::vector<float> faceThickness(const Mesh& mesh, double upTo = kMaxWall);

struct Settings {
    bool thin = true;
    double minWall = 0.8;  // mm: thinner than this is shown
    bool overhangs = true;
    double angle = 45.0;   // degrees from upright: faces leaning out further are shown
};

// What a triangle is, for the colours (Mesh::shade of the shown mesh).
constexpr float kShadeOk = 0.0f;
constexpr float kShadeOverhang = 0.5f;
constexpr float kShadeThin = 1.0f;

struct Report {
    double area = 0.0;          // mm² of the whole surface
    double thinArea = 0.0;      // mm² thinner than the setting
    double overhangArea = 0.0;  // mm² leaning out past the angle
    double thinnest = kNoWall;  // mm, the thinnest place found (kNoWall: none under kMaxWall)
};

// Sorts the triangles with the settings. `place` puts the mesh on the plate (it decides
// which way is down); the faces lying on the part's bottom aren't overhangs. If `shown`
// isn't null it gets the mesh with each triangle's own corners (the mesh's
// coordinates) and its shade set to kShadeOk, kShadeOverhang or kShadeThin (thin wins).
// `thickness` comes from faceThickness() (empty: thin walls aren't checked).
Report classify(const Mesh& mesh, const std::vector<float>& thickness, const Transform& place, const Settings& s,
                Mesh* shown = nullptr);

}  // namespace forge::printcheck
