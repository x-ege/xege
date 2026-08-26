include_guard(GLOBAL)

set(EGE_DEFAULT_BACKEND "AUTO" CACHE STRING
    "Default rendering backend: AUTO, GDI, COREGRAPHICS, or CAIRO")
set_property(CACHE EGE_DEFAULT_BACKEND PROPERTY STRINGS
    AUTO GDI COREGRAPHICS CAIRO)

# find_library(... REQUIRED) 从 CMake 3.18 才支持。这里兼容项目声明的 3.13 下限，
# 同时在 Apple SDK framework 缺失时明确失败。
function(_ege_find_required_framework output_variable framework_name)
    find_library(${output_variable} NAMES ${framework_name})
    if(NOT ${output_variable})
        message(FATAL_ERROR
            "Required Apple framework '${framework_name}' was not found")
    endif()
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

string(TOUPPER "${EGE_DEFAULT_BACKEND}" _EGE_REQUESTED_BACKEND)
set(_EGE_VALID_BACKENDS AUTO GDI COREGRAPHICS CAIRO)
list(FIND _EGE_VALID_BACKENDS "${_EGE_REQUESTED_BACKEND}"
    _EGE_BACKEND_INDEX)
if(_EGE_BACKEND_INDEX EQUAL -1)
    message(FATAL_ERROR
        "EGE_DEFAULT_BACKEND='${EGE_DEFAULT_BACKEND}' is invalid. "
        "Expected one of: AUTO, GDI, COREGRAPHICS, CAIRO.")
endif()

if(_EGE_REQUESTED_BACKEND STREQUAL "AUTO")
    if(WIN32)
        set(_EGE_RESOLVED_BACKEND GDI)
    elseif(APPLE)
        set(_EGE_RESOLVED_BACKEND COREGRAPHICS)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_EGE_RESOLVED_BACKEND CAIRO)
    else()
        message(FATAL_ERROR
            "EGE_DEFAULT_BACKEND=AUTO has no default for target platform "
            "'${CMAKE_SYSTEM_NAME}'. Select a backend explicitly.")
    endif()
else()
    set(_EGE_RESOLVED_BACKEND "${_EGE_REQUESTED_BACKEND}")
endif()

if(_EGE_RESOLVED_BACKEND STREQUAL "GDI" AND NOT WIN32)
    message(FATAL_ERROR
        "The GDI backend requires a Windows target. For cross-compilation, "
        "configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64.cmake.")
elseif(_EGE_RESOLVED_BACKEND STREQUAL "COREGRAPHICS" AND NOT APPLE)
    message(FATAL_ERROR "The Core Graphics backend requires an Apple target.")
elseif(_EGE_RESOLVED_BACKEND STREQUAL "CAIRO"
        AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The Cairo backend currently requires a Linux target.")
endif()

set(EGE_RESOLVED_BACKEND "${_EGE_RESOLVED_BACKEND}" CACHE INTERNAL
    "Resolved EGE rendering backend" FORCE)

function(ege_configure_backend target)
    if(NOT EGE_RESOLVED_BACKEND STREQUAL "GDI")
        target_sources(${target} PRIVATE src/backend/common/PixelSurface.cpp)
    endif()
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/common"
    )

    if(EGE_RESOLVED_BACKEND STREQUAL "GDI")
        target_compile_definitions(${target} PRIVATE EGE_BACKEND_GDI=1)
        target_link_libraries(${target} INTERFACE
            gdiplus
            gdi32
            imm32
            ole32
            oleaut32
            uuid
            winmm
            msimg32
        )
    elseif(EGE_RESOLVED_BACKEND STREQUAL "COREGRAPHICS")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/macos/MacWindow.mm")
            enable_language(OBJCXX)
            set_source_files_properties(src/backend/macos/MacWindow.mm
                PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        endif()
        target_sources(${target} PRIVATE
            src/backend/macos/CoreGraphicsSurface.cpp
            src/backend/macos/CoreGraphicsRenderTarget.cpp
            src/backend/macos/CoreTextRenderer.cpp
            src/backend/macos/MacWindow.mm
        )
        target_compile_definitions(${target} PRIVATE EGE_BACKEND_COREGRAPHICS=1)
        target_include_directories(${target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/macos")
        _ege_find_required_framework(EGE_APPKIT_FRAMEWORK AppKit)
        _ege_find_required_framework(EGE_COREGRAPHICS_FRAMEWORK CoreGraphics)
        _ege_find_required_framework(EGE_CORETEXT_FRAMEWORK CoreText)
        _ege_find_required_framework(EGE_IMAGEIO_FRAMEWORK ImageIO)
        target_link_libraries(${target} PRIVATE
            "${EGE_APPKIT_FRAMEWORK}"
            "${EGE_COREGRAPHICS_FRAMEWORK}"
            "${EGE_CORETEXT_FRAMEWORK}"
            "${EGE_IMAGEIO_FRAMEWORK}")
    elseif(EGE_RESOLVED_BACKEND STREQUAL "CAIRO")
        set(_ege_cairo_required_sources
            src/backend/linux/CairoRenderTarget.cpp
            src/backend/linux/LinuxWindow.cpp)
        foreach(_ege_source IN LISTS _ege_cairo_required_sources)
            if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_ege_source}")
                message(FATAL_ERROR
                    "The Cairo backend is incomplete: missing ${_ege_source}")
            endif()
        endforeach()
        target_sources(${target} PRIVATE
            src/backend/linux/CairoRenderTarget.cpp
            src/backend/linux/LinuxWindow.cpp
        )
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(EGE_CAIRO REQUIRED IMPORTED_TARGET cairo)
        pkg_check_modules(EGE_X11 REQUIRED IMPORTED_TARGET x11)
        target_compile_definitions(${target} PRIVATE EGE_BACKEND_CAIRO=1)
        target_include_directories(${target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/linux")
        target_link_libraries(${target} PRIVATE
            PkgConfig::EGE_CAIRO
            PkgConfig::EGE_X11)
    endif()
endfunction()
