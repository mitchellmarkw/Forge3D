// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Tolerance.h"

#include <optional>

// The fits found with the tolerance test print, per printer and kind of joint. They're
// where Quick Socket and Quick Hinge start when a printer is chosen.
namespace forge::app::fits {

std::optional<double> measured(bool resin, tolerance::Kind kind);
void setMeasured(bool resin, tolerance::Kind kind, std::optional<double> fit);
// The measured fit, or the usual one (0.1 mm resin, 0.2 mm filament).
double starting(bool resin, tolerance::Kind kind);

}  // namespace forge::app::fits
