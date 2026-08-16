#ifndef EGE_BACKEND_LINUX_CAIRO_RENDER_TARGET_H
#define EGE_BACKEND_LINUX_CAIRO_RENDER_TARGET_H

#include "backend/interface/PixelSurface.h"
#include "backend/interface/RenderTarget.h"

#include <cairo/cairo.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace ege
{
namespace backend
{

/**
 * Linux CPU renderer backed directly by PixelSurface.
 *
 * Cairo's native-endian CAIRO_FORMAT_ARGB32 is premultiplied 0xAARRGGBB on
 * little-endian Linux, exactly matching PixelSurface.  There is no staging
 * image and getPixelBuffer() always returns the authoritative storage.
 */
class CairoRenderTarget final : public RenderTarget
{
public:
    explicit CairoRenderTarget(int width, int height, bool onScreen = false);
    ~CairoRenderTarget() override;

    CairoRenderTarget(const CairoRenderTarget&)            = delete;
    CairoRenderTarget& operator=(const CairoRenderTarget&) = delete;

    bool valid() const noexcept;
    bool resize(int width, int height, bool preservePixels = false);

    int  getWidth() const override;
    int  getHeight() const override;
    bool isOnScreen() const override;

    void setLineColor(color_t color) override;
    void setFillColor(color_t color) override;
    void setTextColor(color_t color) override;
    void setBkColor(color_t color) override;
    void setBkMode(bool opaque) override;
    void setLineWidth(float width) override;
    void setLineStyle(LineStyle style, unsigned short pattern, int thickness) override;
    void setLineCap(RTLineCap startCap, RTLineCap endCap) override;
    void setLineJoin(RTLineJoin join, float miterLimit) override;
    void setFillStyle(FillStyle style, color_t color) override;
    void setRasterOp(RasterOp rop) override;
    void setWritingMode(int mode) override;
    void setAntialiasing(bool enabled) override;

    color_t   getLineColor() const override;
    color_t   getFillColor() const override;
    color_t   getTextColor() const override;
    color_t   getBkColor() const override;
    FillStyle getFillStyle() const override;

    void setViewport(int left, int top, int right, int bottom, bool clip) override;
    void getViewport(int* left, int* top, int* right, int* bottom, int* clip) const override;
    void clearViewport() override;

    void pushTransform() override;
    void popTransform() override;
    void resetTransform() override;
    void translate(float dx, float dy) override;
    void rotate(float angle) override;
    void scale(float sx, float sy) override;
    void setTransformMatrix(const float* matrix) override;

    void moveTo(int x, int y) override;
    void moveRel(int dx, int dy) override;
    int  getCurrentX() const override;
    int  getCurrentY() const override;

    void drawLine(int x1, int y1, int x2, int y2) override;
    void drawLineF(float x1, float y1, float x2, float y2) override;
    void lineTo(int x, int y) override;
    void lineRel(int dx, int dy) override;
    void drawRect(int x, int y, int width, int height) override;
    void fillRect(int x, int y, int width, int height) override;
    void drawRoundRect(int x, int y, int width, int height, int ellipseWidth, int ellipseHeight) override;
    void fillRoundRect(int x, int y, int width, int height, int ellipseWidth, int ellipseHeight) override;
    void draw3DBar(int x, int y, int width, int height, int depth, int fillStyle) override;
    void drawCircle(int x, int y, int radius) override;
    void fillCircle(int x, int y, int radius) override;
    void drawEllipse(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void fillEllipse(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void drawSector(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void fillSector(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void drawPie(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void fillPie(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void drawArc(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void drawChord(int x, int y, int startAngle, int endAngle, int radiusX, int radiusY) override;
    void drawPolygon(const int* points, int count) override;
    void fillPolygon(const int* points, int count) override;
    void drawPolyline(const int* points, int count) override;

    void    putPixel(int x, int y, color_t color) override;
    color_t getPixel(int x, int y) const override;
    void    putPixelAlpha(int x, int y, color_t color) override;
    void    putPixelSaveAlpha(int x, int y, color_t color) override;
    void    putPixelAlphaBlend(int x, int y, color_t color, unsigned char alphaFactor) override;
    void    putPixels(int count, const int* points) override;

    void floodFill(int x, int y, color_t borderColor) override;
    void floodFillSurface(int x, int y, color_t surfaceColor) override;
    void clear(color_t color) override;

    void blit(int dstX, int dstY, RenderTarget* source, int srcX, int srcY, int width, int height) override;
    void blitStretch(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source, int srcX, int srcY,
        int srcWidth, int srcHeight) override;
    void alphaBlend(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source, int srcX, int srcY,
        int srcWidth, int srcHeight, unsigned char alpha, ImageAlphaFormat format, bool smooth) override;
    void alphaTransparent(int dstX, int dstY, RenderTarget* source, int srcX, int srcY, int width, int height,
        color_t transparentColor, unsigned char alpha) override;
    void withAlpha(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source, int srcX, int srcY,
        int srcWidth, int srcHeight, bool smooth) override;
    void alphaFilter(int dstX, int dstY, int width, int height, RenderTarget* source, int srcX, int srcY,
        unsigned char alpha) override;
    void rotateBlend(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source, int srcX, int srcY,
        int srcWidth, int srcHeight, float angle, float centerX, float centerY, bool transparent, int alpha,
        bool smooth) override;
    void rotateZoomBlend(int dstX, int dstY, int dstWidth, int dstHeight, RenderTarget* source, int srcX, int srcY,
        int srcWidth, int srcHeight, float angle, float centerX, float centerY, float zoomX, float zoomY,
        bool transparent, int alpha, bool smooth) override;
    void blitAffine(RenderTarget* source, int srcX, int srcY, int srcWidth, int srcHeight,
        const float* destinationPoints, bool premultipliedAlpha, bool smooth) override;
    void filterBlur(int dstX, int dstY, int width, int height, float intensity) override;

    void setFont(int height, int width, const char* face, int escapement, int orientation, int weight, bool italic,
        bool underline, bool strikeout) override;
    void getFont(int* height, int* width, char* face, int faceCapacity, int* escapement, int* orientation,
        int* weight, bool* italic, bool* underline, bool* strikeout) const override;
    void setTextJustify(TextHAlign horizontal, TextVAlign vertical) override;
    void drawText(float x, float y, const char* text) override;
    void drawText(float x, float y, const wchar_t* text) override;
    int  getTextWidth(const char* text) const override;
    int  getTextWidth(const wchar_t* text) const override;
    int  getTextHeight(const char* text) const override;
    int  getTextHeight(const wchar_t* text) const override;
    void measureText(const char* text, float* width, float* height) const override;
    void measureText(const wchar_t* text, float* width, float* height) const override;

    color_t*       getPixelBuffer() override;
    const color_t* getPixelBuffer() const override;
    color_t*       getPixelBufferForWrite(int x, int y, int width, int height) override;
    bool updatePixelBuffer(int x, int y, int width, int height, const color_t* pixels, int pitchBytes) override;

    void flush() override;
    void present() override;

private:
    struct SourceImage
    {
        int width;
        int height;
        std::vector<color_t> pixels;
    };

    struct FontState
    {
        int height = 16;
        int width = 0;
        std::string face = "sans";
        int escapement = 0;
        int orientation = 0;
        int weight = 400;
        bool italic = false;
        bool underline = false;
        bool strikeout = false;
    };

    void recreateCairoSurface();
    cairo_t* drawingContext();
    void mergeRasterScratch();
    void beginDraw(color_t color, bool fill);
    void endStroke();
    void endFill();
    void appendRoundedRectangle(double x, double y, double width, double height, double rx, double ry);
    void appendEllipseArc(double x, double y, double radiusX, double radiusY,
        double startAngle, double endAngle, bool reverse = false);
    void appendPolygon(const int* points, int count, bool close);
    void fillCurrentPath();
    bool insideClip(int x, int y) const;
    color_t& pixelAt(int x, int y);
    color_t  pixelAt(int x, int y) const;
    void writePixel(int x, int y, color_t color, bool useRasterOp = true);
    void floodFillInternal(int x, int y, color_t boundary, bool surfaceMode);
    std::string wideToUtf8(const wchar_t* text) const;
    void configureFont() const;
    void textExtents(const char* text, cairo_text_extents_t* textExtents,
        cairo_font_extents_t* fontExtents) const;
    void drawTextUtf8(float x, float y, const char* text);

    static color_t applyRasterOp(color_t destination, color_t source, RasterOp operation) noexcept;
    static color_t applyPrimitiveRasterOp(color_t destination, color_t source, RasterOp operation) noexcept;
    static color_t premultiply(color_t straight) noexcept;
    static color_t blendPremultiplied(color_t destination, color_t source,
        unsigned char factor = 255) noexcept;
    static color_t blendStraight(color_t destination, color_t source,
        unsigned char factor = 255) noexcept;
    static SourceImage captureSource(RenderTarget* source, int x, int y, int width, int height);
    static color_t sample(const SourceImage& source, float x, float y, bool smooth) noexcept;
    void stretchTransfer(int dstX, int dstY, int dstWidth, int dstHeight, const SourceImage& source,
        ImageAlphaFormat format, unsigned char alpha, bool smooth, bool blend);

    std::unique_ptr<PixelSurface> surface_;
    cairo_surface_t* cairoSurface_ = nullptr;
    cairo_t* cairo_ = nullptr;
    std::unique_ptr<PixelSurface> rasterScratch_;
    cairo_surface_t* rasterSurface_ = nullptr;
    cairo_t* rasterCairo_ = nullptr;
    bool drawingToScratch_ = false;
    bool onScreen_;
    bool antialiasing_ = false;

    color_t lineColor_ = 0xFFFFFFFFU;
    color_t fillColor_ = 0xFFFFFFFFU;
    color_t textColor_ = 0xFFFFFFFFU;
    color_t backgroundColor_ = 0xFF000000U;
    bool backgroundOpaque_ = false;
    float lineWidth_ = 1.0f;
    LineStyle lineStyle_ = LINE_SOLID;
    unsigned short linePattern_ = 0xFFFFU;
    int lineThickness_ = 1;
    RTLineCap startCap_ = RT_LINECAP_FLAT;
    RTLineCap endCap_ = RT_LINECAP_FLAT;
    RTLineJoin lineJoin_ = RT_LINEJOIN_MITER;
    float miterLimit_ = 10.0f;
    FillStyle fillStyle_ = FILL_SOLID;
    color_t fillPatternColor_ = 0xFFFFFFFFU;
    RasterOp rasterOp_ = ROP_COPY;
    int writingMode_ = 0;

    int viewportLeft_ = 0;
    int viewportTop_ = 0;
    int viewportRight_ = 0;
    int viewportBottom_ = 0;
    bool viewportClip_ = false;
    std::vector<std::array<double, 6> > transforms_;
    int currentX_ = 0;
    int currentY_ = 0;
    FontState font_;
    TextHAlign horizontalAlign_ = TEXT_LEFT;
    TextVAlign verticalAlign_ = TEXT_TOP;
};

} // namespace backend
} // namespace ege

#endif
