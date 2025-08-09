#include "ege_head.h"
#include "ege_common.h"

#include "utils.h"

namespace  ege
{

bool isPathExist(const wchar_t* path, PathType* pathType)
{
    if (isEmpty(path))
        return false;

    bool exist = false;
    PathType type;

    DWORD attribute = GetFileAttributesW(path);
    if (attribute == INVALID_FILE_ATTRIBUTES) {
        type = (GetLastError() == ERROR_FILE_NOT_FOUND) ? PathType_NOTEXIST : PathType_NONE;
    } else {
        type = (attribute & FILE_ATTRIBUTE_DIRECTORY) ? PathType_DIR : PathType_FILE;
        exist = true;
    }

    if (pathType != NULL) {
        *pathType = type;
    }

    return exist;
}

bool isFileExist(const wchar_t* filePath)
{
    PathType pathType;
    return isPathExist(filePath, &pathType) && (pathType == PathType_FILE);
}

bool isDirExist(const wchar_t* dirPath)
{
    PathType pathType;
    return isPathExist(dirPath, &pathType) && (pathType == PathType_DIR);
}

bool startsWith(const char* str, const char* prefix)
{
    if (isEmpty(str))
        return false;

    if (isEmpty(prefix))
        return true;

    for (int i = 0; prefix[i] != '\0'; i++) {
        if (str[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

}