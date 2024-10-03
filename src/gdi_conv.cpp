#include "ege_head.h"
#include "gdi_conv.h"

namespace ege
{

/**
 * 将ege_transform_matrix 转换为 Gdiplus::Matrix
 * matrix 参数不为 NULL 时返回 Matrix 对象指针，否则返回 NULL
 */
Gdiplus::Matrix* matrixConvert(const ege_transform_matrix* matrix)
{
    if (matrix != NULL) {
        return new Gdiplus::Matrix(
            matrix->m11, matrix->m12,
            matrix->m21, matrix->m22,
            matrix->m31, matrix->m32
        );
    }

    return NULL;
}

}
