/*
 * Minimal Win32 source-compatibility declarations for EGE's public API.
 *
 * EGE historically exposed a small number of Win32 types and constants from
 * its public header.  Native non-Windows builds still need those declarations
 * in order to preserve the source-level API, but they must not pull in a
 * Windows SDK or pretend that Win32 functions exist on the host platform.
 *
 * Keep this file limited to data types, calling-convention macros, and numeric
 * constants used by public declarations.  Platform implementations belong in
 * their respective backend, not in this compatibility header.
 */

#ifndef EGE_WIN32_COMPAT_H
#define EGE_WIN32_COMPAT_H

#if !defined(_WIN32)

#include <cstddef>
#include <cstdint>
#include <cwchar>

/*
 * Objective-C defines BOOL in <objc/objc.h>.  Reuse that definition for every
 * Apple translation unit, including plain C++, so EGE and AppKit headers can
 * be included in either order without changing the type across translation
 * units.  Other non-Windows platforms keep the Win32-compatible 32-bit type.
 */
#if defined(__APPLE__)
#include <objc/objc.h>
#endif

typedef std::uint8_t  BYTE;
typedef std::uint16_t WORD;
typedef std::uint32_t DWORD;
typedef std::int32_t  LONG;
typedef std::uint32_t UINT;
#if !defined(__APPLE__)
typedef std::int32_t  BOOL;
#endif

typedef std::intptr_t  LONG_PTR;
typedef std::uintptr_t ULONG_PTR;
typedef std::intptr_t  INT_PTR;
typedef std::uintptr_t UINT_PTR;
typedef ULONG_PTR      DWORD_PTR;
typedef LONG_PTR       LRESULT;
typedef ULONG_PTR      WPARAM;
typedef LONG_PTR       LPARAM;

typedef char     CHAR;
typedef wchar_t  WCHAR;
typedef CHAR*    LPSTR;
typedef WCHAR*   LPWSTR;
typedef const CHAR*  LPCSTR;
typedef const WCHAR* LPCWSTR;

#if defined(UNICODE) || defined(_UNICODE)
typedef WCHAR  TCHAR;
typedef LPWSTR LPTSTR;
#else
typedef CHAR  TCHAR;
typedef LPSTR LPTSTR;
#endif

typedef void* HANDLE;
typedef void* LPVOID;
typedef void* PVOID;

/* Match the type relationships used by the Windows SDK for opaque handles. */
struct HWND__;
struct HDC__;
struct HINSTANCE__;
struct HICON__;
struct HMENU__;
struct HBRUSH__;
struct HBITMAP__;
struct HFONT__;

typedef HWND__*      HWND;
typedef HDC__*       HDC;
typedef HINSTANCE__* HINSTANCE;
typedef HINSTANCE    HMODULE;
typedef HICON__*     HICON;
typedef HMENU__*     HMENU;
typedef HBRUSH__*    HBRUSH;
typedef HBITMAP__*   HBITMAP;
typedef HFONT__*     HFONT;

typedef DWORD COLORREF;
typedef WORD  ATOM;
typedef DWORD* PDWORD;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *NPPOINT, *LPPOINT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *NPRECT, *LPRECT;

#ifndef LF_FACESIZE
#define LF_FACESIZE 32
#endif

typedef struct tagLOGFONTA {
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    CHAR lfFaceName[LF_FACESIZE];
} LOGFONTA, *PLOGFONTA, *LPLOGFONTA;

typedef struct tagLOGFONTW {
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    WCHAR lfFaceName[LF_FACESIZE];
} LOGFONTW, *PLOGFONTW, *LPLOGFONTW;

#if defined(UNICODE) || defined(_UNICODE)
typedef LOGFONTW  LOGFONT;
typedef PLOGFONTW PLOGFONT;
typedef LPLOGFONTW LPLOGFONT;
#else
typedef LOGFONTA  LOGFONT;
typedef PLOGFONTA PLOGFONT;
typedef LPLOGFONTA LPLOGFONT;
#endif

#ifndef __cdecl
#define __cdecl
#endif

#ifndef __stdcall
#define __stdcall
#endif

#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

#ifndef WINAPI
#define WINAPI __stdcall
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef CW_USEDEFAULT
#define CW_USEDEFAULT static_cast<int>(0x80000000u)
#endif

#ifndef TRANSPARENT
#define TRANSPARENT 1
#endif

#ifndef OPAQUE
#define OPAQUE 2
#endif

#ifndef PS_SOLID
#define PS_SOLID 0
#define PS_DASH 1
#define PS_DOT 2
#define PS_DASHDOT 3
#define PS_DASHDOTDOT 4
#define PS_NULL 5
#define PS_INSIDEFRAME 6
#define PS_USERSTYLE 7
#define PS_ALTERNATE 8
#endif

#ifndef R2_BLACK
#define R2_BLACK 1
#endif
#ifndef R2_NOTMERGEPEN
#define R2_NOTMERGEPEN 2
#endif
#ifndef R2_MASKNOTPEN
#define R2_MASKNOTPEN 3
#endif
#ifndef R2_NOTCOPYPEN
#define R2_NOTCOPYPEN 4
#endif
#ifndef R2_MASKPENNOT
#define R2_MASKPENNOT 5
#endif
#ifndef R2_NOT
#define R2_NOT 6
#endif
#ifndef R2_XORPEN
#define R2_XORPEN 7
#endif
#ifndef R2_NOTMASKPEN
#define R2_NOTMASKPEN 8
#endif
#ifndef R2_MASKPEN
#define R2_MASKPEN 9
#endif
#ifndef R2_NOTXORPEN
#define R2_NOTXORPEN 10
#endif
#ifndef R2_NOP
#define R2_NOP 11
#endif
#ifndef R2_MERGENOTPEN
#define R2_MERGENOTPEN 12
#endif
#ifndef R2_COPYPEN
#define R2_COPYPEN 13
#endif
#ifndef R2_MERGEPENNOT
#define R2_MERGEPENNOT 14
#endif
#ifndef R2_MERGEPEN
#define R2_MERGEPEN 15
#endif
#ifndef R2_WHITE
#define R2_WHITE 16
#endif

#ifndef SRCCOPY
#define SRCCOPY     static_cast<DWORD>(0x00CC0020u)
#define SRCPAINT    static_cast<DWORD>(0x00EE0086u)
#define SRCAND      static_cast<DWORD>(0x008800C6u)
#define SRCINVERT   static_cast<DWORD>(0x00660046u)
#define SRCERASE    static_cast<DWORD>(0x00440328u)
#define NOTSRCCOPY  static_cast<DWORD>(0x00330008u)
#define NOTSRCERASE static_cast<DWORD>(0x001100A6u)
#define MERGECOPY   static_cast<DWORD>(0x00C000CAu)
#define MERGEPAINT  static_cast<DWORD>(0x00BB0226u)
#define PATCOPY     static_cast<DWORD>(0x00F00021u)
#define PATPAINT    static_cast<DWORD>(0x00FB0A09u)
#define PATINVERT   static_cast<DWORD>(0x005A0049u)
#define DSTINVERT   static_cast<DWORD>(0x00550009u)
#define BLACKNESS   static_cast<DWORD>(0x00000042u)
#define WHITENESS   static_cast<DWORD>(0x00FF0062u)
#endif

#ifndef CP_ACP
#define CP_ACP 0
#endif

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

#ifndef WM_KEYFIRST
#define WM_KEYFIRST 0x0100
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_KEYLAST 0x0109
#endif

#ifndef WM_MOUSEFIRST
#define WM_MOUSEFIRST 0x0200
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_XBUTTONDBLCLK 0x020D
#define WM_MOUSELAST 0x020D
#endif

#ifndef MK_LBUTTON
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT 0x0004
#define MK_CONTROL 0x0008
#define MK_MBUTTON 0x0010
#define MK_XBUTTON1 0x0020
#define MK_XBUTTON2 0x0040
#endif

#ifndef XBUTTON1
#define XBUTTON1 0x0001
#define XBUTTON2 0x0002
#endif

#ifndef MAKELPARAM
#define MAKELPARAM(low, high) \
    static_cast<LPARAM>(static_cast<std::uint16_t>(low) | \
                        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16))
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(value) \
    static_cast<int>(static_cast<std::int16_t>(static_cast<std::uintptr_t>(value) & 0xFFFFu))
#define GET_Y_LPARAM(value) \
    static_cast<int>(static_cast<std::int16_t>((static_cast<std::uintptr_t>(value) >> 16) & 0xFFFFu))
#define GET_WHEEL_DELTA_WPARAM(value) \
    static_cast<int>(static_cast<std::int16_t>((static_cast<std::uintptr_t>(value) >> 16) & 0xFFFFu))
#define GET_XBUTTON_WPARAM(value) \
    static_cast<UINT>((static_cast<std::uintptr_t>(value) >> 16) & 0xFFFFu)
#endif

#ifndef VK_LBUTTON
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#define VK_SPACE 0x20
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_NUMPAD0 0x60
#define VK_F2 0x71
#endif

#endif /* !defined(_WIN32) */

#endif /* EGE_WIN32_COMPAT_H */
