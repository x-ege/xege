#include "ege.h"
#include "../test_shutdown.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

static_assert(sizeof(DWORD) == 4, "DWORD must keep the Win32 ABI width on every platform");
static_assert(sizeof(BITMAPFILEHEADER) == 14, "BMP file header must use the packed Win32 layout");
static_assert(sizeof(BITMAPINFOHEADER) == 40, "BMP info header must use the Win32 layout");

int failures = 0;

unsigned int rgb(ege::color_t color)
{
    return color & 0x00FFFFFFU;
}

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectPixel(ege::PCIMAGE image, int x, int y, ege::color_t expected, const std::string& message)
{
    const ege::color_t actual = ege::getpixel(x, y, image);
    if (rgb(actual) != rgb(expected)) {
        ++failures;
        std::cerr << "FAIL: " << message << " at (" << x << ", " << y << ")"
                  << ", expected RGB=0x" << std::hex << rgb(expected)
                  << ", actual RGB=0x" << rgb(actual) << std::dec << '\n';
    }
}

int countPixelsDifferentFrom(ege::PCIMAGE image, ege::color_t color)
{
    const ege::color_t* pixels = ege::getbuffer(image);
    const int count = ege::getwidth(image) * ege::getheight(image);
    int different = 0;
    for (int i = 0; i < count; ++i) {
        if (rgb(pixels[i]) != rgb(color)) {
            ++different;
        }
    }
    return different;
}

int countPixelsEqualTo(ege::PCIMAGE image, ege::color_t color)
{
    const ege::color_t* pixels = ege::getbuffer(image);
    const int count = ege::getwidth(image) * ege::getheight(image);
    int equal = 0;
    for (int i = 0; i < count; ++i) {
        if (rgb(pixels[i]) == rgb(color)) {
            ++equal;
        }
    }
    return equal;
}

int countPixelsEqualToInRect(ege::PCIMAGE image, ege::color_t color,
                             int left, int top, int right, int bottom)
{
    const int clippedLeft = std::max(0, left);
    const int clippedTop = std::max(0, top);
    const int clippedRight = std::min(ege::getwidth(image), right);
    const int clippedBottom = std::min(ege::getheight(image), bottom);
    int equal = 0;
    for (int y = clippedTop; y < clippedBottom; ++y) {
        for (int x = clippedLeft; x < clippedRight; ++x) {
            if (rgb(ege::getpixel(x, y, image)) == rgb(color)) {
                ++equal;
            }
        }
    }
    return equal;
}

void expectEightPixelFillPattern(ege::PCIMAGE image,
                                 ege::color_t foreground,
                                 ege::color_t background,
                                 const std::string& name)
{
    expect(countPixelsEqualToInRect(image, foreground, 0, 0, 15, 15) > 0,
           name + " paints foreground cells");
    expect(countPixelsEqualToInRect(image, background, 0, 0, 15, 15) > 0,
           name + " paints background cells");

    bool repeats = true;
    for (int y = 0; y < 7 && repeats; ++y) {
        for (int x = 0; x < 7; ++x) {
            const unsigned int sample = rgb(ege::getpixel(x, y, image));
            if (sample != rgb(ege::getpixel(x + 8, y, image)) ||
                sample != rgb(ege::getpixel(x, y + 8, image))) {
                repeats = false;
                break;
            }
        }
    }
    expect(repeats, name + " repeats on an eight-pixel tile");
}

struct PixelBounds {
    int left;
    int top;
    int right;
    int bottom;
    bool valid;
};

PixelBounds boundsDifferentFrom(ege::PCIMAGE image, ege::color_t color)
{
    PixelBounds bounds = {ege::getwidth(image), ege::getheight(image), -1, -1, false};
    const ege::color_t* pixels = ege::getbuffer(image);
    const int width = ege::getwidth(image);
    const int height = ege::getheight(image);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (rgb(pixels[y * width + x]) == rgb(color)) continue;
            bounds.left = std::min(bounds.left, x);
            bounds.top = std::min(bounds.top, y);
            bounds.right = std::max(bounds.right, x);
            bounds.bottom = std::max(bounds.bottom, y);
            bounds.valid = true;
        }
    }
    return bounds;
}

int tempProcessId()
{
#ifdef _WIN32
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

std::string tempPath(const char* extension)
{
    const int processId = tempProcessId();
    return "xege-rendering-correctness-" + std::to_string(processId) + extension;
}

std::vector<unsigned char> readFileBytes(const std::string& path)
{
    std::ifstream stream(path.c_str(), std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(stream)),
                                      std::istreambuf_iterator<char>());
}

bool writeFileBytes(const std::string& path, const void* data, size_t size)
{
    std::ofstream stream(path.c_str(), std::ios::binary);
    stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return stream.good();
}

#ifdef _WIN32
SIZE measureWindowsGdiText(const wchar_t* text, int height, int width,
                           const wchar_t* face, int weight = FW_DONTCARE,
                           bool italic = false)
{
    SIZE size = {};
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return size;

    LOGFONTW fontDescription = {};
    fontDescription.lfHeight = height;
    fontDescription.lfWidth = width;
    fontDescription.lfWeight = weight;
    fontDescription.lfItalic = static_cast<BYTE>(italic);
    fontDescription.lfCharSet = DEFAULT_CHARSET;
    fontDescription.lfOutPrecision = OUT_DEFAULT_PRECIS;
    fontDescription.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    fontDescription.lfQuality = DEFAULT_QUALITY;
    fontDescription.lfPitchAndFamily = DEFAULT_PITCH;
    lstrcpynW(fontDescription.lfFaceName, face, LF_FACESIZE);

    HFONT font = CreateFontIndirectW(&fontDescription);
    HGDIOBJ previousFont = font ? SelectObject(dc, font) : NULL;
    if (font) {
        GetTextExtentPoint32W(dc, text, lstrlenW(text), &size);
        SelectObject(dc, previousFont);
        DeleteObject(font);
    }
    DeleteDC(dc);
    return size;
}

std::vector<unsigned char> readFileBytes(const std::wstring& path)
{
    std::vector<unsigned char> bytes;
    FILE* stream = _wfopen(path.c_str(), L"rb");
    if (!stream) return bytes;

    unsigned char buffer[4096];
    size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), stream)) != 0) {
        bytes.insert(bytes.end(), buffer, buffer + count);
    }
    if (std::ferror(stream)) bytes.clear();
    std::fclose(stream);
    return bytes;
}
#endif

unsigned int readBigEndian32(const std::vector<unsigned char>& bytes, size_t offset)
{
    return (static_cast<unsigned int>(bytes[offset]) << 24) |
           (static_cast<unsigned int>(bytes[offset + 1]) << 16) |
           (static_cast<unsigned int>(bytes[offset + 2]) << 8) |
           static_cast<unsigned int>(bytes[offset + 3]);
}

unsigned int readLittleEndian32(const std::vector<unsigned char>& bytes, size_t offset)
{
    return static_cast<unsigned int>(bytes[offset]) |
           (static_cast<unsigned int>(bytes[offset + 1]) << 8) |
           (static_cast<unsigned int>(bytes[offset + 2]) << 16) |
           (static_cast<unsigned int>(bytes[offset + 3]) << 24);
}

unsigned int readLittleEndian16(const std::vector<unsigned char>& bytes, size_t offset)
{
    return static_cast<unsigned int>(bytes[offset]) |
           (static_cast<unsigned int>(bytes[offset + 1]) << 8);
}

void resetImage(ege::PIMAGE image, ege::color_t color)
{
    ege::setviewport(0, 0, ege::getwidth(image), ege::getheight(image), true, image);
    ege::setbkcolor(color, image);
    ege::cleardevice(image);
}

void testClearAndPixels()
{
    ege::PIMAGE image = ege::newimage(12, 10);
    resetImage(image, ege::BLUE);

    expectPixel(image, 0, 0, ege::BLUE, "clear fills the top-left pixel");
    expectPixel(image, 11, 9, ege::BLUE, "clear fills the bottom-right pixel");

    ege::putpixel(3, 4, ege::RED, image);
    expectPixel(image, 3, 4, ege::RED, "putpixel writes the requested coordinate");
    expectPixel(image, 3, 5, ege::BLUE, "putpixel does not modify the vertically mirrored coordinate");

    ege::delimage(image);
}

void testBasicPrimitives()
{
    ege::PIMAGE image = ege::newimage(20, 16);
    resetImage(image, ege::BLACK);

    ege::setlinecolor(ege::RED, image);
    ege::line(2, 3, 12, 3, image);
    expectPixel(image, 6, 3, ege::RED, "line draws along its requested row");

    ege::setfillcolor(ege::GREEN, image);
    ege::bar(4, 6, 11, 12, image);
    expectPixel(image, 7, 8, ege::GREEN, "bar fills its interior");
    expectPixel(image, 3, 8, ege::BLACK, "bar leaves pixels outside its bounds unchanged");

    ege::delimage(image);
}

void testFilledShapeOutlineCompatibility()
{
    ege::PIMAGE image = ege::newimage(48, 40);
    const int polygonPoints[] = {4, 5, 34, 7, 30, 30, 8, 32};

    const auto prepare = [image]() {
        resetImage(image, ege::BLACK);
        ege::setfillcolor(ege::GREEN, image);
        ege::setlinecolor(ege::RED, image);
        ege::setlinestyle(ege::SOLID_LINE, 0, 1, image);
    };
    const auto expectFillAndOutline = [image](const std::string& shape) {
        expect(countPixelsEqualTo(image, ege::GREEN) > 0,
               shape + " paints its brush fill");
        expect(countPixelsEqualTo(image, ege::RED) > 0,
               shape + " also paints its current-pen outline");
    };
    const auto expectSolidFillOnly = [image](const std::string& shape) {
        expect(countPixelsEqualTo(image, ege::GREEN) > 0,
               shape + " paints its brush fill");
        expect(countPixelsEqualTo(image, ege::RED) == 0,
               shape + " suppresses the current-pen outline");
    };

    prepare();
    ege::fillrect(5, 5, 34, 28, image);
    expectFillAndOutline("fillrect");

    prepare();
    ege::fillellipse(24, 20, 13, 9, image);
    expectFillAndOutline("fillellipse");

    prepare();
    ege::fillpie(24, 20, 20, 155, 14, 11, image);
    expectFillAndOutline("fillpie");

    prepare();
    ege::fillroundrect(5, 5, 38, 30, 6, 4, image);
    expectFillAndOutline("fillroundrect");

    prepare();
    ege::fillpoly(4, polygonPoints, image);
    expectFillAndOutline("fillpoly");

    prepare();
    ege::solidrect(5, 5, 34, 28, image);
    expectSolidFillOnly("solidrect");

    prepare();
    ege::solidellipse(24, 20, 13, 9, image);
    expectSolidFillOnly("solidellipse");

    prepare();
    ege::solidpie(24, 20, 20, 155, 14, 11, image);
    expectSolidFillOnly("solidpie");

    prepare();
    ege::solidroundrect(5, 5, 38, 30, 6, 4, image);
    expectSolidFillOnly("solidroundrect");

    prepare();
    ege::solidroundrect(5, 5, 38, 30, 6, image);
    expectSolidFillOnly("single-radius solidroundrect");

    prepare();
    ege::solidpoly(4, polygonPoints, image);
    expectSolidFillOnly("solidpoly");

    ege::delimage(image);
}

void testEnhancedFillAndCornerRadiusCompatibility()
{
    ege::PIMAGE image = ege::newimage(48, 40);
    const ege::ege_point polygonPoints[] = {
        {4.0f, 5.0f}, {34.0f, 7.0f}, {30.0f, 30.0f}, {8.0f, 32.0f}};

    const auto prepare = [image]() {
        resetImage(image, ege::BLACK);
        ege::ege_setpattern_none(image);
        ege::setfillcolor(ege::GREEN, image);
        ege::setlinecolor(ege::RED, image);
        ege::setlinestyle(ege::SOLID_LINE, 0, 1, image);
    };
    const auto expectEnhancedFillOnly = [image](const std::string& shape) {
        expect(countPixelsEqualTo(image, ege::GREEN) > 0,
               shape + " paints the current enhanced brush");
        expect(countPixelsEqualTo(image, ege::RED) == 0,
               shape + " does not implicitly paint the current pen");
    };

    prepare();
    ege::ege_fillrect(5.0f, 5.0f, 29.0f, 23.0f, image);
    expectEnhancedFillOnly("ege_fillrect");

    prepare();
    ege::ege_fillellipse(10.0f, 10.0f, 28.0f, 18.0f, image);
    expectEnhancedFillOnly("ege_fillellipse");

    prepare();
    ege::ege_fillcircle(24.0f, 20.0f, 10.0f, image);
    expectEnhancedFillOnly("ege_fillcircle");

    prepare();
    ege::ege_fillpoly(4, polygonPoints, image);
    expectEnhancedFillOnly("ege_fillpoly");

    prepare();
    ege::ege_fillroundrect(5.0f, 5.0f, 33.0f, 25.0f, 6.0f, image);
    expectEnhancedFillOnly("ege_fillroundrect");

    prepare();
    ege::ege_fillroundrect(5.0f, 5.0f, 30.0f, 24.0f,
                           8.0f, 8.0f, 0.0f, 0.0f, image);
    expectEnhancedFillOnly("four-radius ege_fillroundrect");
    expectPixel(image, 5, 5, ege::BLACK,
                "four-radius round rectangle rounds its top-left corner");
    expectPixel(image, 34, 5, ege::BLACK,
                "four-radius round rectangle rounds its top-right corner");
    expectPixel(image, 5, 28, ege::GREEN,
                "four-radius round rectangle keeps a square bottom-left corner");
    expectPixel(image, 34, 28, ege::GREEN,
                "four-radius round rectangle keeps a square bottom-right corner");

    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::GREEN, image);
    ege::setviewport(7, 5, 20, 18, true, image);
    ege::ege_fillrect(-4.0f, -4.0f, 10.0f, 10.0f, image);
    ege::setviewport(0, 0, 48, 40, false, image);
    expectPixel(image, 7, 5, ege::GREEN,
                "enhanced drawing applies the viewport origin and clips at its top-left edge");
    expectPixel(image, 12, 10, ege::GREEN,
                "enhanced drawing keeps pixels inside the translated viewport");
    expectPixel(image, 6, 5, ege::BLACK,
                "enhanced drawing does not escape the viewport clip region");
    expectPixel(image, 3, 1, ege::BLACK,
                "enhanced drawing does not use untranslated coordinates");

    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::GREEN, image);
    ege::setviewport(7, 5, 20, 18, true, image);
    expect(ege::resize_f(image, 56, 44) == ege::grOk,
           "resizing an image with an enhanced viewport succeeds");
    ege::ege_fillrect(0.0f, 0.0f, 4.0f, 4.0f, image);
    ege::setviewport(0, 0, 56, 44, false, image);
    expectPixel(image, 7, 5, ege::GREEN,
                "enhanced drawing preserves the viewport after an OpenGL buffer rebuild");
    expectPixel(image, 0, 0, ege::BLACK,
                "a rebuilt enhanced drawing surface does not lose its viewport origin");

    ege::delimage(image);
}

void testEnhancedStrokeStateCompatibility()
{
    ege::PIMAGE image = ege::newimage(48, 40);
    const ege::ege_point triangle[] = {
        {24.0f, 4.0f}, {6.0f, 32.0f}, {42.0f, 32.0f}, {24.0f, 4.0f}};

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::RED, image);
    ege::setfillcolor(ege::YELLOW, image);
    ege::ege_fillpoly(3, triangle, image);
    ege::ege_drawpoly(4, triangle, image);
    expect(countPixelsEqualTo(image, ege::YELLOW) > 0,
           "ege_fillpoly paints the demo polygon fill");
    expect(countPixelsEqualTo(image, ege::RED) > 0,
           "ege_drawpoly uses the current line color after an enhanced fill");

    resetImage(image, ege::BLACK);
    ege::setlinestyle(ege::SOLID_LINE, 0, 1, image);
    ege::setlinecolor(ege::RED, image);
    ege::setlinewidth(5.0f, image);
    ege::ege_line(6.0f, 20.0f, 41.0f, 20.0f, image);
    expect(countPixelsEqualToInRect(image, ege::RED, 5, 16, 43, 25) > 80,
           "ege_line uses the current enhanced line width");

    ege::delimage(image);
}

void testFillPatterns()
{
    ege::PIMAGE image = ege::newimage(16, 16);
    resetImage(image, ege::BLACK);
    ege::setbkcolor(ege::BLUE, image);

    ege::setfillstyle(ege::LINE_FILL, ege::RED, image);
    ege::bar(0, 0, 15, 15, image);
    expectEightPixelFillPattern(image, ege::RED, ege::BLUE, "LINE_FILL");

    resetImage(image, ege::BLACK);
    ege::setbkcolor(ege::BLUE, image);
    ege::setfillstyle(ege::LTSLASH_FILL, ege::GREEN, image);
    ege::bar(0, 0, 15, 15, image);
    expectEightPixelFillPattern(image, ege::GREEN, ege::BLUE, "LTSLASH_FILL");

    resetImage(image, ege::BLACK);
    ege::setbkcolor(ege::BLUE, image);
    ege::setfillstyle(ege::WIDE_DOT_FILL, ege::YELLOW, image);
    ege::bar(0, 0, 15, 15, image);
    expectPixel(image, 0, 0, ege::YELLOW, "WIDE_DOT_FILL paints the tile's sparse dot");
    expectPixel(image, 1, 0, ege::BLUE, "WIDE_DOT_FILL keeps sparse-dot gaps as background");

    ege::delimage(image);
}

void testUserLinePatternAndCaps()
{
    ege::PIMAGE image = ege::newimage(32, 20);
    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinecap(ege::LINECAP_FLAT, image);
    ege::setlinestyle(ege::USERBIT_LINE, 0xAAAA, 1, image);
    ege::line(2, 3, 17, 3, image);
    expectPixel(image, 2, 3, ege::WHITE,
                "USERBIT_LINE starts with the first GDI dash segment");
    expectPixel(image, 3, 3, ege::BLACK,
                "USERBIT_LINE follows the first dash with a gap");
    expectPixel(image, 4, 3, ege::WHITE,
                "USERBIT_LINE repeats the alternating dash");
    expectPixel(image, 5, 3, ege::BLACK,
                "USERBIT_LINE repeats the alternating gap");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::USERBIT_LINE, 0xFFF0, 1, image);
    ege::line(2, 3, 30, 3, image);
    expectPixel(image, 2, 3, ege::WHITE,
                "USERBIT_LINE rotates an initial zero run behind the first dash");
    expectPixel(image, 13, 3, ege::WHITE,
                "USERBIT_LINE keeps the complete leading dash run");
    expectPixel(image, 14, 3, ege::BLACK,
                "USERBIT_LINE preserves the rotated trailing gap run");
    expectPixel(image, 18, 3, ege::WHITE,
                "USERBIT_LINE repeats its 16-unit pattern");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::SOLID_LINE, 0, 6, image);
    ege::setlinecap(ege::LINECAP_FLAT, image);
    ege::line(8, 10, 20, 10, image);
    expectPixel(image, 6, 10, ege::BLACK, "flat line caps do not extend before the endpoint");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::SOLID_LINE, 0, 6, image);
    ege::setlinecap(ege::LINECAP_SQUARE, image);
    ege::line(8, 10, 20, 10, image);
    expectPixel(image, 6, 10, ege::WHITE, "square line caps extend by half the stroke width");
    expectPixel(image, 5, 8, ege::WHITE, "square line caps retain their corner pixels");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::SOLID_LINE, 0, 6, image);
    ege::setlinecap(ege::LINECAP_ROUND, image);
    ege::line(8, 10, 20, 10, image);
    expectPixel(image, 6, 10, ege::WHITE, "round line caps extend around the endpoint");
    expectPixel(image, 5, 8, ege::BLACK, "round line caps omit square-corner pixels");

    const int corner[] = {6, 14, 14, 14, 14, 6};
    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::SOLID_LINE, 0, 8, image);
    ege::setlinecap(ege::LINECAP_FLAT, image);
    ege::setlinejoin(ege::LINEJOIN_MITER, 10.0f, image);
    ege::polyline(3, corner, image);
    expectPixel(image, 17, 17, ege::WHITE, "miter joins extend to the offset-line intersection");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::SOLID_LINE, 0, 8, image);
    ege::setlinecap(ege::LINECAP_FLAT, image);
    ege::setlinejoin(ege::LINEJOIN_BEVEL, image);
    ege::polyline(3, corner, image);
    expectPixel(image, 17, 17, ege::BLACK, "bevel joins clip the miter corner");
    expectPixel(image, 17, 16, ege::BLACK, "bevel joins use a straight outer edge");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::SOLID_LINE, 0, 8, image);
    ege::setlinecap(ege::LINECAP_FLAT, image);
    ege::setlinejoin(ege::LINEJOIN_ROUND, image);
    ege::polyline(3, corner, image);
    expectPixel(image, 17, 16, ege::WHITE, "round joins retain the circular corner arc");
    expectPixel(image, 17, 17, ege::BLACK, "round joins omit the square miter corner");

    ege::delimage(image);
}

void testCurvedLineStyles()
{
    ege::PIMAGE solid = ege::newimage(48, 40);
    ege::PIMAGE dashed = ege::newimage(48, 40);
    resetImage(solid, ege::BLACK);
    resetImage(dashed, ege::BLACK);
    ege::setlinecolor(ege::WHITE, solid);
    ege::setlinecolor(ege::WHITE, dashed);
    ege::setlinestyle(ege::SOLID_LINE, 0, 1, solid);
    ege::setlinestyle(ege::DASHED_LINE, 0, 1, dashed);
    ege::circle(18, 18, 13, solid);
    ege::circle(18, 18, 13, dashed);

    const ege::color_t* solidPixels = ege::getbuffer(solid);
    const ege::color_t* dashedPixels = ege::getbuffer(dashed);
    int dashedPainted = 0;
    int curvedGaps = 0;
    for (int i = 0; i < 48 * 40; ++i) {
        if (rgb(dashedPixels[i]) != rgb(ege::BLACK)) ++dashedPainted;
        if (rgb(solidPixels[i]) != rgb(ege::BLACK) &&
            rgb(dashedPixels[i]) == rgb(ege::BLACK)) {
            ++curvedGaps;
        }
    }
    expect(dashedPainted > 20, "a dashed circle still draws visible dash segments");
    expect(curvedGaps > 8, "a dashed circle keeps its dash phase across curve segments");

    resetImage(solid, ege::BLACK);
    resetImage(dashed, ege::BLACK);
    ege::setlinecolor(ege::WHITE, solid);
    ege::setlinecolor(ege::WHITE, dashed);
    ege::setlinestyle(ege::SOLID_LINE, 0, 1, solid);
    ege::setlinestyle(ege::DASHED_LINE, 0, 1, dashed);
    ege::roundrect(3, 3, 38, 32, 10, solid);
    ege::roundrect(3, 3, 38, 32, 10, dashed);
    solidPixels = ege::getbuffer(solid);
    dashedPixels = ege::getbuffer(dashed);
    curvedGaps = 0;
    for (int i = 0; i < 48 * 40; ++i) {
        if (rgb(solidPixels[i]) != rgb(ege::BLACK) &&
            rgb(dashedPixels[i]) == rgb(ege::BLACK)) {
            ++curvedGaps;
        }
    }
    expect(curvedGaps > 8,
           "a dashed rounded rectangle keeps its dash phase across corner segments");

    resetImage(dashed, ege::BLACK);
    ege::setlinecolor(ege::WHITE, dashed);
    ege::setlinestyle(ege::NULL_LINE, 0, 1, dashed);
    ege::pie(20, 18, 20, 140, 12, 9, dashed);
    expect(countPixelsDifferentFrom(dashed, ege::BLACK) == 0,
           "NULL_LINE suppresses the complete pie outline");

    ege::delimage(dashed);
    ege::delimage(solid);
}

void testScreenFramebufferCapture()
{
    ege::settarget(nullptr);
    ege::setviewport(0, 0, 64, 64, true);
    ege::setbkcolor(ege::BLACK);
    ege::cleardevice();
    ege::setlinecolor(ege::RED);
    ege::line(3, 5, 20, 5);
    ege::setfillcolor(ege::GREEN);
    ege::bar(8, 12, 18, 22);

    ege::PIMAGE capture = ege::newimage();
    expect(ege::getimage(capture, 0, 0, 32, 32) == ege::grOk,
           "screen framebuffer can be captured into an IMAGE");
    expectPixel(capture, 10, 5, ege::RED,
                "captured screen contains native line rendering");
    expectPixel(capture, 12, 16, ege::GREEN,
                "captured screen contains native fill rendering");
    expectPixel(capture, 30, 30, ege::BLACK,
                "captured screen preserves the clear color");
    ege::delimage(capture);
}

void testScreenPutimageOverloads()
{
    ege::PIMAGE source = ege::newimage(2, 2);
    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = ege::RED;
    sourcePixels[1] = ege::GREEN;
    sourcePixels[2] = ege::BLUE;
    sourcePixels[3] = ege::WHITE;

    ege::settarget(nullptr);
    ege::setviewport(0, 0, 64, 64, true);
    ege::setbkcolor(ege::BLACK);
    ege::cleardevice();

    ege::putimage(1, 1, source);
    ege::putimage(5, 1, 1, 1, source, 1, 0);
    ege::putimage(8, 1, 2, 2, source, 0, 1, 1, 1);

    ege::PIMAGE capture = ege::newimage();
    expect(ege::getimage(capture, 0, 0, 12, 4) == ege::grOk,
           "screen putimage overload results can be captured");
    expectPixel(capture, 1, 1, ege::RED,
                "screen whole-image putimage overload draws its top-left pixel");
    expectPixel(capture, 2, 2, ege::WHITE,
                "screen whole-image putimage overload draws its full extent");
    expectPixel(capture, 5, 1, ege::GREEN,
                "screen source-region putimage overload selects the requested pixel");
    expectPixel(capture, 9, 2, ege::BLUE,
                "screen stretched putimage overload scales the requested source region");

    ege::delimage(capture);
    ege::delimage(source);
}

void testPrimitiveBatchRetention()
{
    ege::PIMAGE image = ege::newimage(32, 24);
    resetImage(image, ege::BLACK);

    ege::setlinecolor(ege::RED, image);
    ege::line(2, 2, 12, 2, image);
    ege::ellipse(22, 12, 0, 360, 5, 4, image);

    expectPixel(image, 6, 2, ege::RED, "drawing an ellipse does not discard an earlier line");

    ege::delimage(image);
}

void testPolygonCoordinates()
{
    ege::PIMAGE image = ege::newimage(16, 16);
    resetImage(image, ege::BLACK);

    const int triangle[] = {2, 2, 13, 2, 7, 13};
    ege::setfillcolor(ege::MAGENTA, image);
    ege::fillpoly(3, triangle, image);

    expectPixel(image, 7, 6, ege::MAGENTA, "fillpoly consumes interleaved x/y coordinates");
    expectPixel(image, 1, 1, ege::BLACK, "fillpoly does not corrupt pixels outside the polygon");

    ege::delimage(image);
}

void testViewportOriginAndClip()
{
    ege::PIMAGE image = ege::newimage(16, 16);
    resetImage(image, ege::BLACK);

    ege::setviewport(4, 3, 12, 11, true, image);
    ege::putpixel(0, 0, ege::YELLOW, image);
    ege::putpixel(8, 8, ege::RED, image);

    ege::setviewport(0, 0, 16, 16, false, image);
    expectPixel(image, 4, 3, ege::YELLOW, "viewport left/top becomes the logical origin");
    expectPixel(image, 12, 11, ege::BLACK, "viewport clipping excludes the right/bottom edge");

    ege::delimage(image);
}

void testBufferMutationFeedsImageTransfer()
{
    ege::PIMAGE source = ege::newimage(4, 4);
    ege::PIMAGE destination = ege::newimage(8, 8);
    resetImage(source, ege::BLACK);
    resetImage(destination, ege::BLACK);

    ege::color_t* pixels = ege::getbuffer(source);
    expect(pixels != nullptr, "getbuffer returns writable storage");
    if (pixels) {
        pixels[1 * 4 + 2] = ege::CYAN;
    }

    ege::putimage(destination, 1, 2, source);
    expectPixel(destination, 3, 3, ege::CYAN, "putimage observes direct getbuffer mutations");

    ege::delimage(destination);
    ege::delimage(source);
}

void testConstAndScreenBufferSynchronization()
{
    ege::PIMAGE image = ege::newimage(4, 4);
    resetImage(image, ege::BLACK);
    ege::putpixel(2, 1, ege::YELLOW, image);

    ege::PCIMAGE readOnlyImage = image;
    const ege::color_t* readOnlyPixels = ege::getbuffer(readOnlyImage);
    expect(readOnlyPixels != nullptr && rgb(readOnlyPixels[1 * 4 + 2]) == rgb(ege::YELLOW),
           "const getbuffer overload reads the latest rendered pixels");
    ege::putpixel(3, 2, ege::CYAN, image);
    expectPixel(image, 3, 2, ege::CYAN,
                "GPU drawing remains valid after a read-only buffer access");

    ege::settarget(nullptr);
    ege::setviewport(0, 0, 64, 64, true);
    ege::setbkcolor(ege::BLACK);
    ege::cleardevice();
    ege::color_t* screenPixels = ege::getbuffer(static_cast<ege::PIMAGE>(nullptr));
    expect(screenPixels != nullptr, "screen getbuffer returns writable storage");
    if (screenPixels) {
        screenPixels[6 * 64 + 5] = ege::MAGENTA;
    }

    ege::PIMAGE stamp = ege::newimage(2, 2);
    resetImage(stamp, ege::RED);
    ege::putimage(nullptr, 8, 8, stamp);

    ege::PIMAGE capture = ege::newimage();
    expect(ege::getimage(capture, 0, 0, 12, 12) == ege::grOk,
           "screen buffer edits can be captured without presenting first");
    expectPixel(capture, 5, 6, ege::MAGENTA,
                "an immediate image draw preserves earlier screen getbuffer mutations");
    expectPixel(capture, 8, 8, ege::RED,
                "screen image drawing remains valid after direct buffer mutations");

    ege::delimage(stamp);
    ege::delimage(capture);
    ege::delimage(image);
}

void testGetImageSourceClipping()
{
    ege::PIMAGE source = ege::newimage(2, 2);
    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = ege::RED;
    sourcePixels[1] = ege::GREEN;
    sourcePixels[2] = ege::BLUE;
    sourcePixels[3] = ege::WHITE;

    ege::PIMAGE clipped = ege::newimage();
    expect(ege::getimage(clipped, source, -1, -1, 4, 4) == ege::grOk,
           "getimage accepts a source rectangle extending past every edge");
    expect(ege::getwidth(clipped) == 4 && ege::getheight(clipped) == 4,
           "getimage keeps the requested destination dimensions while clipping the source");
    expectPixel(clipped, 1, 1, ege::RED,
                "getimage maps the clipped source origin into the destination");
    expectPixel(clipped, 2, 2, ege::WHITE,
                "getimage copies the clipped source bottom-right pixel");
    expectPixel(clipped, 0, 0, ege::BLACK,
                "getimage clears pixels preceding the clipped source rectangle");
    expectPixel(clipped, 3, 3, ege::BLACK,
                "getimage clears pixels following the clipped source rectangle");

    ege::delimage(clipped);
    ege::delimage(source);
}

void testMixedBackendBufferSynchronization(ege::PIMAGE legacySource,
                                           ege::PIMAGE legacyDestination)
{
    if (!legacySource || !legacyDestination) return;

    ege::color_t* legacySourcePixels = ege::getbuffer(legacySource);
    std::fill(legacySourcePixels, legacySourcePixels + 6, ege::MAGENTA);
    legacySourcePixels[0] = ege::RED;
    legacySourcePixels[1] = ege::GREEN;
    legacySourcePixels[3] = ege::BLUE;
    legacySourcePixels[4] = ege::WHITE;

    ege::PIMAGE gpuDestination = ege::newimage(3, 2);
#ifdef _WIN32
    expect(ege::getHDC(legacySource) != NULL,
           "getHDC remains available for a legacy GDI image in an OpenGL process");
    expect(ege::getHDC(gpuDestination) == NULL,
           "getHDC reports that an OpenGL render target has no GDI device context");
#endif
    resetImage(gpuDestination, ege::BLACK);
    ege::putimage(gpuDestination, 0, 0, legacySource);
    expectPixel(gpuDestination, 1, 0, ege::GREEN,
                "putimage copies a legacy GDI image into an OpenGL image");

    resetImage(gpuDestination, ege::BLACK);
    ege::putimage_transparent(gpuDestination, legacySource, 0, 0, ege::MAGENTA);
    expectPixel(gpuDestination, 0, 1, ege::BLUE,
                "transparent transfer synchronizes a GDI source and OpenGL destination");
    expectPixel(gpuDestination, 2, 1, ege::BLACK,
                "mixed transparent transfer preserves keyed destination pixels");

    const ege::color_t premultipliedRed = EGEARGB(128, 128, 0, 0);
    legacySourcePixels = ege::getbuffer(legacySource);
    legacySourcePixels[0] = premultipliedRed;
    resetImage(gpuDestination, ege::BLUE);
    expect(ege::putimage_alphablend(gpuDestination, legacySource, 0, 0, 255,
                                    ege::COLORTYPE_PRGB32) == ege::grOk,
           "premultiplied alpha blend accepts a GDI source and OpenGL destination");
    expectPixel(gpuDestination, 0, 0,
                ege::alphablend_premultiplied(ege::BLUE, premultipliedRed),
                "mixed premultiplied alpha blend composites synchronized pixels");

    ege::PIMAGE cropped = ege::newimage();
    expect(ege::getimage(cropped, legacySource, 0, 0, 3, 2) == ege::grOk,
           "getimage accepts a GDI source and OpenGL destination");
    expectPixel(cropped, 1, 1, ege::WHITE,
                "mixed-backend getimage copies source pixels instead of the screen framebuffer");

    ege::PIMAGE clipped = ege::newimage();
    expect(ege::getimage(clipped, legacySource, -1, -1, 5, 4) == ege::grOk,
           "mixed-backend getimage accepts a source rectangle outside the source image");
    expectPixel(clipped, 1, 1, premultipliedRed,
                "mixed-backend getimage clips the negative source origin");
    expectPixel(clipped, 3, 2, ege::MAGENTA,
                "mixed-backend getimage clips the positive source extent");
    expectPixel(clipped, 0, 0, ege::BLACK,
                "mixed-backend getimage clears the leading clipped region");
    expectPixel(clipped, 4, 3, ege::BLACK,
                "mixed-backend getimage clears the trailing clipped region");

    ege::PIMAGE gpuSource = ege::newimage(3, 2);
    resetImage(gpuSource, ege::MAGENTA);
    ege::putpixel(2, 0, ege::CYAN, gpuSource);
    ege::color_t* legacyDestinationPixels = ege::getbuffer(legacyDestination);
    std::fill(legacyDestinationPixels, legacyDestinationPixels + 6, ege::BLACK);
    ege::putimage_transparent(legacyDestination, gpuSource, 0, 0, ege::MAGENTA);
    expectPixel(legacyDestination, 2, 0, ege::CYAN,
                "transparent transfer synchronizes an OpenGL source and GDI destination");

    ege::color_t* gpuSourcePixels = ege::getbuffer(gpuSource);
    gpuSourcePixels[0] = premultipliedRed;
    legacyDestinationPixels = ege::getbuffer(legacyDestination);
    std::fill(legacyDestinationPixels, legacyDestinationPixels + 6, ege::BLUE);
    expect(ege::putimage_withalpha(legacyDestination, gpuSource,
                                   0, 0, 0, 0, 1, 1) == ege::grOk,
           "with-alpha accepts an OpenGL source and GDI destination");
    expectPixel(legacyDestination, 0, 0,
                ege::alphablend_premultiplied(ege::BLUE, premultipliedRed),
                "mixed with-alpha composites synchronized pixels");

    ege::delimage(gpuSource);
    ege::delimage(clipped);
    ege::delimage(cropped);
    ege::delimage(gpuDestination);
}

void testImageTransfersHonorViewportOrigin()
{
    ege::PIMAGE source = ege::newimage(2, 1);
    ege::PIMAGE destination = ege::newimage(16, 12);
    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = ege::RED;
    sourcePixels[1] = ege::CYAN;

    const auto prepareDestination = [&]() {
        resetImage(destination, ege::BLACK);
        ege::setviewport(4, 3, 12, 10, true, destination);
    };
    const auto expectAtViewportOrigin = [&](const std::string& operation) {
        ege::setviewport(0, 0, 16, 12, false, destination);
        expectPixel(destination, 4, 3, ege::RED,
                    operation + " applies the destination viewport origin exactly once");
        expectPixel(destination, 8, 6, ege::BLACK,
                    operation + " does not apply the destination viewport origin twice");
    };

    prepareDestination();
    ege::putimage(destination, 0, 0, source);
    expectAtViewportOrigin("putimage");

    prepareDestination();
    ege::putimage_transparent(destination, source, 0, 0, ege::MAGENTA);
    expectAtViewportOrigin("putimage_transparent");

    prepareDestination();
    ege::putimage_alphablend(destination, source, 0, 0, 255,
                             ege::COLORTYPE_RGB32);
    expectAtViewportOrigin("putimage_alphablend");

    prepareDestination();
    ege::putimage_withalpha(destination, source, 0, 0);
    expectAtViewportOrigin("putimage_withalpha");

    ege::delimage(destination);
    ege::delimage(source);
}

void testImageTransfersHonorSourceViewportOrigin()
{
    ege::PIMAGE source = ege::newimage(6, 4);
    ege::PIMAGE destination = ege::newimage(4, 4);
    ege::color_t* sourcePixels = ege::getbuffer(source);
    std::fill(sourcePixels, sourcePixels + 24, EGEARGB(255, 0, 0, 0));
    sourcePixels[0] = EGEARGB(255, 0, 255, 0);
    sourcePixels[1 * 6 + 2] = EGEARGB(255, 255, 0, 0);
    ege::setviewport(2, 1, 6, 4, true, source);

    resetImage(destination, ege::BLUE);
    ege::putimage(destination, 0, 0, 1, 1, source, 0, 0);
    expectPixel(destination, 0, 0, ege::RED,
                "putimage source coordinates are relative to the source viewport");

    resetImage(destination, ege::BLUE);
    ege::putimage(destination, 0, 0, 2, 2, source, 0, 0, 1, 1);
    expectPixel(destination, 1, 1, ege::RED,
                "stretched putimage applies the source viewport origin");

    ege::PIMAGE cropped = ege::newimage();
    expect(ege::getimage(cropped, source, 0, 0, 1, 1) == ege::grOk,
           "getimage accepts source coordinates relative to a viewport");
    expectPixel(cropped, 0, 0, ege::RED,
                "getimage applies the source viewport origin");

    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphablend(destination, source, 0, 0, 255,
                                    0, 0, 1, 1,
                                    ege::COLORTYPE_PRGB32) == ege::grOk,
           "premultiplied alpha transfer with a source viewport reports success");
    expectPixel(destination, 0, 0, ege::RED,
                "premultiplied alpha transfer applies the source viewport origin");

    // The legacy RGB32/ARGB32 overload is a software-buffer operation and
    // historically treats xSrc/ySrc as physical buffer coordinates. Keep
    // that obscure distinction while PRGB32 follows the source HDC viewport.
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphablend(destination, source, 0, 0, 255,
                                    0, 0, 1, 1,
                                    ege::COLORTYPE_RGB32) == ege::grOk,
           "RGB32 alpha transfer with a source viewport reports success");
    expectPixel(destination, 0, 0, EGEARGB(255, 0, 255, 0),
                "RGB32 software alpha transfer retains legacy physical source coordinates");

    resetImage(destination, ege::BLUE);
    expect(ege::putimage_transparent(destination, source, 0, 0,
                                     ege::MAGENTA, 0, 0, 1, 1) == ege::grOk,
           "color-key transfer with a source viewport reports success");
    expectPixel(destination, 0, 0, EGEARGB(255, 0, 255, 0),
                "color-key software transfer retains legacy physical source coordinates");

    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphablend(destination, source,
                                    0, 0, 2, 2, 255,
                                    0, 0, 1, 1, false,
                                    ege::COLORTYPE_RGB32) == ege::grOk,
           "stretched alpha transfer with a source viewport reports success");
    expectPixel(destination, 1, 1, ege::RED,
                "stretched alpha transfer applies the source viewport origin");

    resetImage(destination, ege::BLUE);
    expect(ege::putimage_withalpha(destination, source, 0, 0, 0, 0, 1, 1) == ege::grOk,
           "with-alpha transfer with a source viewport reports success");
    expectPixel(destination, 0, 0, ege::RED,
                "with-alpha transfer applies the source viewport origin");

    resetImage(destination, ege::BLUE);
    expect(ege::putimage_withalpha(destination, source,
                                   0, 0, 2, 2,
                                   0, 0, 1, 1, false) == ege::grOk,
           "stretched with-alpha transfer with a source viewport reports success");
    expectPixel(destination, 1, 1, ege::RED,
                "stretched with-alpha transfer applies the source viewport origin");

    ege::delimage(cropped);
    ege::delimage(destination);
    ege::delimage(source);
}

void testColorAndMathUtilities()
{
    const ege::color_t source = EGEARGB(128, 200, 100, 50);
    const ege::color_t premultiplied = ege::color_premultiply(source);
    const ege::color_t restored = ege::color_unpremultiply(premultiplied);
    expect(EGEGET_A(restored) == 128 &&
           std::abs(static_cast<int>(EGEGET_R(restored)) - 200) <= 2 &&
           std::abs(static_cast<int>(EGEGET_G(restored)) - 100) <= 2 &&
           std::abs(static_cast<int>(EGEGET_B(restored)) - 50) <= 2,
           "premultiply and unpremultiply preserve ARGB within rounding tolerance");

    expect(rgb(ege::rgb2gray(ege::WHITE)) == rgb(ege::WHITE),
           "rgb2gray preserves white");
    float hue = 0.0f;
    float saturation = 0.0f;
    float lightness = 0.0f;
    ege::rgb2hsl(EGERGB(40, 120, 200), &hue, &saturation, &lightness);
    const ege::color_t hslRoundTrip = ege::hsl2rgb(hue, saturation, lightness);
    expect(std::abs(static_cast<int>(EGEGET_R(hslRoundTrip)) - 40) <= 1 &&
           std::abs(static_cast<int>(EGEGET_G(hslRoundTrip)) - 120) <= 1 &&
           std::abs(static_cast<int>(EGEGET_B(hslRoundTrip)) - 200) <= 1,
           "RGB and HSL conversions round-trip representative input");

    float value = 0.0f;
    ege::rgb2hsv(EGERGB(40, 120, 200), &hue, &saturation, &value);
    const ege::color_t hsvRoundTrip = ege::hsv2rgb(hue, saturation, value);
    expect(std::abs(static_cast<int>(EGEGET_R(hsvRoundTrip)) - 40) <= 1 &&
           std::abs(static_cast<int>(EGEGET_G(hsvRoundTrip)) - 120) <= 1 &&
           std::abs(static_cast<int>(EGEGET_B(hsvRoundTrip)) - 200) <= 1,
           "RGB and HSV conversions round-trip representative input");

    expect(rgb(ege::colorblend(ege::BLUE, ege::RED, 0)) == rgb(ege::BLUE) &&
           rgb(ege::colorblend(ege::BLUE, ege::RED, 255)) == rgb(ege::RED),
           "colorblend honors transparent and opaque endpoints");
    expect(rgb(ege::colorblend_f(ege::BLUE, ege::RED, 0)) == rgb(ege::BLUE) &&
           rgb(ege::colorblend_f(ege::BLUE, ege::RED, 255)) == rgb(ege::RED),
           "colorblend_f honors transparent and opaque endpoints");
    expect(rgb(ege::alphablend(ege::BLUE, EGEARGB(255, 255, 0, 0))) == rgb(ege::RED) &&
           rgb(ege::alphablend(ege::BLUE, EGEARGB(255, 255, 0, 0), 255)) == rgb(ege::RED),
           "alphablend overloads honor an opaque source");
    const ege::color_t premultipliedRed = ege::color_premultiply(EGEARGB(255, 255, 0, 0));
    expect(rgb(ege::alphablend_premultiplied(ege::BLUE, premultipliedRed)) == rgb(ege::RED) &&
           rgb(ege::alphablend_premultiplied(ege::BLUE, premultipliedRed, 255)) == rgb(ege::RED),
           "premultiplied alphablend overloads honor an opaque source");

    const float halfPi = static_cast<float>(ege::PI / 2.0);
    ege::VECTOR3D pointX(0.0f, 1.0f, 0.0f);
    ege::rotate_point3d_x(&pointX, halfPi);
    expect(std::abs(pointX.y) < 0.001f && std::abs(pointX.z - 1.0f) < 0.001f,
           "rotate_point3d_x rotates around the x axis");
    ege::VECTOR3D pointY(0.0f, 0.0f, 1.0f);
    ege::rotate_point3d_y(&pointY, halfPi);
    expect(std::abs(pointY.x - 1.0f) < 0.001f && std::abs(pointY.z) < 0.001f,
           "rotate_point3d_y rotates around the y axis");
    ege::VECTOR3D pointZ(1.0f, 0.0f, 0.0f);
    ege::rotate_point3d_z(&pointZ, halfPi);
    expect(std::abs(pointZ.x) < 0.001f && std::abs(pointZ.y - 1.0f) < 0.001f,
           "rotate_point3d_z rotates around the z axis");
}

void testStateAndPixelUtilities()
{
    ege::PIMAGE image = ege::newimage(10, 8);
    resetImage(image, ege::BLACK);

    ege::setlinecolor(ege::RED, image);
    ege::setfillcolor(ege::GREEN, image);
    ege::settextcolor(ege::YELLOW, image);
    ege::setbkcolor_f(ege::BLUE, image);
    expect(ege::getcolor(image) == ege::RED, "legacy color state follows the line color");
    expect(ege::getlinecolor(image) == ege::RED, "line color state round-trips");
    expect(ege::getfillcolor(image) == ege::GREEN, "fill color state round-trips");
    expect(ege::gettextcolor(image) == ege::YELLOW, "text color state round-trips");
    expect(ege::getbkcolor(image) == ege::BLUE, "background color state round-trips");

    resetImage(image, ege::BLACK);
    ege::putpixel(9, 7, ege::RED, image);
    ege::setbkcolor(ege::BLUE, image);
    expectPixel(image, 0, 0, ege::BLUE,
                "setbkcolor replaces pixels using the previous background color");
    expectPixel(image, 9, 7, ege::RED,
                "setbkcolor preserves pixels that differ from the previous background color");

    const int points[] = {1, 1, static_cast<int>(ege::RED),
                          2, 2, static_cast<int>(ege::CYAN)};
    ege::putpixels(2, points, image);
    expectPixel(image, 1, 1, ege::RED, "putpixels uses each point's supplied color");
    expectPixel(image, 2, 2, ege::CYAN, "putpixels writes multiple colored points");

    const int fastPoints[] = {3, 1, static_cast<int>(ege::YELLOW),
                              3, 2, static_cast<int>(ege::MAGENTA)};
    ege::putpixels_f(2, fastPoints, image);
    expect(rgb(ege::getpixel_f(3, 1, image)) == rgb(ege::YELLOW) &&
           rgb(ege::getpixel_f(3, 2, image)) == rgb(ege::MAGENTA),
           "fast batch pixel APIs preserve supplied colors");

    ege::color_t* mutablePixels = ege::getbuffer(image);
    mutablePixels[0] = EGEARGB(64, 10, 20, 30);
    ege::putpixel_savealpha(0, 0, ege::RED, image);
    expect(EGEGET_A(ege::getpixel_f(0, 0, image)) == 64 &&
           rgb(ege::getpixel_f(0, 0, image)) == rgb(ege::RED),
           "putpixel_savealpha replaces RGB while preserving alpha");
    ege::putpixel_savealpha_f(0, 0, ege::GREEN, image);
    expect(EGEGET_A(ege::getpixel_f(0, 0, image)) == 64 &&
           rgb(ege::getpixel_f(0, 0, image)) == rgb(ege::GREEN),
           "putpixel_savealpha_f preserves alpha");

    mutablePixels = ege::getbuffer(image);
    mutablePixels[1] = EGEARGB(91, 0, 0, 255);
    ege::putpixel_withalpha(1, 0, EGEARGB(128, 255, 0, 0), image);
    expect(EGEGET_A(ege::getpixel_f(1, 0, image)) == 91,
           "putpixel_withalpha preserves destination alpha");
    ege::putpixel_withalpha_f(1, 0, EGEARGB(128, 0, 255, 0), image);
    expect(EGEGET_A(ege::getpixel_f(1, 0, image)) == 91,
           "putpixel_withalpha_f preserves destination alpha");

    mutablePixels = ege::getbuffer(image);
    mutablePixels[2] = ege::BLUE;
    const ege::color_t alphaSource = EGEARGB(128, 255, 0, 0);
    ege::putpixel_alphablend(2, 0, alphaSource, image);
    expect(ege::getpixel_f(2, 0, image) == ege::alphablend(ege::BLUE, alphaSource),
           "putpixel_alphablend matches the scalar color helper");
    mutablePixels = ege::getbuffer(image);
    mutablePixels[2] = ege::BLUE;
    ege::putpixel_alphablend_f(2, 0, alphaSource, image);
    expect(ege::getpixel_f(2, 0, image) == ege::alphablend(ege::BLUE, alphaSource),
           "putpixel_alphablend_f matches the scalar color helper");

    ege::ege_setalpha(37, image);
    const ege::color_t* pixels = ege::getbuffer(image);
    expect(EGEGET_A(pixels[1 * 10 + 1]) == 37, "ege_setalpha updates offscreen image alpha");

    // A subsequent GPU primitive must preserve CPU-side pixel API changes.
    ege::putpixel_f(4, 4, ege::MAGENTA, image);
    ege::setfillcolor(ege::WHITE, image);
    ege::bar(7, 5, 9, 7, image);
    expectPixel(image, 4, 4, ege::MAGENTA, "putpixel_f survives a later GPU draw");

    ege::delimage(image);
}

void testLineAndFillStyles()
{
    ege::PIMAGE image = ege::newimage(28, 18);
    resetImage(image, ege::BLACK);

    ege::setfillstyle(ege::SOLID_FILL, ege::MAGENTA, image);
    ege::bar(2, 2, 10, 8, image);
    expectPixel(image, 5, 5, ege::MAGENTA, "setfillstyle updates native fill color");

    ege::setfillstyle(ege::EMPTY_FILL, ege::RED, image);
    ege::bar(12, 2, 20, 8, image);
    expectPixel(image, 15, 5, ege::BLACK, "EMPTY_FILL suppresses native fill primitives");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setlinestyle(ege::DASHED_LINE, 0, 1, image);
    int lineStyle = 0;
    int thickness = 0;
    unsigned short pattern = 0;
    ege::getlinestyle(&lineStyle, &pattern, &thickness, image);
    expect(lineStyle == ege::DASHED_LINE && thickness == 1,
           "getlinestyle returns the selected line style");
    ege::line(2, 4, 24, 4, image);
    bool sawStroke = false;
    bool sawGap = false;
    for (int x = 3; x < 24; ++x) {
        if (rgb(ege::getpixel(x, 4, image)) == rgb(ege::WHITE)) sawStroke = true;
        else sawGap = true;
    }
    expect(sawStroke && sawGap, "dashed line style contains both strokes and gaps");

    resetImage(image, ege::BLACK);
    ege::setlinestyle(ege::SOLID_LINE, 0, 3, image);
    ege::line(3, 10, 24, 10, image);
    expectPixel(image, 10, 9, ege::WHITE, "line-style thickness expands above the centerline");
    expectPixel(image, 10, 11, ege::WHITE, "line-style thickness expands below the centerline");

    ege::setlinecap(ege::LINECAP_SQUARE, ege::LINECAP_ROUND, image);
    ege::line_cap_type startCap = ege::LINECAP_FLAT;
    ege::line_cap_type endCap = ege::LINECAP_FLAT;
    ege::getlinecap(&startCap, &endCap, image);
    expect(startCap == ege::LINECAP_SQUARE && endCap == ege::LINECAP_ROUND &&
           ege::getlinecap(image) == ege::LINECAP_SQUARE,
           "getlinecap overloads return the selected caps");

    ege::setlinejoin(ege::LINEJOIN_BEVEL, 3.5f, image);
    ege::line_join_type lineJoin = ege::LINEJOIN_MITER;
    float miterLimit = 0.0f;
    ege::getlinejoin(&lineJoin, &miterLimit, image);
    expect(lineJoin == ege::LINEJOIN_BEVEL && miterLimit == 3.5f &&
           ege::getlinejoin(image) == ege::LINEJOIN_BEVEL,
           "getlinejoin overloads return the selected join state");

    ege::delimage(image);
}

void testImageLifecycleCropAndStretch()
{
    ege::PIMAGE source = ege::newimage(4, 4);
    resetImage(source, ege::BLACK);
    ege::putpixel(1, 1, ege::RED, source);
    ege::putpixel(2, 2, ege::CYAN, source);

    ege::PIMAGE cropped = ege::newimage();
    expect(ege::getimage(cropped, source, 1, 1, 2, 2) == ege::grOk,
           "getimage crops an image-to-image source rectangle");
    expect(ege::getwidth(cropped) == 2 && ege::getheight(cropped) == 2,
           "image crop updates destination dimensions");
    expectPixel(cropped, 0, 0, ege::RED, "image crop maps its top-left pixel");
    expectPixel(cropped, 1, 1, ege::CYAN, "image crop maps its bottom-right pixel");

    ege::PIMAGE stretched = ege::newimage(8, 8);
    resetImage(stretched, ege::WHITE);
    ege::putimage(stretched, 2, 2, 4, 4, source, 1, 1, 2, 2);
    expectPixel(stretched, 2, 2, ege::RED, "stretched putimage maps the source top-left");
    expectPixel(stretched, 5, 5, ege::CYAN, "stretched putimage maps the source bottom-right");
    expectPixel(stretched, 1, 1, ege::WHITE, "stretched putimage keeps pixels outside its destination");

    ege::delimage(stretched);
    ege::delimage(cropped);
    ege::delimage(source);
}

void testTransparencyAndAlphaBlend()
{
    ege::PIMAGE source = ege::newimage(2, 1);
    ege::PIMAGE destination = ege::newimage(4, 3);
    resetImage(source, ege::MAGENTA);
    resetImage(destination, ege::GREEN);
    ege::putpixel(1, 0, ege::RED, source);
    ege::color_t* destinationPixels = ege::getbuffer(destination);
    destinationPixels[1 * 4 + 2] = EGEARGB(37, 255, 0, 0);

    expect(ege::putimage_transparent(destination, source, 1, 1, ege::MAGENTA) == ege::grOk,
           "transparent putimage reports success");
    expectPixel(destination, 1, 1, ege::GREEN, "transparent color key leaves destination unchanged");
    expectPixel(destination, 2, 1, ege::RED, "transparent putimage copies non-key pixels");
    destinationPixels = ege::getbuffer(destination);
    expect(EGEGET_A(destinationPixels[1 * 4 + 2]) == 37,
           "transparent putimage preserves the destination alpha channel");

    ege::PIMAGE alphaSource = ege::newimage(1, 1);
    resetImage(alphaSource, ege::RED);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphablend(destination, alphaSource, 0, 0, 128) == ege::grOk,
           "alpha blend reports success");
    const ege::color_t blended = ege::getpixel(0, 0, destination);
    expect(EGEGET_R(blended) >= 120 && EGEGET_R(blended) <= 136 &&
           EGEGET_G(blended) <= 2 &&
           EGEGET_B(blended) >= 120 && EGEGET_B(blended) <= 136,
           "global alpha produces source-over RGB channels");

    ege::delimage(alphaSource);
    ege::delimage(destination);
    ege::delimage(source);
}

void testAlphaFormatsAndCombinedColorKey()
{
    ege::PIMAGE source = ege::newimage(2, 1);
    ege::PIMAGE destination = ege::newimage(4, 3);

    // putimage_withalpha follows the Win32 AlphaBlend contract: source pixels
    // are premultiplied. A half-alpha red pixel therefore stores R=128, not 255.
    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = EGEARGB(128, 128, 0, 0);
    sourcePixels[1] = EGEARGB(255, 0, 0, 0);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_withalpha(destination, source, 0, 0, 0, 0, 1, 1) == ege::grOk,
           "putimage_withalpha accepts a premultiplied source pixel");
    const ege::color_t premultiplied = ege::getpixel(0, 0, destination);
    expect(EGEGET_R(premultiplied) >= 126 && EGEGET_R(premultiplied) <= 130 &&
           EGEGET_G(premultiplied) <= 2 &&
           EGEGET_B(premultiplied) >= 126 && EGEGET_B(premultiplied) <= 130,
           "premultiplied alpha is not multiplied twice by the GPU blend stage");

    sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = EGEARGB(128, 255, 0, 0);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphablend(destination, source, 0, 0, 255,
                                    ege::COLORTYPE_ARGB32) == ege::grOk,
           "ARGB32 alpha blend reports success");
    const ege::color_t straight = ege::getpixel(0, 0, destination);
    expect(EGEGET_R(straight) >= 126 && EGEGET_R(straight) <= 130 &&
           EGEGET_B(straight) >= 126 && EGEGET_B(straight) <= 130,
           "ARGB32 uses straight source alpha exactly once");

    sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = EGEARGB(0, 255, 0, 0);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphablend(destination, source, 0, 0, 128,
                                    ege::COLORTYPE_RGB32) == ege::grOk,
           "RGB32 alpha blend reports success");
    const ege::color_t rgb32 = ege::getpixel(0, 0, destination);
    expect(EGEGET_R(rgb32) >= 126 && EGEGET_R(rgb32) <= 130 &&
           EGEGET_B(rgb32) >= 126 && EGEGET_B(rgb32) <= 130,
           "RGB32 ignores the stored source alpha and uses the global alpha");

    sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = ege::MAGENTA;
    sourcePixels[1] = ege::RED;
    resetImage(destination, ege::BLUE);
    ege::color_t* destinationPixels = ege::getbuffer(destination);
    destinationPixels[1 * 4 + 2] = EGEARGB(73, 0, 0, 255);
    expect(ege::putimage_alphatransparent(destination, source, 1, 1,
                                          ege::MAGENTA, 128, 0, 0, 2, 1) == ege::grOk,
           "combined color-key alpha blend reports success");
    expectPixel(destination, 1, 1, ege::BLUE,
                "combined color-key alpha keeps the key pixel untouched");
    const ege::color_t combined = ege::getpixel(2, 1, destination);
    expect(EGEGET_R(combined) >= 126 && EGEGET_R(combined) <= 130 &&
           EGEGET_B(combined) >= 126 && EGEGET_B(combined) <= 130,
           "combined color-key alpha blends each non-key pixel exactly once");
    destinationPixels = ege::getbuffer(destination);
    expect(EGEGET_A(destinationPixels[1 * 4 + 2]) == 73,
           "combined color-key alpha preserves the destination alpha channel");

    ege::delimage(destination);
    ege::delimage(source);
}

void testEnhancedAlphaSurfaceCompatibility()
{
    ege::PIMAGE source = ege::newimage(16, 16);
    ege::PIMAGE destination = ege::newimage(16, 16);
    ege::setbkcolor(EGERGBA(0, 0, 0, 0), source);
    ege::setlinecolor(EGEARGB(255, 255, 0, 0), source);
    ege::setlinewidth(3.0f, source);
    ege::ege_line(2.0f, 2.0f, 13.0f, 13.0f, source);

    resetImage(destination, EGEARGB(255, 0, 0, 255));
    expect(ege::putimage_withalpha(destination, source, 0, 0) == ege::grOk,
           "enhanced alpha surface compositing reports success");
    expectPixel(destination, 15, 0, ege::BLUE,
                "transparent untouched pixels preserve an offscreen destination");
    expectPixel(destination, 7, 7, ege::RED,
                "enhanced opaque strokes survive putimage_withalpha");

    ege::delimage(destination);
    ege::delimage(source);
}

void testEnhancedAlphaScreenCompatibility()
{
    ege::setrendermode(ege::RENDER_AUTO);
    ege::settarget(nullptr);
    ege::setviewport(0, 0, 64, 64, true);
    ege::setbkcolor(ege::WHITE);
    ege::cleardevice();
    ege::setfillcolor(ege::BLUE);
    ege::bar(10, 10, 60, 40);

    ege::PIMAGE source = ege::newimage(64, 48);
    ege::setbkcolor(EGERGBA(0, 0, 0, 0), source);
    ege::setfillcolor(EGEARGB(255, 255, 0, 0), source);
    ege::setlinecolor(EGEARGB(255, 255, 0, 0), source);
    ege::setlinestyle(ege::CENTER_LINE, 0, 1, source);
    ege::setlinewidth(5.0f, source);
    ege::ege_line(10.0f, 10.0f, 40.0f, 40.0f, source);
    ege::setlinecolor(EGEARGB(255, 0, 255, 0), source);
    ege::setlinestyle(ege::DOTTED_LINE, 0, 1, source);
    ege::setlinewidth(3.0f, source);
    ege::ege_ellipse(20.0f, 10.0f, 20.0f, 20.0f, source);
    ege::setfillcolor(EGEARGB(255, 255, 0, 255), source);
    ege::ege_fillellipse(1.0f, 1.0f, 5.0f, 5.0f, source);
    ege::setfillcolor(EGEARGB(255, 0, 255, 255), source);
    ege::ege_fillellipse(10.0f, 1.0f, 5.0f, 5.0f, source);
    ege::putimage_withalpha(nullptr, source, 0, 0);
    ege::delimage(source);
    ege::delay_ms(150);

    ege::PIMAGE capture = ege::newimage();
    expect(ege::getimage(capture, 0, 0, 64, 48) == ege::grOk,
           "enhanced alpha demo frame can be captured");
    expectPixel(capture, 55, 20, ege::BLUE,
                "transparent enhanced pixels preserve the screen destination");
    expectPixel(capture, 5, 45, ege::WHITE,
                "screen alpha composition preserves the white area outside the blue bar");

    ege::delimage(capture);
    ege::setrendermode(ege::RENDER_MANUAL);
}

void testAlphaMaskDefaultsAndScaledSampling()
{
    ege::PIMAGE source = ege::newimage(2, 2);
    ege::PIMAGE destination = ege::newimage(6, 3);

    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = EGEARGB(128, 128, 0, 0);
    sourcePixels[1] = ege::GREEN;
    sourcePixels[2] = EGEARGB(255, 255, 0, 0);
    sourcePixels[3] = EGEARGB(255, 255, 0, 0);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_withalpha(destination, source, 1, 0) == ege::grOk,
           "default putimage_withalpha reports success");
    const ege::color_t defaultAlpha = ege::getpixel(1, 0, destination);
    expect(EGEGET_R(defaultAlpha) >= 126 && EGEGET_R(defaultAlpha) <= 130 &&
           EGEGET_B(defaultAlpha) >= 126 && EGEGET_B(defaultAlpha) <= 130,
           "default putimage_withalpha uses the full source extent");
    expectPixel(destination, 2, 0, ege::GREEN,
                "default putimage_withalpha copies every source column");

    ege::PIMAGE scaleSource = ege::newimage(2, 1);
    ege::color_t* scalePixels = ege::getbuffer(scaleSource);
    scalePixels[0] = ege::BLACK;
    scalePixels[1] = ege::WHITE;
    resetImage(destination, ege::BLACK);
    expect(ege::putimage_alphablend(destination, scaleSource,
                                    0, 0, 6, 1, 255,
                                    0, 0, 2, 1, true,
                                    ege::COLORTYPE_ARGB32) == ege::grOk,
           "smooth scaled alpha blend reports success");
    bool foundInterpolatedPixel = false;
    for (int x = 0; x < 6; ++x) {
        const unsigned char red = EGEGET_R(ege::getpixel(x, 0, destination));
        foundInterpolatedPixel = foundInterpolatedPixel || (red > 8 && red < 247);
    }
    expect(foundInterpolatedPixel,
           "smooth scaled alpha blend uses filtered texture sampling");

    ege::PIMAGE alphaMask = ege::newimage(3, 2);
    ege::color_t* maskPixels = ege::getbuffer(alphaMask);
    for (int i = 0; i < 6; ++i) maskPixels[i] = 0;
    maskPixels[1] = EGEARGB(255, 0, 0, 255);
    maskPixels[3] = EGEARGB(255, 0, 0, 255);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphafilter(destination, source, 0, 0,
                                     alphaMask, 0, 0, 2, 2) == ege::grOk,
           "alpha-mask image transfer reports success");
    expectPixel(destination, 0, 0, ege::BLUE,
                "zero alpha-mask byte preserves GPU-rendered destination pixels");
    expectPixel(destination, 1, 0, ege::GREEN,
                "alpha-mask byte selects the corresponding source pixel");
    expectPixel(destination, 0, 1, ege::RED,
                "alpha-mask rows use the mask image stride");
    expectPixel(destination, 1, 1, ege::BLUE,
                "zero alpha-mask byte is honored on later rows");

    ege::delimage(alphaMask);
    ege::delimage(scaleSource);
    ege::delimage(destination);
    ege::delimage(source);
}

void testAlphaTransferOverloadsAndClipping()
{
    ege::PIMAGE source = ege::newimage(3, 2);
    ege::PIMAGE destination = ege::newimage(6, 4);
    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = ege::RED;
    sourcePixels[1] = ege::GREEN;
    sourcePixels[2] = ege::BLUE;
    sourcePixels[3] = ege::CYAN;
    sourcePixels[4] = ege::MAGENTA;
    sourcePixels[5] = ege::YELLOW;

    resetImage(destination, ege::BLACK);
    expect(ege::putimage_alphablend(destination, source, 0, 0, 255,
                                    1, 0, ege::COLORTYPE_RGB32) == ege::grOk,
           "source-origin alpha overload reports success");
    expectPixel(destination, 0, 0, ege::GREEN,
                "source-origin alpha overload starts at the requested column");
    expectPixel(destination, 1, 1, ege::YELLOW,
                "source-origin alpha overload uses the remaining source extent");

    resetImage(destination, ege::BLACK);
    expect(ege::putimage_alphablend(destination, source, 2, 1, 255,
                                    2, 0, 1, 1,
                                    ege::COLORTYPE_RGB32) == ege::grOk,
           "source-rectangle alpha overload reports success");
    expectPixel(destination, 2, 1, ege::BLUE,
                "source-rectangle alpha overload restricts the copied extent");
    expectPixel(destination, 3, 1, ege::BLACK,
                "source-rectangle alpha overload leaves adjacent pixels untouched");

    resetImage(destination, ege::BLUE);
    expect(ege::putimage_alphablend(destination, source, 0, 0, 0,
                                    ege::COLORTYPE_RGB32) == ege::grOk,
           "zero-alpha transfer reports success");
    expectPixel(destination, 0, 0, ege::BLUE,
                "zero global alpha leaves the destination unchanged");

    resetImage(destination, ege::BLACK);
    expect(ege::putimage_transparent(destination, source, -1, 0,
                                     ege::MAGENTA, 0, 0, 3, 1) == ege::grOk,
           "negative-destination transparent transfer reports success");
    expectPixel(destination, 0, 0, ege::GREEN,
                "transparent transfer clips the destination and advances the source");
    expectPixel(destination, 1, 0, ege::BLUE,
                "transparent transfer preserves source sampling after clipping");

    resetImage(destination, ege::BLACK);
    expect(ege::putimage_alphablend(destination, source, -1, 0, 255,
                                    0, 0, 3, 1,
                                    ege::COLORTYPE_RGB32) == ege::grOk,
           "negative-destination alpha transfer reports success");
    expectPixel(destination, 0, 0, ege::GREEN,
                "alpha transfer clips the destination and advances the source");

    ege::PIMAGE premultiplied = ege::newimage(3, 3);
    ege::color_t* premultipliedPixels = ege::getbuffer(premultiplied);
    std::fill(premultipliedPixels, premultipliedPixels + 9, EGEARGB(128, 128, 0, 0));
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_withalpha(destination, premultiplied,
                                   0, 0, 6, 4,
                                   0, 0, 3, 3, true) == ege::grOk,
           "smooth stretched with-alpha overload reports success");
    const ege::color_t stretchedAlpha = ege::getpixel(3, 2, destination);
#ifdef _WIN32
    const bool stretchedAlphaMatches =
        EGEGET_R(stretchedAlpha) >= 144 && EGEGET_R(stretchedAlpha) <= 148 &&
        EGEGET_B(stretchedAlpha) >= 107 && EGEGET_B(stretchedAlpha) <= 111;
#else
    const bool stretchedAlphaMatches =
        EGEGET_R(stretchedAlpha) >= 126 && EGEGET_R(stretchedAlpha) <= 130 &&
        EGEGET_B(stretchedAlpha) >= 126 && EGEGET_B(stretchedAlpha) <= 130;
#endif
    expect(stretchedAlphaMatches,
           "smooth stretched with-alpha overload composites premultiplied pixels (R=" +
               std::to_string(EGEGET_R(stretchedAlpha)) + ", B=" +
               std::to_string(EGEGET_B(stretchedAlpha)) + ")");

    expect(ege::putimage_alphafilter(destination, source, 0, 0,
                                     nullptr, 0, 0, 1, 1) == ege::grNullPointer,
           "alpha-filter rejects a null mask image");

    ege::delimage(premultiplied);
    ege::delimage(destination);
    ege::delimage(source);
}

void testImageRotationCoordinatesAndAspectRatio()
{
    ege::PIMAGE source = ege::newimage(4, 4);
    ege::PIMAGE destination = ege::newimage(32, 24);
    resetImage(source, ege::RED);
    resetImage(destination, ege::BLACK);

    expect(ege::putimage_rotate(destination, source, 12, 10, 0.5f, 0.5f, 0.0f) == ege::grOk,
           "zero-angle putimage_rotate reports success");
    expectPixel(destination, 10, 8, ege::RED,
                "putimage_rotate places the normalized source center at xDest/yDest");
    expectPixel(destination, 15, 11, ege::BLACK,
                "putimage_rotate does not treat xDest/yDest as the destination top-left");

    ege::resize(source, 6, 2);
    resetImage(source, ege::CYAN);
    resetImage(destination, ege::BLACK);
    const float halfPi = 1.57079632679489661923f;
    expect(ege::putimage_rotate(destination, source, 16, 12, 0.5f, 0.5f, halfPi) == ege::grOk,
           "quarter-turn putimage_rotate reports success");
    expectPixel(destination, 16, 9, ege::CYAN,
                "rotation is computed in pixel space on a non-square destination");
    expectPixel(destination, 14, 12, ege::BLACK,
                "quarter-turn rotation keeps the expected two-pixel width");

    ege::resize(source, 5, 5);
    resetImage(source, ege::WHITE);
    ege::putpixel(2, 2, ege::RED, source);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_rotatetransparent(destination, source,
                                           16, 12, 2, 2, ege::WHITE,
                                           0.0f, 2.0f) == ege::grOk,
           "putimage_rotatetransparent reports success");
    expectPixel(destination, 16, 12, ege::RED,
                "putimage_rotatetransparent keeps the scaled source-center pixel");
    expectPixel(destination, 12, 8, ege::BLUE,
                "putimage_rotatetransparent skips the transparent source background");

    ege::resize(source, 3, 3);
    resetImage(source, ege::RED);
    resetImage(destination, ege::BLACK);
    expect(ege::putimage_rotatezoom(destination, source,
                                    16, 12, 0.5f, 0.5f,
                                    0.0f, 2.0f) == ege::grOk,
           "putimage_rotatezoom reports success");
    expectPixel(destination, 16, 12, ege::RED,
                "putimage_rotatezoom keeps the selected pivot at its destination");
    expectPixel(destination, 12, 8, ege::BLACK,
                "putimage_rotatezoom keeps pixels outside the scaled image untouched");

    resetImage(destination, ege::BLUE);
    expect(ege::putimage_rotate(destination, source,
                                16, 12, 0.5f, 0.5f, 0.0f,
                                false, 128, false) == ege::grOk,
           "global-alpha putimage_rotate reports success");
    const ege::color_t rotatedAlpha = ege::getpixel(16, 12, destination);
    expect(EGEGET_R(rotatedAlpha) >= 126 && EGEGET_R(rotatedAlpha) <= 130 &&
           EGEGET_B(rotatedAlpha) >= 126 && EGEGET_B(rotatedAlpha) <= 130,
           "global-alpha putimage_rotate blends source and destination once");

    ege::resize(source, 4, 4);
    ege::color_t* transparentRotationPixels = ege::getbuffer(source);
    std::fill(transparentRotationPixels, transparentRotationPixels + 16, 0U);
    transparentRotationPixels[2 * 4 + 2] = ege::GREEN;
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_rotatezoom(destination, source,
                                    16, 12, 0.5f, 0.5f,
                                    0.0f, 1.0f,
                                    true, -1, true) == ege::grOk,
           "smooth zero-key putimage_rotatezoom reports success");
    expect(countPixelsDifferentFrom(destination, ege::BLUE) > 0,
           "smooth zero-key putimage_rotatezoom draws nonzero source pixels");
    expectPixel(destination, 14, 10, ege::BLUE,
                "smooth zero-key putimage_rotatezoom skips zero-valued source pixels");

    ege::resize(source, 5, 5);
    resetImage(source, ege::WHITE);
    ege::putpixel(2, 2, ege::GREEN, source);
    resetImage(destination, ege::BLUE);
    expect(ege::putimage_rotatetransparent(destination, source,
                                           16, 12,
                                           1, 1, 3, 3,
                                           2, 2, ege::WHITE,
                                           0.0f, 1.0f) == ege::grOk,
           "source-rectangle putimage_rotatetransparent reports success");
    expectPixel(destination, 16, 12, ege::GREEN,
                "source-rectangle putimage_rotatetransparent maps its selected center");
    expectPixel(destination, 14, 10, ege::BLUE,
                "source-rectangle putimage_rotatetransparent skips key-colored pixels");

    ege::delimage(destination);
    ege::delimage(source);
}

void testEnhancedImageTransform()
{
    ege::PIMAGE source = ege::newimage(4, 3);
    ege::PIMAGE destination = ege::newimage(32, 20);
    resetImage(source, ege::WHITE);
    resetImage(destination, ege::BLACK);

    ege::ege_transform_reset(destination);
    ege::ege_transform_translate(10.0f, 6.0f, destination);
    ege::ege_drawimage(source, 2, 3, destination);
    ege::ege_transform_reset(destination);
    expectPixel(destination, 12, 9, ege::WHITE,
                "ege_drawimage applies the active enhanced transform");
    expectPixel(destination, 2, 3, ege::BLACK,
                "transformed ege_drawimage does not draw at its untransformed position");

    ege::delimage(destination);
    ege::delimage(source);
}

void testTextureAndEnhancedImageTransferOverloads()
{
    ege::PIMAGE source = ege::newimage(3, 2);
    ege::PIMAGE destination = ege::newimage(12, 8);
    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = ege::RED;
    sourcePixels[1] = ege::GREEN;
    sourcePixels[2] = ege::BLUE;
    sourcePixels[3] = ege::CYAN;
    sourcePixels[4] = ege::MAGENTA;
    sourcePixels[5] = ege::YELLOW;

    ege::ege_gentexture(true, source);

    resetImage(destination, ege::BLACK);
    ege::ege_puttexture(source, 1.0f, 1.0f, 3.0f, 2.0f, destination);
    expectPixel(destination, 1, 1, ege::RED,
                "float ege_puttexture overload maps the source top-left pixel");
    expectPixel(destination, 3, 2, ege::YELLOW,
                "float ege_puttexture overload maps the source bottom-right pixel");

    resetImage(destination, ege::BLACK);
    const ege::ege_rect wholeDestination = {2.0f, 2.0f, 3.0f, 2.0f};
    ege::ege_puttexture(source, wholeDestination, destination);
    expectPixel(destination, 3, 2, ege::GREEN,
                "rectangle ege_puttexture overload draws the generated texture");

    resetImage(destination, ege::BLACK);
    const ege::ege_rect croppedDestination = {5.0f, 1.0f, 1.0f, 2.0f};
    const ege::ege_rect croppedSource = {1.0f, 0.0f, 1.0f, 2.0f};
    ege::ege_puttexture(source, croppedDestination, croppedSource, destination);
    expectPixel(destination, 5, 1, ege::GREEN,
                "source-rectangle ege_puttexture overload crops its top row");
    expectPixel(destination, 5, 2, ege::MAGENTA,
                "source-rectangle ege_puttexture overload crops its bottom row");

    // A generated GDI+ texture wraps the IMAGE buffer. Mutations after
    // generation are therefore observable on the legacy backend and must be
    // synchronized from the OpenGL render target before enhanced drawing.
    ege::putpixel(0, 0, ege::WHITE, source);
    resetImage(destination, ege::BLACK);
    const ege::ege_rect firstPixelDestination = {0.0f, 0.0f, 1.0f, 1.0f};
    const ege::ege_rect firstPixelSource = {0.0f, 0.0f, 1.0f, 1.0f};
    ege::ege_puttexture(source, firstPixelDestination, firstPixelSource, destination);
    expectPixel(destination, 0, 0, ege::WHITE,
                "ege_puttexture observes source draws made after texture generation");

    ege::putpixel(0, 0, ege::CYAN, source);
    resetImage(destination, ege::BLACK);
    ege::ege_setpattern_texture(source, 0.0f, 0.0f, 3.0f, 2.0f, destination);
    ege::ege_fillrect(0.0f, 0.0f, 3.0f, 2.0f, destination);
    expectPixel(destination, 0, 0, ege::CYAN,
                "texture patterns observe source draws made after texture generation");
    ege::ege_setpattern_none(destination);

    resetImage(destination, ege::BLACK);
    ege::ege_drawimage(source, 2, 3, destination);
    expectPixel(destination, 2, 3, ege::CYAN,
                "ege_drawimage whole-image overload observes the current source buffer");
    expectPixel(destination, 4, 4, ege::YELLOW,
                "ege_drawimage whole-image overload draws the full extent");

    resetImage(destination, ege::BLACK);
    ege::ege_drawimage(source, 7, 2, 1, 2, 1, 0, 1, 2, destination);
    expectPixel(destination, 7, 2, ege::GREEN,
                "ege_drawimage source-rectangle overload crops its top row");
    expectPixel(destination, 7, 3, ege::MAGENTA,
                "ege_drawimage source-rectangle overload crops its bottom row");

    ege::ege_gentexture(false, source);
    resetImage(destination, ege::BLACK);
    ege::ege_puttexture(source, firstPixelDestination, firstPixelSource, destination);
    expectPixel(destination, 0, 0, ege::BLACK,
                "ege_gentexture(false) disables subsequent texture draws");

    // Resizing replaces the DIB/GPU storage that a generated texture wraps.
    // Keep texture generation enabled while rebuilding that wrapper so the
    // public object cannot retain a dangling GDI+ bitmap.
    ege::ege_gentexture(true, source);
    expect(ege::resize(source, 2, 1) == ege::grOk,
           "resizing an image with a generated texture reports success");
    ege::putpixel(0, 0, ege::YELLOW, source);
    ege::putpixel(1, 0, ege::BLUE, source);
    resetImage(destination, ege::BLACK);
    const ege::ege_rect resizedDestination = {1.0f, 1.0f, 2.0f, 1.0f};
    ege::ege_puttexture(source, resizedDestination, destination);
    expectPixel(destination, 1, 1, ege::YELLOW,
                "generated texture remains valid after IMAGE storage is resized");
    expectPixel(destination, 2, 1, ege::BLUE,
                "resized generated texture uses the new dimensions and buffer");

    ege::delimage(destination);
    ege::delimage(source);
}

void testEnhancedPathApi()
{
    ege::PIMAGE image = ege::newimage(96, 96);
    resetImage(image, ege::BLACK);

    ege::ege_path* rectanglePath = ege::ege_path_create();
    expect(rectanglePath != NULL, "ege_path_create returns a path");
    ege::ege_path_addrect(rectanglePath, 10.0f, 10.0f, 30.0f, 20.0f);
    ege::ege_path_setfillmode(rectanglePath, ege::FILLMODE_WINDING);
    expect(ege::ege_path_pointcount(rectanglePath) == 4,
           "a rectangle path contains four points");

    const ege::ege_point lastPoint = ege::ege_path_lastpoint(rectanglePath);
    expect(lastPoint.x >= 10.0f && lastPoint.x <= 40.0f &&
           lastPoint.y >= 10.0f && lastPoint.y <= 30.0f,
           "ege_path_lastpoint returns a point from the rectangle");
    const ege::ege_rect bounds = ege::ege_path_getbounds(rectanglePath, NULL);
    expect(bounds.x == 10.0f && bounds.y == 10.0f &&
           bounds.w == 30.0f && bounds.h == 20.0f,
           "ege_path_getbounds reports rectangle geometry");
    expect(ege::ege_path_inpath(rectanglePath, 20.0f, 20.0f),
           "ege_path_inpath recognizes an interior point");
    expect(ege::ege_path_instroke(rectanglePath, 10.0f, 20.0f),
           "ege_path_instroke recognizes an outline point");

    ege::ege_point* copiedPoints = ege::ege_path_getpathpoints(rectanglePath);
    unsigned char* copiedTypes = ege::ege_path_getpathtypes(rectanglePath);
    expect(copiedPoints != NULL && copiedTypes != NULL,
           "path point and type queries allocate result arrays");
    delete[] copiedPoints;
    delete[] copiedTypes;

    ege::setfillcolor(ege::RED, image);
    ege::setlinecolor(ege::GREEN, image);
    ege::setlinewidth(4.0f, image);
    expect(ege::ege_path_inpath(rectanglePath, 20.0f, 20.0f, image),
           "image-aware path hit testing works for an interior point");
    expect(ege::ege_path_instroke(rectanglePath, 10.0f, 20.0f, image),
           "image-aware stroke hit testing uses the image pen");
    ege::ege_fillpath(rectanglePath, image);
    expectPixel(image, 20, 20, ege::RED, "ege_fillpath fills the path interior");
    ege::ege_drawpath(rectanglePath, image);
    expectPixel(image, 10, 20, ege::GREEN,
                "plain ege_drawpath draws the path outline");
    ege::ege_fillpath(rectanglePath, 45.0f, 0.0f, image);
    expectPixel(image, 65, 20, ege::RED,
                "translated ege_fillpath fills at the requested offset");
    ege::ege_drawpath(rectanglePath, 35.0f, 35.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 500,
           "translated ege_drawpath adds visible outline pixels");

    const ege::ege_transform_matrix translation = {1.0f, 0.0f, 0.0f, 1.0f, 5.0f, 7.0f};
    ege::ege_path* transformedPath = ege::ege_path_clone(rectanglePath);
    ege::ege_path_transform(transformedPath, &translation);
    const ege::ege_rect transformedBounds =
        ege::ege_path_getbounds(transformedPath, NULL, image);
    expect(transformedBounds.x <= 15.0f && transformedBounds.y <= 17.0f &&
           transformedBounds.x + transformedBounds.w >= 45.0f &&
           transformedBounds.y + transformedBounds.h >= 37.0f,
           "path transforms and image-aware bounds preserve translated geometry");
    ege::ege_path_reverse(transformedPath);

    const ege::ege_point sourcePoints[] = {{0.0f, 0.0f}, {8.0f, 0.0f}, {8.0f, 8.0f}};
    const unsigned char sourceTypes[] = {0, 1, 1};
    ege::ege_path* sourcePath = ege::ege_path_createfrom(sourcePoints, sourceTypes, 3);
    expect(sourcePath != NULL && ege::ege_path_pointcount(sourcePath) == 3,
           "ege_path_createfrom preserves supplied points");

    ege::ege_path* aggregatePath = ege::ege_path_create();
    ege::ege_path_start(aggregatePath);
    ege::ege_path_addline(aggregatePath, 2.0f, 2.0f, 12.0f, 2.0f);
    ege::ege_path_addarc(aggregatePath, 2.0f, 2.0f, 12.0f, 10.0f, 0.0f, 90.0f);
    ege::ege_path_addpolyline(aggregatePath, 3, sourcePoints);
    const ege::ege_point bezierPoints[] = {
        {2.0f, 20.0f}, {8.0f, 12.0f}, {16.0f, 28.0f}, {22.0f, 20.0f}
    };
    ege::ege_path_addbezier(aggregatePath, 4, bezierPoints);
    ege::ege_path_addbezier(aggregatePath, 24.0f, 20.0f, 28.0f, 12.0f,
                            34.0f, 28.0f, 38.0f, 20.0f);
    ege::ege_path_addcurve(aggregatePath, 3, sourcePoints);
    ege::ege_path_addcurve(aggregatePath, 3, sourcePoints, 0.5f);
    ege::ege_path_addcircle(aggregatePath, 48.0f, 16.0f, 6.0f);
    ege::ege_path_addellipse(aggregatePath, 58.0f, 8.0f, 14.0f, 10.0f);
    ege::ege_path_addpie(aggregatePath, 74.0f, 8.0f, 14.0f, 12.0f, 20.0f, 120.0f);
    ege::ege_path_addtext(aggregatePath, 2.0f, 40.0f, L"A", 12.0f, -1,
                          L"Arial", 0);
    ege::ege_path_addpolygon(aggregatePath, 3, sourcePoints);
    ege::ege_path_addclosedcurve(aggregatePath, 3, sourcePoints);
    ege::ege_path_addclosedcurve(aggregatePath, 3, sourcePoints, 0.5f);
    ege::ege_path_close(aggregatePath);
    ege::ege_path_addpath(aggregatePath, sourcePath, false);
    ege::ege_path_closeall(aggregatePath);
    expect(ege::ege_path_pointcount(aggregatePath) > 20,
           "path segment builders append geometry");

    ege::ege_path* flattenedPath = ege::ege_path_clone(aggregatePath);
    ege::ege_path_flatten(flattenedPath, NULL, 0.25f);
    expect(ege::ege_path_pointcount(flattenedPath) > 0,
           "ege_path_flatten retains usable geometry");
    ege::ege_path_widen(flattenedPath, 2.0f, NULL, 0.25f);
    expect(ege::ege_path_pointcount(flattenedPath) > 0,
           "ege_path_widen produces outline geometry");

    ege::ege_path* outlinePath = ege::ege_path_clone(rectanglePath);
    ege::ege_path_outline(outlinePath, NULL, 0.25f);
    expect(ege::ege_path_pointcount(outlinePath) > 0,
           "ege_path_outline retains the rectangle outline");

    ege::ege_path* warpedPath = ege::ege_path_clone(rectanglePath);
    const ege::ege_point warpPoints[] = {
        {10.0f, 10.0f}, {42.0f, 12.0f}, {12.0f, 34.0f}, {44.0f, 36.0f}
    };
    const ege::ege_rect warpSource = {10.0f, 10.0f, 30.0f, 20.0f};
    ege::ege_path_warp(warpedPath, warpPoints, 4, &warpSource, NULL, 0.25f);
    expect(ege::ege_path_pointcount(warpedPath) > 0,
           "ege_path_warp retains mapped geometry");

    ege::ege_path* defaultFlattenedPath = ege::ege_path_clone(aggregatePath);
    ege::ege_path_flatten(defaultFlattenedPath);
    expect(ege::ege_path_pointcount(defaultFlattenedPath) > 0,
           "default ege_path_flatten overload retains usable geometry");
    ege::ege_path_widen(defaultFlattenedPath, 2.0f);
    expect(ege::ege_path_pointcount(defaultFlattenedPath) > 0,
           "default ege_path_widen overload produces outline geometry");

    ege::ege_path* defaultOutlinePath = ege::ege_path_clone(rectanglePath);
    ege::ege_path_outline(defaultOutlinePath);
    expect(ege::ege_path_pointcount(defaultOutlinePath) > 0,
           "default ege_path_outline overload retains the rectangle outline");

    ege::ege_path* defaultWarpedPath = ege::ege_path_clone(rectanglePath);
    ege::ege_path_warp(defaultWarpedPath, warpPoints, 4, &warpSource);
    expect(ege::ege_path_pointcount(defaultWarpedPath) > 0,
           "default ege_path_warp overload retains mapped geometry");

    ege::ege_path_reset(sourcePath);
    expect(ege::ege_path_pointcount(sourcePath) == 0,
           "ege_path_reset removes all points");

    ege::ege_path_destroy(defaultWarpedPath);
    ege::ege_path_destroy(defaultOutlinePath);
    ege::ege_path_destroy(defaultFlattenedPath);
    ege::ege_path_destroy(warpedPath);
    ege::ege_path_destroy(outlinePath);
    ege::ege_path_destroy(flattenedPath);
    ege::ege_path_destroy(aggregatePath);
    ege::ege_path_destroy(sourcePath);
    ege::ege_path_destroy(transformedPath);
    ege::ege_path_destroy(rectanglePath);
    ege::delimage(image);
}

void testRasterOperations()
{
    ege::PIMAGE source = ege::newimage(4, 1);
    ege::PIMAGE destination = ege::newimage(4, 1);
    const ege::color_t sourceColor = EGEARGB(0xFF, 0x33, 0x66, 0xCC);
    const ege::color_t destinationColor = EGEARGB(0xFF, 0x55, 0xAA, 0x0F);
    const ege::color_t patternColor = EGEARGB(0xFF, 0xC3, 0x3C, 0x5A);
    resetImage(source, sourceColor);

    struct RasterOperationCase {
        DWORD operation;
        ege::color_t expected;
        const char* name;
    };
    const RasterOperationCase operations[] = {
        {SRCCOPY, sourceColor, "SRCCOPY"},
        {SRCPAINT, sourceColor | destinationColor, "SRCPAINT"},
        {SRCAND, sourceColor & destinationColor, "SRCAND"},
        {SRCINVERT, sourceColor ^ destinationColor, "SRCINVERT"},
        {SRCERASE, sourceColor & ~destinationColor, "SRCERASE"},
        {NOTSRCCOPY, ~sourceColor, "NOTSRCCOPY"},
        {NOTSRCERASE, ~(sourceColor | destinationColor), "NOTSRCERASE"},
        {MERGECOPY, sourceColor & patternColor, "MERGECOPY"},
        {MERGEPAINT, ~sourceColor | destinationColor, "MERGEPAINT"},
        {PATCOPY, patternColor, "PATCOPY"},
        {PATPAINT, destinationColor | patternColor | ~sourceColor, "PATPAINT"},
        {PATINVERT, patternColor ^ destinationColor, "PATINVERT"},
        {DSTINVERT, ~destinationColor, "DSTINVERT"},
        {BLACKNESS, 0x00000000U, "BLACKNESS"},
        {WHITENESS, 0xFFFFFFFFU, "WHITENESS"},
    };

    ege::setfillcolor(patternColor, destination);
    for (const RasterOperationCase& operation : operations) {
        resetImage(destination, destinationColor);
        ege::setfillcolor(patternColor, destination);
        ege::putimage(destination, 0, 0, source, operation.operation);
        expectPixel(destination, 0, 0, operation.expected,
                    std::string(operation.name) + " matches the Win32 ternary raster operation");
    }

    ege::PIMAGE patternedReference = ege::newimage(8, 8);
    ege::PIMAGE patternedDestination = ege::newimage(8, 8);
    ege::PIMAGE patternedSource = ege::newimage(8, 8);
    resetImage(patternedReference, ege::GREEN);
    resetImage(patternedDestination, ege::GREEN);
    resetImage(patternedSource, ege::WHITE);
    ege::setbkcolor_f(ege::BLUE, patternedReference);
    ege::setbkmode(OPAQUE, patternedReference);
    ege::setfillstyle(ege::HATCH_FILL, ege::RED, patternedReference);
    ege::bar(0, 0, 8, 8, patternedReference);
    ege::setbkcolor_f(ege::BLUE, patternedDestination);
    ege::setbkmode(OPAQUE, patternedDestination);
    ege::setfillstyle(ege::HATCH_FILL, ege::RED, patternedDestination);
    ege::putimage(patternedDestination, 0, 0, patternedSource, PATCOPY);
    const ege::color_t* expectedPattern = ege::getbuffer(patternedReference);
    const ege::color_t* actualPattern = ege::getbuffer(patternedDestination);
    bool patternedPatCopyMatches = true;
    int firstPatternMismatch = -1;
    for (int index = 0; index < 64; ++index) {
        if (rgb(expectedPattern[index]) != rgb(actualPattern[index])) {
            patternedPatCopyMatches = false;
            if (firstPatternMismatch < 0) firstPatternMismatch = index;
        }
    }
    expect(patternedPatCopyMatches,
           "PATCOPY uses the selected hatch brush rather than a flat fill color (first mismatch=" +
               std::to_string(firstPatternMismatch) +
               (firstPatternMismatch >= 0
                    ? ", expected=" + std::to_string(rgb(expectedPattern[firstPatternMismatch])) +
                      ", actual=" + std::to_string(rgb(actualPattern[firstPatternMismatch]))
                    : std::string()) + ")");
    ege::delimage(patternedSource);
    ege::delimage(patternedDestination);
    ege::delimage(patternedReference);

    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[0] = ege::RED;
    sourcePixels[1] = ege::GREEN;
    sourcePixels[2] = ege::BLUE;
    sourcePixels[3] = ege::WHITE;
    ege::putimage(source, 1, 0, 3, 1, source, 0, 0, SRCCOPY);
    expectPixel(source, 1, 0, ege::RED,
                "overlapping self-putimage snapshots the first source pixel");
    expectPixel(source, 2, 0, ege::GREEN,
                "overlapping self-putimage does not cascade writes through its source");
    expectPixel(source, 3, 0, ege::BLUE,
                "overlapping self-putimage preserves the last copied source pixel");

    ege::delimage(destination);
    ege::delimage(source);
}

void testCurrentPositionAndAdditionalPrimitiveRoutes()
{
    ege::PIMAGE image = ege::newimage(32, 24);
    resetImage(image, ege::BLACK);

    ege::moveto(3, 4, image);
    expect(ege::getx(image) == 3 && ege::gety(image) == 4,
           "getx/gety report the current native drawing position");
    ege::moverel(5, 2, image);
    expect(ege::getx(image) == 8 && ege::gety(image) == 6,
           "moverel updates the current native drawing position");

    ege::setfillcolor(ege::GREEN, image);
    ege::solidcircle(8, 8, 4, image);
    expectPixel(image, 8, 8, ege::GREEN, "solidcircle fills its center on the native backend");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    const int independentLines[] = {2, 3, 12, 3, 2, 7, 12, 7};
    ege::drawlines(2, independentLines, image);
    expectPixel(image, 7, 3, ege::WHITE, "drawlines draws its first independent segment");
    expectPixel(image, 7, 7, ege::WHITE, "drawlines draws its second independent segment");

    resetImage(image, ege::BLACK);
    const int bezier[] = {2, 18, 8, 2, 22, 2, 28, 18};
    ege::drawbezier(4, bezier, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 4,
           "drawbezier routes cubic curves to the native backend");

    ege::delimage(image);
}

void testSurfaceFloodFillAndColorConversion()
{
    ege::PIMAGE image = ege::newimage(16, 12);
    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::CYAN, image);
    ege::bar(2, 2, 13, 9, image);
    ege::setfillcolor(ege::MAGENTA, image);
    ege::floodfillsurface(5, 5, ege::CYAN, image);
    expectPixel(image, 5, 5, ege::MAGENTA, "floodfillsurface replaces the matching seed surface");
    expectPixel(image, 1, 5, ege::BLACK, "floodfillsurface does not cross into a different surface color");

    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::CYAN, image);
    ege::bar(2, 2, 13, 9, image);
    ege::setbkcolor_f(ege::BLUE, image);
    ege::setfillstyle(ege::LINE_FILL, ege::RED, image);
    ege::floodfillsurface(5, 5, ege::CYAN, image);
    expect(countPixelsEqualToInRect(image, ege::RED, 2, 2, 13, 9) > 0,
           "floodfillsurface applies the selected hatch foreground");
    expectPixel(image, 5, 5, ege::BLUE,
                "floodfillsurface applies the selected hatch background");
    expectPixel(image, 1, 5, ege::BLACK,
                "patterned floodfillsurface stays inside the matching surface");

    ege::color_t* pixels = ege::getbuffer(image);
    pixels[0] = EGEARGB(128, 200, 100, 50);
    ege::image_convertcolor(image, ege::COLORTYPE_ARGB32, ege::COLORTYPE_PRGB32);
    expect(EGEGET_A(pixels[0]) == 128 &&
           EGEGET_R(pixels[0]) >= 99 && EGEGET_R(pixels[0]) <= 101 &&
           EGEGET_G(pixels[0]) >= 49 && EGEGET_G(pixels[0]) <= 51 &&
           EGEGET_B(pixels[0]) >= 24 && EGEGET_B(pixels[0]) <= 26,
           "image_convertcolor premultiplies native image pixels");
    ege::image_convertcolor(image, ege::COLORTYPE_PRGB32, ege::COLORTYPE_ARGB32);
    expect(EGEGET_A(pixels[0]) == 128 &&
           EGEGET_R(pixels[0]) >= 198 && EGEGET_R(pixels[0]) <= 202 &&
           EGEGET_G(pixels[0]) >= 98 && EGEGET_G(pixels[0]) <= 102 &&
           EGEGET_B(pixels[0]) >= 48 && EGEGET_B(pixels[0]) <= 52,
           "image_convertcolor unpremultiplies native image pixels");

    ege::delimage(image);
}

void testTextRectangleAndBlur()
{
    ege::PIMAGE image = ege::newimage(80, 36);
    resetImage(image, ege::BLACK);
    ege::setfont(16, 0, "Arial", image);
    ege::settextcolor(ege::WHITE, image);
    ege::outtextrect(4, 3, 50, 26, "wrapped text", image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 0,
           "outtextrect renders text on the native backend");

    ege::PIMAGE blur = ege::newimage(7, 7);
    resetImage(blur, ege::BLACK);
    ege::putpixel(3, 3, ege::WHITE, blur);
    expect(ege::imagefilter_blurring(blur, 64, 256, 0, 0, 7, 7) == ege::grOk,
           "imagefilter_blurring reports success for a valid native image region");
    const ege::color_t center = ege::getpixel(3, 3, blur);
    expect(rgb(center) != rgb(ege::WHITE), "blur reduces an isolated center pixel");
    expect(rgb(ege::getpixel(3, 2, blur)) != rgb(ege::BLACK),
           "blur distributes color to a neighboring pixel");

    ege::PIMAGE blurGolden = ege::newimage(5, 5);
    ege::color_t* goldenPixels = ege::getbuffer(blurGolden);
    for (int i = 0; i < 25; ++i) {
        goldenPixels[i] = EGEARGB(255, 0, 0, 0);
    }
    goldenPixels[12] = EGEARGB(255, 255, 255, 255);
    expect(ege::imagefilter_blurring(blurGolden, 64, 0, 0, 0, 5, 5) == ege::grOk,
           "zero-alpha blur is a safe no-op");
    expect(goldenPixels[12] == EGEARGB(255, 255, 255, 255),
           "zero-alpha blur preserves the source pixels");

    expect(ege::imagefilter_blurring(blurGolden, 64, 256, 0, 0, 5, 5) == ege::grOk,
           "legacy-compatible blur accepts the reference input");
    const ege::color_t expectedBlur[25] = {
        0, 0, 0, 0, 0,
        0, 0, 0x001F1F1FU, 0, 0,
        0, 0x001F1F1FU, 0x007E7E7EU, 0x001F1F1FU, 0,
        0, 0, 0x001F1F1FU, 0, 0,
        0, 0, 0, 0, 0
    };
    bool blurMatchesLegacy = true;
    for (int i = 0; i < 25; ++i) {
        if (goldenPixels[i] != expectedBlur[i]) {
            blurMatchesLegacy = false;
            break;
        }
    }
    expect(blurMatchesLegacy,
           "imagefilter_blurring keeps the established 4-neighbor pixel matrix");

    ege::delimage(blurGolden);
    ege::delimage(blur);
    ege::delimage(image);
}

void testViewportClearAndWritingMode()
{
    ege::PIMAGE image = ege::newimage(18, 16);
    resetImage(image, ege::WHITE);
    ege::setviewport(4, 3, 14, 12, false, image);
    int viewportLeft = 0;
    int viewportTop = 0;
    int viewportRight = 0;
    int viewportBottom = 0;
    int viewportClip = 1;
    ege::getviewport(&viewportLeft, &viewportTop, &viewportRight, &viewportBottom,
                     &viewportClip, image);
    expect(viewportLeft == 4 && viewportTop == 3 && viewportRight == 14 &&
           viewportBottom == 12 && viewportClip == 0,
           "getviewport returns the selected image viewport");
    ege::setbkcolor_f(ege::BLACK, image);
    ege::clearviewport(image);
    ege::setviewport(0, 0, 18, 16, false, image);
    expectPixel(image, 7, 7, ege::BLACK, "clearviewport clears the viewport even when clipping is disabled");
    expectPixel(image, 2, 2, ege::WHITE, "clearviewport preserves pixels outside the viewport");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setwritemode(R2_XORPEN, image);
    ege::line(2, 5, 15, 5, image);
    expectPixel(image, 8, 5, ege::WHITE, "R2_XORPEN draws a line on the first pass");
    ege::line(2, 5, 15, 5, image);
    expectPixel(image, 8, 5, ege::BLACK, "drawing twice with R2_XORPEN restores the destination");

    ege::PIMAGE source = ege::newimage(2, 2);
    resetImage(source, ege::RED);
    resetImage(image, ege::BLUE);
    ege::putimage(image, 0, 0, source, SRCCOPY);
    expectPixel(image, 0, 0, ege::RED, "putimage SRCCOPY is independent of the pen writing mode");
    ege::setwritemode(R2_COPYPEN, image);

    ege::delimage(source);
    ege::delimage(image);
}

void testConcavePolygonAndRoundedRectangleCoverage()
{
    ege::PIMAGE image = ege::newimage(20, 18);
    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::GREEN, image);
    const int concave[] = {2, 2, 16, 2, 16, 15, 11, 15,
                           11, 7, 7, 7, 7, 15, 2, 15};
    ege::fillpoly(8, concave, image);
    expectPixel(image, 4, 10, ege::GREEN, "concave polygon fills its left arm");
    expectPixel(image, 14, 10, ege::GREEN, "concave polygon fills its right arm");
    expectPixel(image, 9, 11, ege::BLACK, "concave polygon leaves its notch unfilled");

    resetImage(image, ege::BLACK);
    ege::fillroundrect(2, 2, 17, 15, 4, image);
    expectPixel(image, 15, 8, ege::GREEN, "fillroundrect covers the right-center area");
    expectPixel(image, 8, 13, ege::GREEN, "fillroundrect covers the bottom-center area");

    ege::delimage(image);
}

void testEnhancedTransformAndGradientFallback()
{
    ege::PIMAGE image = ege::newimage(32, 20);
    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::ege_transform_reset(image);
    ege::ege_transform_translate(10.0f, 6.0f, image);
    ege::ege_transform_matrix currentTransform = {};
    ege::ege_get_transform(&currentTransform, image);
    expect(currentTransform.m31 == 10.0f && currentTransform.m32 == 6.0f,
           "ege_get_transform returns the active translation");
    const ege::ege_point translatedPoint = ege::ege_transform_calc(2.0f, 3.0f, image);
    expect(translatedPoint.x == 12.0f && translatedPoint.y == 9.0f,
           "ege_transform_calc applies the current transform to its coordinate input");
    ege::ege_line(0.0f, 0.0f, 8.0f, 0.0f, image);
    ege::ege_transform_reset(image);

    const ege::ege_transform_matrix explicitTransform = {
        1.0f, 0.0f, 0.0f, 1.0f, 3.0f, 4.0f
    };
    ege::ege_set_transform(&explicitTransform, image);
    const ege::ege_point explicitlyTranslated = ege::ege_transform_calc(2.0f, 3.0f, image);
    expect(explicitlyTranslated.x == 5.0f && explicitlyTranslated.y == 7.0f,
           "ege_set_transform applies an explicit matrix");
    ege::ege_transform_reset(image);

    ege::ege_transform_rotate(90.0f, image);
    ege::ege_get_transform(&currentTransform, image);
    expect(std::abs(currentTransform.m12) > 0.9f &&
           std::abs(currentTransform.m21) > 0.9f,
           "ege_transform_rotate updates the transform matrix");
    ege::ege_transform_reset(image);
    const PixelBounds transformedLine = boundsDifferentFrom(image, ege::BLACK);
    expect(transformedLine.valid && transformedLine.left >= 9 && transformedLine.right <= 19 &&
           transformedLine.top >= 5 && transformedLine.bottom <= 7,
           "enhanced drawing applies the native transform matrix");
    expectPixel(image, 4, 0, ege::BLACK, "enhanced transform does not leave geometry at its untransformed position");

    ege::ege_transform_scale(2.0f, 3.0f, image);
    const ege::ege_point sourcePoint = {4.0f, 5.0f};
    const ege::ege_point scaledPoint = ege::ege_transform_calc(sourcePoint, image);
    expect(scaledPoint.x == 8.0f && scaledPoint.y == 15.0f,
           "ege_transform_scale and point-form ege_transform_calc agree");
    ege::ege_transform_reset(image);

    resetImage(image, ege::BLACK);
    ege::ege_setpattern_lineargradient(2.0f, 0.0f, ege::RED,
                                       28.0f, 0.0f, ege::BLUE, image);
    ege::ege_fillrect(2.0f, 3.0f, 26.0f, 8.0f, image);
    const ege::color_t left = ege::getpixel(3, 6, image);
    const ege::color_t right = ege::getpixel(27, 6, image);
    expect(EGEGET_R(left) > EGEGET_B(left), "linear gradient starts near its first color");
    expect(EGEGET_B(right) > EGEGET_R(right), "linear gradient ends near its second color");

    resetImage(image, ege::BLACK);
    ege::ege_setpattern_lineargradient(0.0f, 0.0f, ege::RED,
                                       8.0f, 0.0f, ege::BLUE, image);
    ege::ege_transform_translate(10.0f, 6.0f, image);
    ege::ege_fillrect(0.0f, 0.0f, 8.0f, 6.0f, image);
    ege::ege_transform_reset(image);
    expectPixel(image, 3, 3, ege::BLACK,
                "enhanced transform moves patterned fills away from their source coordinates");
    expect(rgb(ege::getpixel(13, 9, image)) != rgb(ege::BLACK),
           "enhanced transform applies to patterned fill geometry");
    ege::ege_setpattern_none(image);

    ege::delimage(image);
}

void testRoundedShapesFloodFillAndFloatRoutes()
{
    ege::PIMAGE image = ege::newimage(24, 20);
    resetImage(image, ege::BLACK);

    ege::setfillcolor(ege::GREEN, image);
    ege::fillroundrect(2, 2, 16, 16, 4, image);
    expectPixel(image, 2, 2, ege::BLACK, "fillroundrect leaves its rounded corner outside");
    expectPixel(image, 8, 3, ege::GREEN, "fillroundrect fills the top-center area");
    expectPixel(image, 8, 8, ege::GREEN, "fillroundrect fills its center");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::rectangle(2, 2, 12, 12, image);
    ege::setfillcolor(ege::CYAN, image);
    ege::floodfill(5, 5, ege::WHITE, image);
    expectPixel(image, 5, 5, ege::CYAN, "floodfill colors its seed pixel");
    expectPixel(image, 6, 5, ege::CYAN, "floodfill expands across the bounded area");
    expectPixel(image, 2, 5, ege::WHITE, "floodfill preserves the border");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::rectangle(2, 2, 12, 12, image);
    ege::setbkcolor_f(ege::BLUE, image);
    ege::setfillstyle(ege::LINE_FILL, ege::RED, image);
    ege::floodfill(5, 5, ege::WHITE, image);
    expect(countPixelsEqualToInRect(image, ege::RED, 3, 3, 12, 12) > 0,
           "floodfill applies the selected hatch foreground");
    expectPixel(image, 5, 5, ege::BLUE, "floodfill applies the selected hatch background");

    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::YELLOW, image);
    ege::solidrect(3, 3, 9, 9, image);
    expectPixel(image, 5, 5, ege::YELLOW, "solidrect is available on the native backend");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::RED, image);
    ege::ellipsef(12.0f, 10.0f, 0.0f, 360.0f, 6.0f, 4.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 0,
           "floating-point ellipse routes to the native backend");

    ege::delimage(image);
}

void testAdditionalPrimitiveEntryPoints()
{
    ege::PIMAGE image = ege::newimage(64, 64);
    const int triangle[] = {8, 48, 20, 32, 32, 48};

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::line_f(2.5f, 2.5f, 30.5f, 2.5f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 0,
           "line_f draws floating-point line geometry");

    resetImage(image, ege::BLACK);
    ege::moveto(3, 4, image);
    ege::lineto_f(24.0f, 4.0f, image);
    ege::linerel(0, 12, image);
    ege::linerel_f(-12.0f, 0.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 20,
           "lineto_f and linerel variants draw from the current position");
    expectPixel(image, 24, 10, ege::WHITE,
                "integer linerel draws the segment to its new position");

    resetImage(image, ege::BLACK);
    ege::moveto(2, 6, image);
    ege::lineto(26, 6, image);
    expectPixel(image, 14, 6, ege::WHITE,
                "integer lineto draws from the current position");

    resetImage(image, ege::BLACK);
    ege::arcf(16.0f, 16.0f, 0.0f, 180.0f, 10.0f, image);
    ege::arc(16, 16, 180, 360, 10, image);
    ege::circlef(44.0f, 16.0f, 9.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 20,
           "arcf and circlef draw floating-point outlines");

    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::GREEN, image);
    ege::fillcirclef(12.0f, 12.0f, 7.0f, image);
    ege::solidcirclef(30.0f, 12.0f, 7.0f, image);
    ege::fillellipsef(48.0f, 12.0f, 7.0f, 5.0f, image);
    ege::solidellipsef(12.0f, 30.0f, 7.0f, 5.0f, image);
    expectPixel(image, 12, 12, ege::GREEN,
                "fillcirclef fills its center");
    expectPixel(image, 48, 12, ege::GREEN,
                "fillellipsef fills its center");

    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::CYAN, image);
    ege::setlinecolor(ege::WHITE, image);
    ege::pie(12, 50, 0, 90, 9, 7, image);
    ege::pief(32.0f, 50.0f, 0.0f, 90.0f, 9.0f, 7.0f, image);
    ege::fillpief(50.0f, 50.0f, 0.0f, 90.0f, 8.0f, 7.0f, image);
    ege::solidpief(50.0f, 30.0f, 0.0f, 90.0f, 8.0f, 7.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 30,
           "integer and floating-point pie routes produce geometry");

    resetImage(image, ege::BLACK);
    ege::pieslice(12, 12, 0, 90, 8, image);
    ege::pieslicef(32.0f, 12.0f, 0.0f, 90.0f, 8.0f, image);
    ege::sector(12, 36, 0, 90, 8, 6, image);
    ege::sectorf(32.0f, 36.0f, 0.0f, 90.0f, 8.0f, 6.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 20,
           "pie-slice and sector aliases produce geometry");

    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::BLUE, image);
    ege::bar3d(4, 4, 24, 20, 5, 1, image);
    ege::polygon(3, triangle, image);
    const int closedTriangle[] = {38, 48, 50, 32, 62, 48, 38, 48};
    ege::drawpoly(4, closedTriangle, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 40,
           "bar3d and polygon entry points draw visible pixels");

    resetImage(image, ege::BLACK);
    const ege::ege_colpoint gradientTriangle[] = {
        {4.0f, 4.0f, ege::RED},
        {40.0f, 4.0f, ege::GREEN},
        {4.0f, 40.0f, ege::BLUE}
    };
    ege::fillpoly_gradient(3, gradientTriangle, image);
    expect(rgb(ege::getpixel(10, 10, image)) != rgb(ege::BLACK),
           "fillpoly_gradient rasterizes a colored triangle");

    expect(ege::resize_f(image, 72, 68) == ege::grOk &&
           ege::getwidth(image) == 72 && ege::getheight(image) == 68,
           "resize_f updates native image dimensions");
    ege::delimage(image);
}

void testAdditionalEnhancedEntryPoints()
{
    ege::PIMAGE image = ege::newimage(80, 64);
    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::WHITE, image);
    ege::setfillcolor(ege::GREEN, image);
    ege::ege_enable_aa(true, image);
    ege::ege_circle(40.0f, 32.0f, 12.0f, image);

    const ege::ege_point openCurve[] = {
        {4.0f, 8.0f}, {16.0f, 2.0f}, {28.0f, 14.0f}, {40.0f, 8.0f}
    };
    const ege::ege_point closedCurve[] = {
        {46.0f, 4.0f}, {70.0f, 4.0f}, {74.0f, 22.0f}, {54.0f, 24.0f}
    };
    ege::ege_polyline(4, openCurve, image);
    ege::ege_polygon(4, closedCurve, image);
    ege::ege_bezier(4, openCurve, image);
    ege::ege_drawbezier(4, openCurve, image);
    ege::ege_drawcurve(4, openCurve, image);
    ege::ege_drawclosedcurve(4, closedCurve, image);
    ege::ege_drawcurve(4, openCurve, 0.5f, image);
    ege::ege_drawclosedcurve(4, closedCurve, 0.5f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 60,
           "enhanced polyline, polygon, Bezier, and curve routes draw pixels");
    ege::ege_enable_aa(false, image);

    resetImage(image, ege::BLACK);
    ege::ege_fillclosedcurve(4, closedCurve, image);
    expect(countPixelsEqualToInRect(image, ege::GREEN, 52, 8, 70, 20) > 20,
           "default ege_fillclosedcurve overload fills the closed spline interior");

    resetImage(image, ege::BLACK);
    ege::ege_fillclosedcurve(4, closedCurve, 0.5f, image);
    expect(countPixelsEqualToInRect(image, ege::GREEN, 52, 8, 70, 20) > 20,
           "ege_fillclosedcurve fills the closed spline interior");

    resetImage(image, ege::BLACK);
    ege::ege_rectangle(3.0f, 30.0f, 16.0f, 12.0f, image);
    ege::ege_roundrect(23.0f, 30.0f, 18.0f, 12.0f, 4.0f, image);
    ege::ege_roundrect(48.0f, 30.0f, 24.0f, 14.0f,
                       2.0f, 4.0f, 6.0f, 8.0f, image);
    ege::ege_arc(45.0f, 28.0f, 20.0f, 16.0f, 0.0f, 180.0f, image);
    ege::ege_pie(4.0f, 46.0f, 18.0f, 14.0f, 0.0f, 90.0f, image);
    ege::ege_fillpie(28.0f, 46.0f, 18.0f, 14.0f, 0.0f, 90.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 40,
           "enhanced rectangle, round-rectangle, arc, and pie routes draw pixels");

    resetImage(image, ege::BLACK);
    const ege::ege_point gradientBoundary[] = {
        {4.0f, 4.0f}, {36.0f, 4.0f}, {36.0f, 28.0f}, {4.0f, 28.0f}
    };
    const ege::color_t gradientColors[] = {
        ege::BLUE, ege::BLUE, ege::BLUE, ege::BLUE
    };
    const ege::ege_point gradientCenter = {20.0f, 16.0f};
    ege::ege_setpattern_pathgradient(gradientCenter, ege::RED, 4,
                                     gradientBoundary, 4, gradientColors, image);
    ege::ege_fillrect(4.0f, 4.0f, 32.0f, 24.0f, image);
    const ege::color_t pathCenter = ege::getpixel(20, 16, image);
    const ege::color_t pathEdge = ege::getpixel(5, 5, image);
    expect(EGEGET_R(pathCenter) > EGEGET_B(pathCenter) &&
           EGEGET_B(pathEdge) > EGEGET_R(pathEdge),
           "path-gradient pattern transitions from center to boundary colors");

    resetImage(image, ege::BLACK);
    ege::ege_setpattern_ellipsegradient(gradientCenter, ege::RED,
                                        4.0f, 4.0f, 32.0f, 24.0f,
                                        ege::BLUE, image);
    ege::ege_fillrect(4.0f, 4.0f, 32.0f, 24.0f, image);
    const ege::color_t ellipseCenter = ege::getpixel(20, 16, image);
    const ege::color_t ellipseEdge = ege::getpixel(20, 5, image);
    expect(EGEGET_R(ellipseCenter) > EGEGET_B(ellipseCenter) &&
           EGEGET_B(ellipseEdge) > EGEGET_R(ellipseEdge),
           "ellipse-gradient pattern transitions from center to boundary colors");

    ege::PIMAGE texture = ege::newimage(2, 2);
    ege::color_t* texturePixels = ege::getbuffer(texture);
    texturePixels[0] = ege::RED;
    texturePixels[1] = ege::GREEN;
    texturePixels[2] = ege::BLUE;
    texturePixels[3] = ege::WHITE;
    ege::ege_gentexture(true, texture);
    resetImage(image, ege::BLACK);
    ege::ege_setpattern_texture(texture, 0.0f, 0.0f, 2.0f, 2.0f, image);
    ege::ege_fillrect(2.0f, 2.0f, 20.0f, 16.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 200,
           "texture pattern repeats source pixels across a fill");
    resetImage(image, ege::BLACK);
    const ege::ege_rect textureDestination = {4.0f, 4.0f, 16.0f, 16.0f};
    ege::ege_puttexture(texture, textureDestination, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 100,
           "ege_puttexture draws a generated texture");
    ege::ege_gentexture(false, texture);
    ege::ege_setpattern_none(image);

    ege::delimage(texture);
    ege::delimage(image);
}

void testArcAndPieAngleOrientation()
{
    ege::PIMAGE image = ege::newimage(24, 24);
    resetImage(image, ege::BLACK);
    ege::setfillcolor(ege::GREEN, image);
    ege::fillpie(12, 12, 0, 90, 8, 8, image);

    expectPixel(image, 15, 9, ege::GREEN, "positive pie angles sweep toward the upper-right quadrant");
    expectPixel(image, 15, 15, ege::BLACK, "positive pie angles do not sweep toward the lower-right quadrant");

    resetImage(image, ege::BLACK);
    ege::setlinecolor(ege::RED, image);
    ege::ellipse(12, 12, 0, 90, 8, 8, image);
    expectPixel(image, 12, 4, ege::RED, "a 0..90 degree arc reaches the top endpoint");
    expectPixel(image, 12, 20, ege::BLACK, "a 0..90 degree arc does not reach the bottom endpoint");

    ege::delimage(image);
}

void testTextRendering()
{
    ege::PIMAGE image = ege::newimage(80, 32);
    resetImage(image, ege::BLACK);
    ege::setfont(18, 0, "Arial", image);
    ege::settextcolor(ege::WHITE, image);
    ege::settextjustify(ege::LEFT_TEXT, ege::TOP_TEXT, image);

    expect(ege::textwidth("Test", image) > 0, "textwidth uses the native font backend");
    expect(ege::textheight("Test", image) > 0, "textheight uses the native font backend");
    float measuredWidth = 0.0f;
    float measuredHeight = 0.0f;
    ege::measuretext("Test", &measuredWidth, &measuredHeight, image);
    expect(measuredWidth > 0.0f && measuredHeight > 0.0f,
           "measuretext returns positive enhanced-font dimensions");
    ege::outtextxy(2, 2, "Test", image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 0, "outtextxy renders glyph pixels");

    resetImage(image, ege::BLACK);
    ege::moveto(2, 2, image);
    ege::outtext("Test", image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 0,
           "outtext renders from the current position");

    resetImage(image, ege::BLACK);
    ege::ege_drawtext("Test", 2.0f, 2.0f, image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 0,
           "ege_drawtext renders through the enhanced text route");

    resetImage(image, ege::BLACK);
    ege::setfontbkcolor(ege::BLUE, image);
    ege::setbkmode(OPAQUE, image);
    ege::outtextxy(2, 2, "Test", image);
    expect(countPixelsEqualToInRect(image, ege::BLUE, 2, 2, 50, 24) > 0,
           "setfontbkcolor supplies the opaque text background");
    ege::setbkmode(TRANSPARENT, image);

    ege::delimage(image);
}

void testFormattedScreenText()
{
    ege::setfont(14, 0, "Arial");
    ege::settextcolor(ege::WHITE);
    ege::settextjustify(ege::LEFT_TEXT, ege::TOP_TEXT);
    ege::setbkmode(TRANSPARENT);

    resetImage(nullptr, ege::BLACK);
    ege::xyprintf(2, 2, "%s %d", "xy", 7);
    expect(countPixelsDifferentFrom(nullptr, ege::BLACK) > 0,
           "xyprintf formats and renders narrow text on the active page");

    resetImage(nullptr, ege::BLACK);
    ege::xyprintf(2, 2, L"%ls %d", L"wide", 8);
    expect(countPixelsDifferentFrom(nullptr, ege::BLACK) > 0,
           "xyprintf formats and renders wide text on the active page");

    resetImage(nullptr, ege::BLACK);
    ege::ege_xyprintf(2.0f, 2.0f, "%s %d", "enhanced", 9);
    expect(countPixelsDifferentFrom(nullptr, ege::BLACK) > 0,
           "ege_xyprintf formats and renders narrow enhanced text");

    resetImage(nullptr, ege::BLACK);
    ege::ege_xyprintf(2.0f, 2.0f, L"%ls %d", L"wide", 10);
    expect(countPixelsDifferentFrom(nullptr, ege::BLACK) > 0,
           "ege_xyprintf formats and renders wide enhanced text");

    resetImage(nullptr, ege::BLACK);
    ege::rectprintf(2, 2, 60, 60, "%s %d", "rect", 11);
    expect(countPixelsDifferentFrom(nullptr, ege::BLACK) > 0,
           "rectprintf formats and wraps narrow text");

    resetImage(nullptr, ege::BLACK);
    ege::rectprintf(2, 2, 60, 60, L"%ls %d", L"wide", 12);
    expect(countPixelsDifferentFrom(nullptr, ege::BLACK) > 0,
           "rectprintf formats and wraps wide text");
}

void testWindowCaptionEncoding()
{
#ifdef _WIN32
    const wchar_t* expectedCaption = L"EGE 中文标题";
    ege::setcaption(expectedCaption);

    wchar_t actualCaption[128] = {};
    const int copied = ::GetWindowTextW(ege::getHWnd(), actualCaption,
                                        static_cast<int>(sizeof(actualCaption) /
                                                         sizeof(actualCaption[0])));
    expect(copied > 0 && std::wstring(actualCaption) == expectedCaption,
           "wide window captions preserve Unicode text on the native window");
#endif
}

void testWindowAndPublicStateQueries()
{
    expect(ege::getGraphicsVer() > 0, "getGraphicsVer returns a positive version number");

    const bool previousUnicodeMode = ege::getunicodecharmessage();
    ege::setunicodecharmessage(!previousUnicodeMode);
    expect(ege::getunicodecharmessage() == !previousUnicodeMode,
           "Unicode character-message mode round-trips");
    ege::setunicodecharmessage(previousUnicodeMode);

    ege::viewporttype viewport = {};
    ege::window_getviewport(&viewport);
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    ege::window_getviewport(&left, &top, &right, &bottom);
    expect(left == viewport.left && top == viewport.top &&
           right == viewport.right && bottom == viewport.bottom,
           "window_getviewport overloads return the same bounds");
    ege::window_setviewport(left, top, right, bottom);

#ifdef _WIN32
    expect(ege::getHInstance() != NULL, "getHInstance returns the process instance");
    expect(ege::getProcfunc() != NULL, "getProcfunc exposes the EGE window procedure");
    const HDC nativeDc = ege::getHDC();
#if defined(EGE_BUILD_OPENGL)
    const bool runningOpenGl = (ege::getinitmode() & ege::INIT_OPENGL) != 0;
    expect(runningOpenGl ? nativeDc == NULL : nativeDc != NULL,
           "getHDC distinguishes OpenGL targets from GDI targets");
#else
    expect(nativeDc != NULL, "getHDC returns the GDI drawing context");
#endif
    ege::seticon(0);

    RECT originalWindowRect = {};
    if (::GetWindowRect(ege::getHWnd(), &originalWindowRect)) {
        ege::movewindow(24, 32, false);
        RECT movedWindowRect = {};
        expect(::GetWindowRect(ege::getHWnd(), &movedWindowRect) &&
                   movedWindowRect.left == 24 && movedWindowRect.top == 32,
               "movewindow updates the native window position for the selected backend");
        ege::movewindow(originalWindowRect.left, originalWindowRect.top, false);
    }
#endif
}

void testLegacyPageSwap()
{
    ege::setactivepage(0);
    resetImage(nullptr, ege::RED);
    ege::setactivepage(1);
    resetImage(nullptr, ege::BLUE);

    ege::setactivepage(0);
    ege::swappage();
    expectPixel(nullptr, 1, 1, ege::BLUE,
                "swappage makes the other page active after presenting page zero");
    ege::swappage();
    expectPixel(nullptr, 1, 1, ege::RED,
                "swappage alternates back to the original active page");

    ege::setactivepage(0);
    ege::setvisualpage(0);
}

void testCompressionTimingAndRandomUtilities()
{
    const unsigned char sourceBytes[] = {
        0, 1, 2, 3, 3, 3, 3, 3, 9, 8, 7, 6, 5, 4, 3, 2,
        1, 0, 0, 0, 0, 0, 42, 42, 42, 42, 42, 42, 42, 42
    };
    const uint32_t sourceSize = static_cast<uint32_t>(sizeof(sourceBytes));
    const uint32_t bound = ege::ege_compress_bound(sourceSize);
    expect(bound >= sourceSize, "ege_compress_bound provides sufficient output capacity");

    std::vector<unsigned char> compressed(bound);
    uint32_t compressedSize = bound;
    expect(ege::ege_compress(compressed.data(), &compressedSize,
                             sourceBytes, sourceSize) == ege::grOk,
           "ege_compress accepts a buffer sized by ege_compress_bound");
    expect(compressedSize > sizeof(uint32_t) && compressedSize <= bound,
           "ege_compress reports a bounded compressed size");
    expect(ege::ege_uncompress_size(compressed.data(), compressedSize) == sourceSize,
           "ege_uncompress_size reports the original payload length");

    std::vector<unsigned char> restored(sourceSize);
    uint32_t restoredSize = sourceSize;
    expect(ege::ege_uncompress(restored.data(), &restoredSize,
                               compressed.data(), compressedSize) == ege::grOk &&
               restoredSize == sourceSize &&
               std::equal(restored.begin(), restored.end(), sourceBytes),
           "ege_uncompress restores the exact original payload");

    compressedSize = bound;
    expect(ege::ege_compress2(compressed.data(), &compressedSize,
                              sourceBytes, sourceSize, 9) == ege::grOk,
           "ege_compress2 accepts an explicit compression level");

    expect(ege::randomize(0x12345678U) == 0x12345678U,
           "randomize returns the explicit seed");
    const unsigned int firstRandom = ege::random(1000000U);
    ege::randomize(0x12345678U);
    expect(ege::random(1000000U) == firstRandom,
           "randomize with the same seed reproduces the integer sequence");
    const double randomUnit = ege::randomf();
    expect(randomUnit >= 0.0 && randomUnit < 1.0,
           "randomf stays in its documented half-open unit interval");
    (void)ege::randomize();

    const double clockBefore = ege::fclock();
    ege::ege_sleep(0);
    ege::api_sleep(0);
    ege::delay(0);
    ege::delay_fps(1000000);
    ege::delay_fps(1000000L);
    ege::delay_fps(1000000.0);
    ege::delay_jfps(1000000);
    ege::delay_jfps(1000000L);
    ege::delay_jfps(1000000.0);
    const double clockAfter = ege::fclock();
    expect(clockAfter >= clockBefore, "fclock is monotonic across zero-duration delays");
    expect(ege::getfps() >= 0.0f, "getfps never reports a negative frame rate");
}

void testFontCompatibilityDetails()
{
    ege::PIMAGE image = ege::newimage(180, 90);
    resetImage(image, ege::BLACK);
    ege::settextcolor(ege::WHITE, image);

    ege::setfont(22, 0, "Arial", image);
    const int naturalWidth = ege::textwidth("MMMM", image);
    expect(naturalWidth > 0, "natural font width is measurable");
    expect(ege::textwidth("\xF0\x9F\x98\x80", image) > 0,
           "four-byte UTF-8 contributes one codepoint to text measurement");

    ege::setfont(22, 5, "Arial", image);
    const int narrowWidth = ege::textwidth("MMMM", image);
    expect(narrowWidth > 0 && narrowWidth < naturalWidth,
           "setfont width scales glyph geometry and advance widths");

#ifdef _WIN32
    ege::setfont(20, 0, "Arial", image);
    const SIZE windowsArialMetrics = measureWindowsGdiText(L"Hello", 20, 0, L"Arial");
    expect(windowsArialMetrics.cx > 0 &&
           ege::textwidth(L"Hello", image) == windowsArialMetrics.cx,
           "Windows textwidth matches the GDI font mapper and advance metrics");
    expect(windowsArialMetrics.cy > 0 &&
           ege::textheight(L"Hello", image) == windowsArialMetrics.cy,
           "Windows textheight matches the GDI font mapper and cell metrics");

    ege::setfont(28, 0, L"PingFang SC", image);
    const int chineseWidth = ege::textwidth(L"\u4E2D\u6587", image);
    const int missingGlyphWidth = ege::textwidth(L"??", image);
    expect(chineseWidth > 0 && chineseWidth != missingGlyphWidth,
           "a missing requested CJK face uses Windows-compatible CJK glyph fallback");

    const unsigned int previousCodePage = ege::getcodepage();
    ege::setcodepage(EGE_CODEPAGE_UTF8);
    ege::setfont(28, 0, "\xE5\xAE\x8B\xE4\xBD\x93", image);
    expect(ege::textwidth(L"\u4E2D\u6587", image) != ege::textwidth(L"??", image),
           "UTF-8 Chinese Windows font family names resolve to CJK glyphs");
    resetImage(image, ege::BLACK);
    ege::outtextxy(4, 4, L"\u4E2D", image);
    const ege::color_t* chineseGlyphBuffer = ege::getbuffer(image);
    const size_t glyphPixelCount = static_cast<size_t>(ege::getwidth(image)) *
                                   static_cast<size_t>(ege::getheight(image));
    const std::vector<ege::color_t> chineseGlyphPixels(
        chineseGlyphBuffer, chineseGlyphBuffer + glyphPixelCount);
    resetImage(image, ege::BLACK);
    ege::outtextxy(4, 4, L"?", image);
    expect(!std::equal(chineseGlyphPixels.begin(), chineseGlyphPixels.end(),
                       ege::getbuffer(image)),
           "a localized CJK face rasterizes a real Chinese glyph rather than a question mark");

    const char simSunGb2312[] = "\xCB\xCE\xCC\xE5";
    const char chineseGb2312[] = "\xD6\xD0";
    ege::setcodepage(EGE_CODEPAGE_GB2312);
    ege::setfont(28, 0, simSunGb2312, image);

    LOGFONTW selectedGb2312Font = {};
    ege::getfont(&selectedGb2312Font, image);
    expect(std::wstring(selectedGb2312Font.lfFaceName) == L"\u5B8B\u4F53",
           "GB2312 font family names round-trip through the selected font state");

    resetImage(image, ege::BLACK);
    ege::outtextxy(4, 4, chineseGb2312, image);
    const ege::color_t* gb2312GlyphBuffer = ege::getbuffer(image);
    const std::vector<ege::color_t> gb2312GlyphPixels(
        gb2312GlyphBuffer, gb2312GlyphBuffer + glyphPixelCount);
    resetImage(image, ege::BLACK);
    ege::outtextxy(4, 4, "?", image);
    expect(!std::equal(gb2312GlyphPixels.begin(), gb2312GlyphPixels.end(),
                       ege::getbuffer(image)),
           "GB2312 text uses the selected CJK font instead of a question mark");

    resetImage(image, ege::BLACK);
    ege::ege_outtextxy(4.0f, 4.0f, chineseGb2312, image);
    const ege::color_t* enhancedGb2312Buffer = ege::getbuffer(image);
    const std::vector<ege::color_t> enhancedGb2312Pixels(
        enhancedGb2312Buffer, enhancedGb2312Buffer + glyphPixelCount);
    resetImage(image, ege::BLACK);
    ege::ege_outtextxy(4.0f, 4.0f, L"\u4E2D", image);
    expect(std::equal(enhancedGb2312Pixels.begin(), enhancedGb2312Pixels.end(),
                      ege::getbuffer(image)),
           "enhanced GB2312 text matches the equivalent wide text");
    ege::setcodepage(previousCodePage);
#endif

    ege::setfont(23, 1, "Arial", 30, 20, 600, true, false, true,
                 static_cast<BYTE>(0), static_cast<BYTE>(0), static_cast<BYTE>(0),
                 static_cast<BYTE>(0), static_cast<BYTE>(0), image);
    LOGFONTW advancedNarrowFont = {};
    ege::getfont(&advancedNarrowFont, image);
    expect(advancedNarrowFont.lfHeight == 23 && advancedNarrowFont.lfWidth == 1 &&
           advancedNarrowFont.lfEscapement == 30 && advancedNarrowFont.lfOrientation == 20 &&
           advancedNarrowFont.lfWeight == 600 && advancedNarrowFont.lfItalic &&
           advancedNarrowFont.lfStrikeOut,
           "advanced narrow setfont overload preserves its native font state");

    ege::setfont(24, 2, L"Arial", 40, 10, 500, false, true, false,
                 static_cast<BYTE>(0), static_cast<BYTE>(0), static_cast<BYTE>(0),
                 static_cast<BYTE>(0), static_cast<BYTE>(0), image);
    LOGFONTW advancedWideFont = {};
    ege::getfont(&advancedWideFont, image);
    expect(advancedWideFont.lfHeight == 24 && advancedWideFont.lfWidth == 2 &&
           advancedWideFont.lfEscapement == 40 && advancedWideFont.lfOrientation == 10 &&
           advancedWideFont.lfWeight == 500 && advancedWideFont.lfUnderline,
           "advanced wide setfont overload preserves its native font state");

#ifdef _WIN32
    LOGFONTW wideFont = {};
    wideFont.lfHeight = 25;
    wideFont.lfWeight = 550;
    wideFont.lfItalic = TRUE;
    lstrcpyW(wideFont.lfFaceName, L"Arial");
    ege::setfont(&wideFont, image);
    LOGFONTW selectedWideFont = {};
    ege::getfont(&selectedWideFont, image);
    expect(selectedWideFont.lfHeight == 25 && selectedWideFont.lfWeight == 550 &&
           selectedWideFont.lfItalic,
           "LOGFONTW setfont overload selects the supplied native font state");

    LOGFONTA ansiFont = {};
    ansiFont.lfHeight = 26;
    ansiFont.lfWeight = 450;
    ansiFont.lfUnderline = TRUE;
    lstrcpyA(ansiFont.lfFaceName, "Arial");
    ege::setfont(&ansiFont, image);
    LOGFONTW selectedAnsiFont = {};
    ege::getfont(&selectedAnsiFont, image);
    expect(selectedAnsiFont.lfHeight == 26 && selectedAnsiFont.lfWeight == 450 &&
           selectedAnsiFont.lfUnderline,
           "LOGFONTA setfont overload selects the supplied native font state");
#endif

    ege::setfont(22, 0, "Arial", 120, 70, 650, true, true, true, image);
    LOGFONTW font = {};
    ege::getfont(&font, image);
    expect(font.lfHeight == 22 && font.lfWidth == 0 &&
           font.lfEscapement == 120 && font.lfOrientation == 70 &&
           font.lfWeight == 650 && font.lfItalic && font.lfUnderline && font.lfStrikeOut,
           "getfont returns the active native font configuration");
    expect(std::wstring(font.lfFaceName) == L"Arial",
           "getfont returns the active native font face");

    ege::setfont(22, 0, "Arial", 0, 0, 400, false, false, false, image);
    resetImage(image, ege::BLACK);
    ege::outtextxy(4, 4, "Underline", image);
    const int ordinaryPixels = countPixelsDifferentFrom(image, ege::BLACK);
    resetImage(image, ege::BLACK);
    ege::setfont(22, 0, "Arial", 0, 0, 400, false, true, false, image);
    ege::outtextxy(4, 4, "Underline", image);
    const int underlinedPixels = countPixelsDifferentFrom(image, ege::BLACK);
    expect(underlinedPixels > ordinaryPixels,
           "underline font style adds visible decoration pixels");

    resetImage(image, ege::BLACK);
    ege::setfont(28, 0, "Arial", 0, 0, 400, false, false, false, image);
    ege::outtextxy(4, 4, "Bold text", image);
    const int regularWeightPixels = countPixelsDifferentFrom(image, ege::BLACK);
    resetImage(image, ege::BLACK);
    ege::setfont(28, 0, "Arial", 0, 0, 700, false, false, false, image);
    ege::outtextxy(4, 4, "Bold text", image);
    const int boldWeightPixels = countPixelsDifferentFrom(image, ege::BLACK);
    expect(boldWeightPixels > regularWeightPixels,
           "bold font weight selects visibly heavier glyphs");

    resetImage(image, ege::BLACK);
    ege::setfont(28, 0, "Arial", 0, 0, 400, false, false, false, image);
    ege::outtextxy(4, 4, "M", image);
    const ege::color_t* regularStyleBuffer = ege::getbuffer(image);
    const size_t pixelCount = static_cast<size_t>(ege::getwidth(image)) *
                              static_cast<size_t>(ege::getheight(image));
    const std::vector<ege::color_t> regularStylePixels(
        regularStyleBuffer, regularStyleBuffer + pixelCount);
    resetImage(image, ege::BLACK);
    ege::setfont(28, 0, "Arial", 0, 0, 400, true, false, false, image);
    ege::outtextxy(4, 4, "M", image);
    const ege::color_t* italicStylePixels = ege::getbuffer(image);
    expect(!std::equal(regularStylePixels.begin(), regularStylePixels.end(), italicStylePixels),
           "italic font style selects distinct glyph geometry");

    resetImage(image, ege::BLACK);
    ege::setfont(22, 0, "Arial", image);
    ege::setbkcolor_f(ege::BLUE, image);
    ege::setbkmode(OPAQUE, image);
    ege::outtextxy(4, 4, "I", image);
    expectPixel(image, 4, 4, ege::BLUE,
                "OPAQUE text background fills the text layout rectangle");
    ege::setbkmode(TRANSPARENT, image);

    resetImage(image, ege::BLACK);
    ege::setfont(28, 0, "Arial", 900, 900, 400, false, false, false, image);
    ege::outtextxy(70, 45, "I", image);
    const PixelBounds rotated = boundsDifferentFrom(image, ege::BLACK);
    expect(rotated.valid && (rotated.right - rotated.left) > (rotated.bottom - rotated.top),
           "font escapement rotates each glyph quad, not only the glyph origins");

#ifdef _WIN32
    resetImage(image, ege::BLACK);
    ege::setfont(28, 0, "Arial", 900, 0, 400, false, false, false, image);
    ege::outtextxy(70, 45, "W", image);
    const PixelBounds escapementOnly = boundsDifferentFrom(image, ege::BLACK);
    expect(escapementOnly.valid &&
           (escapementOnly.bottom - escapementOnly.top) >
               (escapementOnly.right - escapementOnly.left),
           "Windows escapement rotates glyphs when orientation is zero");
#endif

    ege::delimage(image);
}

void testPngAndBmpRoundTrip()
{
    const std::string pngPath = tempPath(".png");
    const std::string bmpPath = tempPath(".bmp");
    const std::string alphaPngPath = tempPath(".alpha.png");
    const std::string alphaBmpPath = tempPath(".alpha.bmp");
    const std::string genericPngPath = tempPath(".generic.png");
    const std::string genericBmpPath = tempPath(".generic.BMP");
    const std::string defaultPngPath = tempPath(".generic-no-extension");
    const std::string unicodeUtf8Path = tempPath("-\u56fe\u50cf.png");
    const std::wstring unicodeWidePath =
        L"xege-rendering-correctness-" + std::to_wstring(tempProcessId()) + L"-\u56fe\u50cf.png";

    ege::PIMAGE source = ege::newimage(7, 5);
    resetImage(source, ege::WHITE);
    ege::putpixel(1, 0, ege::RED, source);
    ege::putpixel(5, 4, ege::BLUE, source);
    ege::color_t* sourcePixels = ege::getbuffer(source);
    sourcePixels[2 * 7 + 3] = EGEARGB(128, 100, 50, 25);
    // GDI primitives write RGB while leaving the DIB alpha byte at zero.
    // Non-alpha file output must keep that visible RGB instead of treating it
    // as a fully transparent premultiplied pixel.
    sourcePixels[1 * 7 + 4] = EGEARGB(0, 0, 0, 255);

    expect(ege::savepng(source, pngPath.c_str()) == ege::grOk, "savepng writes a PNG file");
    expect(ege::savebmp(source, bmpPath.c_str()) == ege::grOk, "savebmp writes a BMP file");
    expect(ege::savepng(source, alphaPngPath.c_str(), true) == ege::grOk,
           "savepng writes an alpha-channel PNG file");
    expect(ege::savebmp(source, alphaBmpPath.c_str(), true) == ege::grOk,
           "savebmp writes an alpha-channel BITMAPV4 file");
    expect(ege::saveimage(source, genericPngPath.c_str()) == ege::grOk,
           "saveimage dispatches a .png filename to PNG encoding");
    expect(ege::saveimage(source, genericBmpPath.c_str()) == ege::grOk,
           "saveimage dispatches a case-insensitive .BMP filename to BMP encoding");
    expect(ege::saveimage(source, defaultPngPath.c_str()) == ege::grOk,
           "saveimage defaults to PNG when the filename has no recognized extension");
    expect(ege::saveimage(source, unicodeWidePath.c_str()) == ege::grOk,
           "saveimage accepts a wide filename containing non-ASCII characters");
    expect(ege::saveimage(source, "") == ege::grParamError,
           "saveimage rejects an empty filename");
    expect(ege::saveimage(source, tempPath("-missing-directory/out.png").c_str()) == ege::grIOerror,
           "saveimage reports an I/O error when the destination directory does not exist");

    const std::vector<unsigned char> pngBytes = readFileBytes(pngPath);
    const std::vector<unsigned char> alphaPngBytes = readFileBytes(alphaPngPath);
    const std::vector<unsigned char> bmpBytes = readFileBytes(bmpPath);
    const std::vector<unsigned char> alphaBmpBytes = readFileBytes(alphaBmpPath);
    const std::vector<unsigned char> genericPngBytes = readFileBytes(genericPngPath);
    const std::vector<unsigned char> genericBmpBytes = readFileBytes(genericBmpPath);
    const std::vector<unsigned char> defaultPngBytes = readFileBytes(defaultPngPath);
#ifdef _WIN32
    const std::vector<unsigned char> unicodePngBytes = readFileBytes(unicodeWidePath);
#else
    const std::vector<unsigned char> unicodePngBytes = readFileBytes(unicodeUtf8Path);
#endif

    const unsigned char pngSignature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const auto hasPngSignature = [&pngSignature](const std::vector<unsigned char>& bytes) {
        return bytes.size() >= sizeof(pngSignature) &&
               std::equal(pngSignature, pngSignature + sizeof(pngSignature), bytes.begin());
    };
    const auto hasBmpSignature = [](const std::vector<unsigned char>& bytes) {
        return bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M';
    };

    expect(hasPngSignature(pngBytes), "savepng emits the standard PNG signature");
    expect(hasPngSignature(genericPngBytes), "saveimage .png output has a PNG signature");
    expect(hasPngSignature(defaultPngBytes), "saveimage default output has a PNG signature");
    expect(hasPngSignature(unicodePngBytes), "wide-path saveimage output has a PNG signature");
    expect(hasBmpSignature(bmpBytes), "savebmp emits the standard BMP signature");
    expect(hasBmpSignature(genericBmpBytes), "saveimage .BMP output has a BMP signature");

    if (pngBytes.size() >= 26) {
        expect(readBigEndian32(pngBytes, 8) == 13 &&
               pngBytes[12] == 'I' && pngBytes[13] == 'H' &&
               pngBytes[14] == 'D' && pngBytes[15] == 'R',
               "PNG starts with a 13-byte IHDR chunk");
        expect(readBigEndian32(pngBytes, 16) == 7 && readBigEndian32(pngBytes, 20) == 5,
               "PNG IHDR independently records the requested dimensions");
        expect(pngBytes[25] == 2, "RGB PNG declares truecolor without alpha");
    }
    if (alphaPngBytes.size() >= 26) {
        expect(hasPngSignature(alphaPngBytes) && alphaPngBytes[25] == 6,
               "alpha PNG declares truecolor with alpha");
    }
    if (bmpBytes.size() >= 54) {
        expect(readLittleEndian32(bmpBytes, 2) == bmpBytes.size(),
               "BMP header file size matches the bytes written");
        expect(readLittleEndian32(bmpBytes, 14) == 40 &&
               readLittleEndian32(bmpBytes, 18) == 7 && readLittleEndian32(bmpBytes, 22) == 5,
               "BMP info header independently records its type and dimensions");
        expect(readLittleEndian16(bmpBytes, 28) == 24 && readLittleEndian32(bmpBytes, 30) == BI_RGB,
               "RGB BMP declares an uncompressed 24-bit payload");
    }
    if (alphaBmpBytes.size() >= 70) {
        expect(hasBmpSignature(alphaBmpBytes) && readLittleEndian32(alphaBmpBytes, 14) == 108,
               "alpha BMP uses a BITMAPV4 header");
        expect(readLittleEndian16(alphaBmpBytes, 28) == 32 &&
               readLittleEndian32(alphaBmpBytes, 30) == BI_BITFIELDS,
               "alpha BMP declares a 32-bit bitfield payload");
        expect(readLittleEndian32(alphaBmpBytes, 54) == 0x00FF0000U &&
               readLittleEndian32(alphaBmpBytes, 58) == 0x0000FF00U &&
               readLittleEndian32(alphaBmpBytes, 62) == 0x000000FFU &&
               readLittleEndian32(alphaBmpBytes, 66) == 0xFF000000U,
               "alpha BMP writes canonical RGBA channel masks");
    }

    ege::PIMAGE png = ege::newimage();
    ege::PIMAGE pngSpecific = ege::newimage();
    ege::PIMAGE bmp = ege::newimage();
    ege::PIMAGE alphaPng = ege::newimage();
    ege::PIMAGE alphaBmp = ege::newimage();
    expect(ege::getimage(png, pngPath.c_str()) == ege::grOk, "PNG output can be loaded again");
    expect(ege::getimage_pngfile(pngSpecific, pngPath.c_str()) == ege::grOk,
           "getimage_pngfile loads PNG output through its compatibility entry point");
    expect(ege::getimage(bmp, bmpPath.c_str()) == ege::grOk, "BMP output can be loaded again");
    expect(ege::getimage(alphaPng, alphaPngPath.c_str()) == ege::grOk,
           "alpha-channel PNG output can be loaded again");
    expect(ege::getimage(alphaBmp, alphaBmpPath.c_str()) == ege::grOk,
           "alpha-channel BMP output can be loaded again");

    expect(ege::getwidth(png) == 7 && ege::getheight(png) == 5, "PNG preserves dimensions");
    expect(ege::getwidth(pngSpecific) == 7 && ege::getheight(pngSpecific) == 5,
           "getimage_pngfile preserves PNG dimensions");
    expect(ege::getwidth(bmp) == 7 && ege::getheight(bmp) == 5, "BMP preserves dimensions");
    expectPixel(png, 1, 0, ege::RED, "PNG preserves a top-row drawn pixel");
    expectPixel(png, 5, 4, ege::BLUE, "PNG preserves a bottom-row drawn pixel");
    expectPixel(bmp, 1, 0, ege::RED, "BMP preserves a top-row drawn pixel");
    expectPixel(bmp, 5, 4, ege::BLUE, "BMP preserves a bottom-row drawn pixel");
    expectPixel(png, 4, 1, ege::BLUE,
                "RGB PNG output preserves GDI pixels whose alpha byte is zero");
    expectPixel(bmp, 4, 1, ege::BLUE,
                "RGB BMP output preserves GDI pixels whose alpha byte is zero");

    const auto expectPremultipliedAlphaPixel = [](ege::PCIMAGE image, const std::string& format) {
        const ege::color_t pixel = ege::getpixel(3, 2, image);
        expect(EGEGET_A(pixel) == 128 &&
               EGEGET_R(pixel) >= 99 && EGEGET_R(pixel) <= 101 &&
               EGEGET_G(pixel) >= 49 && EGEGET_G(pixel) <= 51 &&
               EGEGET_B(pixel) >= 24 && EGEGET_B(pixel) <= 26,
               format + " preserves alpha and premultiplied RGB values");
    };
    expectPremultipliedAlphaPixel(alphaPng, "PNG");
    expectPremultipliedAlphaPixel(alphaBmp, "BMP");

    ege::delimage(alphaBmp);
    ege::delimage(alphaPng);
    ege::delimage(bmp);
    ege::delimage(pngSpecific);
    ege::delimage(png);
    ege::delimage(source);
    std::remove(pngPath.c_str());
    std::remove(bmpPath.c_str());
    std::remove(alphaPngPath.c_str());
    std::remove(alphaBmpPath.c_str());
    std::remove(genericPngPath.c_str());
    std::remove(genericBmpPath.c_str());
    std::remove(defaultPngPath.c_str());
#ifdef _WIN32
    _wremove(unicodeWidePath.c_str());
#else
    std::remove(unicodeUtf8Path.c_str());
#endif
}

void testAdditionalImageFileDecoders()
{
    const std::string jpegPath = tempPath(".jpg");
    const std::string gifPath = tempPath(".gif");
    const std::string tgaPath = tempPath(".tga");
    const std::string ppmPath = tempPath(".ppm");

    // A deterministic 2x2 JPEG containing the solid RGB color (200, 30, 20).
    // It was encoded once at quality 90 with 4:4:4 sampling; keeping the bytes
    // in the test avoids relying on an image encoder being installed at run time.
    static const char jpegFixture[] =
        "\xFF\xD8\xFF\xE0\x00\x10\x4A\x46\x49\x46\x00\x01\x01\x00\x00\x01"
        "\x00\x01\x00\x00\xFF\xDB\x00\x43\x00\x03\x02\x02\x03\x02\x02\x03"
        "\x03\x03\x03\x04\x03\x03\x04\x05\x08\x05\x05\x04\x04\x05\x0A\x07"
        "\x07\x06\x08\x0C\x0A\x0C\x0C\x0B\x0A\x0B\x0B\x0D\x0E\x12\x10\x0D"
        "\x0E\x11\x0E\x0B\x0B\x10\x16\x10\x11\x13\x14\x15\x15\x15\x0C\x0F"
        "\x17\x18\x16\x14\x18\x12\x14\x15\x14\xFF\xDB\x00\x43\x01\x03\x04"
        "\x04\x05\x04\x05\x09\x05\x05\x09\x14\x0D\x0B\x0D\x14\x14\x14\x14"
        "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14"
        "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14"
        "\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\x14\xFF\xC0"
        "\x00\x11\x08\x00\x02\x00\x02\x03\x01\x11\x00\x02\x11\x01\x03\x11"
        "\x01\xFF\xC4\x00\x14\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x08\xFF\xC4\x00\x14\x10\x01\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC4\x00"
        "\x15\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x07\x08\xFF\xC4\x00\x14\x11\x01\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xDA\x00\x0C\x03\x01"
        "\x00\x02\x11\x03\x11\x00\x3F\x00\x3F\x89\x17\x2B\xFF\xD9";
    static const unsigned char gifFixture[] = {
        0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x02, 0x00, 0x02, 0x00, 0x81, 0x00,
        0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x08,
        0x07, 0x00, 0x05, 0x04, 0x18, 0x00, 0x20, 0x20, 0x00, 0x3B
    };
    static const unsigned char tgaFixture[] = {
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x18, 0x20,
        0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
        0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF
    };
    static const unsigned char ppmFixture[] = {
        'P', '6', '\n', '2', ' ', '2', '\n', '2', '5', '5', '\n',
        0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00,
        0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
    };

    expect(writeFileBytes(jpegPath, jpegFixture, sizeof(jpegFixture) - 1),
           "JPEG decoder fixture can be written");
    expect(writeFileBytes(gifPath, gifFixture, sizeof(gifFixture)),
           "GIF decoder fixture can be written");
    expect(writeFileBytes(tgaPath, tgaFixture, sizeof(tgaFixture)),
           "TGA decoder fixture can be written");
    expect(writeFileBytes(ppmPath, ppmFixture, sizeof(ppmFixture)),
           "PPM decoder fixture can be written");

    ege::PIMAGE jpeg = ege::newimage();
    ege::PIMAGE gif = ege::newimage();
    ege::PIMAGE tga = ege::newimage();
    ege::PIMAGE ppm = ege::newimage();
    expect(ege::getimage(jpeg, jpegPath.c_str()) == ege::grOk,
           "getimage decodes a JPEG file");
    expect(ege::getimage(gif, gifPath.c_str()) == ege::grOk,
           "getimage decodes the first frame of a GIF file");
    expect(ege::getimage(tga, tgaPath.c_str()) == ege::grOk,
           "getimage decodes a top-down TGA file");
    expect(ege::getimage(ppm, ppmPath.c_str()) == ege::grOk,
           "getimage decodes a binary PPM file");

    expect(ege::getwidth(jpeg) == 2 && ege::getheight(jpeg) == 2,
           "JPEG decoding preserves dimensions");
    const ege::color_t jpegPixel = ege::getpixel(0, 0, jpeg);
    expect(EGEGET_R(jpegPixel) >= 190 && EGEGET_R(jpegPixel) <= 210 &&
           EGEGET_G(jpegPixel) >= 20 && EGEGET_G(jpegPixel) <= 40 &&
           EGEGET_B(jpegPixel) >= 10 && EGEGET_B(jpegPixel) <= 30,
           "JPEG decoding preserves the fixture color within lossy tolerance (actual RGB=" +
               std::to_string(EGEGET_R(jpegPixel)) + "," +
               std::to_string(EGEGET_G(jpegPixel)) + "," +
               std::to_string(EGEGET_B(jpegPixel)) + ")");

    expectPixel(gif, 0, 0, ege::RED, "GIF preserves its top-left palette pixel");
    expectPixel(gif, 1, 1, ege::WHITE, "GIF preserves its bottom-right palette pixel");
    expectPixel(tga, 1, 0, EGERGB(0, 255, 0),
                "TGA preserves its declared top-down orientation");
    expectPixel(tga, 0, 1, ege::BLUE, "TGA preserves its BGR channel order");
    expectPixel(ppm, 0, 0, ege::RED, "PPM preserves its top-left RGB pixel");
    expectPixel(ppm, 1, 1, ege::WHITE, "PPM preserves its bottom-right RGB pixel");

    ege::delimage(ppm);
    ege::delimage(tga);
    ege::delimage(gif);
    ege::delimage(jpeg);
    std::remove(ppmPath.c_str());
    std::remove(tgaPath.c_str());
    std::remove(gifPath.c_str());
    std::remove(jpegPath.c_str());
}

#ifdef _WIN32
void testResourceImageLoading()
{
    ege::PIMAGE narrowResource = ege::newimage();
    ege::PIMAGE wideResource = ege::newimage();
    ege::PIMAGE missingResource = ege::newimage();

    const bool narrowLoaded =
        ege::getimage(narrowResource, "PNG", "EGE_TEST_IMAGE") == ege::grOk;
    const bool wideLoaded =
        ege::getimage(wideResource, L"PNG", L"EGE_TEST_IMAGE") == ege::grOk;
    expect(narrowLoaded, "narrow resource getimage overload loads an embedded PNG");
    expect(wideLoaded, "wide resource getimage overload loads an embedded PNG");

    const bool narrowDimensions = narrowLoaded &&
        ege::getwidth(narrowResource) == 80 && ege::getheight(narrowResource) == 120;
    const bool wideDimensions = wideLoaded &&
        ege::getwidth(wideResource) == 80 && ege::getheight(wideResource) == 120;
    expect(narrowDimensions, "narrow resource getimage preserves embedded image dimensions");
    expect(wideDimensions, "wide resource getimage preserves embedded image dimensions");

    if (narrowDimensions) {
        expectPixel(narrowResource, 0, 0, ege::BLACK,
                    "resource getimage decodes a transparent corner as zero");
        expectPixel(narrowResource, 40, 60, EGERGB(114, 99, 96),
                    "resource getimage decodes an opaque fixture pixel");
    }
    if (narrowDimensions && wideDimensions) {
        const int pixelCount = ege::getwidth(narrowResource) * ege::getheight(narrowResource);
        const ege::color_t* narrowPixels = ege::getbuffer(narrowResource);
        const ege::color_t* widePixels = ege::getbuffer(wideResource);
        expect(std::equal(narrowPixels, narrowPixels + pixelCount, widePixels),
               "narrow and wide resource getimage overloads decode identical pixels");
    }

    expect(ege::getimage(missingResource, "PNG", "EGE_TEST_MISSING_IMAGE") != ege::grOk,
           "resource getimage reports a missing embedded resource");

    ege::delimage(missingResource);
    ege::delimage(wideResource);
    ege::delimage(narrowResource);
}
#endif

} // namespace

int main()
{
    ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
    ege::PIMAGE legacyPreInitSource = nullptr;
    ege::PIMAGE legacyPreInitDestination = nullptr;
#if defined(_WIN32) && defined(EGE_BUILD_OPENGL)
    const char* openGlMode = std::getenv("EGE_TEST_OPENGL");
    if (openGlMode != nullptr && openGlMode[0] == '1') {
        mode = static_cast<ege::initmode_flag>(mode | ege::INIT_OPENGL);
        // Images created before the OpenGL window exists retain the legacy
        // DIB backend. They exercise the supported mixed-backend fallback.
        legacyPreInitSource = ege::newimage(3, 2);
        legacyPreInitDestination = ege::newimage(3, 2);
    }
#endif
    ege::initgraph(64, 64, mode);
    if (!ege::getHWnd()) {
        std::cerr << "FAIL: unable to create the hidden graphics test context\n";
        shutdown_graphics_for_test();
        return EXIT_FAILURE;
    }

    testClearAndPixels();
    testBasicPrimitives();
    testFilledShapeOutlineCompatibility();
    testEnhancedFillAndCornerRadiusCompatibility();
    testEnhancedStrokeStateCompatibility();
    testFillPatterns();
    testUserLinePatternAndCaps();
    testCurvedLineStyles();
    testScreenFramebufferCapture();
    testScreenPutimageOverloads();
    testPrimitiveBatchRetention();
    testPolygonCoordinates();
    testViewportOriginAndClip();
    testBufferMutationFeedsImageTransfer();
    testConstAndScreenBufferSynchronization();
    testGetImageSourceClipping();
    testMixedBackendBufferSynchronization(legacyPreInitSource, legacyPreInitDestination);
    testImageTransfersHonorViewportOrigin();
    testImageTransfersHonorSourceViewportOrigin();
    testColorAndMathUtilities();
    testStateAndPixelUtilities();
    testLineAndFillStyles();
    testImageLifecycleCropAndStretch();
    testTransparencyAndAlphaBlend();
    testAlphaFormatsAndCombinedColorKey();
    testEnhancedAlphaSurfaceCompatibility();
    testEnhancedAlphaScreenCompatibility();
    testAlphaMaskDefaultsAndScaledSampling();
    testAlphaTransferOverloadsAndClipping();
    testImageRotationCoordinatesAndAspectRatio();
    testEnhancedImageTransform();
    testTextureAndEnhancedImageTransferOverloads();
    testEnhancedPathApi();
    testRasterOperations();
    testCurrentPositionAndAdditionalPrimitiveRoutes();
    testSurfaceFloodFillAndColorConversion();
    testTextRectangleAndBlur();
    testViewportClearAndWritingMode();
    testConcavePolygonAndRoundedRectangleCoverage();
    testEnhancedTransformAndGradientFallback();
    testRoundedShapesFloodFillAndFloatRoutes();
    testAdditionalPrimitiveEntryPoints();
    testAdditionalEnhancedEntryPoints();
    testArcAndPieAngleOrientation();
    testTextRendering();
    testFormattedScreenText();
    testWindowCaptionEncoding();
    testWindowAndPublicStateQueries();
    testLegacyPageSwap();
    testCompressionTimingAndRandomUtilities();
    testFontCompatibilityDetails();
    testPngAndBmpRoundTrip();
    testAdditionalImageFileDecoders();
#ifdef _WIN32
    testResourceImageLoading();
#endif

    ege::delimage(legacyPreInitDestination);
    ege::delimage(legacyPreInitSource);

    expect(shutdown_graphics_for_test(),
           "the graphics test window and UI thread shut down cleanly");

    if (failures != 0) {
        std::cerr << failures << " rendering correctness assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All rendering correctness assertions passed\n";
    return EXIT_SUCCESS;
}
