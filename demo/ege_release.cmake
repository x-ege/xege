cmake_minimum_required(VERSION 3.13)


# 设置静态库搜索路径
if(MSVC)
    # 设置 MSVC 编译选项, 当版本大于 vs2019 时, 使用 c++17 标准.
    set(CPP_COMPILE_OPTIONS "/std:c++17")

    if(MSVC_VERSION GREATER_EQUAL 1930)
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
    elseif(MSVC_VERSION GREATER_EQUAL 1910)
        # vs2017
        if(CMAKE_CL_64)
            set(osLibDir "vs2017/x64")
        else()
            set(osLibDir "vs2017/x86")
        endif()

        # 设置 MSVC 编译选项, 当版本为 vs2017 时, 使用 c++14 标准.
        set(CPP_COMPILE_OPTIONS "/std:c++14")
    elseif(MSVC_VERSION GREATER_EQUAL 1900)
        # vs2015
        if(CMAKE_CL_64)
            set(osLibDir "vs2015/x64")
        else()
            set(osLibDir "vs2015/x86")
        endif()

        # 设置 MSVC 编译选项, 当版本为 vs2015 时, 使用 c++14 标准.
        set(CPP_COMPILE_OPTIONS "/std:c++14")
    elseif(MSVC_VERSION GREATER_EQUAL 1600)
        if(MSVC_VERSION GREATER_EQUAL 1700)
            message(WARNING "You are using vs2012/vs2013, which is not tested, please use vs2010, vs2015 or later version of MSVC compiler.")
        endif()

        # vs2010
        if(CMAKE_CL_64)
            set(osLibDir "vs2010/x64")
        else()
            set(osLibDir "vs2010/x86")
        endif()

    else()
        message(FATAL_ERROR "Your MSVC version is too old. Please use a modern version of MSVC compiler.")
    endif()

    add_library(xege STATIC IMPORTED)
    set_target_properties(xege PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../Release/lib/${osLibDir}/graphics.lib)

    target_compile_options(xege INTERFACE
        /source-charset:utf-8
        /MP
        /D__STDC_LIMIT_MACROS
        /D_USE_MATH_DEFINES
        /Zc:__cplusplus
        ${CPP_COMPILE_OPTIONS}
        "$<$<CONFIG:DEBUG>:/DDEBUG>"
        "$<$<CONFIG:RELEASE>:/DNDEBUG>"
        "$<$<CONFIG:RELWITHDEBINFO>:/DNDEBUG>"
        "$<$<CONFIG:MINSIZEREL>:/DNDEBUG>"
        "$<IF:$<CONFIG:Debug>,/MDd,/MD>")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if(CMAKE_HOST_APPLE)
        set(osLibDir "macOS")
    elseif(CMAKE_HOST_UNIX)
        set(osLibDir "mingw-w64-debian")
    elseif(MINGW)
        set(osLibDir "mingw64")
    endif()

    add_library(xege STATIC IMPORTED)
    set_target_properties(xege PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../Release/lib/${osLibDir}/libgraphics.a)

    target_compile_options(xege INTERFACE -D_FORTIFY_SOURCE=0)
    target_link_options(xege INTERFACE -mwindows -static -static-libgcc -static-libstdc++)
    target_link_libraries(xege INTERFACE gdiplus gdi32 imm32 msimg32 ole32 oleaut32 winmm uuid)
endif()

target_include_directories(xege INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
