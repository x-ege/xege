#pragma once

#include <windef.h>
#include "ege_def.h"
#include "ege_math.h"
#include "type.h"

// 0xaarrggbb -> 0xaabbggrr
#define RGBTOBGR(color) ((color_t)((((color) & 0xFF) << 16) | (((color) & 0xFF0000) >> 16) | ((color) & 0xFF00FF00)))

// 将 color_t 与 Bitmap Buffer 所用的 0xaarrggbb 格式
// 转换为 COLORREF 所用的 0x00bbggrr，忽略 Alpha 通道
// 仅用于向 GDI32 API 传递颜色时
#define ARGBTOZBGR(c) ((color_t)((((c) & 0xFF) << 16) | (((c) & 0xFF0000) >> 16) | ((c) & 0xFF00)))

namespace ege
{

#ifndef EGE_COLOR_T_TYPEDEF
#define EGE_COLOR_T_TYPEDEF
typedef unsigned int color_t;
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

/**
 * @brief 以 dst 为背景色，src 为前景色进行混合
 *
 * @param dst 背景颜色
 * @param src 前景色
 * @param alpha 不透明度(0~255)
 * @return color_t: 混合后的颜色，保留 dst 的透明通道
 */
EGE_FORCEINLINE color_t alphablend_inline_fast(color_t dst, color_t src, byte alpha)
{
    uint32_t rb = dst & 0x00FF00FF;
    uint32_t g  = dst & 0x0000FF00;

    rb += ((src & 0x00FF00FF) - rb) * alpha >> 8;
    g  += ((src & 0x0000FF00) - g ) * alpha >> 8;
    return (rb & 0x00FF00FF) | (g & 0x0000FF00) | (dst & 0xFF000000);
}

/**
 * @brief 以 dst 为背景色，src 为前景色进行混合, alpha 取自前景色 src 的透明通道
 *
 * @param dst 背景色
 * @param src 前景色
 * @return color_t: 混合后的颜色，保留 dst 的透明通道
 */
EGE_FORCEINLINE color_t alphablend_inline_fast(color_t dst, color_t src)
{
    return alphablend_inline_fast(dst, src, EGEGET_A(src));
}

//
// RGB   = alpha * RGB(src) + (1.0 - alpha) * RGB(dst);
// alpha = alpha + (1.0 - alpha) * A(dst)
/**
 * @brief 以 dst 为背景色，src 为前景色进行混合
 *
 * @param dst 背景色
 * @param src 前景色
 * @param alpha 不透明度(0~255)
 * @return color_t: 混合后的颜色
 * @note 计算公式:
 * A = A(dst) + alpha * （1.0    - A(dst));
 * R = R(dst) + alpha * （R(src) - R(dst));
 * G = G(dst) + alpha * （G(src) - G(dst));
 * B = B(dst) + alpha * （B(src) - B(dst));
 */
EGE_FORCEINLINE color_t alphablend_inline(color_t dst, color_t src, byte alpha)
{
    const byte a = DIVIDE_255_FAST(255 * EGEGET_A(dst) + (255 - EGEGET_A(dst)) * alpha + 127);
    const byte r = DIVIDE_255_FAST(255 * EGEGET_R(dst) + ((int)(EGEGET_R(src) - EGEGET_R(dst)) * alpha + 127));
    const byte g = DIVIDE_255_FAST(255 * EGEGET_G(dst) + ((int)(EGEGET_G(src) - EGEGET_G(dst)) * alpha + 127));
    const byte b = DIVIDE_255_FAST(255 * EGEGET_B(dst) + ((int)(EGEGET_B(src) - EGEGET_B(dst)) * alpha + 127));

    return EGEARGB(a, r, g, b);
}

EGE_FORCEINLINE color_t alphablend_inline(color_t dst, color_t src)
{
    return alphablend_inline(dst, src, EGEGET_A(src));
}

} //namespace ege


