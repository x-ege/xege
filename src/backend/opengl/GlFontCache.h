// src/backend/opengl/GlFontCache.h
// Glyph atlas and font management for OpenGL text rendering
#pragma once
#include "glad/gl.h"
#include "../../external/stb_truetype.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace ege {

// Glyph information stored in the atlas
struct GlyphInfo {
    int atlasX, atlasY;     // Position in atlas texture
    int width, height;      // Glyph bitmap dimensions
    int bearingX, bearingY; // Offset from baseline
    int advance;            // Horizontal advance
    bool valid;
    GlyphInfo() : atlasX(0), atlasY(0), width(0), height(0),
                  bearingX(0), bearingY(0), advance(0), valid(false) {}
};

// Font configuration matching EGE's setfont parameters
struct FontConfig {
    int     height;       // Font height in pixels
    int     width;        // Font width (0 = auto based on height)
    char    face[128];    // Font face name
    int     escapement;   // Rotation in tenths of degrees
    int     weight;       // Font weight (400 = normal, 700 = bold)
    bool    italic;
    bool    underline;
    bool    strikeout;

    FontConfig() : height(16), width(0), escapement(0), weight(400),
                   italic(false), underline(false), strikeout(false) {
        face[0] = '\0';
    }

    bool operator==(const FontConfig& o) const {
        return height == o.height && width == o.width && escapement == o.escapement &&
               weight == o.weight && italic == o.italic && underline == o.underline &&
               strikeout == o.strikeout && std::string(face) == std::string(o.face);
    }
};

// Glyph atlas: renders characters to a single texture
class GlyphAtlas {
public:
    static constexpr int ATLAS_SIZE = 1024;

    GlyphAtlas();
    ~GlyphAtlas();

    // Load a font file; returns true on success
    bool loadFont(const char* face, int height, int weight, bool italic);

    // Ensure a character glyph is rasterized in the atlas; returns glyph info
    GlyphInfo ensureGlyph(uint32_t codepoint);

    // Get the OpenGL texture ID for the atlas
    GLuint getTexture() const { return m_texture; }
    int getAtlasSize() const { return ATLAS_SIZE; }

    // Get font metrics
    int getAscent() const { return m_ascent; }
    int getDescent() const { return m_descent; }
    int getLineGap() const { return m_lineGap; }
    bool isLoaded() const { return m_fontData != nullptr; }

    // Get the width scale factor (for font width parameter)
    float getWidthScale() const { return m_widthScale; }

private:
    void rasterizeGlyph(uint32_t codepoint);
    void findAtlasSlot(int w, int h, int& outX, int& outY);
    void uploadToTexture(int x, int y, int w, int h, const unsigned char* data);

    stbtt_fontinfo  m_fontInfo;
    unsigned char*  m_fontData;     // Owned font file data
    float           m_scale;        // stb_truetype scale factor
    int             m_ascent;
    int             m_descent;
    int             m_lineGap;
    float           m_widthScale;   // Scale for non-default width

    GLuint          m_texture;      // Atlas texture
    unsigned char*  m_atlasPixels;  // CPU-side RGBA atlas

    // Position tracking for packing
    int             m_rowHeight;
    int             m_cursorX, m_cursorY;

    // Glyph cache
    std::unordered_map<uint32_t, GlyphInfo> m_glyphs;
};

// Find a system font file path for a given face name (macOS)
std::string findFontPath(const char* face);

} // namespace ege
