#include "ege_head.h"
#include "utils.h"

#include "external/sdefl.h"
#include "external/sinfl.h"

namespace ege
{

/**
 * @brief 根据原数据大小，计算数据压缩过程可能占用的最大缓冲区大小
 * @param dataSize 数据大小(以字节为单位)
 * @return 返回数据压缩过程中可能占用的最大缓冲区大小(以字节为单位)
 */
uint32_t ege_compress_bound(uint32_t dataSize)
{
    return (dataSize > INT_MAX) ? 0 : ((uint32_t)sdefl_bound(dataSize) + sizeof(uint32_t));
}

/**
 *
 * @param[out] compressData 指向用于存储压缩数据的缓冲区
 * @param[in, out] compressSize 缓冲区 compressData 的大小(以字节为单位)，应保证不小于由 ege_compress_bound() 给出的值。
 *         压缩完成后，compressSize 的值将是压缩数据的长度(以字节为单位)
 * @param[in] data 原数据
 * @param size 原数据大小(以字节为单位)
 * @return 错误码, @see graphics_errors
 */
int ege_compress(void* compressData, uint32_t* compressSize, const void* data, uint32_t size)
{
    return ege_compress2(compressData, compressSize, data, size, SDEFL_LVL_DEF);
}

/**
 *
 * @param[out] compressData 指向用于存储压缩数据的缓冲区
 * @param[in, out] compressSize 缓冲区 compressData 的大小(以字节为单位)，应保证不小于由 ege_compress_bound() 给出的值。
 *         压缩完成后，compressSize 的值将是压缩数据的长度(以字节为单位)
 * @param[in] data 原数据
 * @param size 原数据大小(以字节为单位)
 * @param level 压缩级别，范围 0 ~ 9，数字越大则压缩程度越高，压缩速度越慢。数值超出范围会被限制在边界值。
 * @return 错误码, @see graphics_errors
 */
int ege_compress2(void* compressData, uint32_t* compressSize, const void* data, uint32_t size, int level)
{
    if (compressData == NULL || compressSize == NULL || data == NULL || size <= 0) {
        return grParamError;
    }

    /* sdefl 数据长度使用 int, 最大只支持压缩 INT_MAX 长度的数据 */
    if (size > INT_MAX) {
        return grParamError;
    }

    /* sdefl 压缩级别范围为 0~8 */
    level = CLAMP(level, SDEFL_LVL_MIN, SDEFL_LVL_MAX);

    sdefl* sd = (sdefl*)malloc(sizeof(struct sdefl));

    if (sd == NULL) {
        return grOutOfMemory;
    }

    /* 前4个字节存储原数据长度 */
    memcpy(compressData, &size, sizeof(uint32_t));

    /* 输出的数据中，前4个字节用于存储原数据大小(小端)，之后才是存储压缩后的数据 */
    *compressSize = sdeflate(sd, (uint8_t*)compressData + sizeof(uint32_t), data, size, level) + sizeof(uint32_t);

    free(sd);

    return grOk;
}

uint32_t ege_uncompress_size(const void* compress, uint32_t compressSize)
{
    if (compress == NULL || compressSize <= sizeof(uint32_t)) {
        return 0;
    }

    uint32_t datasize;
    memcpy(&datasize, compress, sizeof(uint32_t));

    return datasize;
}

int ege_uncompress(void* buffer, uint32_t* bufferSize, const void* compressData, uint32_t compressSize)
{
    uint32_t dataSize = 0;
    graphics_errors error = grOk;

    if (buffer == NULL || bufferSize == NULL || compressData == NULL) {
        error = grNullPointer;
    } else if (*bufferSize <= 0 || compressSize <= sizeof(uint32_t) || compressSize > INT_MAX) {
        error = grParamError;
    } else {
        int capacity = MIN(*bufferSize, INT_MAX); /* sinfl 数据长度使用 int，避免转换溢出 */
        int length = sinflate(buffer, capacity, (uint8_t*)compressData + sizeof(uint32_t), compressSize - sizeof(uint32_t));
        if (length < 0) {
            error = grError;
        } else {
            dataSize = length;
        }
    }

    if (bufferSize != NULL) {
        *bufferSize = dataSize;
    }

    return error;
}

} // namespace ege
