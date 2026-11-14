# SPDX-License-Identifier: GPL-3.0-or-later
# Forge3D: the desktop application (windows, menus, OpenGL viewport).

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

configure_file(BuildInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.h @ONLY)

add_executable(Forge3D WIN32
    main.cpp
    Log.h
    Log.cpp
    CrashHandler.h
    CrashHandler.cpp
    MainWindow.h
    MainWindow.cpp
    PropertiesPanel.h
    PropertiesPanel.cpp
    SplitPanel.h
    SplitPanel.cpp
    EdgesPanel.h
    EdgesPanel.cpp
    FollowPanel.h
    FollowPanel.cpp
    PatternPanel.h
    PatternPanel.cpp
    HolePanel.h
    HolePanel.cpp
    ShellPanel.h
    ShellPanel.cpp
    SketchPanel.h
    SketchPanel.cpp
    SketchView.h
    SketchView.cpp
    EmbossPanel.h
    EmbossPanel.cpp
    WordsEditor.h
    WordsEditor.cpp
    Viewport.h
    Viewport.cpp
    Tour.h
    SculptPanel.h
    SculptPanel.cpp
    Tour.cpp
    icons.qrc
    help.qrc
)
if(WIN32)
    # The program's own icon (Explorer, the taskbar, the installer's shortcuts).
    target_sources(Forge3D PRIVATE Forge3D.rc)
endif()

target_include_directories(Forge3D PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(Forge3D PRIVATE
    Forge3D::core
    Forge3D::view
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Svg
    Qt6::Network
)

if(WIN32)
    target_link_libraries(Forge3D PRIVATE dbghelp)
endif()
