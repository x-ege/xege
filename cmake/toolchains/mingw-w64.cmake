# Explicit mingw-w64 cross-compilation toolchain.
#
# Example:
#   cmake -S . -B build/mingw \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake

set(CMAKE_SYSTEM_NAME Windows)

set(EGE_MINGW_TRIPLE "x86_64-w64-mingw32" CACHE STRING
    "mingw-w64 target triple")
set(EGE_MINGW_ROOT "" CACHE PATH
    "Optional mingw-w64 sysroot (leave empty to use compiler defaults)")

set(CMAKE_C_COMPILER "${EGE_MINGW_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${EGE_MINGW_TRIPLE}-g++")
set(CMAKE_RC_COMPILER "${EGE_MINGW_TRIPLE}-windres")

if(EGE_MINGW_ROOT)
    set(CMAKE_FIND_ROOT_PATH "${EGE_MINGW_ROOT}")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(PKG_CONFIG_EXECUTABLE "${EGE_MINGW_TRIPLE}-pkg-config" CACHE FILEPATH
    "pkg-config executable for the mingw-w64 target")
set(EGE_CROSS_COMPILE_MINGW ON CACHE INTERNAL
    "Configured through the XEGE mingw-w64 toolchain")
