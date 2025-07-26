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

project(XEGE)

# 检查并启用 ccache（当 EGE_IS_ROOT_PROJECT 为 ON 且不是 MSVC 时）
if(EGE_IS_ROOT_PROJECT AND NOT MSVC)
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set(CMAKE_C_COMPILER_LAUNCHER ccache)
        set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
        message(STATUS "ccache found and enabled")
    else()
        message(STATUS "ccache not found")
    endif()
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT MSVC)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
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
else()
    set(EGE_ENABLE_CPP17 OFF CACHE BOOL "Enable C++ 17" FORCE)
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

# 是否使用 libpng 和 zlib 处理 PNG 图片。
# 目前只能为 ON，未来或允许选择使用 WIC。
option(USE_LIBPNG_AND_ZLIB "Use libpng and zlib to process PNG images" ON)

# 是否将 libpng 和 zlib 合并入最终的 EGE 静态库
# 如果为 OFF，将寻找已有的库进行编译
option(MERGE_LIBPNG_AND_ZLIB "Merge libpng and zlib into EGE library" ON)

if(USE_LIBPNG_AND_ZLIB)
    set(EGE_ZLIB_PATH_SUBFIX 3rdparty/zlib)
    set(EGE_ZLIB_SOURCE_DIR ${PROJECT_SOURCE_DIR}/${EGE_ZLIB_PATH_SUBFIX})
    set(EGE_ZLIB_BINARY_DIR ${PROJECT_BINARY_DIR}/${EGE_ZLIB_PATH_SUBFIX})
    set(EGE_LIBPNG_PATH_SUBFIX 3rdparty/libpng)
    set(EGE_LIBPNG_SOURCE_DIR ${PROJECT_SOURCE_DIR}/${EGE_LIBPNG_PATH_SUBFIX})
    set(EGE_LIBPNG_BINARY_DIR ${PROJECT_BINARY_DIR}/${EGE_LIBPNG_PATH_SUBFIX})

    if(MERGE_LIBPNG_AND_ZLIB)
        # 如果子目录下没有源代码，则自动更新 git submodule
        if(
            NOT EXISTS ${EGE_ZLIB_SOURCE_DIR}/CMakeLists.txt OR
            NOT EXISTS ${EGE_LIBPNG_SOURCE_DIR}/CMakeLists.txt
        )
            find_package(Git REQUIRED)
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive)
        endif()

        set(ZLIB_BUILD_EXAMPLES OFF CACHE INTERNAL "")

        add_subdirectory(${EGE_ZLIB_SOURCE_DIR} EXCLUDE_FROM_ALL)

        set(PNG_BUILD_ZLIB ON CACHE INTERNAL "")
        set(PNG_SHARED OFF CACHE INTERNAL "")
        set(PNG_STATIC ON CACHE INTERNAL "")
        set(PNG_TESTS OFF CACHE INTERNAL "")

        set(ZLIB_INCLUDE_DIR ${EGE_ZLIB_BINARY_DIR} ${EGE_ZLIB_SOURCE_DIR})
        add_subdirectory(${EGE_LIBPNG_SOURCE_DIR} EXCLUDE_FROM_ALL)

        # zlib 的 CMake 执行过程会重命名其 zconf.h 文件，这里改回来
        if(EXISTS ${EGE_ZLIB_SOURCE_DIR}/zconf.h.included)
            file(RENAME ${EGE_ZLIB_SOURCE_DIR}/zconf.h.included ${EGE_ZLIB_SOURCE_DIR}/zconf.h)
        endif()

        if(MSVC)
            target_compile_options(png_static PRIVATE
                /MP
                /MT
                /Zl)
            target_compile_options(zlibstatic PRIVATE
                /MP
                /MT
                /Zl)
        else()
            target_compile_options(png_static PRIVATE
                -fPIC -fno-exceptions)
            target_compile_options(zlibstatic PRIVATE
                -fPIC -fno-exceptions)
            target_compile_options(xege PRIVATE
                -fPIC -fno-exceptions)
        endif()

        target_include_directories(xege
            PRIVATE ${EGE_ZLIB_BINARY_DIR}
            PRIVATE ${EGE_ZLIB_SOURCE_DIR}
        )
        target_include_directories(xege
            PRIVATE ${EGE_LIBPNG_BINARY_DIR}
            PRIVATE ${EGE_LIBPNG_SOURCE_DIR}
        )

        target_sources(xege
            PRIVATE $<TARGET_OBJECTS:png_static>
            PRIVATE $<TARGET_OBJECTS:zlibstatic>
        )
        target_include_directories(png_static
            PRIVATE ${EGE_ZLIB_BINARY_DIR}
            PRIVATE ${EGE_ZLIB_SOURCE_DIR}
        )
    else()
        find_package(ZLIB REQUIRED)
        find_package(PNG REQUIRED)
        target_link_libraries(xege
            PRIVATE ZLIB::ZLIB
            PRIVATE PNG::PNG
        )
    endif()
else()
    message(FATAL_ERROR "EGE MUST BE COMPILED WITH LIBPNG AND ZLIB!")
endif()

if(EGE_ENABLE_CPP17)
    # 启用 C++17 特性
    target_compile_features(xege PRIVATE cxx_std_17)
    target_compile_definitions(xege PRIVATE EGE_ENABLE_CPP17=1)
endif()

# 设置库文件名
set_property(TARGET xege PROPERTY OUTPUT_NAME graphics)

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

if(MSVC)
    message(STATUS "MSVC target: MSVC ${MSVC_VERSION}")

    # MSVC 2015 (v140) 及以上版本支持 /utf-8 选项
    # MSVC 2010 (v100) 到 MSVC 2013 (v120) 不支持 /utf-8
    if(MSVC_VERSION GREATER_EQUAL 1900) # MSVC 2015 及以上
        target_compile_options(xege PRIVATE
            /utf-8
            /MP
            /MT
            /Zl)
    else() # MSVC 2010-2013
        # 对于老版本 MSVC，脚本单独处理源码文件(加BOM)
        target_compile_options(xege PRIVATE
            /MP
            /MT
            /Zl)
    endif()

    target_compile_options(xege PUBLIC
        /DNOMINMAX=1
        /D_CRT_SECURE_NO_WARNINGS=1
        /Zc:__cplusplus
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

    if(CMAKE_HOST_UNIX)
        target_link_options(xege PUBLIC -mwindows -static)
        target_compile_definitions(xege PUBLIC _FORTIFY_SOURCE=0)
    else()
        target_link_options(xege PUBLIC -mwindows)
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