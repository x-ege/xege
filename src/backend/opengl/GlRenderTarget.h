// src/backend/opengl/GlRenderTarget.h
// OpenGL 3.3 Core RenderTarget implementation
#pragma once
#include "../interface/RenderTarget.h"
#include "GlShader.h"
#include "GlFontCache.h"
#include <vector>

namespace ege {

// Vertex format for primitive rendering
struct GlVertex {
    float x, y;
    float r, g, b, a;
    float pattern;
    float br, bg, bb, ba;

    GlVertex()
        : x(0), y(0), r(0), g(0), b(0), a(0), pattern(1),
          br(0), bg(0), bb(0), ba(0) {}
    GlVertex(float xValue, float yValue, float red, float green, float blue, float alpha,
             float patternValue = 1.0f,
             float backgroundRed = 0.0f, float backgroundGreen = 0.0f,
             float backgroundBlue = 0.0f, float backgroundAlpha = 0.0f)
        : x(xValue), y(yValue), r(red), g(green), b(blue), a(alpha),
          pattern(patternValue), br(backgroundRed), bg(backgroundGreen),
          bb(backgroundBlue), ba(backgroundAlpha) {}
};

class GlRenderTarget : public RenderTarget {
public:
    GlRenderTarget();
    ~GlRenderTarget() override;

    // Initialize as on-screen (default framebuffer) or off-screen (FBO)
    bool initOnScreen(int width, int height);
    bool initOffscreen(int width, int height);

    // --- RenderTarget interface ---
    int  getWidth()  const override { return m_width; }
    int  getHeight() const override { return m_height; }
    bool isOnScreen() const override { return m_isOnScreen; }

    // --- State ---
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

    color_t getLineColor()  const override { return m_lineColor; }
    color_t getFillColor()  const override { return m_fillColor; }
    color_t getTextColor()  const override { return m_textColor; }
    color_t getBkColor()    const override { return m_bkColor; }
    FillStyle getFillStyle() const override { return m_fillStyle; }

    // --- Viewport ---
    void setViewport(int left, int top, int right, int bottom, bool clip) override;
    void getViewport(int* left, int* top, int* right, int* bottom, int* clip) const override;
    void clearViewport() override;

    // --- Transforms ---
    void pushTransform() override;
    void popTransform() override;
    void resetTransform() override;
    void translate(float dx, float dy) override;
    void rotate(float angle) override;
    void scale(float sx, float sy) override;
    void setTransformMatrix(const float* mat3x3) override;

    // --- Drawing position ---
    void moveTo(int x, int y) override;
    void moveRel(int dx, int dy) override;
    int getCurrentX() const override;
    int getCurrentY() const override;

    // --- Primitives ---
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

    // --- Pixel ---
    void putPixel(int x, int y, color_t color) override;
    color_t getPixel(int x, int y) const override;
    void putPixelAlpha(int x, int y, color_t color) override;
    void putPixelSaveAlpha(int x, int y, color_t color) override;
    void putPixelAlphaBlend(int x, int y, color_t color, unsigned char alphaFactor) override;
    void putPixels(int count, const int* points) override;

    // --- Flood fill ---
    void floodFill(int x, int y, color_t borderColor) override;
    void floodFillSurface(int x, int y, color_t surfaceColor) override;

    // --- Clear ---
    void clear(color_t color) override;

    // --- Image transfer ---
    void blit(int dstX, int dstY, RenderTarget* src, int srcX, int srcY, int w, int h) override;
    void blitStretch(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH) override;
    void alphaBlend(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                     unsigned char alpha, ImageAlphaFormat format, bool smooth) override;
    void alphaTransparent(int dstX, int dstY, RenderTarget* src,
                          int srcX, int srcY, int w, int h,
                          color_t transparentColor, unsigned char alpha) override;
    void withAlpha(int dstX, int dstY, int dstW, int dstH,
                    RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                    bool smooth) override;
    void alphaFilter(int dstX, int dstY, int w, int h,
                     RenderTarget* src, int srcX, int srcY,
                     unsigned char alpha) override;
    void rotateBlend(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                     float angle, float centerX, float centerY,
                     bool transparent, int alpha, bool smooth) override;
    void rotateZoomBlend(int dstX, int dstY, int dstW, int dstH,
                         RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                         float angle, float centerX, float centerY,
                         float zoomX, float zoomY,
                         bool transparent, int alpha, bool smooth) override;
    void blitAffine(RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                    const float* destinationPoints, bool premultipliedAlpha,
                    bool smooth) override;
    void filterBlur(int dstX, int dstY, int w, int h, float intensity) override;

    // --- Text ---
    void setFont(int height, int width, const char* face,
                 int escapement, int orientation, int weight,
                 bool italic, bool underline, bool strikeout) override;
    void getFont(int* height, int* width, char* face, int faceCapacity,
                 int* escapement, int* orientation, int* weight,
                 bool* italic, bool* underline, bool* strikeout) const override;
    void setTextJustify(TextHAlign h, TextVAlign v) override;
    void drawText(float x, float y, const char* text) override;
    void drawText(float x, float y, const wchar_t* text) override;
    int  getTextWidth(const char* text) const override;
    int  getTextWidth(const wchar_t* text) const override;
    int  getTextHeight(const char* text) const override;
    int  getTextHeight(const wchar_t* text) const override;
    void measureText(const char* text, float* width, float* height) const override;
    void measureText(const wchar_t* text, float* width, float* height) const override;

    // --- Pixel buffer ---
    color_t* getPixelBuffer() override;
    const color_t* getPixelBuffer() const override;

    // --- Submit ---
    void flush() override;
    void present() override;

    // Internal: sync CPU buffer to GPU texture (used by image blit)
    void syncToGpu();

    // Internal: capture screen framebuffer to texture before swap
    void captureScreenToTexture();

    // Internal: rebuild GPU resources after resize (called from IMAGE::resize_f)
    void rebuild(int width, int height);

private:
    enum class PixelSyncState {
        Synchronized,
        CpuNewer,
        GpuNewer,
        ScreenTextureNewer
    };

    void appendFillTriangle(float x0, float y0, float x1, float y1,
                            float x2, float y2, float r, float g, float b, float a);
    void appendFillQuad(float x0, float y0, float x1, float y1,
                        float x2, float y2, float x3, float y3,
                        float r, float g, float b, float a);
    void drawPolylineInternal(const int* points, int count, bool closed);
    void initShaders();
    void initVBO();
    void ensureProjection();
    void bindForDrawing();
    void submitBatch();
    void downloadFromGpu();

    // Image blit helpers
    void ensureImageShader();
    void drawImageQuad(GLuint srcTex, int srcW, int srcH,
                       int srcX, int srcY, int srcW2, int srcH2,
                       int dstX, int dstY, int dstW2, int dstH2,
                       float angle, float centerX, float centerY,
                       float zoomX, float zoomY,
                       int mode, color_t keyColor, bool smooth = false);
    void drawImageQuadInternal(GLuint srcTex, int srcW, int srcH,
                               int srcX, int srcY, int srcW2, int srcH2,
                               float dstX, float dstY, float dstW2, float dstH2,
                               float angle, float centerX, float centerY,
                               float zoomX, float zoomY,
                               int mode, color_t keyColor,
                               float alphaOverride, bool smooth = false,
                               const float* destinationPoints = nullptr);

    // Text rendering helpers
    void ensureTextShader();
    void drawGlyphTexture(GLuint tex, int texW, int texH,
                          int srcX, int srcY, int srcW, int srcH,
                          float dstX, float dstY, float dstW, float dstH,
                          float angle, float r, float g, float b, float a);
    void renderText(float x, float y, const wchar_t* text);
    void renderCodepoints(float x, float y, const std::vector<uint32_t>& codepoints);
    void measureCodepoints(const std::vector<uint32_t>& codepoints,
                           float* width, float* height) const;

    // GPU resources
    GLuint  m_texture;       // GL_TEXTURE_2D for this RT
    GLuint  m_fbo;           // Framebuffer (0 for on-screen)
    GLuint  m_vao;
    GLuint  m_vbo;

    GlShader m_primShader;   // Primitive shader (lines, filled shapes)
    GlShader m_imageShader;  // Image blit/blend shader
    bool     m_imageShaderReady;
    GlShader m_textShader;   // Text rendering shader
    bool     m_textShaderReady;

    // CPU pixel buffer
    color_t* m_cpuBuffer;
    mutable PixelSyncState m_pixelSyncState;

    // Dimensions
    int      m_width;
    int      m_height;
    bool     m_isOnScreen;
    bool     m_initialized;

    // State
    color_t  m_lineColor;
    color_t  m_fillColor;
    color_t  m_textColor;
    color_t  m_bkColor;
    bool     m_bkOpaque;
    float    m_lineWidth;
    LineStyle    m_lineStyle;
    unsigned short m_linePattern;
    int      m_lineThickness;
    RTLineCap  m_lineStartCap;
    RTLineCap  m_lineEndCap;
    RTLineJoin m_lineJoin;
    float    m_miterLimit;
    FillStyle m_fillStyle;
    color_t  m_fillPatternColor;
    RasterOp m_rasterOp;
    int      m_writingMode;

    // Viewport
    int m_vpLeft, m_vpTop, m_vpRight, m_vpBottom;
    bool m_vpClip;

    // Transform matrix stack (3x3 column-major, 9 floats)
    std::vector<std::vector<float>> m_transformStack;

    // Drawing position
    int m_curX, m_curY;

    // Batch vertex buffer
    std::vector<GlVertex> m_vertices;
    static constexpr int MAX_BATCH = 65536;

    // Projection dirty flag
    bool m_projectionDirty;

    // Font state
    FontConfig m_fontConfig;
    TextHAlign m_hAlign;
    TextVAlign m_vAlign;
    GlyphAtlas m_glyphAtlas;
};

} // namespace ege
