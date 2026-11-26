# SPDX-License-Identifier: GPL-3.0-or-later
# forge_shell: the window pieces both apps share (Forge3D and Forge3D Scenery): the 3D
# view and its view cube, the light and dark themes, the log file and the crash handler.

set(CMAKE_AUTOMOC ON)

add_library(forge_shell STATIC
    Viewport.h
    Viewport.cpp
    ViewCube.h
    ViewCube.cpp
    Theme.h
    Theme.cpp
    Log.h
    Log.cpp
    CrashHandler.h
    CrashHandler.cpp
)
add_library(Forge3D::shell ALIAS forge_shell)

target_include_directories(forge_shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(forge_shell PUBLIC
    Forge3D::core
    Forge3D::view
    Qt6::Widgets
    Qt6::OpenGLWidgets
)
if(WIN32)
    target_link_libraries(forge_shell PUBLIC dbghelp)
endif()
