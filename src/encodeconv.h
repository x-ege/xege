/**
 * @file  encodeconv.h
 * @brief 编码转换
 */

#pragma once

#include <string>

namespace ege
{

// convert wide char string to multibyte string, using ege::getcodepage
std::string w2mb(const wchar_t wStr[]);

// convert a wide string to UTF-8 for native window and rendering backends
std::string w2utf8(const wchar_t wStr[]);

// convert UTF-8 from backend-facing APIs to a wide string
std::wstring utf82w(const char utf8Str[]);

// convert multibyte string to wide char string, using ege::getcodepage
std::wstring mb2w(const char mbStr[]);

}
