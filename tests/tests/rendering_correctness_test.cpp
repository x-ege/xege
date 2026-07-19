#include "ege.h"
#include "../test_shutdown.h"

#include <algorithm>
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

#ifdef _WIN32
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

void testStateAndPixelUtilities()
{
    ege::PIMAGE image = ege::newimage(10, 8);
    resetImage(image, ege::BLACK);

    ege::setlinecolor(ege::RED, image);
    ege::setfillcolor(ege::GREEN, image);
    ege::settextcolor(ege::YELLOW, image);
    ege::setbkcolor_f(ege::BLUE, image);
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

void testRasterOperations()
{
    ege::PIMAGE source = ege::newimage(2, 2);
    ege::PIMAGE destination = ege::newimage(2, 2);
    const ege::color_t sourceColor = EGEARGB(0xFF, 0x33, 0x66, 0xCC);
    const ege::color_t destinationColor = EGEARGB(0xFF, 0x55, 0xAA, 0x0F);
    resetImage(source, sourceColor);

    resetImage(destination, destinationColor);
    ege::putimage(destination, 0, 0, source, SRCINVERT);
    expectPixel(destination, 0, 0, sourceColor ^ destinationColor,
                "SRCINVERT applies the per-call putimage raster operation");

    resetImage(destination, destinationColor);
    ege::putimage(destination, 0, 0, source, SRCAND);
    expectPixel(destination, 0, 0, sourceColor & destinationColor,
                "SRCAND combines source and destination pixels");

    resetImage(destination, destinationColor);
    ege::putimage(destination, 0, 0, source, SRCPAINT);
    expectPixel(destination, 0, 0, sourceColor | destinationColor,
                "SRCPAINT combines source and destination pixels");

    resetImage(destination, destinationColor);
    ege::putimage(destination, 0, 0, source, NOTSRCCOPY);
    expectPixel(destination, 0, 0, ~sourceColor,
                "NOTSRCCOPY inverts the source pixel");

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
    ege::ege_line(0.0f, 0.0f, 8.0f, 0.0f, image);
    ege::ege_transform_reset(image);
    const PixelBounds transformedLine = boundsDifferentFrom(image, ege::BLACK);
    expect(transformedLine.valid && transformedLine.left >= 9 && transformedLine.right <= 19 &&
           transformedLine.top >= 5 && transformedLine.bottom <= 7,
           "enhanced drawing applies the native transform matrix");
    expectPixel(image, 4, 0, ege::BLACK, "enhanced transform does not leave geometry at its untransformed position");

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

    expect(ege::textwidth("Test", image) > 0, "textwidth uses the native font backend");
    expect(ege::textheight("Test", image) > 0, "textheight uses the native font backend");
    ege::outtextxy(2, 2, "Test", image);
    expect(countPixelsDifferentFrom(image, ege::BLACK) > 0, "outtextxy renders glyph pixels");

    ege::delimage(image);
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
    ege::PIMAGE bmp = ege::newimage();
    ege::PIMAGE alphaPng = ege::newimage();
    ege::PIMAGE alphaBmp = ege::newimage();
    expect(ege::getimage(png, pngPath.c_str()) == ege::grOk, "PNG output can be loaded again");
    expect(ege::getimage(bmp, bmpPath.c_str()) == ege::grOk, "BMP output can be loaded again");
    expect(ege::getimage(alphaPng, alphaPngPath.c_str()) == ege::grOk,
           "alpha-channel PNG output can be loaded again");
    expect(ege::getimage(alphaBmp, alphaBmpPath.c_str()) == ege::grOk,
           "alpha-channel BMP output can be loaded again");

    expect(ege::getwidth(png) == 7 && ege::getheight(png) == 5, "PNG preserves dimensions");
    expect(ege::getwidth(bmp) == 7 && ege::getheight(bmp) == 5, "BMP preserves dimensions");
    expectPixel(png, 1, 0, ege::RED, "PNG preserves a top-row drawn pixel");
    expectPixel(png, 5, 4, ege::BLUE, "PNG preserves a bottom-row drawn pixel");
    expectPixel(bmp, 1, 0, ege::RED, "BMP preserves a top-row drawn pixel");
    expectPixel(bmp, 5, 4, ege::BLUE, "BMP preserves a bottom-row drawn pixel");

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

} // namespace

int main()
{
    ege::initmode_flag mode = static_cast<ege::initmode_flag>(
        ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
#if defined(_WIN32) && defined(EGE_BUILD_OPENGL)
    const char* openGlMode = std::getenv("EGE_TEST_OPENGL");
    if (openGlMode != nullptr && openGlMode[0] == '1') {
        mode = static_cast<ege::initmode_flag>(mode | ege::INIT_OPENGL);
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
    testFillPatterns();
    testUserLinePatternAndCaps();
    testCurvedLineStyles();
    testScreenFramebufferCapture();
    testPrimitiveBatchRetention();
    testPolygonCoordinates();
    testViewportOriginAndClip();
    testBufferMutationFeedsImageTransfer();
    testImageTransfersHonorViewportOrigin();
    testStateAndPixelUtilities();
    testLineAndFillStyles();
    testImageLifecycleCropAndStretch();
    testTransparencyAndAlphaBlend();
    testAlphaFormatsAndCombinedColorKey();
    testAlphaMaskDefaultsAndScaledSampling();
    testImageRotationCoordinatesAndAspectRatio();
    testEnhancedImageTransform();
    testRasterOperations();
    testCurrentPositionAndAdditionalPrimitiveRoutes();
    testSurfaceFloodFillAndColorConversion();
    testTextRectangleAndBlur();
    testViewportClearAndWritingMode();
    testConcavePolygonAndRoundedRectangleCoverage();
    testEnhancedTransformAndGradientFallback();
    testRoundedShapesFloodFillAndFloatRoutes();
    testArcAndPieAngleOrientation();
    testTextRendering();
    testFontCompatibilityDetails();
    testPngAndBmpRoundTrip();

    expect(shutdown_graphics_for_test(),
           "the graphics test window and UI thread shut down cleanly");

    if (failures != 0) {
        std::cerr << failures << " rendering correctness assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All rendering correctness assertions passed\n";
    return EXIT_SUCCESS;
}
