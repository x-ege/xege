/**
 * @file  encodeconv_utf8.cpp
 * @brief 原生后端使用的 UTF-8 编码转换
 */
#include "encodeconv.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdint>

namespace
{

std::wstring decodeUtf8(const char* text)
{
    std::wstring output;
    if (!text) {
        return output;
    }

    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != 0) {
        std::uint32_t codepoint = 0xFFFDU;
        std::size_t   length    = 1;
        if (*cursor < 0x80U) {
            codepoint = *cursor;
        } else if ((*cursor & 0xE0U) == 0xC0U && cursor[1] != 0) {
            codepoint = ((*cursor & 0x1FU) << 6U) | (cursor[1] & 0x3FU);
            length    = codepoint >= 0x80U && (cursor[1] & 0xC0U) == 0x80U ? 2 : 1;
        } else if ((*cursor & 0xF0U) == 0xE0U && cursor[1] != 0 && cursor[2] != 0) {
            codepoint = ((*cursor & 0x0FU) << 12U) | ((cursor[1] & 0x3FU) << 6U) | (cursor[2] & 0x3FU);
            length    = codepoint >= 0x800U && !(codepoint >= 0xD800U && codepoint <= 0xDFFFU) &&
                    (cursor[1] & 0xC0U) == 0x80U && (cursor[2] & 0xC0U) == 0x80U ?
                   3 :
                   1;
        } else if ((*cursor & 0xF8U) == 0xF0U && cursor[1] != 0 && cursor[2] != 0 && cursor[3] != 0) {
            codepoint = ((*cursor & 0x07U) << 18U) | ((cursor[1] & 0x3FU) << 12U) | ((cursor[2] & 0x3FU) << 6U) |
                (cursor[3] & 0x3FU);
            length = codepoint >= 0x10000U && codepoint <= 0x10FFFFU && (cursor[1] & 0xC0U) == 0x80U &&
                    (cursor[2] & 0xC0U) == 0x80U && (cursor[3] & 0xC0U) == 0x80U ?
                4 :
                1;
        }
        if (length == 1 && *cursor >= 0x80U) {
            codepoint = 0xFFFDU;
        }

        if (sizeof(wchar_t) == 2 && codepoint > 0xFFFFU) {
            codepoint -= 0x10000U;
            output.push_back(static_cast<wchar_t>(0xD800U + (codepoint >> 10U)));
            output.push_back(static_cast<wchar_t>(0xDC00U + (codepoint & 0x3FFU)));
        } else {
            output.push_back(static_cast<wchar_t>(codepoint));
        }
        cursor += length;
    }
    return output;
}

} // namespace
#endif

namespace ege
{

std::string w2utf8(const wchar_t wStr[])
{
    if (!wStr) {
        return "";
    }

#ifdef _WIN32
    const int required = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, NULL, 0, NULL, NULL);
    if (required <= 0) {
        return "";
    }

    std::string converted(static_cast<size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wStr, -1, &converted[0], required, NULL, NULL) == 0) {
        return "";
    }
    converted.resize(static_cast<size_t>(required - 1));
    return converted;
#else
    std::string converted;
    for (size_t index = 0; wStr[index] != L'\0'; ++index) {
        std::uint32_t codepoint = static_cast<std::uint32_t>(wStr[index]);
        if (sizeof(wchar_t) == 2 && codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
            const std::uint32_t low = static_cast<std::uint32_t>(wStr[index + 1]);
            if (low >= 0xDC00U && low <= 0xDFFFU) {
                codepoint = 0x10000U + ((codepoint - 0xD800U) << 10) + (low - 0xDC00U);
                ++index;
            } else {
                codepoint = 0xFFFDU;
            }
        } else if ((codepoint >= 0xD800U && codepoint <= 0xDFFFU) || codepoint > 0x10FFFFU) {
            codepoint = 0xFFFDU;
        }

        if (codepoint <= 0x7FU) {
            converted.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FFU) {
            converted.push_back(static_cast<char>(0xC0U | (codepoint >> 6)));
            converted.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        } else if (codepoint <= 0xFFFFU) {
            converted.push_back(static_cast<char>(0xE0U | (codepoint >> 12)));
            converted.push_back(static_cast<char>(0x80U | ((codepoint >> 6) & 0x3FU)));
            converted.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        } else {
            converted.push_back(static_cast<char>(0xF0U | (codepoint >> 18)));
            converted.push_back(static_cast<char>(0x80U | ((codepoint >> 12) & 0x3FU)));
            converted.push_back(static_cast<char>(0x80U | ((codepoint >> 6) & 0x3FU)));
            converted.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
        }
    }
    return converted;
#endif
}

std::wstring utf82w(const char utf8Str[])
{
    if (!utf8Str) {
        return L"";
    }
#ifdef _WIN32
    const int required = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (required <= 0) {
        return L"";
    }

    std::wstring converted(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, &converted[0], required) == 0) {
        return L"";
    }
    converted.resize(static_cast<size_t>(required - 1));
    return converted;
#else
    return decodeUtf8(utf8Str);
#endif
}

} // namespace ege
