include_guard(GLOBAL)

# Source ownership is intentionally explicit.  A new source file must be added
# to the appropriate list instead of being picked up implicitly by a recursive
# glob, otherwise two platform backends can accidentally be linked together.

set(EGE_FRONTEND_SOURCES
    src/camera_capture.cpp
    src/camera_frame_copy.cpp
    src/color.cpp
    src/compress.cpp
    src/console.cpp
    src/crt_compat.cpp
    src/debug.cpp
    src/ege_dllimport.cpp
    src/egecontrolbase.cpp
    src/egegapi.cpp
    src/encodeconv.cpp
    src/encodeconv_utf8.cpp
    src/font.cpp
    src/gdi_conv.cpp
    src/graphics.cpp
    src/image.cpp
    src/image_ex.cpp
    src/keyboard.cpp
    src/logo.cpp
    src/math.cpp
    src/mouse.cpp
    src/music.cpp
    src/random.cpp
    src/sdefl_impl.cpp
    src/sinfl_impl.cpp
    src/stb_image_impl.cpp
    src/sync/semaphore.cpp
    src/time.cpp
    src/utils.cpp
    src/window.cpp
)

if(WIN32)
    list(APPEND EGE_FRONTEND_SOURCES src/ege_path.cpp)
else()
    # 非 Windows 后端使用可移植的增强绘图实现；GDI 不编译这个空翻译单元。
    list(APPEND EGE_FRONTEND_SOURCES src/ege_gdiplus_fallback.cpp)
endif()

set(EGE_CCAP_SOURCES
    3rdparty/ccap/src/ccap_c.cpp
    3rdparty/ccap/src/ccap_convert.cpp
    3rdparty/ccap/src/ccap_convert_apple.cpp
    3rdparty/ccap/src/ccap_convert_avx2.cpp
    3rdparty/ccap/src/ccap_convert_c.cpp
    3rdparty/ccap/src/ccap_convert_frame.cpp
    3rdparty/ccap/src/ccap_convert_neon.cpp
    3rdparty/ccap/src/ccap_core.cpp
    3rdparty/ccap/src/ccap_imp.cpp
    3rdparty/ccap/src/ccap_imp_linux.cpp
    3rdparty/ccap/src/ccap_imp_windows.cpp
    3rdparty/ccap/src/ccap_utils.cpp
    3rdparty/ccap/src/ccap_utils_c.cpp
    3rdparty/ccap/src/ccap_writer.cpp
    3rdparty/ccap/src/ccap_writer_c.cpp
)

set(EGE_CCAP_APPLE_SOURCES
    3rdparty/ccap/src/ccap_file_reader_apple.mm
    3rdparty/ccap/src/ccap_imp_apple.mm
    3rdparty/ccap/src/ccap_writer_apple.mm
)

set(EGE_CCAP_WINDOWS_SOURCES
    3rdparty/ccap/src/ccap_file_reader_windows.cpp
    3rdparty/ccap/src/ccap_imp_windows_msmf.cpp
    3rdparty/ccap/src/ccap_writer_windows.cpp
)

set(EGE_APPLE_MUSIC_SOURCES
    src/music_backend/music_macos.mm
)
