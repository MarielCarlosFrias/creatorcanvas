cmake_minimum_required(VERSION 3.22)

project(MiniPaint
    VERSION 0.1.0
    DESCRIPTION "Reusable Paint-like editor module for SFML 3"
    LANGUAGES CXX
)

option(MINIPAINT_BUILD_EXAMPLE "Build the standalone MiniPaint example" ${PROJECT_IS_TOP_LEVEL})
option(MINIPAINT_BUILD_TESTS "Build the MiniPaint smoke tests" ${PROJECT_IS_TOP_LEVEL})
option(MINIPAINT_INSTALL "Generate installation rules" ${PROJECT_IS_TOP_LEVEL})

find_package(SFML 3.0 COMPONENTS Graphics Window System)
if(NOT SFML_FOUND)
    # Consumed inside a larger build (e.g. CreatorCanvas): a missing SFML only
    # disables this module instead of failing the whole configure. Standalone
    # builds keep the hard requirement via MINIPAINT_REQUIRE_SFML.
    if(MINIPAINT_REQUIRE_SFML OR PROJECT_IS_TOP_LEVEL)
        message(FATAL_ERROR
            "SFML 3.0 (Graphics, Window, System) not found - required to build MiniPaint")
    endif()
    message(WARNING
        "SFML 3.0 not found: MiniPaint will not be built "
        "(CreatorCanvas's 'Edit in MiniPaint' stays disabled)")
    return()
endif()

add_library(MiniPaint
    src/MiniPaint.cpp
    src/Canvas.cpp
    src/Layer.cpp
    src/commands/ImageCommand.cpp
    src/tools/AirbrushTool.cpp
    src/tools/BucketFillTool.cpp
    src/tools/Drawing.cpp
    src/tools/EyedropperTool.cpp
    src/tools/EraserTool.cpp
    src/tools/EllipseTool.cpp
    src/tools/LineTool.cpp
    src/tools/PencilTool.cpp
    src/tools/RectangleTool.cpp
    src/tools/TextTool.cpp
    src/tools/Tool.cpp
)

add_library(MiniPaint::MiniPaint ALIAS MiniPaint)

target_compile_features(MiniPaint PUBLIC cxx_std_17)
target_include_directories(MiniPaint
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(MiniPaint PUBLIC SFML::Graphics)

set_target_properties(MiniPaint PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

if(MSVC)
    target_compile_options(MiniPaint PRIVATE /W4 /permissive-)
else()
    target_compile_options(MiniPaint PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(MINIPAINT_BUILD_EXAMPLE)
    # The standalone example is written in pure SFML (no third-party UI
    # dependency), mirroring the look of MS Paint.
    add_executable(minipaint-standalone example/standalone.cpp)
    target_compile_features(minipaint-standalone PRIVATE cxx_std_17)
    target_link_libraries(minipaint-standalone PRIVATE MiniPaint::MiniPaint)
    set_target_properties(minipaint-standalone PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(MINIPAINT_BUILD_TESTS)
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()
    add_executable(minipaint-tests tests/minipaint_tests.cpp)
    target_compile_features(minipaint-tests PRIVATE cxx_std_17)
    target_link_libraries(minipaint-tests PRIVATE MiniPaint::MiniPaint)
    set_target_properties(minipaint-tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME MiniPaint.smoke COMMAND minipaint-tests)
endif()

if(MINIPAINT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS MiniPaint
        EXPORT MiniPaintTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT MiniPaintTargets
        FILE MiniPaintTargets.cmake
        NAMESPACE MiniPaint::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MiniPaint
    )
    export(EXPORT MiniPaintTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/MiniPaintTargets.cmake"
        NAMESPACE MiniPaint::
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/MiniPaintConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MiniPaintConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/MiniPaintConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MiniPaint
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/MiniPaintConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/MiniPaintConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MiniPaint
    )
endif()
