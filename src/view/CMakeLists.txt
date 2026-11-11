# SPDX-License-Identifier: GPL-3.0-or-later
# forge_view: camera and viewport logic that needs Qt's math types but no window,
# so it can be unit-tested without a display.

add_library(forge_view STATIC
    include/forge/view/OrbitCamera.h
    src/OrbitCamera.cpp
)
add_library(Forge3D::view ALIAS forge_view)

target_include_directories(forge_view PUBLIC include)
target_link_libraries(forge_view PUBLIC Qt6::Gui)
