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
    core/serialization/ProjectFile.h
    core/serialization/ProjectFile.cpp
    core/geometry/AffineTransform.h
    core/assets/AssetStore.h
    core/assets/AssetStore.cpp
    imageio/ImageImporter.h
    imageio/ImageImporter.cpp
    imageio/DocumentExporter.h
    imageio/DocumentExporter.cpp
)
target_link_libraries(cc_core PUBLIC Qt6::Core Qt6::Gui miniz)
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
)
target_link_libraries(creatorcanvas
    PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
            cc_core cc_rendering cc_services cc_localization)
target_include_directories(creatorcanvas PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(creatorcanvas PRIVATE APP_VERSION="${PROJECT_VERSION}")
set_target_properties(creatorcanvas PROPERTIES
    OUTPUT_NAME "CreatorCanvas"
    WIN32_EXECUTABLE $<BOOL:WIN32>
    MACOSX_BUNDLE TRUE)
cc_enable_warnings(creatorcanvas)
