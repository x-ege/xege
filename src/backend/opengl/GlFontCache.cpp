// src/backend/opengl/GlFontCache.cpp
#define STB_TRUETYPE_IMPLEMENTATION
#include "GlFontCache.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace ege {

// ============================================================
// Font path resolution (macOS)
// ============================================================
std::string findFontPath(const char* face) {
    static const char* fontDirs[] = {
        "/System/Library/Fonts/",
        "/Library/Fonts/",
        "/System/Library/Fonts/Supplemental/",
        nullptr
    };

    struct FontMap { const char* face; const char* filename; };
    static const FontMap fontMap[] = {
        {"Arial",               "Arial.ttf"},
        {"Arial",               "Arial Unicode.ttf"},
        {"Times New Roman",     "Times New Roman.ttf"},
        {"Times",               "Times New Roman.ttf"},
        {"Courier",             "Courier New.ttf"},
        {"Courier New",         "Courier New.ttf"},
        {"Helvetica",           "Helvetica.ttc"},
        {"Helvetica Neue",      "HelveticaNeue.ttc"},
        {"PingFang SC",         "PingFang.ttc"},
        {"PingFang TC",         "PingFang.ttc"},
        {"PingFang HK",         "PingFang.ttc"},
        {"STHeiti",             "STHeiti Medium.ttc"},
        {"Songti SC",           "Songti.ttc"},
        {"Songti TC",           "Songti.ttc"},
        {"KaiTi",               "STKaiti.ttf"},
        {"SimSun",              "Songti.ttc"},
        {"SimHei",              "Heiti.ttc"},
        {"Microsoft YaHei",     "PingFang.ttc"},
        {"Consolas",            "Consolas.ttf"},
        {"Georgia",             "Georgia.ttf"},
        {"Verdana",             "Verdana.ttf"},
        {"Impact",              "Impact.ttf"},
        {"Comic Sans MS",       "Comic Sans.ttf"},
        {"Menlo",               "Menlo.ttc"},
        {"Monaco",              "Monaco.ttf"},
        {nullptr, nullptr}
    };

    const char* filename = nullptr;
    for (const FontMap* m = fontMap; m->face; ++m) {
        if (strcasecmp(face, m->face) == 0) {
            filename = m->filename;
            break;
        }
    }

    if (!filename) {
        std::string tryName = std::string(face) + ".ttf";
        for (const char** dir = fontDirs; *dir; ++dir) {
            std::string path = std::string(*dir) + tryName;
            FILE* f = fopen(path.c_str(), "rb");
            if (f) { fclose(f); return path; }
        }
        tryName = std::string(face) + ".ttc";
        for (const char** dir = fontDirs; *dir; ++dir) {
            std::string path = std::string(*dir) + tryName;
            FILE* f = fopen(path.c_str(), "rb");
            if (f) { fclose(f); return path; }
        }
        return "/System/Library/Fonts/PingFang.ttc";
    }

    for (const char** dir = fontDirs; *dir; ++dir) {
        std::string path = std::string(*dir) + filename;
        FILE* f = fopen(path.c_str(), "rb");
        if (f) { fclose(f); return path; }
    }

    return "/System/Library/Fonts/PingFang.ttc";
}

// ============================================================
// GlyphAtlas implementation
// ============================================================

GlyphAtlas::GlyphAtlas()
    : m_fontData(nullptr), m_ascent(0), m_descent(0), m_lineGap(0),
      m_scale(0), m_widthScale(1.0f), m_texture(0),
      m_rowHeight(0), m_cursorX(0), m_cursorY(0), m_atlasPixels(nullptr) {
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_atlasPixels = new unsigned char[ATLAS_SIZE * ATLAS_SIZE * 4];
    memset(m_atlasPixels, 0, ATLAS_SIZE * ATLAS_SIZE * 4);
}

GlyphAtlas::~GlyphAtlas() {
    if (m_fontData) {
        delete[] m_fontData;
    }
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
    }
    delete[] m_atlasPixels;
}

bool GlyphAtlas::loadFont(const char* face, int height, int weight, bool italic) {
    std::string path = findFontPath(face);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "[GlFontCache] Failed to open font: %s (resolved to %s)\n", face, path.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (m_fontData) {
        delete[] m_fontData;
    }
    m_fontData = new unsigned char[size];
    fread(m_fontData, 1, size, f);
    fclose(f);

    if (!stbtt_InitFont(&m_fontInfo, m_fontData, 0)) {
        fprintf(stderr, "[GlFontCache] Failed to parse font: %s\n", path.c_str());
        delete[] m_fontData;
        m_fontData = nullptr;
        return false;
    }

    // Compute scale for target pixel height
    m_scale = stbtt_ScaleForPixelHeight(&m_fontInfo, (float)height);

    // Font width parameter: if width != 0, scale X differently
    m_widthScale = 1.0f;

    // Get font metrics
    int asc, desc, lgap;
    stbtt_GetFontVMetrics(&m_fontInfo, &asc, &desc, &lgap);
    m_ascent  = (int)(asc * m_scale + 0.5f);
    m_descent = (int)(desc * m_scale + 0.5f);
    m_lineGap = (int)(lgap * m_scale + 0.5f);

    // Reset glyph cache
    m_glyphs.clear();
    m_cursorX = 0;
    m_cursorY = 0;
    m_rowHeight = 0;
    memset(m_atlasPixels, 0, ATLAS_SIZE * ATLAS_SIZE * 4);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    return true;
}

void GlyphAtlas::findAtlasSlot(int w, int h, int& outX, int& outY) {
    if (m_cursorX + w > ATLAS_SIZE) {
        m_cursorX = 0;
        m_cursorY += m_rowHeight;
        m_rowHeight = 0;
    }
    if (m_cursorY + h > ATLAS_SIZE) {
        m_cursorX = 0;
        m_cursorY = 0;
        m_rowHeight = 0;
        m_glyphs.clear();
        memset(m_atlasPixels, 0, ATLAS_SIZE * ATLAS_SIZE * 4);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    outX = m_cursorX;
    outY = m_cursorY;
}

void GlyphAtlas::uploadToTexture(int x, int y, int w, int h, const unsigned char* data) {
    for (int row = 0; row < h; row++) {
        unsigned char* dst = m_atlasPixels + ((y + row) * ATLAS_SIZE + x) * 4;
        const unsigned char* src = data + row * w;
        for (int col = 0; col < w; col++) {
            unsigned char a = src[col];
            dst[col * 4 + 0] = 0xFF;
            dst[col * 4 + 1] = 0xFF;
            dst[col * 4 + 2] = 0xFF;
            dst[col * 4 + 3] = a;
        }
    }
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                    m_atlasPixels + (y * ATLAS_SIZE + x) * 4);
}

void GlyphAtlas::rasterizeGlyph(uint32_t codepoint) {
    int glyphIdx = stbtt_FindGlyphIndex(&m_fontInfo, (int)codepoint);
    if (glyphIdx == 0) {
        m_glyphs[codepoint] = GlyphInfo();
        return;
    }

    // Get bitmap box using the actual font scale
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&m_fontInfo, glyphIdx, m_scale, m_scale, &x0, &y0, &x1, &y1);

    int w = x1 - x0;
    int h = y1 - y0;
    if (w <= 0 || h <= 0) {
        GlyphInfo info;
        info.valid = false;
        m_glyphs[codepoint] = info;
        return;
    }

    int atlasX, atlasY;
    findAtlasSlot(w + 1, h + 1, atlasX, atlasY);

    unsigned char* bitmap = new unsigned char[w * h];
    stbtt_MakeGlyphBitmap(&m_fontInfo, bitmap, w, h, w, m_scale, m_scale, glyphIdx);

    // Faux bold: dilate bitmap by 1 pixel if weight >= 700
    // (skipped for simplicity — the font's bold weight is usually enough)

    uploadToTexture(atlasX, atlasY, w, h, bitmap);
    delete[] bitmap;

    // Store glyph info
    GlyphInfo info;
    info.atlasX = atlasX;
    info.atlasY = atlasY;
    info.width = w;
    info.height = h;

    int advance, lsb;
    stbtt_GetGlyphHMetrics(&m_fontInfo, glyphIdx, &advance, &lsb);
    info.advance  = (int)(advance * m_scale + 0.5f);
    info.bearingX = (int)(x0 + 0.5f); // already in pixel coords
    info.bearingY = (int)(y0 + 0.5f);
    info.valid = true;

    m_glyphs[codepoint] = info;

    m_cursorX = atlasX + w + 1;
    int totalH = h + 1;
    if (totalH > m_rowHeight) m_rowHeight = totalH;
}

GlyphInfo GlyphAtlas::ensureGlyph(uint32_t codepoint) {
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) {
        return it->second;
    }
    rasterizeGlyph(codepoint);
    return m_glyphs[codepoint];
}

} // namespace ege
