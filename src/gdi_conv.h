#pragma once

#include "ege_head.h"

namespace ege
{
    /* 矩阵类型转换 */
#ifdef EGE_GDIPLUS
    void matrixConvert(const ege_transform_matrix& from, Gdiplus::Matrix& to);
#endif
}
