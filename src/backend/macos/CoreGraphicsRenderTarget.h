#ifndef EGE_BACKEND_MACOS_CORE_GRAPHICS_RENDER_TARGET_H
#define EGE_BACKEND_MACOS_CORE_GRAPHICS_RENDER_TARGET_H

#include "backend/interface/RenderTarget.h"
#include "backend/macos/CoreGraphicsSurface.h"
#include "backend/macos/CoreTextRenderer.h"

#include <CoreGraphics/CoreGraphics.h>

#include <memory>
#include <vector>

namespace ege
{
namespace backend
{

class CoreGraphicsRenderTarget : public RenderTarget
{
public:
    explicit CoreGraphicsRenderTarget(int width, int height, bool onScreen = false);
    ~CoreGraphicsRenderTarget() override;

    CoreGraphicsRenderTarget(const CoreGraphicsRenderTarget&)            = delete;
    CoreGraphicsRenderTarget& operator=(const CoreGraphicsRenderTarget&) = delete;

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
    void setTransformMatrix(const float* mat3x3) override;

    void moveTo(int x, int y) override;
    void moveRel(int dx, int dy) override;
    int  getCurrentX() const override;
    int  getCurrentY() const override;

    void drawLine(int x1, int y1, int x2, int y2) override;
    void drawLineF(float x1, float y1, float x2, float y2) override;
    void lineTo(int x, int y) override;
    void lineRel(int dx, int dy) override;
    void drawRect(int x, int y, int w, int h) override;
    void fillRect(int x, int y, int w, int h) override;
    void drawRoundRect(int x, int y, int w, int h, int ew, int eh) override;
    void fillRoundRect(int x, int y, int w, int h, int ew, int eh) override;
    void draw3DBar(int x, int y, int w, int h, int depth, int fillStyle) override;
    void drawCircle(int x, int y, int r) override;
    void fillCircle(int x, int y, int r) override;
    void drawEllipse(int x, int y, int sa, int ea, int rx, int ry) override;
    void fillEllipse(int x, int y, int sa, int ea, int rx, int ry) override;
    void drawSector(int x, int y, int sa, int ea, int rx, int ry) override;
    void fillSector(int x, int y, int sa, int ea, int rx, int ry) override;
    void drawPie(int x, int y, int sa, int ea, int rx, int ry) override;
    void fillPie(int x, int y, int sa, int ea, int rx, int ry) override;
    void drawArc(int x, int y, int sa, int ea, int rx, int ry) override;
    void drawChord(int x, int y, int sa, int ea, int rx, int ry) override;
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

    void blit(int dstX, int dstY, RenderTarget* src, int srcX, int srcY, int w, int h) override;
    void blitStretch(
        int dstX, int dstY, int dstW, int dstH, RenderTarget* src, int srcX, int srcY, int srcW, int srcH) override;
    void alphaBlend(int dstX, int dstY, int dstW, int dstH, RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
        unsigned char alpha, ImageAlphaFormat format, bool smooth) override;
    void alphaTransparent(int dstX, int dstY, RenderTarget* src, int srcX, int srcY, int w, int h,
        color_t transparentColor, unsigned char alpha) override;
    void withAlpha(int dstX, int dstY, int dstW, int dstH, RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
        bool smooth) override;
    void alphaFilter(
        int dstX, int dstY, int w, int h, RenderTarget* src, int srcX, int srcY, unsigned char alpha) override;
    void rotateBlend(int dstX, int dstY, int dstW, int dstH, RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
        float angle, float centerX, float centerY, bool transparent, int alpha, bool smooth) override;
    void rotateZoomBlend(int dstX, int dstY, int dstW, int dstH, RenderTarget* src, int srcX, int srcY, int srcW,
        int srcH, float angle, float centerX, float centerY, float zoomX, float zoomY, bool transparent, int alpha,
        bool smooth) override;
    void blitAffine(RenderTarget* src, int srcX, int srcY, int srcW, int srcH, const float* destinationPoints,
        bool premultipliedAlpha, bool smooth) override;
    void filterBlur(int dstX, int dstY, int w, int h, float intensity) override;

    void setFont(int height, int width, const char* face, int escapement, int orientation, int weight, bool italic,
        bool underline, bool strikeout) override;
    void getFont(int* height, int* width, char* face, int faceCapacity, int* escapement, int* orientation, int* weight,
        bool* italic, bool* underline, bool* strikeout) const override;
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
        int                  width;
        int                  height;
        std::vector<color_t> pixels;
    };

    struct RasterBounds
    {
        int left;
        int top;
        int right;
        int bottom;
    };

    void      beginPrimitive(CGRect logicalBounds, float padding);
    void      endPrimitive();
    void      configureStroke();
    RasterBounds primitiveRasterBounds(CGRect logicalBounds, float padding) const;
    float       primitiveStrokePadding(bool includeMiter) const noexcept;
    color_t     primitiveColor(color_t color) const noexcept;
    CGPathRef createArcPath(
        int x, int y, int startAngle, int endAngle, int width, int height, bool center, bool close) const;
    CGPoint  physicalPoint(float x, float y) const;
    bool     physicalPixel(int x, int y, int* physicalX, int* physicalY) const;
    bool     insideClip(int x, int y) const;
    color_t& pixelAt(int x, int y);
    color_t  pixelAt(int x, int y) const;
    void     writePixel(int x, int y, color_t color, bool useRasterOp = true);
    void     floodFillInternal(int x, int y, color_t boundary, bool surfaceMode);

    static color_t     applyRasterOp(color_t destination, color_t source, RasterOp operation) noexcept;
    static color_t     applyPrimitiveRasterOp(
        color_t destination, color_t source, RasterOp operation) noexcept;
    static color_t     premultiply(color_t straight) noexcept;
    static color_t     blendPremultiplied(color_t destination, color_t source, unsigned char factor = 255) noexcept;
    static color_t     blendStraight(color_t destination, color_t source, unsigned char factor = 255) noexcept;
    static SourceImage captureSource(RenderTarget* source, int x, int y, int width, int height);
    static color_t     sample(const SourceImage& source, float x, float y, bool smooth) noexcept;
    void stretchTransfer(int dstX, int dstY, int dstW, int dstH, const SourceImage& source, ImageAlphaFormat format,
        unsigned char alpha, bool smooth, bool blend);

    std::unique_ptr<PixelSurface>        surface_;
    std::unique_ptr<CoreGraphicsSurface> graphics_;
    std::unique_ptr<PixelSurface>        rasterScratchSurface_;
    std::unique_ptr<CoreGraphicsSurface> rasterScratchGraphics_;
    std::unique_ptr<CoreGraphicsSurface> rasterDestinationGraphics_;
    CoreTextRenderer                     textRenderer_;
    bool                                 onScreen_;
    bool                                 antialiasing_;
    RasterBounds                         rasterDirtyBounds_;

    color_t        lineColor_;
    color_t        fillColor_;
    color_t        textColor_;
    color_t        backgroundColor_;
    bool           backgroundOpaque_;
    float          lineWidth_;
    LineStyle      lineStyle_;
    unsigned short linePattern_;
    int            lineThickness_;
    RTLineCap      startCap_;
    RTLineCap      endCap_;
    RTLineJoin     lineJoin_;
    float          miterLimit_;
    FillStyle      fillStyle_;
    color_t        fillPatternColor_;
    RasterOp       rasterOp_;
    int            writingMode_;

    int  viewportLeft_;
    int  viewportTop_;
    int  viewportRight_;
    int  viewportBottom_;
    bool viewportClip_;

    std::vector<CGAffineTransform> transforms_;
    int                            currentX_;
    int                            currentY_;
    TextHAlign                     horizontalAlign_;
    TextVAlign                     verticalAlign_;
};

} // namespace backend
} // namespace ege

#endif // EGE_BACKEND_MACOS_CORE_GRAPHICS_RENDER_TARGET_H
