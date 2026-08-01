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

// Resize EGE drawing pages to the native framebuffer size without requesting
// another platform-window resize. Used by native backend callbacks.
void resize_window_surface(int width, int height);

HWND createWindow(HWND parentWindow, const wchar_t* caption, DWORD style, DWORD exstyle, POINT pos, SIZE size);

BOOL init_instance(HINSTANCE hInstance);

ATOM register_classW(struct _graph_setting* pg, HINSTANCE hInstance);

} // namespace ege
