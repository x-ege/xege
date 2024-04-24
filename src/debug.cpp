#include "debug.h"

#include "ege_head.h"
#include "ege_common.h"

#include <Windows.h>

namespace ege
{

void internal_panic(LPCWSTR errmsg)
{
    MessageBoxW(graph_setting.hwnd, errmsg, L"EGE INTERNAL ERROR", MB_ICONSTOP);
    ExitProcess((UINT)grError);
}

} // namespace ege
