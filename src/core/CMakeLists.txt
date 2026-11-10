# SPDX-License-Identifier: GPL-3.0-or-later
# forge_core: geometry and document logic. Depends on OpenCASCADE only (no Qt),
# so it can be unit-tested anywhere and reused by command-line tools later.

add_library(forge_core STATIC
    include/forge/core/Mesh.h
    include/forge/core/Primitives.h
    include/forge/core/Tessellator.h
    src/Mesh.cpp
    src/Primitives.cpp
    src/Tessellator.cpp
)
add_library(Forge3D::core ALIAS forge_core)

target_include_directories(forge_core PUBLIC include)
# SYSTEM keeps OCCT's own warnings out of our build output.
target_include_directories(forge_core SYSTEM PUBLIC ${OpenCASCADE_INCLUDE_DIR})

target_link_libraries(forge_core PUBLIC
    TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep
    TKGeomAlgo TKTopAlgo TKPrim TKMesh
)
