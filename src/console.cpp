#include "ege_head.h"

#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <wincon.h>


#undef _INC_CONIO
#undef _CONIO_H_
#include <conio.h>

// macro to check whether hConsole is valid
#define CHECK_CONSOLE_HANDLE(hHandle) \
    {                                 \
        if (hHandle == NULL)          \
            return FALSE;             \
    }

namespace ege
{

// We don't support vc6
#ifndef EGE_COMPILERINFO_VC6

static HANDLE hInput = NULL;
static HANDLE hOutput = NULL;
static HWND hConsoleWnd = NULL;
static FILE fOldIn;
static FILE fOldOut;

bool init_console()
{
    HMENU hMenu;
    int hCrt;
    FILE* hf;
    if (hInput != NULL) {
        return false;
    }
    if (GetConsoleWindow() != NULL) {
        return false;
    }
    if (!AllocConsole()) {
        return false;
    }
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    if (INVALID_HANDLE_VALUE == hOutput) {
        return false;
    }
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (INVALID_HANDLE_VALUE == hInput) {
        return false;
    }
    SetConsoleTitle("EGE CONSOLE");
    hConsoleWnd = GetConsoleWindow();
    if (INVALID_HANDLE_VALUE == hConsoleWnd) {
        return false;
    }
    hMenu = GetSystemMenu(hConsoleWnd, 0);
    if (hMenu != NULL) {
        DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
        DrawMenuBar(hConsoleWnd);
    }

    hCrt = _open_osfhandle((intptr_t)hOutput, _O_TEXT);
    hf = _fdopen(hCrt, "w");
    fOldOut = *stdout;
    *stdout = *hf;
    setvbuf(stdout, NULL, _IONBF, 0);

    hCrt = _open_osfhandle((intptr_t)hInput, _O_TEXT);
    hf = _fdopen(hCrt, "r");
    fOldIn = *stdin;
    *stdin = *hf;
    // setvbuf( stdin, NULL, _IONBF, 0 );
    //  test code

    // SetConsoleTextAttribute(hOutput, BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED ); // white text on black bg
    clear_console();
    ShowWindow(hConsoleWnd, SW_SHOW);
    return true;
}

bool clear_console()
{
    /***************************************/
    // This code is from one of Microsoft's
    // knowledge base articles, you can find it at
    // http://support.microsoft.com/default.aspx?scid=KB;EN-US;q99261&
    /***************************************/

    COORD coordScreen = {0, 0};

    bool bSuccess;
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */
    DWORD dwConSize;

    if (hOutput == NULL) {
        return false;
    }

    /* get the number of character cells in the current buffer */

    bSuccess = GetConsoleScreenBufferInfo(hOutput, &csbi);

    if (!bSuccess) {
        return false;
    }

    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    /* fill the entire screen with blanks */

    bSuccess = FillConsoleOutputCharacter(hOutput, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten);
    if (!bSuccess) {
        return false;
    }

    /* get the current text attribute */

    bSuccess = GetConsoleScreenBufferInfo(hOutput, &csbi);
    if (!bSuccess) {
        return false;
    }

    /* now set the buffer's attributes accordingly */

    bSuccess = FillConsoleOutputAttribute(hOutput, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    if (!bSuccess) {
        return false;
    }

    /* put the cursor at (0, 0) */

    bSuccess = SetConsoleCursorPosition(hOutput, coordScreen);
    if (!bSuccess) {
        return false;
    }

    return true;
}

bool show_console()
{
    if (hConsoleWnd == NULL || !IsWindow(hConsoleWnd)) {
        return false;
    }
    ShowWindow(hConsoleWnd, SW_SHOW);
    return true;
}

bool hide_console()
{
    if (hConsoleWnd == NULL || !IsWindow(hConsoleWnd)) {
        return false;
    }
    ShowWindow(hConsoleWnd, SW_HIDE);
    return true;
}

bool close_console()
{
    if (hInput == NULL || hOutput == NULL ||
        hConsoleWnd == NULL || !IsWindow(hConsoleWnd)) {
        return false;
    }
    if (!FreeConsole()) {
        return false;
    }

    hOutput = NULL;
    hInput = NULL;
    hConsoleWnd = NULL;
    *stdout = fOldOut;
    *stdin = fOldIn;
    return true;
};

#endif

int getch_console()
{
#ifdef MSVC_VER
    return ::_getch();
#else
    return ::getch();
#endif
}

int kbhit_console()
{
#ifdef MSVC_VER
    return ::_kbhit();
#else
    return ::kbhit();
#endif
}

} // namespace ege
#else
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace ege
{
    bool init_console() { return ::isatty(STDIN_FILENO) != 0; }

    bool clear_console()
    {
        if (::isatty(STDOUT_FILENO) == 0) {
            return false;
        }
        return ::fputs("\033[2J\033[H", stdout) >= 0 &&
               ::fflush(stdout) == 0;
    }

    // A terminal is owned by its host application on Unix. EGE cannot show,
    // hide, or close that window, so report the unsupported operation.
    bool show_console() { return false; }
    bool hide_console() { return false; }
    bool close_console() { return false; }

    int getch_console()
    {
        termios original{};
        if (::tcgetattr(STDIN_FILENO, &original) != 0) {
            return ::getchar();
        }
        termios raw = original;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
            return ::getchar();
        }
        const int character = ::getchar();
        (void)::tcsetattr(STDIN_FILENO, TCSANOW, &original);
        return character;
    }

    int kbhit_console()
    {
        termios original{};
        if (::tcgetattr(STDIN_FILENO, &original) != 0) {
            return 0;
        }
        termios polling = original;
        polling.c_lflag &= static_cast<tcflag_t>(~ICANON);
        polling.c_cc[VMIN] = 0;
        polling.c_cc[VTIME] = 0;
        if (::tcsetattr(STDIN_FILENO, TCSANOW, &polling) != 0) {
            return 0;
        }
        fd_set input;
        FD_ZERO(&input);
        FD_SET(STDIN_FILENO, &input);
        timeval timeout{};
        const int ready = ::select(STDIN_FILENO + 1, &input, nullptr, nullptr, &timeout);
        (void)::tcsetattr(STDIN_FILENO, TCSANOW, &original);
        return ready > 0 ? 1 : 0;
    }
}
#endif
