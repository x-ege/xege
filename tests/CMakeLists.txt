# 包含EGE头文件目录
include_directories(../include)

# The public backend selector is intentionally compile-time gated.  Positive
# visibility is asserted in public_headers_test when OpenGL is enabled; this
# configure check locks the equally important compatibility contract that an
# OFF/default Windows build does not expose the new enum at all.
if(NOT EGE_BUILD_OPENGL)
    include(CheckCXXSourceCompiles)
    set(_EGE_SAVED_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES}")
    set(_EGE_SAVED_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
    set(CMAKE_REQUIRED_INCLUDES "${PROJECT_SOURCE_DIR}/include")
    set(CMAKE_REQUIRED_DEFINITIONS -DEGE_GRAPH_NO_LIB)
    unset(EGE_INIT_OPENGL_VISIBLE_WHEN_DISABLED CACHE)
    check_cxx_source_compiles([[
        #include <ege.h>
        int main() { return static_cast<int>(ege::INIT_OPENGL); }
    ]] EGE_INIT_OPENGL_VISIBLE_WHEN_DISABLED)
    set(CMAKE_REQUIRED_INCLUDES "${_EGE_SAVED_REQUIRED_INCLUDES}")
    set(CMAKE_REQUIRED_DEFINITIONS "${_EGE_SAVED_REQUIRED_DEFINITIONS}")
    unset(_EGE_SAVED_REQUIRED_INCLUDES)
    unset(_EGE_SAVED_REQUIRED_DEFINITIONS)
    if(EGE_INIT_OPENGL_VISIBLE_WHEN_DISABLED)
        message(FATAL_ERROR
            "INIT_OPENGL must not be visible when EGE_BUILD_OPENGL=OFF")
    endif()
endif()

# 添加编译定义
add_definitions(-DSHOW_CONSOLE=1)
add_definitions(-DEGE_GRAPH_NO_LIB)
add_definitions(-DNOMINMAX)

# 添加Windows系统库
if(WIN32 OR NOT EGE_BUILD_FOR_LINUX)
    set(SYSTEM_LIBS
        gdi32
        user32
        kernel32
        gdiplus
        ole32
        oleaut32
        uuid
        winmm
    )
else()
    set(SYSTEM_LIBS "")
endif()

# 创建测试工具库
add_library(test_framework STATIC
    test_framework.cpp
    test_framework.h
    image_generator.cpp
    image_generator.h
    performance_timer.cpp
    performance_timer.h
)

target_link_libraries(test_framework ${SYSTEM_LIBS})

if(MSVC)
    target_compile_options(test_framework PUBLIC
        /Zc:__cplusplus
    )
endif()

# 添加测试可执行文件的宏
macro(add_test_executable name source)
    add_executable(${name} ${source})
    target_link_libraries(${name}
        test_framework
        xege
        ${SYSTEM_LIBS}
    )
    
    if(MSVC)
        if(MSVC_VERSION GREATER_EQUAL 1900) # MSVC 2015 及以上支持 /utf-8
            target_compile_options(${name} PRIVATE /utf-8)
        endif()
    endif()
endmacro()

# 主性能测试程序
add_test_executable(putimage_performance_test tests/putimage_performance_test.cpp)

# 单独的测试用例
add_test_executable(putimage_basic_test tests/putimage_basic_test.cpp)
add_test_executable(putimage_alphablend_test tests/putimage_alphablend_test.cpp)
add_test_executable(putimage_transparent_test tests/putimage_transparent_test.cpp)
add_test_executable(putimage_rotate_test tests/putimage_rotate_test.cpp)
add_test_executable(putimage_comparison_test tests/putimage_comparison_test.cpp)
add_test_executable(putimage_alphablend_comprehensive_test tests/putimage_alphablend_comprehensive_test.cpp)
add_test_executable(rendering_correctness_test tests/rendering_correctness_test.cpp)
add_test_executable(public_headers_test tests/public_headers_test.cpp)
add_test_executable(camera_frame_copy_test tests/camera_frame_copy_test.cpp)
if(EGE_ENABLE_CAMERA_CAPTURE)
    add_test_executable(camera_device_lifecycle_test
        tests/camera_device_lifecycle_test.cpp)
endif()
if(WIN32)
    add_test_executable(gdi_lifecycle_test tests/gdi_lifecycle_test.cpp)
    add_test_executable(gdi_process_exit_test tests/gdi_process_exit_test.cpp)
endif()
if(EGE_ENABLE_CAMERA_CAPTURE AND (APPLE OR WIN32))
    add_test_executable(camera_capture_test tests/camera_capture_test.cpp)
    target_compile_definitions(camera_capture_test PRIVATE
        EGE_TEST_VIDEO_PATH="${PROJECT_SOURCE_DIR}/3rdparty/ccap/tests/test-data/test.mp4"
    )
endif()
if(EGE_BUILD_OPENGL)
    add_test_executable(input_backend_test tests/input_backend_test.cpp)
    add_test_executable(page_backend_test tests/page_backend_test.cpp)
    add_test_executable(window_backend_test tests/window_backend_test.cpp)
    add_test_executable(opengl_process_exit_test tests/opengl_process_exit_test.cpp)
    # These white-box backend tests include GLFW headers directly.  Link the
    # selected GLFW target explicitly so bundled builds do not accidentally
    # depend on a system-wide GLFW development package.
    target_link_libraries(input_backend_test glfw)
    target_link_libraries(page_backend_test glfw)
    target_link_libraries(window_backend_test glfw)
    target_link_libraries(opengl_process_exit_test glfw)
    if(NOT MSVC)
        # This target intentionally verifies the deprecated BGI page API for
        # compatibility with existing EGE programs.
        target_compile_options(page_backend_test PRIVATE -Wno-deprecated-declarations)
    endif()
endif()

# 设置输出目录
set_target_properties(
    putimage_performance_test
    putimage_basic_test
    putimage_alphablend_test
    putimage_transparent_test
    putimage_rotate_test
    putimage_comparison_test
    putimage_alphablend_comprehensive_test
    rendering_correctness_test
    public_headers_test
    camera_frame_copy_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
if(EGE_ENABLE_CAMERA_CAPTURE)
    set_target_properties(camera_device_lifecycle_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
if(EGE_ENABLE_CAMERA_CAPTURE AND (APPLE OR WIN32))
    set_target_properties(camera_capture_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
if(WIN32)
    set_target_properties(gdi_lifecycle_test gdi_process_exit_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()
if(EGE_BUILD_OPENGL)
    set_target_properties(input_backend_test page_backend_test window_backend_test
        opengl_process_exit_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# 创建测试目标
enable_testing()
add_test(
    NAME default_build_contract
    COMMAND ${CMAKE_COMMAND}
        "-DEGE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DEGE_BINARY_DIR=${CMAKE_BINARY_DIR}/default-build-contract"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/default_build_contract_test.cmake"
)
add_test(NAME putimage_performance COMMAND putimage_performance_test)
add_test(NAME putimage_basic COMMAND putimage_basic_test)
add_test(NAME putimage_alphablend COMMAND putimage_alphablend_test)
add_test(NAME putimage_transparent COMMAND putimage_transparent_test)
add_test(NAME putimage_rotate COMMAND putimage_rotate_test)
add_test(NAME putimage_comparison COMMAND putimage_comparison_test)
add_test(NAME putimage_alphablend_comprehensive COMMAND putimage_alphablend_comprehensive_test)
add_test(NAME rendering_correctness COMMAND rendering_correctness_test)
add_test(NAME public_headers COMMAND public_headers_test)
add_test(NAME camera_frame_copy COMMAND camera_frame_copy_test)
if(EGE_ENABLE_CAMERA_CAPTURE)
    add_test(NAME camera_device_lifecycle COMMAND camera_device_lifecycle_test)
endif()
if(WIN32)
    add_test(NAME gdi_lifecycle COMMAND gdi_lifecycle_test)
    add_test(NAME gdi_process_exit COMMAND gdi_process_exit_test)
endif()
if(EGE_ENABLE_CAMERA_CAPTURE AND (APPLE OR WIN32))
    add_test(NAME camera_capture COMMAND camera_capture_test)
    add_test(NAME camera_capture_headless COMMAND camera_capture_test --headless)
endif()
if(EGE_BUILD_OPENGL)
    add_test(NAME input_backend COMMAND input_backend_test)
    add_test(NAME page_backend COMMAND page_backend_test)
    add_test(NAME window_backend COMMAND window_backend_test)
    add_test(NAME opengl_process_exit COMMAND opengl_process_exit_test)
endif()
set_tests_properties(
    putimage_performance
    putimage_basic
    putimage_alphablend
    putimage_transparent
    putimage_rotate
    putimage_comparison
    putimage_alphablend_comprehensive
    PROPERTIES LABELS "performance"
)
set_tests_properties(default_build_contract PROPERTIES
    LABELS "correctness;configure"
    TIMEOUT 180
)
set_tests_properties(rendering_correctness PROPERTIES LABELS "correctness")
set_tests_properties(public_headers PROPERTIES LABELS "correctness;compile")
set_tests_properties(camera_frame_copy PROPERTIES LABELS "correctness;camera")
if(EGE_ENABLE_CAMERA_CAPTURE)
    set_tests_properties(camera_device_lifecycle PROPERTIES
        LABELS "correctness;camera;headless"
        TIMEOUT 30
    )
endif()
if(WIN32)
    set_tests_properties(gdi_lifecycle PROPERTIES LABELS "correctness;window;input;gdi")
    set_tests_properties(gdi_process_exit PROPERTIES
        LABELS "correctness;window;exit;gdi"
        TIMEOUT 15
    )
endif()
if(EGE_ENABLE_CAMERA_CAPTURE AND (APPLE OR WIN32))
    set_tests_properties(camera_capture PROPERTIES LABELS "correctness;camera")
    set_tests_properties(camera_capture_headless PROPERTIES
        LABELS "correctness;camera;headless"
    )
endif()
if(EGE_BUILD_OPENGL)
    set_tests_properties(input_backend PROPERTIES LABELS "correctness;input")
    set_tests_properties(page_backend PROPERTIES LABELS "correctness;page")
    set_tests_properties(window_backend PROPERTIES LABELS "correctness;window")
    set_tests_properties(opengl_process_exit PROPERTIES
        LABELS "correctness;window;exit"
        TIMEOUT 15
    )
endif()
