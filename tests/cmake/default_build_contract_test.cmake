cmake_minimum_required(VERSION 3.13)

foreach(required_variable EGE_SOURCE_DIR EGE_BINARY_DIR)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} must be provided")
    endif()
endforeach()

file(REMOVE_RECURSE "${EGE_BINARY_DIR}")

set(configure_command
    "${CMAKE_COMMAND}"
    -S "${EGE_SOURCE_DIR}"
    -B "${EGE_BINARY_DIR}"
    -DEGE_BUILD_DEMO=OFF
    -DEGE_BUILD_TEST=OFF
    -DEGE_BUILD_TEMP=OFF
    -DEGE_ENABLE_CAMERA_CAPTURE=OFF
)

if(CMAKE_HOST_WIN32)
    # This test validates platform option defaults, not CMake's generator
    # selection. A nested Visual Studio configure can strand MSBuild tracking
    # processes when CTest itself runs from a non-interactive shell. Prefer a
    # multi-config Ninja probe with the already validated parent toolchain so
    # CMAKE_BUILD_TYPE remains intentionally unset and no backend option is
    # smuggled into the contract check.
    find_program(EGE_NINJA_EXECUTABLE ninja)
    if(EGE_NINJA_EXECUTABLE AND
       DEFINED EGE_C_COMPILER AND NOT EGE_C_COMPILER STREQUAL "" AND
       DEFINED EGE_CXX_COMPILER AND NOT EGE_CXX_COMPILER STREQUAL "")
        list(APPEND configure_command
            -G "Ninja Multi-Config"
            "-DCMAKE_MAKE_PROGRAM=${EGE_NINJA_EXECUTABLE}"
            "-DCMAKE_C_COMPILER=${EGE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${EGE_CXX_COMPILER}"
            -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
        )
        if(DEFINED EGE_AR AND NOT EGE_AR STREQUAL "")
            list(APPEND configure_command "-DCMAKE_AR=${EGE_AR}")
        endif()
        if(DEFINED EGE_LINKER AND NOT EGE_LINKER STREQUAL "")
            list(APPEND configure_command "-DCMAKE_LINKER=${EGE_LINKER}")
        endif()
    endif()
endif()

execute_process(
    COMMAND ${configure_command}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)

if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Default XEGE configuration failed (${configure_result})\n"
        "stdout:\n${configure_stdout}\n"
        "stderr:\n${configure_stderr}")
endif()

set(cache_file "${EGE_BINARY_DIR}/CMakeCache.txt")
if(NOT EXISTS "${cache_file}")
    message(FATAL_ERROR "Default configuration did not create ${cache_file}")
endif()

file(READ "${cache_file}" cache_contents)

function(assert_cache_value variable expected)
    string(REGEX MATCH "(^|\n)${variable}:[^=]*=([^\n\r]*)" match "${cache_contents}")
    if(NOT match)
        message(FATAL_ERROR "${variable} is missing from the default CMake cache")
    endif()

    set(actual "${CMAKE_MATCH_2}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR
            "Expected default ${variable}=${expected}, got ${actual}")
    endif()
endfunction()

if(CMAKE_HOST_UNIX)
    assert_cache_value(EGE_BUILD_OPENGL ON)
    assert_cache_value(EGE_USE_BUNDLED_GLFW ON)
    assert_cache_value(EGE_BUILD_FOR_LINUX ON)

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        assert_cache_value(GLFW_BUILD_X11 ON)
        assert_cache_value(GLFW_BUILD_WAYLAND OFF)
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        assert_cache_value(GLFW_BUILD_COCOA ON)
    endif()
else()
    assert_cache_value(EGE_BUILD_OPENGL OFF)
    assert_cache_value(EGE_USE_BUNDLED_GLFW OFF)
    assert_cache_value(EGE_BUILD_FOR_LINUX OFF)
endif()

# Multi-config generators intentionally leave CMAKE_BUILD_TYPE empty.
if(NOT cache_contents MATCHES "(^|\n)CMAKE_CONFIGURATION_TYPES:[^=]*=[^\n\r]+")
    assert_cache_value(CMAKE_BUILD_TYPE Release)
endif()
