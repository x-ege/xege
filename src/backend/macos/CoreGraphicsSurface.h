#ifndef EGE_BACKEND_MACOS_CORE_GRAPHICS_SURFACE_H
#define EGE_BACKEND_MACOS_CORE_GRAPHICS_SURFACE_H

#include "backend/interface/PixelSurface.h"

#include <CoreGraphics/CoreGraphics.h>

#include <cstddef>

namespace ege
{
namespace backend
{

/**
 * A non-owning Core Graphics drawing context over a PixelSurface.
 *
 * CGBitmapContextCreate receives PixelSurface::data() directly. No staging
 * allocation or pixel copy is performed. The PixelSurface must outlive this
 * object. Coordinates use EGE's top-left origin with y increasing downward.
 */
class CoreGraphicsSurface
{
public:
    explicit CoreGraphicsSurface(PixelSurface& surface);
    ~CoreGraphicsSurface();

    CoreGraphicsSurface(const CoreGraphicsSurface&)            = delete;
    CoreGraphicsSurface& operator=(const CoreGraphicsSurface&) = delete;
    CoreGraphicsSurface(CoreGraphicsSurface&&)                 = delete;
    CoreGraphicsSurface& operator=(CoreGraphicsSurface&&)      = delete;

    PixelSurface& surface() noexcept { return surface_; }

    const PixelSurface& surface() const noexcept { return surface_; }

    CGContextRef context() noexcept { return context_; }

    CGContextRef context() const noexcept { return context_; }

    void clear(PixelSurface::Pixel pixel) noexcept;
    void flush() noexcept;
    void setAntialiasing(bool enabled) noexcept;
    void setBlendMode(CGBlendMode mode) noexcept;

    void drawLine(CGPoint start, CGPoint end, CGFloat lineWidth, PixelSurface::Pixel pixel);
    void fillRect(CGRect rect, PixelSurface::Pixel pixel);
    void strokeRect(CGRect rect, CGFloat lineWidth, PixelSurface::Pixel pixel);
    void fillEllipse(CGRect bounds, PixelSurface::Pixel pixel);
    void strokeEllipse(CGRect bounds, CGFloat lineWidth, PixelSurface::Pixel pixel);
    void fillPath(const CGPoint* points, std::size_t count, PixelSurface::Pixel pixel, bool closePath = true);
    void strokePath(
        const CGPoint* points, std::size_t count, CGFloat lineWidth, PixelSurface::Pixel pixel, bool closePath = false);

private:
    void setFillColor(PixelSurface::Pixel pixel) noexcept;
    void setStrokeColor(PixelSurface::Pixel pixel) noexcept;
    void addPath(const CGPoint* points, std::size_t count, bool closePath);

    PixelSurface& surface_;
    CGContextRef  context_;
};

} // namespace backend
} // namespace ege

#endif // EGE_BACKEND_MACOS_CORE_GRAPHICS_SURFACE_H
