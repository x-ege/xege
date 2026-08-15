#include <graphics.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace
{

int fail(const char* message)
{
    std::cerr << "EGE native API smoke failed: " << message << '\n';
    ege::closegraph();
    return 1;
}

} // namespace

int main()
{
    // Headless mode must be selected before initgraph. It creates the normal
    // global EGE drawing surface without constructing NSApplication/NSWindow.
    setenv("EGE_HEADLESS", "1", 1);

    constexpr int width  = 64;
    constexpr int height = 48;
    ege::initgraph(width, height, ege::INIT_NOBORDER | ege::INIT_RENDERMANUAL);
    if (ege::getHWnd() != nullptr) {
        return fail("headless initgraph unexpectedly created a native window");
    }
    if (ege::getwidth() != width || ege::getheight() != height) {
        return fail("headless canvas size did not reach the public API");
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

    const std::filesystem::path artifactDir(EGE_TEST_ARTIFACT_DIR);
    std::filesystem::create_directories(artifactDir);
    const std::filesystem::path artifact = artifactDir / "ege-api-headless.png";
    if (ege::savepng(static_cast<ege::PCIMAGE>(nullptr), artifact.string().c_str(), false)
        != ege::grOk) {
        return fail("headless canvas could not be written to PNG");
    }
    if (!std::filesystem::is_regular_file(artifact)
        || std::filesystem::file_size(artifact) <= 64) {
        return fail("headless PNG artifact is missing or empty");
    }

    ege::PIMAGE decoded = ege::newimage(1, 1);
    const bool decodedMatches = decoded != nullptr
        && ege::getimage(decoded, artifact.string().c_str()) == ege::grOk
        && ege::getwidth(decoded) == width
        && ege::getheight(decoded) == height
        && ege::getpixel(3, 4, decoded) == direct
        && ege::getpixel(9, 7, decoded) == bufferEdit
        && ege::getpixel(25, 15, decoded) == fill;
    ege::delimage(decoded);
    if (!decodedMatches) {
        return fail("saved headless PNG did not preserve the rendered pixels");
    }

    ege::closegraph();
    std::cout << "EGE headless API smoke passed: " << artifact << '\n';
    return 0;
}
