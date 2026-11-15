// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Mesh.h"

#include <cstddef>

// Evening out and thinning triangle meshes (slice 23).
namespace forge::remesh {

// The same surface with triangles of about `edge` across everywhere: long edges split,
// short ones collapsed, edges flipped so each point has about six neighbours, points
// slid along the surface to even the spacing, and everything kept on the original
// surface. For closed meshes with shared corners (as sculpting uses); open edges are
// kept as they are. A mask (shade) is carried over. Stops growing at maxTriangles.
Mesh remeshed(const Mesh& mesh, double edge, int rounds = 4, std::size_t maxTriangles = 4'000'000);

// The same surface with about `triangles` triangles: edges collapsed, cheapest first,
// where the shape changes least (quadric error). Open edges and the corners on them
// stay; collapses that would fold a triangle over or join the surface to itself are
// skipped, so a closed mesh stays closed. Corners are joined first (any within a
// millionth of the size), so a tessellated solid can be given as it is.
Mesh simplified(const Mesh& mesh, std::size_t triangles);

}  // namespace forge::remesh
