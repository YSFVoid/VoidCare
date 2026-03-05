cmake_minimum_required(VERSION 3.24)

project(VoidCare VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "VoidCare supports Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "VoidCare supports x64 only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(VOIDCARE_ENABLE_TESTS "Build VoidCare tests" ON)
option(VOIDCARE_BUILD_IMGUI "Build the ImGui frontend (primary)" ON)
option(VOIDCARE_BUILD_WPF "Enable WPF experimental frontend artifacts" OFF)
option(VOIDCARE_BUILD_BRIDGE "Build the C++ bridge executable" OFF)

if(VOIDCARE_BUILD_WPF)
    set(VOIDCARE_BUILD_BRIDGE ON CACHE BOOL "Build the C++ bridge executable" FORCE)
endif()

set(VOIDCARE_DISCORD_CLIENT_ID "1479026910574153921" CACHE STRING "Discord application client ID for Rich Presence")
if(VOIDCARE_DISCORD_CLIENT_ID STREQUAL "")
    set(VOIDCARE_DISCORD_CLIENT_ID "1479026910574153921" CACHE STRING "Discord application client ID for Rich Presence" FORCE)
endif()

if(NOT Qt6_DIR AND DEFINED ENV{Qt6_DIR} AND NOT "$ENV{Qt6_DIR}" STREQUAL "")
    set(Qt6_DIR "$ENV{Qt6_DIR}" CACHE PATH "Qt6 CMake directory" FORCE)
endif()

if((NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
   AND DEFINED ENV{CMAKE_PREFIX_PATH}
   AND NOT "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}" CACHE STRING "Qt kit prefix path" FORCE)
endif()

if((NOT Qt6_DIR OR Qt6_DIR STREQUAL "")
   AND (NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL ""))
    file(GLOB _voidcare_qt6_candidates LIST_DIRECTORIES true
        "C:/Qt/*/msvc*64/lib/cmake/Qt6")
    if(_voidcare_qt6_candidates)
        list(SORT _voidcare_qt6_candidates COMPARE NATURAL ORDER DESCENDING)
        list(GET _voidcare_qt6_candidates 0 _voidcare_qt6_best)
        set(Qt6_DIR "${_voidcare_qt6_best}" CACHE PATH "Auto-detected Qt6 CMake directory" FORCE)
        message(STATUS "Auto-detected Qt6_DIR=${Qt6_DIR}")
    endif()
endif()

find_package(Qt6 6.6 QUIET COMPONENTS Core Gui Widgets)
if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "Qt6 (>= 6.6) was not found.\n"
        "Install Qt 6.6+ with an MSVC x64 kit and configure one of:\n"
        "  -DQt6_DIR=<QtKit>/lib/cmake/Qt6\n"
        "  -DCMAKE_PREFIX_PATH=<QtKit>\n"
        "Or run scripts/configure.ps1 -QtDir <QtKit or Qt6 cmake dir>."
    )
endif()

add_subdirectory(platform/windows)
add_subdirectory(core)
add_subdirectory(ui)

if(VOIDCARE_BUILD_IMGUI)
    add_subdirectory(app)
endif()

if(VOIDCARE_BUILD_BRIDGE)
    add_subdirectory(bridge)
endif()

if(NOT VOIDCARE_BUILD_IMGUI AND NOT VOIDCARE_BUILD_BRIDGE)
    message(WARNING "No runnable frontend target is enabled. Enable VOIDCARE_BUILD_IMGUI and/or VOIDCARE_BUILD_BRIDGE.")
endif()

if(VOIDCARE_ENABLE_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
