# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Catch2 3 CONFIG REQUIRED)

add_executable(forge_tests
    test_primitives.cpp
    test_tessellator.cpp
    test_orbit_camera.cpp
    test_document.cpp
    test_command_stack.cpp
    test_picking.cpp
    test_file_format.cpp
    test_transform.cpp
    test_gizmo_edits.cpp
    test_gizmo_view.cpp
    test_combine.cpp
    test_align.cpp
)
target_link_libraries(forge_tests PRIVATE
    Forge3D::core
    Forge3D::view
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(forge_tests DISCOVERY_MODE PRE_TEST)
