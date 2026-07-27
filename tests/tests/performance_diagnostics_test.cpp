#define SHOW_CONSOLE 1
#include "ege.h"
#include "diagnostics.h"
#include "../test_framework.h"

#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

class StderrCapture {
public:
    StderrCapture() : m_file(nullptr), m_savedDescriptor(-1), m_active(false) {}

    ~StderrCapture()
    {
        if (m_active) finish();
        if (m_file) std::fclose(m_file);
    }

    bool start()
    {
        std::fflush(stderr);
        m_file = std::tmpfile();
        if (!m_file) return false;
#ifdef _WIN32
        m_savedDescriptor = _dup(_fileno(stderr));
        if (m_savedDescriptor < 0 ||
            _dup2(_fileno(m_file), _fileno(stderr)) != 0) {
#else
        m_savedDescriptor = dup(fileno(stderr));
        if (m_savedDescriptor < 0 ||
            dup2(fileno(m_file), fileno(stderr)) < 0) {
#endif
            return false;
        }
        m_active = true;
        return true;
    }

    std::string finish()
    {
        if (!m_active) return std::string();
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(m_savedDescriptor, _fileno(stderr));
        _close(m_savedDescriptor);
#else
        dup2(m_savedDescriptor, fileno(stderr));
        close(m_savedDescriptor);
#endif
        m_savedDescriptor = -1;
        m_active = false;

        std::rewind(m_file);
        std::string output;
        char buffer[512];
        size_t count = 0;
        while ((count = std::fread(buffer, 1, sizeof(buffer), m_file)) != 0) {
            output.append(buffer, count);
        }
        return output;
    }

private:
    FILE* m_file;
    int m_savedDescriptor;
    bool m_active;
};

size_t occurrenceCount(const std::string& text, const std::string& needle)
{
    size_t count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

bool usesOpenGlRuntime()
{
#if !defined(EGE_BUILD_OPENGL)
    return false;
#elif defined(_WIN32)
    const char* enabled = std::getenv("EGE_TEST_OPENGL");
    return enabled && enabled[0] == '1';
#else
    return true;
#endif
}

void selectLogOnlyDiagnostics()
{
    if (std::getenv("EGE_DIAGNOSTICS") != nullptr) return;
#ifdef _WIN32
    _putenv_s("EGE_DIAGNOSTICS", "log");
#else
    setenv("EGE_DIAGNOSTICS", "log", 1);
#endif
}

bool runtimeDiagnosticsDisabled()
{
    const char* mode = std::getenv("EGE_DIAGNOSTICS");
    if (!mode) return false;
    std::string normalized(mode);
    for (char& value : normalized) {
        if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
    }
    return normalized == "off" || normalized == "false" || normalized == "0";
}

#if defined(_WIN32) && defined(EGE_BUILD_OPENGL)

struct PopupObservation {
    HWND window;
    PopupObservation() : window(nullptr) {}
};

BOOL CALLBACK findDiagnosticPopup(HWND window, LPARAM parameter)
{
    wchar_t text[256] = {};
    GetWindowTextW(window, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
    if (std::wcsstr(text, L"EGE 性能提示") != nullptr) {
        reinterpret_cast<PopupObservation*>(parameter)->window = window;
        return FALSE;
    }
    return TRUE;
}

int runPopupSmokeTest()
{
    _putenv_s("EGE_DIAGNOSTICS", "all");
    const ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_OPENGL);
    ege::initgraph(160, 120, mode);
    HWND graphicsWindow = ege::getHWnd();
    if (!graphicsWindow || !IsWindowVisible(graphicsWindow)) {
        std::cerr << "FAIL: popup smoke test needs a visible OpenGL window\n";
        return EXIT_FAILURE;
    }

    SetActiveWindow(graphicsWindow);
    const HWND foregroundBefore = GetForegroundWindow();
    ege::PIMAGE source = ege::newimage(32, 32);
    ege::PIMAGE destination = ege::newimage(32, 32);
    if (!source || !destination) {
        std::cerr << "FAIL: popup smoke test could not allocate images\n";
        if (destination) ege::delimage(destination);
        if (source) ege::delimage(source);
        ege::closegraph();
        return EXIT_FAILURE;
    }

    ege::color_t* pixels = ege::getbuffer(source);
    if (pixels) pixels[0] = ege::GREEN;
    for (int iteration = 0; iteration < 3; ++iteration) {
        ege::putimage(destination, 0, 0, source);
    }
    ege::delay_ms(0);

    PopupObservation observation;
    EnumThreadWindows(GetCurrentThreadId(), findDiagnosticPopup,
                      reinterpret_cast<LPARAM>(&observation));
    const LONG_PTR extendedStyle = observation.window
        ? GetWindowLongPtrW(observation.window, GWL_EXSTYLE) : 0;
    const bool passed = observation.window != nullptr &&
        GetWindow(observation.window, GW_OWNER) == graphicsWindow &&
        (extendedStyle & WS_EX_NOACTIVATE) != 0 &&
        (extendedStyle & WS_EX_TOOLWINDOW) != 0 &&
        GetForegroundWindow() == foregroundBefore;

    if (observation.window) DestroyWindow(observation.window);
    ege::delimage(destination);
    ege::delimage(source);
    ege::closegraph();
    if (!passed) {
        std::cerr << "FAIL: diagnostic popup was missing, modal, or changed focus\n";
    }
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif

} // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32) && defined(EGE_BUILD_OPENGL)
    if (argc == 2 && std::strcmp(argv[1], "--popup-smoke") == 0) {
        return runPopupSmokeTest();
    }
#else
    (void)argc;
    (void)argv;
#endif
    selectLogOnlyDiagnostics();

    TestFramework framework;
    if (!framework.initialize(48, 48)) {
        std::cerr << "FAIL: unable to initialize performance diagnostics test\n";
        return EXIT_FAILURE;
    }

    ege::PIMAGE cpuBitmapSource = ege::newimage(32, 32);
    ege::PIMAGE gpuReadbackSource = ege::newimage(24, 24);
    ege::PIMAGE exactUpdateSource = ege::newimage(16, 16);
    ege::PIMAGE destination = ege::newimage(32, 32);
    if (!cpuBitmapSource || !gpuReadbackSource ||
        !exactUpdateSource || !destination) {
        std::cerr << "FAIL: unable to allocate diagnostics fixtures\n";
        if (destination) ege::delimage(destination);
        if (exactUpdateSource) ege::delimage(exactUpdateSource);
        if (gpuReadbackSource) ege::delimage(gpuReadbackSource);
        if (cpuBitmapSource) ege::delimage(cpuBitmapSource);
        framework.cleanup();
        return EXIT_FAILURE;
    }

    StderrCapture capture;
    if (!capture.start()) {
        std::cerr << "FAIL: unable to capture stderr\n";
        ege::delimage(destination);
        ege::delimage(exactUpdateSource);
        ege::delimage(gpuReadbackSource);
        ege::delimage(cpuBitmapSource);
        framework.cleanup();
        return EXIT_FAILURE;
    }

    // Exact updates stay GPU-backed and must not be diagnosed as persistent
    // CPU-bitmap uploads, even when the image is sampled repeatedly.
    const ege::image_storage_mode exactMode =
        ege::getimagestoragemode(exactUpdateSource);
    const ege::color_t exactPixel = ege::YELLOW;
    bool setupSucceeded =
        ege::updatebuffer(exactUpdateSource, 0, 0, 1, 1, &exactPixel) == ege::grOk &&
        ege::getimagestoragemode(exactUpdateSource) == exactMode;
    for (int iteration = 0; iteration < 3; ++iteration) {
        ege::putimage(destination, 0, 0, exactUpdateSource);
    }

    // A retained writable pointer promotes the image to persistent CPU-bitmap
    // storage when that capability is available. Repeated OpenGL sampling must
    // report the real full-upload cost once only on those runtimes.
    ege::color_t* cpuPixels = ege::getbuffer(cpuBitmapSource);
    const bool hasPersistentCpuBitmap =
        ege::getimagestoragemode(cpuBitmapSource) ==
        ege::IMAGE_STORAGE_CPU_BITMAP;
    if (cpuPixels) {
        cpuPixels[0] = ege::GREEN;
    }
    for (int iteration = 0; iteration < 5; ++iteration) {
        ege::putimage(destination, 0, 0, cpuBitmapSource);
    }

    // Repeated const access to an already synchronized GPU shadow is cheap and
    // must not count as repeated GPU readback.
    ege::PCIMAGE cachedSource = exactUpdateSource;
    for (int iteration = 0; iteration < 10; ++iteration) {
        const ege::color_t* pixels = ege::getbuffer(cachedSource);
        if (pixels) {
            volatile ege::color_t observed = pixels[iteration];
            (void)observed;
        }
    }

    // Count actual GPU-to-CPU transitions, not API calls against an already
    // synchronized buffer. Three immediate draw/read cycles cross the debug
    // diagnostic threshold while remaining deterministic and inexpensive.
    ege::PCIMAGE readOnlySource = gpuReadbackSource;
    for (int iteration = 0; iteration < 3; ++iteration) {
        ege::setcolor(ege::RED, gpuReadbackSource);
        ege::line(0, iteration, 20, iteration, gpuReadbackSource);
        const ege::color_t* pixels = ege::getbuffer(readOnlySource);
        if (pixels) {
            volatile ege::color_t observed = pixels[iteration * 24];
            (void)observed;
        }
    }

    const std::string diagnostics = capture.finish();
    const bool expectMessages = usesOpenGlRuntime() &&
        !runtimeDiagnosticsDisabled() &&
        ege::detail::performanceDiagnosticsCompiled();

    bool passed = setupSucceeded;
    if (expectMessages) {
        passed = passed && occurrenceCount(diagnostics, "EGE-PERF-001") ==
            (hasPersistentCpuBitmap ? 1u : 0u);
        passed = passed && occurrenceCount(diagnostics, "EGE-PERF-002") == 1;
        passed = passed && diagnostics.find(
            "EGE-PERF-001] OpenGL IMAGE 16x16") == std::string::npos;
        if (hasPersistentCpuBitmap) {
            passed = passed && diagnostics.find(
                "EGE-PERF-001] OpenGL IMAGE 32x32") != std::string::npos;
            passed = passed &&
                diagnostics.find("persistent CPU bitmap") != std::string::npos;
            passed = passed && diagnostics.find("updatebuffer()") != std::string::npos;
        }
        passed = passed && diagnostics.find("GPU-to-CPU") != std::string::npos;
    } else {
        passed = passed && diagnostics.find("EGE-PERF-") == std::string::npos;
    }
    // stderr is redirected here, so terminal color escapes must not leak into
    // captured logs or CI output.
    passed = passed && diagnostics.find("\x1b[") == std::string::npos;

    if (!passed) {
        std::cerr << "FAIL: unexpected diagnostic output:\n" << diagnostics;
    }

    ege::delimage(destination);
    ege::delimage(exactUpdateSource);
    ege::delimage(gpuReadbackSource);
    ege::delimage(cpuBitmapSource);
    framework.cleanup();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
