#include "ege.h"
#include "../test_opengl_mode.h"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;
int closeHandlerCalls = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void closeHandler()
{
    ++closeHandlerCalls;
}

} // namespace

int main()
{
    const ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE |
        ege::INIT_NOBORDER | ege::INIT_TOPMOST);
    const ege::initmode_flag backendMode = with_opengl_test_mode(mode);
    ege::setinitmode(backendMode, 48, 64);
    ege::initgraph(-1, -1, backendMode);
    GLFWwindow* firstWindow = glfwGetCurrentContext();
    if (!firstWindow) {
        std::cerr << "FAIL: unable to create hidden GLFW lifecycle test window\n";
        return EXIT_FAILURE;
    }
    HWND firstNativeHandle = ege::getHWnd();
#ifdef _WIN32
    expect(firstNativeHandle != nullptr && IsWindow(firstNativeHandle),
           "getHWnd returns the real HWND owned by the opt-in GLFW backend");
#else
    expect(firstNativeHandle == firstWindow,
           "getHWnd returns the GLFW native window token on Unix");
#endif

    int defaultWindowWidth = 0;
    int defaultWindowHeight = 0;
    glfwGetWindowSize(firstWindow, &defaultWindowWidth, &defaultWindowHeight);
    int expectedDefaultWidth = 640;
    int expectedDefaultHeight = 480;
#ifdef _WIN32
    RECT desktopRect = {};
    GetWindowRect(GetDesktopWindow(), &desktopRect);
    expectedDefaultWidth = desktopRect.right - desktopRect.left;
    expectedDefaultHeight = desktopRect.bottom - desktopRect.top;
#endif
    expect(defaultWindowWidth == expectedDefaultWidth &&
               defaultWindowHeight == expectedDefaultHeight,
           "initgraph(-1, -1) uses the native default canvas size (got " +
               std::to_string(defaultWindowWidth) + "x" +
               std::to_string(defaultWindowHeight) + ")");
    expect(ege::getwidth() == defaultWindowWidth &&
               ege::getheight() == defaultWindowHeight,
           "the default window and EGE screen image use the same dimensions (window " +
               std::to_string(defaultWindowWidth) + "x" +
               std::to_string(defaultWindowHeight) + ", image " +
               std::to_string(ege::getwidth()) + "x" +
               std::to_string(ege::getheight()) + ")");
    ege::resizewindow(80, 60);

    expect(glfwGetWindowAttrib(firstWindow, GLFW_VISIBLE) == GLFW_FALSE,
           "INIT_HIDE creates a hidden GLFW window");
    expect(glfwGetWindowAttrib(firstWindow, GLFW_DECORATED) == GLFW_FALSE,
           "INIT_NOBORDER creates an undecorated GLFW window");
    expect(glfwGetWindowAttrib(firstWindow, GLFW_FLOATING) == GLFW_TRUE,
           "INIT_TOPMOST creates a floating GLFW window");
    expect(glfwGetWindowAttrib(firstWindow, GLFW_RESIZABLE) == GLFW_FALSE,
           "the native window keeps the legacy fixed-size user style");
    int windowX = 0;
    int windowY = 0;
    glfwGetWindowPos(firstWindow, &windowX, &windowY);
    expect(windowX == 48 && windowY == 64,
           "setinitmode applies the requested initial window position (got " +
               std::to_string(windowX) + "," + std::to_string(windowY) + ")");
    ege::showwindow();
    expect(glfwGetWindowAttrib(firstWindow, GLFW_VISIBLE) == GLFW_TRUE,
           "showwindow shows the GLFW window");
    ege::hidewindow();
    expect(glfwGetWindowAttrib(firstWindow, GLFW_VISIBLE) == GLFW_FALSE,
           "hidewindow hides the GLFW window");

    expect(ege::showmouse(0) != 0, "showmouse returns the previous visible state");
    expect(glfwGetInputMode(firstWindow, GLFW_CURSOR) == GLFW_CURSOR_HIDDEN,
           "showmouse(0) hides the native GLFW cursor");
    expect(ege::showmouse(1) == 0, "showmouse returns the previous hidden state");
    expect(glfwGetInputMode(firstWindow, GLFW_CURSOR) == GLFW_CURSOR_NORMAL,
           "showmouse(1) restores the native GLFW cursor");

    ege::resizewindow(96, 72);
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(firstWindow, &windowWidth, &windowHeight);
    expect(windowWidth == 96 && windowHeight == 72,
           "resizewindow resizes the native GLFW window");
    expect(ege::getwidth() == 96 && ege::getheight() == 72,
           "resizewindow keeps the EGE screen image dimensions in sync (got " +
               std::to_string(ege::getwidth()) + "x" + std::to_string(ege::getheight()) + ")");

    ege::setviewport(0, 0, 96, 72, true);
    ege::putpixel(95, 71, ege::RED);
    expect((ege::getpixel(95, 71) & 0x00FFFFFFU) == (ege::RED & 0x00FFFFFFU),
           "the resized framebuffer renders through its new bottom-right pixel");

    ege::closegraph();
    expect(ege::getHWnd() == firstNativeHandle,
           "closegraph keeps the reusable native window handle");
    expect(glfwGetWindowAttrib(firstWindow, GLFW_VISIBLE) == GLFW_FALSE,
           "closegraph logically closes EGE by hiding the GLFW window");
    expect(ege::is_run(), "logical close preserves the initialized graphics environment");

    ege::initgraph(40, 30, backendMode);
    expect(ege::getHWnd() == firstNativeHandle && glfwGetCurrentContext() == firstWindow,
           "initgraph reuses both the public native handle and GLFW context");
    expect(glfwGetWindowAttrib(firstWindow, GLFW_VISIBLE) == GLFW_TRUE,
           "reinitializing a logically closed window shows it again");
    expect(ege::getwidth() == 40 && ege::getheight() == 30,
           "reinitialized graphics uses the new dimensions (got " +
               std::to_string(ege::getwidth()) + "x" + std::to_string(ege::getheight()) + ")");
    if (ege::getHWnd()) {
        ege::setviewport(0, 0, 40, 30, true);
        ege::putpixel(39, 29, ege::CYAN);
        expect((ege::getpixel(39, 29) & 0x00FFFFFFU) == (ege::CYAN & 0x00FFFFFFU),
               "the reinitialized screen framebuffer is drawable");
    }

    GLFWframebuffersizefun framebufferSizeCallback =
        glfwSetFramebufferSizeCallback(firstWindow, nullptr);
    expect(framebufferSizeCallback != nullptr, "GLFW framebuffer-size callback is installed");
    if (framebufferSizeCallback) {
        glfwSetFramebufferSizeCallback(firstWindow, framebufferSizeCallback);
        framebufferSizeCallback(firstWindow, 128, 96);
        expect(ege::getwidth() == 40 && ege::getheight() == 30,
               "HiDPI backing-pixel changes do not alter EGE logical dimensions");
    }

    GLFWwindowsizefun windowSizeCallback = glfwSetWindowSizeCallback(firstWindow, nullptr);
    expect(windowSizeCallback != nullptr, "GLFW logical-window-size callback is installed");
    if (windowSizeCallback) {
        glfwSetWindowSizeCallback(firstWindow, windowSizeCallback);
        // Invoke the registered handler directly, as above for framebuffer
        // changes. X11 delivers programmatic ConfigureNotify asynchronously,
        // so a single glfwPollEvents() is not a deterministic callback gate.
        windowSizeCallback(firstWindow, 64, 48);
        expect(ege::getwidth() == 64 && ege::getheight() == 48,
               "logical resize events keep EGE dimensions in sync (got " +
                   std::to_string(ege::getwidth()) + "x" + std::to_string(ege::getheight()) + ")");
        ege::setviewport(0, 0, 64, 48, true);
        ege::putpixel(63, 47, ege::YELLOW);
        expect((ege::getpixel(63, 47) & 0x00FFFFFFU) == (ege::YELLOW & 0x00FFFFFFU),
               "the callback-resized logical canvas is drawable");
    }

    glfwSetWindowShouldClose(firstWindow, GLFW_TRUE);
    ege::delay_ms(0);
    expect(!ege::is_run(),
           "delay_ms pumps native close events instead of leaving the window unresponsive");
    glfwSetWindowShouldClose(firstWindow, GLFW_FALSE);
    ege::initgraph(64, 48, backendMode);
    expect(ege::is_run(), "reinitializing clears a close event observed by delay_ms");

    GLFWwindowclosefun closeCallback = glfwSetWindowCloseCallback(firstWindow, nullptr);
    expect(closeCallback != nullptr, "GLFW close callback is installed");
    if (closeCallback) {
        glfwSetWindowCloseCallback(firstWindow, closeCallback);

        glfwSetWindowShouldClose(firstWindow, GLFW_TRUE);
        closeCallback(firstWindow);
        expect(!ege::is_run(), "INIT_NOFORCEEXIT close requests update is_run");
        expect(glfwWindowShouldClose(firstWindow) == GLFW_FALSE,
               "INIT_NOFORCEEXIT keeps the native window alive");

        ege::initgraph(64, 48, backendMode);
        expect(ege::is_run(), "reinitializing clears the logical close state");

        ege::SetCloseHandler(closeHandler);
        glfwSetWindowShouldClose(firstWindow, GLFW_TRUE);
        closeCallback(firstWindow);
        expect(closeHandlerCalls == 1, "SetCloseHandler receives GLFW close requests");
        expect(ege::is_run(), "a custom close handler controls the logical lifetime");
        expect(glfwWindowShouldClose(firstWindow) == GLFW_FALSE,
               "a custom close handler cancels GLFW's default close action");

        ege::SetCloseHandler(nullptr);
        closeCallback(firstWindow);
        expect(!ege::is_run(), "an unhandled GLFW close request ends the logical run state");
        expect(glfwWindowShouldClose(firstWindow) == GLFW_TRUE,
               "an unhandled GLFW close request preserves GLFW's close flag");
        glfwSetWindowShouldClose(firstWindow, GLFW_FALSE);
    }
    ege::closegraph();

    if (failures != 0) {
        std::cerr << failures << " window backend assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All window backend assertions passed\n";
    return EXIT_SUCCESS;
}
