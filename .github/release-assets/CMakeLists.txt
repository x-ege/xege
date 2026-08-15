cmake_minimum_required(VERSION 3.13)

if(CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS version supported by the packaged EGE library")
endif()

# Preserve the existing Linux SDK experience: when no explicit toolchain is
# supplied, a Linux host selects the packaged MinGW Windows target. Apple hosts
# are deliberately excluded because macOS now consumes the native archive.
if(CMAKE_HOST_UNIX AND NOT CMAKE_HOST_APPLE
        AND NOT DEFINED CMAKE_TOOLCHAIN_FILE
        AND NOT DEFINED CMAKE_SYSTEM_NAME)
    set(CMAKE_SYSTEM_NAME Windows)
    set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
    set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(PKG_CONFIG_EXECUTABLE ${TOOLCHAIN_PREFIX}-pkg-config)
endif()

project(ege-demos LANGUAGES CXX)

add_library(ege-common INTERFACE)
target_include_directories(ege-common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

message(STATUS "ege include dir: ${CMAKE_CURRENT_SOURCE_DIR}/include")

# Select the prebuilt library for the target platform. macOS is always native;
# on Linux the compatibility block above selects the packaged MinGW toolchain
# when the caller did not already provide one explicitly.
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
    set(osLibDir "macOS")
    add_library(ege-macos STATIC IMPORTED)
    set_target_properties(ege-macos PROPERTIES
        IMPORTED_LOCATION
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/${osLibDir}/libgraphics.a")
    target_link_libraries(ege-common INTERFACE ege-macos)

    foreach(framework
            AppKit CoreGraphics CoreText ImageIO Foundation AVFoundation
            CoreVideo CoreMedia Accelerate AVFAudio AudioToolbox)
        find_library(EGE_RELEASE_${framework}_FRAMEWORK NAMES ${framework})
        if(NOT EGE_RELEASE_${framework}_FRAMEWORK)
            message(FATAL_ERROR
                "Required macOS framework '${framework}' was not found")
        endif()
        target_link_libraries(ege-common INTERFACE
            "${EGE_RELEASE_${framework}_FRAMEWORK}")
    endforeach()
elseif(MSVC)
    # 设置 MSVC 编译选项, 当版本大于 vs2019 时, 使用 c++17 标准.
    set(CPP_COMPILE_OPTIONS "/std:c++17")

    if(MSVC_VERSION GREATER_EQUAL 1950)
        # VS2026 (speculative - MSVC_VERSION value not confirmed by Microsoft yet)
        # This is forward-looking support; will be updated when VS2026 is officially released
        # Falls back to VS2022 libraries which should be ABI compatible
        if(CMAKE_CL_64)
            set(osLibDir "vs2022/x64")
        else()
            set(osLibDir "vs2022/x86")
        endif()
        message(WARNING "Using VS2026 or newer detected. Using VS2022 libraries for compatibility.")
    elseif(MSVC_VERSION GREATER_EQUAL 1930)
        # vs2022 以上, 静态库是兼容的.
        if(CMAKE_CL_64)
            set(osLibDir "vs2022/x64")
        else()
            set(osLibDir "vs2022/x86")
        endif()
    elseif(MSVC_VERSION GREATER_EQUAL 1920)
        # vs2019 以上, 静态库是兼容的.
        if(CMAKE_CL_64)
            set(osLibDir "vs2019/x64")
        else()
            set(osLibDir "vs2019/x86")
        endif()
    else()
        message(FATAL_ERROR
            "This release package contains MSVC libraries for VS2019 and "
            "VS2022 only. Build XEGE from source when using an older compiler.")
    endif()

    # /Zc:__cplusplus 选项仅在 VS2017 15.7 (MSVC_VERSION >= 1914) 及以上版本支持
    if(MSVC_VERSION GREATER_EQUAL 1914)
        set(MSVC_CXXPLUSPLUS_FLAG "/Zc:__cplusplus")
    endif()

    target_compile_options(ege-common INTERFACE
        /source-charset:utf-8
        /MP
        /D__STDC_LIMIT_MACROS
        /D_USE_MATH_DEFINES
        ${MSVC_CXXPLUSPLUS_FLAG}
        ${CPP_COMPILE_OPTIONS}
        "$<$<CONFIG:DEBUG>:/DDEBUG>"
        "$<$<CONFIG:RELEASE>:/DNDEBUG>"
        "$<$<CONFIG:RELWITHDEBINFO>:/DNDEBUG>"
        "$<$<CONFIG:MINSIZEREL>:/DNDEBUG>"
        "$<IF:$<CONFIG:Debug>,/MDd,/MD>")
elseif(MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_HOST_APPLE)
        message(FATAL_ERROR
            "The macOS MinGW prebuilt package was retired. Build natively with "
            "AppleClang, or use an older EGE release for that workflow.")
    elseif(CMAKE_HOST_UNIX)
        set(osLibDir "mingw-w64-debian")
    else()
        # Note: Automatic detection of MinGW variant (MSYS2/Code::Blocks/RedPanda) is not reliable
        # Default to mingw64 (MSYS2). Users should download the package matching their compiler.
        # Available variants: mingw64 (MSYS2), codeblocks, redpanda (CLion/小熊猫C++)
        # To use a specific variant, use the matching release package
        set(osLibDir "mingw64")
        message(STATUS "Using default MinGW libraries (mingw64). Ensure you downloaded the correct package for your compiler.")
    endif()

    target_compile_options(ege-common INTERFACE -D_FORTIFY_SOURCE=0)
    target_link_options(ege-common INTERFACE -mwindows -static -static-libgcc -static-libstdc++)
    target_link_libraries(ege-common INTERFACE graphics gdiplus gdi32 imm32 msimg32 ole32 oleaut32 winmm uuid)
else()
    message(FATAL_ERROR
        "No prebuilt EGE package is available for ${CMAKE_SYSTEM_NAME} / "
        "${CMAKE_CXX_COMPILER_ID}. Linux packages require MinGW-w64; macOS "
        "packages require AppleClang.")
endif()

if(DEFINED osLibDir)
    message(STATUS "ege library dir: ${CMAKE_CURRENT_SOURCE_DIR}/lib/${osLibDir}")
    target_link_directories(ege-common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/${osLibDir})
endif()

file(GLOB DEMO_SRCS
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/demo/*.cpp")

message(STATUS "DEMO_SRCS: ${DEMO_SRCS}")

add_custom_target(demos)

foreach(DEMO_SRC ${DEMO_SRCS})
    message(STATUS "DEMO_SRC: ${DEMO_SRC}")
    get_filename_component(DEMO_NAME ${DEMO_SRC} NAME_WE)

    if(DEMO_NAME STREQUAL "graph_star" AND NOT WIN32)
        message(STATUS "Skipping ${DEMO_NAME}: Windows screensaver demo")
        continue()
    endif()

    add_executable(${DEMO_NAME} ${DEMO_SRC})
    target_link_libraries(${DEMO_NAME} ege-common)
    add_dependencies(demos ${DEMO_NAME})

    if(DEMO_NAME STREQUAL "game_cube_runner")
        target_compile_features(${DEMO_NAME} PRIVATE cxx_std_17)
    elseif(DEMO_NAME MATCHES "^camera_")
        target_compile_features(${DEMO_NAME} PRIVATE cxx_std_17)
        target_compile_definitions(${DEMO_NAME} PRIVATE
            EGE_ENABLE_CAMERA_CAPTURE=1)
        if(APPLE)
            target_link_options(${DEMO_NAME} PRIVATE
                "-sectcreate" "__TEXT" "__info_plist"
                "${CMAKE_CURRENT_SOURCE_DIR}/demo/macos-camera-info.plist")
        endif()
    endif()
endforeach()

if(WIN32)
    target_sources(graph_rotateimage PRIVATE demo/egelogo.rc)
endif()

message(STATUS "Finding GMP Library")
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_LIBRARIES gmpxx gmp)
check_cxx_source_compiles("
#include <gmpxx.h>
int main() {
    mpz_class a;
    return 0;
}
" GMP_AVAILABLE)
unset(CMAKE_REQUIRED_LIBRARIES)

if(GMP_AVAILABLE)
    message(STATUS "Finding GMP Library - found")
    file(
        GLOB GMP_DEMO_SRCS
        LIST_DIRECTORIES false
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/demo/gmp-demo/*.cpp"
    )

    foreach(GMP_DEMO_SRC ${GMP_DEMO_SRCS})
        message(STATUS "GMP_DEMO_SRC: ${GMP_DEMO_SRC}")
        get_filename_component(DEMO_NAME ${GMP_DEMO_SRC} NAME_WE)
        add_executable(${DEMO_NAME} ${GMP_DEMO_SRC})
        target_link_libraries(${DEMO_NAME} gmpxx gmp ege-common)
        add_dependencies(demos ${DEMO_NAME})
    endforeach()
else()
    message(STATUS "Finding GMP Library - not found")
endif()
