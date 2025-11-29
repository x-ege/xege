cmake_minimum_required(VERSION 3.13)

# 当 CMakeLists.txt 是顶层项目的 CMakeLists.txt 时，设置默认选项。
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    set(EGE_IS_ROOT_PROJECT ON)
endif()

if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.13)
    cmake_policy(VERSION ${CMAKE_POLICY_VERSION_MINIMUM})
endif()

# Clear CMake cache
option(EGE_CLEAR_OPTIONS_CACHE "Clear caches before setting options" OFF)

if(EGE_CLEAR_OPTIONS_CACHE AND EGE_IS_ROOT_PROJECT)
    unset(EGE_BUILD_DEMO CACHE)
    unset(EGE_BUILD_TEST CACHE)
    unset(EGE_BUILD_TEMP CACHE)
endif()

# Options
option(EGE_BUILD_DEMO "Build ege demos" ${EGE_IS_ROOT_PROJECT})
option(EGE_BUILD_TEST "Build ege tests" ${EGE_IS_ROOT_PROJECT})
option(EGE_BUILD_TEMP "Build ege temp" ${EGE_IS_ROOT_PROJECT})

message(STATUS "EGE_IS_ROOT_PROJECT: ${EGE_IS_ROOT_PROJECT}")
message(STATUS "EGE_CLEAR_OPTIONS_CACHE: ${EGE_CLEAR_OPTIONS_CACHE}")
message(STATUS "EGE_BUILD_DEMO:  ${EGE_BUILD_DEMO}")
message(STATUS "EGE_BUILD_TEST:  ${EGE_BUILD_TEST}")
message(STATUS "EGE_BUILD_TEMP:  ${EGE_BUILD_TEMP}")

if(CMAKE_HOST_UNIX)
    set(CMAKE_SYSTEM_NAME Windows)
    set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

    # cross compilers to use for C, C++ and Fortran
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
    set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)
    set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

    # target environment on the build host system
    set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

    # modify default behavior of FIND_XXX() commands
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(PKG_CONFIG_EXECUTABLE ${TOOLCHAIN_PREFIX}-pkg-config)

    # EGE 交叉编译的时候, 实际上是 WINE 环境, 这里标记一下, 一些代码需要额外处理.
    set(EGE_NON_WINDOWS ON CACHE BOOL "EGE is not compiled on Windows" FORCE)
endif()

# 检查并启用 ccache（当 EGE_IS_ROOT_PROJECT 为 ON 且不是 Visual Studio generator 时）
if(EGE_IS_ROOT_PROJECT AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    find_program(CCACHE_FOUND ccache)

    if(CCACHE_FOUND)
        set(CMAKE_C_COMPILER_LAUNCHER ccache)
        set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
        message(STATUS "ccache found and enabled")
    else()
        message(STATUS "ccache not found")
    endif()
endif()

project(XEGE)

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT MSVC)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (defaulted by EGE when not provided).")
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
elseif(NOT MSVC)
    message(STATUS "CMAKE_BUILD_TYPE is set to ${CMAKE_BUILD_TYPE}")
endif()

# 检查编译器是否支持 C++17, 支持的话， 启用相机模块.
include(CheckCXXSourceCompiles)

if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "/std:c++17")
else()
    set(CMAKE_REQUIRED_FLAGS "-std=c++17")
endif()

check_cxx_source_compiles("
    #include <optional>
    int main() { std::optional<int> o; return 0; }
" COMPILER_SUPPORTS_CXX17)
unset(CMAKE_REQUIRED_FLAGS)

if(COMPILER_SUPPORTS_CXX17)
    option(EGE_ENABLE_CPP17 "Enable C++ 17" ON)

    set(EGE_ENABLE_CAMERA_CAPTURE_DEFAULT ON)
    option(EGE_ENABLE_CAMERA_CAPTURE "Enable camera capture" ${EGE_ENABLE_CAMERA_CAPTURE_DEFAULT})
    message(STATUS "EGE_ENABLE_CAMERA_CAPTURE: ${EGE_ENABLE_CAMERA_CAPTURE}")
else()
    set(EGE_ENABLE_CPP17 OFF CACHE BOOL "Enable C++ 17" FORCE)
    set(EGE_ENABLE_CAMERA_CAPTURE OFF CACHE BOOL "Enable camera capture" FORCE)
endif()

message(STATUS "EGE enable C++ 17: ${EGE_ENABLE_CPP17}")

if(EGE_ENABLE_CPP17)
    # 将编译标准设置为 C++17, 仓库对外标准保持不变, 未引入额外的依赖.
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

file(GLOB EGE_CPP_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(xege STATIC ${EGE_CPP_SRC})

target_include_directories(xege PUBLIC include)

# 自动检查并拉取缺失的子模块
if(EXISTS "${PROJECT_SOURCE_DIR}/.gitmodules")
    message(STATUS "Submodule checking...")
    file(READ "${PROJECT_SOURCE_DIR}/.gitmodules" GITMODULES_CONTENT)
    string(REGEX MATCHALL "path[ \t]*=[ \t]*([^\n\r]+)" SUBMODULE_PATHS "${GITMODULES_CONTENT}")
    find_package(Git QUIET)

    set(SUBMODULE_UNABLE_TO_UPDATE OFF)

    foreach(SUBMODULE_LINE ${SUBMODULE_PATHS})
        string(REGEX REPLACE "path *= *" "" SUBMODULE_PATH ${SUBMODULE_LINE})
        string(STRIP "${SUBMODULE_PATH}" SUBMODULE_PATH)
        set(SUBMODULE_DIR "${PROJECT_SOURCE_DIR}/${SUBMODULE_PATH}")

        if(NOT EXISTS "${SUBMODULE_DIR}/.git")
            message(STATUS "Submodule '${SUBMODULE_PATH}' not found, updating...")

            if (Git_FOUND)
                execute_process(
                    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive ${SUBMODULE_PATH}
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    RESULT_VARIABLE result_submodule
                    TIMEOUT 120
                )
                if(NOT result_submodule EQUAL 0)
                    set(SUBMODULE_UNABLE_TO_UPDATE ON)
                    message(WARNING "Failed to update submodule: ${SUBMODULE_PATH}")
                endif()
            else()
                set(SUBMODULE_UNABLE_TO_UPDATE ON)
                message(WARNING "Git is not installed, unable to update submodule: ${SUBMODULE_PATH};")
            endif()
        else()
            message(STATUS "Submodule '${SUBMODULE_PATH}' already exists.")
        endif()

    endforeach()

    # 检查到有未拉取的子模块但无法更新，通知用户自行拉取
    if (SUBMODULE_UNABLE_TO_UPDATE)
        message(WARNING "Unable to update submodule, please manually execute: git submodule update --init --recursive")
    endif()

endif()

# 将3rdparty 添加到包含目录
if(EXISTS "${PROJECT_SOURCE_DIR}/.gitmodules")
    target_include_directories(xege PRIVATE 3rdparty)
endif()

if(EGE_ENABLE_CPP17)
    # 启用 C++17 特性
    target_compile_features(xege PRIVATE cxx_std_17)
    target_compile_definitions(xege PRIVATE EGE_ENABLE_CPP17=1)
endif()

if(EGE_ENABLE_CAMERA_CAPTURE)
    include(CheckCXXCompilerFlag)

    # 检查是否支持AVX2
    if(MSVC)
        check_cxx_compiler_flag("/arch:AVX2" CCAP_SUPPORTS_AVX2)
    else()
        check_cxx_compiler_flag("-mavx2" CCAP_SUPPORTS_AVX2)
    endif()

    if(CCAP_SUPPORTS_AVX2)
        set(CCAP_CONVERT_AVX2_SRC ${PROJECT_SOURCE_DIR}/3rdparty/ccap/src/ccap_convert_avx2.cpp)

        if(MSVC)
            set_source_files_properties(${CCAP_CONVERT_AVX2_SRC} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
        else()
            # GCC/Clang/MinGW need these options to enable AVX2
            set_source_files_properties(${CCAP_CONVERT_AVX2_SRC} PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
        endif()
    else()
        target_compile_definitions(xege PRIVATE ENABLE_AVX2_IMP=0)
    endif()

    # 启用相机相关功能
    # 不用 submodule, 直接引入源码, 对齐 ege 的编译条件
    target_include_directories(xege PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/ccap/include)
    file(GLOB CCAP_SOURCE ${PROJECT_SOURCE_DIR}/3rdparty/ccap/src/*.cpp)
    target_sources(xege PRIVATE ${CCAP_SOURCE})
    target_compile_definitions(xege PRIVATE EGE_ENABLE_CAMERA_CAPTURE=1)
else()
    message(WARNING "Camera capture is disabled because your compiler does not support C++17.")
endif()

# 设置库文件名
# MSVC 下 Debug 版本使用 graphicsd.lib，Release 版本使用 graphics.lib
# MinGW 不区分 Debug/Release，统一使用 graphics
if(MSVC)
    set_target_properties(xege PROPERTIES
        OUTPUT_NAME_DEBUG graphicsd
        OUTPUT_NAME_RELEASE graphics
        OUTPUT_NAME_RELWITHDEBINFO graphics
        OUTPUT_NAME_MINSIZEREL graphics
    )
else()
    set_property(TARGET xege PROPERTY OUTPUT_NAME graphics)
endif()

if(EGE_NON_WINDOWS)
    target_compile_definitions(xege PUBLIC EGE_NON_WINDOWS=1)
endif()

target_compile_definitions(xege PUBLIC
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG=1>
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Release>:NDEBUG=1>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG=1>
    $<$<CONFIG:MinSizeRel>:NDEBUG=1>
)

target_compile_definitions(xege PRIVATE EGE_GRAPH_LIB_BUILD=1)

if(MSVC)
    message(STATUS "MSVC target: MSVC ${MSVC_VERSION}")

    # MSVC 2015 (v140) 及以上版本支持 /utf-8 选项
    # MSVC 2010 (v100) 到 MSVC 2013 (v120) 不支持 /utf-8
    if(MSVC_VERSION GREATER_EQUAL 1900) # MSVC 2015 及以上
        target_compile_options(xege PRIVATE
            /utf-8
            /MP
            /Zl)
    else() # MSVC 2010-2013
        # 对于老版本 MSVC，脚本单独处理源码文件(加BOM)
        target_compile_options(xege PRIVATE
            /MP
            /Zl)
    endif()

    target_compile_options(xege PUBLIC
        /DNOMINMAX=1
        /D_CRT_SECURE_NO_WARNINGS=1
        /Zc:__cplusplus
    )

    # EGE 对外仅发布 Release 版本
    # 对外会保障内存分配的匹配性.
    # 这些选项仅在编译 ege 内部生效
    target_compile_definitions(xege PRIVATE
        _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH=1
        _ALLOW_RUNTIME_LIBRARY_MISMATCH=1
        _CRT_NON_CONFORMING_SWPRINTFS=1
        _CRT_SECURE_NO_WARNINGS=1
        _CRT_SECURE_NO_DEPRECATE=1
    )

    if(MSVC_VERSION EQUAL 1200)
        # VC6 默认编译单线程版本（/ML(d)），无法找到 `_imp__xxx` 的定义
        # 而 CMake 生成的则使用 /MD，在此覆盖设置。
        # 编译时会有 D4025 警告，不用在意。
        target_compile_options(xege
            PRIVATE $<$<CONFIG:Debug>:/MLd> $<$<CONFIG:Release>:/ML>
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "GNU target: GCC ${CMAKE_C_COMPILER_VERSION}")

    if (DEFINED EGE_CRT_COMPAT_DEVCPP)
        message(STATUS "Using Dev-C++ compatibility mode.")
        target_compile_definitions(xege PRIVATE
            _GLIBCXX_USE_CXX11_ABI=0
            EGE_CRT_COMPAT_DEVCPP=1
        )
    endif()

    if(CMAKE_HOST_UNIX)
        target_compile_definitions(xege PRIVATE _FORTIFY_SOURCE=0)
    endif()
else()
    if(CMAKE_CXX_COMPILER_ID)
        message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
    else()
        message(FATAL_ERROR "Unsupported compiler: ${CMAKE_C_COMPILER}")
    endif()
endif()

target_link_libraries(xege INTERFACE
    gdiplus
    gdi32
)

if(EGE_BUILD_DEMO)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demo/CMakeLists.txt")
        message(STATUS "Building EGE demo")
        add_subdirectory(demo)
    else()
        message("EGE demo not found, skipping")
    endif()
endif()

if(EGE_BUILD_TEST)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message("EGE test not found, skipping")
    endif()
endif()

if(EGE_BUILD_TEMP)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/temp/CMakeLists.txt")
        add_subdirectory(temp)
    else()
        message("EGE temp not found, skipping")
    endif()
endif()
