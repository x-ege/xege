#include "backend/interface/PixelSurface.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ege
{
namespace backend
{
namespace
{

std::size_t checkedStride(std::size_t width)
{
    if (width == 0) {
        throw std::invalid_argument("PixelSurface width must be greater than zero");
    }
    if (width > std::numeric_limits<std::size_t>::max() / sizeof(PixelSurface::Pixel)) {
        throw std::length_error("PixelSurface row byte count overflows size_t");
    }
    return width * sizeof(PixelSurface::Pixel);
}

std::size_t checkedByteCount(std::size_t strideBytes, std::size_t height)
{
    if (height == 0) {
        throw std::invalid_argument("PixelSurface height must be greater than zero");
    }
    // A single C++ object cannot be addressed beyond PTRDIFF_MAX.  Reject
    // such dimensions before entering the allocator; some allocators abort
    // for an impossible request instead of reporting std::bad_alloc.
    const std::size_t maxObjectBytes =
        static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    if (height > maxObjectBytes / strideBytes) {
        throw std::length_error("PixelSurface byte count exceeds the addressable object size");
    }
    return strideBytes * height;
}

std::uint32_t premultiply(std::uint8_t component, std::uint8_t alpha) noexcept
{
    return (static_cast<std::uint32_t>(component) * alpha + 127U) / 255U;
}

} // namespace

PixelSurface::PixelSurface(std::size_t width, std::size_t height) :
    width_(width),
    height_(height),
    strideBytes_(checkedStride(width)),
    byteCount_(checkedByteCount(strideBytes_, height)),
    pixels_(new Pixel[byteCount_ / sizeof(Pixel)]())
{}

PixelSurface::Pixel* PixelSurface::row(std::size_t y)
{
    if (y >= height_) {
        throw std::out_of_range("PixelSurface row is outside the surface");
    }
    return pixels_.get() + y * width_;
}

const PixelSurface::Pixel* PixelSurface::row(std::size_t y) const
{
    if (y >= height_) {
        throw std::out_of_range("PixelSurface row is outside the surface");
    }
    return pixels_.get() + y * width_;
}

PixelSurface::Pixel PixelSurface::getPixel(std::size_t x, std::size_t y) const
{
    checkCoordinates(x, y);
    return pixels_[y * width_ + x];
}

void PixelSurface::setPixel(std::size_t x, std::size_t y, Pixel pixel)
{
    checkCoordinates(x, y);
    pixels_[y * width_ + x] = pixel;
}

void PixelSurface::clear(Pixel pixel) noexcept
{
    std::fill_n(pixels_.get(), pixelCount(), pixel);
}

PixelSurface::Pixel PixelSurface::makePremultipliedPixel(
    std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) noexcept
{
    return (static_cast<Pixel>(alpha) << 24U) | (premultiply(red, alpha) << 16U) | (premultiply(green, alpha) << 8U) |
        premultiply(blue, alpha);
}

void PixelSurface::checkCoordinates(std::size_t x, std::size_t y) const
{
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("PixelSurface pixel is outside the surface");
    }
}

} // namespace backend
} // namespace ege
