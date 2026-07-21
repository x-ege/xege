#include "diagnostics.h"

#include "ege.h"
#include "ege_graph.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ege {
namespace detail {
namespace {

struct DiagnosticState {
    std::atomic<bool> cpuBitmapUploadEmitted;
    std::atomic<bool> repeatedReadbackEmitted;
    std::atomic<bool> popupShown;
    std::mutex outputMutex;

    DiagnosticState()
        : cpuBitmapUploadEmitted(false), repeatedReadbackEmitted(false),
          popupShown(false) {}
};

DiagnosticState& diagnosticState()
{
    static DiagnosticState state;
    return state;
}

bool equalsIgnoreCase(const char* value, const char* expected)
{
    if (!value || !expected) return false;
    while (*value && *expected) {
        char lhs = *value++;
        char rhs = *expected++;
        if (lhs >= 'A' && lhs <= 'Z') lhs = static_cast<char>(lhs - 'A' + 'a');
        if (rhs >= 'A' && rhs <= 'Z') rhs = static_cast<char>(rhs - 'A' + 'a');
        if (lhs != rhs) return false;
    }
    return *value == '\0' && *expected == '\0';
}

const char* runtimeMode()
{
    return std::getenv("EGE_DIAGNOSTICS");
}

bool runtimeDiagnosticsEnabled()
{
    const char* mode = runtimeMode();
    return !equalsIgnoreCase(mode, "off") &&
           !equalsIgnoreCase(mode, "false") &&
           !equalsIgnoreCase(mode, "0");
}

bool runtimePopupEnabled()
{
    const char* mode = runtimeMode();
    return equalsIgnoreCase(mode, "all") ||
           equalsIgnoreCase(mode, "popup");
}

bool stderrSupportsColor()
{
    if (std::getenv("NO_COLOR") != nullptr) return false;
#ifdef _WIN32
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

std::string formatDiagnostic(
    PerformanceDiagnosticCode code,
    const PerformanceDiagnosticContext& context)
{
    std::ostringstream message;
    switch (code) {
    case PerformanceDiagnosticCode::RepeatedCpuBitmapFullUpload:
        message << "[EGE][performance][EGE-PERF-001] OpenGL IMAGE "
                << context.width << 'x' << context.height
                << ": a persistent CPU bitmap required "
                << context.occurrenceCount
                << " complete uploads within "
                << context.intervalMilliseconds
                << " ms. Batch retained-pointer/HDC writes and minimize "
                   "sampling; when stable CPU storage is required, sample at "
                   "most once per frame. Otherwise, keep "
                   "the image GPU-backed and use updatebuffer() for known "
                   "regions.";
        break;
    case PerformanceDiagnosticCode::RepeatedGpuReadback:
        message << "[EGE][performance][EGE-PERF-002] OpenGL IMAGE "
                << context.width << 'x' << context.height << ": "
                << context.occurrenceCount
                << " pixel-buffer GPU-to-CPU readbacks occurred within "
                << context.intervalMilliseconds
                << " ms. Batch CPU-reading operations, or avoid alternating "
                   "GPU drawing with synchronous pixel access.";
        break;
    }
    return message.str();
}

void writeDiagnostic(const std::string& message)
{
    DiagnosticState& state = diagnosticState();
    std::lock_guard<std::mutex> lock(state.outputMutex);
    const bool useColor = stderrSupportsColor();

#ifdef _WIN32
    HANDLE errorHandle = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO originalAttributes = {};
    const bool canSetColor = useColor && errorHandle != INVALID_HANDLE_VALUE &&
        GetConsoleScreenBufferInfo(errorHandle, &originalAttributes) != FALSE;
    const char* terminal = std::getenv("TERM");
    const bool useAnsiFallback = useColor && !canSetColor && terminal &&
        !equalsIgnoreCase(terminal, "dumb");
    if (canSetColor) {
        SetConsoleTextAttribute(
            errorHandle,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
    if (useAnsiFallback) std::fputs("\033[1;33m", stderr);
    std::fprintf(stderr, "%s\n", message.c_str());
    if (useAnsiFallback) std::fputs("\033[0m", stderr);
    std::fflush(stderr);
    if (canSetColor) {
        SetConsoleTextAttribute(errorHandle, originalAttributes.wAttributes);
    }
#else
    if (useColor) std::fputs("\033[1;33m", stderr);
    std::fprintf(stderr, "%s", message.c_str());
    if (useColor) std::fputs("\033[0m", stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
#endif

#if defined(_WIN32) && defined(DEBUG) && !defined(NDEBUG)
    std::string debuggerMessage = message + "\r\n";
    OutputDebugStringA(debuggerMessage.c_str());
#endif
}

#if defined(_WIN32) && defined(DEBUG) && !defined(NDEBUG)

void CALLBACK dismissDiagnosticPopup(
    HWND window, UINT, UINT_PTR timer, DWORD)
{
    KillTimer(window, timer);
    DestroyWindow(window);
}

void showFirstDiagnosticPopup()
{
    if (!runtimePopupEnabled()) return;
    if ((static_cast<unsigned int>(getinitmode()) &
         static_cast<unsigned int>(INIT_HIDE)) != 0) {
        return;
    }

    HWND owner = graph_setting.hwnd;
    if (!owner || !IsWindow(owner) || !IsWindowVisible(owner)) return;
    if ((GetWindowLongPtrW(owner, GWL_STYLE) & WS_CHILD) != 0) return;

    DWORD ownerThread = GetWindowThreadProcessId(owner, nullptr);
    if (ownerThread != GetCurrentThreadId()) return;

    DiagnosticState& state = diagnosticState();
    bool expected = false;
    if (!state.popupShown.compare_exchange_strong(expected, true)) return;

    const int popupWidth = 500;
    const int popupHeight = 108;
    RECT workArea = {};
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST),
                        &monitorInfo)) {
        workArea = monitorInfo.rcWork;
    } else if (!SystemParametersInfoW(
                   SPI_GETWORKAREA, 0, &workArea, 0)) {
        GetWindowRect(owner, &workArea);
    }
    const int x = workArea.right - popupWidth - 16;
    const int y = workArea.bottom - popupHeight - 16;

    HWND popup = CreateWindowExW(
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"STATIC",
        L"EGE 性能提示 / Performance diagnostic\r\n"
        L"检测到兼容但低效的像素访问；详情见 stderr 或调试器输出。",
        WS_POPUP | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
        x, y, popupWidth, popupHeight, owner, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!popup) {
        state.popupShown.store(false);
        return;
    }

    SendMessageW(popup, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
    if (SetTimer(popup, 1, 8000, dismissDiagnosticPopup) == 0) {
        DestroyWindow(popup);
        state.popupShown.store(false);
        return;
    }
    ShowWindow(popup, SW_SHOWNOACTIVATE);
    SetWindowPos(popup, HWND_TOPMOST, x, y, popupWidth, popupHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

#else

void showFirstDiagnosticPopup() {}

#endif

std::atomic<bool>& emissionFlag(PerformanceDiagnosticCode code)
{
    DiagnosticState& state = diagnosticState();
    if (code == PerformanceDiagnosticCode::RepeatedCpuBitmapFullUpload) {
        return state.cpuBitmapUploadEmitted;
    }
    return state.repeatedReadbackEmitted;
}

} // namespace

bool performanceDiagnosticsCompiled()
{
    return EGE_ENABLE_PERFORMANCE_DIAGNOSTICS != 0;
}

void reportPerformanceDiagnostic(
    PerformanceDiagnosticCode code,
    const PerformanceDiagnosticContext& context)
{
#if EGE_ENABLE_PERFORMANCE_DIAGNOSTICS
    if (!runtimeDiagnosticsEnabled()) return;
    if (emissionFlag(code).exchange(true, std::memory_order_relaxed)) return;

    writeDiagnostic(formatDiagnostic(code, context));
    showFirstDiagnosticPopup();
#else
    (void)code;
    (void)context;
#endif
}

} // namespace detail
} // namespace ege
