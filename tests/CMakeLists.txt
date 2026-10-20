function(cc_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE Qt6::Test Qt6::Widgets cc_services cc_localization cc_tools cc_core cc_rendering miniz)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_compile_definitions(${name} PRIVATE
        CC_PROJECT_ROOT="${PROJECT_SOURCE_DIR}")
    if(NOT WIN32)
        set_target_properties(${name} PROPERTIES
            BUILD_RPATH "${PROJECT_SOURCE_DIR}/3rdparty/onnxruntime/lib"
        )
    endif()
    cc_enable_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ASAN_OPTIONS=detect_leaks=1;LSAN_OPTIONS=suppressions=${PROJECT_SOURCE_DIR}/tests/asan_suppressions.txt"
    )
endfunction()

cc_add_test(test_settings      unit/test_settings.cpp)
cc_add_test(test_i18n          unit/test_i18n.cpp)
cc_add_test(test_logging       unit/test_logging.cpp)
cc_add_test(test_layers        unit/test_layers.cpp)
cc_add_test(test_document      unit/test_document.cpp)
cc_add_test(test_history       unit/test_history.cpp)
cc_add_test(test_serialization unit/test_serialization.cpp)
cc_add_test(test_renderer      unit/test_renderer.cpp)
cc_add_test(test_newdocument   unit/test_newdocument.cpp)
cc_add_test(test_import unit/test_import.cpp)
cc_add_test(test_transform unit/test_transform.cpp)
cc_add_test(test_textbox unit/test_textbox.cpp)
cc_add_test(test_effects unit/test_effects.cpp)
cc_add_test(test_export unit/test_export.cpp)
cc_add_test(test_autosave      unit/test_autosave.cpp)
cc_add_test(test_snap unit/test_snap.cpp)
cc_add_test(test_image_processing unit/test_image_processing.cpp)
cc_add_test(test_tools unit/test_tools.cpp)
cc_add_test(test_tools_extras unit/test_tools_extras.cpp)
cc_add_test(test_ai_quick unit/test_ai_quick.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/canvasview.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/aibackgrounddialog.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/maskeditcanvas.cpp
)
cc_add_test(test_ui_phase1 unit/test_ui_phase1.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/themeicons.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/collapsiblesection.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/startscreen.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/layerspanel.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/historypanel.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/textinspector.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/darktheme.cpp
)
cc_add_test(test_photocraft unit/test_photocraft.cpp)
cc_add_test(test_hit_alpha unit/test_hit_alpha.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/canvasview.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/aibackgrounddialog.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/maskeditcanvas.cpp
)
cc_add_test(test_canvas_zoom unit/test_canvas_zoom.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/canvasview.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/aibackgrounddialog.cpp
    ${PROJECT_SOURCE_DIR}/src/ui/maskeditcanvas.cpp
)
cc_add_test(test_template_textfit unit/test_template_textfit.cpp)
