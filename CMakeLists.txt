cmake_minimum_required(VERSION 3.24)

project(CreatorCanvas
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Fast, layer-based image editor for content creators")

option(CC_BUILD_TESTS         "Build unit tests"                    ON)
option(CC_WARNINGS_AS_ERRORS  "Treat compiler warnings as errors"  OFF)

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

add_subdirectory(src)

if(CC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "CreatorCanvas ${PROJECT_VERSION} configured")
message(STATUS "  Tests:              ${CC_BUILD_TESTS}")
message(STATUS "  Warnings as errors: ${CC_WARNINGS_AS_ERRORS}")
