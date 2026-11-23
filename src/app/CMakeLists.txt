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
    FitPanel.cpp
    CustomShapesDialog.h
    CustomShapesDialog.cpp
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
    MeasurePanel.h
    SectionPanel.h
    Theme.h
    Theme.cpp
    SectionPanel.cpp
    MeasurePanel.cpp
    ThreadPanel.h
    ThreadPanel.cpp
    SocketPanel.h
    SocketPanel.cpp
    JoinPanel.h
    JoinPanel.cpp
    MaskPanel.h
    MaskPanel.cpp
    ReferencePanel.h
    HingePanel.h
    ScaleCalculator.h
    PosePanel.h
    Fits.h
    Fits.cpp
    ToleranceDialog.h
    ToleranceDialog.cpp
    PosePanel.cpp
    PrintCheckPanel.h
    PrintCheckPanel.cpp
    RescaleDialog.h
    RescaleDialog.cpp
    MannequinPanel.h
    MannequinPanel.cpp
    HollowPanel.h
    HollowPanel.cpp
    MouldPanel.h
    MouldPanel.cpp
    PortPanel.h
    PortPanel.cpp
    HandPanel.h
    HandPanel.cpp
    PoseFigurePanel.h
    PoseFigurePanel.cpp
    PaintPanel.h
    PaintPanel.cpp
    Standards.h
    Standards.cpp
    JointStandardsDialog.h
    JointStandardsDialog.cpp
    CutPartsPanel.h
    DisplayBaseDialog.h
    DisplayBaseDialog.cpp
    VariablesDialog.h
    VariablesDialog.cpp
    InstructionSheetDialog.h
    InstructionSheetDialog.cpp
    SewingPatternDialog.h
    SewingPatternDialog.cpp
    PictureDialog.h
    PictureDialog.cpp
    CutPartsPanel.cpp
    StampPanel.h
    StampPanel.cpp
    PanelLinePanel.h
    PanelLinePanel.cpp
    KitDialog.h
    KitDialog.cpp
    PartsListDialog.h
    PartsListDialog.cpp
    ScaleCalculator.cpp
    HingePanel.cpp
    ReferencePanel.cpp
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
    ViewCube.h
    ViewCube.cpp
    SculptPanel.h
    SculptPanel.cpp
    Tour.cpp
    icons.qrc
    help.qrc
)

# MainWindow.cpp has grown past MSVC's default limit of sections in one object file.
if(MSVC)
    target_compile_options(Forge3D PRIVATE /bigobj)
endif()
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
