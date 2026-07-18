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

    const int required = MultiByteToWideChar(CP_UTF8, 0, mbStr, -1, NULL, 0);
    if (required <= 0) return L"";
    std::wstring converted(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, mbStr, -1, &converted[0], required) == 0) {
        return L"";
    }
    converted.resize(static_cast<size_t>(required - 1));
    return converted;
}
#endif

} // namespace ege
