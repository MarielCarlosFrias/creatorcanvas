cmake_minimum_required(VERSION 3.24)

project(CreatorCanvas
    VERSION 0.2.1
    LANGUAGES C CXX
    DESCRIPTION "Fast, layer-based image editor for content creators")

option(CC_BUILD_TESTS         "Build unit tests"                    ON)
option(CC_WARNINGS_AS_ERRORS  "Treat compiler warnings as errors"  OFF)

# Optional external editor (Image > Edit in MiniPaint): point this at a
# MiniPaint source tree to build AND install minipaint-standalone beside
# CreatorCanvas in one flag. It needs SFML 3 (Graphics/Window/System) at
# configure time. Defaults to the vendored copy under 3rdparty/minipaint
# (which is how CI and the packaging scripts get it); set it to another
# checkout to build that one instead, or to "OFF" to disable and fall back to
# CC_MINIPAINT_BINARY / PATH discovery.
if(NOT DEFINED CC_MINIPAINT_SOURCE_DIR AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/minipaint)
    set(CC_MINIPAINT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/minipaint"
        CACHE PATH "MiniPaint source tree to build in-tree (optional; needs SFML 3)")
else()
    set(CC_MINIPAINT_SOURCE_DIR "" CACHE PATH
        "MiniPaint source tree to build in-tree (optional; needs SFML 3)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Deployment/reference toolchain is Qt 6.8 LTS; older 6.x is tolerated so
# distro-provided Qt can build during development. Revisit if APIs require.
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
if(CC_BUILD_TESTS)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
endif()

function(cc_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(CC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(CC_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------- MiniPaint (optional) --
# Build the external editor in-tree so a single flag configures, builds,
# installs and stages it beside CreatorCanvas. Its own CMake disables the
# example/tests/install rules unless it is the top-level project, so they are
# forced here regardless of how it was found.
if(CC_MINIPAINT_SOURCE_DIR AND IS_DIRECTORY ${CC_MINIPAINT_SOURCE_DIR})
    set(MINIPAINT_BUILD_EXAMPLE  ON  CACHE BOOL "" FORCE)
    set(MINIPAINT_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(MINIPAINT_INSTALL        OFF CACHE BOOL "" FORCE)
    # MiniPaint calls project(), which rebinds PROJECT_SOURCE_DIR — the
    # source lists in src/CMakeLists.txt are expressed with it, so it has to
    # be restored right after the subdirectory is added.
    set(CC_PARENT_PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR})
    set(CC_PARENT_PROJECT_BINARY_DIR ${PROJECT_BINARY_DIR})
    add_subdirectory(${CC_MINIPAINT_SOURCE_DIR} minipaint)
    set(PROJECT_SOURCE_DIR ${CC_PARENT_PROJECT_SOURCE_DIR})
    set(PROJECT_BINARY_DIR ${CC_PARENT_PROJECT_BINARY_DIR})

    if(TARGET minipaint-standalone)
        # Land in the same bin/ as CreatorCanvas and keep the installed
        # package relocatable: SFML is staged under lib/creatorcanvas
        # (packaging scripts), so the editor resolves it through the same
        # $ORIGIN-relative rpath.
        set_target_properties(minipaint-standalone PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            INSTALL_RPATH "$ORIGIN/../lib/creatorcanvas")

        # From here on, src/CMakeLists.txt treats it like a prebuilt binary.
        set(CC_MINIPAINT_BINARY $<TARGET_FILE:minipaint-standalone>)
        set(CC_MINIPAINT_IN_TREE ON)
    else()
        message(STATUS "  MiniPaint standalone:   SFML 3 not found - editor disabled")
    endif()
endif()

add_subdirectory(src)

if(CC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "CreatorCanvas ${PROJECT_VERSION} configured")
message(STATUS "  Tests:              ${CC_BUILD_TESTS}")
message(STATUS "  Warnings as errors: ${CC_WARNINGS_AS_ERRORS}")
