# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.21)

project(Forge3D
    VERSION 0.18.0
    DESCRIPTION "Parametric 3D modeling with sculpting"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FORGE3D_BUILD_APP   "Build the Forge3D desktop application" ON)
option(FORGE3D_BUILD_TESTS "Build the unit tests"                  ON)

# Put every executable and DLL in one folder so the app runs straight from the build tree.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---- Dependencies ---------------------------------------------------------------
find_package(OpenCASCADE CONFIG REQUIRED)
message(STATUS "OpenCASCADE ${OpenCASCADE_VERSION} headers: ${OpenCASCADE_INCLUDE_DIR}")
find_package(miniz CONFIG REQUIRED)
find_package(nlohmann_json 3 CONFIG REQUIRED)
find_package(manifold CONFIG REQUIRED)  # the mesh engine (imported meshes)

if(FORGE3D_BUILD_APP OR FORGE3D_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)
    message(STATUS "Qt ${Qt6_VERSION}")
endif()

# ---- Targets --------------------------------------------------------------------
add_subdirectory(src/core)

if(FORGE3D_BUILD_APP OR FORGE3D_BUILD_TESTS)
    add_subdirectory(src/view)
endif()

if(FORGE3D_BUILD_APP)
    add_subdirectory(src/app)
endif()

if(FORGE3D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
