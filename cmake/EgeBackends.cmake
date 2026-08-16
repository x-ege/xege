include_guard(GLOBAL)

set(EGE_DEFAULT_BACKEND "AUTO" CACHE STRING
    "Default rendering backend: AUTO, GDI, COREGRAPHICS, CAIRO, or OPENGL")
set_property(CACHE EGE_DEFAULT_BACKEND PROPERTY STRINGS
    AUTO GDI COREGRAPHICS CAIRO OPENGL)

option(EGE_ENABLE_OPENGL
    "Compile the optional OpenGL backend in addition to the platform backend"
    OFF)

# find_library(... REQUIRED) was introduced in CMake 3.18. Keep the project
# compatible with its declared 3.13 minimum while still failing clearly when
# an Apple SDK framework is unavailable.
function(_ege_find_required_framework output_variable framework_name)
    find_library(${output_variable} NAMES ${framework_name})
    if(NOT ${output_variable})
        message(FATAL_ERROR
            "Required Apple framework '${framework_name}' was not found")
    endif()
    set(${output_variable} "${${output_variable}}" PARENT_SCOPE)
endfunction()

string(TOUPPER "${EGE_DEFAULT_BACKEND}" _EGE_REQUESTED_BACKEND)
set(_EGE_VALID_BACKENDS AUTO GDI COREGRAPHICS CAIRO OPENGL)
list(FIND _EGE_VALID_BACKENDS "${_EGE_REQUESTED_BACKEND}"
    _EGE_BACKEND_INDEX)
if(_EGE_BACKEND_INDEX EQUAL -1)
    message(FATAL_ERROR
        "EGE_DEFAULT_BACKEND='${EGE_DEFAULT_BACKEND}' is invalid. "
        "Expected one of: AUTO, GDI, COREGRAPHICS, CAIRO, OPENGL.")
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

if(_EGE_RESOLVED_BACKEND STREQUAL "OPENGL" AND NOT EGE_ENABLE_OPENGL)
    message(FATAL_ERROR
        "EGE_DEFAULT_BACKEND=OPENGL requires -DEGE_ENABLE_OPENGL=ON. "
        "OpenGL is intentionally opt-in.")
endif()

set(EGE_RESOLVED_BACKEND "${_EGE_RESOLVED_BACKEND}" CACHE INTERNAL
    "Resolved EGE rendering backend" FORCE)

function(_ege_add_existing_sources target output_var)
    set(_added_sources)
    foreach(_source IN LISTS ARGN)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_source}")
            list(APPEND _added_sources "${_source}")
        endif()
    endforeach()
    if(_added_sources)
        target_sources(${target} PRIVATE ${_added_sources})
    endif()
    set(${output_var} "${_added_sources}" PARENT_SCOPE)
endfunction()

function(ege_configure_backend target)
    _ege_add_existing_sources(${target} _ege_common_backend_sources
        src/backend/common/PixelSurface.cpp
    )
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/interface"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/common"
    )

    if(EGE_RESOLVED_BACKEND STREQUAL "GDI")
        _ege_add_existing_sources(${target} _ege_gdi_sources
            src/backend/win32/GDIGraphicsContext.cpp
            src/backend/win32/GDIWindow.cpp
        )
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
        _ege_add_existing_sources(${target} _ege_coregraphics_sources
            src/backend/macos/CoreGraphicsSurface.cpp
            src/backend/macos/CoreGraphicsRenderTarget.cpp
            src/backend/macos/CoreTextRenderer.cpp
            src/backend/macos/MacWindow.cpp
            src/backend/macos/MacWindow.mm
        )
        if(NOT _ege_coregraphics_sources)
            message(FATAL_ERROR
                "The Core Graphics backend was selected, but no implementation "
                "source exists under src/backend/macos.")
        endif()
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
        _ege_add_existing_sources(${target} _ege_cairo_sources
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
    elseif(EGE_RESOLVED_BACKEND STREQUAL "OPENGL")
        target_compile_definitions(${target} PRIVATE EGE_BACKEND_OPENGL=1)
    endif()

    if(EGE_ENABLE_OPENGL)
        _ege_add_existing_sources(${target} _ege_opengl_sources
            src/backend/opengl/GLFWWindow.cpp
            src/backend/opengl/GlFontCache.cpp
            src/backend/opengl/GlRenderTarget.cpp
            src/backend/opengl/GlShader.cpp
            src/backend/opengl/OpenGLGraphicsContext.cpp
            src/backend/opengl/glad/gl_loader.cpp
        )
        if(NOT _ege_opengl_sources)
            message(FATAL_ERROR
                "EGE_ENABLE_OPENGL=ON, but the OpenGL backend sources are absent. "
                "Forward-port src/backend/opengl before enabling it.")
        endif()
        find_package(OpenGL REQUIRED)
        find_package(glfw3 REQUIRED)
        target_compile_definitions(${target} PRIVATE EGE_ENABLE_OPENGL=1)
        target_include_directories(${target} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/opengl")
        target_link_libraries(${target} PRIVATE glfw OpenGL::GL)
    endif()
endfunction()
