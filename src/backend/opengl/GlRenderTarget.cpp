// src/backend/opengl/GlRenderTarget.cpp
#include "GlRenderTarget.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

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

// Image blit/blend vertex shader — passes src UVs with transform
static const char* g_imgVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;  // quad coords 0..1
out vec2 vUV;

uniform vec4 uSrcRect;  // srcX, srcY, srcW, srcH
uniform float uAngle;
uniform vec2 uCenter;
uniform vec2 uZoom;

void main() {
    vec2 n = aPos; // 0..1
    // Map to source pixel coords
    float sx = uSrcRect.x + n.x * uSrcRect.z;
    float sy = uSrcRect.y + n.y * uSrcRect.w;
    // Zoom around center
    float cx = uCenter.x, cy = uCenter.y;
    sx = cx + (sx - cx) / uZoom.x;
    sy = cy + (sy - cy) / uZoom.y;
    // Rotation around center
    float c = cos(uAngle), s = sin(uAngle);
    float dx = sx - cx, dy = sy - cy;
    float rx = c * dx + s * dy + cx;
    float ry = -s * dx + c * dy + cy;
    // Texture coords (flip Y for top-left origin)
    vUV = vec2(rx / uSrcRect.z, 1.0 - ry / uSrcRect.w);
    gl_Position = vec4(n * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Image fragment shader — simple sampling + optional color key discard + alpha scaling
// ROP2 modes use destination texture for XOR/AND/OR operations
static const char* g_imgFragSrc = R"(
#version 330 core
uniform sampler2D uTex;
uniform sampler2D uDstTex;      // destination texture for ROP2
uniform int uMode;               // 0=copy, 1=colorKey, 2=XOR, 3=AND, 4=OR
uniform vec3 uKeyColor;          // for color key mode (normalized RGB)
uniform float uKeyTol;           // tolerance for color key matching
uniform float uAlphaOverride;    // if > 0, multiply source alpha by this
in vec2 vUV;
out vec4 fragColor;

void main() {
    vec4 src = texture(uTex, vUV);
    vec4 dst = texture(uDstTex, vUV);

    if (uMode == 1) {
        // Transparent color key: discard if within tolerance
        vec3 diff = abs(src.rgb - uKeyColor);
        if (max(diff.r, max(diff.g, diff.b)) < uKeyTol) discard;
    }

    vec4 result = src;
    if (uMode == 2) {
        // XOR: (src XOR dst) per channel
        ivec4 si = ivec4(src * 255.0);
        ivec4 di = ivec4(dst * 255.0);
        result = vec4(si ^ di) / 255.0;
    } else if (uMode == 3) {
        // AND: (src AND dst) per channel
        ivec4 si = ivec4(src * 255.0);
        ivec4 di = ivec4(dst * 255.0);
        result = vec4(si & di) / 255.0;
    } else if (uMode == 4) {
        // OR: (src OR dst) per channel
        ivec4 si = ivec4(src * 255.0);
        ivec4 di = ivec4(dst * 255.0);
        result = vec4(si | di) / 255.0;
    }

    if (uAlphaOverride > 0.0) {
        result.a *= uAlphaOverride;
    }

    // For ROP2 modes, blend the result over destination using alpha
    if (uMode >= 2 && uMode <= 4) {
        float a = result.a;
        result.rgb = mix(dst.rgb, result.rgb, a);
    }

    fragColor = result;
}
)";

// Text shader — samples glyph atlas (grayscale-as-alpha) and outputs text color
static const char* g_textVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)";

static const char* g_textFragSrc = R"(
#version 330 core
uniform sampler2D uGlyphTex;
uniform vec4 uTextColor;  // RGB = text color, A = alpha
in vec2 vUV;
out vec4 fragColor;
void main() {
    float a = texture(uGlyphTex, vUV).a;
    fragColor = vec4(uTextColor.rgb, uTextColor.a * a);
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
      m_projectionDirty(true),
      m_imageShaderReady(false) {
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

    FILE* dbg = fopen("/tmp/ege_capture_debug.txt", "a");
    if(dbg) { fprintf(dbg, "  proj: l=%g r=%g b=%g t=%g m[0]=%g m[5]=%g m[12]=%g m[13]=%g\n",
        left, right, bottom, top, proj[0], proj[5], proj[12], proj[13]); fclose(dbg); }

    m_primShader.use();
    m_primShader.setUniformMatrix4("uProj", proj);
    m_projectionDirty = false;
}

void GlRenderTarget::bindForDrawing() {
    if (m_isOnScreen) {
        // For screen, bind framebuffer 0 but ensure proper state
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }
    glViewport(0, 0, m_width, m_height);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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

    // Create fresh VAO/VBO
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GlVertex) * m_vertices.size(), m_vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (void*)offsetof(GlVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (void*)offsetof(GlVertex, r));

    // Test: try drawing without shader first (fixed-function, but that doesn't exist in Core)
    // Instead, check the shader program
    GLint prog;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);

    FILE* dbg = fopen("/tmp/ege_submit_debug.txt", "a");
    if(dbg) {
        fprintf(dbg, "submit: %d verts prog=%d linked=%d\n", (int)m_vertices.size(), prog, linked);
    }

    // Check vertex attrib state
    GLint maxAttribs, attrib0, attrib1;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attrib0);
    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attrib1);
    if(dbg) {
        fprintf(dbg, "  attrib0: enabled=%d attrib1: enabled=%d maxAttribs=%d\n", attrib0, attrib1, maxAttribs);
    }

    m_primShader.use();
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertices.size());

    GLenum err = glGetError();
    if(dbg) { fprintf(dbg, "  err=0x%x\n", err); fclose(dbg); }

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    m_vertices.clear();
}

// ============================================================
// Image blit helpers
// ============================================================

// Sync source RenderTarget's CPU buffer to GPU texture if dirty
static void syncSrcTexture(RenderTarget* src) {
    if (!src) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    // Force GPU sync: getPixelBuffer ensures GPU→CPU sync first
    glSrc->getPixelBuffer();
    // Then sync CPU buffer to GPU texture
    glSrc->syncToGpu();
}

void GlRenderTarget::ensureImageShader() {
    if (m_imageShaderReady) return;
    m_imageShader.compileVertex(g_imgVertSrc);
    m_imageShader.compileFragment(g_imgFragSrc);
    m_imageShader.link();
    m_imageShaderReady = true;
}

// Draw source texture onto the current framebuffer with a full-screen quad.
// The quad is rendered in normalized device coordinates (NDC: -1..1).
// The source UV transformation (rotation, zoom) is applied in the vertex shader.
// OpenGL blend function controls the compositing mode.
void GlRenderTarget::drawImageQuad(GLuint srcTex, int srcW, int srcH,
                                    int srcX, int srcY, int srcW2, int srcH2,
                                    int dstX, int dstY, int dstW2, int dstH2,
                                    float angle, float centerX, float centerY,
                                    float zoomX, float zoomY,
                                    int mode, color_t keyColor) {
    drawImageQuadInternal(srcTex, srcW, srcH, srcX, srcY, srcW2, srcH2,
                          dstX, dstY, dstW2, dstH2, angle, centerX, centerY,
                          zoomX, zoomY, mode, keyColor, -1.0f);
}

void GlRenderTarget::drawImageQuadInternal(GLuint srcTex, int srcW, int srcH,
                                            int srcX, int srcY, int srcW2, int srcH2,
                                            int dstX, int dstY, int dstW2, int dstH2,
                                            float angle, float centerX, float centerY,
                                            float zoomX, float zoomY,
                                            int mode, color_t keyColor,
                                            float alphaOverride) {
    if (!m_initialized || srcW <= 0 || srcH <= 0) return;

    ensureImageShader();

    // Save current GL state
    GLint prevFbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLint prevTex;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    GLint prevProg;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    GLint prevVao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
    GLboolean blendWas;
    glGetBooleanv(GL_BLEND, &blendWas);
    GLint prevBlendSrc, prevBlendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDst);

    // Bind destination framebuffer
    if (m_isOnScreen) glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Compute NDC coordinates for destination quad
    // NDC: x=-1 is left, x=+1 is right, y=-1 is bottom, y=+1 is top
    float dstL =  2.0f * dstX        / m_width  - 1.0f;
    float dstR =  2.0f * (dstX+dstW2) / m_width  - 1.0f;
    float dstB = -2.0f * (dstY+dstH2) / m_height + 1.0f;
    float dstT = -2.0f * dstY         / m_height + 1.0f;

    // Quad vertices in NDC (top-left, top-right, bottom-right, bottom-left)
    float quad[16] = {
        dstL, dstT, dstR, dstT,
        dstR, dstB, dstL, dstB
    };

    // Create VAO + VBO for this draw call
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, quad, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Set up shader
    m_imageShader.use();

    // Source rectangle uniform (pixel coords in source texture)
    float srcRect[4] = { (float)srcX, (float)srcY, (float)srcW2, (float)srcH2 };
    m_imageShader.setUniform4f("uSrcRect", srcRect[0], srcRect[1], srcRect[2], srcRect[3]);

    // Rotation and zoom uniforms
    m_imageShader.setUniform1f("uAngle", angle);
    m_imageShader.setUniform2f("uCenter", centerX, centerY);
    m_imageShader.setUniform2f("uZoom", zoomX, zoomY);

    // Mode uniform
    m_imageShader.setUniform1i("uMode", mode);

    // Key color uniform (normalized RGB)
    float kr = ((keyColor >> 16) & 0xFF) / 255.0f;
    float kg = ((keyColor >>  8) & 0xFF) / 255.0f;
    float kb = ( keyColor        & 0xFF) / 255.0f;
    m_imageShader.setUniform3f("uKeyColor", kr, kg, kb);
    m_imageShader.setUniform1f("uKeyTol", 0.02f);
    m_imageShader.setUniform1f("uAlphaOverride", alphaOverride > 0.0f ? alphaOverride : 0.0f);

    // Bind source texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    GLint loc = glGetUniformLocation(m_imageShader.getProgram(), "uTex");
    glUniform1i(loc, 0);

    // Bind destination texture to unit 1 for ROP2 modes
    GLuint dstTexForShader = 0;
    bool needDstTex = (mode >= 2 && mode <= 4);
    if (needDstTex) {
        if (!m_isOnScreen) {
            // Offscreen: the FBO's texture is the destination
            dstTexForShader = m_texture;
        } else {
            // On-screen: read current framebuffer into a temp texture
            GLint prevReadFbo;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevReadFbo);
            std::vector<unsigned char> dstPixels(dstW2 * dstH2 * 4);
            glReadPixels(dstX, m_height - dstY - dstH2, dstW2, dstH2, GL_RGBA, GL_UNSIGNED_BYTE, dstPixels.data());
            glGenTextures(1, &dstTexForShader);
            glBindTexture(GL_TEXTURE_2D, dstTexForShader);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dstW2, dstH2, 0, GL_RGBA, GL_UNSIGNED_BYTE, dstPixels.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, dstTexForShader);
        loc = glGetUniformLocation(m_imageShader.getProgram(), "uDstTex");
        glUniform1i(loc, 1);
    } else {
        // For non-ROP modes, bind a dummy texture to unit 1
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, srcTex); // reuse source as dummy
        loc = glGetUniformLocation(m_imageShader.getProgram(), "uDstTex");
        glUniform1i(loc, 1);
    }

    // Configure blend mode based on the operation
    if (mode == 0) {
        // Copy mode: disable blending (source overwrites destination)
        glDisable(GL_BLEND);
    } else if (mode >= 2 && mode <= 4) {
        // ROP2 modes: shader handles compositing, disable OpenGL blending
        glDisable(GL_BLEND);
    } else {
        // Enable alpha blending for color key and alpha override modes
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glViewport(0, 0, m_width, m_height);

    // Draw
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Restore state
    glBindVertexArray(prevVao);
    glBindBuffer(GL_ARRAY_BUFFER, prevTex); // not needed but safe
    glBindTexture(GL_TEXTURE_2D, prevTex);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    if (prevProg) glUseProgram(prevProg);
    if (blendWas) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    glBlendFunc(prevBlendSrc, prevBlendDst);

    // Clean up temporary on-screen dst texture
    if (needDstTex && m_isOnScreen && dstTexForShader) {
        glDeleteTextures(1, &dstTexForShader);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
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
// Image transfer — Phase 3 implementation
// ============================================================

void GlRenderTarget::blit(int dstX, int dstY, RenderTarget* src, int srcX, int srcY, int w, int h) {
    if (!src || w <= 0 || h <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) {
        // Fallback: CPU pixel copy for non-GL render targets
        return;
    }
    syncSrcTexture(glSrc);
    // Map RasterOp to shader mode: COPY=0, XOR=2, AND=3, OR=4
    int shaderMode = 0;
    switch (m_rasterOp) {
        case ROP_XOR: shaderMode = 2; break;
        case ROP_AND: shaderMode = 3; break;
        case ROP_OR:  shaderMode = 4; break;
        default:      shaderMode = 0; break; // ROP_COPY, ROP_NOP
    }
    drawImageQuad(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                  srcX, srcY, w, h, dstX, dstY, w, h,
                  0, 0, 0, 1.0f, 1.0f, shaderMode, 0);
}

void GlRenderTarget::blitStretch(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH) {
    if (!src || dstW <= 0 || dstH <= 0 || srcW <= 0 || srcH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    drawImageQuad(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                  srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH,
                  0, 0, 0, 1.0f, 1.0f, 0, 0);
}

void GlRenderTarget::alphaBlend(int dstX, int dstY, int dstW, int dstH,
                    RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                    unsigned char alpha) {
    if (!src || alpha == 0 || dstW <= 0 || dstH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    float af = alpha / 255.0f;
    drawImageQuadInternal(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                          srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH,
                          0, 0, 0, 1.0f, 1.0f, 0, 0, af);
}

void GlRenderTarget::alphaTransparent(int dstX, int dstY, RenderTarget* src,
                          int srcX, int srcY, int w, int h,
                          color_t transparentColor) {
    if (!src || w <= 0 || h <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    drawImageQuad(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                  srcX, srcY, w, h, dstX, dstY, w, h,
                  0, 0, 0, 1.0f, 1.0f,
                  1, transparentColor); // mode=1 = transparent color key
}

void GlRenderTarget::withAlpha(int dstX, int dstY, int dstW, int dstH,
                   RenderTarget* src, int srcX, int srcY, int srcW, int srcH) {
    if (!src || dstW <= 0 || dstH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    // withAlpha uses per-pixel alpha from the source (PRGB32 pre-multiplied)
    drawImageQuad(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                  srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH,
                  0, 0, 0, 1.0f, 1.0f, 0, 0);
}

void GlRenderTarget::alphaFilter(int dstX, int dstY, int w, int h,
                     RenderTarget* src, int srcX, int srcY,
                     unsigned char alpha) {
    if (!src || w <= 0 || h <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    // alphaFilter uses source image's alpha channel modulated by uniform alpha
    float af = alpha / 255.0f;
    drawImageQuadInternal(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                          srcX, srcY, w, h, dstX, dstY, w, h,
                          0, 0, 0, 1.0f, 1.0f, 0, 0, af);
}

void GlRenderTarget::rotateBlend(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                     float angle, float centerX, float centerY) {
    if (!src || dstW <= 0 || dstH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    drawImageQuad(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                  srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH,
                  angle, centerX, centerY, 1.0f, 1.0f, 0, 0);
}

void GlRenderTarget::rotateZoomBlend(int dstX, int dstY, int dstW, int dstH,
                         RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                         float angle, float centerX, float centerY,
                         float zoomX, float zoomY) {
    if (!src || dstW <= 0 || dstH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    drawImageQuad(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                  srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH,
                  angle, centerX, centerY, zoomX, zoomY, 0, 0);
}

void GlRenderTarget::filterBlur(int dstX, int dstY, int w, int h, float intensity) {
    if (w <= 0 || h <= 0 || intensity <= 0) return;
    // Read pixels from GPU, apply box blur on CPU, write back
    if (m_isOnScreen) glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    std::vector<unsigned char> rgba(w * h * 4);
    glReadPixels(dstX, m_height - dstY - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Apply simple box blur
    int radius = (int)(intensity / 2.0f) + 1;
    std::vector<unsigned char> out(rgba.size());
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float r = 0, g = 0, b = 0, a = 0;
            int count = 0;
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        int idx = (ny * w + nx) * 4;
                        r += rgba[idx]; g += rgba[idx+1]; b += rgba[idx+2]; a += rgba[idx+3];
                        count++;
                    }
                }
            }
            int oi = (y * w + x) * 4;
            out[oi]   = (unsigned char)(r / count);
            out[oi+1] = (unsigned char)(g / count);
            out[oi+2] = (unsigned char)(b / count);
            out[oi+3] = (unsigned char)(a / count);
        }
    }

    // Write back to texture
    if (m_isOnScreen) glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glTexSubImage2D(GL_TEXTURE_2D, 0, dstX, m_height - dstY - h, w, h,
                    GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Update CPU buffer
    if (m_cpuBuffer) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int oi = (y * w + x) * 4;
                unsigned char r = out[oi], g = out[oi+1], b = out[oi+2], a = out[oi+3];
                if (y + dstY < m_height && x + dstX < m_width) {
                    m_cpuBuffer[(y + dstY) * m_width + (x + dstX)] =
                        ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                }
            }
        }
    }
    m_gpuDirty = true;
}

// ============================================================
// Text — Phase 4 implementation
// ============================================================

void GlRenderTarget::setFont(int height, int width, const char* face,
                             int escapement, int orientation, int weight,
                             bool italic, bool underline, bool strikeout) {
    // Store font configuration
    m_fontConfig.height = height > 0 ? height : 16;
    m_fontConfig.width = width;
    m_fontConfig.escapement = escapement;
    m_fontConfig.weight = weight;
    m_fontConfig.italic = italic;
    m_fontConfig.underline = underline;
    m_fontConfig.strikeout = strikeout;
    if (face && face[0]) {
        strncpy(m_fontConfig.face, face, sizeof(m_fontConfig.face) - 1);
        m_fontConfig.face[sizeof(m_fontConfig.face) - 1] = '\0';
    } else {
        strncpy(m_fontConfig.face, "Arial", sizeof(m_fontConfig.face) - 1);
        m_fontConfig.face[sizeof(m_fontConfig.face) - 1] = '\0';
    }

    // Load the font into the glyph atlas
    m_glyphAtlas.loadFont(m_fontConfig.face, m_fontConfig.height,
                          m_fontConfig.weight, m_fontConfig.italic);
}

void GlRenderTarget::setTextJustify(TextHAlign h, TextVAlign v) {
    m_hAlign = h;
    m_vAlign = v;
}

// Render a single glyph quad using the text shader
void GlRenderTarget::drawGlyphTexture(GLuint tex, int texW, int texH,
                                      int srcX, int srcY, int srcW, int srcH,
                                      int dstX, int dstY, int dstW, int dstH,
                                      float angle, float r, float g, float b, float a) {
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return;
    if (!m_initialized) return;

    ensureTextShader();

    // Save GL state
    GLint prevFbo, prevTex, prevProg, prevVao;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
    GLboolean blendWas;
    glGetBooleanv(GL_BLEND, &blendWas);
    GLint prevBlendSrc, prevBlendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDst);

    // Bind destination FBO
    if (m_isOnScreen) glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Compute NDC for destination quad
    float dstL =  2.0f * dstX          / m_width  - 1.0f;
    float dstR =  2.0f * (dstX+dstW)   / m_width  - 1.0f;
    float dstB = -2.0f * (dstY+dstH)   / m_height + 1.0f;
    float dstT = -2.0f * dstY           / m_height + 1.0f;

    // Compute UVs for the glyph sub-rect in the atlas
    float uL = (float)srcX / texW;
    float uR = (float)(srcX + srcW) / texW;
    float vT = (float)srcY / texH;  // top-left origin for texture
    float vB = (float)(srcY + srcH) / texH;

    // Interleaved vertex data: 2 pos (NDC) + 2 UV per vertex
    float verts[24] = {
        dstL, dstT,  uL, vT,  // top-left
        dstR, dstT,  uR, vT,  // top-right
        dstR, dstB,  uR, vB,  // bottom-right
        dstL, dstB,  uL, vB,  // bottom-left
    };

    // Build VAO+VBO
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Use text shader
    m_textShader.use();

    // Bind glyph atlas texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint loc = glGetUniformLocation(m_textShader.getProgram(), "uGlyphTex");
    glUniform1i(loc, 0);

    // Set text color
    m_textShader.setUniform4f("uTextColor", r, g, b, a);

    // Render
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, m_width, m_height);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Restore state
    glBindVertexArray(prevVao);
    glBindBuffer(GL_ARRAY_BUFFER, prevTex);
    glBindTexture(GL_TEXTURE_2D, prevTex);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    if (prevProg) glUseProgram(prevProg);
    if (blendWas) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    glBlendFunc(prevBlendSrc, prevBlendDst);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    // Mark GPU dirty so CPU buffer stays in sync
    m_gpuDirty = true;
}

void GlRenderTarget::ensureTextShader() {
    if (m_textShaderReady) return;
    m_textShader.compileVertex(g_textVertSrc);
    m_textShader.compileFragment(g_textFragSrc);
    m_textShader.link();
    m_textShaderReady = true;
}

// Core text rendering: render a wchar_t string
void GlRenderTarget::renderText(float x, float y, const wchar_t* text) {
    if (!text || !text[0] || !m_glyphAtlas.isLoaded()) return;

    GlyphAtlas& atlas = m_glyphAtlas;
    const FontConfig& fc = m_fontConfig;

    // Compute escapement rotation in radians
    float angleRad = -fc.escapement / 10.0f * (float)M_PI / 180.0f;
    float cosA = cosf(angleRad);
    float sinA = sinf(angleRad);

    // Compute starting x offset based on horizontal alignment
    // First pass: measure total width
    float totalWidth = 0;
    const wchar_t* p = text;
    while (*p) {
        GlyphInfo gi = m_glyphAtlas.ensureGlyph(*p);
        if (gi.valid) {
            totalWidth += gi.advance;
        } else {
            totalWidth += atlas.getAscent() * 0.5f; // fallback advance
        }
        p++;
    }

    float cursorX = 0;
    switch (m_hAlign) {
        case TEXT_LEFT:   cursorX = 0; break;
        case TEXT_CENTER: cursorX = -totalWidth * 0.5f; break;
        case TEXT_RIGHT:  cursorX = -totalWidth; break;
    }

    // Vertical alignment offset
    float cursorY = 0;
    switch (m_vAlign) {
        case TEXT_TOP:    cursorY = 0; break;
        case TEXT_MIDDLE: cursorY = -(atlas.getAscent() + atlas.getDescent()) * 0.5f; break;
        case TEXT_BOTTOM: cursorY = -(atlas.getAscent() + atlas.getDescent()); break;
    }

    // Get text color
    float tr, tg, tb, ta;
    color_t_to_rgba(m_textColor, tr, tg, tb, ta);

    // Second pass: render each glyph
    p = text;
    while (*p) {
        GlyphInfo gi = atlas.ensureGlyph(*p);

        if (gi.valid) {
            // Compute glyph position relative to baseline
            float gx = cursorX + gi.bearingX;
            float gy = cursorY - gi.bearingY; // stb y-down convention

            // Apply escapement rotation
            float rx = gx * cosA - gy * sinA + x;
            float ry = gx * sinA + gy * cosA + y;

            // Draw glyph quad
            drawGlyphTexture(
                atlas.getTexture(), atlas.getAtlasSize(), atlas.getAtlasSize(),
                gi.atlasX, gi.atlasY, gi.width, gi.height,
                (int)rx, (int)ry, gi.width, gi.height,
                angleRad, tr, tg, tb, ta
            );
        } else {
            // Space character or missing glyph — just advance
            float spaceAdvance = atlas.getAscent() * 0.3f;
            float gx = cursorX;
            float gy = cursorY;
            float rx = gx * cosA - gy * sinA + x;
            float ry = gx * sinA + gy * cosA + y;
        }

        cursorX += gi.advance;
        p++;
    }
}

void GlRenderTarget::drawText(float x, float y, const char* text) {
    // Convert UTF-8/ANSI to wchar_t
    if (!text) return;
    int len = (int)strlen(text);
    if (len == 0) return;

    // Simple UTF-8 to wchar_t conversion
    std::wstring wtext;
    wtext.reserve(len);
    const unsigned char* p = (const unsigned char*)text;
    while (*p) {
        if (*p < 0x80) {
            wtext += (wchar_t)*p;
            p++;
        } else if ((*p & 0xE0) == 0xC0) {
            wchar_t cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            wtext += cp;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            wchar_t cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            wtext += cp;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            // Surrogate pair for > U+FFFF
            uint32_t cp = ((uint32_t)(p[0] & 0x07) << 18) |
                          ((uint32_t)(p[1] & 0x3F) << 12) |
                          ((uint32_t)(p[2] & 0x3F) << 6) |
                          (p[3] & 0x3F);
            if (cp >= 0x10000) {
                cp -= 0x10000;
                wtext += (wchar_t)(0xD800 + (cp >> 10));
                wtext += (wchar_t)(0xDC00 + (cp & 0x3FF));
            }
            p += 4;
        } else {
            p++; // skip invalid
        }
    }
    wtext += L'\0';
    drawText(x, y, wtext.c_str());
}

void GlRenderTarget::drawText(float x, float y, const wchar_t* text) {
    renderText(x, y, text);
}

int GlRenderTarget::getTextWidth(const char* text) const {
    if (!text || !m_glyphAtlas.isLoaded()) return 0;
    // Convert to wchar_t and measure
    int len = (int)strlen(text);
    if (len == 0) return 0;
    std::wstring wtext;
    wtext.reserve(len);
    const unsigned char* p = (const unsigned char*)text;
    while (*p) {
        if (*p < 0x80) { wtext += (wchar_t)*p; p++; }
        else if ((*p & 0xE0) == 0xC0) {
            wtext += (wchar_t)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F)); p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            wtext += (wchar_t)(((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F)); p += 3;
        } else { p++; }
    }
    return getTextWidth(wtext.c_str());
}

int GlRenderTarget::getTextWidth(const wchar_t* text) const {
    if (!text || !m_glyphAtlas.isLoaded()) return 0;
    float w = 0, h = 0;
    const_cast<GlRenderTarget*>(this)->measureText(text, &w, &h);
    return (int)(w + 0.5f);
}

int GlRenderTarget::getTextHeight(const char* text) const {
    (void)text;
    if (!m_glyphAtlas.isLoaded()) return 0;
    return m_glyphAtlas.getAscent() - m_glyphAtlas.getDescent();
}

int GlRenderTarget::getTextHeight(const wchar_t* text) const {
    (void)text;
    if (!m_glyphAtlas.isLoaded()) return 0;
    return m_glyphAtlas.getAscent() - m_glyphAtlas.getDescent();
}

void GlRenderTarget::measureText(const char* text, float* width, float* height) const {
    if (!text) { if (width) *width = 0; if (height) *height = 0; return; }
    int len = (int)strlen(text);
    if (len == 0) { if (width) *width = 0; if (height) *height = 0; return; }
    std::wstring wtext;
    wtext.reserve(len);
    const unsigned char* p = (const unsigned char*)text;
    while (*p) {
        if (*p < 0x80) { wtext += (wchar_t)*p; p++; }
        else if ((*p & 0xE0) == 0xC0) {
            wtext += (wchar_t)(((p[0] & 0x1F) << 6) | (p[1] & 0x3F)); p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            wtext += (wchar_t)(((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F)); p += 3;
        } else { p++; }
    }
    measureText(wtext.c_str(), width, height);
}

void GlRenderTarget::measureText(const wchar_t* text, float* width, float* height) const {
    if (!m_glyphAtlas.isLoaded()) { if (width) *width = 0; if (height) *height = 0; return; }

    float totalWidth = 0;
    if (text) {
        const wchar_t* p = text;
        while (*p) {
            // Cast away const to allow glyph caching (side-effect of measuring)
            GlyphInfo gi = const_cast<GlyphAtlas&>(m_glyphAtlas).ensureGlyph(*p);
            if (gi.valid) {
                totalWidth += gi.advance;
            } else {
                totalWidth += m_glyphAtlas.getAscent() * 0.5f;
            }
            p++;
        }
    }

    if (width) *width = totalWidth;
    if (height) *height = (float)(m_glyphAtlas.getAscent() - m_glyphAtlas.getDescent());
}

// ============================================================
// Pixel buffer access
// ============================================================
color_t* GlRenderTarget::getPixelBuffer() {
    if (m_initialized) {
        // Flush any pending draw commands before reading back
        submitBatch();

        // For on-screen render targets:
        // If not dirty, the CPU buffer was already synced by captureScreenToTexture()
        // during the last swap. If dirty, new draws happened after capture, so we need
        // to re-read from the GPU.
        if (m_isOnScreen) {
            if (!m_gpuDirty && m_cpuBuffer) {
                return m_cpuBuffer; // Already captured by swapBuffers
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        }
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

void GlRenderTarget::captureScreenToTexture() {
    if (!m_isOnScreen || !m_initialized || !m_texture) return;

    // Flush pending draw commands first
    submitBatch();

    // Ensure GPU has completed all rendering
    glFinish();

    // Read pixels from default framebuffer (back buffer, before swap)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned char* tmpBuf = new unsigned char[m_width * m_height * 4];
    glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, tmpBuf);

    FILE* dbg = fopen("/tmp/ege_capture_debug.txt", "a");
    GLenum glErr = glGetError();
    GLint curProg = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &curProg);
    GLboolean masks[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, masks);
    if(dbg) {
        fprintf(dbg, "capture: px0=0x%02x%02x%02x%02x glErrPre=0x%x prog=%d colorMask=%d%d%d%d\n",
            tmpBuf[3], tmpBuf[0], tmpBuf[1], tmpBuf[2], glErr, curProg, masks[0], masks[1], masks[2], masks[3]);
        // Count non-zero pixels
        int colored = 0, alpha0 = 0;
        for (int i = 0; i < m_width * m_height; i++) {
            if (tmpBuf[i*4+0] != 0 || tmpBuf[i*4+1] != 0 || tmpBuf[i*4+2] != 0) colored++;
            if (tmpBuf[i*4+3] == 0) alpha0++;
        }
        fprintf(dbg, "  coloredPixels=%d/%d alphaZero=%d\n", colored, m_width * m_height, alpha0);
        fclose(dbg);
    }

    // Upload to texture
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, tmpBuf);

    // Also update CPU buffer
    if (m_cpuBuffer) {
        for (int i = 0; i < m_width * m_height; i++) {
            unsigned char r = tmpBuf[i*4+0];
            unsigned char g = tmpBuf[i*4+1];
            unsigned char b = tmpBuf[i*4+2];
            unsigned char a = tmpBuf[i*4+3];
            m_cpuBuffer[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
    m_gpuDirty = false;

    delete[] tmpBuf;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlRenderTarget::syncToGpu() {
    if (!m_cpuBuffer || !m_initialized) return;
    int w = m_width, h = m_height;
    // Convert ARGB CPU buffer to RGBA for OpenGL upload
    std::vector<unsigned char> rgba(w * h * 4);
    for (int i = 0; i < w * h; i++) {
        color_t c = m_cpuBuffer[i];
        rgba[i*4+0] = (c >> 16) & 0xFF; // R
        rgba[i*4+1] = (c >>  8) & 0xFF; // G
        rgba[i*4+2] = (c >>  0) & 0xFF; // B
        rgba[i*4+3] = (c >> 24) & 0xFF; // A
    }
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    m_gpuDirty = false;
}

void GlRenderTarget::rebuild(int width, int height) {
    if (!m_initialized) return;

    // Delete old GPU resources
    glDeleteTextures(1, &m_texture);
    if (!m_isOnScreen) glDeleteFramebuffers(1, &m_fbo);

    // Recreate texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (!m_isOnScreen) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Recreate CPU buffer
    delete[] m_cpuBuffer;
    m_cpuBuffer = new color_t[width * height];
    memset(m_cpuBuffer, 0, sizeof(color_t) * width * height);
    m_gpuDirty = true;

    m_width = width;
    m_height = height;
}

// ============================================================
// Submit
// ============================================================
void GlRenderTarget::flush() {
    static int totalVerts = 0;
    static int flushCount = 0;
    totalVerts += (int)m_vertices.size();
    flushCount++;
    FILE* dbg = fopen("/tmp/ege_capture_debug.txt", "a");
    if(dbg) {
        fprintf(dbg, "flush #%d: pendingVerts=%d totalVerts=%d\n", flushCount, (int)m_vertices.size(), totalVerts);
        fclose(dbg);
    }
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
