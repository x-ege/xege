#include "backend/linux/CairoRenderTarget.h"
#include "encodeconv.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
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
    return (std::min(255U, alpha) << 24U) |
        (std::min(255U, red) << 16U) |
        (std::min(255U, green) << 8U) |
        std::min(255U, blue);
}

bool patternUsesForeground(FillStyle style, int x, int y) noexcept
{
    const int slash = (x + y) & 7;
    const int backslash = (x - y) & 7;
    switch (style) {
    case FILL_EMPTY: return false;
    case FILL_HORIZONTAL: return (y & 7) == 0;
    case FILL_LIGHT_SLASH: return slash == 0;
    case FILL_SLASH: return slash <= 1;
    case FILL_BACKSLASH: return backslash <= 1;
    case FILL_LIGHT_BACKSLASH: return backslash == 0;
    case FILL_HATCH: return (x & 7) == 0 || (y & 7) == 0;
    case FILL_CROSS_HATCH: return slash <= 1 || backslash <= 1;
    case FILL_INTERLEAVE:
        return ((y & 7) == 0 && (x & 7) < 4) || ((y & 7) == 4 && (x & 7) >= 4);
    case FILL_WIDE_DOT: return (x & 7) == 0 && (y & 7) == 0;
    case FILL_CLOSE_DOT: return (x & 3) == 0 && (y & 3) == 0;
    case FILL_USER:
    case FILL_SOLID:
    default: return true;
    }
}

void setSourceColor(cairo_t* context, color_t color, bool storedPremultiplied = false)
{
    const unsigned int alpha = channel(color, 24);
    double red = channel(color, 16) / 255.0;
    double green = channel(color, 8) / 255.0;
    double blue = channel(color, 0) / 255.0;
    if (storedPremultiplied && alpha != 0) {
        red = std::min(1.0, red * 255.0 / alpha);
        green = std::min(1.0, green * 255.0 / alpha);
        blue = std::min(1.0, blue * 255.0 / alpha);
    }
    cairo_set_source_rgba(context, red, green, blue, alpha / 255.0);
}

cairo_line_cap_t lineCap(RTLineCap cap)
{
    switch (cap) {
    case RT_LINECAP_ROUND: return CAIRO_LINE_CAP_ROUND;
    case RT_LINECAP_SQUARE: return CAIRO_LINE_CAP_SQUARE;
    default: return CAIRO_LINE_CAP_BUTT;
    }
}

cairo_line_join_t lineJoin(RTLineJoin join)
{
    switch (join) {
    case RT_LINEJOIN_BEVEL: return CAIRO_LINE_JOIN_BEVEL;
    case RT_LINEJOIN_ROUND: return CAIRO_LINE_JOIN_ROUND;
    default: return CAIRO_LINE_JOIN_MITER;
    }
}

} // namespace

CairoRenderTarget::CairoRenderTarget(int width, int height, bool onScreen) :
    onScreen_(onScreen), viewportRight_(width), viewportBottom_(height)
{
    transforms_.push_back({1.0, 0.0, 0.0, 1.0, 0.0, 0.0});
    if (!resize(width, height, false)) {
        throw std::runtime_error("Unable to create CairoRenderTarget");
    }
}

CairoRenderTarget::~CairoRenderTarget()
{
    if (rasterCairo_ != nullptr) cairo_destroy(rasterCairo_);
    if (rasterSurface_ != nullptr) cairo_surface_destroy(rasterSurface_);
    if (cairo_ != nullptr) cairo_destroy(cairo_);
    if (cairoSurface_ != nullptr) cairo_surface_destroy(cairoSurface_);
}

bool CairoRenderTarget::valid() const noexcept
{
    return surface_ != nullptr && cairoSurface_ != nullptr && cairo_ != nullptr &&
        cairo_surface_status(cairoSurface_) == CAIRO_STATUS_SUCCESS &&
        cairo_status(cairo_) == CAIRO_STATUS_SUCCESS;
}

void CairoRenderTarget::recreateCairoSurface()
{
    if (rasterCairo_ != nullptr) { cairo_destroy(rasterCairo_); rasterCairo_ = nullptr; }
    if (rasterSurface_ != nullptr) { cairo_surface_destroy(rasterSurface_); rasterSurface_ = nullptr; }
    if (cairo_ != nullptr) { cairo_destroy(cairo_); cairo_ = nullptr; }
    if (cairoSurface_ != nullptr) { cairo_surface_destroy(cairoSurface_); cairoSurface_ = nullptr; }

    cairoSurface_ = cairo_image_surface_create_for_data(
        reinterpret_cast<unsigned char*>(surface_->data()), CAIRO_FORMAT_ARGB32,
        getWidth(), getHeight(), static_cast<int>(surface_->strideBytes()));
    cairo_ = cairo_create(cairoSurface_);

    rasterScratch_.reset(new PixelSurface(surface_->width(), surface_->height()));
    rasterSurface_ = cairo_image_surface_create_for_data(
        reinterpret_cast<unsigned char*>(rasterScratch_->data()), CAIRO_FORMAT_ARGB32,
        getWidth(), getHeight(), static_cast<int>(rasterScratch_->strideBytes()));
    rasterCairo_ = cairo_create(rasterSurface_);
}

bool CairoRenderTarget::resize(int width, int height, bool preservePixels)
{
    if (width <= 0 || height <= 0 ||
        width > std::numeric_limits<int>::max() / static_cast<int>(sizeof(color_t))) {
        return false;
    }
    try {
        std::unique_ptr<PixelSurface> replacement(
            new PixelSurface(static_cast<std::size_t>(width), static_cast<std::size_t>(height)));
        if (preservePixels && surface_ != nullptr) {
            const std::size_t copyWidth = std::min(replacement->width(), surface_->width());
            const std::size_t copyHeight = std::min(replacement->height(), surface_->height());
            for (std::size_t y = 0; y < copyHeight; ++y) {
                std::memcpy(replacement->row(y), surface_->row(y), copyWidth * sizeof(color_t));
            }
        }
        surface_ = std::move(replacement);
        recreateCairoSurface();
        viewportLeft_ = 0;
        viewportTop_ = 0;
        viewportRight_ = width;
        viewportBottom_ = height;
        return valid();
    } catch (...) {
        return false;
    }
}

int CairoRenderTarget::getWidth() const
{
    return surface_ != nullptr ? static_cast<int>(surface_->width()) : 0;
}

int CairoRenderTarget::getHeight() const
{
    return surface_ != nullptr ? static_cast<int>(surface_->height()) : 0;
}

bool CairoRenderTarget::isOnScreen() const { return onScreen_; }

void CairoRenderTarget::setLineColor(color_t color) { lineColor_ = color; }
void CairoRenderTarget::setFillColor(color_t color) { fillColor_ = color; fillPatternColor_ = color; }
void CairoRenderTarget::setTextColor(color_t color) { textColor_ = color; }
void CairoRenderTarget::setBkColor(color_t color) { backgroundColor_ = color; }
void CairoRenderTarget::setBkMode(bool opaque) { backgroundOpaque_ = opaque; }
void CairoRenderTarget::setLineWidth(float width) { lineWidth_ = std::max(1.0f, width); }

void CairoRenderTarget::setLineStyle(LineStyle style, unsigned short pattern, int thickness)
{
    lineStyle_ = style;
    linePattern_ = pattern;
    lineThickness_ = std::max(1, thickness);
    lineWidth_ = static_cast<float>(lineThickness_);
}

void CairoRenderTarget::setLineCap(RTLineCap startCap, RTLineCap endCap)
{
    startCap_ = startCap;
    endCap_ = endCap;
}

void CairoRenderTarget::setLineJoin(RTLineJoin join, float miterLimit)
{
    lineJoin_ = join;
    miterLimit_ = std::max(1.0f, miterLimit);
}

void CairoRenderTarget::setFillStyle(FillStyle style, color_t color)
{
    fillStyle_ = style;
    fillPatternColor_ = color;
    fillColor_ = color;
}

void CairoRenderTarget::setRasterOp(RasterOp operation) { rasterOp_ = operation; }

void CairoRenderTarget::setWritingMode(int mode)
{
    writingMode_ = mode;
    if (mode >= static_cast<int>(ROP_BLACK) && mode <= static_cast<int>(ROP_WHITE)) {
        rasterOp_ = static_cast<RasterOp>(mode);
    }
}

void CairoRenderTarget::setAntialiasing(bool enabled) { antialiasing_ = enabled; }
color_t CairoRenderTarget::getLineColor() const { return lineColor_; }
color_t CairoRenderTarget::getFillColor() const { return fillColor_; }
color_t CairoRenderTarget::getTextColor() const { return textColor_; }
color_t CairoRenderTarget::getBkColor() const { return backgroundColor_; }
FillStyle CairoRenderTarget::getFillStyle() const { return fillStyle_; }

void CairoRenderTarget::setViewport(int left, int top, int right, int bottom, bool clip)
{
    viewportLeft_ = left;
    viewportTop_ = top;
    viewportRight_ = right;
    viewportBottom_ = bottom;
    viewportClip_ = clip;
}

void CairoRenderTarget::getViewport(int* left, int* top, int* right, int* bottom, int* clip) const
{
    if (left != nullptr) *left = viewportLeft_;
    if (top != nullptr) *top = viewportTop_;
    if (right != nullptr) *right = viewportRight_;
    if (bottom != nullptr) *bottom = viewportBottom_;
    if (clip != nullptr) *clip = viewportClip_ ? 1 : 0;
}

void CairoRenderTarget::clearViewport()
{
    const int left = std::clamp(viewportLeft_, 0, getWidth());
    const int top = std::clamp(viewportTop_, 0, getHeight());
    const int right = std::clamp(viewportRight_, left, getWidth());
    const int bottom = std::clamp(viewportBottom_, top, getHeight());
    const color_t stored = premultiply(backgroundColor_);
    cairo_surface_flush(cairoSurface_);
    for (int y = top; y < bottom; ++y) {
        std::fill(surface_->row(static_cast<std::size_t>(y)) + left,
            surface_->row(static_cast<std::size_t>(y)) + right, stored);
    }
    cairo_surface_mark_dirty_rectangle(cairoSurface_, left, top, right - left, bottom - top);
}

void CairoRenderTarget::pushTransform() { transforms_.push_back(transforms_.back()); }
void CairoRenderTarget::popTransform() { if (transforms_.size() > 1) transforms_.pop_back(); }
void CairoRenderTarget::resetTransform() { transforms_.back() = {1, 0, 0, 1, 0, 0}; }

void CairoRenderTarget::translate(float dx, float dy)
{
    auto& m = transforms_.back();
    m[4] += m[0] * dx + m[2] * dy;
    m[5] += m[1] * dx + m[3] * dy;
}

void CairoRenderTarget::rotate(float angle)
{
    auto& m = transforms_.back();
    const double c = std::cos(angle), s = std::sin(angle);
    const std::array<double, 6> old = m;
    m[0] = old[0] * c + old[2] * s;
    m[1] = old[1] * c + old[3] * s;
    m[2] = old[2] * c - old[0] * s;
    m[3] = old[3] * c - old[1] * s;
}

void CairoRenderTarget::scale(float sx, float sy)
{
    auto& m = transforms_.back();
    m[0] *= sx; m[1] *= sx; m[2] *= sy; m[3] *= sy;
}

void CairoRenderTarget::setTransformMatrix(const float* matrix)
{
    if (matrix != nullptr) {
        transforms_.back() = {matrix[0], matrix[1], matrix[3], matrix[4], matrix[6], matrix[7]};
    }
}

void CairoRenderTarget::moveTo(int x, int y) { currentX_ = x; currentY_ = y; }
void CairoRenderTarget::moveRel(int dx, int dy) { currentX_ += dx; currentY_ += dy; }
int CairoRenderTarget::getCurrentX() const { return currentX_; }
int CairoRenderTarget::getCurrentY() const { return currentY_; }

cairo_t* CairoRenderTarget::drawingContext()
{
    return drawingToScratch_ ? rasterCairo_ : cairo_;
}

void CairoRenderTarget::beginDraw(color_t color, bool fill)
{
    drawingToScratch_ = rasterOp_ != ROP_COPY;
    if (drawingToScratch_) {
        rasterScratch_->clear(0U);
        cairo_surface_mark_dirty(rasterSurface_);
    }
    cairo_t* context = drawingContext();
    cairo_save(context);
    cairo_identity_matrix(context);
    if (viewportClip_) {
        const int left = std::max(0, viewportLeft_);
        const int top = std::max(0, viewportTop_);
        const int right = std::min(getWidth(), viewportRight_);
        const int bottom = std::min(getHeight(), viewportBottom_);
        cairo_rectangle(context, left, top, std::max(0, right - left), std::max(0, bottom - top));
        cairo_clip(context);
    }
    const auto& t = transforms_.back();
    cairo_matrix_t matrix;
    cairo_matrix_init(&matrix, t[0], t[1], t[2], t[3],
        t[4] + viewportLeft_, t[5] + viewportTop_);
    cairo_set_matrix(context, &matrix);
    cairo_set_antialias(context,
        rasterOp_ == ROP_COPY && antialiasing_ ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
    cairo_set_line_width(context, lineWidth_);
    cairo_set_line_cap(context, lineCap(startCap_ == endCap_ ? startCap_ : RT_LINECAP_FLAT));
    cairo_set_line_join(context, lineJoin(lineJoin_));
    cairo_set_miter_limit(context, miterLimit_);

    double dash[16];
    int count = 0;
    switch (lineStyle_) {
    case LINE_DASHED: dash[0] = 6; dash[1] = 3; count = 2; break;
    case LINE_DOTTED: dash[0] = 1; dash[1] = 2; count = 2; break;
    case LINE_DASHDOT: dash[0] = 6; dash[1] = 2; dash[2] = 1; dash[3] = 2; count = 4; break;
    case LINE_DASHDOTDOT:
        dash[0] = 6; dash[1] = 2; dash[2] = 1; dash[3] = 2; dash[4] = 1; dash[5] = 2; count = 6; break;
    case LINE_USER:
        for (int bit = 15; bit >= 0 && count < 16; --bit) {
            const bool on = (linePattern_ & (1U << bit)) != 0;
            if (count == 0 || ((count & 1) == 0) != on) dash[count++] = 1.0;
            else dash[count - 1] += 1.0;
        }
        break;
    default: break;
    }
    cairo_set_dash(context, count == 0 ? nullptr : dash, count, 0.0);
    color_t source = color;
    if (drawingToScratch_) source = 0xFF000000U | (color & 0x00FFFFFFU);
    setSourceColor(context, source, false);
    (void)fill;
}

void CairoRenderTarget::mergeRasterScratch()
{
    if (!drawingToScratch_) return;
    cairo_surface_flush(rasterSurface_);
    cairo_surface_flush(cairoSurface_);
    for (int y = 0; y < getHeight(); ++y) {
        color_t* destination = surface_->row(static_cast<std::size_t>(y));
        const color_t* source = rasterScratch_->row(static_cast<std::size_t>(y));
        for (int x = 0; x < getWidth(); ++x) {
            const unsigned int coverage = channel(source[x], 24);
            if (coverage == 0) continue;
            const color_t operand = 0xFF000000U | (source[x] & 0x00FFFFFFU);
            const color_t result = applyPrimitiveRasterOp(destination[x], operand, rasterOp_);
            destination[x] = coverage == 255 ? result :
                blendPremultiplied(destination[x], premultiply(result), static_cast<unsigned char>(coverage));
        }
    }
    cairo_surface_mark_dirty(cairoSurface_);
    drawingToScratch_ = false;
}

void CairoRenderTarget::endStroke()
{
    cairo_t* context = drawingContext();
    if (lineStyle_ != LINE_NONE) cairo_stroke(context); else cairo_new_path(context);
    cairo_restore(context);
    mergeRasterScratch();
}

void CairoRenderTarget::fillCurrentPath()
{
    cairo_t* context = drawingContext();
    if (fillStyle_ == FILL_EMPTY) {
        cairo_new_path(context);
        return;
    }
    if (fillStyle_ == FILL_SOLID || fillStyle_ == FILL_USER) {
        cairo_fill(context);
        return;
    }
    color_t cells[64];
    const color_t foreground = premultiply(fillPatternColor_);
    const color_t background = backgroundOpaque_ ? premultiply(backgroundColor_) : 0U;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            cells[y * 8 + x] = patternUsesForeground(fillStyle_, x, y) ? foreground : background;
        }
    }
    cairo_surface_t* tile = cairo_image_surface_create_for_data(
        reinterpret_cast<unsigned char*>(cells), CAIRO_FORMAT_ARGB32, 8, 8, 32);
    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(tile);
    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
    cairo_set_source(context, pattern);
    cairo_fill(context);
    cairo_pattern_destroy(pattern);
    cairo_surface_destroy(tile);
}

void CairoRenderTarget::endFill()
{
    fillCurrentPath();
    cairo_restore(drawingContext());
    mergeRasterScratch();
}

void CairoRenderTarget::appendRoundedRectangle(
    double x, double y, double width, double height, double rx, double ry)
{
    cairo_t* context = drawingContext();
    rx = std::max(0.0, std::min(std::abs(width) * 0.5, std::abs(rx)));
    ry = std::max(0.0, std::min(std::abs(height) * 0.5, std::abs(ry)));
    if (rx == 0.0 || ry == 0.0) { cairo_rectangle(context, x, y, width, height); return; }
    cairo_save(context);
    cairo_translate(context, x, y);
    cairo_scale(context, rx, ry);
    const double right = width / rx, bottom = height / ry;
    cairo_new_sub_path(context);
    cairo_arc(context, right - 1.0, 1.0, 1.0, -kPi / 2.0, 0.0);
    cairo_arc(context, right - 1.0, bottom - 1.0, 1.0, 0.0, kPi / 2.0);
    cairo_arc(context, 1.0, bottom - 1.0, 1.0, kPi / 2.0, kPi);
    cairo_arc(context, 1.0, 1.0, 1.0, kPi, 3.0 * kPi / 2.0);
    cairo_close_path(context);
    cairo_restore(context);
}

void CairoRenderTarget::appendEllipseArc(double x, double y, double radiusX, double radiusY,
    double startAngle, double endAngle, bool reverse)
{
    cairo_t* context = drawingContext();
    if (radiusX <= 0.0 || radiusY <= 0.0) return;
    cairo_save(context);
    cairo_translate(context, x, y);
    cairo_scale(context, radiusX, radiusY);
    if (reverse) cairo_arc_negative(context, 0, 0, 1, startAngle, endAngle);
    else cairo_arc(context, 0, 0, 1, startAngle, endAngle);
    cairo_restore(context);
}

void CairoRenderTarget::appendPolygon(const int* points, int count, bool close)
{
    cairo_t* context = drawingContext();
    if (points == nullptr || count <= 0) return;
    cairo_move_to(context, points[0], points[1]);
    for (int i = 1; i < count; ++i) cairo_line_to(context, points[i * 2], points[i * 2 + 1]);
    if (close) cairo_close_path(context);
}

void CairoRenderTarget::drawLine(int x1, int y1, int x2, int y2)
{
    if (lineStyle_ == LINE_NONE) return;
    const float offset = !antialiasing_ &&
        (static_cast<int>(std::lround(lineWidth_)) & 1) ? 0.5f : 0.0f;
    beginDraw(lineColor_, false);
    cairo_move_to(drawingContext(), x1 + offset, y1 + offset);
    cairo_line_to(drawingContext(), x2 + offset, y2 + offset);
    endStroke();
}

void CairoRenderTarget::drawLineF(float x1, float y1, float x2, float y2)
{
    beginDraw(lineColor_, false);
    cairo_move_to(drawingContext(), x1, y1);
    cairo_line_to(drawingContext(), x2, y2);
    endStroke();
}

void CairoRenderTarget::lineTo(int x, int y)
{
    drawLine(currentX_, currentY_, x, y);
    currentX_ = x;
    currentY_ = y;
}

void CairoRenderTarget::lineRel(int dx, int dy) { lineTo(currentX_ + dx, currentY_ + dy); }

void CairoRenderTarget::drawRect(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0) return;
    beginDraw(lineColor_, false);
    cairo_rectangle(drawingContext(), x + 0.5, y + 0.5,
        std::max(0, width - 1), std::max(0, height - 1));
    endStroke();
}

void CairoRenderTarget::fillRect(int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0) return;
    beginDraw(fillPatternColor_, true);
    cairo_rectangle(drawingContext(), x, y, width, height);
    endFill();
}

void CairoRenderTarget::drawRoundRect(
    int x, int y, int width, int height, int ellipseWidth, int ellipseHeight)
{
    if (width <= 0 || height <= 0) return;
    beginDraw(lineColor_, false);
    appendRoundedRectangle(x + 0.5, y + 0.5, std::max(0, width - 1),
        std::max(0, height - 1), ellipseWidth * 0.5, ellipseHeight * 0.5);
    endStroke();
}

void CairoRenderTarget::fillRoundRect(
    int x, int y, int width, int height, int ellipseWidth, int ellipseHeight)
{
    if (width <= 0 || height <= 0) return;
    beginDraw(fillPatternColor_, true);
    appendRoundedRectangle(x, y, width, height, ellipseWidth * 0.5, ellipseHeight * 0.5);
    endFill();
}

void CairoRenderTarget::draw3DBar(
    int x, int y, int width, int height, int depth, int requestedFillStyle)
{
    const FillStyle savedStyle = fillStyle_;
    if (requestedFillStyle >= static_cast<int>(FILL_EMPTY) &&
        requestedFillStyle <= static_cast<int>(FILL_USER)) {
        fillStyle_ = static_cast<FillStyle>(requestedFillStyle);
    }
    fillRect(x, y, width, height);
    fillStyle_ = savedStyle;
    drawRect(x, y, width, height);
    const int topFace[] = {x, y, x + depth, y - depth,
        x + width + depth, y - depth, x + width, y};
    const int sideFace[] = {x + width, y, x + width + depth, y - depth,
        x + width + depth, y + height - depth, x + width, y + height};
    drawPolygon(topFace, 4);
    drawPolygon(sideFace, 4);
}

void CairoRenderTarget::drawCircle(int x, int y, int radius)
{
    drawEllipse(x, y, 0, 360, radius, radius);
}

void CairoRenderTarget::fillCircle(int x, int y, int radius)
{
    fillEllipse(x, y, 0, 360, radius, radius);
}

void CairoRenderTarget::drawEllipse(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    if (radiusX <= 0 || radiusY <= 0) return;
    beginDraw(lineColor_, false);
    appendEllipseArc(x, y, radiusX, radiusY,
        -startAngle * kPi / 180.0, -endAngle * kPi / 180.0, true);
    endStroke();
}

void CairoRenderTarget::fillEllipse(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    if (radiusX <= 0 || radiusY <= 0) return;
    beginDraw(fillPatternColor_, true);
    appendEllipseArc(x, y, radiusX, radiusY,
        -startAngle * kPi / 180.0, -endAngle * kPi / 180.0, true);
    cairo_close_path(drawingContext());
    endFill();
}

void CairoRenderTarget::drawSector(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    if (radiusX <= 0 || radiusY <= 0) return;
    beginDraw(lineColor_, false);
    cairo_move_to(drawingContext(), x, y);
    appendEllipseArc(x, y, radiusX, radiusY,
        -startAngle * kPi / 180.0, -endAngle * kPi / 180.0, true);
    cairo_close_path(drawingContext());
    endStroke();
}

void CairoRenderTarget::fillSector(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    if (radiusX <= 0 || radiusY <= 0) return;
    beginDraw(fillPatternColor_, true);
    cairo_move_to(drawingContext(), x, y);
    appendEllipseArc(x, y, radiusX, radiusY,
        -startAngle * kPi / 180.0, -endAngle * kPi / 180.0, true);
    cairo_close_path(drawingContext());
    endFill();
}

void CairoRenderTarget::drawPie(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    drawSector(x, y, startAngle, endAngle, radiusX, radiusY);
}

void CairoRenderTarget::fillPie(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    fillSector(x, y, startAngle, endAngle, radiusX, radiusY);
}

void CairoRenderTarget::drawArc(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    drawEllipse(x, y, startAngle, endAngle, radiusX, radiusY);
}

void CairoRenderTarget::drawChord(
    int x, int y, int startAngle, int endAngle, int radiusX, int radiusY)
{
    if (radiusX <= 0 || radiusY <= 0) return;
    beginDraw(lineColor_, false);
    appendEllipseArc(x, y, radiusX, radiusY,
        -startAngle * kPi / 180.0, -endAngle * kPi / 180.0, true);
    cairo_close_path(drawingContext());
    endStroke();
}

void CairoRenderTarget::drawPolygon(const int* points, int count)
{
    if (points == nullptr || count < 2) return;
    beginDraw(lineColor_, false);
    appendPolygon(points, count, true);
    endStroke();
}

void CairoRenderTarget::fillPolygon(const int* points, int count)
{
    if (points == nullptr || count < 3) return;
    beginDraw(fillPatternColor_, true);
    appendPolygon(points, count, true);
    endFill();
}

void CairoRenderTarget::drawPolyline(const int* points, int count)
{
    if (points == nullptr || count < 2) return;
    beginDraw(lineColor_, false);
    appendPolygon(points, count, false);
    endStroke();
}

bool CairoRenderTarget::insideClip(int x, int y) const
{
    if (x < 0 || y < 0 || x >= getWidth() || y >= getHeight()) return false;
    return !viewportClip_ || (x >= viewportLeft_ && y >= viewportTop_ &&
        x < viewportRight_ && y < viewportBottom_);
}

color_t& CairoRenderTarget::pixelAt(int x, int y)
{
    return surface_->row(static_cast<std::size_t>(y))[x];
}

color_t CairoRenderTarget::pixelAt(int x, int y) const
{
    return surface_->row(static_cast<std::size_t>(y))[x];
}

color_t CairoRenderTarget::applyRasterOp(
    color_t destination, color_t source, RasterOp operation) noexcept
{
    switch (operation) {
    case ROP_BLACK: return 0x00000000U;
    case ROP_NOTMERGEPEN: return ~(destination | source);
    case ROP_MASKNOTPEN: return destination & ~source;
    case ROP_NOTCOPYPEN: return ~source;
    case ROP_MASKPENNOT: return source & ~destination;
    case ROP_NOT: return ~destination;
    case ROP_XOR: return destination ^ source;
    case ROP_NOTMASKPEN: return ~(destination & source);
    case ROP_AND: return destination & source;
    case ROP_NOTXORPEN: return ~(destination ^ source);
    case ROP_NOP: return destination;
    case ROP_MERGENOTPEN: return destination | ~source;
    case ROP_COPY: return source;
    case ROP_MERGEPENNOT: return source | ~destination;
    case ROP_OR: return destination | source;
    case ROP_WHITE: return 0xFFFFFFFFU;
    default: return source;
    }
}

color_t CairoRenderTarget::applyPrimitiveRasterOp(
    color_t destination, color_t source, RasterOp operation) noexcept
{
    if (operation == ROP_NOP) return destination;
    const unsigned int alpha = channel(destination, 24);
    const auto unpremultiplyChannel = [alpha](unsigned int value) {
        return alpha == 0 ? 0U : std::min(255U, (value * 255U + alpha / 2U) / alpha);
    };
    const color_t destinationRGB =
        (unpremultiplyChannel(channel(destination, 16)) << 16U) |
        (unpremultiplyChannel(channel(destination, 8)) << 8U) |
        unpremultiplyChannel(channel(destination, 0));
    const color_t sourceRGB = source & 0x00FFFFFFU;
    color_t result = destinationRGB;
    switch (operation) {
    case ROP_BLACK:       result = 0x000000U; break;
    case ROP_NOTMERGEPEN: result = ~(destinationRGB | sourceRGB); break;
    case ROP_MASKNOTPEN:  result = destinationRGB & ~sourceRGB; break;
    case ROP_NOTCOPYPEN:  result = ~sourceRGB; break;
    case ROP_MASKPENNOT:  result = sourceRGB & ~destinationRGB; break;
    case ROP_NOT:         result = ~destinationRGB; break;
    case ROP_XOR:         result = destinationRGB ^ sourceRGB; break;
    case ROP_NOTMASKPEN:  result = ~(destinationRGB & sourceRGB); break;
    case ROP_AND:         result = destinationRGB & sourceRGB; break;
    case ROP_NOTXORPEN:   result = ~(destinationRGB ^ sourceRGB); break;
    case ROP_NOP:         result = destinationRGB; break;
    case ROP_MERGENOTPEN: result = destinationRGB | ~sourceRGB; break;
    case ROP_COPY:        result = sourceRGB; break;
    case ROP_MERGEPENNOT: result = sourceRGB | ~destinationRGB; break;
    case ROP_OR:          result = destinationRGB | sourceRGB; break;
    case ROP_WHITE:       result = 0x00FFFFFFU; break;
    default:              result = sourceRGB; break;
    }
    return pack(alpha, scaleByte(channel(result, 16), alpha),
        scaleByte(channel(result, 8), alpha), scaleByte(channel(result, 0), alpha));
}

color_t CairoRenderTarget::premultiply(color_t straight) noexcept
{
    const unsigned int alpha = channel(straight, 24);
    return pack(alpha, scaleByte(channel(straight, 16), alpha),
        scaleByte(channel(straight, 8), alpha), scaleByte(channel(straight, 0), alpha));
}

color_t CairoRenderTarget::blendPremultiplied(
    color_t destination, color_t source, unsigned char factor) noexcept
{
    const unsigned int sourceAlpha = scaleByte(channel(source, 24), factor);
    const unsigned int inverse = 255U - sourceAlpha;
    return pack(sourceAlpha + scaleByte(channel(destination, 24), inverse),
        scaleByte(channel(source, 16), factor) + scaleByte(channel(destination, 16), inverse),
        scaleByte(channel(source, 8), factor) + scaleByte(channel(destination, 8), inverse),
        scaleByte(channel(source, 0), factor) + scaleByte(channel(destination, 0), inverse));
}

color_t CairoRenderTarget::blendStraight(
    color_t destination, color_t source, unsigned char factor) noexcept
{
    return blendPremultiplied(destination, premultiply(source), factor);
}

void CairoRenderTarget::writePixel(int x, int y, color_t color, bool useRasterOp)
{
    if (!insideClip(x, y)) return;
    color_t& destination = pixelAt(x, y);
    destination = useRasterOp ? applyRasterOp(destination, color, rasterOp_) : color;
    cairo_surface_mark_dirty_rectangle(cairoSurface_, x, y, 1, 1);
}

void CairoRenderTarget::putPixel(int x, int y, color_t color)
{
    const auto& m = transforms_.back();
    const int px = static_cast<int>(std::lround(m[0] * x + m[2] * y + m[4])) + viewportLeft_;
    const int py = static_cast<int>(std::lround(m[1] * x + m[3] * y + m[5])) + viewportTop_;
    writePixel(px, py, color);
}

color_t CairoRenderTarget::getPixel(int x, int y) const
{
    const int px = x + viewportLeft_, py = y + viewportTop_;
    return insideClip(px, py) ? pixelAt(px, py) : 0U;
}

void CairoRenderTarget::putPixelAlpha(int x, int y, color_t color)
{
    const int px = x + viewportLeft_, py = y + viewportTop_;
    if (insideClip(px, py)) writePixel(px, py, blendStraight(pixelAt(px, py), color), false);
}

void CairoRenderTarget::putPixelSaveAlpha(int x, int y, color_t color)
{
    const int px = x + viewportLeft_, py = y + viewportTop_;
    if (insideClip(px, py)) writePixel(px, py,
        (pixelAt(px, py) & 0xFF000000U) | (color & 0x00FFFFFFU), false);
}

void CairoRenderTarget::putPixelAlphaBlend(
    int x, int y, color_t color, unsigned char alphaFactor)
{
    const int px = x + viewportLeft_, py = y + viewportTop_;
    if (insideClip(px, py)) writePixel(px, py,
        blendStraight(pixelAt(px, py), color, alphaFactor), false);
}

void CairoRenderTarget::putPixels(int count, const int* points)
{
    if (points == nullptr || count <= 0) return;
    for (int i = 0; i < count; ++i) putPixel(points[i * 2], points[i * 2 + 1], lineColor_);
}

void CairoRenderTarget::floodFillInternal(int x, int y, color_t boundary, bool surfaceMode)
{
    if (fillStyle_ == FILL_EMPTY) return;
    const int seedX = x + viewportLeft_, seedY = y + viewportTop_;
    if (!insideClip(seedX, seedY)) return;
    cairo_surface_flush(cairoSurface_);
    const color_t target = pixelAt(seedX, seedY);
    const color_t compare = surfaceMode ? premultiply(boundary) : boundary;
    const bool seedMatches = surfaceMode ?
        ((target & 0x00FFFFFFU) == (compare & 0x00FFFFFFU)) :
        ((target & 0x00FFFFFFU) != (compare & 0x00FFFFFFU));
    if (!seedMatches) return;
    const color_t storedFill = premultiply(fillPatternColor_);
    const color_t storedBackground = backgroundOpaque_ ? premultiply(backgroundColor_) : target;
    std::vector<unsigned char> visited(static_cast<std::size_t>(getWidth()) * getHeight(), 0);
    std::vector<int> stack;
    stack.push_back(seedY * getWidth() + seedX);
    while (!stack.empty()) {
        const int index = stack.back(); stack.pop_back();
        if (visited[static_cast<std::size_t>(index)] != 0) continue;
        visited[static_cast<std::size_t>(index)] = 1;
        const int px = index % getWidth(), py = index / getWidth();
        if (!insideClip(px, py)) continue;
        const color_t value = pixelAt(px, py);
        const bool matches = surfaceMode ?
            ((value & 0x00FFFFFFU) == (compare & 0x00FFFFFFU)) :
            ((value & 0x00FFFFFFU) != (compare & 0x00FFFFFFU));
        if (!matches) continue;
        pixelAt(px, py) = patternUsesForeground(fillStyle_, px, py) ? storedFill : storedBackground;
        if (px > 0) stack.push_back(index - 1);
        if (px + 1 < getWidth()) stack.push_back(index + 1);
        if (py > 0) stack.push_back(index - getWidth());
        if (py + 1 < getHeight()) stack.push_back(index + getWidth());
    }
    cairo_surface_mark_dirty(cairoSurface_);
}

void CairoRenderTarget::floodFill(int x, int y, color_t borderColor)
{
    floodFillInternal(x, y, borderColor, false);
}

void CairoRenderTarget::floodFillSurface(int x, int y, color_t surfaceColor)
{
    floodFillInternal(x, y, surfaceColor, true);
}

void CairoRenderTarget::clear(color_t color)
{
    cairo_surface_flush(cairoSurface_);
    surface_->clear(premultiply(color));
    cairo_surface_mark_dirty(cairoSurface_);
}

CairoRenderTarget::SourceImage CairoRenderTarget::captureSource(
    RenderTarget* source, int x, int y, int width, int height)
{
    SourceImage result = {std::max(0, width), std::max(0, height), {}};
    if (source == nullptr || width <= 0 || height <= 0) return result;
    source->flush();
    const color_t* pixels = source->getPixelBuffer();
    if (pixels == nullptr) return result;
    result.pixels.assign(static_cast<std::size_t>(width) * height, 0U);
    for (int row = 0; row < height; ++row) {
        const int sourceY = y + row;
        if (sourceY < 0 || sourceY >= source->getHeight()) continue;
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

color_t CairoRenderTarget::sample(const SourceImage& source, float x, float y, bool smooth) noexcept
{
    if (source.width <= 0 || source.height <= 0 || source.pixels.empty()) return 0;
    if (!smooth) {
        const int sx = std::clamp(static_cast<int>(std::floor(x + 0.5f)), 0, source.width - 1);
        const int sy = std::clamp(static_cast<int>(std::floor(y + 0.5f)), 0, source.height - 1);
        return source.pixels[static_cast<std::size_t>(sy) * source.width + sx];
    }
    const float cx = std::clamp(x, 0.0f, static_cast<float>(source.width - 1));
    const float cy = std::clamp(y, 0.0f, static_cast<float>(source.height - 1));
    const int x0 = static_cast<int>(std::floor(cx)), y0 = static_cast<int>(std::floor(cy));
    const int x1 = std::min(source.width - 1, x0 + 1), y1 = std::min(source.height - 1, y0 + 1);
    const float fx = cx - x0, fy = cy - y0;
    const color_t p00 = source.pixels[static_cast<std::size_t>(y0) * source.width + x0];
    const color_t p10 = source.pixels[static_cast<std::size_t>(y0) * source.width + x1];
    const color_t p01 = source.pixels[static_cast<std::size_t>(y1) * source.width + x0];
    const color_t p11 = source.pixels[static_cast<std::size_t>(y1) * source.width + x1];
    auto interpolate = [=](unsigned int shift) {
        const float top = channel(p00, shift) + (channel(p10, shift) - channel(p00, shift)) * fx;
        const float bottom = channel(p01, shift) + (channel(p11, shift) - channel(p01, shift)) * fx;
        return static_cast<unsigned int>(std::lround(top + (bottom - top) * fy));
    };
    return pack(interpolate(24), interpolate(16), interpolate(8), interpolate(0));
}

void CairoRenderTarget::stretchTransfer(int dstX, int dstY, int dstWidth, int dstHeight,
    const SourceImage& source, ImageAlphaFormat format, unsigned char alpha, bool smooth, bool blend)
{
    if (dstWidth <= 0 || dstHeight <= 0 || source.width <= 0 || source.height <= 0) return;
    cairo_surface_flush(cairoSurface_);
    for (int y = 0; y < dstHeight; ++y) {
        const float sy = (y + 0.5f) * source.height / dstHeight - 0.5f;
        for (int x = 0; x < dstWidth; ++x) {
            const int px = dstX + x + viewportLeft_, py = dstY + y + viewportTop_;
            if (!insideClip(px, py)) continue;
            const float sx = (x + 0.5f) * source.width / dstWidth - 0.5f;
            color_t value = sample(source, sx, sy, smooth);
            if (format == IMAGE_ALPHA_STRAIGHT) value = premultiply(value);
            else if (format == IMAGE_ALPHA_OPAQUE) value = 0xFF000000U | (value & 0x00FFFFFFU);
            pixelAt(px, py) = blend ? blendPremultiplied(pixelAt(px, py), value, alpha) :
                applyRasterOp(pixelAt(px, py), value, rasterOp_);
        }
    }
    cairo_surface_mark_dirty(cairoSurface_);
}

void CairoRenderTarget::blit(
    int dstX, int dstY, RenderTarget* source, int srcX, int srcY, int width, int height)
{
    stretchTransfer(dstX, dstY, width, height,
        captureSource(source, srcX, srcY, width, height),
        IMAGE_ALPHA_PREMULTIPLIED, 255, false, false);
}

void CairoRenderTarget::blitStretch(int dstX, int dstY, int dstWidth, int dstHeight,
    RenderTarget* source, int srcX, int srcY, int srcWidth, int srcHeight)
{
    stretchTransfer(dstX, dstY, dstWidth, dstHeight,
        captureSource(source, srcX, srcY, srcWidth, srcHeight),
        IMAGE_ALPHA_PREMULTIPLIED, 255, false, false);
}

void CairoRenderTarget::alphaBlend(int dstX, int dstY, int dstWidth, int dstHeight,
    RenderTarget* source, int srcX, int srcY, int srcWidth, int srcHeight,
    unsigned char alpha, ImageAlphaFormat format, bool smooth)
{
    stretchTransfer(dstX, dstY, dstWidth, dstHeight,
        captureSource(source, srcX, srcY, srcWidth, srcHeight), format, alpha, smooth, true);
}

void CairoRenderTarget::alphaTransparent(int dstX, int dstY, RenderTarget* source,
    int srcX, int srcY, int width, int height, color_t transparentColor, unsigned char alpha)
{
    const SourceImage snapshot = captureSource(source, srcX, srcY, width, height);
    cairo_surface_flush(cairoSurface_);
    for (int y = 0; y < snapshot.height; ++y) {
        for (int x = 0; x < snapshot.width; ++x) {
            const color_t value = snapshot.pixels[static_cast<std::size_t>(y) * snapshot.width + x];
            if ((value & 0x00FFFFFFU) == (transparentColor & 0x00FFFFFFU)) continue;
            const int px = dstX + x + viewportLeft_, py = dstY + y + viewportTop_;
            if (insideClip(px, py)) pixelAt(px, py) = blendPremultiplied(pixelAt(px, py), value, alpha);
        }
    }
    cairo_surface_mark_dirty(cairoSurface_);
}

void CairoRenderTarget::withAlpha(int dstX, int dstY, int dstWidth, int dstHeight,
    RenderTarget* source, int srcX, int srcY, int srcWidth, int srcHeight, bool smooth)
{
    alphaBlend(dstX, dstY, dstWidth, dstHeight, source, srcX, srcY,
        srcWidth, srcHeight, 255, IMAGE_ALPHA_PREMULTIPLIED, smooth);
}

void CairoRenderTarget::alphaFilter(int dstX, int dstY, int width, int height,
    RenderTarget* source, int srcX, int srcY, unsigned char alpha)
{
    alphaBlend(dstX, dstY, width, height, source, srcX, srcY,
        width, height, alpha, IMAGE_ALPHA_PREMULTIPLIED, false);
}

void CairoRenderTarget::rotateBlend(int dstX, int dstY, int dstWidth, int dstHeight,
    RenderTarget* source, int srcX, int srcY, int srcWidth, int srcHeight,
    float angle, float centerX, float centerY, bool transparent, int alpha, bool smooth)
{
    rotateZoomBlend(dstX, dstY, dstWidth, dstHeight, source, srcX, srcY,
        srcWidth, srcHeight, angle, centerX, centerY, 1.0f, 1.0f, transparent, alpha, smooth);
}

void CairoRenderTarget::rotateZoomBlend(int dstX, int dstY, int dstWidth, int dstHeight,
    RenderTarget* source, int srcX, int srcY, int srcWidth, int srcHeight,
    float angle, float centerX, float centerY, float zoomX, float zoomY,
    bool transparent, int alpha, bool smooth)
{
    if (dstWidth <= 0 || dstHeight <= 0 || zoomX == 0.0f || zoomY == 0.0f) return;
    const SourceImage snapshot = captureSource(source, srcX, srcY, srcWidth, srcHeight);
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const unsigned char factor = static_cast<unsigned char>(std::clamp(alpha < 0 ? 255 : alpha, 0, 255));
    cairo_surface_flush(cairoSurface_);
    for (int y = 0; y < dstHeight; ++y) {
        for (int x = 0; x < dstWidth; ++x) {
            const float dx = (x - centerX) / zoomX, dy = (y - centerY) / zoomY;
            const float sx = cosine * dx + sine * dy + centerX;
            const float sy = -sine * dx + cosine * dy + centerY;
            if (sx < -0.5f || sy < -0.5f || sx >= snapshot.width - 0.5f || sy >= snapshot.height - 0.5f) continue;
            const color_t value = sample(snapshot, sx, sy, smooth);
            if (transparent && value == 0) continue;
            const int px = dstX - static_cast<int>(std::lround(centerX)) + x + viewportLeft_;
            const int py = dstY - static_cast<int>(std::lround(centerY)) + y + viewportTop_;
            if (insideClip(px, py)) pixelAt(px, py) = alpha >= 0 ?
                blendPremultiplied(pixelAt(px, py), value, factor) :
                applyRasterOp(pixelAt(px, py), value, rasterOp_);
        }
    }
    cairo_surface_mark_dirty(cairoSurface_);
}

void CairoRenderTarget::blitAffine(RenderTarget* source, int srcX, int srcY,
    int srcWidth, int srcHeight, const float* points, bool premultipliedAlpha, bool smooth)
{
    if (points == nullptr || srcWidth <= 0 || srcHeight <= 0) return;
    const SourceImage snapshot = captureSource(source, srcX, srcY, srcWidth, srcHeight);
    const float p0x = points[0], p0y = points[1];
    const float ux = points[2] - p0x, uy = points[3] - p0y;
    const float vx = points[6] - p0x, vy = points[7] - p0y;
    const float determinant = ux * vy - uy * vx;
    if (std::abs(determinant) < 1e-8f) return;
    float minX = points[0], maxX = points[0], minY = points[1], maxY = points[1];
    for (int i = 1; i < 4; ++i) {
        minX = std::min(minX, points[i * 2]); maxX = std::max(maxX, points[i * 2]);
        minY = std::min(minY, points[i * 2 + 1]); maxY = std::max(maxY, points[i * 2 + 1]);
    }
    cairo_surface_flush(cairoSurface_);
    for (int y = static_cast<int>(std::floor(minY)); y < static_cast<int>(std::ceil(maxY)); ++y) {
        for (int x = static_cast<int>(std::floor(minX)); x < static_cast<int>(std::ceil(maxX)); ++x) {
            const float dx = x + 0.5f - p0x, dy = y + 0.5f - p0y;
            const float u = (dx * vy - dy * vx) / determinant;
            const float v = (ux * dy - uy * dx) / determinant;
            if (u < 0 || u > 1 || v < 0 || v > 1) continue;
            color_t value = sample(snapshot, u * (srcWidth - 1), v * (srcHeight - 1), smooth);
            if (!premultipliedAlpha) value = premultiply(value);
            const int px = x + viewportLeft_, py = y + viewportTop_;
            if (insideClip(px, py)) pixelAt(px, py) =
                blendPremultiplied(pixelAt(px, py), value);
        }
    }
    cairo_surface_mark_dirty(cairoSurface_);
}

void CairoRenderTarget::filterBlur(int dstX, int dstY, int width, int height, float intensity)
{
    if (width <= 0 || height <= 0 || intensity <= 0) return;
    const int left = std::max(0, dstX + viewportLeft_);
    const int top = std::max(0, dstY + viewportTop_);
    const int right = std::min(getWidth(), dstX + viewportLeft_ + width);
    const int bottom = std::min(getHeight(), dstY + viewportTop_ + height);
    if (left >= right || top >= bottom) return;
    const int clippedWidth = right - left, clippedHeight = bottom - top;
    const int radius = std::max(1, static_cast<int>(std::ceil(intensity)));
    std::vector<color_t> source(static_cast<std::size_t>(clippedWidth) * clippedHeight);
    cairo_surface_flush(cairoSurface_);
    for (int y = 0; y < clippedHeight; ++y) {
        std::memcpy(source.data() + static_cast<std::size_t>(y) * clippedWidth,
            surface_->row(top + y) + left, static_cast<std::size_t>(clippedWidth) * sizeof(color_t));
    }
    for (int y = 0; y < clippedHeight; ++y) {
        for (int x = 0; x < clippedWidth; ++x) {
            unsigned long long sums[4] = {0, 0, 0, 0}; unsigned int count = 0;
            for (int sy = std::max(0, y - radius); sy <= std::min(clippedHeight - 1, y + radius); ++sy) {
                for (int sx = std::max(0, x - radius); sx <= std::min(clippedWidth - 1, x + radius); ++sx) {
                    const color_t value = source[static_cast<std::size_t>(sy) * clippedWidth + sx];
                    sums[0] += channel(value, 24); sums[1] += channel(value, 16);
                    sums[2] += channel(value, 8); sums[3] += channel(value, 0); ++count;
                }
            }
            pixelAt(left + x, top + y) = pack(sums[0] / count, sums[1] / count,
                sums[2] / count, sums[3] / count);
        }
    }
    cairo_surface_mark_dirty(cairoSurface_);
}

void CairoRenderTarget::setFont(int height, int width, const char* face,
    int escapement, int orientation, int weight, bool italic, bool underline, bool strikeout)
{
    font_.height = height != 0 ? height : 16;
    font_.width = width;
    font_.face = face != nullptr && face[0] != '\0' ? face : "sans";
    font_.escapement = escapement;
    font_.orientation = orientation;
    font_.weight = weight;
    font_.italic = italic;
    font_.underline = underline;
    font_.strikeout = strikeout;
}

void CairoRenderTarget::getFont(int* height, int* width, char* face, int faceCapacity,
    int* escapement, int* orientation, int* weight, bool* italic,
    bool* underline, bool* strikeout) const
{
    if (height != nullptr) *height = font_.height;
    if (width != nullptr) *width = font_.width;
    if (face != nullptr && faceCapacity > 0) {
        std::strncpy(face, font_.face.c_str(), static_cast<std::size_t>(faceCapacity - 1));
        face[faceCapacity - 1] = '\0';
    }
    if (escapement != nullptr) *escapement = font_.escapement;
    if (orientation != nullptr) *orientation = font_.orientation;
    if (weight != nullptr) *weight = font_.weight;
    if (italic != nullptr) *italic = font_.italic;
    if (underline != nullptr) *underline = font_.underline;
    if (strikeout != nullptr) *strikeout = font_.strikeout;
}

void CairoRenderTarget::setTextJustify(TextHAlign horizontal, TextVAlign vertical)
{
    horizontalAlign_ = horizontal;
    verticalAlign_ = vertical;
}

std::string CairoRenderTarget::wideToUtf8(const wchar_t* text) const
{
    return text != nullptr ? ege::w2utf8(text) : std::string();
}

void CairoRenderTarget::configureFont() const
{
    cairo_select_font_face(cairo_, font_.face.c_str(),
        font_.italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
        font_.weight >= 600 ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cairo_, std::max(1, std::abs(font_.height)));
}

void CairoRenderTarget::textExtents(const char* text, cairo_text_extents_t* textExtents,
    cairo_font_extents_t* fontExtents) const
{
    cairo_save(cairo_);
    configureFont();
    if (textExtents != nullptr) cairo_text_extents(cairo_, text != nullptr ? text : "", textExtents);
    if (fontExtents != nullptr) cairo_font_extents(cairo_, fontExtents);
    cairo_restore(cairo_);
}

void CairoRenderTarget::measureText(const char* text, float* width, float* height) const
{
    cairo_text_extents_t extents = {};
    cairo_font_extents_t fontExtents = {};
    textExtents(text, &extents, &fontExtents);
    const double widthScale = font_.width > 0 ?
        static_cast<double>(font_.width) / std::max(1, std::abs(font_.height)) : 1.0;
    if (width != nullptr) *width = static_cast<float>(extents.x_advance * widthScale);
    if (height != nullptr) *height = static_cast<float>(fontExtents.height);
}

void CairoRenderTarget::measureText(const wchar_t* text, float* width, float* height) const
{
    const std::string utf8 = wideToUtf8(text);
    measureText(utf8.c_str(), width, height);
}

int CairoRenderTarget::getTextWidth(const char* text) const
{
    float width = 0; measureText(text, &width, nullptr); return static_cast<int>(std::lround(width));
}

int CairoRenderTarget::getTextWidth(const wchar_t* text) const
{
    float width = 0; measureText(text, &width, nullptr); return static_cast<int>(std::lround(width));
}

int CairoRenderTarget::getTextHeight(const char* text) const
{
    float height = 0; measureText(text, nullptr, &height); return static_cast<int>(std::lround(height));
}

int CairoRenderTarget::getTextHeight(const wchar_t* text) const
{
    float height = 0; measureText(text, nullptr, &height); return static_cast<int>(std::lround(height));
}

void CairoRenderTarget::drawTextUtf8(float x, float y, const char* text)
{
    if (text == nullptr) return;
    cairo_text_extents_t textMetrics = {};
    cairo_font_extents_t fontMetrics = {};
    textExtents(text, &textMetrics, &fontMetrics);
    const double widthScale = font_.width > 0 ?
        static_cast<double>(font_.width) / std::max(1, std::abs(font_.height)) : 1.0;
    const double textWidth = textMetrics.x_advance * widthScale;
    const double textHeight = fontMetrics.height;
    double drawX = x, drawY = y;
    if (horizontalAlign_ == TEXT_CENTER) drawX -= textWidth * 0.5;
    else if (horizontalAlign_ == TEXT_RIGHT) drawX -= textWidth;
    if (verticalAlign_ == TEXT_MIDDLE) drawY -= textHeight * 0.5;
    else if (verticalAlign_ == TEXT_BOTTOM) drawY -= textHeight;

    if (backgroundOpaque_) {
        const FillStyle savedStyle = fillStyle_;
        const color_t savedColor = fillPatternColor_;
        fillStyle_ = FILL_SOLID;
        fillPatternColor_ = backgroundColor_;
        fillRect(static_cast<int>(std::floor(drawX)), static_cast<int>(std::floor(drawY)),
            static_cast<int>(std::ceil(textWidth)), static_cast<int>(std::ceil(textHeight)));
        fillStyle_ = savedStyle;
        fillPatternColor_ = savedColor;
    }

    beginDraw(textColor_, true);
    cairo_t* context = drawingContext();
    cairo_select_font_face(context, font_.face.c_str(),
        font_.italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
        font_.weight >= 600 ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(context, std::max(1, std::abs(font_.height)));
    cairo_translate(context, drawX, drawY + fontMetrics.ascent);
    cairo_rotate(context, -font_.escapement * kPi / 1800.0);
    cairo_scale(context, widthScale, 1.0);
    cairo_move_to(context, 0, 0);
    cairo_show_text(context, text);
    const double lineWidth = std::max(1.0, textHeight / 14.0);
    cairo_set_line_width(context, lineWidth / std::max(0.01, widthScale));
    if (font_.underline) {
        cairo_move_to(context, 0, fontMetrics.descent * 0.45);
        cairo_line_to(context, textMetrics.x_advance, fontMetrics.descent * 0.45);
        cairo_stroke(context);
    }
    if (font_.strikeout) {
        cairo_move_to(context, 0, -fontMetrics.ascent * 0.42);
        cairo_line_to(context, textMetrics.x_advance, -fontMetrics.ascent * 0.42);
        cairo_stroke(context);
    }
    cairo_restore(context);
    mergeRasterScratch();
}

void CairoRenderTarget::drawText(float x, float y, const char* text) { drawTextUtf8(x, y, text); }

void CairoRenderTarget::drawText(float x, float y, const wchar_t* text)
{
    const std::string utf8 = wideToUtf8(text); drawTextUtf8(x, y, utf8.c_str());
}

color_t* CairoRenderTarget::getPixelBuffer()
{
    cairo_surface_flush(cairoSurface_); return surface_->data();
}

const color_t* CairoRenderTarget::getPixelBuffer() const
{
    cairo_surface_flush(cairoSurface_); return surface_->data();
}

color_t* CairoRenderTarget::getPixelBufferForWrite(int x, int y, int width, int height)
{
    (void)x; (void)y; (void)width; (void)height;
    cairo_surface_flush(cairoSurface_); return surface_->data();
}

bool CairoRenderTarget::updatePixelBuffer(int x, int y, int width, int height,
    const color_t* pixels, int pitchBytes)
{
    const std::size_t rowBytes = width > 0 ? static_cast<std::size_t>(width) * sizeof(color_t) : 0;
    if (pixels == nullptr || width <= 0 || height <= 0 || x < 0 || y < 0 ||
        width > getWidth() - x || height > getHeight() - y || pitchBytes < 0 ||
        static_cast<std::size_t>(pitchBytes) < rowBytes) return false;
    cairo_surface_flush(cairoSurface_);
    const unsigned char* source = reinterpret_cast<const unsigned char*>(pixels);
    for (int row = 0; row < height; ++row) {
        std::memmove(surface_->row(static_cast<std::size_t>(y + row)) + x,
            source + static_cast<std::size_t>(row) * pitchBytes, rowBytes);
    }
    cairo_surface_mark_dirty_rectangle(cairoSurface_, x, y, width, height);
    return true;
}

void CairoRenderTarget::flush() { cairo_surface_flush(cairoSurface_); }
void CairoRenderTarget::present() { flush(); }

} // namespace backend
} // namespace ege
