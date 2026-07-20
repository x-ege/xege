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
layout(location = 2) in float aPattern;
layout(location = 3) in vec4 aBackgroundColor;
uniform mat4 uProj;
out vec4 vColor;
flat out int vPattern;
out vec4 vBackgroundColor;
void main() {
    vColor = aColor;
    vPattern = int(aPattern + 0.5);
    vBackgroundColor = aBackgroundColor;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

static const char* g_primFragSrc = R"(
#version 330 core
in vec4 vColor;
flat in int vPattern;
in vec4 vBackgroundColor;
uniform int uHeight;
out vec4 fragColor;

bool foregroundPatternPixel(int pattern, int x, int y) {
    int slash = (x + y) & 7;
    int backslash = (x - y) & 7;
    if (pattern == 2) return y == 0;
    if (pattern == 3) return slash == 0;
    if (pattern == 4) return slash <= 1;
    if (pattern == 5) return backslash <= 1;
    if (pattern == 6) return backslash == 0;
    if (pattern == 7) return x == 0 || y == 0;
    if (pattern == 8) return slash <= 1 || backslash <= 1;
    if (pattern == 9) return (y == 0 && x < 4) || (y == 4 && x >= 4);
    if (pattern == 10) return x == 0 && y == 0;
    if (pattern == 11) return (x & 3) == 0 && (y & 3) == 0;
    return true;
}

void main() {
    int x = int(floor(gl_FragCoord.x)) & 7;
    int y = (uHeight - 1 - int(floor(gl_FragCoord.y))) & 7;
    fragColor = foregroundPatternPixel(vPattern, x, y) ? vColor : vBackgroundColor;
}
)";

// Image blit/blend vertex shader. Positions are already in normalized device
// coordinates and UVs are calculated on the CPU from the complete source
// texture dimensions.
static const char* g_imgVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

enum ImageShaderMode {
    IMG_COPY = 0,
    IMG_COLOR_KEY_COPY = 1,
    IMG_XOR = 2,
    IMG_AND = 3,
    IMG_OR = 4,
    IMG_ALPHA_PREMULTIPLIED = 5,
    IMG_ALPHA_STRAIGHT = 6,
    IMG_ALPHA_OPAQUE = 7,
    IMG_COLOR_KEY_ALPHA = 8,
    IMG_ZERO_KEY_COPY = 9,
    IMG_ZERO_KEY_ALPHA = 10,
};

// Image fragment shader — sampling, color-keying and the three public alpha
// formats. ROP modes are retained for compatibility with the original phase-3
// backend, although current putimage raster operations use the exact CPU path.
static const char* g_imgFragSrc = R"(
#version 330 core
uniform sampler2D uTex;
uniform sampler2D uDstTex;      // destination texture for ROP2
uniform int uMode;
uniform vec3 uKeyColor;          // for color key mode (normalized RGB)
uniform float uKeyTol;           // tolerance for color key matching
uniform float uAlphaOverride;    // global alpha, normalized to 0..1
in vec2 vUV;
out vec4 fragColor;

void main() {
    vec4 src = texture(uTex, vUV);
    vec4 dst = texture(uDstTex, vUV);
    float colorKeyWeight = 1.0;

    if (uMode == 1 || uMode == 8) {
        // The legacy CPU path compares the RGB channels exactly. Half of one
        // 8-bit step accounts for normalized texture conversion roundoff.
        vec3 diff = abs(src.rgb - uKeyColor);
        colorKeyWeight = step(uKeyTol, max(diff.r, max(diff.g, diff.b)));
    }
    if (uMode == 9 || uMode == 10) {
        // putimage_rotate(..., transparent=true) treats the all-zero pixel as
        // transparent, matching the pre-OpenGL triangle rasterizer.
        if (max(max(abs(src.r), abs(src.g)), max(abs(src.b), abs(src.a))) < uKeyTol) discard;
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

    if (uMode == 5) {
        // PARGB: global alpha scales every premultiplied component.
        result *= uAlphaOverride;
    } else if (uMode == 6) {
        // ARGB: RGB remains straight; the blend stage applies effective alpha.
        result.a *= uAlphaOverride;
    } else if (uMode == 1 || uMode == 8) {
        // A zero alpha leaves key pixels untouched; one performs an exact RGB
        // copy without a divergent discard branch.
        result.a = colorKeyWeight * uAlphaOverride;
    } else if (uMode == 7 || uMode == 10) {
        // RGB/constant-alpha operations ignore the stored source alpha.
        result.a = uAlphaOverride;
    } else if (uMode >= 2 && uMode <= 4) {
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
    gl_Position = vec4(aPos, 0.0, 1.0);
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

static std::vector<uint32_t> decodeUtf8(const char* text) {
    std::vector<uint32_t> result;
    if (!text) return result;
    const unsigned char* current = reinterpret_cast<const unsigned char*>(text);
    while (*current) {
        uint32_t codepoint = 0xFFFDU;
        size_t length = 1;
        if (current[0] < 0x80) {
            codepoint = current[0];
        } else if (current[0] >= 0xC2 && current[0] <= 0xDF &&
                   current[1] >= 0x80 && current[1] <= 0xBF) {
            codepoint = ((current[0] & 0x1FU) << 6) | (current[1] & 0x3FU);
            length = 2;
        } else if (current[0] >= 0xE0 && current[0] <= 0xEF && current[1] && current[2] &&
                   current[1] >= 0x80 && current[1] <= 0xBF &&
                   current[2] >= 0x80 && current[2] <= 0xBF) {
            codepoint = ((current[0] & 0x0FU) << 12) |
                        ((current[1] & 0x3FU) << 6) | (current[2] & 0x3FU);
            if (codepoint >= 0x800 && !(codepoint >= 0xD800 && codepoint <= 0xDFFF)) length = 3;
            else codepoint = 0xFFFDU;
        } else if (current[0] >= 0xF0 && current[0] <= 0xF4 &&
                   current[1] && current[2] && current[3] &&
                   current[1] >= 0x80 && current[1] <= 0xBF &&
                   current[2] >= 0x80 && current[2] <= 0xBF &&
                   current[3] >= 0x80 && current[3] <= 0xBF) {
            codepoint = ((current[0] & 0x07U) << 18) |
                        ((current[1] & 0x3FU) << 12) |
                        ((current[2] & 0x3FU) << 6) | (current[3] & 0x3FU);
            if (codepoint >= 0x10000 && codepoint <= 0x10FFFF) length = 4;
            else codepoint = 0xFFFDU;
        }
        result.push_back(codepoint);
        current += length;
    }
    return result;
}

static std::vector<uint32_t> decodeWide(const wchar_t* text) {
    std::vector<uint32_t> result;
    if (!text) return result;
    for (size_t i = 0; text[i]; ++i) {
        uint32_t codepoint = static_cast<uint32_t>(text[i]);
        if (sizeof(wchar_t) == 2 && codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            const uint32_t low = static_cast<uint32_t>(text[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            } else {
                codepoint = 0xFFFDU;
            }
        } else if ((codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF) {
            codepoint = 0xFFFDU;
        }
        result.push_back(codepoint);
    }
    return result;
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
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row) {
            result[column * 3 + row] = 0;
            for (int k = 0; k < 3; ++k)
                result[column * 3 + row] += a[k * 3 + row] * b[column * 3 + k];
        }
}

static GLenum rasterOpToGlLogicOp(RasterOp rop) {
    switch (rop) {
        case ROP_BLACK:       return GL_CLEAR;
        case ROP_NOTMERGEPEN: return GL_NOR;
        case ROP_MASKNOTPEN:  return GL_AND_INVERTED;
        case ROP_NOTCOPYPEN:  return GL_COPY_INVERTED;
        case ROP_MASKPENNOT:  return GL_AND_REVERSE;
        case ROP_NOT:         return GL_INVERT;
        case ROP_XOR:         return GL_XOR;
        case ROP_NOTMASKPEN:  return GL_NAND;
        case ROP_AND:         return GL_AND;
        case ROP_NOTXORPEN:   return GL_EQUIV;
        case ROP_NOP:         return GL_NOOP;
        case ROP_MERGENOTPEN: return GL_OR_INVERTED;
        case ROP_COPY:        return GL_COPY;
        case ROP_MERGEPENNOT: return GL_OR_REVERSE;
        case ROP_OR:          return GL_OR;
        case ROP_WHITE:       return GL_SET;
        default:              return GL_COPY;
    }
}

static bool fillPatternUsesForeground(FillStyle pattern, int x, int y) {
    x &= 7;
    y &= 7;
    const int slash = (x + y) & 7;
    const int backslash = (x - y) & 7;
    switch (pattern) {
        case FILL_HORIZONTAL:      return y == 0;
        case FILL_LIGHT_SLASH:     return slash == 0;
        case FILL_SLASH:           return slash <= 1;
        case FILL_BACKSLASH:       return backslash <= 1;
        case FILL_LIGHT_BACKSLASH: return backslash == 0;
        case FILL_HATCH:           return x == 0 || y == 0;
        case FILL_CROSS_HATCH:     return slash <= 1 || backslash <= 1;
        case FILL_INTERLEAVE:      return (y == 0 && x < 4) || (y == 4 && x >= 4);
        case FILL_WIDE_DOT:        return x == 0 && y == 0;
        case FILL_CLOSE_DOT:       return (x & 3) == 0 && (y & 3) == 0;
        default:                   return true;
    }
}

static int buildLineDashPattern(LineStyle style, unsigned short userPattern,
                                float pieces[16]) {
    if (style == LINE_DASHED) {
        pieces[0] = 6.0f; pieces[1] = 4.0f;
        return 2;
    }
    if (style == LINE_DOTTED) {
        pieces[0] = 1.0f; pieces[1] = 3.0f;
        return 2;
    }
    if (style == LINE_DASHDOT) {
        pieces[0] = 6.0f; pieces[1] = 4.0f;
        pieces[2] = 1.0f; pieces[3] = 3.0f;
        return 4;
    }
    if (style == LINE_DASHDOTDOT) {
        pieces[0] = 6.0f; pieces[1] = 4.0f;
        pieces[2] = 1.0f; pieces[3] = 3.0f;
        pieces[4] = 1.0f; pieces[5] = 3.0f;
        return 6;
    }
    if (style != LINE_USER) return 0;

    int pieceCount = 0;
    int runLength = 1;
    int state = (userPattern & 1U) != 0;
    for (int bit = 1; bit < 16; ++bit) {
        const int current = (userPattern & (1U << bit)) != 0;
        if (current == state) {
            ++runLength;
        } else {
            pieces[pieceCount++] = static_cast<float>(runLength);
            state = current;
            runLength = 1;
        }
    }
    pieces[pieceCount++] = static_cast<float>(runLength);
    if ((userPattern & 1U) == 0 && (pieceCount & 1) == 0) {
        const float initialGap = pieces[0];
        for (int i = 0; i + 1 < pieceCount; ++i) pieces[i] = pieces[i + 1];
        pieces[pieceCount - 1] = initialGap;
    }
    return pieceCount;
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
      m_imageShaderReady(false), m_textShaderReady(false),
      m_cpuBuffer(nullptr), m_pixelSyncState(PixelSyncState::Synchronized),
      m_cpuDirtyUnknown(false),
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
      m_projectionDirty(true), m_hAlign(TEXT_LEFT), m_vAlign(TEXT_TOP) {
    m_transformStack.push_back(std::vector<float>(identity3, identity3 + 9));
}

GlRenderTarget::PixelRect GlRenderTarget::clippedRect(
    int x, int y, int width, int height) const {
    if (width <= 0 || height <= 0 || m_width <= 0 || m_height <= 0) {
        return PixelRect();
    }
    const long long requestedRight = static_cast<long long>(x) + width;
    const long long requestedBottom = static_cast<long long>(y) + height;
    PixelRect result;
    result.left = std::max(0, x);
    result.top = std::max(0, y);
    result.right = static_cast<int>(std::min<long long>(m_width, requestedRight));
    result.bottom = static_cast<int>(std::min<long long>(m_height, requestedBottom));
    if (result.empty()) return PixelRect();
    return result;
}

GlRenderTarget::PixelRect GlRenderTarget::fullRect() const {
    return PixelRect(0, 0, m_width, m_height);
}

void GlRenderTarget::unionRect(PixelRect& destination, const PixelRect& source) {
    if (source.empty()) return;
    if (destination.empty()) {
        destination = source;
        return;
    }
    destination.left = std::min(destination.left, source.left);
    destination.top = std::min(destination.top, source.top);
    destination.right = std::max(destination.right, source.right);
    destination.bottom = std::max(destination.bottom, source.bottom);
}

void GlRenderTarget::markGpuDirty(const PixelRect& rect) {
    if (rect.empty()) return;
    if (m_pixelSyncState == PixelSyncState::GpuNewer ||
        m_pixelSyncState == PixelSyncState::ScreenTextureNewer) {
        unionRect(m_gpuDirtyRect, rect);
    } else {
        m_gpuDirtyRect = rect;
    }
    m_cpuDirtyRect = PixelRect();
    m_cpuDirtyUnknown = false;
    m_pixelSyncState = PixelSyncState::GpuNewer;
}

void GlRenderTarget::markGpuDirtyFull() {
    markGpuDirty(fullRect());
}

void GlRenderTarget::markCpuDirty(const PixelRect& rect, bool unknownRange) {
    if (rect.empty()) return;
    if (unknownRange) {
        // Preserve already declared writes while remembering that the newest
        // writable-pointer exposure has not been described yet. If it is not
        // followed by markPixelBufferDirty(), syncToGpu() falls back to the
        // complete image.
        if (m_pixelSyncState != PixelSyncState::CpuNewer) {
            m_cpuDirtyRect = PixelRect();
        }
        m_cpuDirtyUnknown = true;
    } else {
        if (m_pixelSyncState != PixelSyncState::CpuNewer) {
            m_cpuDirtyRect = rect;
            m_cpuDirtyUnknown = false;
        } else {
            unionRect(m_cpuDirtyRect, rect);
            // An internal precise write cannot narrow an earlier unresolved
            // public writable-pointer exposure.
        }
    }
    m_gpuDirtyRect = PixelRect();
    m_pixelSyncState = PixelSyncState::CpuNewer;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // CPU buffer
    m_cpuBuffer = new color_t[width * height];
    memset(m_cpuBuffer, 0, sizeof(color_t) * width * height);
    m_pixelSyncState = PixelSyncState::CpuNewer;
    m_cpuDirtyRect = fullRect();
    m_gpuDirtyRect = PixelRect();
    m_cpuDirtyUnknown = false;

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
    m_pixelSyncState = PixelSyncState::CpuNewer;
    m_cpuDirtyRect = fullRect();
    m_gpuDirtyRect = PixelRect();
    m_cpuDirtyUnknown = false;

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
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (void*)offsetof(GlVertex, pattern));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(GlVertex), (void*)offsetof(GlVertex, br));
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
void GlRenderTarget::setLineWidth(float width)   {
    m_lineWidth = std::max(1.0f, width);
    glLineWidth(m_lineWidth);
}
void GlRenderTarget::setLineStyle(LineStyle style, unsigned short pattern, int thickness) {
    m_lineStyle = style;
    m_linePattern = pattern;
    m_lineThickness = thickness;
    m_lineWidth = std::max(1, thickness);
}
void GlRenderTarget::setLineCap(RTLineCap startCap, RTLineCap endCap) {
    m_lineStartCap = startCap; m_lineEndCap = endCap;
}
void GlRenderTarget::setLineJoin(RTLineJoin join, float miterLimit) {
    m_lineJoin = join; m_miterLimit = miterLimit;
}
void GlRenderTarget::setFillStyle(FillStyle style, color_t color) {
    m_fillStyle = style;
    m_fillPatternColor = color;
    m_fillColor = color;
}
void GlRenderTarget::setRasterOp(RasterOp rop) {
    submitBatch();
    m_rasterOp = rop;
}
void GlRenderTarget::setWritingMode(int mode)  { m_writingMode = mode; }

// ============================================================
// Viewport
// ============================================================
void GlRenderTarget::setViewport(int left, int top, int right, int bottom, bool clip) {
    // Projection and scissor are applied when a batch is submitted. Preserve
    // the viewport that was active when already-queued primitives were added.
    submitBatch();
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
    submitBatch();
    bindForDrawing();
    const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint previousScissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    const int left = std::max(0, m_vpLeft);
    const int top = std::max(0, m_vpTop);
    const int right = std::min(m_width, m_vpRight);
    const int bottom = std::min(m_height, m_vpBottom);
    if (left < right && top < bottom) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(left, m_height - bottom, right - left, bottom - top);
        float r, g, b, a;
        color_t_to_rgba(m_bkColor, r, g, b, a);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
        markGpuDirty(clippedRect(left, top, right - left, bottom - top));
    }
    if (scissorWasEnabled) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
    glScissor(previousScissor[0], previousScissor[1], previousScissor[2], previousScissor[3]);
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
    // EGE coordinates are relative to the viewport. Mapping the full target
    // through negative viewport offsets makes logical (0, 0) land at the
    // viewport's physical left/top while preserving target-sized NDC.
    float left   = (float)-m_vpLeft;
    float right  = (float)(m_width - m_vpLeft);
    float bottom = (float)(m_height - m_vpTop);
    float top    = (float)-m_vpTop;
    ortho2D(proj, left, right, bottom, top);

    m_primShader.use();
    m_primShader.setUniformMatrix4("uProj", proj);
    m_projectionDirty = false;
}

void GlRenderTarget::bindForDrawing() {
    // A caller may have edited the pointer returned by getbuffer(). Upload it
    // before rendering more GPU commands so those edits remain observable.
    syncToGpu();

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

    float minimumX = m_vertices.front().x;
    float minimumY = m_vertices.front().y;
    float maximumX = minimumX;
    float maximumY = minimumY;
    for (std::vector<GlVertex>::const_iterator vertex = m_vertices.begin();
         vertex != m_vertices.end(); ++vertex) {
        minimumX = std::min(minimumX, vertex->x);
        minimumY = std::min(minimumY, vertex->y);
        maximumX = std::max(maximumX, vertex->x);
        maximumY = std::max(maximumY, vertex->y);
    }
    int dirtyLeft = static_cast<int>(std::floor(minimumX)) + m_vpLeft;
    int dirtyTop = static_cast<int>(std::floor(minimumY)) + m_vpTop;
    int dirtyRight = static_cast<int>(std::ceil(maximumX)) + m_vpLeft;
    int dirtyBottom = static_cast<int>(std::ceil(maximumY)) + m_vpTop;
    if (dirtyRight <= dirtyLeft) ++dirtyRight;
    if (dirtyBottom <= dirtyTop) ++dirtyBottom;
    PixelRect batchDirty = clippedRect(
        dirtyLeft, dirtyTop, dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
    if (m_vpClip) {
        PixelRect viewportDirty = clippedRect(
            m_vpLeft, m_vpTop, m_vpRight - m_vpLeft, m_vpBottom - m_vpTop);
        if (!batchDirty.empty() && !viewportDirty.empty()) {
            batchDirty.left = std::max(batchDirty.left, viewportDirty.left);
            batchDirty.top = std::max(batchDirty.top, viewportDirty.top);
            batchDirty.right = std::min(batchDirty.right, viewportDirty.right);
            batchDirty.bottom = std::min(batchDirty.bottom, viewportDirty.bottom);
        } else {
            batchDirty = PixelRect();
        }
    }

    bindForDrawing();
    m_primShader.use();
    ensureProjection();
    m_primShader.setUniform1i("uHeight", m_height);
    glLineWidth(m_lineWidth);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GlVertex) * m_vertices.size(), m_vertices.data(), GL_STREAM_DRAW);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean logicOpWasEnabled = glIsEnabled(GL_COLOR_LOGIC_OP);
    GLint previousLogicOp = GL_COPY;
    glGetIntegerv(GL_LOGIC_OP_MODE, &previousLogicOp);
    if (m_rasterOp == ROP_COPY) {
        glDisable(GL_COLOR_LOGIC_OP);
        // Legacy primitive drawing is an opaque copy operation regardless of
        // the alpha byte supplied by older callers, so source-alpha blending
        // must not discard or partially blend these pixels.
        glDisable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(rasterOpToGlLogicOp(m_rasterOp));
    }
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertices.size());
    if (logicOpWasEnabled) glEnable(GL_COLOR_LOGIC_OP);
    else glDisable(GL_COLOR_LOGIC_OP);
    glLogicOp(previousLogicOp);
    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    glBindVertexArray(0);

    m_vertices.clear();
    if (!batchDirty.empty()) {
        markGpuDirty(batchDirty);
    }
}

// ============================================================
// Image blit helpers
// ============================================================

// Sync source RenderTarget's CPU buffer to GPU texture if dirty
static void syncSrcTexture(RenderTarget* src) {
    if (!src) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    // A source image can still have primitives queued in its CPU-side batch.
    // Submit those before sampling the texture, otherwise putimage observes
    // only the last completed GPU state.
    // flush() uploads a CPU-edited off-screen texture and, for the window
    // target, also draws that uploaded texture into the back buffer. Calling
    // syncToGpu() first would clear the state that tells flush() to do the
    // latter, losing direct edits made through getbuffer().
    glSrc->flush();
    if (glSrc->isOnScreen()) {
        glSrc->captureScreenToTexture();
    }
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
                                    int mode, color_t keyColor, bool smooth) {
    drawImageQuadInternal(srcTex, srcW, srcH, srcX, srcY, srcW2, srcH2,
                          dstX, dstY, dstW2, dstH2, angle, centerX, centerY,
                          zoomX, zoomY, mode, keyColor, -1.0f, smooth);
}

void GlRenderTarget::drawImageQuadInternal(GLuint srcTex, int srcW, int srcH,
                                            int srcX, int srcY, int srcW2, int srcH2,
                                            float dstX, float dstY, float dstW2, float dstH2,
                                            float angle, float centerX, float centerY,
                                            float zoomX, float zoomY,
                                            int mode, color_t keyColor,
                                            float alphaOverride, bool smooth,
                                            const float* destinationPoints) {
    if (!m_initialized || srcW <= 0 || srcH <= 0 || srcW2 <= 0 || srcH2 <= 0 || dstW2 <= 0 || dstH2 <= 0) return;

    // Preserve API call ordering when primitive commands are still batched and
    // incorporate any direct CPU-buffer edits before drawing the image.
    submitBatch();
    syncToGpu();
    ensureImageShader();

    // Save current GL state
    GLint prevFbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    GLint prevProg;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    GLint prevVao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
    GLint prevArrayBuffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
    GLint prevActiveTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
    GLint prevTex0, prevTex1;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex1);
    glActiveTexture(prevActiveTexture);
    GLboolean blendWas;
    glGetBooleanv(GL_BLEND, &blendWas);
    GLint prevBlendSrcRgb, prevBlendDstRgb, prevBlendSrcAlpha, prevBlendDstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
    const GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean prevColorMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, prevColorMask);
    GLint prevScissor[4], prevViewport[4];
    glGetIntegerv(GL_SCISSOR_BOX, prevScissor);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Bind destination framebuffer.  Source synchronization may have left an
    // off-screen FBO's color attachment selected; explicitly restore the
    // default back buffer for a window presentation.
    if (m_isOnScreen) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    float pixelPositions[4][2];
    if (destinationPoints) {
        for (int i = 0; i < 4; ++i) {
            pixelPositions[i][0] = destinationPoints[i * 2];
            pixelPositions[i][1] = destinationPoints[i * 2 + 1];
        }
    } else {
        pixelPositions[0][0] = dstX;         pixelPositions[0][1] = dstY;
        pixelPositions[1][0] = dstX + dstW2; pixelPositions[1][1] = dstY;
        pixelPositions[2][0] = dstX + dstW2; pixelPositions[2][1] = dstY + dstH2;
        pixelPositions[3][0] = dstX;         pixelPositions[3][1] = dstY + dstH2;
    }

    // Rotation must happen in EGE pixel space. Rotating NDC directly scales X
    // and Y differently whenever the render target is not square.
    if (!destinationPoints && (angle != 0.0f || zoomX != 1.0f || zoomY != 1.0f)) {
        const float pivotPixelX = dstX + centerX;
        const float pivotPixelY = dstY + centerY;
        const float c = cosf(angle);
        const float s = sinf(angle);
        for (int i = 0; i < 4; ++i) {
            const float dx = (pixelPositions[i][0] - pivotPixelX) * zoomX;
            const float dy = (pixelPositions[i][1] - pivotPixelY) * zoomY;
            pixelPositions[i][0] = pivotPixelX + c * dx - s * dy;
            pixelPositions[i][1] = pivotPixelY + s * dx + c * dy;
        }
    }

    // EGE image coordinates are logical viewport coordinates. Convert each
    // transformed point separately so non-axis-aligned quads remain accurate.
    float positions[4][2];
    float dirtyMinimumX = pixelPositions[0][0] + m_vpLeft;
    float dirtyMinimumY = pixelPositions[0][1] + m_vpTop;
    float dirtyMaximumX = dirtyMinimumX;
    float dirtyMaximumY = dirtyMinimumY;
    for (int i = 0; i < 4; ++i) {
        const float physicalX = pixelPositions[i][0] + m_vpLeft;
        const float physicalY = pixelPositions[i][1] + m_vpTop;
        dirtyMinimumX = std::min(dirtyMinimumX, physicalX);
        dirtyMinimumY = std::min(dirtyMinimumY, physicalY);
        dirtyMaximumX = std::max(dirtyMaximumX, physicalX);
        dirtyMaximumY = std::max(dirtyMaximumY, physicalY);
        positions[i][0] = 2.0f * physicalX / m_width - 1.0f;
        positions[i][1] = -2.0f * physicalY / m_height + 1.0f;
    }
    PixelRect imageDirty = clippedRect(
        static_cast<int>(std::floor(dirtyMinimumX)),
        static_cast<int>(std::floor(dirtyMinimumY)),
        static_cast<int>(std::ceil(dirtyMaximumX)) -
            static_cast<int>(std::floor(dirtyMinimumX)),
        static_cast<int>(std::ceil(dirtyMaximumY)) -
            static_cast<int>(std::floor(dirtyMinimumY)));
    if (m_vpClip && !imageDirty.empty()) {
        const PixelRect viewportDirty = clippedRect(
            m_vpLeft, m_vpTop, m_vpRight - m_vpLeft, m_vpBottom - m_vpTop);
        imageDirty.left = std::max(imageDirty.left, viewportDirty.left);
        imageDirty.top = std::max(imageDirty.top, viewportDirty.top);
        imageDirty.right = std::min(imageDirty.right, viewportDirty.right);
        imageDirty.bottom = std::min(imageDirty.bottom, viewportDirty.bottom);
    }

    const float uLeft = static_cast<float>(srcX) / srcW;
    const float uRight = static_cast<float>(srcX + srcW2) / srcW;
    const float vTop = 1.0f - static_cast<float>(srcY) / srcH;
    const float vBottom = 1.0f - static_cast<float>(srcY + srcH2) / srcH;

    // Interleaved NDC position and UV, top-left first.
    const float quad[16] = {
        positions[0][0], positions[0][1], uLeft,  vTop,
        positions[1][0], positions[1][1], uRight, vTop,
        positions[2][0], positions[2][1], uRight, vBottom,
        positions[3][0], positions[3][1], uLeft,  vBottom,
    };

    // Create VAO + VBO for this draw call
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Set up shader
    m_imageShader.use();

    // Mode uniform
    m_imageShader.setUniform1i("uMode", mode);

    // Key color uniform (normalized RGB)
    float kr = ((keyColor >> 16) & 0xFF) / 255.0f;
    float kg = ((keyColor >>  8) & 0xFF) / 255.0f;
    float kb = ( keyColor        & 0xFF) / 255.0f;
    m_imageShader.setUniform3f("uKeyColor", kr, kg, kb);
    m_imageShader.setUniform1f("uKeyTol", 0.5f / 255.0f);
    m_imageShader.setUniform1f("uAlphaOverride", alphaOverride >= 0.0f ? alphaOverride : 1.0f);

    // Bind source texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    GLint previousMinFilter, previousMagFilter, previousWrapS, previousWrapT;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &previousMinFilter);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &previousMagFilter);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &previousWrapS);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &previousWrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
    if (mode == IMG_COPY || mode == IMG_ZERO_KEY_COPY) {
        // Copy mode: disable blending (source overwrites destination)
        glDisable(GL_BLEND);
    } else if (mode >= 2 && mode <= 4) {
        // ROP2 modes: shader handles compositing, disable OpenGL blending
        glDisable(GL_BLEND);
    } else if (mode == IMG_ALPHA_PREMULTIPLIED) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else if (mode == IMG_COLOR_KEY_COPY || mode == IMG_COLOR_KEY_ALPHA) {
        // colorblend_inline changes RGB only; retain the destination alpha.
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                            GL_ZERO, GL_ONE);
    } else {
        // Straight/opaque source-over compositing.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glViewport(0, 0, m_width, m_height);
    if (m_vpClip) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(m_vpLeft, m_height - m_vpBottom,
                  m_vpRight - m_vpLeft, m_vpBottom - m_vpTop);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    // Draw
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Restore state
    if (!imageDirty.empty()) {
        markGpuDirty(imageDirty);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, previousMinFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, previousMagFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, previousWrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, previousWrapT);

    glBindVertexArray(prevVao);
    glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prevTex0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, prevTex1);
    glActiveTexture(prevActiveTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    if (prevProg) glUseProgram(prevProg);
    if (blendWas) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    glBlendFuncSeparate(prevBlendSrcRgb, prevBlendDstRgb,
                        prevBlendSrcAlpha, prevBlendDstAlpha);
    glColorMask(prevColorMask[0], prevColorMask[1],
                prevColorMask[2], prevColorMask[3]);
    if (scissorWas) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
    glScissor(prevScissor[0], prevScissor[1], prevScissor[2], prevScissor[3]);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

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
                   float r, float g, float b, float a, float pattern = 1.0f,
                   float br = 0.0f, float bg = 0.0f, float bb = 0.0f, float ba = 0.0f) {
    v.push_back(GlVertex(x0, y0, r, g, b, a, pattern, br, bg, bb, ba));
    v.push_back(GlVertex(x1, y1, r, g, b, a, pattern, br, bg, bb, ba));
    v.push_back(GlVertex(x2, y2, r, g, b, a, pattern, br, bg, bb, ba));
}

static void addQuad(std::vector<GlVertex>& v, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                    float r, float g, float b, float a, float pattern = 1.0f,
                    float br = 0.0f, float bg = 0.0f, float bb = 0.0f, float ba = 0.0f) {
    addTri(v, x0, y0, x1, y1, x2, y2, r, g, b, a, pattern, br, bg, bb, ba);
    addTri(v, x0, y0, x2, y2, x3, y3, r, g, b, a, pattern, br, bg, bb, ba);
}

void GlRenderTarget::appendFillTriangle(float x0, float y0, float x1, float y1,
                                        float x2, float y2,
                                        float r, float g, float b, float a) {
    float br, bg, bb, ba;
    color_t_to_rgba(m_bkColor, br, bg, bb, ba);
    addTri(m_vertices, x0, y0, x1, y1, x2, y2, r, g, b, a,
           static_cast<float>(m_fillStyle), br, bg, bb, ba);
}

void GlRenderTarget::appendFillQuad(float x0, float y0, float x1, float y1,
                                    float x2, float y2, float x3, float y3,
                                    float r, float g, float b, float a) {
    float br, bg, bb, ba;
    color_t_to_rgba(m_bkColor, br, bg, bb, ba);
    addQuad(m_vertices, x0, y0, x1, y1, x2, y2, x3, y3, r, g, b, a,
            static_cast<float>(m_fillStyle), br, bg, bb, ba);
}

// ============================================================
// Basic primitives — Phase 1 implementation
// ============================================================
void GlRenderTarget::drawLine(int x1, int y1, int x2, int y2) {
    if (m_lineStyle == LINE_NONE) return;

    // Integer drawing coordinates address pixel centers. Offset the geometric
    // centerline by half a device pixel so a one-pixel vertical/horizontal
    // stroke covers the requested row or column instead of a neighbouring one.
    const float fx1 = x1 + 0.5f, fy1 = y1 + 0.5f;
    const float fx2 = x2 + 0.5f, fy2 = y2 + 0.5f;
    float dx = fx2 - fx1, dy = fy2 - fy1;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.5f) {
        putPixel(x1, y1, m_lineColor);
        return;
    }
    float r, g, b, a;
    color_t_to_rgba(m_lineColor, r, g, b, a);
    float& mat = m_transformStack.back()[0];
    const float hw = m_lineWidth * 0.5f;

    const auto appendDisk = [&](float cx, float cy) {
        float transformedCenterX = cx, transformedCenterY = cy;
        transformPoint(&mat, transformedCenterX, transformedCenterY);
        const int segments = std::max(12, static_cast<int>(std::ceil(m_lineWidth * 3.0f)));
        float previousX = cx + hw, previousY = cy;
        transformPoint(&mat, previousX, previousY);
        for (int i = 1; i <= segments; ++i) {
            const float angle = static_cast<float>(2.0 * M_PI * i / segments);
            float nextX = cx + std::cos(angle) * hw;
            float nextY = cy + std::sin(angle) * hw;
            transformPoint(&mat, nextX, nextY);
            addTri(m_vertices, transformedCenterX, transformedCenterY,
                   previousX, previousY, nextX, nextY, r, g, b, a);
            previousX = nextX;
            previousY = nextY;
        }
    };

    const auto appendSegment = [&](float sx, float sy, float ex, float ey,
                                   RTLineCap startCap, RTLineCap endCap) {
        const float sdx = ex - sx, sdy = ey - sy;
        const float segmentLength = sqrtf(sdx * sdx + sdy * sdy);
        if (segmentLength <= 0.0f) return;
        const float ux = sdx / segmentLength, uy = sdy / segmentLength;
        if (startCap == RT_LINECAP_SQUARE) {
            sx -= ux * hw;
            sy -= uy * hw;
        }
        if (endCap == RT_LINECAP_SQUARE) {
            ex += ux * hw;
            ey += uy * hw;
        }
        const float nx = -uy * hw;
        const float ny =  ux * hw;
        float px[8] = {sx+nx, sy+ny, sx-nx, sy-ny,
                       ex-nx, ey-ny, ex+nx, ey+ny};
        for (int i = 0; i < 4; ++i) {
            transformPoint(&mat, px[i * 2], px[i * 2 + 1]);
        }
        addTri(m_vertices, px[0], px[1], px[2], px[3], px[4], px[5], r, g, b, a);
        addTri(m_vertices, px[0], px[1], px[4], px[5], px[6], px[7], r, g, b, a);
        if (startCap == RT_LINECAP_ROUND) appendDisk(sx, sy);
        if (endCap == RT_LINECAP_ROUND) appendDisk(ex, ey);
    };

    if (m_lineStyle == LINE_SOLID || m_lineStyle == LINE_INSIDE) {
        appendSegment(fx1, fy1, fx2, fy2, m_lineStartCap, m_lineEndCap);
        return;
    }

    float pattern[16] = {};
    const int patternSize = buildLineDashPattern(m_lineStyle, m_linePattern, pattern);
    if (patternSize <= 0) return;

    float offset = 0.0f;
    int patternIndex = 0;
    while (offset < len) {
        const float piece = std::min(pattern[patternIndex], len - offset);
        if ((patternIndex & 1) == 0) {
            const float t0 = offset / len;
            const float t1 = (offset + piece) / len;
            appendSegment(fx1 + dx * t0, fy1 + dy * t0,
                          fx1 + dx * t1, fy1 + dy * t1,
                          m_lineStartCap, m_lineEndCap);
        }
        offset += piece;
        patternIndex = (patternIndex + 1) % patternSize;
    }
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
    const int points[] = {x, y, x + w, y, x + w, y + h, x, y + h};
    drawPolylineInternal(points, 4, true);
}

void GlRenderTarget::fillRect(int x, int y, int w, int h) {
    if (m_fillStyle == FILL_EMPTY) return;
    float r, g, b, a;
    color_t_to_rgba(m_fillColor, r, g, b, a);

    float& mat = m_transformStack.back()[0];
    float pts[4][2] = { {(float)x, (float)y}, {(float)(x+w), (float)y},
                        {(float)(x+w), (float)(y+h)}, {(float)x, (float)(y+h)} };
    for (int i = 0; i < 4; i++)
        transformPoint(&mat, pts[i][0], pts[i][1]);

    appendFillQuad(pts[0][0], pts[0][1], pts[1][0], pts[1][1],
                   pts[2][0], pts[2][1], pts[3][0], pts[3][1], r, g, b, a);
}

void GlRenderTarget::drawCircle(int x, int y, int r) {
    drawEllipse(x - r, y - r, 0, 360, 2 * r, 2 * r);
}

void GlRenderTarget::fillCircle(int x, int y, int r) {
    fillEllipse(x - r, y - r, 0, 360, 2 * r, 2 * r);
}

void GlRenderTarget::drawEllipse(int x, int y, int sa, int ea, int rx, int ry) {
    if (m_lineStyle == LINE_NONE) return;
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    int segments = std::max(16, (int)(rx + ry) / 4);

    float previousX = cx + rdx * cosf(startRad);
    float previousY = cy - rdy * sinf(startRad);
    float pattern[16] = {};
    const int patternSize = buildLineDashPattern(m_lineStyle, m_linePattern, pattern);
    int patternIndex = 0;
    float patternRemaining = patternSize > 0 ? pattern[0] : 0.0f;
    const LineStyle savedStyle = m_lineStyle;
    if (patternSize > 0) m_lineStyle = LINE_SOLID;
    for (int i = 1; i <= segments; i++) {
        float t = startRad + (endRad - startRad) * i / segments;
        float px = cx + rdx * cosf(t), py = cy - rdy * sinf(t);
        if (patternSize <= 0) {
            drawLineF(previousX, previousY, px, py);
        } else {
            const float segmentDx = px - previousX;
            const float segmentDy = py - previousY;
            const float segmentLength = std::sqrt(segmentDx * segmentDx + segmentDy * segmentDy);
            float consumed = 0.0f;
            while (consumed + 1e-5f < segmentLength) {
                const float amount = std::min(patternRemaining, segmentLength - consumed);
                if ((patternIndex & 1) == 0 && amount > 1e-5f) {
                    const float begin = consumed / segmentLength;
                    const float end = (consumed + amount) / segmentLength;
                    drawLineF(previousX + segmentDx * begin, previousY + segmentDy * begin,
                              previousX + segmentDx * end, previousY + segmentDy * end);
                }
                consumed += amount;
                patternRemaining -= amount;
                if (patternRemaining <= 1e-5f) {
                    patternIndex = (patternIndex + 1) % patternSize;
                    patternRemaining = pattern[patternIndex];
                }
            }
        }
        previousX = px;
        previousY = py;
    }
    m_lineStyle = savedStyle;
}

void GlRenderTarget::fillEllipse(int x, int y, int sa, int ea, int rx, int ry) {
    if (m_fillStyle == FILL_EMPTY) return;
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
        float px = cx + rdx * cosf(t), py = cy - rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) {
            appendFillTriangle(cpx, cpy, prevTx, prevTy, tx, ty, r, g, b, a);
        }
        prevTx = tx; prevTy = ty;
    }
}

void GlRenderTarget::drawSector(int x, int y, int sa, int ea, int rx, int ry) {
    if (m_lineStyle == LINE_NONE) return;
    // Sector = arc + two radial lines to center (outline only).
    // Route every edge through the normal stroke path so line styles, caps,
    // transforms and raster operations stay consistent with lines/ellipses.
    drawEllipse(x, y, sa, ea, rx, ry);
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    drawLineF(cx, cy, cx + rdx * cosf(startRad), cy - rdy * sinf(startRad));
    drawLineF(cx, cy, cx + rdx * cosf(endRad), cy - rdy * sinf(endRad));
}

void GlRenderTarget::fillSector(int x, int y, int sa, int ea, int rx, int ry) {
    if (m_fillStyle == FILL_EMPTY) return;
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
        float px = cx + rdx * cosf(t), py = cy - rdy * sinf(t);
        float tx = px, ty = py;
        transformPoint(&mat, tx, ty);
        if (i > 0) appendFillTriangle(cpx, cpy, prevTx, prevTy, tx, ty, r, g, b, a);
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
    drawEllipse(x, y, sa, ea, rx, ry);
}

void GlRenderTarget::drawChord(int x, int y, int sa, int ea, int rx, int ry) {
    // Chord = arc + closing line between endpoints
    drawArc(x, y, sa, ea, rx, ry);
    float cx = x + rx * 0.5f, cy = y + ry * 0.5f;
    float rdx = rx * 0.5f, rdy = ry * 0.5f;
    float startRad = sa * M_PI / 180.0f;
    float endRad   = ea * M_PI / 180.0f;
    float px0 = cx + rdx * cosf(startRad), py0 = cy - rdy * sinf(startRad);
    float px1 = cx + rdx * cosf(endRad), py1 = cy - rdy * sinf(endRad);
    drawLine((int)px0, (int)py0, (int)px1, (int)py1);
}

void GlRenderTarget::drawPolygon(const int* points, int count) {
    drawPolylineInternal(points, count, true);
}

void GlRenderTarget::fillPolygon(const int* points, int count) {
    if (!points || count < 3 || m_fillStyle == FILL_EMPTY) return;
    float r, g, b, a;
    color_t_to_rgba(m_fillColor, r, g, b, a);
    float& mat = m_transformStack.back()[0];

    std::vector<float> polygon;
    polygon.reserve(static_cast<size_t>(count) * 2);
    for (int i = 0; i < count; ++i) {
        const float x = (float)points[i * 2];
        const float y = (float)points[i * 2 + 1];
        if (!polygon.empty() && polygon[polygon.size() - 2] == x && polygon.back() == y) continue;
        polygon.push_back(x);
        polygon.push_back(y);
    }
    if (polygon.size() >= 6 && polygon[0] == polygon[polygon.size() - 2] && polygon[1] == polygon.back()) {
        polygon.resize(polygon.size() - 2);
    }
    const int vertexCount = (int)polygon.size() / 2;
    if (vertexCount < 3) return;

    double signedArea = 0.0;
    for (int i = 0; i < vertexCount; ++i) {
        const int next = (i + 1) % vertexCount;
        signedArea += polygon[i * 2] * polygon[next * 2 + 1] -
                      polygon[next * 2] * polygon[i * 2 + 1];
    }
    const float orientation = signedArea >= 0.0 ? 1.0f : -1.0f;
    std::vector<int> remaining(vertexCount);
    for (int i = 0; i < vertexCount; ++i) remaining[i] = i;

    const auto cross = [&](int ia, int ib, int ic) {
        const float ax = polygon[ia * 2], ay = polygon[ia * 2 + 1];
        const float bx = polygon[ib * 2], by = polygon[ib * 2 + 1];
        const float cx = polygon[ic * 2], cy = polygon[ic * 2 + 1];
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
    };
    const auto pointInTriangle = [&](int point, int ia, int ib, int ic) {
        return cross(ia, ib, point) * orientation >= -1e-5f &&
               cross(ib, ic, point) * orientation >= -1e-5f &&
               cross(ic, ia, point) * orientation >= -1e-5f;
    };
    const auto emitTriangle = [&](int ia, int ib, int ic) {
        float ax = polygon[ia * 2], ay = polygon[ia * 2 + 1];
        float bx = polygon[ib * 2], by = polygon[ib * 2 + 1];
        float cx = polygon[ic * 2], cy = polygon[ic * 2 + 1];
        transformPoint(&mat, ax, ay);
        transformPoint(&mat, bx, by);
        transformPoint(&mat, cx, cy);
        appendFillTriangle(ax, ay, bx, by, cx, cy, r, g, b, a);
    };

    int guard = vertexCount * vertexCount;
    while (remaining.size() > 3 && guard-- > 0) {
        bool clippedEar = false;
        for (size_t i = 0; i < remaining.size(); ++i) {
            const int previous = remaining[(i + remaining.size() - 1) % remaining.size()];
            const int current = remaining[i];
            const int next = remaining[(i + 1) % remaining.size()];
            if (cross(previous, current, next) * orientation <= 1e-5f) continue;
            bool containsVertex = false;
            for (int candidate : remaining) {
                if (candidate != previous && candidate != current && candidate != next &&
                    pointInTriangle(candidate, previous, current, next)) {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex) continue;
            emitTriangle(previous, current, next);
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            clippedEar = true;
            break;
        }
        if (!clippedEar) break;
    }
    if (remaining.size() == 3) {
        emitTriangle(remaining[0], remaining[1], remaining[2]);
    }
}

void GlRenderTarget::drawPolyline(const int* points, int count) {
    drawPolylineInternal(points, count, false);
}

void GlRenderTarget::drawPolylineInternal(const int* points, int count, bool closed) {
    if (!points || count < 2 || m_lineStyle == LINE_NONE) return;

    const RTLineCap savedStartCap = m_lineStartCap;
    const RTLineCap savedEndCap = m_lineEndCap;
    const int segmentCount = closed ? count : count - 1;
    for (int i = 0; i < segmentCount; ++i) {
        const int next = (i + 1) % count;
        m_lineStartCap = (!closed && i == 0) ? savedStartCap : RT_LINECAP_FLAT;
        m_lineEndCap = (!closed && i + 1 == segmentCount) ? savedEndCap : RT_LINECAP_FLAT;
        drawLine(points[i * 2], points[i * 2 + 1],
                 points[next * 2], points[next * 2 + 1]);
    }
    m_lineStartCap = savedStartCap;
    m_lineEndCap = savedEndCap;

    if (m_lineStyle != LINE_SOLID && m_lineStyle != LINE_INSIDE) return;

    float r, g, b, a;
    color_t_to_rgba(m_lineColor, r, g, b, a);
    float& matrix = m_transformStack.back()[0];
    const float halfWidth = m_lineWidth * 0.5f;
    const int firstJoin = closed ? 0 : 1;
    const int endJoin = closed ? count : count - 1;

    const auto appendJoinTriangle = [&](float ax, float ay, float bx, float by,
                                        float cx, float cy) {
        transformPoint(&matrix, ax, ay);
        transformPoint(&matrix, bx, by);
        transformPoint(&matrix, cx, cy);
        addTri(m_vertices, ax, ay, bx, by, cx, cy, r, g, b, a);
    };
    const auto appendRoundJoin = [&](float cx, float cy) {
        float transformedCenterX = cx, transformedCenterY = cy;
        transformPoint(&matrix, transformedCenterX, transformedCenterY);
        const int segments = std::max(12, static_cast<int>(std::ceil(m_lineWidth * 3.0f)));
        float previousX = cx + halfWidth, previousY = cy;
        transformPoint(&matrix, previousX, previousY);
        for (int i = 1; i <= segments; ++i) {
            const float angle = static_cast<float>(2.0 * M_PI * i / segments);
            float nextX = cx + std::cos(angle) * halfWidth;
            float nextY = cy + std::sin(angle) * halfWidth;
            transformPoint(&matrix, nextX, nextY);
            addTri(m_vertices, transformedCenterX, transformedCenterY,
                   previousX, previousY, nextX, nextY, r, g, b, a);
            previousX = nextX;
            previousY = nextY;
        }
    };

    for (int i = firstJoin; i < endJoin; ++i) {
        const int previous = (i + count - 1) % count;
        const int next = (i + 1) % count;
        const float centerX = points[i * 2] + 0.5f;
        const float centerY = points[i * 2 + 1] + 0.5f;
        float incomingX = centerX - (points[previous * 2] + 0.5f);
        float incomingY = centerY - (points[previous * 2 + 1] + 0.5f);
        float outgoingX = (points[next * 2] + 0.5f) - centerX;
        float outgoingY = (points[next * 2 + 1] + 0.5f) - centerY;
        const float incomingLength = std::sqrt(incomingX * incomingX + incomingY * incomingY);
        const float outgoingLength = std::sqrt(outgoingX * outgoingX + outgoingY * outgoingY);
        if (incomingLength <= 1e-5f || outgoingLength <= 1e-5f) continue;
        incomingX /= incomingLength;
        incomingY /= incomingLength;
        outgoingX /= outgoingLength;
        outgoingY /= outgoingLength;
        const float cross = incomingX * outgoingY - incomingY * outgoingX;
        if (std::abs(cross) <= 1e-5f) continue;

        if (m_lineJoin == RT_LINEJOIN_ROUND) {
            appendRoundJoin(centerX, centerY);
            continue;
        }

        const float side = cross < 0.0f ? 1.0f : -1.0f;
        const float firstX = centerX - incomingY * halfWidth * side;
        const float firstY = centerY + incomingX * halfWidth * side;
        const float secondX = centerX - outgoingY * halfWidth * side;
        const float secondY = centerY + outgoingX * halfWidth * side;

        if (m_lineJoin == RT_LINEJOIN_MITER) {
            const float deltaX = secondX - firstX;
            const float deltaY = secondY - firstY;
            const float parameter = (deltaX * outgoingY - deltaY * outgoingX) / cross;
            const float miterX = firstX + incomingX * parameter;
            const float miterY = firstY + incomingY * parameter;
            const float miterDistance = std::sqrt((miterX - centerX) * (miterX - centerX) +
                                                  (miterY - centerY) * (miterY - centerY));
            if (halfWidth > 0.0f && miterDistance / halfWidth <= m_miterLimit) {
                appendJoinTriangle(centerX, centerY, firstX, firstY, miterX, miterY);
                appendJoinTriangle(centerX, centerY, miterX, miterY, secondX, secondY);
                continue;
            }
        }
        appendJoinTriangle(centerX, centerY, firstX, firstY, secondX, secondY);
    }
}

// ============================================================
// Rounded rectangles
// ============================================================
void GlRenderTarget::drawRoundRect(int x, int y, int w, int h, int ew, int eh) {
    if (w <= 0 || h <= 0) return;
    const float rx = std::min(std::abs(ew) * 0.5f, w * 0.5f);
    const float ry = std::min(std::abs(eh) * 0.5f, h * 0.5f);
    if (rx <= 0.0f || ry <= 0.0f) {
        drawRect(x, y, w, h);
        return;
    }

    const float centers[4][2] = {
        {x + rx, y + ry}, {x + w - rx, y + ry},
        {x + w - rx, y + h - ry}, {x + rx, y + h - ry}
    };
    const float starts[4] = {(float)M_PI, (float)(M_PI * 1.5), 0.0f, (float)(M_PI * 0.5)};
    const int segments = std::max(4, (int)std::ceil(std::max(rx, ry) * 0.75f));
    std::vector<float> boundary;
    boundary.reserve((segments + 1) * 8);
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= segments; ++i) {
            const float angle = starts[corner] + (float)(M_PI * 0.5) * i / segments;
            boundary.push_back(centers[corner][0] + rx * cosf(angle));
            boundary.push_back(centers[corner][1] + ry * sinf(angle));
        }
    }
    const int pointCount = (int)boundary.size() / 2;
    for (int i = 0; i < pointCount; ++i) {
        const int next = (i + 1) % pointCount;
        drawLineF(boundary[i * 2], boundary[i * 2 + 1],
                  boundary[next * 2], boundary[next * 2 + 1]);
    }
}

void GlRenderTarget::fillRoundRect(int x, int y, int w, int h, int ew, int eh) {
    if (w <= 0 || h <= 0 || m_fillStyle == FILL_EMPTY) return;
    const float rx = std::min(std::abs(ew) * 0.5f, w * 0.5f);
    const float ry = std::min(std::abs(eh) * 0.5f, h * 0.5f);
    if (rx <= 0.0f || ry <= 0.0f) {
        fillRect(x, y, w, h);
        return;
    }

    float r, g, b, a;
    color_t_to_rgba(m_fillColor, r, g, b, a);
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    float& mat = m_transformStack.back()[0];
    transformPoint(&mat, cx, cy);

    const float centers[4][2] = {
        {x + rx, y + ry}, {x + w - rx, y + ry},
        {x + w - rx, y + h - ry}, {x + rx, y + h - ry}
    };
    const float starts[4] = {(float)M_PI, (float)(M_PI * 1.5), 0.0f, (float)(M_PI * 0.5)};
    const int segments = std::max(4, (int)std::ceil(std::max(rx, ry) * 0.75f));
    std::vector<float> boundary;
    boundary.reserve((segments + 1) * 8);
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= segments; ++i) {
            const float angle = starts[corner] + (float)(M_PI * 0.5) * i / segments;
            float px = centers[corner][0] + rx * cosf(angle);
            float py = centers[corner][1] + ry * sinf(angle);
            transformPoint(&mat, px, py);
            boundary.push_back(px);
            boundary.push_back(py);
        }
    }
    const int pointCount = (int)boundary.size() / 2;
    for (int i = 0; i < pointCount; ++i) {
        const int next = (i + 1) % pointCount;
        appendFillTriangle(cx, cy,
                           boundary[i * 2], boundary[i * 2 + 1],
                           boundary[next * 2], boundary[next * 2 + 1], r, g, b, a);
    }
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
    float& mat = m_transformStack.back()[0];
    // Integer EGE coordinates identify the pixel whose device-space cell is
    // [x, x + 1) × [y, y + 1).  Centering the quad on the integer coordinate
    // puts all sample points on triangle edges and can rasterize no fragment.
    float pts[4][2] = {
        {(float)x, (float)y}, {(float)(x + 1), (float)y},
        {(float)(x + 1), (float)(y + 1)}, {(float)x, (float)(y + 1)}
    };
    for (int i = 0; i < 4; ++i) {
        transformPoint(&mat, pts[i][0], pts[i][1]);
    }
    addQuad(m_vertices, pts[0][0], pts[0][1], pts[1][0], pts[1][1],
            pts[2][0], pts[2][1], pts[3][0], pts[3][1], r, g, b, a);
}

color_t GlRenderTarget::getPixel(int x, int y) const {
    const int physicalX = x + m_vpLeft;
    const int physicalY = y + m_vpTop;
    if (physicalX < 0 || physicalX >= m_width || physicalY < 0 || physicalY >= m_height) return 0;
    const color_t* pixels = getPixelBuffer();
    return pixels ? pixels[physicalY * m_width + physicalX] : 0;
}

void GlRenderTarget::putPixelAlpha(int x, int y, color_t color) { putPixel(x, y, color); }
void GlRenderTarget::putPixelSaveAlpha(int x, int y, color_t color) { putPixel(x, y, color); }
void GlRenderTarget::putPixelAlphaBlend(int x, int y, color_t color, unsigned char alphaFactor) { putPixel(x, y, color); }

void GlRenderTarget::putPixels(int count, const int* points) {
    for (int i = 0; i < count; i++)
        putPixel(points[i*2], points[i*2+1], m_lineColor);
}

void GlRenderTarget::floodFill(int x, int y, color_t borderColor) {
    if (m_fillStyle == FILL_EMPTY) return;
    const int seedX = x + m_vpLeft;
    const int seedY = y + m_vpTop;
    const int left = m_vpClip ? std::max(0, m_vpLeft) : 0;
    const int top = m_vpClip ? std::max(0, m_vpTop) : 0;
    const int right = m_vpClip ? std::min(m_width, m_vpRight) : m_width;
    const int bottom = m_vpClip ? std::min(m_height, m_vpBottom) : m_height;
    if (seedX < left || seedX >= right || seedY < top || seedY >= bottom) return;

    color_t* pixels = getPixelBufferForWrite(left, top, right - left, bottom - top);
    if (!pixels) return;
    const color_t borderRgb = borderColor & 0x00FFFFFFU;
    const color_t fillRgb = m_fillColor & 0x00FFFFFFU;
    const color_t seedRgb = pixels[seedY * m_width + seedX] & 0x00FFFFFFU;
    if (seedRgb == borderRgb || (m_fillStyle == FILL_SOLID && seedRgb == fillRgb)) return;

    std::vector<int> stack;
    std::vector<unsigned char> visited(static_cast<size_t>(m_width) * m_height, 0);
    stack.push_back(seedY * m_width + seedX);
    while (!stack.empty()) {
        const int index = stack.back();
        stack.pop_back();
        if (visited[index]) continue;
        visited[index] = 1;
        const int px = index % m_width;
        const int py = index / m_width;
        const color_t currentRgb = pixels[index] & 0x00FFFFFFU;
        if (currentRgb == borderRgb) continue;
        pixels[index] = fillPatternUsesForeground(m_fillStyle, px, py) ? m_fillColor : m_bkColor;
        if (px > left) stack.push_back(index - 1);
        if (px + 1 < right) stack.push_back(index + 1);
        if (py > top) stack.push_back(index - m_width);
        if (py + 1 < bottom) stack.push_back(index + m_width);
    }
}

void GlRenderTarget::floodFillSurface(int x, int y, color_t surfaceColor) {
    if (m_fillStyle == FILL_EMPTY) return;
    const int seedX = x + m_vpLeft;
    const int seedY = y + m_vpTop;
    const int left = m_vpClip ? std::max(0, m_vpLeft) : 0;
    const int top = m_vpClip ? std::max(0, m_vpTop) : 0;
    const int right = m_vpClip ? std::min(m_width, m_vpRight) : m_width;
    const int bottom = m_vpClip ? std::min(m_height, m_vpBottom) : m_height;
    if (seedX < left || seedX >= right || seedY < top || seedY >= bottom) return;

    color_t* pixels = getPixelBufferForWrite(left, top, right - left, bottom - top);
    if (!pixels) return;
    const color_t surfaceRgb = surfaceColor & 0x00FFFFFFU;
    const color_t fillRgb = m_fillColor & 0x00FFFFFFU;
    if ((pixels[seedY * m_width + seedX] & 0x00FFFFFFU) != surfaceRgb ||
        (m_fillStyle == FILL_SOLID && surfaceRgb == fillRgb)) {
        return;
    }

    std::vector<int> stack(1, seedY * m_width + seedX);
    std::vector<unsigned char> visited(static_cast<size_t>(m_width) * m_height, 0);
    while (!stack.empty()) {
        const int index = stack.back();
        stack.pop_back();
        if (visited[index]) continue;
        visited[index] = 1;
        if ((pixels[index] & 0x00FFFFFFU) != surfaceRgb) continue;

        const int px = index % m_width;
        const int py = index / m_width;
        pixels[index] = fillPatternUsesForeground(m_fillStyle, px, py) ? m_fillColor : m_bkColor;
        if (px > left) stack.push_back(index - 1);
        if (px + 1 < right) stack.push_back(index + 1);
        if (py > top) stack.push_back(index - m_width);
        if (py + 1 < bottom) stack.push_back(index + m_width);
    }
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

    markGpuDirtyFull();
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
    drawImageQuad(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                  srcX, srcY, w, h, dstX, dstY, w, h,
                  0, 0, 0, 1.0f, 1.0f, 0, 0);
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
                    unsigned char alpha, ImageAlphaFormat format, bool smooth) {
    if (!src || alpha == 0 || dstW <= 0 || dstH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    const float af = alpha / 255.0f;
    int mode = IMG_ALPHA_PREMULTIPLIED;
    if (format == IMAGE_ALPHA_STRAIGHT) mode = IMG_ALPHA_STRAIGHT;
    else if (format == IMAGE_ALPHA_OPAQUE) mode = IMG_ALPHA_OPAQUE;
    drawImageQuadInternal(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                          srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH,
                          0, 0, 0, 1.0f, 1.0f, mode, 0, af, smooth);
}

void GlRenderTarget::alphaTransparent(int dstX, int dstY, RenderTarget* src,
                          int srcX, int srcY, int w, int h,
                          color_t transparentColor, unsigned char alpha) {
    if (!src || alpha == 0 || w <= 0 || h <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    const int mode = alpha == 255 ? IMG_COLOR_KEY_COPY : IMG_COLOR_KEY_ALPHA;
    drawImageQuadInternal(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                          srcX, srcY, w, h, dstX, dstY, w, h,
                          0, 0, 0, 1.0f, 1.0f, mode,
                          transparentColor, alpha / 255.0f);
}

void GlRenderTarget::withAlpha(int dstX, int dstY, int dstW, int dstH,
                   RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                   bool smooth) {
    if (!src || dstW <= 0 || dstH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    // withAlpha uses per-pixel alpha from the source (PRGB32 pre-multiplied)
    drawImageQuadInternal(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                          srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH,
                          0, 0, 0, 1.0f, 1.0f, IMG_ALPHA_PREMULTIPLIED, 0,
                          -1.0f, smooth);
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
                          0, 0, 0, 1.0f, 1.0f, IMG_ALPHA_PREMULTIPLIED, 0, af);
}

void GlRenderTarget::rotateBlend(int dstX, int dstY, int dstW, int dstH,
                     RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                     float angle, float centerX, float centerY,
                     bool transparent, int alpha, bool smooth) {
    rotateZoomBlend(dstX, dstY, dstW, dstH, src, srcX, srcY, srcW, srcH,
                    angle, centerX, centerY, 1.0f, 1.0f,
                    transparent, alpha, smooth);
}

void GlRenderTarget::rotateZoomBlend(int dstX, int dstY, int dstW, int dstH,
                         RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                         float angle, float centerX, float centerY,
                         float zoomX, float zoomY,
                         bool transparent, int alpha, bool smooth) {
    if (!src || dstW <= 0 || dstH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    const bool useGlobalAlpha = alpha >= 0 && alpha < 256;
    const int mode = transparent
        ? (useGlobalAlpha ? IMG_ZERO_KEY_ALPHA : IMG_ZERO_KEY_COPY)
        : (useGlobalAlpha ? IMG_ALPHA_OPAQUE : IMG_COPY);
    const float globalAlpha = useGlobalAlpha ? alpha / 255.0f : 1.0f;
    // Public xDest/yDest is the destination location of the selected source
    // pivot, not the destination top-left corner.
    const float topLeftX = dstX - centerX;
    const float topLeftY = dstY - centerY;
    drawImageQuadInternal(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                          srcX, srcY, srcW, srcH,
                          topLeftX, topLeftY, dstW, dstH,
                          angle, centerX, centerY, zoomX, zoomY,
                          mode, 0, globalAlpha, smooth);
}

void GlRenderTarget::blitAffine(RenderTarget* src, int srcX, int srcY, int srcW, int srcH,
                                const float* destinationPoints, bool premultipliedAlpha,
                                bool smooth) {
    if (!src || !destinationPoints || srcW <= 0 || srcH <= 0) return;
    GlRenderTarget* glSrc = dynamic_cast<GlRenderTarget*>(src);
    if (!glSrc) return;
    syncSrcTexture(glSrc);
    drawImageQuadInternal(glSrc->m_texture, glSrc->m_width, glSrc->m_height,
                          srcX, srcY, srcW, srcH, 0.0f, 0.0f,
                          static_cast<float>(srcW), static_cast<float>(srcH),
                          0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                          premultipliedAlpha ? IMG_ALPHA_PREMULTIPLIED : IMG_COPY,
                          0, 1.0f, smooth, destinationPoints);
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
    markGpuDirty(clippedRect(dstX, dstY, w, h));
}

// ============================================================
// Text — Phase 4 implementation
// ============================================================

void GlRenderTarget::setFont(int height, int width, const char* face,
                             int escapement, int orientation, int weight,
                             bool italic, bool underline, bool strikeout) {
    // Store font configuration
    m_fontConfig.height = height != 0 ? height : 16;
    m_fontConfig.width = width;
    m_fontConfig.escapement = escapement;
    m_fontConfig.orientation = orientation;
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
    m_glyphAtlas.loadFont(m_fontConfig.face, m_fontConfig.height, m_fontConfig.width,
                          m_fontConfig.weight, m_fontConfig.italic);
}

void GlRenderTarget::getFont(int* height, int* width, char* face, int faceCapacity,
                             int* escapement, int* orientation, int* weight,
                             bool* italic, bool* underline, bool* strikeout) const {
    if (height) *height = m_fontConfig.height;
    if (width) *width = m_fontConfig.width;
    if (face && faceCapacity > 0) {
        strncpy(face, m_fontConfig.face, static_cast<size_t>(faceCapacity - 1));
        face[faceCapacity - 1] = '\0';
    }
    if (escapement) *escapement = m_fontConfig.escapement;
    if (orientation) *orientation = m_fontConfig.orientation;
    if (weight) *weight = m_fontConfig.weight;
    if (italic) *italic = m_fontConfig.italic;
    if (underline) *underline = m_fontConfig.underline;
    if (strikeout) *strikeout = m_fontConfig.strikeout;
}

void GlRenderTarget::setTextJustify(TextHAlign h, TextVAlign v) {
    m_hAlign = h;
    m_vAlign = v;
}

// Render a single glyph quad using the text shader
void GlRenderTarget::drawGlyphTexture(GLuint tex, int texW, int texH,
                                      int srcX, int srcY, int srcW, int srcH,
                                      float dstX, float dstY, float dstW, float dstH,
                                      float angle, float r, float g, float b, float a) {
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return;
    if (!m_initialized) return;

    submitBatch();
    syncToGpu();
    ensureTextShader();

    // Save GL state
    GLint prevFbo, prevProg, prevVao, prevArrayBuffer, prevActiveTexture;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
    GLint prevTex0;
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex0);
    glActiveTexture(prevActiveTexture);
    GLboolean blendWas;
    glGetBooleanv(GL_BLEND, &blendWas);
    GLint prevBlendSrcRgb, prevBlendDstRgb, prevBlendSrcAlpha, prevBlendDstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
    const GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
    GLint prevScissor[4], prevViewport[4];
    glGetIntegerv(GL_SCISSOR_BOX, prevScissor);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Bind destination FBO
    if (m_isOnScreen) glBindFramebuffer(GL_FRAMEBUFFER, 0);
    else glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    float pixelPositions[4][2] = {
        {dstX, dstY}, {dstX + dstW, dstY},
        {dstX + dstW, dstY + dstH}, {dstX, dstY + dstH}};
    if (angle != 0.0f) {
        const float c = cosf(angle);
        const float s = sinf(angle);
        for (int i = 0; i < 4; ++i) {
            const float dx = pixelPositions[i][0] - dstX;
            const float dy = pixelPositions[i][1] - dstY;
            pixelPositions[i][0] = dstX + c * dx - s * dy;
            pixelPositions[i][1] = dstY + s * dx + c * dy;
        }
    }
    float positions[4][2];
    for (int i = 0; i < 4; ++i) {
        const float physicalX = pixelPositions[i][0] + m_vpLeft;
        const float physicalY = pixelPositions[i][1] + m_vpTop;
        positions[i][0] = 2.0f * physicalX / m_width - 1.0f;
        positions[i][1] = -2.0f * physicalY / m_height + 1.0f;
    }

    // Compute UVs for the glyph sub-rect in the atlas
    float uL = (float)srcX / texW;
    float uR = (float)(srcX + srcW) / texW;
    float vT = (float)srcY / texH;  // top-left origin for texture
    float vB = (float)(srcY + srcH) / texH;

    // Interleaved vertex data: 2 pos (NDC) + 2 UV per vertex
    float verts[16] = {
        positions[0][0], positions[0][1], uL, vT,
        positions[1][0], positions[1][1], uR, vT,
        positions[2][0], positions[2][1], uR, vB,
        positions[3][0], positions[3][1], uL, vB,
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
    if (m_vpClip) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(m_vpLeft, m_height - m_vpBottom,
                  m_vpRight - m_vpLeft, m_vpBottom - m_vpTop);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Restore state
    glBindVertexArray(prevVao);
    glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prevTex0);
    glActiveTexture(prevActiveTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glUseProgram(prevProg);
    if (blendWas) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    glBlendFuncSeparate(prevBlendSrcRgb, prevBlendDstRgb,
                        prevBlendSrcAlpha, prevBlendDstAlpha);
    if (scissorWas) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
    glScissor(prevScissor[0], prevScissor[1], prevScissor[2], prevScissor[3]);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    // Glyph transforms can rotate the destination. Until glyph bounds are
    // accumulated here, retain the conservative full-target fallback.
    markGpuDirtyFull();
}

void GlRenderTarget::ensureTextShader() {
    if (m_textShaderReady) return;
    m_textShader.compileVertex(g_textVertSrc);
    m_textShader.compileFragment(g_textFragSrc);
    m_textShader.link();
    m_textShaderReady = true;
}

void GlRenderTarget::renderCodepoints(float x, float y, const std::vector<uint32_t>& codepoints) {
    if (codepoints.empty() || !m_glyphAtlas.isLoaded()) return;
    GlyphAtlas& atlas = m_glyphAtlas;
    const FontConfig& fc = m_fontConfig;

    constexpr float kPi = 3.14159265358979323846f;
    const float baselineAngle = -fc.escapement / 10.0f * kPi / 180.0f;
    // In Windows' compatible graphics mode an escapement rotates the glyphs
    // as well as the baseline when no independent orientation is supplied.
    const int glyphOrientation = fc.orientation != 0 ? fc.orientation : fc.escapement;
    const float glyphAngle = -glyphOrientation / 10.0f * kPi / 180.0f;
    const float cosA = cosf(baselineAngle);
    const float sinA = sinf(baselineAngle);

    float totalWidth = 0;
    for (uint32_t codepoint : codepoints) {
        const GlyphInfo gi = atlas.ensureGlyph(codepoint);
        totalWidth += gi.advance > 0 ? gi.advance : atlas.getAscent() * 0.5f;
    }

    float startX = 0;
    switch (m_hAlign) {
        case TEXT_LEFT:   startX = 0; break;
        case TEXT_CENTER: startX = -totalWidth * 0.5f; break;
        case TEXT_RIGHT:  startX = -totalWidth; break;
    }

    const float textHeight = static_cast<float>(atlas.getAscent() - atlas.getDescent());
    float top = 0;
    switch (m_vAlign) {
        case TEXT_TOP:    top = 0; break;
        case TEXT_MIDDLE: top = -textHeight * 0.5f; break;
        case TEXT_BOTTOM: top = -textHeight; break;
    }
    const float baseline = top + atlas.getAscent();

    float tr, tg, tb, ta;
    color_t_to_rgba(m_textColor, tr, tg, tb, ta);

    auto transformedPoint = [&](float localX, float localY, float& outX, float& outY) {
        outX = x + cosA * localX - sinA * localY;
        outY = y + sinA * localX + cosA * localY;
    };
    auto appendSolidQuad = [&](float left, float quadTop, float right, float bottom, color_t color) {
        float px[4], py[4];
        transformedPoint(left, quadTop, px[0], py[0]);
        transformedPoint(right, quadTop, px[1], py[1]);
        transformedPoint(right, bottom, px[2], py[2]);
        transformedPoint(left, bottom, px[3], py[3]);
        float r, g, b, a;
        color_t_to_rgba(color, r, g, b, a);
        addQuad(m_vertices, px[0], py[0], px[1], py[1], px[2], py[2], px[3], py[3], r, g, b, a);
    };

    if (m_bkOpaque && totalWidth > 0 && textHeight > 0) {
        appendSolidQuad(startX, top, startX + totalWidth, top + textHeight, m_bkColor);
    }

    float cursorX = startX;
    for (uint32_t codepoint : codepoints) {
        const GlyphInfo gi = atlas.ensureGlyph(codepoint);

        if (gi.valid) {
            const float gx = cursorX + gi.bearingX;
            const float gy = baseline + gi.bearingY;
            float rx, ry;
            transformedPoint(gx, gy, rx, ry);
            drawGlyphTexture(
                atlas.getTexture(), atlas.getAtlasSize(), atlas.getAtlasSize(),
                gi.atlasX, gi.atlasY, gi.width, gi.height,
                rx, ry, static_cast<float>(gi.width), static_cast<float>(gi.height),
                glyphAngle, tr, tg, tb, ta
            );
        }
        cursorX += gi.advance > 0 ? gi.advance : atlas.getAscent() * 0.5f;
    }

    const float decorationThickness = std::max(1.0f, textHeight / 14.0f);
    if (fc.underline) {
        const float underlineY = baseline + std::max(1.0f, -atlas.getDescent() * 0.2f);
        appendSolidQuad(startX, underlineY, startX + totalWidth,
                        underlineY + decorationThickness, m_textColor);
    }
    if (fc.strikeout) {
        const float strikeY = top + atlas.getAscent() * 0.55f;
        appendSolidQuad(startX, strikeY, startX + totalWidth,
                        strikeY + decorationThickness, m_textColor);
    }
}

void GlRenderTarget::renderText(float x, float y, const wchar_t* text) {
    renderCodepoints(x, y, decodeWide(text));
}

void GlRenderTarget::drawText(float x, float y, const char* text) {
    renderCodepoints(x, y, decodeUtf8(text));
}

void GlRenderTarget::drawText(float x, float y, const wchar_t* text) {
    renderText(x, y, text);
}

int GlRenderTarget::getTextWidth(const char* text) const {
    if (!text || !m_glyphAtlas.isLoaded()) return 0;
    float width = 0, height = 0;
    const_cast<GlRenderTarget*>(this)->measureCodepoints(decodeUtf8(text), &width, &height);
    return static_cast<int>(width + 0.5f);
}

int GlRenderTarget::getTextWidth(const wchar_t* text) const {
    if (!text || !m_glyphAtlas.isLoaded()) return 0;
    float w = 0, h = 0;
    const_cast<GlRenderTarget*>(this)->measureCodepoints(decodeWide(text), &w, &h);
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
    measureCodepoints(decodeUtf8(text), width, height);
}

void GlRenderTarget::measureText(const wchar_t* text, float* width, float* height) const {
    measureCodepoints(decodeWide(text), width, height);
}

void GlRenderTarget::measureCodepoints(const std::vector<uint32_t>& codepoints,
                                       float* width, float* height) const {
    if (!m_glyphAtlas.isLoaded()) { if (width) *width = 0; if (height) *height = 0; return; }

    float totalWidth = 0;
    for (uint32_t codepoint : codepoints) {
        const GlyphInfo gi = const_cast<GlyphAtlas&>(m_glyphAtlas).ensureGlyph(codepoint);
        totalWidth += gi.advance > 0 ? gi.advance : m_glyphAtlas.getAscent() * 0.5f;
    }

    if (width) *width = totalWidth;
    if (height) *height = (float)(m_glyphAtlas.getAscent() - m_glyphAtlas.getDescent());
}

// ============================================================
// Pixel buffer access
// ============================================================
color_t* GlRenderTarget::getPixelBuffer() {
    submitBatch();
    if (m_isOnScreen && m_pixelSyncState == PixelSyncState::GpuNewer) {
        captureScreenToTexture();
    }
    downloadFromGpu();
    // The API returns a writable pointer, so conservatively assume it may be
    // changed before the next GPU operation.
    markCpuDirty(fullRect(), true);
    return m_cpuBuffer;
}

const color_t* GlRenderTarget::getPixelBuffer() const {
    GlRenderTarget* self = const_cast<GlRenderTarget*>(this);
    self->submitBatch();
    if (self->m_isOnScreen &&
        self->m_pixelSyncState == PixelSyncState::GpuNewer) {
        self->captureScreenToTexture();
    }
    self->downloadFromGpu();
    return m_cpuBuffer;
}

color_t* GlRenderTarget::getPixelBufferForWrite(
    int x, int y, int width, int height) {
    submitBatch();
    if (m_isOnScreen && m_pixelSyncState == PixelSyncState::GpuNewer) {
        captureScreenToTexture();
    }
    downloadFromGpu();
    markCpuDirty(clippedRect(x, y, width, height), false);
    return m_cpuBuffer;
}

void GlRenderTarget::markPixelBufferDirty(
    int x, int y, int width, int height) {
    const PixelRect dirty = clippedRect(x, y, width, height);
    if (dirty.empty()) return;

    // This function narrows the conservative full-image exposure created by
    // writable getPixelBuffer(). Its public contract requires no intervening
    // EGE operation, so CpuNewer is the expected state here.
    if (m_pixelSyncState == PixelSyncState::CpuNewer) {
        // Accumulate regions declared for earlier writable-buffer batches.
        // Clearing unknown only resolves the most recent pointer exposure;
        // already known dirty pixels must remain pending.
        unionRect(m_cpuDirtyRect, dirty);
        m_cpuDirtyUnknown = false;
    }
}

bool GlRenderTarget::updatePixelBuffer(
    int x, int y, int width, int height,
    const color_t* pixels, int pitchBytes) {
    const PixelRect dirty = clippedRect(x, y, width, height);
    if (!pixels || dirty.empty() || dirty.left != x || dirty.top != y ||
        dirty.right != x + width || dirty.bottom != y + height) {
        return false;
    }

    // Complete earlier API calls first. Unlike getPixelBuffer(), this explicit
    // overwrite does not need current destination pixels and therefore never
    // performs a GPU-to-CPU readback.
    submitBatch();
    syncToGpu();
    const PixelSyncState stateBeforeUpdate = m_pixelSyncState;
    const PixelRect gpuDirtyBeforeUpdate = m_gpuDirtyRect;

    const size_t rowBytes = static_cast<size_t>(width) * sizeof(color_t);
    const unsigned char* sourceRow = reinterpret_cast<const unsigned char*>(pixels);
    for (int row = 0; row < height; ++row) {
        std::memcpy(m_cpuBuffer + static_cast<size_t>(y + row) * m_width + x,
                    sourceRow, rowBytes);
        sourceRow += pitchBytes;
    }
    uploadRect(dirty);

    if (stateBeforeUpdate == PixelSyncState::GpuNewer ||
        stateBeforeUpdate == PixelSyncState::ScreenTextureNewer) {
        m_gpuDirtyRect = gpuDirtyBeforeUpdate;
        unionRect(m_gpuDirtyRect, dirty);
        m_pixelSyncState = stateBeforeUpdate;
    } else {
        m_gpuDirtyRect = PixelRect();
        m_pixelSyncState = PixelSyncState::Synchronized;
    }
    m_cpuDirtyRect = PixelRect();
    m_cpuDirtyUnknown = false;

    if (m_isOnScreen) {
        drawImageQuad(m_texture, m_width, m_height,
                      x, y, width, height, x - m_vpLeft, y - m_vpTop,
                      width, height, 0, 0, 0, 1.0f, 1.0f, 0, 0);
    }
    return true;
}

void GlRenderTarget::downloadFromGpu() {
    const bool gpuPixelsNeedDownload =
        m_pixelSyncState == PixelSyncState::GpuNewer ||
        (m_isOnScreen && m_pixelSyncState == PixelSyncState::ScreenTextureNewer);
    if (!m_initialized || !m_cpuBuffer || !gpuPixelsNeedDownload) return;

    const PixelRect dirty = m_gpuDirtyRect.empty() ? fullRect() : m_gpuDirtyRect;
    const int dirtyWidth = dirty.right - dirty.left;
    const int dirtyHeight = dirty.bottom - dirty.top;

    GLint previousReadFramebuffer = 0;
    GLint previousReadBuffer = GL_BACK;
    GLint previousPackBuffer = 0;
    GLint previousPackAlignment = 4;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPackBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);

    GLuint screenReadFramebuffer = 0;
    if (m_isOnScreen) {
        // The window back buffer may already have been swapped. The screen
        // texture captured immediately before the swap is the stable source.
        glGenFramebuffers(1, &screenReadFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, screenReadFramebuffer);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_texture, 0);
    } else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    }
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    m_pixelTransferBuffer.resize(static_cast<size_t>(dirtyWidth) * dirtyHeight);
    glReadPixels(dirty.left, m_height - dirty.bottom,
                 dirtyWidth, dirtyHeight, GL_BGRA, GL_UNSIGNED_BYTE,
                 m_pixelTransferBuffer.data());

    // OpenGL returns the lowest row first; EGE buffers are top-down. BGRA
    // matches color_t byte layout, so each row can be copied without a
    // per-pixel channel conversion.
    for (int glRow = 0; glRow < dirtyHeight; ++glRow) {
        const int egeY = dirty.bottom - 1 - glRow;
        std::memcpy(m_cpuBuffer + static_cast<size_t>(egeY) * m_width + dirty.left,
                    m_pixelTransferBuffer.data() +
                        static_cast<size_t>(glRow) * dirtyWidth,
                    static_cast<size_t>(dirtyWidth) * sizeof(color_t));
    }

    m_pixelSyncState = PixelSyncState::Synchronized;
    m_gpuDirtyRect = PixelRect();
    m_cpuDirtyRect = PixelRect();
    m_cpuDirtyUnknown = false;
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, previousPackBuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);
    glReadBuffer(previousReadBuffer);
    if (screenReadFramebuffer) {
        glDeleteFramebuffers(1, &screenReadFramebuffer);
    }
}

void GlRenderTarget::uploadRect(const PixelRect& dirty) {
    if (!m_initialized || !m_cpuBuffer || dirty.empty()) return;
    const int dirtyWidth = dirty.right - dirty.left;
    const int dirtyHeight = dirty.bottom - dirty.top;
    m_pixelTransferBuffer.resize(static_cast<size_t>(dirtyWidth) * dirtyHeight);
    for (int glRow = 0; glRow < dirtyHeight; ++glRow) {
        const int egeY = dirty.bottom - 1 - glRow;
        std::memcpy(m_pixelTransferBuffer.data() +
                        static_cast<size_t>(glRow) * dirtyWidth,
                    m_cpuBuffer + static_cast<size_t>(egeY) * m_width + dirty.left,
                    static_cast<size_t>(dirtyWidth) * sizeof(color_t));
    }

    GLint previousTexture = 0;
    GLint previousUnpackBuffer = 0;
    GLint previousUnpackAlignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previousUnpackBuffer);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, dirty.left, m_height - dirty.bottom,
                    dirtyWidth, dirtyHeight, GL_BGRA, GL_UNSIGNED_BYTE,
                    m_pixelTransferBuffer.data());
    glBindTexture(GL_TEXTURE_2D, previousTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, previousUnpackBuffer);
}

void GlRenderTarget::captureScreenToTexture() {
    if (!m_isOnScreen || !m_initialized || !m_texture) return;

    // Flush pending draw commands first
    submitBatch();
    if (m_pixelSyncState != PixelSyncState::GpuNewer) {
        return;
    }

    // Keep the screen usable as an IMAGE source without forcing a blocking
    // GPU-to-CPU readback on every swap. The CPU copy remains lazy and is
    // refreshed by getPixelBuffer() only when a caller actually requests it.
    GLint previousReadFramebuffer = 0;
    GLint previousReadBuffer = GL_BACK;
    GLint previousTexture = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_width, m_height);
    glBindTexture(GL_TEXTURE_2D, previousTexture);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);
    glReadBuffer(previousReadBuffer);
    // The texture now contains the last back-buffer image. Keep this distinct
    // from GpuNewer: after glfwSwapBuffers(), GL_BACK is a different buffer and
    // recapturing it would make screen reads lag one frame behind presentation.
    m_pixelSyncState = PixelSyncState::ScreenTextureNewer;
}

void GlRenderTarget::syncToGpu() {
    if (!m_cpuBuffer || !m_initialized ||
        m_pixelSyncState != PixelSyncState::CpuNewer) return;
    const PixelRect dirty = (m_cpuDirtyUnknown || m_cpuDirtyRect.empty())
        ? fullRect() : m_cpuDirtyRect;
    uploadRect(dirty);
    m_pixelSyncState = PixelSyncState::Synchronized;
    m_cpuDirtyRect = PixelRect();
    m_gpuDirtyRect = PixelRect();
    m_cpuDirtyUnknown = false;
}

void GlRenderTarget::rebuild(int width, int height) {
    if (!m_initialized) return;

    // resize_f replaces the image storage instead of preserving its pixels.
    // Primitives queued against the old dimensions must not be replayed into
    // the newly allocated framebuffer on the next readback or flush.
    m_vertices.clear();

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

    m_width = width;
    m_height = height;
    m_pixelSyncState = PixelSyncState::CpuNewer;
    m_cpuDirtyRect = fullRect();
    m_gpuDirtyRect = PixelRect();
    m_cpuDirtyUnknown = false;
    m_pixelTransferBuffer.clear();
    m_projectionDirty = true;
}

// ============================================================
// Submit
// ============================================================
void GlRenderTarget::flush() {
    const bool uploadScreenBuffer =
        m_isOnScreen && m_pixelSyncState == PixelSyncState::CpuNewer;
    syncToGpu();
    if (uploadScreenBuffer) {
        drawImageQuad(m_texture, m_width, m_height, 0, 0, m_width, m_height,
                      -m_vpLeft, -m_vpTop, m_width, m_height,
                      0, 0, 0, 1.0f, 1.0f, 0, 0);
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
