#include <graphics.h>

#include <CoreGraphics/CoreGraphics.h>

#include <cstddef>
#include <cstdint>
#include <iostream>

namespace
{

bool hasWindowServerSession()
{
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session == nullptr) {
        return false;
    }
    CFRelease(session);
    return true;
}

int fail(const char* message)
{
    std::cerr << "EGE native API smoke failed: " << message << '\n';
    ege::closegraph();
    return 1;
}

} // namespace

int main()
{
    if (!hasWindowServerSession()) {
        std::cout << "EGE native API smoke skipped: no WindowServer session\n";
        return 77;
    }

    constexpr int width  = 64;
    constexpr int height = 48;
    ege::initgraph(width, height, ege::INIT_NOBORDER | ege::INIT_RENDERMANUAL);
    if (ege::getwidth() != width || ege::getheight() != height) {
        return fail("AppKit window size did not reach the public API");
    }

    const ege::color_t background = EGERGB(4, 8, 12);
    const ege::color_t direct     = EGERGB(17, 34, 51);
    const ege::color_t bufferEdit = EGERGB(68, 85, 102);
    ege::setbkcolor(background);
    ege::cleardevice();
    ege::putpixel(3, 4, direct);

    ege::color_t* pixels = ege::getbuffer(static_cast<ege::PIMAGE>(nullptr));
    if (pixels == nullptr || pixels[4 * width + 3] != direct || ege::getpixel(3, 4) != direct) {
        return fail("putpixel/getpixel/getbuffer do not share one CPU surface");
    }

    pixels[7 * width + 9] = bufferEdit;
    if (ege::getpixel(9, 7) != bufferEdit) {
        return fail("a direct getbuffer write was not immediately observable");
    }

    const ege::color_t fill = EGERGB(120, 60, 30);
    ege::setfillcolor(fill);
    ege::bar(20, 10, 30, 20);
    if (ege::getpixel(25, 15) != fill) {
        return fail("Core Graphics primitive drawing did not update the CPU surface");
    }

    ege::PIMAGE image = ege::newimage(13, 7);
    if (image == nullptr) {
        return fail("newimage failed");
    }
    ege::putpixel(5, 2, direct, image);
    const bool offscreenMatches = ege::getpixel(5, 2, image) == direct
        && ege::getbuffer(image)[2 * 13 + 5] == direct;
    ege::delimage(image);
    if (!offscreenMatches) {
        return fail("offscreen IMAGE is not CPU-authoritative");
    }

    ege::closegraph();
    std::cout << "EGE native API smoke passed\n";
    return 0;
}
