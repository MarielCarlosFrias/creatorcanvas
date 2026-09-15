set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ------------------------------------------------------------ 3rdparty: miniz --
# Modular miniz. miniz.c is the BASE (crc32, adler32, default allocators,
# mz_free); tinfl/tdef/zip are the modules that depend on it. All four are
# required. miniz_tester.cpp is miniz's own example program - excluded.
add_library(miniz STATIC
    ${PROJECT_SOURCE_DIR}/3rdparty/miniz/miniz.c
    ${PROJECT_SOURCE_DIR}/3rdparty/miniz/miniz_tdef.cpp
    ${PROJECT_SOURCE_DIR}/3rdparty/miniz/miniz_tinfl.cpp
    ${PROJECT_SOURCE_DIR}/3rdparty/miniz/miniz_zip.cpp
)
target_include_directories(miniz SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/3rdparty/miniz)

# -------------------------------------------------------- 3rdparty: onnxruntime --
set(ONNX_DIR "${PROJECT_SOURCE_DIR}/3rdparty/onnxruntime")
add_library(onnxruntime SHARED IMPORTED)
if(WIN32)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_DIR}/lib/onnxruntime.dll"
        IMPORTED_IMPLIB "${ONNX_DIR}/lib/onnxruntime.lib"
    )
else()
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION "${ONNX_DIR}/lib/libonnxruntime.so"
    )
endif()

# --------------------------------------------------------------------- core --
add_library(cc_core STATIC
    core/layers/Layer.h
    core/layers/Layer.cpp
    core/Document.h
    core/Document.cpp
    core/document/NewDocumentSpec.h
    core/document/NewDocumentSpec.cpp
    core/history/Command.h
    core/history/Command.cpp
    core/history/CommandStack.h
    core/history/CommandStack.cpp
    core/history/DocumentCommands.h
    core/snap/SnapEngine.h
    core/snap/SnapEngine.cpp
    core/serialization/ProjectFile.h
    core/serialization/ProjectFile.cpp
    core/geometry/AffineTransform.h
    core/assets/AssetStore.h
    core/assets/AssetStore.cpp
    core/image/ImageProcessing.h
    core/image/ImageProcessing.cpp
    core/image/BackgroundRemover.h
    core/image/BackgroundRemover.cpp
    imageio/ImageImporter.h
    imageio/ImageImporter.cpp
    imageio/DocumentExporter.h
    imageio/DocumentExporter.cpp
)
target_link_libraries(cc_core PUBLIC Qt6::Core Qt6::Gui miniz onnxruntime)
target_include_directories(cc_core SYSTEM PRIVATE ${ONNX_DIR}/include)
target_compile_definitions(cc_core PRIVATE CC_PROJECT_VERSION="${PROJECT_VERSION}")
target_include_directories(cc_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_core)

# --------------------------------------------------------------- rendering --
add_library(cc_rendering STATIC
    rendering/CanvasRenderer.h
    rendering/CanvasRenderer.cpp
)
target_link_libraries(cc_rendering PUBLIC Qt6::Gui cc_core)
target_include_directories(cc_rendering PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_rendering)

# ---------------------------------------------------------------- services --
add_library(cc_services STATIC
    services/logservice.h
    services/logservice.cpp
    services/settingsservice.h
    services/settingsservice.cpp
    services/presetstore.h
    services/presetstore.cpp
    services/recentfiles.h
    services/recentfiles.cpp
    services/autosaveservice.h
    services/autosaveservice.cpp
    ${PROJECT_SOURCE_DIR}/resources/presets.qrc
)
target_link_libraries(cc_services PUBLIC Qt6::Core Qt6::Gui)
target_include_directories(cc_services PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_services)

# ------------------------------------------------------------ localization --
add_library(cc_localization STATIC
    localization/i18nservice.h
    localization/i18nservice.cpp
    ${PROJECT_SOURCE_DIR}/resources/locales.qrc
)
target_link_libraries(cc_localization PUBLIC Qt6::Core cc_services)
target_include_directories(cc_localization PUBLIC ${PROJECT_SOURCE_DIR}/src)
cc_enable_warnings(cc_localization)

# ------------------------------------------------------------- application --
add_executable(creatorcanvas
    main.cpp
    ui/darktheme.h
    ui/darktheme.cpp
    ui/mainwindow.h
    ui/mainwindow.cpp
    ui/canvasview.h
    ui/canvasview.cpp
    ui/newdocumentdialog.h
    ui/newdocumentdialog.cpp
    ui/textinspector.h
    ui/textinspector.cpp
    ui/exportdialog.h
    ui/exportdialog.cpp
    ui/layerspanel.h
    ui/layerspanel.cpp
    ui/settingsdialog.h
    ui/settingsdialog.cpp
    ui/startscreen.h
    ui/startscreen.cpp
    ui/shapeinspector.h
    ui/shapeinspector.cpp
    ui/imageinspector.h
    ui/imageinspector.cpp
    ui/maskeditcanvas.h
    ui/maskeditcanvas.cpp
    ui/aibackgrounddialog.h
    ui/aibackgrounddialog.cpp
    ui/themeicons.h
    ui/themeicons.cpp
    ui/collapsiblesection.h
    ui/collapsiblesection.cpp
)
target_link_libraries(creatorcanvas
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
            cc_core cc_rendering cc_services cc_localization)
target_include_directories(creatorcanvas PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(creatorcanvas PRIVATE APP_VERSION="${PROJECT_VERSION}")
if(NOT WIN32)
    set_target_properties(creatorcanvas PROPERTIES
        OUTPUT_NAME "CreatorCanvas"
        WIN32_EXECUTABLE $<BOOL:WIN32>
        MACOSX_BUNDLE TRUE
        BUILD_RPATH "${ONNX_DIR}/lib"
        INSTALL_RPATH "$ORIGIN/../lib/creatorcanvas"
    )
else()
    set_target_properties(creatorcanvas PROPERTIES
        OUTPUT_NAME "CreatorCanvas"
        WIN32_EXECUTABLE $<BOOL:WIN32>
        MACOSX_BUNDLE TRUE)
endif()
cc_enable_warnings(creatorcanvas)

# ===================================================================
# M14 - Distribution: install rules (DEB/AppImage/NSIS-friendly)
# ===================================================================

set(CC_STAGING_DIR ${CMAKE_BINARY_DIR}/staging)

install(TARGETS creatorcanvas
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/resources/locales
        DESTINATION share/creatorcanvas/locales
        OPTIONAL)

install(FILES ${PROJECT_SOURCE_DIR}/resources/presets/builtin.json
        DESTINATION share/creatorcanvas/presets
        OPTIONAL)

install(FILES ${PROJECT_SOURCE_DIR}/resources/icons/creatorcanvas.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME creatorcanvas.svg
        OPTIONAL)

install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/creatorcanvas.desktop
        DESTINATION share/applications
        OPTIONAL)

install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/creatorcanvas.appdata.xml
        DESTINATION share/metainfo
        RENAME creatorcanvas.appdata.xml
        OPTIONAL)

install(DIRECTORY ${ONNX_DIR}/lib/
        DESTINATION lib/creatorcanvas
        OPTIONAL
        FILES_MATCHING PATTERN "libonnxruntime*.so*")

add_custom_target(stage ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CC_STAGING_DIR}/bin
                ${CC_STAGING_DIR}/lib/creatorcanvas
                ${CC_STAGING_DIR}/share/creatorcanvas
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:creatorcanvas>
                ${CC_STAGING_DIR}/bin/creatorcanvas
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                ${ONNX_DIR}/lib
                ${CC_STAGING_DIR}/lib/creatorcanvas
        COMMENT "Staging CreatorCanvas for packaging")
