#include "test_support.h"

#include <array>
#include <filesystem>
#include <iostream>

namespace {

constexpr int kWidth = 128;
constexpr int kHeight = 96;
const ege::color_t kBackground = EGERGB(7, 11, 19);
const ege::color_t kLine = EGERGB(231, 76, 60);
const ege::color_t kFill = EGERGB(46, 204, 113);
const ege::color_t kBlue = EGERGB(52, 152, 219);

bool isLinePixel(ege::color_t pixel)
{
    return EGEGET_R(pixel) > 140 && EGEGET_G(pixel) < 110 && EGEGET_B(pixel) < 100;
}

int countLinePixels(ege::PCIMAGE image)
{
    const ege::color_t* pixels = ege::getbuffer(image);
    int count = 0;
    for (int index = 0; index < kWidth * kHeight; ++index) {
        count += isLinePixel(pixels[index]);
    }
    return count;
}

void drawReferenceScene(ege::PIMAGE image)
{
    ege::setbkcolor(kBackground, image);
    ege::cleardevice(image);

    // The centre of this diagonal is intentionally sampled below.  It is the
    // user-facing draw -> save -> read-back visual contract.
    ege::setlinecolor(kLine, image);
    ege::setlinestyle(PS_SOLID, 0, 1, image);
    ege::line(4, 4, 123, 84, image);

    ege::setfillcolor(kFill, image);
    ege::bar(8, 60, 30, 83, image);
    ege::setfillcolor(kBlue, image);
    ege::fillcircle(90, 30, 12, image);

    const int triangle[] = { 46, 55, 68, 55, 57, 76 };
    ege::setfillcolor(EGERGB(241, 196, 15), image);
    ege::fillpoly(3, triangle, image);
}

void testRasterAndPixelAccess()
{
    ege_test::Image image(kWidth, kHeight);
    EGE_CHECK(image.value != nullptr);
    drawReferenceScene(image.value);

    EGE_CHECK(ege::getpixel(8, 60, image.value) == kFill);
    EGE_CHECK(ege::getpixel(90, 30, image.value) == kBlue);
    EGE_CHECK(ege::getpixel(57, 62, image.value) == EGERGB(241, 196, 15));
    EGE_CHECK(countLinePixels(image.value) > 80);

    ege::color_t* pixels = ege::getbuffer(image.value, ege::IMAGE_BUFFER_READ_WRITE);
    EGE_CHECK(pixels != nullptr);
    pixels[2 * kWidth + 2] = kBlue;
    EGE_CHECK(ege::getpixel(2, 2, image.value) == kBlue);
    EGE_CHECK(ege::updatebuffer(image.value, 2, 2, 1, 1, pixels + 2 * kWidth + 2) == ege::grOk);

    // Viewport clipping exercises the CPU target coordinate translation on a
    // clean surface so its samples cannot overlap the reference scene.
    ege_test::Image viewportImage(kWidth, kHeight);
    ege::setbkcolor(kBackground, viewportImage.value);
    ege::cleardevice(viewportImage.value);
    ege::setviewport(100, 70, 110, 80, 1, viewportImage.value);
    ege::setfillcolor(kLine, viewportImage.value);
    ege::bar(0, 0, 50, 50, viewportImage.value);
    EGE_CHECK(ege::getpixel(5, 5, viewportImage.value) == kLine);
    ege::setviewport(0, 0, kWidth, kHeight, 1, viewportImage.value);
    EGE_CHECK(ege::getpixel(99, 69, viewportImage.value) == kBackground);
}

void testCopyAndRasterOps()
{
    ege_test::Image source(4, 4);
    ege_test::Image target(12, 12);
    EGE_CHECK(source.value != nullptr && target.value != nullptr);
    ege::setbkcolor(kBackground, source.value);
    ege::cleardevice(source.value);
    ege::putpixel(1, 1, kLine, source.value);
    ege::setbkcolor(kBackground, target.value);
    ege::cleardevice(target.value);

    ege::putimage(target.value, 3, 4, source.value, SRCCOPY);
    EGE_CHECK(ege::getpixel(4, 5, target.value) == kLine);
    ege::putimage(target.value, 3, 4, source.value, SRCINVERT);
    EGE_CHECK(ege::getpixel(4, 5, target.value) == (kLine ^ kLine));

    ege::putimage(target.value, 0, 0, 8, 8, source.value, 0, 0, 4, 4, SRCCOPY);
    EGE_CHECK(ege::getpixel(2, 2, target.value) == kLine);
}

void testSaveDecodeAndVisualRecognition()
{
    ege_test::Image original(kWidth, kHeight);
    EGE_CHECK(original.value != nullptr);
    drawReferenceScene(original.value);
    const auto output = ege_test::artifacts();
    const auto png = output / "raster-contract.png";
    const auto bmp = output / "raster-contract.bmp";

    EGE_CHECK(ege::savepng(original.value, png.string().c_str(), false) == ege::grOk);
    EGE_CHECK(ege::savebmp(original.value, bmp.string().c_str(), false) == ege::grOk);
    EGE_CHECK(std::filesystem::is_regular_file(png) && std::filesystem::file_size(png) > 64);
    EGE_CHECK(std::filesystem::is_regular_file(bmp) && std::filesystem::file_size(bmp) > 64);

    ege_test::Image decodedPng(1, 1);
    ege_test::Image decodedBmp(1, 1);
    EGE_CHECK(ege::getimage(decodedPng.value, png.string().c_str()) == ege::grOk);
    EGE_CHECK(ege::getimage(decodedBmp.value, bmp.string().c_str()) == ege::grOk);
    EGE_CHECK(ege::getwidth(decodedPng.value) == kWidth && ege::getheight(decodedPng.value) == kHeight);
    EGE_CHECK(ege::getwidth(decodedBmp.value) == kWidth && ege::getheight(decodedBmp.value) == kHeight);

    // Recognition is deliberately structural rather than a fragile encoder
    // byte comparison: it confirms line, rectangle, circle and triangle at
    // characteristic pixels after each format has been decoded.
    for (ege::PCIMAGE decoded : { static_cast<ege::PCIMAGE>(decodedPng.value), static_cast<ege::PCIMAGE>(decodedBmp.value) }) {
        EGE_CHECK(ege::getpixel(8, 60, decoded) == kFill);
        EGE_CHECK(ege::getpixel(90, 30, decoded) == kBlue);
        EGE_CHECK(ege::getpixel(57, 62, decoded) == EGERGB(241, 196, 15));
        EGE_CHECK(countLinePixels(decoded) > 80);
    }
}

} // namespace

int main()
{
    testRasterAndPixelAccess();
    testCopyAndRasterOps();
    testSaveDecodeAndVisualRecognition();
    return ege_test::finish("EGE raster/image contract");
}
