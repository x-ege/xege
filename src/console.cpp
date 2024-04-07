#include "ege_head.h"

#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <wincon.h>

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

BOOL init_console()
{
    HMENU hMenu;
    int hCrt;
    FILE* hf;
    if (hInput != NULL) {
        return FALSE;
    }
    if (GetConsoleWindow() != NULL) {
        return FALSE;
    }
    if (!AllocConsole()) {
        return FALSE;
    }
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);

    if (INVALID_HANDLE_VALUE == hOutput) {
        return FALSE;
    }
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (INVALID_HANDLE_VALUE == hInput) {
        return FALSE;
    }
    SetConsoleTitle("EGE CONSOLE");
    hConsoleWnd = GetConsoleWindow();
    if (INVALID_HANDLE_VALUE == hConsoleWnd) {
        return FALSE;
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
    return TRUE;
}

void clear_console()
{
    /***************************************/
    // This code is from one of Microsoft's
    // knowledge base articles, you can find it at
    // http://support.microsoft.com/default.aspx?scid=KB;EN-US;q99261&
    /***************************************/

    COORD coordScreen = {0, 0};

    BOOL bSuccess;
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */
    DWORD dwConSize;

    if (hOutput == NULL) {
        return;
    }

    /* get the number of character cells in the current buffer */

    bSuccess = GetConsoleScreenBufferInfo(hOutput, &csbi);

    if (!bSuccess) {
        return;
    }

    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    /* fill the entire screen with blanks */

    bSuccess = FillConsoleOutputCharacter(hOutput, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten);
    if (!bSuccess) {
        return;
    }

    /* get the current text attribute */

    bSuccess = GetConsoleScreenBufferInfo(hOutput, &csbi);
    if (!bSuccess) {
        return;
    }

    /* now set the buffer's attributes accordingly */

    bSuccess = FillConsoleOutputAttribute(hOutput, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    if (!bSuccess) {
        return;
    }

    /* put the cursor at (0, 0) */

    bSuccess = SetConsoleCursorPosition(hOutput, coordScreen);
    if (!bSuccess) {
        return;
    }
}

BOOL show_console()
{
    CHECK_CONSOLE_HANDLE(hConsoleWnd);
    return ShowWindow(hConsoleWnd, SW_SHOW);
}

BOOL hide_console()
{
    CHECK_CONSOLE_HANDLE(hConsoleWnd);
    return ShowWindow(hConsoleWnd, SW_HIDE);
}

BOOL close_console()
{
    if (!FreeConsole()) {
        return FALSE;
    }

    hOutput = NULL;
    hInput = NULL;
    hConsoleWnd = NULL;
    *stdout = fOldOut;
    *stdin = fOldIn;
    return TRUE;
};

#endif

}
