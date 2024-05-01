#pragma once

// MSVC 从 10.0（VS2010）开始有 stdint.h
// GCC 从 4.5 开始有 stdint.h
#if _MSC_VER >= 1600 || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#include <stdint.h>
#elif !defined(_MSC_VER) || _MSC_VER > 1300
#include "stdint.h"
#endif

#include <windows.h>

#if !defined(EGE_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && _MSC_VER >= 1300
#define EGE_W64 __w64
#else
#define EGE_W64
#endif
#endif

#ifndef __int3264
#if defined(_WIN64)
typedef __int64          LONG_PTR, *PLONG_PTR;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;

#define __int3264 __int64

#else
typedef EGE_W64 long          LONG_PTR, *PLONG_PTR;
typedef EGE_W64 unsigned long ULONG_PTR, *PULONG_PTR;

#define __int3264 __int32

#endif
#endif

typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;

typedef unsigned int uint32;

#if !defined(_MSC_VER) || _MSC_VER > 1200
typedef intptr_t POINTER_SIZE;
#else
typedef long POINTER_SIZE;
#endif

namespace ege
{


struct Point
{
    int x;
    int y;

public:
    Point() : x(0), y(0) {}
    Point(int x, int y) : x(x), y(y) {}
};


struct Size
{
    int width;
    int height;

public:
    Size() : width(0), height(0) {}
    Size(int width, int height) : width(width), height(height) {}
};

struct Rect
{
    int x;
    int y;
    int width;
    int height;

public:
    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}
    Rect(Point point, Size size) : x(point.x), y(point.y), width(size.width), height(size.height) {}
};

}
