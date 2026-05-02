// src/backend/interface/RenderTarget.h
// Abstract render target interface — represents a drawable canvas (screen or offscreen IMAGE)
#pragma once
#include <stdint.h>
#include <vector>

namespace ege {

typedef uint32_t color_t;

enum RasterOp {
    ROP_COPY     = 13,  // SRCCOPY
    ROP_XOR      = 7,   // R2_XORPEN
    ROP_AND      = 1,   // R2_MASKPEN
    ROP_OR       = 15,  // R2_MERGEPEN
    ROP_NOP      = 11,  // R2_NOP
    ROP_NOTSRC   = 6,   // R2_NOTSRCPEN
};

enum LineStyle {
    LINE_SOLID,
    LINE_DASHED,
    LINE_DOTTED,
    LINE_DASHDOT,
    LINE_DASHDOTDOT,
    LINE_USER,
};

// Line cap types (prefixed to avoid conflict with ege.h enums)
enum RTLineCap {
    RT_LINECAP_FLAT,
    RT_LINECAP_ROUND,
    RT_LINECAP_SQUARE,
};

enum RTLineJoin {
    RT_LINEJOIN_MITER,
    RT_LINEJOIN_ROUND,
    RT_LINEJOIN_BEVEL,
};

enum FillStyle {
    FILL_SOLID,
    FILL_EMPTY,
    FILL_HATCHED,
};

enum TextHAlign {
    TEXT_LEFT,
    TEXT_CENTER,
    TEXT_RIGHT,
};

enum TextVAlign {
    TEXT_TOP,
    TEXT_MIDDLE,
    TEXT_BOTTOM,
};

enum AlphaMode {
    ALPHA_NONE,
    ALPHA_BLEND,
    ALPHA_TRANSPARENT,
    ALPHA_PREMULTIPLIED,
    ALPHA_FILTER,
};

// ============================================================
// RenderTarget abstract interface
// ============================================================
class RenderTarget {
public:
    virtual ~RenderTarget() {}

    // --- Basic info ---
    virtual int  getWidth()  const = 0;
    virtual int  getHeight() const = 0;
    virtual bool isOnScreen() const = 0;

    // --- State management ---
    virtual void setLineColor(color_t color) = 0;
    virtual void setFillColor(color_t color) = 0;
    virtual void setTextColor(color_t color) = 0;
    virtual void setBkColor(color_t color) = 0;
    virtual void setBkMode(bool opaque) = 0;
    virtual void setLineWidth(float width) = 0;
    virtual void setLineStyle(LineStyle style, unsigned short pattern, int thickness) = 0;
    virtual void setLineCap(RTLineCap startCap, RTLineCap endCap) = 0;
    virtual void setLineJoin(RTLineJoin join, float miterLimit) = 0;
    virtual void setFillStyle(FillStyle style, color_t color) = 0;
    virtual void setRasterOp(RasterOp rop) = 0;
    virtual void setWritingMode(int mode) = 0;

    // --- Color queries ---
    virtual color_t getLineColor()  const = 0;
    virtual color_t getFillColor()  const = 0;
    virtual color_t getTextColor()  const = 0;
    virtual color_t getBkColor()    const = 0;

    // --- Viewport & clipping ---
    virtual void setViewport(int left, int top, int right, int bottom, bool clip) = 0;
    virtual void getViewport(int* left, int* top, int* right, int* bottom, int* clip) const = 0;
    virtual void clearViewport() = 0;

    // --- Coordinate transforms ---
    virtual void pushTransform() = 0;
    virtual void popTransform()  = 0;
    virtual void resetTransform() = 0;
    virtual void translate(float dx, float dy) = 0;
    virtual void rotate(float angle) = 0;
    virtual void scale(float sx, float sy) = 0;
    virtual void setTransformMatrix(const float* mat3x3) = 0;

    // --- Drawing position state (moveto/lineto) ---
    virtual void moveTo(int x, int y) = 0;
    virtual void moveRel(int dx, int dy) = 0;
    virtual int getCurrentX() const = 0;
    virtual int getCurrentY() const = 0;

    // --- Basic primitives ---
    virtual void drawLine(int x1, int y1, int x2, int y2) = 0;
    virtual void drawLineF(float x1, float y1, float x2, float y2) = 0;
    virtual void lineTo(int x, int y) = 0;
    virtual void lineRel(int dx, int dy) = 0;
    virtual void drawRect(int x, int y, int w, int h) = 0;
    virtual void fillRect(int x, int y, int w, int h) = 0;
    virtual void drawRoundRect(int x, int y, int w, int h, int ew, int eh) = 0;
    virtual void fillRoundRect(int x, int y, int w, int h, int ew, int eh) = 0;
    virtual void draw3DBar(int x, int y, int w, int h, int depth, int fillStyle) = 0;

    virtual void drawCircle(int x, int y, int r) = 0;
    virtual void fillCircle(int x, int y, int r) = 0;
    virtual void drawEllipse(int x, int y, int sa, int ea, int rx, int ry) = 0;
    virtual void fillEllipse(int x, int y, int sa, int ea, int rx, int ry) = 0;
    virtual void drawSector(int x, int y, int sa, int ea, int rx, int ry) = 0;
    virtual void fillSector(int x, int y, int sa, int ea, int rx, int ry) = 0;
    virtual void drawPie(int x, int y, int sa, int ea, int rx, int ry) = 0;
    virtual void fillPie(int x, int y, int sa, int ea, int rx, int ry) = 0;
    virtual void drawArc(int x, int y, int sa, int ea, int rx, int ry) = 0;
    virtual void drawChord(int x, int y, int sa, int ea, int rx, int ry) = 0;

    virtual void drawPolygon(const int* x, const int* y, int count) = 0;
    virtual void fillPolygon(const int* x, const int* y, int count) = 0;
    virtual void drawPolyline(const int* x, const int* y, int count) = 0;

    // --- Pixel operations ---
    virtual void putPixel(int x, int y, color_t color) = 0;
    virtual color_t getPixel(int x, int y) const = 0;
    virtual void putPixelAlpha(int x, int y, color_t color) = 0;
    virtual void putPixelSaveAlpha(int x, int y, color_t color) = 0;
    virtual void putPixelAlphaBlend(int x, int y, color_t color, unsigned char alphaFactor) = 0;
    virtual void putPixels(int count, const int* points) = 0;

    // --- Flood fill ---
    virtual void floodFill(int x, int y, color_t borderColor) = 0;

    // --- Clear ---
    virtual void clear(color_t color) = 0;

    // --- Image transfer (RT ↔ RT) ---
    virtual void blit(int dstX, int dstY, RenderTarget* src, int srcX, int srcY,
                       int w, int h) = 0;
    virtual void blitStretch(int dstX, int dstY, int dstW, int dstH,
                              RenderTarget* src, int srcX, int srcY, int srcW, int srcH) = 0;
    virtual void alphaBlend(int dstX, int dstY, int dstW, int dstH,
                             RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                             unsigned char alpha) = 0;
    virtual void alphaTransparent(int dstX, int dstY, RenderTarget* src,
                                   int srcX, int srcY, int w, int h,
                                   color_t transparentColor) = 0;
    virtual void withAlpha(int dstX, int dstY, int dstW, int dstH,
                            RenderTarget* src, int srcX, int srcY, int srcW, int srcH) = 0;
    virtual void alphaFilter(int dstX, int dstY, int w, int h,
                              RenderTarget* src, int srcX, int srcY,
                              unsigned char alpha) = 0;
    virtual void rotateBlend(int dstX, int dstY, int dstW, int dstH,
                              RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                              float angle, float centerX, float centerY) = 0;
    virtual void rotateZoomBlend(int dstX, int dstY, int dstW, int dstH,
                                  RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                                  float angle, float centerX, float centerY,
                                  float zoomX, float zoomY) = 0;
    virtual void filterBlur(int dstX, int dstY, int w, int h, float intensity) = 0;

    // --- Text rendering ---
    virtual void setFont(int height, int width, const char* face,
                          int escapement, int orientation, int weight,
                          bool italic, bool underline, bool strikeout) = 0;
    virtual void setTextJustify(TextHAlign h, TextVAlign v) = 0;
    virtual void drawText(float x, float y, const char* text) = 0;
    virtual void drawText(float x, float y, const wchar_t* text) = 0;
    virtual int  getTextWidth(const char* text) const = 0;
    virtual int  getTextWidth(const wchar_t* text) const = 0;
    virtual int  getTextHeight(const char* text) const = 0;
    virtual int  getTextHeight(const wchar_t* text) const = 0;
    virtual void measureText(const char* text, float* width, float* height) const = 0;
    virtual void measureText(const wchar_t* text, float* width, float* height) const = 0;

    // --- Pixel buffer access ---
    virtual color_t* getPixelBuffer() = 0;
    virtual const color_t* getPixelBuffer() const = 0;

    // --- Submit ---
    virtual void flush() = 0;
    virtual void present() = 0;
};

} // namespace ege
