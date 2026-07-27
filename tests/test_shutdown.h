#pragma once

#include "ege.h"

#ifdef _WIN32
#include <chrono>
#include <thread>
#endif

// closegraph() intentionally keeps the EGE environment reusable and only
// hides the window. Tests need a real process-level teardown so the Win32 UI
// thread can leave GetMessage() before static destructors join it.
inline bool shutdown_graphics_for_test()
{
#ifdef _WIN32
#if defined(EGE_BUILD_OPENGL)
    if ((ege::getinitmode() & ege::INIT_OPENGL) != 0) {
        // The GLFW backend owns and destroys its HWND during normal process
        // teardown. closegraph() performs the documented reusable logical
        // close; the separate process-exit test covers final native cleanup.
        ege::closegraph();
        return true;
    }
#endif
    HWND window = ege::getHWnd();
    if (window == nullptr || !IsWindow(window)) {
        return true;
    }

    ege::SetCloseHandler(nullptr);
    if (!PostMessageW(window, WM_CLOSE, 0, 0)) {
        return false;
    }

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (IsWindow(window) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return !IsWindow(window);
#else
    ege::closegraph();
    return true;
#endif
}
