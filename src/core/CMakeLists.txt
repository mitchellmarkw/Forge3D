# SPDX-License-Identifier: GPL-3.0-or-later
# forge_core: geometry and document logic. No Qt: OpenCASCADE, plus miniz and
# nlohmann-json for the file format,
# so it can be unit-tested anywhere and reused by command-line tools later.

add_library(forge_core STATIC
    include/forge/core/Body.h
    include/forge/core/CommandStack.h
    include/forge/core/Commands.h
    include/forge/core/Document.h
    include/forge/core/FileFormat.h
    include/forge/core/Mesh.h
    include/forge/core/Picking.h
    include/forge/core/Primitives.h
    include/forge/core/Tessellator.h
    src/CommandStack.cpp
    src/Commands.cpp
    src/Document.cpp
    src/FileFormat.cpp
    src/Mesh.cpp
    src/Picking.cpp
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

# File format libraries are an implementation detail of FileFormat.cpp.
target_link_libraries(forge_core PRIVATE miniz::miniz nlohmann_json::nlohmann_json)
