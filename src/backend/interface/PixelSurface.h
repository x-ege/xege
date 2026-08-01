#ifndef EGE_BACKEND_INTERFACE_PIXEL_SURFACE_H
#define EGE_BACKEND_INTERFACE_PIXEL_SURFACE_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace ege
{
namespace backend
{

/**
 * Owns the CPU-authoritative pixels for an EGE render target.
 *
 * Pixels are tightly packed, top-down, premultiplied BGRA bytes. On the
 * little-endian platforms supported by the native backends, Pixel therefore
 * has the same numeric representation as EGE's PRGB color_t: 0xAARRGGBB.
 * The allocation never changes during the surface lifetime, so a data()
 * pointer remains valid until the PixelSurface is destroyed.
 */
class PixelSurface
{
public:
    using Pixel = std::uint32_t;

    PixelSurface(std::size_t width, std::size_t height);
    ~PixelSurface() = default;

    PixelSurface(const PixelSurface&)            = delete;
    PixelSurface& operator=(const PixelSurface&) = delete;
    PixelSurface(PixelSurface&&)                 = delete;
    PixelSurface& operator=(PixelSurface&&)      = delete;

    std::size_t width() const noexcept { return width_; }

    std::size_t height() const noexcept { return height_; }

    std::size_t strideBytes() const noexcept { return strideBytes_; }

    std::size_t byteCount() const noexcept { return byteCount_; }

    std::size_t pixelCount() const noexcept { return byteCount_ / sizeof(Pixel); }

    Pixel* data() noexcept { return pixels_.get(); }

    const Pixel* data() const noexcept { return pixels_.get(); }

    Pixel*       row(std::size_t y);
    const Pixel* row(std::size_t y) const;

    Pixel getPixel(std::size_t x, std::size_t y) const;
    void  setPixel(std::size_t x, std::size_t y, Pixel pixel);
    void  clear(Pixel pixel) noexcept;

    /** Packs straight RGBA components into a premultiplied 0xAARRGGBB pixel. */
    static Pixel makePremultipliedPixel(
        std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) noexcept;

private:
    void checkCoordinates(std::size_t x, std::size_t y) const;

    const std::size_t        width_;
    const std::size_t        height_;
    const std::size_t        strideBytes_;
    const std::size_t        byteCount_;
    std::unique_ptr<Pixel[]> pixels_;
};

} // namespace backend
} // namespace ege

#endif // EGE_BACKEND_INTERFACE_PIXEL_SURFACE_H
