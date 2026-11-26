# SPDX-License-Identifier: GPL-3.0-or-later
# Forge3D Scenery: the companion app for scenery (buildings, trees, rocks, terrain). The
# same engine, view and .f3d files as Forge3D; its own window, version and releases.

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

configure_file(BuildInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.h @ONLY)

add_executable(Scenery WIN32
    main.cpp
    SceneryWindow.h
    SceneryWindow.cpp
    scenery.qrc
)
# Forge3D Scenery.exe
set_target_properties(Scenery PROPERTIES OUTPUT_NAME "Forge3D Scenery")
if(WIN32)
    target_sources(Scenery PRIVATE Scenery.rc)
endif()

target_include_directories(Scenery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(Scenery PRIVATE
    Forge3D::core
    Forge3D::view
    Forge3D::shell
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Network
)
