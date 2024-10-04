#include "ege_head.h"
#include "gdi_conv.h"

namespace ege
{

/**
 * 将 ege_transform_matrix 类型转换为 Gdiplus::Matrix 类型
 * @param[in]  from  输入的矩阵
 * @param[out] to    保存转换结果
 * @note 如果 from 参数为 NULL，输出结果为单位矩阵。
 */
void matrixConvert(const ege_transform_matrix* from, Gdiplus::Matrix& to)
{
    if (from) {
        to.SetElements(from->m11, from->m12, from->m21, from->m22, from->m31, from->m32);
    } else {
        to.Reset();
    }
}

}
