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
    test_shape_library.cpp
    test_split.cpp
    test_handles.cpp
    test_arrange.cpp
    test_export.cpp
    test_import.cpp
    test_mesh_engine.cpp
    test_text_emboss.cpp
    test_edges.cpp
    test_history.cpp
    test_sketch.cpp
    test_features.cpp
    test_picture.cpp
    test_updates.cpp
    test_sculpt.cpp
    test_remesh.cpp
    test_measure.cpp
    test_thread.cpp
    test_loft.cpp
    test_sketch_edit.cpp
    test_taper.cpp
    test_sketch_text.cpp
    test_print_split.cpp
    test_colour.cpp
    test_mesh_steps.cpp
    test_import_ply_gltf.cpp
    test_layout.cpp
    test_custom_shapes.cpp
    test_socket.cpp
    bench_sculpt.cpp
    fuzz_files.cpp
    test_undo_random.cpp
)
target_link_libraries(forge_tests PRIVATE
    Forge3D::core
    Forge3D::view
    Catch2::Catch2WithMain
    miniz::miniz  # the export test opens 3MF files
)

include(Catch)
catch_discover_tests(forge_tests DISCOVERY_MODE PRE_TEST)
