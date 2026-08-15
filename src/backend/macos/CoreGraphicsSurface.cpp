#include "backend/macos/CoreGraphicsSurface.h"

#include <algorithm>
#include <stdexcept>

namespace ege
{
namespace backend
{
namespace
{

struct StraightColor
{
    CGFloat red;
    CGFloat green;
    CGFloat blue;
    CGFloat alpha;
};

StraightColor toStraightColor(PixelSurface::Pixel pixel) noexcept
{
    const unsigned int alphaByte = (pixel >> 24U) & 0xFFU;
    if (alphaByte == 0) {
        return {0.0, 0.0, 0.0, 0.0};
    }

    const CGFloat alpha        = static_cast<CGFloat>(alphaByte) / 255.0;
    const CGFloat premulRed    = static_cast<CGFloat>((pixel >> 16U) & 0xFFU) / 255.0;
    const CGFloat premulGreen  = static_cast<CGFloat>((pixel >> 8U) & 0xFFU) / 255.0;
    const CGFloat premulBlue   = static_cast<CGFloat>(pixel & 0xFFU) / 255.0;
    const CGFloat inverseAlpha = 1.0 / alpha;

    return {std::min<CGFloat>(1.0, premulRed * inverseAlpha), std::min<CGFloat>(1.0, premulGreen * inverseAlpha),
        std::min<CGFloat>(1.0, premulBlue * inverseAlpha), alpha};
}

} // namespace

CoreGraphicsSurface::CoreGraphicsSurface(PixelSurface& surface) : surface_(surface), context_(nullptr)
{
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (colorSpace == nullptr) {
        throw std::runtime_error("Unable to create the Core Graphics device RGB color space");
    }

    const CGBitmapInfo bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst;
    context_                      = CGBitmapContextCreate(
        surface_.data(), surface_.width(), surface_.height(), 8, surface_.strideBytes(), colorSpace, bitmapInfo);
    CGColorSpaceRelease(colorSpace);

    if (context_ == nullptr) {
        throw std::runtime_error("Unable to create a Core Graphics bitmap context for PixelSurface");
    }

    // Core Graphics defaults to a bottom-left user-space origin. EGE surfaces
    // and their underlying rows are top-down, so expose top-left coordinates.
    CGContextTranslateCTM(context_, 0.0, static_cast<CGFloat>(surface_.height()));
    CGContextScaleCTM(context_, 1.0, -1.0);
}

CoreGraphicsSurface::~CoreGraphicsSurface()
{
    CGContextRelease(context_);
}

void CoreGraphicsSurface::clear(PixelSurface::Pixel pixel) noexcept
{
    surface_.clear(pixel);
}

void CoreGraphicsSurface::flush() noexcept
{
    CGContextFlush(context_);
}

void CoreGraphicsSurface::setAntialiasing(bool enabled) noexcept
{
    CGContextSetShouldAntialias(context_, enabled);
    CGContextSetAllowsAntialiasing(context_, enabled);
}

void CoreGraphicsSurface::setBlendMode(CGBlendMode mode) noexcept
{
    CGContextSetBlendMode(context_, mode);
}

void CoreGraphicsSurface::drawLine(CGPoint start, CGPoint end, CGFloat lineWidth, PixelSurface::Pixel pixel)
{
    CGContextBeginPath(context_);
    CGContextMoveToPoint(context_, start.x, start.y);
    CGContextAddLineToPoint(context_, end.x, end.y);
    CGContextSetLineWidth(context_, lineWidth);
    setStrokeColor(pixel);
    CGContextStrokePath(context_);
}

void CoreGraphicsSurface::fillRect(CGRect rect, PixelSurface::Pixel pixel)
{
    setFillColor(pixel);
    CGContextFillRect(context_, rect);
}

void CoreGraphicsSurface::strokeRect(CGRect rect, CGFloat lineWidth, PixelSurface::Pixel pixel)
{
    CGContextSetLineWidth(context_, lineWidth);
    setStrokeColor(pixel);
    CGContextStrokeRect(context_, rect);
}

void CoreGraphicsSurface::fillEllipse(CGRect bounds, PixelSurface::Pixel pixel)
{
    setFillColor(pixel);
    CGContextFillEllipseInRect(context_, bounds);
}

void CoreGraphicsSurface::strokeEllipse(CGRect bounds, CGFloat lineWidth, PixelSurface::Pixel pixel)
{
    CGContextSetLineWidth(context_, lineWidth);
    setStrokeColor(pixel);
    CGContextStrokeEllipseInRect(context_, bounds);
}

void CoreGraphicsSurface::fillPath(const CGPoint* points, std::size_t count, PixelSurface::Pixel pixel, bool closePath)
{
    addPath(points, count, closePath);
    setFillColor(pixel);
    CGContextFillPath(context_);
}

void CoreGraphicsSurface::strokePath(
    const CGPoint* points, std::size_t count, CGFloat lineWidth, PixelSurface::Pixel pixel, bool closePath)
{
    addPath(points, count, closePath);
    CGContextSetLineWidth(context_, lineWidth);
    setStrokeColor(pixel);
    CGContextStrokePath(context_);
}

void CoreGraphicsSurface::setFillColor(PixelSurface::Pixel pixel) noexcept
{
    const StraightColor color = toStraightColor(pixel);
    CGContextSetRGBFillColor(context_, color.red, color.green, color.blue, color.alpha);
}

void CoreGraphicsSurface::setStrokeColor(PixelSurface::Pixel pixel) noexcept
{
    const StraightColor color = toStraightColor(pixel);
    CGContextSetRGBStrokeColor(context_, color.red, color.green, color.blue, color.alpha);
}

void CoreGraphicsSurface::addPath(const CGPoint* points, std::size_t count, bool closePath)
{
    if (count == 0) {
        throw std::invalid_argument("A Core Graphics path must contain at least one point");
    }
    if (points == nullptr) {
        throw std::invalid_argument("Core Graphics path points must not be null");
    }

    CGContextBeginPath(context_);
    CGContextMoveToPoint(context_, points[0].x, points[0].y);
    for (std::size_t i = 1; i < count; ++i) {
        CGContextAddLineToPoint(context_, points[i].x, points[i].y);
    }
    if (closePath) {
        CGContextClosePath(context_);
    }
}

} // namespace backend
} // namespace ege
