#include "ege.h"
#include "../test_shutdown.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32

namespace {

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

HWND findEditWindow(HWND parent)
{
    struct SearchState {
        HWND result;
    } state = {nullptr};
    EnumChildWindows(parent, [](HWND child, LPARAM parameter) -> BOOL {
        wchar_t className[32] = {};
        GetClassNameW(child, className, 32);
        if (std::wstring(className) == L"Edit") {
            static_cast<SearchState*>(reinterpret_cast<void*>(parameter))->result = child;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&state));
    return state.result;
}

bool supplyInput(const wchar_t* text)
{
    HWND edit = nullptr;
    for (int attempt = 0; attempt < 500 && edit == nullptr; ++attempt) {
        edit = findEditWindow(ege::getHWnd());
        if (edit == nullptr) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (edit == nullptr || !SetWindowTextW(edit, text)) return false;
    const int textLength = GetWindowTextLengthW(edit);
    SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(textLength),
                  static_cast<LPARAM>(textLength));

    // sys_edit subclasses the native EDIT control with EGE's window
    // procedure. Posting the Enter release to that HWND exercises the modal
    // completion path without asking the native multiline control to insert
    // an additional CR/LF into the already supplied fixture text.
    PostMessageW(edit, WM_KEYUP, VK_RETURN, 0);
    return true;
}

int runInputboxChecks()
{
    char narrowSentinel = 'x';
    wchar_t wideSentinel = L'x';
    if (ege::inputbox_getline("", "", nullptr, 16) != 0 ||
        ege::inputbox_getline(L"", L"", nullptr, 16) != 0 ||
        ege::inputbox_getline("", "", &narrowSentinel, 0) != 0 ||
        ege::inputbox_getline(L"", L"", &wideSentinel, 0) != 0 ||
        narrowSentinel != 'x' || wideSentinel != L'x') {
        return 5;
    }

    ege::initgraph(480, 360, testMode());
    if (!ege::getHWnd() || !IsWindow(ege::getHWnd())) return 1;

    std::atomic<bool> wideInputSupplied(false);
    std::thread wideInput([&wideInputSupplied]() {
        wideInputSupplied = supplyInput(L"OpenGL 输入");
    });
    wchar_t wideBuffer[64] = {};
    const int wideLength = ege::inputbox_getline(
        L"Unicode input", L"Enter Unicode text", wideBuffer, 64);
    wideInput.join();
    if (!wideInputSupplied) return 20;
    if (wideLength != 9 || std::wstring(wideBuffer) != L"OpenGL 输入") {
        std::cerr << "wide input mismatch: length=" << wideLength << " code-units=";
        for (const wchar_t* cursor = wideBuffer; *cursor; ++cursor) {
            std::cerr << static_cast<unsigned>(*cursor) << ',';
        }
        std::cerr << '\n';
        return wideLength != 9 ? 100 + wideLength : 21;
    }

    std::atomic<bool> narrowInputSupplied(false);
    std::thread narrowInput([&narrowInputSupplied]() {
        narrowInputSupplied = supplyInput(L"ASCII input");
    });
    char narrowBuffer[64] = {};
    const int narrowLength = ege::inputbox_getline(
        "Narrow input", "Enter ASCII text", narrowBuffer, 64);
    narrowInput.join();
    if (!narrowInputSupplied) return 30;
    if (narrowLength != 11) return 150 + narrowLength;
    if (std::strcmp(narrowBuffer, "ASCII input") != 0) return 31;

    if (!shutdown_graphics_for_test()) return 4;
    return 0;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc == 2 && std::wstring(argv[1]) == L"--isolated-child") {
        return runInputboxChecks();
    }

    wchar_t executable[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        std::cerr << "FAIL: unable to resolve the inputbox contract executable\n";
        return EXIT_FAILURE;
    }
    std::wstring commandLine = L"\"" + std::wstring(executable) + L"\" --isolated-child";
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, &commandLine[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        std::cerr << "FAIL: unable to launch the isolated inputbox child\n";
        return EXIT_FAILURE;
    }

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 15000);
    DWORD exitCode = EXIT_FAILURE;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, EXIT_FAILURE);
        WaitForSingleObject(process.hProcess, 5000);
        std::cerr << "FAIL: isolated inputbox child timed out\n";
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exitCode != EXIT_SUCCESS) {
        std::cerr << "FAIL: inputbox API contract checks failed at stage "
                  << exitCode << '\n';
    }
    return static_cast<int>(exitCode);
}

#else

int main()
{
    return EXIT_SUCCESS;
}

#endif
