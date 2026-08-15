#include <graphics.h>

#include <cstdlib>
#include <iostream>

extern "C" void egeTestPerformNativeClose(void* nativeWindow);
extern "C" bool egeTestNativeWindowVisible(void* nativeWindow);
extern "C" bool egeTestHasWindowServerSession();
extern "C" bool egeTestAppKitFirstHeadersCompile();

namespace
{

int closeCallbacks = 0;

void closeHandler()
{
    ++closeCallbacks;
}

int fail(const char* message)
{
    std::cerr << "Public close callback smoke failed: " << message << '\n';
    ege::closegraph();
    return 1;
}

} // namespace

int main()
{
    // Keep this contract in a child process. A successful assertion path uses
    // a distinctive status so an accidental exit(0) during window teardown
    // cannot be mistaken for a passing test.
    setenv("EGE_MACOS_TEST_NO_ACTIVATE", "1", 1);
    if (!egeTestHasWindowServerSession()) {
        std::cout << "Public close callback smoke skipped: no WindowServer session\n";
        return 77;
    }
    if (!egeTestAppKitFirstHeadersCompile()) {
        return fail("AppKit-first public header probe failed");
    }

    // The callback is a Win32-like notification. INIT_NOFORCEEXIT keeps the
    // process alive while the native close request is still accepted.
    closeCallbacks = 0;
    ege::initgraph(64, 48, ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT);
    void* nativeWindow = reinterpret_cast<void*>(ege::getHWnd());
    if (nativeWindow == nullptr) {
        return fail("initgraph did not expose its native window");
    }

    ege::SetCloseHandler(closeHandler);
    egeTestPerformNativeClose(nativeWindow);
    ege::delay_ms(0);
    if (closeCallbacks != 1 || ege::is_run()
        || egeTestNativeWindowVisible(nativeWindow)) {
        return fail("SetCloseHandler vetoed or missed the native close");
    }

    ege::closegraph();
    return 42;
}
