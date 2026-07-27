#include "ege.h"
#include "../test_shutdown.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool waitUntil(const bool expectedRunState)
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (ege::is_run() != expectedRunState &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return ege::is_run() == expectedRunState;
}

} // namespace

int main()
{
    const ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
    ege::initgraph(80, 60, mode);

    HWND window = ege::getHWnd();
    expect(window != nullptr && IsWindow(window),
           "the default Windows backend creates a valid HWND");
    expect(ege::is_run(), "the GDI window starts in the running state");
    expect(ege::getwidth() == 80 && ege::getheight() == 60,
           "the GDI canvas keeps the requested dimensions");

    ege::setbkcolor(ege::BLACK);
    ege::cleardevice();
    ege::putpixel(7, 9, ege::CYAN);
    ege::delay_ms(0);
    ege::delay_ms(20);
    expect(ege::is_run(),
           "delay_ms does not mistake the legacy GDI window for a closed abstract window");
    expect((ege::getpixel(7, 9) & 0x00FFFFFFU) ==
               (ege::CYAN & 0x00FFFFFFU),
           "the default GDI framebuffer remains drawable after event pumping");

    ege::showwindow();
    ege::flushwindow();
    ege::delay_ms(20);
    // Hosted Windows runners do not provide a reliable compositor/front-buffer
    // readback. Verify the public flush contract deterministically: it returns,
    // keeps the visible HWND alive, and does not corrupt the backbuffer.
    expect(IsWindowVisible(window) && IsWindow(window),
           "flushwindow keeps the shown GDI window valid");
    expect(ege::is_run() &&
               (ege::getpixel(7, 9) & 0x00FFFFFFU) ==
                   (ege::CYAN & 0x00FFFFFFU),
           "flushwindow completes without corrupting the GDI backbuffer");
    ege::hidewindow();

    PostMessageW(window, WM_KEYDOWN, VK_ESCAPE, 1);
    const auto keyDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!ege::keystate(ege::key_esc) &&
           std::chrono::steady_clock::now() < keyDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    expect(ege::keystate(ege::key_esc),
           "the Win32 UI thread delivers a posted Escape key-down event");
    PostMessageW(window, WM_KEYUP, VK_ESCAPE, 1);

    PostMessageW(window, WM_CLOSE, 0, 0);
    expect(waitUntil(false),
           "INIT_NOFORCEEXIT turns WM_CLOSE into an observable stopped state");
    expect(IsWindow(window),
           "INIT_NOFORCEEXIT preserves the reusable legacy HWND");

    ege::initgraph(80, 60, mode);
    expect(ege::getHWnd() == window,
           "reinitialization reuses the logically closed GDI window");
    expect(ege::is_run(),
           "reinitialization clears the logical close state");

    expect(shutdown_graphics_for_test(),
           "the GDI window and its UI thread shut down without hanging");

    if (failures != 0) {
        std::cerr << failures << " GDI lifecycle assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All GDI lifecycle assertions passed\n";
    return EXIT_SUCCESS;
}

#else

int main()
{
    return EXIT_SUCCESS;
}

#endif
