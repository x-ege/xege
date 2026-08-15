cmake_minimum_required(VERSION 3.13)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(EGE_IS_ROOT_PROJECT ON)
else()
    set(EGE_IS_ROOT_PROJECT OFF)
endif()

if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.13)
endif()
cmake_policy(VERSION ${CMAKE_POLICY_VERSION_MINIMUM})

# Keep native Apple builds compatible with the oldest supported macOS release.
# This must be set before project()/enable_language(), otherwise objects inherit
# the SDK version of the (rolling) build machine as their deployment target.
# Explicit caller values always win. Cross-compilation toolchains are excluded.
if(CMAKE_HOST_APPLE
        AND NOT CMAKE_TOOLCHAIN_FILE
        AND (NOT DEFINED CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS version supported by native XEGE builds")
endif()

# Cross-compilers belong in a toolchain file and must be selected before this
# project() call.  In particular, a Unix host no longer implies a Windows
# target; CMAKE_SYSTEM_NAME always describes the actual target platform.
project(XEGE LANGUAGES C CXX)

option(EGE_CLEAR_OPTIONS_CACHE "Clear XEGE option caches before configuring" OFF)
if(EGE_CLEAR_OPTIONS_CACHE AND EGE_IS_ROOT_PROJECT)
    foreach(_ege_cached_option
            EGE_BUILD_DEMO
            EGE_BUILD_TEST
            EGE_ENABLE_WINDOW_TESTS
            EGE_BUILD_TEMP
            EGE_DEFAULT_BACKEND
            EGE_ENABLE_OPENGL
            EGE_ENABLE_CAMERA_CAPTURE
            EGE_ENABLE_CPP17
            EGE_DISABLE_DEBUG_INFO)
        unset(${_ege_cached_option} CACHE)
    endforeach()
    # Treat this cache reset as a one-shot request instead of leaving every
    # future configure in reset mode.
    set(EGE_CLEAR_OPTIONS_CACHE OFF CACHE BOOL
        "Clear XEGE option caches before configuring" FORCE)
endif()

option(EGE_BUILD_DEMO "Build EGE demos" ${EGE_IS_ROOT_PROJECT})
option(EGE_BUILD_TEST "Build EGE tests" ${EGE_IS_ROOT_PROJECT})
option(EGE_ENABLE_WINDOW_TESTS
    "Register tests that create visible native windows (never enabled by default)" OFF)
option(EGE_BUILD_TEMP "Build EGE temporary programs" ${EGE_IS_ROOT_PROJECT})

if(NOT DEFINED EGE_DISABLE_DEBUG_INFO)
    if(DEFINED ENV{EGE_DISABLE_DEBUG_INFO}
            AND "$ENV{EGE_DISABLE_DEBUG_INFO}" MATCHES
                "^(1|[Tt][Rr][Uu][Ee]|[Oo][Nn]|[Yy][Ee][Ss])$")
        set(_ege_disable_debug_info_default ON)
    else()
        set(_ege_disable_debug_info_default OFF)
    endif()
    set(EGE_DISABLE_DEBUG_INFO ${_ege_disable_debug_info_default} CACHE BOOL
        "Disable debug information to avoid MSVC LNK4099 warnings")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EgeBackends)
include(EgeSources)

# project() creates an empty CMAKE_BUILD_TYPE cache entry for single-config
# generators.  Multi-config generators intentionally keep it empty.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# ccache is a developer convenience, never a target-platform decision.
if(EGE_IS_ROOT_PROJECT AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    find_program(EGE_CCACHE_PROGRAM ccache)
    if(EGE_CCACHE_PROGRAM)
        execute_process(
            COMMAND "${EGE_CCACHE_PROGRAM}" --version
            RESULT_VARIABLE _ege_ccache_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
        if(_ege_ccache_result EQUAL 0)
            set(CMAKE_C_COMPILER_LAUNCHER "${EGE_CCACHE_PROGRAM}")
            set(CMAKE_CXX_COMPILER_LAUNCHER "${EGE_CCACHE_PROGRAM}")
        else()
            message(WARNING
                "ccache was found at ${EGE_CCACHE_PROGRAM}, but it cannot run; "
                "building without a compiler launcher")
            unset(CMAKE_C_COMPILER_LAUNCHER)
            unset(CMAKE_CXX_COMPILER_LAUNCHER)
        endif()
    endif()
endif()

include(CheckCXXSourceCompiles)
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "/std:c++17")
else()
    set(CMAKE_REQUIRED_FLAGS "-std=c++17")
endif()
check_cxx_source_compiles("#include <optional>
int main() { std::optional<int> value; return value.has_value(); }"
    COMPILER_SUPPORTS_CXX17)
unset(CMAKE_REQUIRED_FLAGS)

if(COMPILER_SUPPORTS_CXX17)
    option(EGE_ENABLE_CPP17 "Enable C++17 features" ON)
else()
    set(EGE_ENABLE_CPP17 OFF CACHE BOOL "Enable C++17 features" FORCE)
endif()

# CMake configuration must never mutate the source checkout or access the
# network.  A recursive clone keeps camera support enabled by default; a clone
# without ccap remains configurable and prints the exact recovery command.
set(_ege_ccap_ready OFF)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ccap/include/ccap.h"
        AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ccap/src/ccap_core.cpp")
    set(_ege_ccap_ready ON)
endif()

if(COMPILER_SUPPORTS_CXX17 AND _ege_ccap_ready)
    set(_ege_camera_default ON)
else()
    set(_ege_camera_default OFF)
endif()
option(EGE_ENABLE_CAMERA_CAPTURE "Enable camera capture" ${_ege_camera_default})

if(EGE_ENABLE_CAMERA_CAPTURE AND NOT COMPILER_SUPPORTS_CXX17)
    message(FATAL_ERROR "EGE camera capture requires a C++17 compiler.")
elseif(EGE_ENABLE_CAMERA_CAPTURE AND NOT _ege_ccap_ready)
    message(FATAL_ERROR
        "EGE_ENABLE_CAMERA_CAPTURE=ON requires the ccap submodule. "
        "Run 'git submodule update --init --recursive 3rdparty/ccap' before "
        "configuring; CMake will not update submodules automatically.")
elseif(NOT _ege_ccap_ready)
    message(STATUS
        "ccap submodule is absent; camera capture is disabled. Run "
        "'git submodule update --init --recursive 3rdparty/ccap' to enable it.")
endif()

add_library(xege STATIC ${EGE_FRONTEND_SOURCES})
target_include_directories(xege
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(EGE_ENABLE_CPP17)
    target_compile_features(xege PRIVATE cxx_std_17)
    set_target_properties(xege PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(xege PRIVATE EGE_ENABLE_CPP17=1)
endif()

if(EGE_ENABLE_CAMERA_CAPTURE)
    include(CheckCXXCompilerFlag)
    if(MSVC)
        check_cxx_compiler_flag("/arch:AVX2" CCAP_SUPPORTS_AVX2)
    else()
        check_cxx_compiler_flag("-mavx2" CCAP_SUPPORTS_AVX2)
    endif()

    target_include_directories(xege PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ccap/include")
    target_sources(xege PRIVATE ${EGE_CCAP_SOURCES})
    target_compile_definitions(xege PRIVATE EGE_ENABLE_CAMERA_CAPTURE=1)

    set(_ege_ccap_avx2_source
        "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ccap/src/ccap_convert_avx2.cpp")
    if(CCAP_SUPPORTS_AVX2)
        if(MSVC)
            set_source_files_properties(${_ege_ccap_avx2_source}
                PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
        else()
            set_source_files_properties(${_ege_ccap_avx2_source}
                PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
        endif()
    else()
        target_compile_definitions(xege PRIVATE ENABLE_AVX2_IMP=0)
    endif()

    if(APPLE)
        enable_language(OBJCXX)
        target_sources(xege PRIVATE ${EGE_CCAP_APPLE_SOURCES})
        set_source_files_properties(${EGE_CCAP_APPLE_SOURCES}
            PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        _ege_find_required_framework(EGE_FOUNDATION_FRAMEWORK Foundation)
        _ege_find_required_framework(EGE_AVFOUNDATION_FRAMEWORK AVFoundation)
        _ege_find_required_framework(EGE_COREVIDEO_FRAMEWORK CoreVideo)
        _ege_find_required_framework(EGE_COREMEDIA_FRAMEWORK CoreMedia)
        _ege_find_required_framework(EGE_ACCELERATE_FRAMEWORK Accelerate)
        target_link_libraries(xege PRIVATE
            "${EGE_FOUNDATION_FRAMEWORK}"
            "${EGE_AVFOUNDATION_FRAMEWORK}"
            "${EGE_COREVIDEO_FRAMEWORK}"
            "${EGE_COREMEDIA_FRAMEWORK}"
            "${EGE_ACCELERATE_FRAMEWORK}"
        )
        target_compile_definitions(xege PRIVATE
            CCAP_ENABLE_FILE_PLAYBACK=1
            CCAP_ENABLE_VIDEO_WRITER=1)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Threads REQUIRED)
        target_link_libraries(xege PRIVATE Threads::Threads)
    elseif(WIN32)
        target_sources(xege PRIVATE ${EGE_CCAP_WINDOWS_SOURCES})
        target_link_libraries(xege PRIVATE
            mf mfplat mfreadwrite mfuuid shlwapi propsys)
        target_compile_definitions(xege PRIVATE
            CCAP_ENABLE_FILE_PLAYBACK=1
            CCAP_ENABLE_VIDEO_WRITER=1)
    endif()
endif()

if(APPLE)
    set(_ege_macos_music_source
        "${CMAKE_CURRENT_SOURCE_DIR}/${EGE_APPLE_MUSIC_SOURCES}")
    if(NOT EXISTS "${_ege_macos_music_source}")
        message(FATAL_ERROR
            "The macOS MUSIC backend is missing: ${_ege_macos_music_source}")
    endif()
    enable_language(OBJCXX)
    target_sources(xege PRIVATE "${_ege_macos_music_source}")
    set_source_files_properties("${_ege_macos_music_source}"
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    _ege_find_required_framework(EGE_FOUNDATION_FRAMEWORK Foundation)
    _ege_find_required_framework(EGE_AVFAUDIO_FRAMEWORK AVFAudio)
    _ege_find_required_framework(EGE_AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    target_link_libraries(xege PRIVATE
        "${EGE_FOUNDATION_FRAMEWORK}"
        "${EGE_AVFAUDIO_FRAMEWORK}"
        "${EGE_AUDIOTOOLBOX_FRAMEWORK}")
endif()

ege_configure_backend(xege)

if(MSVC)
    set_target_properties(xege PROPERTIES
        OUTPUT_NAME_DEBUG graphicsd
        OUTPUT_NAME_RELEASE graphics
        OUTPUT_NAME_RELWITHDEBINFO graphics
        OUTPUT_NAME_MINSIZEREL graphics
    )
else()
    set_target_properties(xege PROPERTIES OUTPUT_NAME graphics)
endif()

target_compile_definitions(xege
    PUBLIC
        $<$<CONFIG:Debug>:DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG=1>
    PRIVATE
        EGE_GRAPH_LIB_BUILD=1
)

if(MSVC)
    if(EGE_DISABLE_DEBUG_INFO)
        foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            foreach(_language C CXX)
                string(REPLACE "/Zi" ""
                    CMAKE_${_language}_FLAGS_${_config}
                    "${CMAKE_${_language}_FLAGS_${_config}}")
                string(REPLACE "/ZI" ""
                    CMAKE_${_language}_FLAGS_${_config}
                    "${CMAKE_${_language}_FLAGS_${_config}}")
            endforeach()
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
        target_compile_options(xege PRIVATE
            $<$<CONFIG:Debug>:/MLd>
            $<$<CONFIG:Release>:/ML>)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND DEFINED EGE_CRT_COMPAT_DEVCPP)
        target_compile_definitions(xege PRIVATE
            _GLIBCXX_USE_CXX11_ABI=0
            EGE_CRT_COMPAT_DEVCPP=1)
    endif()
    if(UNIX OR MINGW)
        target_compile_definitions(xege PRIVATE _FORTIFY_SOURCE=0)
    endif()
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# enable_testing() has directory scope: it must run here so tests registered by
# subdirectories are visible from the top-level build directory.
if(EGE_BUILD_TEST)
    enable_testing()
    # The historical putimage suite links Win32 libraries directly.  Keep it
    # intact for GDI regression coverage, while native platform tests live in
    # tests/native and carry no Windows linkage.
    if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/native/CMakeLists.txt")
        add_subdirectory(tests/native)
    endif()
    if(EGE_IS_ROOT_PROJECT)
        if(CMAKE_GENERATOR STREQUAL "Xcode")
            # Xcode's OBJECT_FILE_DIR_normal is derived from the target name
            # (`xege.build`), not the CMake project name (`XEGE.build`). These
            # differ on case-sensitive macOS volumes.
            set(_ege_native_object_dir
                "${CMAKE_CURRENT_BINARY_DIR}/build/xege.build/$<CONFIG>/Objects-normal")
        else()
            set(_ege_native_object_dir
                "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/xege.dir")
        endif()
        add_test(NAME cmake.backend_default_contract
            COMMAND "${CMAKE_COMMAND}"
                -DEGE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DEGE_CONTRACT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/cmake/backend-default
                -DEGE_EXPECTED_BACKEND=${EGE_RESOLVED_BACKEND}
                -DEGE_GENERATOR=${CMAKE_GENERATOR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/backend_contract.cmake)
        add_test(NAME cmake.mingw_toolchain_contract
            COMMAND "${CMAKE_COMMAND}"
                -DEGE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DEGE_TOOLCHAIN_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/cmake/mingw-toolchain
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/mingw_toolchain_contract.cmake)
        add_test(NAME cmake.macos_release_contract
            COMMAND "${CMAKE_COMMAND}"
                -DEGE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DEGE_NATIVE_LIBRARY=$<TARGET_FILE:xege>
                -DEGE_NATIVE_OBJECT_DIR=${_ege_native_object_dir}
                -DEGE_TARGET_APPLE=${APPLE}
                -DEGE_EXPECTED_MACOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/macos_release_contract.cmake)
        if(APPLE)
            add_test(NAME cmake.macos_native_tasks_contract
                COMMAND "${CMAKE_COMMAND}"
                    -DEGE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                    -DEGE_TASKS_CONTRACT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/cmake/tasks-native-probe
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/macos_native_tasks_contract.cmake)
        endif()
    endif()
endif()

if(EGE_BUILD_DEMO)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/demo/CMakeLists.txt")
        add_subdirectory(demo)
    else()
        message(STATUS "EGE demo sources are absent; skipping demos")
    endif()
endif()

if(EGE_BUILD_TEMP)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/temp/CMakeLists.txt")
        add_subdirectory(temp)
    else()
        message(STATUS "EGE temporary sources are absent; skipping")
    endif()
endif()

message(STATUS "XEGE configuration")
message(STATUS "  Target platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Default backend: ${EGE_RESOLVED_BACKEND}")
message(STATUS "  Optional OpenGL: ${EGE_ENABLE_OPENGL}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(APPLE)
    message(STATUS "  macOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "  Build configurations: ${CMAKE_CONFIGURATION_TYPES}")
else()
    message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
endif()
