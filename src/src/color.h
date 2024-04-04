#pragma once

#include <windef.h>
#include "ege_def.h"

// 0xaarrggbb -> 0xaabbggrr
#define RGBTOBGR(color) ((color_t)((((color) & 0xFF) << 16) | (((color) & 0xFF0000) >> 16) | ((color) & 0xFF00FF00)))

// 将 color_t 与 Bitmap Buffer 所用的 0xaarrggbb 格式
// 转换为 COLORREF 所用的 0x00bbggrr，忽略 Alpha 通道
// 仅用于向 GDI32 API 传递颜色时
#define ARGBTOZBGR(c) ((color_t)((((c) & 0xFF) << 16) | (((c) & 0xFF0000) >> 16) | ((c) & 0xFF00)))

namespace ege
{

#ifndef EGE_COLOR_TYPEDEF
#define EGE_COLOR_TYPEDEF
typedef DWORD color_t;
#endif

typedef struct COLORHSL
{
    float h;
    float s;
    float l;
} COLORHSL;

typedef struct COLORHSV
{
    float h;
    float s;
    float v;
} COLORHSV;

typedef struct COLORRGB
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} COLORRGB;

// 以 bkg 为背景色，src 为前景色，alpha 为 0~255 的整数进行混合，
// 混合结果保留 bkg 的 Alpha 通道
EGE_FORCEINLINE color_t alphablend_inline(color_t bkg, color_t src, unsigned char alpha)
{
    DWORD rb = bkg & 0x00FF00FF;
    DWORD g  = bkg & 0x0000FF00;

    rb += ((src & 0x00FF00FF) - rb) * alpha >> 8;
    g += ((src & 0x0000FF00) - g) * alpha >> 8;
    return (rb & 0x00FF00FF) | (g & 0x0000FF00) | (bkg & 0xFF000000);
}

} //namespace ege


