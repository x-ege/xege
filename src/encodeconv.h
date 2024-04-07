/**
 * @file  encodeconv.h
 * @brief 编码转换
 */

#pragma once

#include <string>

namespace ege
{

// convert wide char string to multibyte ANSI string
std::string w2mb(const wchar_t wStr[]);

// convert multibyte ANSI string to wide char string
std::wstring mb2w(const char mbStr[]);

}
