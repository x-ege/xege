#include "backend/macos/CoreGraphicsRenderTarget.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ege
{
namespace backend
{
namespace
{

constexpr double kPi = 3.14159265358979323846;

unsigned int channel(color_t color, unsigned int shift) noexcept
{
    return (color >> shift) & 0xFFU;
}

unsigned int scaleByte(unsigned int value, unsigned int factor) noexcept
{
    return (value * factor + 127U) / 255U;
}

color_t pack(unsigned int alpha, unsigned int red, unsigned int green, unsigned int blue) noexcept
{
    return (std::min(255U, alpha) << 24U) | (std::min(255U, red) << 16U) | (std::min(255U, green) << 8U) |
        std::min(255U, blue);
}

bool patternUsesForeground(FillStyle style, int x, int y) noexcept
{
    const int slash     = (x + y) & 7;
    const int backslash = (x - y) & 7;
    switch (style) {
    case FILL_EMPTY:
        return false;
    case FILL_HORIZONTAL:
        return (y & 7) == 0;
    case FILL_LIGHT_SLASH:
        return slash == 0;
    case FILL_SLASH:
        return slash <= 1;
    case FILL_BACKSLASH:
        return backslash <= 1;
    case FILL_LIGHT_BACKSLASH:
        return backslash == 0;
    case FILL_HATCH:
        return (x & 7) == 0 || (y & 7) == 0;
    case FILL_CROSS_HATCH:
        return slash <= 1 || backslash <= 1;
    case FILL_INTERLEAVE:
        return ((y & 7) == 0 && (x & 7) < 4) || ((y & 7) == 4 && (x & 7) >= 4);
    case FILL_WIDE_DOT:
        return (x & 7) == 0 && (y & 7) == 0;
    case FILL_CLOSE_DOT:
        return (x & 3) == 0 && (y & 3) == 0;
    case FILL_USER:
    case FILL_SOLID:
    default:
        return true;
    }
}

CGLineCap lineCap(RTLineCap cap) noexcept
{
    switch (cap) {
    case RT_LINECAP_ROUND:
        return kCGLineCapRound;
    case RT_LINECAP_SQUARE:
        return kCGLineCapSquare;
    case RT_LINECAP_FLAT:
    default:
        return kCGLineCapButt;
    }
}

CGLineJoin lineJoin(RTLineJoin join) noexcept
{
    switch (join) {
    case RT_LINEJOIN_BEVEL:
        return kCGLineJoinBevel;
    case RT_LINEJOIN_ROUND:
        return kCGLineJoinRound;
    case RT_LINEJOIN_MITER:
    default:
        return kCGLineJoinMiter;
    }
}

} // namespace

CoreGraphicsRenderTarget::CoreGraphicsRenderTarget(int width, int height, bool onScreen) :
    onScreen_(onScreen),
    lineColor_(0xFFFFFFFFU),
    fillColor_(0xFFFFFFFFU),
    textColor_(0xFFFFFFFFU),
    backgroundColor_(0xFF000000U),
    backgroundOpaque_(false),
    lineWidth_(1.0f),
    lineStyle_(LINE_SOLID),
    linePattern_(0xFFFFU),
    lineThickness_(1),
    startCap_(RT_LINECAP_FLAT),
    endCap_(RT_LINECAP_FLAT),
    lineJoin_(RT_LINEJOIN_MITER),
    miterLimit_(10.0f),
    fillStyle_(FILL_SOLID),
    fillPatternColor_(0xFFFFFFFFU),
    rasterOp_(ROP_COPY),
    writingMode_(0),
    viewportLeft_(0),
    viewportTop_(0),
    viewportRight_(width),
    viewportBottom_(height),
    viewportClip_(false),
    currentX_(0),
    currentY_(0),
    horizontalAlign_(TEXT_LEFT),
    verticalAlign_(TEXT_TOP)
{
    transforms_.push_back(CGAffineTransformIdentity);
    if (!resize(width, height, false)) {
        throw std::runtime_error("Unable to create CoreGraphicsRenderTarget");
    }
}

CoreGraphicsRenderTarget::~CoreGraphicsRenderTarget() = default;

bool CoreGraphicsRenderTarget::valid() const noexcept
{
    return surface_ != nullptr && graphics_ != nullptr && graphics_->context() != nullptr;
}

bool CoreGraphicsRenderTarget::resize(int width, int height, bool preservePixels)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    try {
        std::unique_ptr<PixelSurface> replacement(
            new PixelSurface(static_cast<std::size_t>(width), static_cast<std::size_t>(height)));
        if (preservePixels && surface_ != nullptr) {
            const std::size_t copyWidth  = std::min(replacement->width(), surface_->width());
            const std::size_t copyHeight = std::min(replacement->height(), surface_->height());
            for (std::size_t y = 0; y < copyHeight; ++y) {
                std::memcpy(replacement->row(y), surface_->row(y), copyWidth * sizeof(color_t));
            }
        }
        std::unique_ptr<CoreGraphicsSurface> replacementGraphics(new CoreGraphicsSurface(*replacement));
        graphics_.swap(replacementGraphics);
        surface_.swap(replacement);
        viewportLeft_   = 0;
        viewportTop_    = 0;
        viewportRight_  = width;
        viewportBottom_ = height;
        return true;
    }
    catch (...) {
        return false;
    }
}

int CoreGraphicsRenderTarget::getWidth() const
{
    return surface_ != nullptr ? static_cast<int>(surface_->width()) : 0;
}

int CoreGraphicsRenderTarget::getHeight() const
{
    return surface_ != nullptr ? static_cast<int>(surface_->height()) : 0;
}

bool CoreGraphicsRenderTarget::isOnScreen() const
{
    return onScreen_;
}

void CoreGraphicsRenderTarget::setLineColor(color_t color)
{
    lineColor_ = color;
}

void CoreGraphicsRenderTarget::setFillColor(color_t color)
{
    fillColor_ = color;
}

void CoreGraphicsRenderTarget::setTextColor(color_t color)
{
    textColor_ = color;
}

void CoreGraphicsRenderTarget::setBkColor(color_t color)
{
    backgroundColor_ = color;
}

void CoreGraphicsRenderTarget::setBkMode(bool opaque)
{
    backgroundOpaque_ = opaque;
}

void CoreGraphicsRenderTarget::setLineWidth(float width)
{
    lineWidth_ = std::max(1.0f, width);
}

void CoreGraphicsRenderTarget::setLineStyle(LineStyle style, unsigned short pattern, int thickness)
{
    lineStyle_     = style;
    linePattern_   = pattern;
    lineThickness_ = std::max(1, thickness);
    lineWidth_     = static_cast<float>(lineThickness_);
}

void CoreGraphicsRenderTarget::setLineCap(RTLineCap startCap, RTLineCap endCap)
{
    startCap_ = startCap;
    endCap_   = endCap;
}

void CoreGraphicsRenderTarget::setLineJoin(RTLineJoin join, float miterLimit)
{
    lineJoin_   = join;
    miterLimit_ = std::max(1.0f, miterLimit);
}

void CoreGraphicsRenderTarget::setFillStyle(FillStyle style, color_t color)
{
    fillStyle_        = style;
    fillPatternColor_ = color;
    fillColor_        = color;
}

void CoreGraphicsRenderTarget::setRasterOp(RasterOp operation)
{
    rasterOp_ = operation;
}

void CoreGraphicsRenderTarget::setWritingMode(int mode)
{
    writingMode_ = mode;
    if (mode >= static_cast<int>(ROP_BLACK) && mode <= static_cast<int>(ROP_WHITE)) {
        rasterOp_ = static_cast<RasterOp>(mode);
    }
}

color_t CoreGraphicsRenderTarget::getLineColor() const
{
    return lineColor_;
}

color_t CoreGraphicsRenderTarget::getFillColor() const
{
    return fillColor_;
}

color_t CoreGraphicsRenderTarget::getTextColor() const
{
    return textColor_;
}

color_t CoreGraphicsRenderTarget::getBkColor() const
{
    return backgroundColor_;
}

FillStyle CoreGraphicsRenderTarget::getFillStyle() const
{
    return fillStyle_;
}

void CoreGraphicsRenderTarget::setViewport(int left, int top, int right, int bottom, bool clip)
{
    viewportLeft_   = left;
    viewportTop_    = top;
    viewportRight_  = right;
    viewportBottom_ = bottom;
    viewportClip_   = clip;
}

void CoreGraphicsRenderTarget::getViewport(int* left, int* top, int* right, int* bottom, int* clip) const
{
    if (left != nullptr) {
        *left = viewportLeft_;
    }
    if (top != nullptr) {
        *top = viewportTop_;
    }
    if (right != nullptr) {
        *right = viewportRight_;
    }
    if (bottom != nullptr) {
        *bottom = viewportBottom_;
    }
    if (clip != nullptr) {
        *clip = viewportClip_ ? 1 : 0;
    }
}

void CoreGraphicsRenderTarget::clearViewport()
{
    const int left   = std::max(0, viewportLeft_);
    const int top    = std::max(0, viewportTop_);
    const int right  = std::min(getWidth(), viewportRight_);
    const int bottom = std::min(getHeight(), viewportBottom_);
    for (int y = top; y < bottom; ++y) {
        std::fill(surface_->row(static_cast<std::size_t>(y)) + left, surface_->row(static_cast<std::size_t>(y)) + right,
            backgroundColor_);
    }
}

void CoreGraphicsRenderTarget::pushTransform()
{
    transforms_.push_back(transforms_.back());
}

void CoreGraphicsRenderTarget::popTransform()
{
    if (transforms_.size() > 1) {
        transforms_.pop_back();
    }
}

void CoreGraphicsRenderTarget::resetTransform()
{
    transforms_.back() = CGAffineTransformIdentity;
}

void CoreGraphicsRenderTarget::translate(float dx, float dy)
{
    transforms_.back() = CGAffineTransformTranslate(transforms_.back(), dx, dy);
}

void CoreGraphicsRenderTarget::rotate(float angle)
{
    transforms_.back() = CGAffineTransformRotate(transforms_.back(), angle);
}

void CoreGraphicsRenderTarget::scale(float sx, float sy)
{
    transforms_.back() = CGAffineTransformScale(transforms_.back(), sx, sy);
}

void CoreGraphicsRenderTarget::setTransformMatrix(const float* matrix)
{
    if (matrix != nullptr) {
        transforms_.back() = CGAffineTransformMake(matrix[0], matrix[1], matrix[3], matrix[4], matrix[6], matrix[7]);
    }
}

void CoreGraphicsRenderTarget::moveTo(int x, int y)
{
    currentX_ = x;
    currentY_ = y;
}

void CoreGraphicsRenderTarget::moveRel(int dx, int dy)
{
    currentX_ += dx;
    currentY_ += dy;
}

int CoreGraphicsRenderTarget::getCurrentX() const
{
    return currentX_;
}

int CoreGraphicsRenderTarget::getCurrentY() const
{
    return currentY_;
}

void CoreGraphicsRenderTarget::beginPrimitive()
{
    CGContextRef context = graphics_->context();
    CGContextSaveGState(context);
    if (viewportClip_) {
        const int left   = std::max(0, viewportLeft_);
        const int top    = std::max(0, viewportTop_);
        const int right  = std::min(getWidth(), viewportRight_);
        const int bottom = std::min(getHeight(), viewportBottom_);
        CGContextClipToRect(context, CGRectMake(left, top, std::max(0, right - left), std::max(0, bottom - top)));
    }
    CGContextTranslateCTM(context, viewportLeft_, viewportTop_);
    CGContextConcatCTM(context, transforms_.back());
    graphics_->setBlendMode(rasterOp_ == ROP_XOR ? kCGBlendModeXOR : kCGBlendModeCopy);
    configureStroke();
}

void CoreGraphicsRenderTarget::endPrimitive()
{
    CGContextRestoreGState(graphics_->context());
}

void CoreGraphicsRenderTarget::configureStroke()
{
    CGContextRef context = graphics_->context();
    CGContextSetLineWidth(context, lineWidth_);
    CGContextSetLineCap(context, lineCap(startCap_ == endCap_ ? startCap_ : RT_LINECAP_FLAT));
    CGContextSetLineJoin(context, lineJoin(lineJoin_));
    CGContextSetMiterLimit(context, miterLimit_);

    CGFloat     pattern[16];
    std::size_t count = 0;
    switch (lineStyle_) {
    case LINE_DASHED:
        pattern[0] = 6.0;
        pattern[1] = 3.0;
        count      = 2;
        break;
    case LINE_DOTTED:
        pattern[0] = 1.0;
        pattern[1] = 2.0;
        count      = 2;
        break;
    case LINE_DASHDOT:
        pattern[0] = 6.0;
        pattern[1] = 2.0;
        pattern[2] = 1.0;
        pattern[3] = 2.0;
        count      = 4;
        break;
    case LINE_DASHDOTDOT:
        pattern[0] = 6.0;
        pattern[1] = 2.0;
        pattern[2] = 1.0;
        pattern[3] = 2.0;
        pattern[4] = 1.0;
        pattern[5] = 2.0;
        count      = 6;
        break;
    case LINE_USER:
        for (int bit = 15; bit >= 0; --bit) {
            const bool on = (linePattern_ & (1U << bit)) != 0;
            if (count == 0 || (count & 1U) != static_cast<std::size_t>(!on)) {
                pattern[count++] = 1.0;
            } else {
                pattern[count - 1] += 1.0;
            }
        }
        break;
    default:
        break;
    }
    CGContextSetLineDash(context, 0.0, count == 0 ? nullptr : pattern, count);
}

void CoreGraphicsRenderTarget::drawLine(int x1, int y1, int x2, int y2)
{
    drawLineF(static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(x2), static_cast<float>(y2));
}

void CoreGraphicsRenderTarget::drawLineF(float x1, float y1, float x2, float y2)
{
    if (lineStyle_ == LINE_NONE) {
        return;
    }
    beginPrimitive();
    graphics_->drawLine(CGPointMake(x1, y1), CGPointMake(x2, y2), lineWidth_, lineColor_);
    endPrimitive();
}

void CoreGraphicsRenderTarget::lineTo(int x, int y)
{
    drawLine(currentX_, currentY_, x, y);
    currentX_ = x;
    currentY_ = y;
}

void CoreGraphicsRenderTarget::lineRel(int dx, int dy)
{
    lineTo(currentX_ + dx, currentY_ + dy);
}

void CoreGraphicsRenderTarget::drawRect(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0 || lineStyle_ == LINE_NONE) {
        return;
    }
    beginPrimitive();
    graphics_->strokeRect(CGRectMake(x, y, width, height), lineWidth_, lineColor_);
    endPrimitive();
}

void CoreGraphicsRenderTarget::fillRect(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0 || fillStyle_ == FILL_EMPTY) {
        return;
    }
    beginPrimitive();
    graphics_->fillRect(CGRectMake(x, y, width, height), fillPatternColor_);
    endPrimitive();
}

void CoreGraphicsRenderTarget::drawRoundRect(int x, int y, int width, int height, int ellipseWidth, int ellipseHeight)
{
    if (width <= 0 || height <= 0 || lineStyle_ == LINE_NONE) {
        return;
    }
    beginPrimitive();
    CGPathRef path = CGPathCreateWithRoundedRect(CGRectMake(x, y, width, height),
        std::min(std::abs(ellipseWidth) * 0.5, width * 0.5), std::min(std::abs(ellipseHeight) * 0.5, height * 0.5),
        nullptr);
    CGContextAddPath(graphics_->context(), path);
    CGContextSetRGBStrokeColor(graphics_->context(), channel(lineColor_, 16) / 255.0, channel(lineColor_, 8) / 255.0,
        channel(lineColor_, 0) / 255.0, channel(lineColor_, 24) / 255.0);
    CGContextStrokePath(graphics_->context());
    CGPathRelease(path);
    endPrimitive();
}

void CoreGraphicsRenderTarget::fillRoundRect(int x, int y, int width, int height, int ellipseWidth, int ellipseHeight)
{
    if (width <= 0 || height <= 0 || fillStyle_ == FILL_EMPTY) {
        return;
    }
    beginPrimitive();
    CGPathRef path = CGPathCreateWithRoundedRect(CGRectMake(x, y, width, height),
        std::min(std::abs(ellipseWidth) * 0.5, width * 0.5), std::min(std::abs(ellipseHeight) * 0.5, height * 0.5),
        nullptr);
    CGContextAddPath(graphics_->context(), path);
    const color_t color = fillPatternColor_;
    const double  alpha = channel(color, 24) / 255.0;
    CGContextSetRGBFillColor(graphics_->context(), alpha > 0 ? channel(color, 16) / 255.0 / alpha : 0.0,
        alpha > 0 ? channel(color, 8) / 255.0 / alpha : 0.0, alpha > 0 ? channel(color, 0) / 255.0 / alpha : 0.0,
        alpha);
    CGContextFillPath(graphics_->context());
    CGPathRelease(path);
    endPrimitive();
}

void CoreGraphicsRenderTarget::draw3DBar(int x, int y, int width, int height, int depth, int requestedFillStyle)
{
    const FillStyle savedStyle = fillStyle_;
    if (requestedFillStyle >= static_cast<int>(FILL_EMPTY) && requestedFillStyle <= static_cast<int>(FILL_USER)) {
        fillStyle_ = static_cast<FillStyle>(requestedFillStyle);
    }
    fillRect(x, y, width, height);
    drawRect(x, y, width, height);
    const int points[] = {
        x + width, y, x + width + depth, y - depth, x + width + depth, y + height - depth, x + width, y + height};
    drawPolygon(points, 4);
    const int top[] = {x, y, x + depth, y - depth, x + width + depth, y - depth, x + width, y};
    drawPolygon(top, 4);
    fillStyle_ = savedStyle;
}

void CoreGraphicsRenderTarget::drawCircle(int x, int y, int radius)
{
    drawEllipse(x - radius, y - radius, 0, 360, radius * 2, radius * 2);
}

void CoreGraphicsRenderTarget::fillCircle(int x, int y, int radius)
{
    fillEllipse(x - radius, y - radius, 0, 360, radius * 2, radius * 2);
}

CGPathRef CoreGraphicsRenderTarget::createArcPath(
    int x, int y, int startAngle, int endAngle, int width, int height, bool center, bool close) const
{
    CGMutablePathRef  path      = CGPathCreateMutable();
    const CGFloat     cx        = x + width * 0.5;
    const CGFloat     cy        = y + height * 0.5;
    CGAffineTransform transform = CGAffineTransformMake(width * 0.5, 0.0, 0.0, -height * 0.5, cx, cy);
    if (center) {
        CGPathMoveToPoint(path, nullptr, cx, cy);
    }
    CGPathAddArc(path, &transform, 0.0, 0.0, 1.0, startAngle * kPi / 180.0, endAngle * kPi / 180.0, false);
    if (close) {
        CGPathCloseSubpath(path);
    }
    return path;
}

void CoreGraphicsRenderTarget::drawEllipse(int x, int y, int startAngle, int endAngle, int width, int height)
{
    if (width <= 0 || height <= 0 || lineStyle_ == LINE_NONE) {
        return;
    }
    beginPrimitive();
    if (std::abs(endAngle - startAngle) >= 360) {
        graphics_->strokeEllipse(CGRectMake(x, y, width, height), lineWidth_, lineColor_);
    } else {
        CGPathRef path = createArcPath(x, y, startAngle, endAngle, width, height, false, false);
        CGContextAddPath(graphics_->context(), path);
        const double alpha = channel(lineColor_, 24) / 255.0;
        CGContextSetRGBStrokeColor(graphics_->context(), alpha > 0 ? channel(lineColor_, 16) / 255.0 / alpha : 0.0,
            alpha > 0 ? channel(lineColor_, 8) / 255.0 / alpha : 0.0,
            alpha > 0 ? channel(lineColor_, 0) / 255.0 / alpha : 0.0, alpha);
        CGContextStrokePath(graphics_->context());
        CGPathRelease(path);
    }
    endPrimitive();
}

void CoreGraphicsRenderTarget::fillEllipse(int x, int y, int startAngle, int endAngle, int width, int height)
{
    if (width <= 0 || height <= 0 || fillStyle_ == FILL_EMPTY) {
        return;
    }
    if (std::abs(endAngle - startAngle) >= 360) {
        beginPrimitive();
        graphics_->fillEllipse(CGRectMake(x, y, width, height), fillPatternColor_);
        endPrimitive();
        return;
    }
    fillSector(x, y, startAngle, endAngle, width, height);
}

void CoreGraphicsRenderTarget::drawSector(int x, int y, int startAngle, int endAngle, int width, int height)
{
    if (width <= 0 || height <= 0 || lineStyle_ == LINE_NONE) {
        return;
    }
    beginPrimitive();
    CGPathRef path = createArcPath(x, y, startAngle, endAngle, width, height, true, true);
    CGContextAddPath(graphics_->context(), path);
    const double alpha = channel(lineColor_, 24) / 255.0;
    CGContextSetRGBStrokeColor(graphics_->context(), alpha > 0 ? channel(lineColor_, 16) / 255.0 / alpha : 0.0,
        alpha > 0 ? channel(lineColor_, 8) / 255.0 / alpha : 0.0,
        alpha > 0 ? channel(lineColor_, 0) / 255.0 / alpha : 0.0, alpha);
    CGContextStrokePath(graphics_->context());
    CGPathRelease(path);
    endPrimitive();
}

void CoreGraphicsRenderTarget::fillSector(int x, int y, int startAngle, int endAngle, int width, int height)
{
    if (width <= 0 || height <= 0 || fillStyle_ == FILL_EMPTY) {
        return;
    }
    beginPrimitive();
    CGPathRef path = createArcPath(x, y, startAngle, endAngle, width, height, true, true);
    CGContextAddPath(graphics_->context(), path);
    const color_t color = fillPatternColor_;
    const double  alpha = channel(color, 24) / 255.0;
    CGContextSetRGBFillColor(graphics_->context(), alpha > 0 ? channel(color, 16) / 255.0 / alpha : 0.0,
        alpha > 0 ? channel(color, 8) / 255.0 / alpha : 0.0, alpha > 0 ? channel(color, 0) / 255.0 / alpha : 0.0,
        alpha);
    CGContextFillPath(graphics_->context());
    CGPathRelease(path);
    endPrimitive();
}

void CoreGraphicsRenderTarget::drawPie(int x, int y, int sa, int ea, int width, int height)
{
    drawSector(x, y, sa, ea, width, height);
}

void CoreGraphicsRenderTarget::fillPie(int x, int y, int sa, int ea, int width, int height)
{
    fillSector(x, y, sa, ea, width, height);
}

void CoreGraphicsRenderTarget::drawArc(int x, int y, int sa, int ea, int width, int height)
{
    drawEllipse(x, y, sa, ea, width, height);
}

void CoreGraphicsRenderTarget::drawChord(int x, int y, int sa, int ea, int width, int height)
{
    if (width <= 0 || height <= 0 || lineStyle_ == LINE_NONE) {
        return;
    }
    beginPrimitive();
    CGPathRef path = createArcPath(x, y, sa, ea, width, height, false, true);
    CGContextAddPath(graphics_->context(), path);
    const double alpha = channel(lineColor_, 24) / 255.0;
    CGContextSetRGBStrokeColor(graphics_->context(), alpha > 0 ? channel(lineColor_, 16) / 255.0 / alpha : 0.0,
        alpha > 0 ? channel(lineColor_, 8) / 255.0 / alpha : 0.0,
        alpha > 0 ? channel(lineColor_, 0) / 255.0 / alpha : 0.0, alpha);
    CGContextStrokePath(graphics_->context());
    CGPathRelease(path);
    endPrimitive();
}

void CoreGraphicsRenderTarget::drawPolygon(const int* points, int count)
{
    if (points == nullptr || count < 2 || lineStyle_ == LINE_NONE) {
        return;
    }
    std::vector<CGPoint> converted(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        converted[static_cast<std::size_t>(i)] = CGPointMake(points[i * 2], points[i * 2 + 1]);
    }
    beginPrimitive();
    graphics_->strokePath(converted.data(), converted.size(), lineWidth_, lineColor_, true);
    endPrimitive();
}

void CoreGraphicsRenderTarget::fillPolygon(const int* points, int count)
{
    if (points == nullptr || count < 3 || fillStyle_ == FILL_EMPTY) {
        return;
    }
    std::vector<CGPoint> converted(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        converted[static_cast<std::size_t>(i)] = CGPointMake(points[i * 2], points[i * 2 + 1]);
    }
    beginPrimitive();
    graphics_->fillPath(converted.data(), converted.size(), fillPatternColor_, true);
    endPrimitive();
}

void CoreGraphicsRenderTarget::drawPolyline(const int* points, int count)
{
    if (points == nullptr || count < 2 || lineStyle_ == LINE_NONE) {
        return;
    }
    std::vector<CGPoint> converted(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        converted[static_cast<std::size_t>(i)] = CGPointMake(points[i * 2], points[i * 2 + 1]);
    }
    beginPrimitive();
    graphics_->strokePath(converted.data(), converted.size(), lineWidth_, lineColor_, false);
    endPrimitive();
}

CGPoint CoreGraphicsRenderTarget::physicalPoint(float x, float y) const
{
    CGPoint point  = CGPointApplyAffineTransform(CGPointMake(x, y), transforms_.back());
    point.x       += viewportLeft_;
    point.y       += viewportTop_;
    return point;
}

bool CoreGraphicsRenderTarget::physicalPixel(int x, int y, int* physicalX, int* physicalY) const
{
    const CGPoint point = physicalPoint(static_cast<float>(x), static_cast<float>(y));
    const int     px    = static_cast<int>(std::lround(point.x));
    const int     py    = static_cast<int>(std::lround(point.y));
    if (!insideClip(px, py)) {
        return false;
    }
    if (physicalX != nullptr) {
        *physicalX = px;
    }
    if (physicalY != nullptr) {
        *physicalY = py;
    }
    return true;
}

bool CoreGraphicsRenderTarget::insideClip(int x, int y) const
{
    if (x < 0 || y < 0 || x >= getWidth() || y >= getHeight()) {
        return false;
    }
    return !viewportClip_ || (x >= viewportLeft_ && y >= viewportTop_ && x < viewportRight_ && y < viewportBottom_);
}

color_t& CoreGraphicsRenderTarget::pixelAt(int x, int y)
{
    return surface_->row(static_cast<std::size_t>(y))[x];
}

color_t CoreGraphicsRenderTarget::pixelAt(int x, int y) const
{
    return surface_->row(static_cast<std::size_t>(y))[x];
}

color_t CoreGraphicsRenderTarget::applyRasterOp(color_t destination, color_t source, RasterOp operation) noexcept
{
    switch (operation) {
    case ROP_BLACK:
        return 0x00000000U;
    case ROP_NOTMERGEPEN:
        return ~(destination | source);
    case ROP_MASKNOTPEN:
        return destination & ~source;
    case ROP_NOTCOPYPEN:
        return ~source;
    case ROP_MASKPENNOT:
        return source & ~destination;
    case ROP_NOT:
        return ~destination;
    case ROP_XOR:
        return destination ^ source;
    case ROP_NOTMASKPEN:
        return ~(destination & source);
    case ROP_AND:
        return destination & source;
    case ROP_NOTXORPEN:
        return ~(destination ^ source);
    case ROP_NOP:
        return destination;
    case ROP_MERGENOTPEN:
        return destination | ~source;
    case ROP_COPY:
        return source;
    case ROP_MERGEPENNOT:
        return source | ~destination;
    case ROP_OR:
        return destination | source;
    case ROP_WHITE:
        return 0xFFFFFFFFU;
    default:
        return source;
    }
}

color_t CoreGraphicsRenderTarget::premultiply(color_t straight) noexcept
{
    const unsigned int alpha = channel(straight, 24);
    return pack(alpha, scaleByte(channel(straight, 16), alpha), scaleByte(channel(straight, 8), alpha),
        scaleByte(channel(straight, 0), alpha));
}

color_t CoreGraphicsRenderTarget::blendPremultiplied(color_t destination, color_t source, unsigned char factor) noexcept
{
    const unsigned int sourceAlpha = scaleByte(channel(source, 24), factor);
    const unsigned int inverse     = 255U - sourceAlpha;
    const unsigned int sourceRed   = scaleByte(channel(source, 16), factor);
    const unsigned int sourceGreen = scaleByte(channel(source, 8), factor);
    const unsigned int sourceBlue  = scaleByte(channel(source, 0), factor);
    return pack(sourceAlpha + scaleByte(channel(destination, 24), inverse),
        sourceRed + scaleByte(channel(destination, 16), inverse),
        sourceGreen + scaleByte(channel(destination, 8), inverse),
        sourceBlue + scaleByte(channel(destination, 0), inverse));
}

color_t CoreGraphicsRenderTarget::blendStraight(color_t destination, color_t source, unsigned char factor) noexcept
{
    return blendPremultiplied(destination, premultiply(source), factor);
}

void CoreGraphicsRenderTarget::writePixel(int x, int y, color_t color, bool useRasterOp)
{
    if (!insideClip(x, y)) {
        return;
    }
    color_t& destination = pixelAt(x, y);
    destination          = useRasterOp ? applyRasterOp(destination, color, rasterOp_) : color;
}

void CoreGraphicsRenderTarget::putPixel(int x, int y, color_t color)
{
    int physicalX = 0;
    int physicalY = 0;
    if (physicalPixel(x, y, &physicalX, &physicalY)) {
        writePixel(physicalX, physicalY, color);
    }
}

color_t CoreGraphicsRenderTarget::getPixel(int x, int y) const
{
    const int physicalX = x + viewportLeft_;
    const int physicalY = y + viewportTop_;
    if (!insideClip(physicalX, physicalY)) {
        return 0;
    }
    return pixelAt(physicalX, physicalY);
}

void CoreGraphicsRenderTarget::putPixelAlpha(int x, int y, color_t color)
{
    int px = 0;
    int py = 0;
    if (physicalPixel(x, y, &px, &py)) {
        pixelAt(px, py) = blendStraight(pixelAt(px, py), color);
    }
}

void CoreGraphicsRenderTarget::putPixelSaveAlpha(int x, int y, color_t color)
{
    int px = 0;
    int py = 0;
    if (physicalPixel(x, y, &px, &py)) {
        pixelAt(px, py) = (pixelAt(px, py) & 0xFF000000U) | (color & 0x00FFFFFFU);
    }
}

void CoreGraphicsRenderTarget::putPixelAlphaBlend(int x, int y, color_t color, unsigned char alphaFactor)
{
    int px = 0;
    int py = 0;
    if (physicalPixel(x, y, &px, &py)) {
        pixelAt(px, py) = blendStraight(pixelAt(px, py), color, alphaFactor);
    }
}

void CoreGraphicsRenderTarget::putPixels(int count, const int* points)
{
    if (points == nullptr || count <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        putPixel(points[i * 2], points[i * 2 + 1], lineColor_);
    }
}

void CoreGraphicsRenderTarget::floodFillInternal(int x, int y, color_t boundary, bool surfaceMode)
{
    if (fillStyle_ == FILL_EMPTY) {
        return;
    }
    const int seedX = x + viewportLeft_;
    const int seedY = y + viewportTop_;
    if (!insideClip(seedX, seedY)) {
        return;
    }
    const color_t seed = pixelAt(seedX, seedY);
    if ((surfaceMode && (seed & 0x00FFFFFFU) != (boundary & 0x00FFFFFFU)) ||
        (!surfaceMode && (seed & 0x00FFFFFFU) == (boundary & 0x00FFFFFFU)))
    {
        return;
    }

    std::vector<unsigned char> visited(static_cast<std::size_t>(getWidth()) * getHeight(), 0);
    std::vector<int>           stack(1, seedY * getWidth() + seedX);
    while (!stack.empty()) {
        const int index = stack.back();
        stack.pop_back();
        if (visited[static_cast<std::size_t>(index)] != 0) {
            continue;
        }
        visited[static_cast<std::size_t>(index)] = 1;
        const int px                             = index % getWidth();
        const int py                             = index / getWidth();
        if (!insideClip(px, py)) {
            continue;
        }
        const color_t current = pixelAt(px, py);
        const bool    matches = surfaceMode ? (current & 0x00FFFFFFU) == (boundary & 0x00FFFFFFU) :
                                              (current & 0x00FFFFFFU) != (boundary & 0x00FFFFFFU);
        if (!matches) {
            continue;
        }
        pixelAt(px, py) = patternUsesForeground(fillStyle_, px, py) ? fillColor_ : backgroundColor_;
        if (px > 0) {
            stack.push_back(index - 1);
        }
        if (px + 1 < getWidth()) {
            stack.push_back(index + 1);
        }
        if (py > 0) {
            stack.push_back(index - getWidth());
        }
        if (py + 1 < getHeight()) {
            stack.push_back(index + getWidth());
        }
    }
}

void CoreGraphicsRenderTarget::floodFill(int x, int y, color_t borderColor)
{
    floodFillInternal(x, y, borderColor, false);
}

void CoreGraphicsRenderTarget::floodFillSurface(int x, int y, color_t surfaceColor)
{
    floodFillInternal(x, y, surfaceColor, true);
}

void CoreGraphicsRenderTarget::clear(color_t color)
{
    graphics_->clear(color);
}

CoreGraphicsRenderTarget::SourceImage CoreGraphicsRenderTarget::captureSource(
    RenderTarget* source, int x, int y, int width, int height)
{
    SourceImage result = {std::max(0, width), std::max(0, height), {}};
    if (source == nullptr || width <= 0 || height <= 0) {
        return result;
    }
    result.pixels.assign(static_cast<std::size_t>(width) * height, 0);
    const color_t* pixels = source->getPixelBuffer();
    if (pixels == nullptr) {
        return result;
    }
    for (int row = 0; row < height; ++row) {
        const int sourceY = y + row;
        if (sourceY < 0 || sourceY >= source->getHeight()) {
            continue;
        }
        for (int column = 0; column < width; ++column) {
            const int sourceX = x + column;
            if (sourceX >= 0 && sourceX < source->getWidth()) {
                result.pixels[static_cast<std::size_t>(row) * width + column] =
                    pixels[static_cast<std::size_t>(sourceY) * source->getWidth() + sourceX];
            }
        }
    }
    return result;
}

color_t CoreGraphicsRenderTarget::sample(const SourceImage& source, float x, float y, bool smooth) noexcept
{
    if (source.width <= 0 || source.height <= 0 || source.pixels.empty()) {
        return 0;
    }
    if (!smooth) {
        const int sx = std::max(0, std::min(source.width - 1, static_cast<int>(std::floor(x + 0.5f))));
        const int sy = std::max(0, std::min(source.height - 1, static_cast<int>(std::floor(y + 0.5f))));
        return source.pixels[static_cast<std::size_t>(sy) * source.width + sx];
    }

    const float   clampedX    = std::max(0.0f, std::min(static_cast<float>(source.width - 1), x));
    const float   clampedY    = std::max(0.0f, std::min(static_cast<float>(source.height - 1), y));
    const int     x0          = static_cast<int>(std::floor(clampedX));
    const int     y0          = static_cast<int>(std::floor(clampedY));
    const int     x1          = std::min(source.width - 1, x0 + 1);
    const int     y1          = std::min(source.height - 1, y0 + 1);
    const float   fx          = clampedX - x0;
    const float   fy          = clampedY - y0;
    const color_t p00         = source.pixels[static_cast<std::size_t>(y0) * source.width + x0];
    const color_t p10         = source.pixels[static_cast<std::size_t>(y0) * source.width + x1];
    const color_t p01         = source.pixels[static_cast<std::size_t>(y1) * source.width + x0];
    const color_t p11         = source.pixels[static_cast<std::size_t>(y1) * source.width + x1];
    auto          interpolate = [=](unsigned int shift) {
        const float top    = channel(p00, shift) + (channel(p10, shift) - channel(p00, shift)) * fx;
        const float bottom = channel(p01, shift) + (channel(p11, shift) - channel(p01, shift)) * fx;
        return static_cast<unsigned int>(std::lround(top + (bottom - top) * fy));
    };
    return pack(interpolate(24), interpolate(16), interpolate(8), interpolate(0));
}

void CoreGraphicsRenderTarget::stretchTransfer(int dstX, int dstY, int dstWidth, int dstHeight,
    const SourceImage& source, ImageAlphaFormat format, unsigned char alpha, bool smooth, bool blend)
{
    if (dstWidth <= 0 || dstHeight <= 0 || source.width <= 0 || source.height <= 0) {
        return;
    }
    for (int y = 0; y < dstHeight; ++y) {
        const float sourceY = (y + 0.5f) * source.height / dstHeight - 0.5f;
        for (int x = 0; x < dstWidth; ++x) {
            const int px = dstX + x + viewportLeft_;
            const int py = dstY + y + viewportTop_;
            if (!insideClip(px, py)) {
                continue;
            }
            const float sourceX = (x + 0.5f) * source.width / dstWidth - 0.5f;
            color_t     value   = sample(source, sourceX, sourceY, smooth);
            if (format == IMAGE_ALPHA_STRAIGHT) {
                value = premultiply(value);
            } else if (format == IMAGE_ALPHA_OPAQUE) {
                value = 0xFF000000U | (value & 0x00FFFFFFU);
            }
            pixelAt(px, py) = blend ? blendPremultiplied(pixelAt(px, py), value, alpha) :
                                      applyRasterOp(pixelAt(px, py), value, rasterOp_);
        }
    }
}

void CoreGraphicsRenderTarget::blit(
    int dstX, int dstY, RenderTarget* source, int sourceX, int sourceY, int width, int height)
{
    const SourceImage snapshot = captureSource(source, sourceX, sourceY, width, height);
    stretchTransfer(dstX, dstY, width, height, snapshot, IMAGE_ALPHA_PREMULTIPLIED, 255, false, false);
}

void CoreGraphicsRenderTarget::blitStretch(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source,
    int sourceX, int sourceY, int sourceWidth, int sourceHeight)
{
    const SourceImage snapshot = captureSource(source, sourceX, sourceY, sourceWidth, sourceHeight);
    stretchTransfer(dstX, dstY, dstWidth, dstHeight, snapshot, IMAGE_ALPHA_PREMULTIPLIED, 255, false, false);
}

void CoreGraphicsRenderTarget::alphaBlend(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source,
    int sourceX, int sourceY, int sourceWidth, int sourceHeight, unsigned char alpha, ImageAlphaFormat format,
    bool smooth)
{
    const SourceImage snapshot = captureSource(source, sourceX, sourceY, sourceWidth, sourceHeight);
    stretchTransfer(dstX, dstY, dstWidth, dstHeight, snapshot, format, alpha, smooth, true);
}

void CoreGraphicsRenderTarget::alphaTransparent(int dstX, int dstY, RenderTarget* source, int sourceX, int sourceY,
    int width, int height, color_t transparentColor, unsigned char alpha)
{
    const SourceImage snapshot = captureSource(source, sourceX, sourceY, width, height);
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            const color_t value = snapshot.pixels[static_cast<std::size_t>(y) * snapshot.width + x];
            if ((value & 0x00FFFFFFU) == (transparentColor & 0x00FFFFFFU)) {
                continue;
            }
            const int px = dstX + x + viewportLeft_;
            const int py = dstY + y + viewportTop_;
            if (insideClip(px, py)) {
                pixelAt(px, py) = blendPremultiplied(pixelAt(px, py), value, alpha);
            }
        }
    }
}

void CoreGraphicsRenderTarget::withAlpha(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source,
    int sourceX, int sourceY, int sourceWidth, int sourceHeight, bool smooth)
{
    alphaBlend(dstX, dstY, dstWidth, dstHeight, source, sourceX, sourceY, sourceWidth, sourceHeight, 255,
        IMAGE_ALPHA_PREMULTIPLIED, smooth);
}

void CoreGraphicsRenderTarget::alphaFilter(
    int dstX, int dstY, int width, int height, RenderTarget* source, int sourceX, int sourceY, unsigned char alpha)
{
    alphaBlend(
        dstX, dstY, width, height, source, sourceX, sourceY, width, height, alpha, IMAGE_ALPHA_PREMULTIPLIED, false);
}

void CoreGraphicsRenderTarget::rotateBlend(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source,
    int sourceX, int sourceY, int sourceWidth, int sourceHeight, float angle, float centerX, float centerY,
    bool transparent, int alpha, bool smooth)
{
    rotateZoomBlend(dstX, dstY, dstWidth, dstHeight, source, sourceX, sourceY, sourceWidth, sourceHeight, angle,
        centerX, centerY, 1.0f, 1.0f, transparent, alpha, smooth);
}

void CoreGraphicsRenderTarget::rotateZoomBlend(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source,
    int sourceX, int sourceY, int sourceWidth, int sourceHeight, float angle, float centerX, float centerY, float zoomX,
    float zoomY, bool transparent, int alpha, bool smooth)
{
    if (dstWidth <= 0 || dstHeight <= 0 || zoomX == 0.0f || zoomY == 0.0f) {
        return;
    }
    const SourceImage   snapshot = captureSource(source, sourceX, sourceY, sourceWidth, sourceHeight);
    const float         cosine   = std::cos(angle);
    const float         sine     = std::sin(angle);
    const unsigned char factor   = static_cast<unsigned char>(std::max(0, std::min(255, alpha < 0 ? 255 : alpha)));
    for (int y = 0; y < dstHeight; ++y) {
        for (int x = 0; x < dstWidth; ++x) {
            const float dx = (x - centerX) / zoomX;
            const float dy = (y - centerY) / zoomY;
            const float sx = cosine * dx + sine * dy + centerX;
            const float sy = -sine * dx + cosine * dy + centerY;
            if (sx < -0.5f || sy < -0.5f || sx >= snapshot.width - 0.5f || sy >= snapshot.height - 0.5f) {
                continue;
            }
            const color_t value = sample(snapshot, sx, sy, smooth);
            if (transparent && value == 0) {
                continue;
            }
            const int px = dstX - static_cast<int>(std::lround(centerX)) + x + viewportLeft_;
            const int py = dstY - static_cast<int>(std::lround(centerY)) + y + viewportTop_;
            if (insideClip(px, py)) {
                pixelAt(px, py) = alpha >= 0 ? blendPremultiplied(pixelAt(px, py), value, factor) :
                                               applyRasterOp(pixelAt(px, py), value, rasterOp_);
            }
        }
    }
}

void CoreGraphicsRenderTarget::blitAffine(RenderTarget* source, int sourceX, int sourceY, int sourceWidth,
    int sourceHeight, const float* destinationPoints, bool premultipliedAlpha, bool smooth)
{
    if (destinationPoints == nullptr || sourceWidth <= 0 || sourceHeight <= 0) {
        return;
    }
    const SourceImage snapshot = captureSource(source, sourceX, sourceY, sourceWidth, sourceHeight);
    const float       p0x = destinationPoints[0], p0y = destinationPoints[1];
    const float       ux = destinationPoints[2] - p0x, uy = destinationPoints[3] - p0y;
    const float       vx = destinationPoints[6] - p0x, vy = destinationPoints[7] - p0y;
    const float       determinant = ux * vy - uy * vx;
    if (std::abs(determinant) < 1e-8f) {
        return;
    }
    float minX = destinationPoints[0], maxX = destinationPoints[0];
    float minY = destinationPoints[1], maxY = destinationPoints[1];
    for (int i = 1; i < 4; ++i) {
        minX = std::min(minX, destinationPoints[i * 2]);
        maxX = std::max(maxX, destinationPoints[i * 2]);
        minY = std::min(minY, destinationPoints[i * 2 + 1]);
        maxY = std::max(maxY, destinationPoints[i * 2 + 1]);
    }
    for (int y = static_cast<int>(std::floor(minY)); y < static_cast<int>(std::ceil(maxY)); ++y) {
        for (int x = static_cast<int>(std::floor(minX)); x < static_cast<int>(std::ceil(maxX)); ++x) {
            const float dx = x + 0.5f - p0x;
            const float dy = y + 0.5f - p0y;
            const float u  = (dx * vy - dy * vx) / determinant;
            const float v  = (ux * dy - uy * dx) / determinant;
            if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
                continue;
            }
            color_t value = sample(snapshot, u * (sourceWidth - 1), v * (sourceHeight - 1), smooth);
            if (!premultipliedAlpha) {
                value = premultiply(value);
            }
            const int px = x + viewportLeft_;
            const int py = y + viewportTop_;
            if (insideClip(px, py)) {
                pixelAt(px, py) = premultipliedAlpha ? blendPremultiplied(pixelAt(px, py), value) : value;
            }
        }
    }
}

void CoreGraphicsRenderTarget::filterBlur(int dstX, int dstY, int width, int height, float intensity)
{
    if (width <= 0 || height <= 0 || intensity <= 0.0f) {
        return;
    }
    const int left   = std::max(0, dstX + viewportLeft_);
    const int top    = std::max(0, dstY + viewportTop_);
    const int right  = std::min(getWidth(), dstX + viewportLeft_ + width);
    const int bottom = std::min(getHeight(), dstY + viewportTop_ + height);
    if (left >= right || top >= bottom) {
        return;
    }
    const int            clippedWidth  = right - left;
    const int            clippedHeight = bottom - top;
    const int            radius        = std::max(1, static_cast<int>(std::ceil(intensity)));
    std::vector<color_t> source(static_cast<std::size_t>(clippedWidth) * clippedHeight);
    for (int y = 0; y < clippedHeight; ++y) {
        std::memcpy(source.data() + static_cast<std::size_t>(y) * clippedWidth, surface_->row(top + y) + left,
            static_cast<std::size_t>(clippedWidth) * sizeof(color_t));
    }
    for (int y = 0; y < clippedHeight; ++y) {
        for (int x = 0; x < clippedWidth; ++x) {
            unsigned long long sums[4] = {0, 0, 0, 0};
            unsigned int       count   = 0;
            for (int sampleY = std::max(0, y - radius); sampleY <= std::min(clippedHeight - 1, y + radius); ++sampleY) {
                for (int sampleX = std::max(0, x - radius); sampleX <= std::min(clippedWidth - 1, x + radius);
                    ++sampleX)
                {
                    const color_t value  = source[static_cast<std::size_t>(sampleY) * clippedWidth + sampleX];
                    sums[0]             += channel(value, 24);
                    sums[1]             += channel(value, 16);
                    sums[2]             += channel(value, 8);
                    sums[3]             += channel(value, 0);
                    ++count;
                }
            }
            pixelAt(left + x, top + y) =
                pack(static_cast<unsigned int>(sums[0] / count), static_cast<unsigned int>(sums[1] / count),
                    static_cast<unsigned int>(sums[2] / count), static_cast<unsigned int>(sums[3] / count));
        }
    }
}

void CoreGraphicsRenderTarget::setFont(int height, int width, const char* face, int escapement, int orientation,
    int weight, bool italic, bool underline, bool strikeout)
{
    textRenderer_.setFont(height, width, face, escapement, orientation, weight, italic, underline, strikeout);
}

void CoreGraphicsRenderTarget::getFont(int* height, int* width, char* face, int faceCapacity, int* escapement,
    int* orientation, int* weight, bool* italic, bool* underline, bool* strikeout) const
{
    textRenderer_.getFont(
        height, width, face, faceCapacity, escapement, orientation, weight, italic, underline, strikeout);
}

void CoreGraphicsRenderTarget::setTextJustify(TextHAlign horizontal, TextVAlign vertical)
{
    horizontalAlign_ = horizontal;
    verticalAlign_   = vertical;
}

void CoreGraphicsRenderTarget::drawText(float x, float y, const char* text)
{
    if (text == nullptr) {
        return;
    }
    float width = 0.0f, height = 0.0f;
    textRenderer_.measure(text, &width, &height);
    if (backgroundOpaque_) {
        const color_t   saved      = fillPatternColor_;
        const FillStyle savedStyle = fillStyle_;
        fillPatternColor_          = backgroundColor_;
        fillStyle_                 = FILL_SOLID;
        fillRect(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::ceil(width)),
            static_cast<int>(std::ceil(height)));
        fillPatternColor_ = saved;
        fillStyle_        = savedStyle;
    }
    beginPrimitive();
    graphics_->setBlendMode(kCGBlendModeNormal);
    textRenderer_.draw(graphics_->context(), x, y, text, textColor_, horizontalAlign_, verticalAlign_);
    endPrimitive();
}

void CoreGraphicsRenderTarget::drawText(float x, float y, const wchar_t* text)
{
    if (text == nullptr) {
        return;
    }
    float width = 0.0f, height = 0.0f;
    textRenderer_.measure(text, &width, &height);
    if (backgroundOpaque_) {
        const color_t   saved      = fillPatternColor_;
        const FillStyle savedStyle = fillStyle_;
        fillPatternColor_          = backgroundColor_;
        fillStyle_                 = FILL_SOLID;
        fillRect(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(std::ceil(width)),
            static_cast<int>(std::ceil(height)));
        fillPatternColor_ = saved;
        fillStyle_        = savedStyle;
    }
    beginPrimitive();
    graphics_->setBlendMode(kCGBlendModeNormal);
    textRenderer_.draw(graphics_->context(), x, y, text, textColor_, horizontalAlign_, verticalAlign_);
    endPrimitive();
}

int CoreGraphicsRenderTarget::getTextWidth(const char* text) const
{
    float width = 0.0f;
    textRenderer_.measure(text, &width, nullptr);
    return static_cast<int>(std::lround(width));
}

int CoreGraphicsRenderTarget::getTextWidth(const wchar_t* text) const
{
    float width = 0.0f;
    textRenderer_.measure(text, &width, nullptr);
    return static_cast<int>(std::lround(width));
}

int CoreGraphicsRenderTarget::getTextHeight(const char* text) const
{
    float height = 0.0f;
    textRenderer_.measure(text, nullptr, &height);
    return static_cast<int>(std::lround(height));
}

int CoreGraphicsRenderTarget::getTextHeight(const wchar_t* text) const
{
    float height = 0.0f;
    textRenderer_.measure(text, nullptr, &height);
    return static_cast<int>(std::lround(height));
}

void CoreGraphicsRenderTarget::measureText(const char* text, float* width, float* height) const
{
    textRenderer_.measure(text, width, height);
}

void CoreGraphicsRenderTarget::measureText(const wchar_t* text, float* width, float* height) const
{
    textRenderer_.measure(text, width, height);
}

color_t* CoreGraphicsRenderTarget::getPixelBuffer()
{
    return surface_->data();
}

const color_t* CoreGraphicsRenderTarget::getPixelBuffer() const
{
    return surface_->data();
}

color_t* CoreGraphicsRenderTarget::getPixelBufferForWrite(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    return surface_->data();
}

bool CoreGraphicsRenderTarget::updatePixelBuffer(
    int x, int y, int width, int height, const color_t* pixels, int pitchBytes)
{
    const std::size_t rowBytes = width > 0 ? static_cast<std::size_t>(width) * sizeof(color_t) : 0;
    if (pixels == nullptr || width <= 0 || height <= 0 || x < 0 || y < 0 || width > getWidth() - x ||
        height > getHeight() - y || pitchBytes < 0 || static_cast<std::size_t>(pitchBytes) < rowBytes)
    {
        return false;
    }
    const unsigned char* source = reinterpret_cast<const unsigned char*>(pixels);
    for (int row = 0; row < height; ++row) {
        std::memmove(surface_->row(static_cast<std::size_t>(y + row)) + x,
            source + static_cast<std::size_t>(row) * pitchBytes, rowBytes);
    }
    return true;
}

void CoreGraphicsRenderTarget::flush()
{
    graphics_->flush();
}

void CoreGraphicsRenderTarget::present()
{
    // AppKit owns window presentation. Flushing here guarantees that callers
    // and the NSView presenter observe all writes to the shared CPU surface.
    flush();
}

} // namespace backend
} // namespace ege
