// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "forge/core/Body.h"
#include "forge/core/Mesh.h"
#include "forge/core/MeshKernel.h"
#include "forge/core/Split.h"
#include "forge/core/Transform.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace forge {

class CommandStack;
class Document;

// Split by Mask (slice 48).
namespace mask {

// Whether points are masked, quickly: the dabs filed in a grid as big as the largest.
class Field {
public:
    explicit Field(const std::vector<MaskDab>& dabs);
    bool masked(const Vec3& p) const;
    // Whether the edge of some dab's ball passes within `slack` of p (slack no more than
    // the largest dab's radius).
    bool nearEdge(const Vec3& p, double slack) const;
    double smallest() const { return m_smallest; }
    double cellSize() const { return m_cell; }
    bool empty() const { return m_dabs.empty(); }

private:
    std::vector<MaskDab> m_dabs;
    double m_cell = 1.0;
    double m_smallest = 1.0;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> m_cells;
    std::uint64_t key(std::int64_t i, std::int64_t j, std::int64_t k) const;
};

// Where an alignment key can go on a cap: its middle (the point furthest in), the way
// out of the masked piece into the rest there, a direction across the cap (the key's
// point aims along it), and how far it is to the cap's edge.
struct KeySpot {
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 0, 1};
    Vec3 across{1, 0, 0};
    double room = 0.0;
};

// The two pieces of a closed triangle surface cut along the mask's edge, each closed
// with the same smooth cap (facing opposite ways), so they fit back together.
struct Pieces {
    Mesh masked, rest;  // positions and indices only
    int caps = 0;       // how many cut edges (loops) were capped
    std::vector<KeySpot> keySpots;  // one per cap
};
// Throws std::invalid_argument (in plain words) if either piece would be empty.
Pieces split(const std::vector<float>& positions, const std::vector<std::uint32_t>& indices,
             const std::vector<MaskDab>& dabs);

// "the masked part (240 dabs)" / "the rest"
std::string describe(const MaskSplitFinish& f);

// A key's size for a cap with this much room: its point's distance from its middle
// (0: too small for one). And how far it reaches into the other piece.
double keySize(double room);
double keyLength(double size);
// The piece with its keys: pegs added to the masked piece, holes (bigger by `fit` all
// round) cut in the rest. Caps too small for one are left as they are.
MeshSolid keyed(const Mesh& piece, const std::vector<KeySpot>& spots, bool masked, double fit);

}  // namespace mask

// The Split by Mask step for `target` (dabs in plate coordinates), keeping the masked
// piece or the rest. Just its recipe.
// `keyFit` < 0: no alignment keys.
BooleanSpec maskSplitSpec(const Document& doc, ObjectId target, const std::vector<MaskDab>& dabs, bool masked,
                          double keyFit = -1.0);

// Cuts shape `id` (on the plate) along the mask as one undo step: Both gives two
// shapes ("Arm (masked)" and "Arm (rest)", the second made from a copy), Front keeps the
// masked piece, Back the rest. Throws (changing nothing) if the mask leaves either
// piece empty.
std::vector<ObjectId> splitByMask(CommandStack& stack, Document& doc, ObjectId id, const std::vector<MaskDab>& dabs,
                                  SplitKeep keep, double keyFit = -1.0);

// A Split by Mask step's dabs, in plate coordinates.
std::vector<MaskDab> maskDabs(const Document& doc, ObjectId result);
// The step with new dabs (plate coordinates), and its other piece likewise if it's still
// on the plate with the same mask (so they keep fitting together). The caller records
// them (EditBodiesCommand).
// `keyFit`: as for maskSplitSpec; unset keeps the keys as they are.
std::vector<Body> withMask(const Document& doc, ObjectId result, const std::vector<MaskDab>& dabs,
                           std::optional<double> keyFit = std::nullopt);

}  // namespace forge
