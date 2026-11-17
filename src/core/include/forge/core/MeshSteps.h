// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"
#include "forge/core/Transform.h"

#include <vector>

// Whole-shape steps worked out on triangles (slice 40): Smooth and Extrude Down.
namespace forge::meshsteps {

// The surface smoothed all over, 1 (gentle) .. 10 (strong: edges and corners rounded
// about a sixth of the shape's size in): first evened out into small triangles (unless
// it already has very many), then each point moved towards its neighbours, then grown
// back to the volume it had. Open edges stay where they are.
Mesh smoothed(const Mesh& closed, int strength);

// What Extrude Down adds: the space straight below every part of the surface that
// faces down (against `up`) and is above the plane through `planePoint`, down to the
// plane (and a hair past it), as closed solids: one per connected patch of such
// triangles. Merge them all with the shape. Empty if nothing hangs above the plane.
// The surface must share its corners between triangles (as the mesh engine's does).
std::vector<Mesh> columnsBelow(const Mesh& closed, const Vec3& planePoint, const Vec3& up);

}  // namespace forge::meshsteps
