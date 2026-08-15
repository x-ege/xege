#include "ege.zh_CN.h"

#include <cstdint>
#include <type_traits>

static_assert(sizeof(DWORD) == sizeof(std::uint32_t), "DWORD 必须为 32 位");
static_assert(sizeof(LONG_PTR) == sizeof(void*), "LONG_PTR 必须与指针同宽");
static_assert(sizeof(POINT) == 8, "POINT 布局必须保持兼容");
static_assert(sizeof(SIZE) == 8, "SIZE 布局必须保持兼容");
static_assert(sizeof(RECT) == 16, "RECT 布局必须保持兼容");
static_assert(std::is_pointer<HWND>::value, "HWND 必须为不透明句柄");
static_assert(std::is_pointer<HINSTANCE>::value, "HINSTANCE 必须为不透明句柄");
static_assert(sizeof(((LOGFONTA*)nullptr)->lfFaceName) == LF_FACESIZE,
              "LOGFONTA 必须保留 LF_FACESIZE 个字符");

void public_headers_zh_smoke()
{
    ege::setinitmode(ege::INIT_DEFAULT);
    SIZE size = {1, 1};
    (void)size;
}
