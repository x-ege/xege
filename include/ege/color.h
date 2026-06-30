#pragma once

#include <cstdint>

namespace ege
{

#ifndef EGE_COLOR_T_TYPEDEF
#define EGE_COLOR_T_TYPEDEF
/// @brief Color type definition, uses 32-bit unsigned integer to represent ARGB color
typedef uint32_t color_t;
#endif

inline constexpr color_t EGERGBA(color_t r, color_t g, color_t b, color_t a)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

inline constexpr color_t EGERGB(color_t r, color_t g, color_t b)
{
    return EGERGBA(r, g, b, 0xFF);
}

inline constexpr color_t EGEARGB(color_t a, color_t r, color_t g, color_t b)
{
    return EGERGBA(r, g, b, a);
}

inline constexpr color_t EGEACOLOR(color_t a, color_t color)
{
    return ((color & 0xFFFFFF) | (a << 24));
}

inline constexpr color_t EGECOLORA(color_t color, color_t a)
{
    return EGEACOLOR(a, color);
}

inline constexpr color_t EGEGET_R(color_t c)
{
    return ((c >> 16) & 0xFF);
}

inline constexpr color_t EGEGET_G(color_t c)
{
    return ((c >> 8) & 0xFF);
}

inline constexpr color_t EGEGET_B(color_t c)
{
    return (c & 0xFF);
}

inline constexpr color_t EGEGET_A(color_t c)
{
    return ((c >> 24) & 0xFF);
}

inline constexpr color_t EGEGRAY(color_t gray)
{
    return EGERGB(gray, gray, gray);
}

inline constexpr color_t EGEGRAYA(color_t gray, color_t a)
{
    return EGERGBA(gray, gray, gray, a);
}

inline constexpr color_t EGEAGRAY(color_t a, color_t gray)
{
    return EGEGRAYA(gray, a);
}

} // namespace ege

using ege::EGEACOLOR;
using ege::EGEAGRAY;
using ege::EGEARGB;
using ege::EGECOLORA;
using ege::EGEGET_A;
using ege::EGEGET_B;
using ege::EGEGET_G;
using ege::EGEGET_R;
using ege::EGEGRAY;
using ege::EGEGRAYA;
using ege::EGERGB;
using ege::EGERGBA;
