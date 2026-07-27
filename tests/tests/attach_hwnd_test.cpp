#include "ege.h"
#include "../test_shutdown.h"

#include <cstdlib>
#include <iostream>
#include <thread>

#ifdef _WIN32

namespace {

struct HostWindowThread {
    HANDLE readyEvent;
    HWND window;
    std::thread thread;

    HostWindowThread() : readyEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)), window(nullptr) {}

    static LRESULT CALLBACK windowProcedure(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    bool start()
    {
        if (!readyEvent) return false;
        thread = std::thread([this]() {
            const wchar_t* className = L"EgeAttachHwndTestHost";
            WNDCLASSW windowClass = {};
            windowClass.lpfnWndProc = windowProcedure;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.lpszClassName = className;
            RegisterClassW(&windowClass);
            window = CreateWindowExW(0, className, L"EGE attachHWND test host",
                                     WS_OVERLAPPEDWINDOW,
                                     40, 50, 320, 240,
                                     nullptr, nullptr, windowClass.hInstance, nullptr);
            SetEvent(readyEvent);
            if (window) {
                MSG message;
                while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            UnregisterClassW(className, windowClass.hInstance);
        });
        return WaitForSingleObject(readyEvent, 5000) == WAIT_OBJECT_0 && window != nullptr;
    }

    void stop()
    {
        if (window && IsWindow(window)) PostMessageW(window, WM_CLOSE, 0, 0);
        if (thread.joinable()) thread.join();
        window = nullptr;
        if (readyEvent) {
            CloseHandle(readyEvent);
            readyEvent = nullptr;
        }
    }
};

ege::initmode_flag testMode()
{
    ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
#if defined(EGE_BUILD_OPENGL)
    const char* openGlMode = std::getenv("EGE_TEST_OPENGL");
    if (openGlMode != nullptr && openGlMode[0] == '1') {
        mode = static_cast<ege::initmode_flag>(mode | ege::INIT_OPENGL);
    }
#endif
    return mode;
}

} // namespace

int main()
{
    HostWindowThread host;
    if (!host.start()) {
        std::cerr << "FAIL: unable to create the attachHWND host window\n";
        host.stop();
        return EXIT_FAILURE;
    }

    bool passed = true;
    if (ege::attachHWND(host.window) != ege::grOk) {
        std::cerr << "FAIL: attachHWND rejected a valid host window\n";
        passed = false;
    }

    ege::initgraph(80, 60, testMode());
    HWND egeWindow = ege::getHWnd();
    if (!egeWindow || !IsWindow(egeWindow)) {
        std::cerr << "FAIL: initgraph did not create an EGE child/owned window\n";
        passed = false;
    } else if (GetParent(egeWindow) != host.window) {
        std::cerr << "FAIL: the EGE native window is not associated with the attached host HWND\n";
        passed = false;
    }

    if (!shutdown_graphics_for_test()) {
        std::cerr << "FAIL: the attached EGE window did not shut down cleanly\n";
        passed = false;
    }
#if defined(EGE_BUILD_OPENGL)
    // closegraph intentionally keeps the reusable GLFW HWND alive. Detach it
    // before the host thread destroys its window, otherwise DestroyWindow on
    // the host waits for a child owned by this test thread while this thread
    // is waiting to join the host.
    if ((ege::getinitmode() & ege::INIT_OPENGL) != 0 &&
        egeWindow && IsWindow(egeWindow)) {
        SetParent(egeWindow, nullptr);
        LONG_PTR topLevelStyle = GetWindowLongPtrW(egeWindow, GWL_STYLE);
        topLevelStyle &= ~WS_CHILD;
        topLevelStyle |= WS_POPUP;
        SetWindowLongPtrW(egeWindow, GWL_STYLE, topLevelStyle);
    }
#endif
    ege::attachHWND(nullptr);
    host.stop();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else

int main()
{
    return EXIT_SUCCESS;
}

#endif
