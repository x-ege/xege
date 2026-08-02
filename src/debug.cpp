#include "debug.h"

#include "ege_head.h"
#include "ege_common.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <cstdio>
#endif

namespace ege
{

void internal_panic(const wchar_t* errmsg)
{
#ifdef _WIN32
    MessageBoxW(graph_setting.hwnd, errmsg, L"EGE INTERNAL ERROR", MB_ICONSTOP);
    ExitProcess((UINT)grError);
#else
    fprintf(stderr, "EGE INTERNAL ERROR: %ls\n", errmsg);
    exit(grError);
#endif
}

} // namespace ege
