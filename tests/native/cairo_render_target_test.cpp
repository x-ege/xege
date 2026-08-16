#include "backend/linux/CairoRenderTarget.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace
{
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::cerr << __FILE__ << ':' << __LINE__ \
    << ": check failed: " #value << '\n'; ++failures; } } while (false)

using ege::backend::CairoRenderTarget;

void testSurfaceAndRasterOperations()
{
    CairoRenderTarget target(641, 3);
    ege::color_t* stable = target.getPixelBuffer();
    target.clear(0xFF010203U);
    target.putPixel(640, 2, 0xFF112233U);
    CHECK(target.valid());
    CHECK(target.getPixelBuffer() == stable);
    CHECK(target.getPixel(640, 2) == 0xFF112233U);
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
    CHECK(std::find(target.getPixelBuffer(), target.getPixelBuffer() + 643 * 4,
        0xFFABCDEFU) == target.getPixelBuffer() + 643 * 4);
}

void testPrimitivesTransfersAndAlpha()
{
    CairoRenderTarget source(2, 2);
    source.getPixelBuffer()[0] = 0xFFFF0000U;
    source.getPixelBuffer()[1] = 0xFF00FF00U;
    source.getPixelBuffer()[2] = 0xFF0000FFU;
    source.getPixelBuffer()[3] = 0xFFFFFFFFU;

    CairoRenderTarget target(48, 48);
    target.clear(0);
    target.blitStretch(0, 0, 4, 4, &source, 0, 0, 2, 2);
    CHECK(target.getPixel(0, 0) == 0xFFFF0000U);
    CHECK(target.getPixel(3, 3) == 0xFFFFFFFFU);

    target.clear(0);
    target.setLineColor(0xFFFFFFFFU);
    target.setFillStyle(ege::FILL_SOLID, 0xFF00FF00U);
    target.drawLineF(1.5f, 1.5f, 20.5f, 1.5f);
    target.fillRect(2, 4, 8, 6);
    target.fillCircle(20, 20, 4);
    const int triangle[] = {30, 3, 44, 3, 37, 15};
    target.fillPolygon(triangle, 3);
    target.flush();
    CHECK(target.getPixel(8, 1) == 0xFFFFFFFFU);
    CHECK(target.getPixel(5, 6) == 0xFF00FF00U);
    CHECK(target.getPixel(20, 20) == 0xFF00FF00U);
    CHECK(target.getPixel(37, 7) == 0xFF00FF00U);

    CairoRenderTarget alphaSource(1, 1);
    alphaSource.getPixelBuffer()[0] = 0x80800000U;
    target.clear(0xFF000000U);
    target.withAlpha(0, 0, 1, 1, &alphaSource, 0, 0, 1, 1, false);
    CHECK(target.getPixel(0, 0) == 0xFF800000U);

    target.clear(0);
    target.putPixel(20, 20, 0xFFFFFFFFU);
    target.filterBlur(18, 18, 5, 5, 1.0f);
    CHECK(target.getPixel(20, 20) != 0xFFFFFFFFU);
    CHECK(target.getPixel(19, 20) != 0);
}

void testTextAndExternalUpdates()
{
    CairoRenderTarget target(320, 100);
    const ege::color_t rows[] = {
        0xFF010203U, 0xFF040506U, 0xDEADBEEFU,
        0xFF070809U, 0xFF0A0B0CU, 0xCAFEBABEU};
    CHECK(target.updatePixelBuffer(1, 1, 2, 2, rows,
        3 * static_cast<int>(sizeof(ege::color_t))));
    CHECK(target.getPixel(2, 2) == 0xFF0A0B0CU);
    CHECK(!target.updatePixelBuffer(319, 99, 2, 2, rows, 8));

    target.clear(0);
    target.setFont(24, 0, "sans", 0, 0, 700, false, false, false);
    float utf8Width = 0, utf8Height = 0, wideWidth = 0, wideHeight = 0;
    target.measureText("Hello, world", &utf8Width, &utf8Height);
    target.measureText(L"Hello, world", &wideWidth, &wideHeight);
    CHECK(utf8Width > 20 && utf8Height > 10);
    CHECK(std::abs(utf8Width - wideWidth) < 1 && std::abs(utf8Height - wideHeight) < 1);
    target.setTextColor(0xFFFFFFFFU);
    target.drawText(4, 4, "Native UTF-8");
    target.drawText(4, 40, L"Linux Cairo");
    target.flush();
    CHECK(std::count_if(target.getPixelBuffer(), target.getPixelBuffer() + 320 * 100,
        [](ege::color_t pixel) { return pixel != 0; }) > 50);
}
}

int main()
{
    testSurfaceAndRasterOperations();
    testPrimitivesTransfersAndAlpha();
    testTextAndExternalUpdates();
    if (failures) {
        std::cerr << failures << " CairoRenderTarget check(s) failed\n";
        return 1;
    }
    std::cout << "CairoRenderTarget checks passed\n";
    return 0;
}
