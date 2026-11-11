// SPDX-License-Identifier: GPL-3.0-or-later
#include "forge/core/Tessellator.h"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace forge {

Mesh tessellate(const TopoDS_Shape& shape, const TessellationSettings& settings)
{
    Mesh mesh;
    if (shape.IsNull())
        return mesh;

    // Builds (or reuses) a triangulation stored on each face of the shape.
    BRepMesh_IncrementalMesh mesher(shape, settings.linearDeflection, settings.relative,
                                    settings.angularDeflection, /*isInParallel=*/true);

    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        const TopoDS_Face& face = TopoDS::Face(exp.Current());
        TopLoc_Location location;
        const auto& tri = BRep_Tool::Triangulation(face, location);
        if (tri.IsNull())
            continue;

        const gp_Trsf transform = location.Transformation();
        const bool reversed = (face.Orientation() == TopAbs_REVERSED);
        const auto base = static_cast<std::uint32_t>(mesh.vertexCount());
        const int nodeCount = tri->NbNodes();

        // Vertices of this face (each face gets its own copy, which keeps edges sharp).
        std::vector<gp_Pnt> nodes;
        nodes.reserve(static_cast<std::size_t>(nodeCount));
        for (int i = 1; i <= nodeCount; ++i) {
            gp_Pnt p = tri->Node(i);
            p.Transform(transform);
            nodes.push_back(p);
            mesh.positions.push_back(static_cast<float>(p.X()));
            mesh.positions.push_back(static_cast<float>(p.Y()));
            mesh.positions.push_back(static_cast<float>(p.Z()));
        }

        // Accumulate area-weighted triangle normals per vertex.
        std::vector<gp_Vec> normals(static_cast<std::size_t>(nodeCount), gp_Vec(0.0, 0.0, 0.0));
        for (int t = 1; t <= tri->NbTriangles(); ++t) {
            int n1 = 0, n2 = 0, n3 = 0;
            tri->Triangle(t).Get(n1, n2, n3);
            if (reversed)
                std::swap(n2, n3);  // keep outward-facing winding

            const gp_Pnt& a = nodes[static_cast<std::size_t>(n1 - 1)];
            const gp_Pnt& b = nodes[static_cast<std::size_t>(n2 - 1)];
            const gp_Pnt& c = nodes[static_cast<std::size_t>(n3 - 1)];
            const gp_Vec faceNormal = gp_Vec(a, b).Crossed(gp_Vec(a, c));  // length = 2 * area
            normals[static_cast<std::size_t>(n1 - 1)] += faceNormal;
            normals[static_cast<std::size_t>(n2 - 1)] += faceNormal;
            normals[static_cast<std::size_t>(n3 - 1)] += faceNormal;

            mesh.indices.push_back(base + static_cast<std::uint32_t>(n1 - 1));
            mesh.indices.push_back(base + static_cast<std::uint32_t>(n2 - 1));
            mesh.indices.push_back(base + static_cast<std::uint32_t>(n3 - 1));
        }

        for (const gp_Vec& n : normals) {
            const double len = n.Magnitude();
            const gp_Vec unit = len > 1e-12 ? n / len : gp_Vec(0.0, 0.0, 1.0);
            mesh.normals.push_back(static_cast<float>(unit.X()));
            mesh.normals.push_back(static_cast<float>(unit.Y()));
            mesh.normals.push_back(static_cast<float>(unit.Z()));
        }
    }

    return mesh;
}

}  // namespace forge
