#pragma once

#include <type_traits>

template <typename T> void swap(T& a, T& b)
{
    T t = a;
    a   = b;
    b   = t;
}

template <typename T> constexpr T min(T a)
{
    return a;
}

template <typename T, typename... Args> constexpr T min(T a, Args... args)
{
    static_assert((std::is_same_v<T, Args> && ...), "min() 的所有参数必须是相同类型");
    T b = min(args...);
    return (a < b) ? a : b;
}

template <typename T> constexpr T max(T a)
{
    return a;
}

template <typename T, typename... Args> constexpr T max(T a, Args... args)
{
    static_assert((std::is_same_v<T, Args> && ...), "max() 的所有参数必须是相同类型");
    T b = max(args...);
    return (a > b) ? a : b;
}

namespace ege
{} // namespace ege
