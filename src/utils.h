#pragma once

// 交换 a 和 b 的值(t 为临时变量)
#ifndef SWAP
#define SWAP(a, b, t) \
    {                    \
        t = a;         \
        a = b;         \
        b = t;         \
    }
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(a, min, max)  (((a) < (min)) ? (min) : ((a) > (max) ? (max) : (a)))
#endif


namespace ege
{

enum PathType
{
    PathType_NONE     = 0,   /* 未检测或检测失败 */
    PathType_INVALID  = 1,   /* 无效路径 */
    PathType_NOTEXIST = 2,   /* 路径不存在 */
    PathType_FILE     = 3,   /* 文件路径 */
    PathType_DIR      = 4    /* 目录路径 */
};

/* 判断路径是否存在，路径类型由 pathType 返回 */
bool isPathExist(const wchar_t* path, PathType* pathType = NULL);

/* 判断文件是否存在 */
bool isFileExist(const wchar_t* filePath);

/* 判断目录是否存在 */
bool isDirExist(const wchar_t* dirPath);


bool startsWith(const char* str, const char* prefix);

inline bool isEmpty(const char* str)
{
    return (str == NULL) || (str[0] == '\0');
}

inline bool isEmpty(const wchar_t* str)
{
    return (str == NULL) || (str[0] == L'\0');
}

} // namespace ege
