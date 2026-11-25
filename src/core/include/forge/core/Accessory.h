// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"

#include <string>
#include <vector>

// Accessory makers (slice 100): a blade, a shield, a staff or spear, a belt with a buckle
// and a holster, sized from life at the figure's scale, with handles that fit the hands'
// grip holes. Made as smooth closed surfaces, lying ready to print.
namespace forge {

class CommandStack;
class Document;

enum class AccessoryKind { Blade, Shield, Staff, Belt, Holster };
inline constexpr int kAccessoryKindCount = 5;
enum class BladeTip { Point, Round, Angled };
enum class ShieldShape { Round, Heater, Kite };
enum class StaffEnd { Plain, Spear, Orb };

// Sizes marked "life" are a person's (mm), divided by the scale when made; the others are
// as printed (mm).
struct AccessorySettings {
    AccessoryKind kind = AccessoryKind::Blade;
    double scale = 12.0;   // 1 : scale
    double handle = 3.0;   // printed: the handle's diameter (the hands' grip holes take it)

    // Blade (life)
    double bladeLength = 750.0;
    double bladeWidth = 45.0;
    double bladeThickness = 6.0;
    double guardWidth = 180.0;
    double gripLength = 180.0;
    BladeTip tip = BladeTip::Point;
    bool pommel = true;

    // Shield (life)
    ShieldShape shield = ShieldShape::Heater;
    double shieldWidth = 600.0;
    double shieldHeight = 750.0;
    double shieldThickness = 15.0;
    double shieldCurve = 0.3;  // 0 flat .. 1 bowed strongly
    bool boss = true;
    bool gripBar = true;       // a bar on the back the hand's grip hole takes

    // Staff or spear (life)
    double staffLength = 1700.0;
    StaffEnd staffEnd = StaffEnd::Plain;

    // Belt: round a waist this size as printed, the band in life sizes
    double waistWidth = 30.0;   // printed: across the waist, left to right
    double waistDepth = 20.0;   // printed: front to back
    double beltWidth = 40.0;    // life
    double fit = 0.2;           // printed: room all round the waist
    bool beltOpen = true;       // a gap at the back to clip it on
    double beltGap = 3.0;       // printed

    // Holster (life), with a loop on the back for a belt as set above
    double holsterLength = 250.0;
    double holsterWidth = 90.0;
    double holsterDepth = 45.0;

    bool operator==(const AccessorySettings&) const = default;
};

namespace accessory {

// "Blade", "Shield"...
const char* title(AccessoryKind kind);
// The thinnest a part is made, printed (mm): thinner would break or not print.
inline constexpr double kThinnest = 0.8;
// Throws std::invalid_argument (in plain words) if it can't be made.
void check(const AccessorySettings& s);
// The accessory: a closed surface lying ready to print (its lowest point on Z = 0, centred
// on X and Y).
Mesh make(const AccessorySettings& s);
// "Sword", "Dagger", "Heater shield", "Spear", "Belt", "Holster"...
std::string nameOf(const AccessorySettings& s);
// Adds the accessory at (x, y) on the plate as one undo step. Returns it.
ObjectId add(CommandStack& stack, Document& doc, const AccessorySettings& s, double x, double y);

}  // namespace accessory

}  // namespace forge
