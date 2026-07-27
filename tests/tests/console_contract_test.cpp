#include "ege.h"

#include <cstdlib>
#include <iostream>
#include <string>

#ifdef _WIN32

namespace {

int runIsolatedConsoleChecks()
{
    int failureStage = 0;
    const auto check = [&failureStage](bool condition, int stage) {
        if (!condition && failureStage == 0) failureStage = stage;
    };
    check(GetConsoleWindow() == nullptr, 1);
    check(!ege::clear_console(), 2);
    check(!ege::show_console(), 3);
    check(!ege::hide_console(), 4);
    check(!ege::close_console(), 5);

    // A CREATE_NO_WINDOW child launched from a ConPTY can remain attached to
    // the pseudoconsole even though GetConsoleWindow() is null. Detach that
    // inherited association so init_console can exercise AllocConsole.
    FreeConsole();
    SetLastError(ERROR_SUCCESS);
    if (!ege::init_console()) {
        return 1000 + static_cast<int>(GetLastError());
    }
    HWND consoleWindow = GetConsoleWindow();
    check(consoleWindow != nullptr && IsWindow(consoleWindow), 7);
    check(ege::clear_console(), 8);

    check(ege::hide_console(), 9);
    check(!IsWindowVisible(consoleWindow), 10);
    check(ege::show_console(), 11);
    check(IsWindowVisible(consoleWindow), 12);

    INPUT_RECORD input = {};
    input.EventType = KEY_EVENT;
    input.Event.KeyEvent.bKeyDown = TRUE;
    input.Event.KeyEvent.wRepeatCount = 1;
    input.Event.KeyEvent.wVirtualKeyCode = 'Z';
    input.Event.KeyEvent.wVirtualScanCode =
        static_cast<WORD>(MapVirtualKeyW('Z', MAPVK_VK_TO_VSC));
    input.Event.KeyEvent.uChar.AsciiChar = 'z';
    DWORD written = 0;
    check(WriteConsoleInputA(GetStdHandle(STD_INPUT_HANDLE), &input, 1, &written) &&
          written == 1, 13);
    check(ege::kbhit_console() != 0, 14);
    check(ege::getch_console() == 'z', 15);

    check(ege::close_console(), 16);
    check(GetConsoleWindow() == nullptr, 17);
    return failureStage;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc == 2 && std::wstring(argv[1]) == L"--isolated-child") {
        return runIsolatedConsoleChecks();
    }

    wchar_t executable[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        std::cerr << "FAIL: unable to resolve the console contract test executable\n";
        return EXIT_FAILURE;
    }
    std::wstring commandLine = L"\"" + std::wstring(executable) + L"\" --isolated-child";
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, &commandLine[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        std::cerr << "FAIL: unable to launch the isolated console child\n";
        return EXIT_FAILURE;
    }

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 15000);
    DWORD exitCode = EXIT_FAILURE;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, EXIT_FAILURE);
        WaitForSingleObject(process.hProcess, 5000);
        std::cerr << "FAIL: isolated console child timed out\n";
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exitCode != EXIT_SUCCESS) {
        std::cerr << "FAIL: isolated console API contract checks failed at stage "
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
