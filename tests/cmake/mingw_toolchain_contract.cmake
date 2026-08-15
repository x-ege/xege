if(NOT DEFINED EGE_SOURCE_DIR)
    message(FATAL_ERROR
        "mingw_toolchain_contract.cmake requires EGE_SOURCE_DIR")
endif()

find_program(_ege_real_mingw_c_compiler x86_64-w64-mingw32-gcc)

set(EGE_MINGW_TRIPLE "contract-w64-mingw32" CACHE STRING "" FORCE)
include("${EGE_SOURCE_DIR}/cmake/toolchains/mingw-w64.cmake")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "mingw-w64 toolchain must target Windows")
endif()
if(NOT CMAKE_C_COMPILER STREQUAL "contract-w64-mingw32-gcc")
    message(FATAL_ERROR "Unexpected C compiler: ${CMAKE_C_COMPILER}")
endif()
if(NOT CMAKE_CXX_COMPILER STREQUAL "contract-w64-mingw32-g++")
    message(FATAL_ERROR "Unexpected C++ compiler: ${CMAKE_CXX_COMPILER}")
endif()
if(NOT CMAKE_RC_COMPILER STREQUAL "contract-w64-mingw32-windres")
    message(FATAL_ERROR "Unexpected resource compiler: ${CMAKE_RC_COMPILER}")
endif()
if(NOT CMAKE_FIND_ROOT_PATH_MODE_PROGRAM STREQUAL "NEVER"
        OR NOT CMAKE_FIND_ROOT_PATH_MODE_LIBRARY STREQUAL "ONLY"
        OR NOT CMAKE_FIND_ROOT_PATH_MODE_INCLUDE STREQUAL "ONLY"
        OR NOT CMAKE_FIND_ROOT_PATH_MODE_PACKAGE STREQUAL "ONLY")
    message(FATAL_ERROR "mingw-w64 find-root modes are not isolated")
endif()

# When mingw-w64 is installed, exercise the complete configure path too.  CI
# jobs without the compiler still validate the deterministic toolchain values
# above instead of failing for a missing optional host package.
if(_ege_real_mingw_c_compiler AND DEFINED EGE_TOOLCHAIN_BINARY_DIR)
    file(REMOVE_RECURSE "${EGE_TOOLCHAIN_BINARY_DIR}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -S "${EGE_SOURCE_DIR}"
            -B "${EGE_TOOLCHAIN_BINARY_DIR}"
            -DCMAKE_TOOLCHAIN_FILE=${EGE_SOURCE_DIR}/cmake/toolchains/mingw-w64.cmake
            -DEGE_MINGW_TRIPLE=x86_64-w64-mingw32
            -DEGE_BUILD_DEMO=OFF
            -DEGE_BUILD_TEST=OFF
            -DEGE_BUILD_TEMP=OFF
            -DEGE_ENABLE_CAMERA_CAPTURE=OFF
            -DEGE_DEFAULT_BACKEND=AUTO
        RESULT_VARIABLE _ege_mingw_configure_result
        OUTPUT_VARIABLE _ege_mingw_configure_stdout
        ERROR_VARIABLE _ege_mingw_configure_stderr
    )
    if(NOT _ege_mingw_configure_result EQUAL 0)
        message(FATAL_ERROR
            "Real mingw-w64 configure failed (${_ege_mingw_configure_result}).\n"
            "stdout:\n${_ege_mingw_configure_stdout}\n"
            "stderr:\n${_ege_mingw_configure_stderr}")
    endif()
    file(READ "${EGE_TOOLCHAIN_BINARY_DIR}/CMakeCache.txt" _ege_mingw_cache)
    string(FIND "${_ege_mingw_cache}"
        "EGE_RESOLVED_BACKEND:INTERNAL=GDI" _ege_gdi_match)
    if(_ege_gdi_match EQUAL -1)
        message(FATAL_ERROR
            "AUTO did not resolve to GDI for the Windows toolchain")
    endif()
endif()
