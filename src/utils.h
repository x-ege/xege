#pragma once

#include "ege_def.h"
#include <type_traits>

template <typename T> EGE_CONSTEXPR T min(T a)
{
    return a;
}

template <typename T, typename... Args> EGE_CONSTEXPR T min(T a, Args... args)
{
    static_assert((std::is_same_v<T, Args> && ...), "min() 的所有参数必须是相同类型");
    T b = min(args...);
    return (a < b) ? a : b;
}

template <typename T> EGE_CONSTEXPR T max(T a)
{
    return a;
}

template <typename T, typename... Args> EGE_CONSTEXPR T max(T a, Args... args)
{
    static_assert((std::is_same_v<T, Args> && ...), "max() 的所有参数必须是相同类型");
    T b = max(args...);
    return (a > b) ? a : b;
}

namespace ege
{} // namespace ege
