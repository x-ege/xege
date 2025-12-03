# 包含EGE头文件目录
include_directories(../include)

# 添加编译定义
add_definitions(-DSHOW_CONSOLE=1)
add_definitions(-DEGE_GRAPH_NO_LIB)
add_definitions(-DNOMINMAX)

# 添加Windows系统库
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
        /source-charset:utf-8
        /Zc:__cplusplus
    )
endif()

# 测试分类选项
option(EGE_TEST_PERFORMANCE "Build performance tests" ON)
option(EGE_TEST_FUNCTIONAL "Build functional tests" ON)

# 主性能测试程序
add_executable(putimage_performance_test
    tests/putimage_performance_test.cpp
)

target_link_libraries(putimage_performance_test
    test_framework
    xege
    ${SYSTEM_LIBS}
)

# 单独的测试用例
add_executable(putimage_basic_test
    tests/putimage_basic_test.cpp
)

target_link_libraries(putimage_basic_test
    test_framework
    xege
    ${SYSTEM_LIBS}
)

add_executable(putimage_alphablend_test
    tests/putimage_alphablend_test.cpp
)

target_link_libraries(putimage_alphablend_test
    test_framework
    xege
    ${SYSTEM_LIBS}
)

add_executable(putimage_transparent_test
    tests/putimage_transparent_test.cpp
)

target_link_libraries(putimage_transparent_test
    test_framework
    xege
    ${SYSTEM_LIBS}
)

add_executable(putimage_rotate_test
    tests/putimage_rotate_test.cpp
)

target_link_libraries(putimage_rotate_test
    test_framework
    xege
    ${SYSTEM_LIBS}
)

add_executable(putimage_comparison_test
    tests/putimage_comparison_test.cpp
)

target_link_libraries(putimage_comparison_test
    test_framework
    xege
    ${SYSTEM_LIBS}
)

add_executable(putimage_alphablend_comprehensive_test
    tests/putimage_alphablend_comprehensive_test.cpp
)

target_link_libraries(putimage_alphablend_comprehensive_test
    test_framework
    xege
    ${SYSTEM_LIBS}
)

# 设置输出目录
set_target_properties(
    putimage_performance_test
    putimage_basic_test
    putimage_alphablend_test
    putimage_transparent_test
    putimage_rotate_test
    putimage_comparison_test
    putimage_alphablend_comprehensive_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 创建测试目标
enable_testing()

# 性能测试 (Performance Tests)
if(EGE_TEST_PERFORMANCE)
    add_test(NAME performance_putimage COMMAND putimage_performance_test)
    add_test(NAME performance_alphablend COMMAND putimage_alphablend_test)
    set_tests_properties(
        performance_putimage
        performance_alphablend
        PROPERTIES LABELS "performance"
    )
endif()

# 功能性测试 (Functional Tests)
if(EGE_TEST_FUNCTIONAL)
    add_test(NAME functional_putimage_basic COMMAND putimage_basic_test)
    add_test(NAME functional_putimage_transparent COMMAND putimage_transparent_test)
    add_test(NAME functional_putimage_rotate COMMAND putimage_rotate_test)
    add_test(NAME functional_putimage_comparison COMMAND putimage_comparison_test)
    add_test(NAME functional_putimage_alphablend_comprehensive COMMAND putimage_alphablend_comprehensive_test)
    set_tests_properties(
        functional_putimage_basic
        functional_putimage_transparent
        functional_putimage_rotate
        functional_putimage_comparison
        functional_putimage_alphablend_comprehensive
        PROPERTIES LABELS "functional"
    )
endif()

# 新增功能性测试

# 图形绘制基础功能测试
if(EGE_TEST_FUNCTIONAL)
    add_executable(drawing_primitives_test
        tests/drawing_primitives_test.cpp
    )
    target_link_libraries(drawing_primitives_test
        test_framework
        xege
        ${SYSTEM_LIBS}
    )
    set_target_properties(drawing_primitives_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME functional_drawing_primitives COMMAND drawing_primitives_test)
    set_tests_properties(functional_drawing_primitives PROPERTIES LABELS "functional")
endif()

# 颜色操作功能测试
if(EGE_TEST_FUNCTIONAL)
    add_executable(color_operations_test
        tests/color_operations_test.cpp
    )
    target_link_libraries(color_operations_test
        test_framework
        xege
        ${SYSTEM_LIBS}
    )
    set_target_properties(color_operations_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME functional_color_operations COMMAND color_operations_test)
    set_tests_properties(functional_color_operations PROPERTIES LABELS "functional")
endif()

# 图像管理功能测试
if(EGE_TEST_FUNCTIONAL)
    add_executable(image_management_test
        tests/image_management_test.cpp
    )
    target_link_libraries(image_management_test
        test_framework
        xege
        ${SYSTEM_LIBS}
    )
    set_target_properties(image_management_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME functional_image_management COMMAND image_management_test)
    set_tests_properties(functional_image_management PROPERTIES LABELS "functional")
endif()

# 窗口管理功能测试
if(EGE_TEST_FUNCTIONAL)
    add_executable(window_management_test
        tests/window_management_test.cpp
    )
    target_link_libraries(window_management_test
        test_framework
        xege
        ${SYSTEM_LIBS}
    )
    set_target_properties(window_management_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME functional_window_management COMMAND window_management_test)
    set_tests_properties(functional_window_management PROPERTIES LABELS "functional")
endif()

# 新增性能测试

# 图形绘制性能测试
if(EGE_TEST_PERFORMANCE)
    add_executable(drawing_performance_test
        tests/drawing_performance_test.cpp
    )
    target_link_libraries(drawing_performance_test
        test_framework
        xege
        ${SYSTEM_LIBS}
    )
    set_target_properties(drawing_performance_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME performance_drawing COMMAND drawing_performance_test)
    set_tests_properties(performance_drawing PROPERTIES LABELS "performance")
endif()

# 像素操作性能测试
if(EGE_TEST_PERFORMANCE)
    add_executable(pixel_operations_performance_test
        tests/pixel_operations_performance_test.cpp
    )
    target_link_libraries(pixel_operations_performance_test
        test_framework
        xege
        ${SYSTEM_LIBS}
    )
    set_target_properties(pixel_operations_performance_test
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    add_test(NAME performance_pixel_operations COMMAND pixel_operations_performance_test)
    set_tests_properties(performance_pixel_operations PROPERTIES LABELS "performance")
endif()
