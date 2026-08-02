if(NOT APPLE OR NOT EGE_RESOLVED_BACKEND STREQUAL "COREGRAPHICS")
    return()
endif()

function(ege_add_native_test target test_name)
    add_executable(${target} ${ARGN})
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${target} PRIVATE xege)
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

function(ege_enable_objc_arc target)
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        set_target_properties(${target} PROPERTIES
            XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
    else()
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
    endif()
endfunction()

ege_add_native_test(ege_native_public_headers_en native.public_headers_en
    public_headers_main.cpp
    public_headers_en.cpp)

ege_add_native_test(ege_native_public_headers_zh native.public_headers_zh
    public_headers_main.cpp
    public_headers_zh.cpp)
target_compile_definitions(ege_native_public_headers_zh PRIVATE EGE_TEST_ZH_HEADER=1)

ege_add_native_test(ege_native_pixel_surface native.pixel_surface
    pixel_surface_test.cpp)

ege_add_native_test(ege_native_raster_image_contract
    native.ege_raster_image_contract
    ege_raster_image_contract_test.cpp)
target_compile_definitions(ege_native_raster_image_contract PRIVATE
    EGE_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts")

ege_add_native_test(ege_native_text_utility_contract
    native.ege_text_utility_contract
    ege_text_utility_contract_test.cpp)

ege_add_native_test(ege_native_enhanced_api_contract
    native.ege_enhanced_api_contract
    ege_enhanced_api_contract_test.cpp)

ege_add_native_test(ege_native_control_focus_contract
    native.ege_control_focus_contract
    ege_control_focus_contract_test.cpp)

if(APPLE AND EGE_RESOLVED_BACKEND STREQUAL "COREGRAPHICS")
    _ege_find_required_framework(EGE_TEST_COREGRAPHICS_FRAMEWORK CoreGraphics)

    ege_add_native_test(ege_native_coregraphics_surface
        native.coregraphics_surface
        coregraphics_surface_test.cpp)
    target_link_libraries(ege_native_coregraphics_surface PRIVATE
        "${EGE_TEST_COREGRAPHICS_FRAMEWORK}")

    ege_add_native_test(ege_native_coregraphics_render_target
        native.coregraphics_render_target
        coregraphics_render_target_test.cpp)
    target_link_libraries(ege_native_coregraphics_render_target PRIVATE
        "${EGE_TEST_COREGRAPHICS_FRAMEWORK}")

    ege_add_native_test(ege_native_api_smoke native.ege_api_smoke
        ege_api_smoke.cpp)
    target_compile_definitions(ege_native_api_smoke PRIVATE
        EGE_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts")
    target_link_libraries(ege_native_api_smoke PRIVATE
        "${EGE_TEST_COREGRAPHICS_FRAMEWORK}")
    set_tests_properties(native.ege_api_smoke PROPERTIES
        ENVIRONMENT "EGE_HEADLESS=1"
        TIMEOUT 15)

    if(EGE_ENABLE_WINDOW_TESTS)
        message(WARNING
            "EGE_ENABLE_WINDOW_TESTS=ON: native tests will create visible windows")
        _ege_find_required_framework(EGE_TEST_APPKIT_FRAMEWORK AppKit)
        _ege_find_required_framework(EGE_TEST_COREFOUNDATION_FRAMEWORK CoreFoundation)

        ege_add_native_test(ege_native_mac_window_smoke native.mac_window_smoke
            mac_window_smoke.mm)
        target_link_libraries(ege_native_mac_window_smoke PRIVATE
            "${EGE_TEST_APPKIT_FRAMEWORK}"
            "${EGE_TEST_COREGRAPHICS_FRAMEWORK}")
        ege_enable_objc_arc(ege_native_mac_window_smoke)
        set_tests_properties(native.mac_window_smoke PROPERTIES
            RUN_SERIAL TRUE
            SKIP_RETURN_CODE 77
            TIMEOUT 15)

        add_executable(ege_native_demo_launch_smoke demo_launch_smoke.mm)
        target_compile_features(ege_native_demo_launch_smoke PRIVATE cxx_std_17)
        ege_enable_objc_arc(ege_native_demo_launch_smoke)
        target_link_libraries(ege_native_demo_launch_smoke PRIVATE
            "${EGE_TEST_APPKIT_FRAMEWORK}"
            "${EGE_TEST_COREFOUNDATION_FRAMEWORK}"
            "${EGE_TEST_COREGRAPHICS_FRAMEWORK}")
    endif()
endif()
