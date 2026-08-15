#include "backend/macos/CoreGraphicsRenderTarget.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace ege {
namespace {

int failures = 0;

#define CHECK(condition)                                                                       \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " #condition << '\n'; \
            ++failures;                                                                        \
        }                                                                                      \
    } while (false)

using ege::backend::CoreGraphicsRenderTarget;

void testBufferResizeRopAndViewport() {
    CoreGraphicsRenderTarget target(641, 3);
    ege::color_t* const stable = target.getPixelBuffer();
    target.clear(0xFF010203U);
    target.putPixel(640, 2, 0xFF112233U);
    target.flush();
    CHECK(target.valid() && target.getPixelBuffer() == stable);
    CHECK(target.getPixelBufferForWrite(0, 0, 641, 3) == stable);
    CHECK(stable[2 * 641 + 640] == 0xFF112233U);
    CHECK(target.resize(643, 4, true));
    CHECK(target.getPixel(640, 2) == 0xFF112233U);
    CHECK(!target.resize(0, 4));

    target.setRasterOp(ege::ROP_COPY);
    target.putPixel(1, 1, 0x12345678U);
    target.setRasterOp(ege::ROP_XOR);
    target.putPixel(1, 1, 0x00FF00FFU);
    CHECK(target.getPixel(1, 1) == (0x12345678U ^ 0x00FF00FFU));

    target.setRasterOp(ege::ROP_COPY);
    target.setViewport(2, 1, 6, 4, true);
    target.putPixel(0, 0, 0xFFFFFFFFU);
    CHECK(target.getPixelBuffer()[1 * 643 + 2] == 0xFFFFFFFFU);
    target.putPixel(5, 0, 0xFFABCDEFU);
    CHECK(std::find(target.getPixelBuffer(), target.getPixelBuffer() + 643 * 4, 0xFFABCDEFU) == target.getPixelBuffer() + 643 * 4);

    target.clear(0xFF010203U);
    target.setBkColor(0xFF556677U);
    target.setViewport(-8, -7, -2, -1, true);
    target.clearViewport();
    CHECK(target.getPixelBuffer()[0] == 0xFF010203U);
    target.setViewport(640, 2, 700, 9, true);
    target.clearViewport();
    CHECK(target.getPixelBuffer()[3 * 643 + 642] == 0xFF556677U);
}

void testTransferAlphaBlurAndPrimitives() {
    CoreGraphicsRenderTarget source(2, 2);
    source.getPixelBuffer()[0] = 0xFFFF0000U;
    source.getPixelBuffer()[1] = 0xFF00FF00U;
    source.getPixelBuffer()[2] = 0xFF0000FFU;
    source.getPixelBuffer()[3] = 0xFFFFFFFFU;

    CoreGraphicsRenderTarget target(24, 24);
    target.clear(0);
    target.blitStretch(0, 0, 4, 4, &source, 0, 0, 2, 2);
    CHECK(target.getPixel(0, 0) == 0xFFFF0000U);
    CHECK(target.getPixel(3, 3) == 0xFFFFFFFFU);

    for (int x = 0; x < 6; ++x) {
        target.getPixelBuffer()[x] = 0xFF000000U | static_cast<ege::color_t>(x);
    }
    target.blit(1, 0, &target, 0, 0, 5, 1);
    for (int x = 1; x < 6; ++x) {
        CHECK(target.getPixel(x, 0) == (0xFF000000U | static_cast<ege::color_t>(x - 1)));
    }

    CoreGraphicsRenderTarget alphaSource(2, 1);
    alphaSource.getPixelBuffer()[0] = 0x80800000U;
    alphaSource.getPixelBuffer()[1] = 0xFF00FF00U;
    target.clear(0xFF000000U);
    target.withAlpha(0, 0, 1, 1, &alphaSource, 0, 0, 1, 1, false);
    CHECK(target.getPixel(0, 0) == 0xFF800000U);
    target.alphaTransparent(1, 0, &alphaSource, 1, 0, 1, 1, 0xFF00FF00U, 255);
    CHECK(target.getPixel(1, 0) == 0xFF000000U);

    target.clear(0);
    target.putPixel(12, 12, 0xFFFFFFFFU);
    target.filterBlur(10, 10, 5, 5, 1.0f);
    CHECK(target.getPixel(12, 12) != 0xFFFFFFFFU && target.getPixel(11, 12) != 0);

    target.clear(0);
    target.setLineColor(0xFFFFFFFFU);
    target.setFillStyle(ege::FILL_SOLID, 0xFF00FF00U);
    target.drawLineF(1.5f, 1.5f, 10.5f, 1.5f);
    target.fillRect(2, 3, 5, 4);
    target.fillCircle(12, 12, 3);
    const int triangle[] = { 16, 3, 22, 3, 19, 9 };
    target.fillPolygon(triangle, 3);
    target.flush();
    CHECK(target.getPixel(5, 1) == 0xFFFFFFFFU);
    CHECK(target.getPixel(4, 4) == 0xFF00FF00U);
    CHECK(target.getPixel(12, 12) == 0xFF00FF00U);
    CHECK(target.getPixel(19, 5) == 0xFF00FF00U);
}

void testStraightStateColorsBecomePremultipliedPixels() {
    constexpr ege::color_t straight = 0x80201008U;
    constexpr ege::color_t premultiplied = 0x80100804U;
    CoreGraphicsRenderTarget target(96, 48);

    // Public drawing state is straight ARGB, while the backing surface is
    // premultiplied.  Low RGB values catch accidental unpremultiplication.
    target.clear(straight);
    CHECK(target.getPixel(0, 0) == premultiplied);

    target.clear(0);
    target.setFillStyle(ege::FILL_SOLID, straight);
    target.fillRect(2, 2, 8, 8);
    CHECK(target.getPixel(4, 4) == premultiplied);

    target.clear(0);
    target.setBkColor(straight);
    target.setViewport(2, 2, 10, 10, true);
    target.clearViewport();
    CHECK(target.getPixelBuffer()[4 * target.getWidth() + 4] == premultiplied);

    target.clear(0xFF000000U);
    target.setViewport(0, 0, target.getWidth(), target.getHeight(), true);
    target.setFillStyle(ege::FILL_SOLID, straight);
    target.floodFillSurface(4, 4, 0xFF000000U);
    CHECK(target.getPixel(4, 4) == premultiplied);

    target.clear(0);
    target.setLineColor(straight);
    target.setLineWidth(1.0f);
    target.drawLineF(2.5f, 2.5f, 20.5f, 2.5f);
    CHECK(target.getPixel(8, 2) == premultiplied);

    target.clear(0);
    target.setLineWidth(2.0f);
    target.drawRoundRect(4, 4, 24, 16, 8, 8);
    CHECK(std::find(target.getPixelBuffer(),
        target.getPixelBuffer() + target.getWidth() * target.getHeight(),
        premultiplied) != target.getPixelBuffer() + target.getWidth() * target.getHeight());

    target.clear(0);
    target.drawEllipse(4, 4, 0, 180, 24, 16);
    CHECK(std::find(target.getPixelBuffer(),
        target.getPixelBuffer() + target.getWidth() * target.getHeight(),
        premultiplied) != target.getPixelBuffer() + target.getWidth() * target.getHeight());

    target.clear(0);
    target.setFont(24, 0, "Helvetica", 0, 0, 400, false, false, false);
    target.setTextColor(straight);
    target.drawText(2, 2, "Color");
    target.flush();
    bool foundTextPixel = false;
    for (int index = 0; index < target.getWidth() * target.getHeight(); ++index) {
        const ege::color_t pixel = target.getPixelBuffer()[index];
        if ((pixel >> 24U) == 0) continue;
        foundTextPixel = true;
        CHECK(((pixel >> 16U) & 0xFFU) <= 17U);
        CHECK(((pixel >> 8U) & 0xFFU) <= 9U);
        CHECK((pixel & 0xFFU) <= 5U);
    }
    CHECK(foundTextPixel);
}

void testUpdateRotateAndText() {
    CoreGraphicsRenderTarget target(320, 100);
    const ege::color_t rows[] = { 0xFF010203U, 0xFF040506U, 0xDEADBEEFU, 0xFF070809U, 0xFF0A0B0CU, 0xCAFEBABEU };
    CHECK(target.updatePixelBuffer(1, 1, 2, 2, rows, 3 * static_cast<int>(sizeof(ege::color_t))));
    CHECK(target.getPixel(2, 2) == 0xFF0A0B0CU);
    CHECK(!target.updatePixelBuffer(319, 99, 2, 2, rows, 8));

    CoreGraphicsRenderTarget source(2, 2);
    source.clear(0xFFFFFFFFU);
    target.rotateBlend(8, 8, 2, 2, &source, 0, 0, 2, 2, 0.0f, 1.0f, 1.0f, false, -1, false);
    CHECK(target.getPixel(7, 7) == 0xFFFFFFFFU);

    target.clear(0);
    target.setFont(24, 0, "Helvetica", 0, 0, 700, false, false, false);
    float utf8Width = 0.0f, utf8Height = 0.0f, wideWidth = 0.0f, wideHeight = 0.0f;
    target.measureText("Hello, \xE4\xB8\x96\xE7\x95\x8C", &utf8Width, &utf8Height);
    target.measureText(L"Hello, 世界", &wideWidth, &wideHeight);
    CHECK(utf8Width > 20.0f && utf8Height > 10.0f);
    CHECK(std::abs(utf8Width - wideWidth) < 1.0f && std::abs(utf8Height - wideHeight) < 1.0f);
    target.setFont(24, 0, "EGE Missing Font 0123456789", 0, 0, 400, false, false, false);
    CHECK(target.getTextWidth("font fallback") > 10);
    target.setTextColor(0xFFFFFFFFU);
    target.drawText(4, 4, "Native UTF-8");
    target.drawText(4, 40, L"原生文字");
    target.flush();
    CHECK(std::count_if(target.getPixelBuffer(), target.getPixelBuffer() + 320 * 100, [](ege::color_t pixel) { return pixel != 0; }) > 50);
}

} // namespace

int runCoreGraphicsRenderTargetTests() {
    testBufferResizeRopAndViewport();
    testTransferAlphaBlurAndPrimitives();
    testStraightStateColorsBecomePremultipliedPixels();
    testUpdateRotateAndText();
    if (failures != 0) {
        std::cerr << failures << " CoreGraphicsRenderTarget check(s) failed\n";
        return 1;
    }
    std::cout << "CoreGraphicsRenderTarget checks passed\n";
    return 0;
}

} // namespace ege

int main() {
    return ege::runCoreGraphicsRenderTargetTests();
}
