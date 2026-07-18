// src/backend/opengl/GlFontCache.cpp
#define STB_TRUETYPE_IMPLEMENTATION
#include "GlFontCache.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

namespace ege {

// ============================================================
// Font path resolution (Windows/macOS/Linux)
// ============================================================
namespace {

std::string lowerAscii(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::string normalizedFontName(const std::string& value) {
    std::string result;
    for (unsigned char c : value) {
        if (std::isalnum(c)) result.push_back(static_cast<char>(std::tolower(c)));
    }
    return result;
}

std::string baseName(const std::string& path) {
    const std::string::size_type slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool isFontFile(const std::string& name) {
    const std::string lower = lowerAscii(name);
    return lower.size() > 4 &&
        (lower.compare(lower.size() - 4, 4, ".ttf") == 0 ||
         lower.compare(lower.size() - 4, 4, ".ttc") == 0 ||
         lower.compare(lower.size() - 4, 4, ".otf") == 0 ||
         lower.compare(lower.size() - 4, 4, ".otc") == 0);
}

void collectFontFiles(const std::string& directory, int depth, std::vector<std::string>& files) {
    if (depth < 0) return;
#ifdef _WIN32
    struct _finddata_t entry;
    const std::string pattern = directory + "/*";
    const intptr_t handle = _findfirst(pattern.c_str(), &entry);
    if (handle == -1) return;
    do {
        const char* name = entry.name;
        if (name[0] == '.' &&
            (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) continue;
        const std::string path = directory + "/" + name;
        if ((entry.attrib & _A_SUBDIR) != 0) collectFontFiles(path, depth - 1, files);
        else if (isFontFile(name)) files.push_back(path);
    } while (_findnext(handle, &entry) == 0);
    _findclose(handle);
#else
    DIR* dir = opendir(directory.c_str());
    if (!dir) return;
    while (dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) continue;
        const std::string path = directory + "/" + entry->d_name;
        struct stat info;
        if (stat(path.c_str(), &info) != 0) continue;
        if (S_ISDIR(info.st_mode)) collectFontFiles(path, depth - 1, files);
        else if (S_ISREG(info.st_mode) && isFontFile(entry->d_name)) files.push_back(path);
    }
    closedir(dir);
#endif
}

std::vector<std::string> systemFontFiles() {
    std::vector<std::string> roots;
#ifdef _WIN32
    const char* windowsDirectory = std::getenv("WINDIR");
    roots.push_back(windowsDirectory && windowsDirectory[0]
                        ? std::string(windowsDirectory) + "/Fonts"
                        : "C:/Windows/Fonts");
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData && localAppData[0]) {
        roots.push_back(std::string(localAppData) + "/Microsoft/Windows/Fonts");
    }
#elif defined(__APPLE__)
    roots.push_back("/System/Library/Fonts");
    roots.push_back("/Library/Fonts");
#else
    roots.push_back("/usr/share/fonts");
    roots.push_back("/usr/local/share/fonts");
#endif
#ifndef _WIN32
    const char* home = std::getenv("HOME");
    if (home && home[0]) {
#ifdef __APPLE__
        roots.push_back(std::string(home) + "/Library/Fonts");
#else
        const char* xdgDataHome = std::getenv("XDG_DATA_HOME");
        roots.push_back(xdgDataHome && xdgDataHome[0]
                            ? std::string(xdgDataHome) + "/fonts"
                            : std::string(home) + "/.local/share/fonts");
        roots.push_back(std::string(home) + "/.fonts");
#endif
    }
#endif

    std::vector<std::string> files;
    for (const std::string& root : roots) collectFontFiles(root, 8, files);
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
}

void appendAliasCandidates(const std::string& faceKey, std::vector<std::string>& candidates) {
    static const std::unordered_map<std::string, std::vector<std::string>> aliases = {
        {"arial", {"Arial.ttf", "Arial Unicode.ttf", "LiberationSans-Regular.ttf", "DejaVuSans.ttf"}},
        {"timesnewroman", {"Times New Roman.ttf", "LiberationSerif-Regular.ttf", "DejaVuSerif.ttf"}},
        {"times", {"Times New Roman.ttf", "LiberationSerif-Regular.ttf", "DejaVuSerif.ttf"}},
        {"courier", {"Courier New.ttf", "LiberationMono-Regular.ttf", "DejaVuSansMono.ttf"}},
        {"couriernew", {"Courier New.ttf", "LiberationMono-Regular.ttf", "DejaVuSansMono.ttf"}},
        {"helvetica", {"Helvetica.ttc", "Arial.ttf", "LiberationSans-Regular.ttf", "DejaVuSans.ttf"}},
        {"helveticaneue", {"HelveticaNeue.ttc", "Helvetica.ttc", "LiberationSans-Regular.ttf"}},
        {"consolas", {"consola.ttf", "Consolas.ttf", "Menlo.ttc", "LiberationMono-Regular.ttf", "DejaVuSansMono.ttf"}},
        {"menlo", {"Menlo.ttc", "DejaVuSansMono.ttf", "LiberationMono-Regular.ttf"}},
        {"monaco", {"Monaco.ttf", "Menlo.ttc", "DejaVuSansMono.ttf"}},
        {"simsun", {"simsun.ttc", "Songti.ttc", "NotoSerifCJK-Regular.ttc", "NotoSerifCJKsc-Regular.otf", "DroidSansFallbackFull.ttf"}},
        {"simhei", {"simhei.ttf", "Heiti.ttc", "PingFang.ttc", "NotoSansCJK-Regular.ttc", "NotoSansCJKsc-Regular.otf"}},
        {"microsoftyahei", {"msyh.ttc", "msyhbd.ttc", "msyhl.ttc", "PingFang.ttc", "NotoSansCJK-Regular.ttc", "NotoSansCJKsc-Regular.otf"}},
        {"pingfangsc", {"PingFang.ttc", "NotoSansCJK-Regular.ttc", "NotoSansCJKsc-Regular.otf"}},
        {"pingfangtc", {"PingFang.ttc", "NotoSansCJK-Regular.ttc", "NotoSansCJKtc-Regular.otf"}},
        {"songtisc", {"Songti.ttc", "NotoSerifCJK-Regular.ttc", "NotoSerifCJKsc-Regular.otf"}},
        {"kaiti", {"STKaiti.ttf", "Kaiti.ttc", "NotoSerifCJK-Regular.ttc"}},
    };
    const auto found = aliases.find(faceKey);
    if (found != aliases.end()) candidates.insert(candidates.end(), found->second.begin(), found->second.end());
}

} // anonymous namespace

std::string findFontPath(const char* face, int weight, bool italic) {
    const std::string requested = face && face[0] ? face : "Arial";
    const std::string faceKey = normalizedFontName(requested);
    std::vector<std::string> candidates;
    const bool bold = weight >= 600;
    if (bold && italic) {
        candidates.push_back(requested + " Bold Italic.ttf");
        candidates.push_back(requested + "-BoldItalic.ttf");
    }
    if (bold) {
        candidates.push_back(requested + " Bold.ttf");
        candidates.push_back(requested + "-Bold.ttf");
    }
    if (italic) {
        candidates.push_back(requested + " Italic.ttf");
        candidates.push_back(requested + "-Italic.ttf");
    }
    candidates.push_back(requested + ".ttf");
    candidates.push_back(requested + ".ttc");
    candidates.push_back(requested + ".otf");
    appendAliasCandidates(faceKey, candidates);

    static const std::vector<std::string> files = systemFontFiles();
    for (const std::string& candidate : candidates) {
        const std::string wanted = lowerAscii(candidate);
        for (const std::string& path : files) {
            if (lowerAscii(baseName(path)) == wanted) return path;
        }
    }

    // A family name often differs from its exact filename only by spaces and
    // punctuation (for example DejaVu Sans -> DejaVuSans.ttf).
    for (const std::string& path : files) {
        std::string stem = baseName(path);
        const std::string::size_type dot = stem.find_last_of('.');
        if (dot != std::string::npos) stem.resize(dot);
        if (normalizedFontName(stem) == faceKey) return path;
    }

    static const char* fallbackNames[] = {
#ifdef _WIN32
        "segoeui.ttf", "arial.ttf", "tahoma.ttf",
#elif defined(__APPLE__)
        "PingFang.ttc", "Helvetica.ttc", "Arial.ttf",
#else
        "DejaVuSans.ttf", "LiberationSans-Regular.ttf", "NotoSans-Regular.ttf",
#endif
        nullptr};
    for (const char** fallback = fallbackNames; *fallback; ++fallback) {
        const std::string wanted = lowerAscii(*fallback);
        for (const std::string& path : files) {
            if (lowerAscii(baseName(path)) == wanted) return path;
        }
    }
    return std::string();
}

// ============================================================
// GlyphAtlas implementation
// ============================================================

GlyphAtlas::GlyphAtlas()
    : m_fontData(nullptr), m_ascent(0), m_descent(0), m_lineGap(0),
      m_scale(0), m_widthScale(1.0f), m_weight(400), m_italic(false), m_texture(0),
      m_rowHeight(0), m_cursorX(0), m_cursorY(0), m_atlasPixels(nullptr) {
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, previousTexture);

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

bool GlyphAtlas::loadFont(const char* face, int height, int width, int weight, bool italic) {
    std::string path = findFontPath(face, weight, italic);
    if (path.empty()) {
        fprintf(stderr, "[GlFontCache] No usable system font found for: %s\n", face ? face : "(null)");
        return false;
    }
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

    int styleFlags = 0;
    if (weight >= 600) styleFlags |= STBTT_MACSTYLE_BOLD;
    if (italic) styleFlags |= STBTT_MACSTYLE_ITALIC;
    if (styleFlags == 0) styleFlags = STBTT_MACSTYLE_NONE;
    int fontOffset = stbtt_FindMatchingFont(m_fontData, face, styleFlags);
    if (fontOffset < 0) fontOffset = stbtt_FindMatchingFont(m_fontData, face, STBTT_MACSTYLE_DONTCARE);
    if (fontOffset < 0) fontOffset = stbtt_GetFontOffsetForIndex(m_fontData, 0);
    if (fontOffset < 0 || !stbtt_InitFont(&m_fontInfo, m_fontData, fontOffset)) {
        fprintf(stderr, "[GlFontCache] Failed to parse font: %s\n", path.c_str());
        delete[] m_fontData;
        m_fontData = nullptr;
        return false;
    }

    // Compute scale for target pixel height
    const int pixelHeight = height == 0 ? 16 : std::abs(height);
    m_scale = stbtt_ScaleForPixelHeight(&m_fontInfo, (float)pixelHeight);

    int referenceAdvance = 0, referenceBearing = 0;
    stbtt_GetCodepointHMetrics(&m_fontInfo, '0', &referenceAdvance, &referenceBearing);
    const float naturalWidth = referenceAdvance * m_scale;
    m_widthScale = width != 0 && naturalWidth > 0.0f
        ? std::abs((float)width) / naturalWidth : 1.0f;
    m_weight = weight;
    m_italic = italic;

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
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, previousTexture);

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
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, previousTexture);
    }
    outX = m_cursorX;
    outY = m_cursorY;
}

void GlyphAtlas::uploadToTexture(int x, int y, int w, int h, const unsigned char* data) {
    std::vector<unsigned char> upload(static_cast<size_t>(w) * h * 4);
    for (int row = 0; row < h; row++) {
        unsigned char* dst = m_atlasPixels + ((y + row) * ATLAS_SIZE + x) * 4;
        unsigned char* uploadRow = upload.data() + static_cast<size_t>(row) * w * 4;
        const unsigned char* src = data + row * w;
        for (int col = 0; col < w; col++) {
            unsigned char a = src[col];
            dst[col * 4 + 0] = 0xFF;
            dst[col * 4 + 1] = 0xFF;
            dst[col * 4 + 2] = 0xFF;
            dst[col * 4 + 3] = a;
            uploadRow[col * 4 + 0] = 0xFF;
            uploadRow[col * 4 + 1] = 0xFF;
            uploadRow[col * 4 + 2] = 0xFF;
            uploadRow[col * 4 + 3] = a;
        }
    }
    GLint previousTexture = 0;
    GLint previousUnpackAlignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                    upload.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, previousTexture);
}

void GlyphAtlas::rasterizeGlyph(uint32_t codepoint) {
    int glyphIdx = stbtt_FindGlyphIndex(&m_fontInfo, (int)codepoint);
    if (glyphIdx == 0) {
        glyphIdx = stbtt_FindGlyphIndex(&m_fontInfo, '?');
    }

    GlyphInfo info;
    int advance = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&m_fontInfo, glyphIdx, &advance, &lsb);
    info.advance = (int)std::lround(advance * m_scale * m_widthScale);

    // Get bitmap box using the actual font scale
    int x0, y0, x1, y1;
    const float scaleX = m_scale * m_widthScale;
    stbtt_GetGlyphBitmapBox(&m_fontInfo, glyphIdx, scaleX, m_scale, &x0, &y0, &x1, &y1);

    int w = x1 - x0;
    int h = y1 - y0;
    if (w <= 0 || h <= 0) {
        info.valid = false;
        m_glyphs[codepoint] = info;
        return;
    }
    if (w + 1 > ATLAS_SIZE || h + 1 > ATLAS_SIZE) {
        info.valid = false;
        m_glyphs[codepoint] = info;
        return;
    }

    int atlasX, atlasY;
    findAtlasSlot(w + 1, h + 1, atlasX, atlasY);

    unsigned char* bitmap = new unsigned char[w * h];
    stbtt_MakeGlyphBitmap(&m_fontInfo, bitmap, w, h, w, scaleX, m_scale, glyphIdx);

    // Faux bold: dilate bitmap by 1 pixel if weight >= 700
    // (skipped for simplicity — the font's bold weight is usually enough)

    uploadToTexture(atlasX, atlasY, w, h, bitmap);
    delete[] bitmap;

    // Store glyph info
    info.atlasX = atlasX;
    info.atlasY = atlasY;
    info.width = w;
    info.height = h;

    info.bearingX = x0;
    info.bearingY = y0;
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
