/**
 * @file  encodeconv.cpp
 * @brief 编码转换
 */
#include "ege_head.h"

#include "encodeconv.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <cwchar>
#include <cstdint>
#endif

namespace ege
{

#ifdef _WIN32
/**
 * @brief 将宽字符串转为多字节字符串（多字节字符编码由 setcodepage 确定）
 *
 * @param wStr 以终止字符结尾的宽字符串
 * @return std::string 转换后的字符串
 */
std::string w2mb(const wchar_t wStr[])
{
    unsigned int codepage = ege::getcodepage();
    int bufsize = WideCharToMultiByte(codepage, 0, wStr, -1, NULL, 0, 0, 0);
    std::string mbStr(bufsize, '\0');
    WideCharToMultiByte(codepage, 0, wStr, -1, &mbStr[0], bufsize, 0, 0);
    return mbStr;
}

std::string w2utf8(const wchar_t wStr[])
{
    if (!wStr) return "";

    const int required = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, NULL, 0, NULL, NULL);
    if (required <= 0) return "";

    std::string converted(static_cast<size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wStr, -1, &converted[0], required,
                            NULL, NULL) == 0) {
        return "";
    }
    converted.resize(static_cast<size_t>(required - 1));
    return converted;
}

/**
 * @brief 将多字节字符串转为宽字符串（多字节字符编码由 setcodepage 确定）
 *
 * @param mbStr 以终止字符结尾的多字节字符串
 * @return std::wstring 转换后的宽字符
 */
std::wstring mb2w(const char mbStr[])
{
    unsigned int codepage = ege::getcodepage();
    int bufsize = MultiByteToWideChar(codepage, 0, mbStr, -1, NULL, 0);
    std::wstring wStr(bufsize, L'\0');
    MultiByteToWideChar(codepage, 0, mbStr, -1, &wStr[0], bufsize);
    return wStr;
}
#else
namespace
{
std::wstring decodeUtf8(const char* text)
{
    std::wstring output;
    if (!text) return output;

    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != 0) {
        std::uint32_t codepoint = 0xFFFDU;
        std::size_t length = 1;
        if (*cursor < 0x80U) {
            codepoint = *cursor;
        } else if ((*cursor & 0xE0U) == 0xC0U && cursor[1] != 0) {
            codepoint = ((*cursor & 0x1FU) << 6U) | (cursor[1] & 0x3FU);
            length = codepoint >= 0x80U && (cursor[1] & 0xC0U) == 0x80U ? 2 : 1;
        } else if ((*cursor & 0xF0U) == 0xE0U && cursor[1] != 0 && cursor[2] != 0) {
            codepoint = ((*cursor & 0x0FU) << 12U) | ((cursor[1] & 0x3FU) << 6U) |
                (cursor[2] & 0x3FU);
            length = codepoint >= 0x800U && !(codepoint >= 0xD800U && codepoint <= 0xDFFFU) &&
                    (cursor[1] & 0xC0U) == 0x80U && (cursor[2] & 0xC0U) == 0x80U ? 3 : 1;
        } else if ((*cursor & 0xF8U) == 0xF0U && cursor[1] != 0 && cursor[2] != 0 && cursor[3] != 0) {
            codepoint = ((*cursor & 0x07U) << 18U) | ((cursor[1] & 0x3FU) << 12U) |
                ((cursor[2] & 0x3FU) << 6U) | (cursor[3] & 0x3FU);
            length = codepoint >= 0x10000U && codepoint <= 0x10FFFFU &&
                    (cursor[1] & 0xC0U) == 0x80U && (cursor[2] & 0xC0U) == 0x80U &&
                    (cursor[3] & 0xC0U) == 0x80U ? 4 : 1;
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

std::string w2mb(const wchar_t wStr[])
{
    if (!wStr) return "";

    const unsigned int codepage = ege::getcodepage();
    if (codepage != CP_ACP && codepage != CP_UTF8) {
        const size_t len = std::wcstombs(NULL, wStr, 0);
        if (len == (size_t)-1) return "";
        std::string converted(len, '\0');
        if (len != 0) std::wcstombs(&converted[0], wStr, len);
        return converted;
    }

    // Unix source files and font names are UTF-8. Encode directly instead of
    // depending on the process locale, which differs between macOS and Linux.
    std::string converted;
    for (size_t index = 0; wStr[index] != L'\0'; ++index) {
        uint32_t codepoint = static_cast<uint32_t>(wStr[index]);
        if (sizeof(wchar_t) == 2 && codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
            const uint32_t low = static_cast<uint32_t>(wStr[index + 1]);
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
}

std::string w2utf8(const wchar_t wStr[])
{
    if (!wStr) return "";

    std::string converted;
    for (size_t index = 0; wStr[index] != L'\0'; ++index) {
        uint32_t codepoint = static_cast<uint32_t>(wStr[index]);
        if (sizeof(wchar_t) == 2 && codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
            const uint32_t low = static_cast<uint32_t>(wStr[index + 1]);
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
}

std::wstring mb2w(const char mbStr[])
{
    if (!mbStr) return L"";

    const unsigned int codepage = ege::getcodepage();
    if (codepage != CP_ACP && codepage != CP_UTF8) {
        const size_t len = std::mbstowcs(NULL, mbStr, 0);
        if (len == (size_t)-1) return L"";
        std::wstring converted(len, L'\0');
        if (len != 0) std::mbstowcs(&converted[0], mbStr, len);
        return converted;
    }

    return decodeUtf8(mbStr);
}
#endif

std::wstring utf82w(const char utf8Str[])
{
    if (!utf8Str) return L"";
#ifdef _WIN32
    const int required = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (required <= 0) return L"";

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
