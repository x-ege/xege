#ifndef EGE_CAMERA_FRAME_COPY_H
#define EGE_CAMERA_FRAME_COPY_H

#include <stddef.h>

#include "ege/stdint.h"

namespace ege
{
namespace detail
{

struct BgraFrameView
{
    const unsigned char* data;
    size_t               dataSize;
    uint32_t             width;
    uint32_t             height;
    uint32_t             stride;
};

bool validateBgraFrameLayout(const BgraFrameView& frame,
                             size_t* rowBytes,
                             size_t* imageBytes);

bool copyBgraFramePixels(void* destination,
                         size_t destinationSize,
                         const BgraFrameView& frame);

} // namespace detail
} // namespace ege

#endif
