#pragma once

#ifdef _WIN32
#include <windows.h>
#include <windef.h>
#endif
#include <wchar.h>

namespace ege
{

HWND getParentWindow();

void getParentSize(int* width, int* height);

#ifndef _WIN32
// 调整 EGE 绘图页以匹配原生帧缓冲区，但不再次请求平台窗口调整尺寸。
// 仅供原生后端的窗口回调使用。
void resize_window_surface(int width, int height);
#endif

HWND createWindow(HWND parentWindow, const wchar_t* caption, DWORD style, DWORD exstyle, POINT pos, SIZE size);

BOOL init_instance(HINSTANCE hInstance);

ATOM register_classW(struct _graph_setting* pg, HINSTANCE hInstance);

} // namespace ege
