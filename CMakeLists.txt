cmake_minimum_required(VERSION 3.13)

# 当 CMakeLists.txt 是顶层项目的 CMakeLists.txt 时，设置默认选项。
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    set(EGE_IS_ROOT_PROJECT ON)
endif()

if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.13)
    cmake_policy(VERSION ${CMAKE_POLICY_VERSION_MINIMUM})
endif()

# Developer-only local overrides (not tracked by git).
# Create a ./dev.cmake next to this CMakeLists.txt to preset cache options
# (e.g. force EGE_BUILD_OPENGL=ON for local development).
if(EGE_IS_ROOT_PROJECT AND EXISTS "${CMAKE_SOURCE_DIR}/dev.cmake")
    message(STATUS "Including developer overrides: ${CMAKE_SOURCE_DIR}/dev.cmake")
    include("${CMAKE_SOURCE_DIR}/dev.cmake")
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

# EGE_DISABLE_DEBUG_INFO 选项：禁用调试信息，避免 LNK4099 警告
# 优先使用 CMake 参数，其次检查环境变量，默认 OFF
if(NOT DEFINED EGE_DISABLE_DEBUG_INFO)
    # CMake 参数未定义，检查环境变量
    if(DEFINED ENV{EGE_DISABLE_DEBUG_INFO})
        set(_env_value "$ENV{EGE_DISABLE_DEBUG_INFO}")
        # 检查环境变量是否为真值 (1, true, TRUE, on, ON, yes, YES)
        if(_env_value MATCHES "^(1|[Tt][Rr][Uu][Ee]|[Oo][Nn]|[Yy][Ee][Ss])$")
            set(EGE_DISABLE_DEBUG_INFO ON CACHE BOOL "Disable debug info to avoid LNK4099 warnings")
        else()
            set(EGE_DISABLE_DEBUG_INFO OFF CACHE BOOL "Disable debug info to avoid LNK4099 warnings")
        endif()
        unset(_env_value)
    else()
        set(EGE_DISABLE_DEBUG_INFO OFF CACHE BOOL "Disable debug info to avoid LNK4099 warnings")
    endif()
endif()

message(STATUS "EGE_IS_ROOT_PROJECT: ${EGE_IS_ROOT_PROJECT}")
message(STATUS "EGE_CLEAR_OPTIONS_CACHE: ${EGE_CLEAR_OPTIONS_CACHE}")
message(STATUS "EGE_BUILD_DEMO:  ${EGE_BUILD_DEMO}")
message(STATUS "EGE_BUILD_TEST:  ${EGE_BUILD_TEST}")
message(STATUS "EGE_BUILD_TEMP:  ${EGE_BUILD_TEMP}")
message(STATUS "EGE_DISABLE_DEBUG_INFO: ${EGE_DISABLE_DEBUG_INFO}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

# ============================================================================
# Backend Selection (macOS/Linux)
# ============================================================================
# Two mutually exclusive paths on Unix systems:
#
# 1. OPENGL + NATIVE (Recommended, default on macOS/Linux)
#    - Uses native compiler (AppleClang on macOS, GCC on Linux)
#    - Builds native executable for the host platform
#    - Requires: GLFW, OpenGL
#
# 2. MINGW + GDI (Legacy, deprecated)
#    - Uses mingw-w64 cross-compiler
#    - Builds Windows PE executable (.exe)
#    - Requires: mingw-w64, wine (to run)
#
# These two paths CANNOT be mixed in a single build.
# ============================================================================

# Default backend selection
if(CMAKE_HOST_UNIX)
    # macOS/Linux: Default to OpenGL native
    option(EGE_BUILD_OPENGL "Build with OpenGL backend (native on macOS/Linux)" ON)
else()
    # Windows: Default to GDI, OpenGL is opt-in
    option(EGE_BUILD_OPENGL "Build with OpenGL backend (Windows: opt-in via INIT_OPENGL)" OFF)
endif()
message(STATUS "EGE_BUILD_OPENGL: ${EGE_BUILD_OPENGL}")

# Derive EGE_BUILD_FOR_LINUX from EGE_BUILD_OPENGL on Unix
# STRICT ISOLATION: These two flags must be consistent
if(CMAKE_HOST_UNIX)
    if(EGE_BUILD_OPENGL)
        # OpenGL mode: MUST use native compiler
        set(EGE_BUILD_FOR_LINUX ON CACHE BOOL "Build for native Unix (OpenGL mode)" FORCE)
        message(STATUS "EGE_BACKEND: OpenGL + Native Compiler")
        message(STATUS "  Target: Native executable (${CMAKE_HOST_SYSTEM_NAME})")
    else()
        # GDI mode: MUST use mingw-w64 cross-compiler
        set(EGE_BUILD_FOR_LINUX OFF CACHE BOOL "Build for native Unix (OpenGL mode)" FORCE)
        message(WARNING "EGE DEPRECATION: mingw-w64 cross-compilation is deprecated")
        message(WARNING "  Use -DEGE_BUILD_OPENGL=ON for native OpenGL backend")
        message(STATUS "EGE_BACKEND: GDI + mingw-w64 (LEGACY)")
        message(STATUS "  Target: Windows PE executable (.exe)")
    endif()
else()
    # Windows: Use native compiler regardless of backend
    set(EGE_BUILD_FOR_LINUX OFF CACHE BOOL "Build for native Unix (OpenGL mode)" FORCE)
endif()
message(STATUS "EGE_BUILD_FOR_LINUX: ${EGE_BUILD_FOR_LINUX}")

# ============================================================================
# Compiler Configuration
# ============================================================================

# macOS/Linux with OpenGL: Use native compiler (already set by CMake)
# macOS/Linux with GDI (legacy): Use mingw-w64 cross-compiler
if(CMAKE_HOST_UNIX AND NOT EGE_BUILD_FOR_LINUX)
    message(STATUS "EGE legacy mode: configuring mingw-w64 cross-compiler")
    set(CMAKE_SYSTEM_NAME Windows)
    set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

    # Cross compilers
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
    set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)
    set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

    # Target environment
    set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

    # FindXXX behavior
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(PKG_CONFIG_EXECUTABLE ${TOOLCHAIN_PREFIX}-pkg-config)

    # Mark as non-Windows build environment
    set(EGE_NON_WINDOWS ON CACHE BOOL "EGE is cross-compiling from non-Windows" FORCE)
endif()

# ============================================================================
# Build Configuration
# ============================================================================

# 检查并启用 ccache
if(EGE_IS_ROOT_PROJECT AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set(CMAKE_C_COMPILER_LAUNCHER ccache)
        set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
        message(STATUS "ccache: enabled")
    else()
        message(STATUS "ccache: not found")
    endif()
endif()

project(XEGE)

# Build type
if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT MSVC)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type")
    message(STATUS "CMAKE_BUILD_TYPE: defaulting to Release")
else()
    message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

# ============================================================================
# Feature Detection
# ============================================================================

# C++17 support check (enables camera module)
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
    option(EGE_ENABLE_CPP17 "Enable C++17 features" ON)
    set(EGE_ENABLE_CAMERA_CAPTURE_DEFAULT ON)
    option(EGE_ENABLE_CAMERA_CAPTURE "Enable camera capture" ${EGE_ENABLE_CAMERA_CAPTURE_DEFAULT})
else()
    set(EGE_ENABLE_CPP17 OFF CACHE BOOL "Enable C++17 features" FORCE)
    set(EGE_ENABLE_CAMERA_CAPTURE OFF CACHE BOOL "Enable camera capture" FORCE)
endif()

message(STATUS "EGE_ENABLE_CPP17: ${EGE_ENABLE_CPP17}")
message(STATUS "EGE_ENABLE_CAMERA_CAPTURE: ${EGE_ENABLE_CAMERA_CAPTURE}")

# ============================================================================
# Source Files
# ============================================================================

file(GLOB EGE_CPP_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(xege STATIC ${EGE_CPP_SRC})

target_include_directories(xege PUBLIC include)

# Submodules
if(EXISTS "${PROJECT_SOURCE_DIR}/.gitmodules")
    message(STATUS "Checking submodules...")
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
            if(Git_FOUND)
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
                message(WARNING "Git not found, cannot update submodule: ${SUBMODULE_PATH}")
            endif()
        endif()
    endforeach()
    if(SUBMODULE_UNABLE_TO_UPDATE)
        message(WARNING "Please manually run: git submodule update --init --recursive")
    endif()
endif()

if(EXISTS "${PROJECT_SOURCE_DIR}/.gitmodules")
    target_include_directories(xege PRIVATE 3rdparty)
endif()

# ============================================================================
# Compiler Flags
# ============================================================================

if(EGE_ENABLE_CPP17)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    target_compile_features(xege PRIVATE cxx_std_17)
    target_compile_definitions(xege PRIVATE EGE_ENABLE_CPP17=1)
endif()

# Camera capture
if(EGE_ENABLE_CAMERA_CAPTURE)
    include(CheckCXXCompilerFlag)
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
            set_source_files_properties(${CCAP_CONVERT_AVX2_SRC} PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
        endif()
    else()
        target_compile_definitions(xege PRIVATE ENABLE_AVX2_IMP=0)
    endif()
    target_include_directories(xege PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/ccap/include)
    file(GLOB CCAP_SOURCE ${PROJECT_SOURCE_DIR}/3rdparty/ccap/src/*.cpp)
    target_sources(xege PRIVATE ${CCAP_SOURCE})
    target_compile_definitions(xege PRIVATE EGE_ENABLE_CAMERA_CAPTURE=1)
endif()

# Library name
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

# Compiler-specific flags
if(MSVC)
    message(STATUS "Compiler: MSVC ${MSVC_VERSION}")
    if(EGE_DISABLE_DEBUG_INFO)
        foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            string(REPLACE "/Zi" "" CMAKE_C_FLAGS_${_config} "${CMAKE_C_FLAGS_${_config}}")
            string(REPLACE "/Zi" "" CMAKE_CXX_FLAGS_${_config} "${CMAKE_CXX_FLAGS_${_config}}")
            string(REPLACE "/ZI" "" CMAKE_C_FLAGS_${_config} "${CMAKE_C_FLAGS_${_config}}")
            string(REPLACE "/ZI" "" CMAKE_CXX_FLAGS_${_config} "${CMAKE_CXX_FLAGS_${_config}}")
        endforeach()
    endif()
    if(MSVC_VERSION GREATER_EQUAL 1900)
        target_compile_options(xege PRIVATE /utf-8 /MP /Zl)
    else()
        target_compile_options(xege PRIVATE /MP /Zl)
    endif()
    target_compile_options(xege PUBLIC
        /DNOMINMAX=1
        /D_CRT_SECURE_NO_WARNINGS=1
        /Zc:__cplusplus
    )
    target_compile_definitions(xege PRIVATE
        _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH=1
        _ALLOW_RUNTIME_LIBRARY_MISMATCH=1
        _CRT_NON_CONFORMING_SWPRINTFS=1
        _CRT_SECURE_NO_WARNINGS=1
        _CRT_SECURE_NO_DEPRECATE=1
    )
    if(MSVC_VERSION EQUAL 1200)
        target_compile_options(xege PRIVATE $<$<CONFIG:Debug>:/MLd> $<$<CONFIG:Release>:/ML>)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Compiler: GCC ${CMAKE_C_COMPILER_VERSION}")
    if(DEFINED EGE_CRT_COMPAT_DEVCPP)
        target_compile_definitions(xege PRIVATE _GLIBCXX_USE_CXX11_ABI=0 EGE_CRT_COMPAT_DEVCPP=1)
    endif()
    if(CMAKE_HOST_UNIX)
        target_compile_definitions(xege PRIVATE _FORTIFY_SOURCE=0)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Compiler: Clang ${CMAKE_C_COMPILER_VERSION}")
    if(CMAKE_HOST_UNIX)
        target_compile_definitions(xege PRIVATE _FORTIFY_SOURCE=0)
    endif()
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# ============================================================================
# Backend Sources (MUTUALLY EXCLUSIVE)
# ============================================================================

# Windows GDI backend (for Windows native and mingw-w64 cross-compile)
if(WIN32 OR MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    file(GLOB EGE_BACKEND_WIN32_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/win32/*.cpp)
    target_sources(xege PRIVATE ${EGE_BACKEND_WIN32_SRC})
    target_link_libraries(xege INTERFACE
        $<$<PLATFORM_ID:Windows>:gdiplus>
        $<$<PLATFORM_ID:Windows>:gdi32>
        $<$<PLATFORM_ID:Windows>:imm32>
        $<$<PLATFORM_ID:Windows>:ole32>
        $<$<PLATFORM_ID:Windows>:oleaut32>
        $<$<PLATFORM_ID:Windows>:uuid>
        $<$<PLATFORM_ID:Windows>:winmm>
        $<$<PLATFORM_ID:Windows>:msimg32>
    )
endif()

# OpenGL backend (MUTUALLY EXCLUSIVE with GDI on macOS/Linux)
if(EGE_BUILD_OPENGL)
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(xege PRIVATE glfw OpenGL::GL)
    target_compile_definitions(xege PUBLIC EGE_BUILD_OPENGL=1)
    target_include_directories(xege PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/opengl)
    file(GLOB EGE_BACKEND_OPENGL_SRC CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/opengl/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/opengl/glad/*.cpp
    )
    target_sources(xege PRIVATE ${EGE_BACKEND_OPENGL_SRC})
    if(APPLE)
        target_compile_options(xege PRIVATE -Wno-deprecated-declarations)
    endif()
endif()

# ============================================================================
# Subdirectories
# ============================================================================

if(EGE_BUILD_DEMO AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demo/CMakeLists.txt")
    message(STATUS "Building demos")
    add_subdirectory(demo)
endif()

if(EGE_BUILD_TEST AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

if(EGE_BUILD_TEMP AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/temp/CMakeLists.txt")
    add_subdirectory(temp)
endif()

message(STATUS "EGE configuration complete")
message(STATUS "  Backend: $<IF:$<BOOL:${EGE_BUILD_OPENGL}>,OpenGL,GDI>")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
