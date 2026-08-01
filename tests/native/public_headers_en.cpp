#include "ege.h"

#include <cstdint>
#include <type_traits>

static_assert(sizeof(BYTE) == 1, "BYTE must remain 8-bit");
static_assert(sizeof(WORD) == 2, "WORD must remain 16-bit");
static_assert(sizeof(DWORD) == 4, "DWORD must remain 32-bit");
static_assert(sizeof(LONG) == 4, "LONG must remain 32-bit");
static_assert(sizeof(LONG_PTR) == sizeof(void*), "LONG_PTR must match pointer width");
static_assert(sizeof(POINT) == 8, "POINT must preserve its public layout");
static_assert(sizeof(SIZE) == 8, "SIZE must preserve its public layout");
static_assert(sizeof(RECT) == 16, "RECT must preserve its public layout");
static_assert(std::is_pointer<HWND>::value, "HWND must remain an opaque handle");
static_assert(std::is_pointer<HDC>::value, "HDC must remain an opaque handle");
static_assert(LF_FACESIZE == 32, "LOGFONT face storage must remain source compatible");
static_assert(sizeof(((LOGFONTW*)nullptr)->lfFaceName) / sizeof(WCHAR) == LF_FACESIZE,
              "LOGFONTW must expose LF_FACESIZE wide characters");

void public_headers_en_smoke()
{
    ege::setinitmode(ege::INIT_DEFAULT);
    POINT point = {0, 0};
    RECT rect = {point.x, point.y, 1, 1};
    (void)rect;
}
