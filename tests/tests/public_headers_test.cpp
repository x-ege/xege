#include "ege.zh_CN.h"

#include <cstdint>

static_assert(sizeof(DWORD) == sizeof(std::uint32_t), "DWORD must be 32-bit");
static_assert(sizeof(LONG) == sizeof(std::int32_t), "LONG must be 32-bit");
static_assert(sizeof(LONG_PTR) == sizeof(void*), "LONG_PTR must match pointer width");
static_assert(sizeof(BITMAPFILEHEADER) == 14, "BITMAPFILEHEADER must be packed");
static_assert(sizeof(BITMAPINFOHEADER) == 40, "BITMAPINFOHEADER must match Win32 ABI");
static_assert(LF_FACESIZE == 32, "LF_FACESIZE must match the Win32 API contract");
static_assert(sizeof(((LOGFONTA*)nullptr)->lfFaceName) == LF_FACESIZE,
              "LOGFONTA face storage must match LF_FACESIZE");
static_assert(sizeof(((LOGFONTW*)nullptr)->lfFaceName) / sizeof(wchar_t) == LF_FACESIZE,
              "LOGFONTW face storage must match LF_FACESIZE");

#ifdef EGE_BUILD_OPENGL
static_assert(ege::INIT_OPENGL == 0x80, "OpenGL builds must expose INIT_OPENGL");
#endif

int main()
{
    const char emoji[] = "\xF0\x9F\x98\x80";
    wchar_t decoded[3] = {};
    const int expectedUnits = sizeof(wchar_t) == 2 ? 2 : 1;
    const int required = MultiByteToWideChar(CP_UTF8, 0, emoji, -1, nullptr, 0);
    if (required != expectedUnits + 1 ||
        MultiByteToWideChar(CP_UTF8, 0, emoji, -1, decoded, 3) != required) {
        return 1;
    }

    if (sizeof(wchar_t) == 2) {
        if (static_cast<std::uint32_t>(decoded[0]) != 0xD83D ||
            static_cast<std::uint32_t>(decoded[1]) != 0xDE00 || decoded[2] != 0) {
            return 2;
        }
    } else if (static_cast<std::uint32_t>(decoded[0]) != 0x1F600 || decoded[1] != 0) {
        return 3;
    }

    return 0;
}
