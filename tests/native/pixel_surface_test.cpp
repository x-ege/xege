#include "backend/interface/PixelSurface.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

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

template <typename Exception, typename Callable> void checkThrows(Callable callable) {
    try {
        callable();
        CHECK(false);
    } catch (const Exception&) {
    } catch (...) {
        CHECK(false);
    }
}

void testOddWidthAndStablePointer() {
    ege::backend::PixelSurface surface(641, 3);
    CHECK(surface.width() == 641);
    CHECK(surface.height() == 3);
    CHECK(surface.strideBytes() == 641 * sizeof(ege::backend::PixelSurface::Pixel));
    CHECK(surface.byteCount() == surface.strideBytes() * 3);

    auto* const original = surface.data();
    CHECK(original != nullptr);
    CHECK(surface.row(1) == original + 641);
    CHECK(surface.row(2) == original + 1282);

    surface.clear(0xFF010203U);
    CHECK(surface.data() == original);
    CHECK(surface.getPixel(640, 2) == 0xFF010203U);

    surface.setPixel(640, 2, 0x80402010U);
    CHECK(surface.data() == original);
    CHECK(surface.getPixel(640, 2) == 0x80402010U);
}

void testByteLayoutAndPremultiplication() {
    ege::backend::PixelSurface surface(1, 1);
    surface.setPixel(0, 0, 0x80402010U);

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(surface.data());
    CHECK(bytes[0] == 0x10U); // blue
    CHECK(bytes[1] == 0x20U); // green
    CHECK(bytes[2] == 0x40U); // red
    CHECK(bytes[3] == 0x80U); // alpha

    const auto premultiplied = ege::backend::PixelSurface::makePremultipliedPixel(200, 100, 50, 128);
    CHECK(premultiplied == 0x80643219U);
}

void testBoundsAndOverflowSafety() {
    using Surface = ege::backend::PixelSurface;

    checkThrows<std::invalid_argument>([] { Surface surface(0, 1); });
    checkThrows<std::invalid_argument>([] { Surface surface(1, 0); });
    checkThrows<std::length_error>([] { Surface surface(std::numeric_limits<std::size_t>::max() / sizeof(Surface::Pixel) + 1, 1); });
    checkThrows<std::length_error>([] { Surface surface(std::numeric_limits<std::size_t>::max() / sizeof(Surface::Pixel), 2); });

    Surface surface(2, 2);
    checkThrows<std::out_of_range>([&surface] { (void)surface.row(2); });
    checkThrows<std::out_of_range>([&surface] { (void)surface.getPixel(2, 0); });
    checkThrows<std::out_of_range>([&surface] { surface.setPixel(0, 2, 0); });
}

} // namespace

int runPixelSurfaceTests() {
    testOddWidthAndStablePointer();
    testByteLayoutAndPremultiplication();
    testBoundsAndOverflowSafety();

    if (failures != 0) {
        std::cerr << failures << " PixelSurface check(s) failed\n";
        return 1;
    }
    std::cout << "PixelSurface checks passed\n";
    return 0;
}

} // namespace ege

int main() {
    return ege::runPixelSurfaceTests();
}
