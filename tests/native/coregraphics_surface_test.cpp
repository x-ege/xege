#include "backend/macos/CoreGraphicsSurface.h"

#include <CoreGraphics/CoreGraphics.h>
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

using Pixel = ege::backend::PixelSurface::Pixel;
using Surface = ege::backend::PixelSurface;

void testDirectBufferAndTopDownCoordinates() {
    Surface surface(641, 5);
    ege::backend::CoreGraphicsSurface graphics(surface);

    CHECK(CGBitmapContextGetData(graphics.context()) == surface.data());
    CHECK(CGBitmapContextGetBytesPerRow(graphics.context()) == surface.strideBytes());
    CHECK(CGBitmapContextGetWidth(graphics.context()) == 641);
    CHECK(CGBitmapContextGetHeight(graphics.context()) == 5);

    graphics.clear(0x00000000U);
    Pixel* const stablePointer = surface.data();
    surface.setPixel(640, 4, 0xFF112233U);

    graphics.setAntialiasing(false);
    graphics.setBlendMode(kCGBlendModeCopy);
    graphics.fillRect(CGRectMake(0, 0, 1, 1), 0xFFFF0000U);
    graphics.flush();

    CHECK(surface.data() == stablePointer);
    CHECK(surface.getPixel(0, 0) == 0xFFFF0000U);
    CHECK(surface.getPixel(0, 4) == 0x00000000U);
    CHECK(surface.getPixel(640, 4) == 0xFF112233U);

    // A CPU write through a retained pointer remains immediately visible after
    // Core Graphics drawing and requires no reacquire/synchronization call.
    stablePointer[2 * surface.width() + 320] = 0xFFABCDEFU;
    CHECK(surface.getPixel(320, 2) == 0xFFABCDEFU);
}

void testPremultipliedAlphaLayout() {
    Surface surface(3, 2);
    ege::backend::CoreGraphicsSurface graphics(surface);
    const Pixel semiTransparent = Surface::makePremultipliedPixel(200, 100, 50, 128);

    graphics.clear(0);
    graphics.setAntialiasing(false);
    graphics.setBlendMode(kCGBlendModeCopy);
    graphics.fillRect(CGRectMake(1, 0, 1, 1), semiTransparent);
    graphics.flush();

    const Pixel actual = surface.getPixel(1, 0);
    CHECK(actual == semiTransparent);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(surface.row(0) + 1);
    CHECK(bytes[0] == 25U);
    CHECK(bytes[1] == 50U);
    CHECK(bytes[2] == 100U);
    CHECK(bytes[3] == 128U);
}

void testDrawingPrimitivesAndPath() {
    Surface surface(24, 24);
    ege::backend::CoreGraphicsSurface graphics(surface);
    graphics.clear(0);
    graphics.setAntialiasing(false);
    graphics.setBlendMode(kCGBlendModeCopy);

    // A one-pixel Core Graphics stroke is centered on its path, so use
    // half-pixel coordinates to cover exactly one device row.
    graphics.drawLine(CGPointMake(1.5, 1.5), CGPointMake(10.5, 1.5), 1, 0xFFFFFFFFU);
    graphics.strokeRect(CGRectMake(2, 3, 6, 5), 1, 0xFF00FF00U);
    graphics.fillEllipse(CGRectMake(10, 2, 8, 8), 0xFF0000FFU);
    graphics.strokeEllipse(CGRectMake(10, 11, 8, 8), 1, 0xFFFF0000U);

    const CGPoint triangle[] = { CGPointMake(2, 12), CGPointMake(8, 12), CGPointMake(5, 18) };
    graphics.fillPath(triangle, 3, 0xFFFFFF00U);
    graphics.flush();

    CHECK(surface.getPixel(5, 1) == 0xFFFFFFFFU);
    CHECK(surface.getPixel(2, 4) == 0xFF00FF00U);
    CHECK(surface.getPixel(14, 6) == 0xFF0000FFU);
    CHECK(surface.getPixel(5, 14) == 0xFFFFFF00U);

    // The raw CGContext entry point is intentionally available for future EGE
    // path/clip/transform operations without adding another backing store.
    CHECK(graphics.context() != nullptr);
}

} // namespace

int runCoreGraphicsSurfaceTests() {
    testDirectBufferAndTopDownCoordinates();
    testPremultipliedAlphaLayout();
    testDrawingPrimitivesAndPath();

    if (failures != 0) {
        std::cerr << failures << " CoreGraphicsSurface check(s) failed\n";
        return 1;
    }
    std::cout << "CoreGraphicsSurface checks passed\n";
    return 0;
}

} // namespace ege

int main() {
    return ege::runCoreGraphicsSurfaceTests();
}
