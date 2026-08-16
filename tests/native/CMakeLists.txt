if(NOT (APPLE AND EGE_RESOLVED_BACKEND STREQUAL "COREGRAPHICS")
        AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux"
            AND EGE_RESOLVED_BACKEND STREQUAL "CAIRO"))
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

ege_add_native_test(ege_native_camera_frame_copy native.camera_frame_copy
    camera_frame_copy_test.cpp)

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

if(APPLE)
    ege_add_native_test(ege_native_music_contract
        native.ege_music_contract
        ege_music_contract_test.cpp)
    target_compile_definitions(ege_native_music_contract PRIVATE
        EGE_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts")
endif()

add_executable(ege_native_process_exit_contract
    ege_process_exit_contract.cpp)
target_compile_features(ege_native_process_exit_contract PRIVATE cxx_std_17)
target_link_libraries(ege_native_process_exit_contract PRIVATE xege)
add_test(NAME native.ege_process_exit_contract
    COMMAND "${CMAKE_COMMAND}"
        -DEGE_EXIT_PROGRAM=$<TARGET_FILE:ege_native_process_exit_contract>
        -P "${CMAKE_CURRENT_SOURCE_DIR}/assert_process_exit.cmake")
set_tests_properties(native.ege_process_exit_contract PROPERTIES TIMEOUT 15)

if(APPLE AND EGE_RESOLVED_BACKEND STREQUAL "COREGRAPHICS")
    _ege_find_required_framework(EGE_TEST_COREGRAPHICS_FRAMEWORK CoreGraphics)
    _ege_find_required_framework(EGE_TEST_APPKIT_FRAMEWORK AppKit)

    ege_add_native_test(ege_native_public_objcxx_headers
        native.public_objcxx_headers
        public_headers_objcxx_main.cpp
        public_close_callback_appkit.mm
        public_headers_appkit_first.mm)
    target_link_libraries(ege_native_public_objcxx_headers PRIVATE
        "${EGE_TEST_APPKIT_FRAMEWORK}"
        "${EGE_TEST_COREGRAPHICS_FRAMEWORK}")
    ege_enable_objc_arc(ege_native_public_objcxx_headers)

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

        add_executable(ege_native_expected_exit_runner expected_exit_runner.cpp)
        target_compile_features(ege_native_expected_exit_runner PRIVATE cxx_std_17)

        add_executable(ege_native_public_close_callback_contract
            public_close_callback_smoke.cpp
            public_close_callback_appkit.mm
            public_headers_appkit_first.mm)
        target_compile_features(ege_native_public_close_callback_contract
            PRIVATE cxx_std_17)
        target_link_libraries(ege_native_public_close_callback_contract PRIVATE
            xege
            "${EGE_TEST_APPKIT_FRAMEWORK}"
            "${EGE_TEST_COREGRAPHICS_FRAMEWORK}")
        ege_enable_objc_arc(ege_native_public_close_callback_contract)
        add_test(NAME native.public_close_callback_contract
            COMMAND ege_native_expected_exit_runner
                $<TARGET_FILE:ege_native_public_close_callback_contract> 42)
        set_tests_properties(native.public_close_callback_contract PROPERTIES
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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EGE_RESOLVED_BACKEND STREQUAL "CAIRO")
    ege_add_native_test(ege_native_cairo_render_target
        native.cairo_render_target
        cairo_render_target_test.cpp)
    target_link_libraries(ege_native_cairo_render_target PRIVATE
        PkgConfig::EGE_CAIRO)

    ege_add_native_test(ege_native_api_smoke native.ege_api_smoke
        ege_api_smoke.cpp)
    target_compile_definitions(ege_native_api_smoke PRIVATE
        EGE_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts")
    set_tests_properties(native.ege_api_smoke PROPERTIES
        ENVIRONMENT "EGE_HEADLESS=1"
        TIMEOUT 15)

    if(EGE_ENABLE_WINDOW_TESTS)
        ege_add_native_test(ege_native_linux_window_smoke
            native.linux_window_smoke
            linux_window_smoke.cpp)
        target_link_libraries(ege_native_linux_window_smoke PRIVATE
            PkgConfig::EGE_X11)
        set_tests_properties(native.linux_window_smoke PROPERTIES
            RUN_SERIAL TRUE
            TIMEOUT 15)
    endif()

    if(EGE_ENABLE_CAMERA_TESTS)
        if(NOT EGE_ENABLE_CAMERA_CAPTURE)
            message(FATAL_ERROR
                "EGE_ENABLE_CAMERA_TESTS=ON requires EGE_ENABLE_CAMERA_CAPTURE=ON")
        endif()

        add_library(ege_native_fake_v4l2 SHARED fake_v4l2_preload.cpp)
        target_compile_features(ege_native_fake_v4l2 PRIVATE cxx_std_17)
        target_link_libraries(ege_native_fake_v4l2 PRIVATE ${CMAKE_DL_LIBS})

        add_executable(ege_native_camera_capture_virtual
            ege_camera_capture_virtual_test.cpp)
        target_compile_features(ege_native_camera_capture_virtual PRIVATE cxx_std_17)
        target_include_directories(ege_native_camera_capture_virtual PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_definitions(ege_native_camera_capture_virtual PRIVATE
            EGE_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts")
        target_link_libraries(ege_native_camera_capture_virtual PRIVATE xege)

        add_test(NAME native.ege_camera_capture_virtual
            COMMAND "${CMAKE_COMMAND}" -E env
                "EGE_TEST_V4L2_PATH=/dev/video99"
                "LD_PRELOAD=$<TARGET_FILE:ege_native_fake_v4l2>"
                "$<TARGET_FILE:ege_native_camera_capture_virtual>")
        set_tests_properties(native.ege_camera_capture_virtual PROPERTIES
            RUN_SERIAL TRUE
            TIMEOUT 15)
    endif()
endif()
