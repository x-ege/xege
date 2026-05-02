// src/backend/opengl/GlRenderTarget.cpp
#include "GlRenderTarget.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ege {

// ============================================================
// Shader source code
// ============================================================
static const char* g_primVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uProj;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

static const char* g_primFragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)";

// ============================================================
// Helpers
// ============================================================
static void color_t_to_rgba(color_t c, float& r, float& g, float& b, float& a) {
    r = ((c >> 16) & 0xFF) / 255.0f;
    g = ((c >> 8)  & 0xFF) / 255.0f;
    b = (c & 0xFF)         / 255.0f;
    a = ((c >> 24) & 0xFF) / 255.0f;
}

static void ortho2D(float* out, float left, float right, float bottom, float top) {
    // Column-major 4x4 orthographic projection
    memset(out, 0, sizeof(float) * 16);
    out[0]  = 2.0f / (right - left);
    out[5]  = 2.0f / (top - bottom);
    out[10] = -1.0f;
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[15] = 1.0f;
}

static float identity3[9] = {
    1,0,0,
    0,1,0,
    0,0,1
};

// Transform a point by 3x3 matrix (column-major)
static void transformPoint(const float* mat, float& x, float& y) {
    float nx = mat[0]*x + mat[3]*y + mat[6];
    float ny = mat[1]*x + mat[4]*y + mat[7];
    x = nx; y = ny;
}

// Multiply two 3x3 matrices: result = a * b
static void mat3Mul(float* result, const float* a, const float* b) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            result[i*3+j] = 0;
            for (int k = 0; k < 3; k++)
                result[i*3+j] += a[i*3+k] * b[k*3+j];
        }
}

static void mat3Translate(float* out, float dx, float dy) {
    memcpy(out, identity3, sizeof(float)*9);
    out[6] = dx; out[7] = dy;
}

static void mat3Rotate(float* out, float angle) {
    float c = cosf(angle), s = sinf(angle);
    memset(out, 0, sizeof(float)*9);
    out[0] = c;  out[1] = s;  out[3] = -s; out[4] = c;
    out[8] = 1;
}

static void mat3Scale(float* out, float sx, float sy) {
    memset(out, 0, sizeof(float)*9);
    out[0] = sx; out[4] = sy; out[8] = 1;
}

// ============================================================
// Constructor / Destructor
// ============================================================
GlRenderTarget::GlRenderTarget()
    : m_texture(0), m_fbo(0), m_vao(0), m_vbo(0),
      m_cpuBuffer(nullptr), m_gpuDirty(false),
      m_width(0), m_height(0), m_isOnScreen(false), m_initialized(false),
      m_lineColor(0xFFFFFFFF), m_fillColor(0xFFFFFFFF),
      m_textColor(0xFFFFFFFF), m_bkColor(0x00000000),
      m_bkOpaque(false), m_lineWidth(1.0f),
      m_lineStyle(LINE_SOLID), m_linePattern(0), m_lineThickness(1),
      m_lineStartCap(RT_LINECAP_FLAT), m_lineEndCap(RT_LINECAP_FLAT),
      m_lineJoin(RT_LINEJOIN_MITER), m_miterLimit(10.0f),
      m_fillStyle(FILL_SOLID), m_fillPatternColor(0xFFFFFFFF),
      m_rasterOp(ROP_COPY), m_writingMode(0),
      m_vpLeft(0), m_vpTop(0), m_vpRight(0), m_vpBottom(0), m_vpClip(false),
      m_curX(0), m_curY(0),
      m_projectionDirty(true) {
    m_transformStack.push_back(std::vector<float>(identity3, identity3 + 9));
}

GlRenderTarget::~GlRenderTarget() {
    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_fbo)     glDeleteFramebuffers(1, &m_fbo);
    if (m_vao)     glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)     glDeleteBuffers(1, &m_vbo);
    if (m_cpuBuffer) delete[] m_cpuBuffer;
}

bool GlRenderTarget::initOnScreen(int width, int height) {
    m_width = width; m_height = height;
    m_isOnScreen = true;
    m_fbo = 0;
    m_initialized = true;
    m_vpRight = width; m_vpBottom = height;

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // CPU buffer
    m_cpuBuffer = new color_t[width * height];
    memset(m_cpuBuffer, 0, sizeof(color_t) * width * height);
    m_gpuDirty = true;

    initShaders();
    initVBO();
    return true;
}

bool GlRenderTarget::initOffscreen(int width, int height) {
    m_width = width; m_height = height;
    m_isOnScreen = false;
    m_initialized = true;
    m_vpRight = width; m_vpBottom = height;

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "GlRenderTarget: FBO incomplete\n");
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_cpuBuffer = new color_t[width * height];
    memset(m_cpuBuffer, 0, sizeof(color_t) * width * height);
    m_gpuDirty = true;

    initShaders();
    initVBO();
    return true;
}

void GlRenderTarget::initShaders() {
    m_primShader.compileVertex(g_primVertSrc);
    m_primShader.compileFragment(g_primFragSrc);
    m_primShader.link();
}

void GlRenderTarget::initVBO() {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GlVertex) * MAX_BATCH, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (void*)offsetof(GlVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (void*)offsetof(GlVertex, r));
    glBindVertexArray(0);
}

// ============================================================
// State
// ============================================================
void GlRenderTarget::setLineColor(color_t color) { m_lineColor = color; }
void GlRenderTarget::setFillColor(color_t color) { m_fillColor = color; }
void GlRenderTarget::setTextColor(color_t color) { m_textColor = color; }
void GlRenderTarget::setBkColor(color_t color)   { m_bkColor = color; }
void GlRenderTarget::setBkMode(bool opaque)      { m_bkOpaque = opaque; }
void GlRenderTarget::setLineWidth(float width)   { m_lineWidth = width; glLineWidth(width); }
void GlRenderTarget::setLineStyle(LineStyle style, unsigned short pattern, int thickness) {
    m_lineStyle = style; m_linePattern = pattern; m_lineThickness = thickness;
}
void GlRenderTarget::setLineCap(RTLineCap startCap, RTLineCap endCap) {
    m_lineStartCap = startCap; m_lineEndCap = endCap;
}
void GlRenderTarget::setLineJoin(RTLineJoin join, float miterLimit) {
    m_lineJoin = join; m_miterLimit = miterLimit;
}
void GlRenderTarget::setFillStyle(FillStyle style, color_t color) {
    m_fillStyle = style; m_fillPatternColor = color;
}
void GlRenderTarget::setRasterOp(RasterOp rop) { m_rasterOp = rop; }
void GlRenderTarget::setWritingMode(int mode)  { m_writingMode = mode; }

// ============================================================
// Viewport
// ============================================================
void GlRenderTarget::setViewport(int left, int top, int right, int bottom, bool clip) {
    m_vpLeft = left; m_vpTop = top; m_vpRight = right; m_vpBottom = bottom; m_vpClip = clip;
    m_projectionDirty = true;
}

void GlRenderTarget::getViewport(int* left, int* top, int* right, int* bottom, int* clip) const {
    if (left)  *left  = m_vpLeft;
    if (top)   *top   = m_vpTop;
    if (right) *right = m_vpRight;
    if (bottom)*bottom= m_vpBottom;
    if (clip)  *clip  = m_vpClip;
}

void GlRenderTarget::clearViewport() {
    clear(m_bkColor);
}

// ============================================================
// Transforms
// ============================================================
void GlRenderTarget::pushTransform() {
    m_transformStack.push_back(m_transformStack.back());
}
void GlRenderTarget::popTransform() {
    if (m_transformStack.size() > 1) m_transformStack.pop_back();
}
void GlRenderTarget::resetTransform() {
    m_transformStack.back().assign(identity3, identity3 + 9);
    m_projectionDirty = true;
}

void GlRenderTarget::translate(float dx, float dy) {
    float t[9]; mat3Translate(t, dx, dy);
    float& cur = m_transformStack.back()[0];
    float r[9]; mat3Mul(r, &cur, t);
    m_transformStack.back().assign(r, r + 9);
    m_projectionDirty = true;
}

void GlRenderTarget::rotate(float angle) {
    float t[9]; mat3Rotate(t, angle);
    float& cur = m_transformStack.back()[0];
    float r[9]; mat3Mul(r, &cur, t);
    m_transformStack.back().assign(r, r + 9);
    m_projectionDirty = true;
}

void GlRenderTarget::scale(float sx, float sy) {
    float t[9]; mat3Scale(t, sx, sy);
    float& cur = m_transformStack.back()[0];
    float r[9]; mat3Mul(r, &cur, t);
    m_transformStack.back().assign(r, r + 9);
    m_projectionDirty = true;
}

void GlRenderTarget::setTransformMatrix(const float* mat3x3) {
    m_transformStack.back().assign(mat3x3, mat3x3 + 9);
    m_projectionDirty = true;
}

// ============================================================
// Drawing position
// ============================================================
void GlRenderTarget::moveTo(int x, int y) { m_curX = x; m_curY = y; }
void GlRenderTarget::moveRel(int dx, int dy) { m_curX += dx; m_curY += dy; }
int GlRenderTarget::getCurrentX() const { return m_curX; }
int GlRenderTarget::getCurrentY() const { return m_curY; }

// ============================================================
// Projection / binding
// ============================================================
void GlRenderTarget::ensureProjection() {
    if (!m_projectionDirty) return;
    float proj[16];
    // Apply viewport offset to projection
    float left   = (float)m_vpLeft;
    float right  = (float)m_vpRight;
    float bottom = (float)m_vpBottom;  // top-left origin
    float top    = (float)m_vpTop;
    ortho2D(proj, left, right, bottom, top);
    m_primShader.use();
    m_primShader.setUniformMatrix4("uProj", proj);
    m_projectionDirty = false;
}

void GlRenderTarget::bindForDrawing() {
    if (m_isOnScreen) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (m_vpClip) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(m_vpLeft, m_height - m_vpBottom, m_vpRight - m_vpLeft, m_vpBottom - m_vpTop);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void GlRenderTarget::submitBatch() {
    if (m_vertices.empty()) return;
    bindForDrawing();
    ensureProjection();

    glLineWidth(m_lineWidth);

    m_primShader.use();
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GlVertex) * m_vertices.size(), m_vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertices.size());
    glBindVertexArray(0);

    m_vertices.clear();
}

// ============================================================
// Primitive batching helpers
// ============================================================
static void addTri(std::vector<GlVertex>& v, float x0, float y0, float x1, float y1, float x2, float y2,
                   float r, float g, float b, float a) {
    v.push_back({x0, y0, r, g, b, a});
    v.push_back({x1, y1, r, g, b, a});
    v.push_back({x2, y2, r, g, b, a});
}

static void addQuad(std::vector<GlVertex>& v, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                    float r, float g, float b, float a) {
    addTri(v, x0, y0, x1, y1, x2, y2, r, g, b, a);
    addTri(v, x0, y0, x2, y2, x3, y3, r, g, b, a);
}

// ============================================================
// Basic primitives — Phase 1 implementation
// ============================================================
void GlRenderTarget::drawLine(int x1, int y1, int x2, int y2) {
    float cx = (x1 + x2) * 0.5f, cy = (y1 + y2) * 0.5f;
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.5f) return;
    float hw = m_lineWidth * 0.5f;
    float nx = -dy / len * hw, ny = dx / len * hw;

    float r, g, b, a;
    color_t_to_rgba(m_lineColor, r, g, b, a);

    float& mat = m_transformStack.back()[0];
    float px[8] = { x1+nx, y1+ny, x1-nx, y1-ny, x2-nx, y2-ny, x2+nx, y2+ny };
    for (int i = 0; i < 4; i++)
        transformPoint(&mat, px[i*2], px[i*2+1]);

    addTri(m_vertices, px[0], px[1], px[2], px[3], px[4], px[5], r, g, b, a);
    addTri(m_vertices, px[0], px[1], px[4], px[5], px[6], px[7], r, g, b, a);
}

void GlRenderTarget::drawLineF(float x1, float y1, float x2, float y2) {
    drawLine((int)roundf(x1), (int)roundf(y1), (int)roundf(x2), (int)roundf(y2));
}

void GlRenderTarget::lineTo(int x, int y) {
    drawLine(m_curX, m_curY, x, y);
    m_curX = x; m_curY = y;
}

void GlRenderTarget::lineRel(int dx, int dy) {
    lineTo(m_curX + dx, m_curY + dy);
}

void GlRenderTarget::drawRect(int x, int y, int w, int h) {
    drawLine(x, y, x + w, y);
    drawLine(x + w, y, x + w, y + h);
    drawLine(x + w, y + h, x, y + h);
    drawLine(x, y + h, x, y);
}

void GlRenderTarget::fillRect(int x, int y, int w, int h) {
    float r, g, b, a;
    color_t_to_rgba(m_fillColor, r, g, b, a);

    float& mat = m_transformStack.back()[0];
    float pts[4][2] = { {(float)x, (float)y}, {(float)(x+w), (float)y},
                        {(float)(x+w), (float)(y+h)}, {(float)x, (float)(y+h)} };
    for (int i = 0; i < 4; i++)
        transformPoint(&mat, pts[i][0], pts[i][1]);

    addQuad(m_vertices, pts[0][0], pts[0][1], pts[1][0], pts[1][1],
            pts[2][0], pts[2][1], pts[3][0], pts[3][1], r, g, b, a);
}

void GlRenderTarget::drawCircle(int x, int y, int r) {
    drawEllipse(x - r, y - r, 0, 360, 2 * r, 2 * r);
}

void GlRenderTarget::fillCircle(int x, int y, int r) {
    fillEllipse(x - r, y - r, 0, 360, 2 * r, 2 * r);
}

void GlRenderTarget::drawEllipse(int x, int y, int sa, int ea, int rx, int ry) {
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    int segments = std::max(16, (int)(rx + ry) / 4);

    float r, g, b, a;
    color_t_to_rgba(m_lineColor, r, g, b, a);

    float& mat = m_transformStack.back()[0];
    float prevX = 0, prevY = 0;

    for (int i = 0; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy + rdy * sinf(t);
        transformPoint(&mat, px, py);
        if (i > 0)
            addTri(m_vertices, prevX, prevY, px, py, cx, cy, r, g, b, a);
        prevX = px; prevY = py;
    }
    // Actually for draw (outline), we use line segments
    m_vertices.clear(); // undo the above — we used fill approach, redo as lines
    for (int i = 0; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy + rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) {
            float lx = prevX, ly = prevY;
            transformPoint(&mat, lx, ly); // WRONG — already transformed
        }
        prevX = px; prevY = py;
    }
    // Let me redo properly:
    m_vertices.clear();
    float prevTx = 0, prevTy = 0;
    for (int i = 0; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy + rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) {
            float dx = tx - prevTx, dy = ty - prevTy;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 0.1f) {
                float hw = m_lineWidth * 0.5f;
                float nx = -dy/len*hw, ny = dx/len*hw;
                addQuad(m_vertices, prevTx+nx, prevTy+ny, prevTx-nx, prevTy-ny,
                        tx-nx, ty-ny, tx+nx, ty+ny, r, g, b, a);
            }
        }
        prevTx = tx; prevTy = ty;
    }
}

void GlRenderTarget::fillEllipse(int x, int y, int sa, int ea, int rx, int ry) {
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    int segments = std::max(16, (int)(rx + ry) / 4);

    float r, g, b, a;
    color_t_to_rgba(m_fillColor, r, g, b, a);

    float& mat = m_transformStack.back()[0];

    // First triangle fan center
    float cpx = cx, cpy = cy;
    transformPoint(&mat, cpx, cpy);

    float prevTx = 0, prevTy = 0;
    for (int i = 0; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy + rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) {
            addTri(m_vertices, cpx, cpy, prevTx, prevTy, tx, ty, r, g, b, a);
        }
        prevTx = tx; prevTy = ty;
    }
}

void GlRenderTarget::drawSector(int x, int y, int sa, int ea, int rx, int ry) {
    // Sector = arc + two radial lines to center (outline only)
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    int segments = std::max(16, (int)(rx + ry) / 4);

    float r, g, b, a;
    color_t_to_rgba(m_lineColor, r, g, b, a);

    float& mat = m_transformStack.back()[0];

    // Draw arc
    float prevTx = 0, prevTy = 0;
    bool first = true;
    for (int i = 0; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy + rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) {
            float dx = tx - prevTx, dy = ty - prevTy;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 0.1f) {
                float hw = m_lineWidth * 0.5f;
                float nx = -dy/len*hw, ny = dx/len*hw;
                addQuad(m_vertices, prevTx+nx, prevTy+ny, prevTx-nx, prevTy-ny,
                        tx-nx, ty-ny, tx+nx, ty+ny, r, g, b, a);
            }
        }
        if (first) { prevTx = tx; prevTy = ty; first = false; }
        float startX = prevTx, startY = prevTy;
        prevTx = tx; prevTy = ty;
    }
    // Radial lines from center to arc endpoints
    float cpx = cx, cpy = cy;
    transformPoint(&mat, cpx, cpy);
    float t0 = startRad, px0 = cx + rdx * cosf(t0), py0 = cy + rdy * sinf(t0);
    float ex0 = px0, ey0 = py0; transformPoint(&mat, ex0, ey0);
    float t1 = endRad, px1 = cx + rdx * cosf(t1), py1 = cy + rdy * sinf(t1);
    float ex1 = px1, ey1 = py1; transformPoint(&mat, ex1, ey1);
    // Line center -> start
    { float dx=ex0-cpx, dy=ey0-cpy, len=sqrtf(dx*dx+dy*dy);
      if (len>0.1f) { float hw=m_lineWidth*0.5f, nx=-dy/len*hw, ny=dx/len*hw;
        addQuad(m_vertices, cpx+nx,cpy+ny,cpx-nx,cpy-ny,ex0-nx,ey0-ny,ex0+nx,ey0+ny,r,g,b,a); }}
    // Line center -> end
    { float dx=ex1-cpx, dy=ey1-cpy, len=sqrtf(dx*dx+dy*dy);
      if (len>0.1f) { float hw=m_lineWidth*0.5f, nx=-dy/len*hw, ny=dx/len*hw;
        addQuad(m_vertices, cpx+nx,cpy+ny,cpx-nx,cpy-ny,ex1-nx,ey1-ny,ex1+nx,ey1+ny,r,g,b,a); }}
}

void GlRenderTarget::fillSector(int x, int y, int sa, int ea, int rx, int ry) {
    // Fill sector = triangle fan from center to arc
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    int segments = std::max(16, (int)(rx + ry) / 4);
    float r, g, b, a;
    color_t_to_rgba(m_fillColor, r, g, b, a);
    float& mat = m_transformStack.back()[0];
    float cpx = cx, cpy = cy;
    transformPoint(&mat, cpx, cpy);
    float prevTx = 0, prevTy = 0;
    for (int i = 0; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy + rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) addTri(m_vertices, cpx, cpy, prevTx, prevTy, tx, ty, r, g, b, a);
        prevTx = tx; prevTy = ty;
    }
}

void GlRenderTarget::drawPie(int x, int y, int sa, int ea, int rx, int ry) {
    // Pie = sector outline (same as drawSector for outline)
    drawSector(x, y, sa, ea, rx, ry);
}

void GlRenderTarget::fillPie(int x, int y, int sa, int ea, int rx, int ry) {
    fillSector(x, y, sa, ea, rx, ry);
}

void GlRenderTarget::drawArc(int x, int y, int sa, int ea, int rx, int ry) {
    // Arc = just the curve (no radial lines)
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    int segments = std::max(16, (int)(rx + ry) / 4);
    float r, g, b, a;
    color_t_to_rgba(m_lineColor, r, g, b, a);
    float& mat = m_transformStack.back()[0];
    float prevTx = 0, prevTy = 0;
    for (int i = 0; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy + rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) {
            float dx = tx - prevTx, dy = ty - prevTy;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 0.1f) {
                float hw = m_lineWidth * 0.5f;
                float nx = -dy/len*hw, ny = dx/len*hw;
                addQuad(m_vertices, prevTx+nx, prevTy+ny, prevTx-nx, prevTy-ny,
                        tx-nx, ty-ny, tx+nx, ty+ny, r, g, b, a);
            }
        }
        prevTx = tx; prevTy = ty;
    }
}

void GlRenderTarget::drawChord(int x, int y, int sa, int ea, int rx, int ry) {
    // Chord = arc + closing line between endpoints
    drawArc(x, y, sa, ea, rx, ry);
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    float px0 = cx + rdx * cosf(startRad), py0 = cy + rdy * sinf(startRad);
    float px1 = cx + rdx * cosf(endRad), py1 = cy + rdy * sinf(endRad);
    drawLine((int)px0, (int)py0, (int)px1, (int)py1);
}

void GlRenderTarget::drawPolygon(const int* x, const int* y, int count) {
    for (int i = 0; i < count; i++) {
        int next = (i + 1) % count;
        drawLine(x[i], y[i], x[next], y[next]);
    }
}

void GlRenderTarget::fillPolygon(const int* x, const int* y, int count) {
    // Simple triangle fan from first vertex
    float r, g, b, a;
    color_t_to_rgba(m_fillColor, r, g, b, a);
    float& mat = m_transformStack.back()[0];
    float tx0 = (float)x[0], ty0 = (float)y[0];
    transformPoint(&mat, tx0, ty0);
    for (int i = 1; i < count - 1; i++) {
        float tx1 = (float)x[i], ty1 = (float)y[i];
        float tx2 = (float)x[i+1], ty2 = (float)y[i+1];
        transformPoint(&mat, tx1, ty1);
        transformPoint(&mat, tx2, ty2);
        addTri(m_vertices, tx0, ty0, tx1, ty1, tx2, ty2, r, g, b, a);
    }
}

void GlRenderTarget::drawPolyline(const int* x, const int* y, int count) {
    for (int i = 0; i < count - 1; i++)
        drawLine(x[i], y[i], x[i+1], y[i+1]);
}

// ============================================================
// Round rect — Phase 2 full implementation, stub for Phase 1
// ============================================================
void GlRenderTarget::drawRoundRect(int x, int y, int w, int h, int ew, int eh) {
    // Simplified: draw as rect for Phase 1
    drawRect(x, y, w, h);
}

void GlRenderTarget::fillRoundRect(int x, int y, int w, int h, int ew, int eh) {
    fillRect(x, y, w, h);
}

void GlRenderTarget::draw3DBar(int x, int y, int w, int h, int depth, int fillStyle) {
    // Simplified: draw as rect for Phase 1
    fillRect(x, y, w, h);
}

// ============================================================
// Pixel operations
// ============================================================
void GlRenderTarget::putPixel(int x, int y, color_t color) {
    float r, g, b, a;
    color_t_to_rgba(color, r, g, b, a);
    float hw = 0.5f;
    float& mat = m_transformStack.back()[0];
    float tx = (float)x, ty = (float)y;
    transformPoint(&mat, tx, ty);
    addQuad(m_vertices, tx-hw, ty-hw, tx+hw, ty-hw, tx+hw, ty+hw, tx-hw, ty+hw, r, g, b, a);
}

color_t GlRenderTarget::getPixel(int x, int y) const {
    // Sync GPU → CPU first if needed
    GlRenderTarget* self = const_cast<GlRenderTarget*>(this);
    if (m_gpuDirty && m_initialized) {
        if (m_isOnScreen) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        }
        glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_cpuBuffer);
        // Convert RGBA byte order to EGE color_t (ARGB)
        for (int i = 0; i < m_width * m_height; i++) {
            unsigned char* p = (unsigned char*)&m_cpuBuffer[i];
            unsigned char tmp = p[0]; p[0] = p[2]; p[2] = tmp; // BGR → RGB
            // EGE color_t is ARGB, glReadPixels gives RGBA
            // Swap to match: ABGR → ARGB (swap R and B)
            m_cpuBuffer[i] = (m_cpuBuffer[i] & 0xFF00FF00) |
                             ((m_cpuBuffer[i] >> 16) & 0xFF) |
                             ((m_cpuBuffer[i] << 16) & 0xFF0000);
        }
        m_gpuDirty = false;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return 0;
    return m_cpuBuffer[y * m_width + x];
}

void GlRenderTarget::putPixelAlpha(int x, int y, color_t color) { putPixel(x, y, color); }
void GlRenderTarget::putPixelSaveAlpha(int x, int y, color_t color) { putPixel(x, y, color); }
void GlRenderTarget::putPixelAlphaBlend(int x, int y, color_t color, unsigned char alphaFactor) { putPixel(x, y, color); }

void GlRenderTarget::putPixels(int count, const int* points) {
    for (int i = 0; i < count; i++)
        putPixel(points[i*2], points[i*2+1], m_lineColor);
}

void GlRenderTarget::floodFill(int x, int y, color_t borderColor) {
    // CPU-side flood fill — Phase 6 full implementation
    // For Phase 1: just put a single pixel
    putPixel(x, y, m_fillColor);
}

// ============================================================
// Clear
// ============================================================
void GlRenderTarget::clear(color_t color) {
    submitBatch();
    bindForDrawing();

    float r, g, b, a;
    color_t_to_rgba(color, r, g, b, a);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);

    m_gpuDirty = true;

    // Clear CPU buffer too
    if (m_cpuBuffer) {
        memset(m_cpuBuffer, 0, sizeof(color_t) * m_width * m_height);
    }
}

// ============================================================
// Image transfer — Phase 3 stubs
// ============================================================
void GlRenderTarget::blit(int dstX, int dstY, RenderTarget* src, int srcX, int srcY, int w, int h) {
    // TODO: Phase 3
}
void GlRenderTarget::blitStretch(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH) {}
void GlRenderTarget::alphaBlend(int dstX, int dstY, int dstW, int dstH,
                    RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                    unsigned char alpha) {}
void GlRenderTarget::alphaTransparent(int dstX, int dstY, RenderTarget* src,
                          int srcX, int srcY, int w, int h,
                          color_t transparentColor) {}
void GlRenderTarget::withAlpha(int dstX, int dstY, int dstW, int dstH,
                   RenderTarget* src, int srcX, int srcY, int srcW, int srcH) {}
void GlRenderTarget::alphaFilter(int dstX, int dstY, int w, int h,
                     RenderTarget* src, int srcX, int srcY,
                     unsigned char alpha) {}
void GlRenderTarget::rotateBlend(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                     float angle, float centerX, float centerY) {}
void GlRenderTarget::rotateZoomBlend(int dstX, int dstY, int dstW, int dstH,
                         RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                         float angle, float centerX, float centerY,
                         float zoomX, float zoomY) {}
void GlRenderTarget::filterBlur(int dstX, int dstY, int w, int h, float intensity) {}

// ============================================================
// Text — Phase 4 stubs
// ============================================================
void GlRenderTarget::setFont(int, int, const char*, int, int, int, bool, bool, bool) {}
void GlRenderTarget::setTextJustify(TextHAlign, TextVAlign) {}
void GlRenderTarget::drawText(float, float, const char*) {}
void GlRenderTarget::drawText(float, float, const wchar_t*) {}
int  GlRenderTarget::getTextWidth(const char*) const { return 0; }
int  GlRenderTarget::getTextWidth(const wchar_t*) const { return 0; }
int  GlRenderTarget::getTextHeight(const char*) const { return 0; }
int  GlRenderTarget::getTextHeight(const wchar_t*) const { return 0; }
void GlRenderTarget::measureText(const char*, float*, float*) const {}
void GlRenderTarget::measureText(const wchar_t*, float*, float*) const {}

// ============================================================
// Pixel buffer access
// ============================================================
color_t* GlRenderTarget::getPixelBuffer() {
    if (m_gpuDirty && m_initialized) {
        // Read back from GPU
        if (m_isOnScreen) glBindFramebuffer(GL_FRAMEBUFFER, 0);
        else glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        // glReadPixels returns RGBA, need to convert to EGE ARGB
        unsigned char* tmpBuf = new unsigned char[m_width * m_height * 4];
        glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, tmpBuf);
        for (int i = 0; i < m_width * m_height; i++) {
            unsigned char r = tmpBuf[i*4+0];
            unsigned char g = tmpBuf[i*4+1];
            unsigned char b = tmpBuf[i*4+2];
            unsigned char a = tmpBuf[i*4+3];
            m_cpuBuffer[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        delete[] tmpBuf;
        m_gpuDirty = false;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    return m_cpuBuffer;
}

const color_t* GlRenderTarget::getPixelBuffer() const {
    return const_cast<GlRenderTarget*>(this)->getPixelBuffer();
}

// ============================================================
// Submit
// ============================================================
void GlRenderTarget::flush() {
    submitBatch();
    glFlush();
}

void GlRenderTarget::present() {
    flush();
    if (m_isOnScreen) {
        GLFWwindow* win = glfwGetCurrentContext() ? glfwGetCurrentContext() : nullptr;
        if (win) glfwSwapBuffers(win);
    }
}

} // namespace ege
