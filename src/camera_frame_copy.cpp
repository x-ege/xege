#include "camera_frame_copy.h"

#include <climits>
#include <cstring>
#include <limits>

namespace ege
{
namespace detail
{

bool validateBgraFrameLayout(const BgraFrameView& frame,
                             size_t* rowBytes,
                             size_t* imageBytes)
{
    if (rowBytes) {
        *rowBytes = 0;
    }
    if (imageBytes) {
        *imageBytes = 0;
    }
    if (!frame.data || frame.width == 0 || frame.height == 0 ||
        frame.width > static_cast<uint32_t>(INT_MAX) ||
        frame.height > static_cast<uint32_t>(INT_MAX))
    {
        return false;
    }

    const size_t width = frame.width;
    const size_t height = frame.height;
    const size_t bytesPerPixel = 4;
    if (width > std::numeric_limits<size_t>::max() / bytesPerPixel) {
        return false;
    }
    const size_t activeRowBytes = width * bytesPerPixel;
    if (height > std::numeric_limits<size_t>::max() / activeRowBytes) {
        return false;
    }
    const size_t packedImageBytes = activeRowBytes * height;

    // IMAGE still contains legacy int arithmetic for pixel and byte counts.
    // Reject frames that cannot safely pass through those paths before an
    // IMAGE allocation is attempted.
    if (packedImageBytes > static_cast<size_t>(INT_MAX)) {
        return false;
    }

    const size_t sourceStride = frame.stride;
    if (sourceStride < activeRowBytes) {
        return false;
    }
    const size_t precedingRows = height - 1;
    if (precedingRows >
        (std::numeric_limits<size_t>::max() - activeRowBytes) / sourceStride)
    {
        return false;
    }
    const size_t requiredSourceBytes = precedingRows * sourceStride + activeRowBytes;
    if (frame.dataSize < requiredSourceBytes) {
        return false;
    }

    if (rowBytes) {
        *rowBytes = activeRowBytes;
    }
    if (imageBytes) {
        *imageBytes = packedImageBytes;
    }
    return true;
}

bool copyBgraFramePixels(void* destination,
                         size_t destinationSize,
                         const BgraFrameView& frame)
{
    size_t rowBytes = 0;
    size_t imageBytes = 0;
    if (!validateBgraFrameLayout(frame, &rowBytes, &imageBytes) ||
        !destination || destinationSize < imageBytes)
    {
        return false;
    }

    unsigned char* output = static_cast<unsigned char*>(destination);
    for (size_t y = 0; y < frame.height; ++y) {
        std::memcpy(output + y * rowBytes,
                    frame.data + y * static_cast<size_t>(frame.stride),
                    rowBytes);
    }
    return true;
}

} // namespace detail
} // namespace ege
