#include "test_support.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>

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

int countColor(ege::PCIMAGE image, ege::color_t color, int left, int top, int right, int bottom)
{
    int count = 0;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            count += ege::getpixel(x, y, image) == color;
        }
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
    ege::putpixel(4, 5, kBlue, target.value);
    ege::putimage(target.value, 3, 4, source.value, SRCINVERT);
    EGE_CHECK(ege::getpixel(4, 5, target.value) == (kBlue ^ kLine));

    EGE_CHECK(ege::putimage_rotate(target.value, nullptr, 0, 0, 0.5f, 0.5f, 0.0f)
        == ege::grNullPointer);
    EGE_CHECK(ege::putimage_rotatezoom(target.value, nullptr, 0, 0, 0.5f, 0.5f, 0.0f, 1.0f)
        == ege::grNullPointer);

    ege::putimage(target.value, 0, 0, 8, 8, source.value, 0, 0, 4, 4, SRCCOPY);
    EGE_CHECK(ege::getpixel(2, 2, target.value) == kLine);

    // GDI putpixel writes the requested color directly; setwritemode applies
    // to drawing primitives, not raw pixel access. Keep that public contract
    // on the native backend as well.
    ege::setwritemode(R2_XORPEN, target.value);
    ege::putpixel(4, 5, kBlue, target.value);
    EGE_CHECK(ege::getpixel(4, 5, target.value) == kBlue);
    ege::setwritemode(R2_COPYPEN, target.value);
}

void testBasicLineAntialiasingAndXor()
{
    ege_test::Image image(16, 16);
    EGE_CHECK(image.value != nullptr);
    ege::setbkcolor(kBackground, image.value);
    ege::cleardevice(image.value);
    ege::ege_enable_aa(false, image.value);
    ege::setlinecolor(kLine, image.value);
    ege::setlinestyle(PS_SOLID, 0, 1, image.value);

    ege::line(2, 4, 12, 4, image.value);
    EGE_CHECK(ege::getpixel(6, 4, image.value) == kLine);
    EGE_CHECK(ege::getpixel(6, 3, image.value) == kBackground);
    EGE_CHECK(ege::getpixel(6, 5, image.value) == kBackground);

    ege::line(8, 7, 8, 13, image.value);
    EGE_CHECK(ege::getpixel(8, 10, image.value) == kLine);
    EGE_CHECK(ege::getpixel(7, 10, image.value) == kBackground);
    EGE_CHECK(ege::getpixel(9, 10, image.value) == kBackground);

    ege::cleardevice(image.value);
    ege::setwritemode(R2_XORPEN, image.value);
    ege::line(2, 4, 12, 4, image.value);
    const ege::color_t xorResult = (kBackground & 0xFF000000U) |
        ((kBackground ^ kLine) & 0x00FFFFFFU);
    EGE_CHECK(ege::getpixel(6, 4, image.value) == xorResult);
    EGE_CHECK(EGEGET_A(ege::getpixel(6, 4, image.value)) ==
        EGEGET_A(kBackground));
    ege::line(2, 4, 12, 4, image.value);
    EGE_CHECK(ege::getpixel(6, 4, image.value) == kBackground);
    ege::setwritemode(R2_COPYPEN, image.value);
}

void testAllPrimitiveRasterOps()
{
    ege_test::Image image(16, 16);
    EGE_CHECK(image.value != nullptr);
    ege::ege_enable_aa(false, image.value);
    ege::setlinecolor(kLine, image.value);
    ege::setlinestyle(PS_SOLID, 0, 1, image.value);

    const auto withDestinationAlpha = [](ege::color_t rgb) {
        return (kBackground & 0xFF000000U) | (rgb & 0x00FFFFFFU);
    };
    const std::array<std::pair<int, ege::color_t>, 16> operations = {{
        {R2_BLACK, withDestinationAlpha(0x00000000U)},
        {R2_NOTMERGEPEN, withDestinationAlpha(~(kBackground | kLine))},
        {R2_MASKNOTPEN, withDestinationAlpha(kBackground & ~kLine)},
        {R2_NOTCOPYPEN, withDestinationAlpha(~kLine)},
        {R2_MASKPENNOT, withDestinationAlpha(kLine & ~kBackground)},
        {R2_NOT, withDestinationAlpha(~kBackground)},
        {R2_XORPEN, withDestinationAlpha(kBackground ^ kLine)},
        {R2_NOTMASKPEN, withDestinationAlpha(~(kBackground & kLine))},
        {R2_MASKPEN, withDestinationAlpha(kBackground & kLine)},
        {R2_NOTXORPEN, withDestinationAlpha(~(kBackground ^ kLine))},
        {R2_NOP, kBackground},
        {R2_MERGENOTPEN, withDestinationAlpha(kBackground | ~kLine)},
        {R2_COPYPEN, kLine},
        {R2_MERGEPENNOT, withDestinationAlpha(kLine | ~kBackground)},
        {R2_MERGEPEN, withDestinationAlpha(kBackground | kLine)},
        {R2_WHITE, withDestinationAlpha(0x00FFFFFFU)},
    }};

    for (const auto& operation : operations) {
        ege::setbkcolor(kBackground, image.value);
        ege::cleardevice(image.value);
        ege::setwritemode(operation.first, image.value);
        ege::line(2, 4, 12, 4, image.value);
        EGE_CHECK(ege::getpixel(6, 4, image.value) == operation.second);
    }
    ege::setwritemode(R2_COPYPEN, image.value);
}

void testRasterOpMiterBoundsUnderTransform()
{
    constexpr int size = 200;
    ege_test::Image copyImage(size, size);
    ege_test::Image xorImage(size, size);
    EGE_CHECK(copyImage.value != nullptr && xorImage.value != nullptr);

    const ege::color_t black = EGERGB(0, 0, 0);
    const ege::color_t white = EGERGB(255, 255, 255);
    const int narrowTriangle[] = {85, 170, 100, 80, 115, 170};
    const ege::ege_transform_matrix transform = {
        1.0f, 0.15f, 0.35f, 1.0f, -30.0f, -5.0f};

    ege::PIMAGE images[] = {copyImage.value, xorImage.value};
    for (ege::PIMAGE image : images) {
        ege::setbkcolor(black, image);
        ege::cleardevice(image);
        ege::ege_enable_aa(false, image);
        ege::setlinecolor(white, image);
        ege::setlinestyle(PS_SOLID, 0, 20, image);
        ege::setlinejoin(ege::LINEJOIN_MITER, 10.0f, image);
        ege::ege_set_transform(&transform, image);
    }

    ege::drawpoly(3, narrowTriangle, copyImage.value);
    ege::setwritemode(R2_XORPEN, xorImage.value);
    ege::drawpoly(3, narrowTriangle, xorImage.value);

    const ege::color_t* copyPixels = ege::getbuffer(copyImage.value);
    const ege::color_t* xorPixels = ege::getbuffer(xorImage.value);
    EGE_CHECK(copyPixels != nullptr && xorPixels != nullptr);
    const std::size_t pixelCount = static_cast<std::size_t>(size) * size;
    EGE_CHECK(std::count(copyPixels, copyPixels + pixelCount, white) > 1000);
    // On black, COPY white and XOR white have identical output.  Comparing the
    // full transformed acute join catches any pixels omitted from the ROP dirty
    // region rather than sampling only the triangle's ordinary edges.
    EGE_CHECK(std::equal(copyPixels, copyPixels + pixelCount, xorPixels));
}

void testRasterOpPreservesPremultipliedAlpha()
{
    ege_test::Image image(16, 16);
    EGE_CHECK(image.value != nullptr);
    const ege::color_t destination = EGEARGB(128, 200, 0, 0);
    const ege::color_t source = EGERGB(255, 255, 255);
    ege::setbkcolor(destination, image.value);
    ege::cleardevice(image.value);
    ege::ege_enable_aa(false, image.value);
    ege::setlinecolor(source, image.value);
    ege::setlinestyle(PS_SOLID, 0, 1, image.value);
    ege::setwritemode(R2_XORPEN, image.value);
    ege::line(2, 4, 12, 4, image.value);

    const ege::color_t actual = ege::getpixel(6, 4, image.value);
    const ege::color_t expected = ege::color_premultiply(
        EGEARGB(128, 200 ^ 255, 0 ^ 255, 0 ^ 255));
    EGE_CHECK(actual == expected);
    EGE_CHECK(EGEGET_A(actual) == 128);
    EGE_CHECK(EGEGET_R(actual) <= EGEGET_A(actual));
    EGE_CHECK(EGEGET_G(actual) <= EGEGET_A(actual));
    EGE_CHECK(EGEGET_B(actual) <= EGEGET_A(actual));
}

void testPatternFillContract()
{
    ege_test::Image image(kWidth, kHeight);
    EGE_CHECK(image.value != nullptr);

    ege::setbkcolor(kBackground, image.value);
    ege::cleardevice(image.value);
    ege::setbkmode(TRANSPARENT, image.value);
    ege::setfillstyle(LINE_FILL, kLine, image.value);
    ege::bar(0, 0, 32, 32, image.value);
    const int transparentLines = countColor(image.value, kLine, 0, 0, 32, 32);
    const int transparentGaps = countColor(image.value, kBackground, 0, 0, 32, 32);
    EGE_CHECK(transparentLines > 0 && transparentGaps > 0);

    // In OPAQUE mode the gaps in a hatch use the current background color,
    // matching the Win32 brush contract instead of preserving old pixels.
    ege::setbkcolor(kFill, image.value);
    ege::cleardevice(image.value);
    ege::setbkcolor_f(kBlue, image.value);
    ege::setbkmode(OPAQUE, image.value);
    ege::setfillstyle(LINE_FILL, kLine, image.value);
    ege::bar(0, 0, 32, 32, image.value);
    EGE_CHECK(countColor(image.value, kLine, 0, 0, 32, 32) > 0);
    EGE_CHECK(countColor(image.value, kBlue, 0, 0, 32, 32) > 0);
    EGE_CHECK(countColor(image.value, kFill, 0, 0, 32, 32) == 0);
}

void testFailedResizePreservesImage()
{
    ege_test::Image image(2, 2);
    ege::putpixel(1, 1, kLine, image.value);
    ege::color_t* const originalBuffer = ege::getbuffer(image.value);
    const int result = ege::resize(image.value,
        std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    EGE_CHECK(result != ege::grOk);
    EGE_CHECK(ege::getwidth(image.value) == 2 && ege::getheight(image.value) == 2);
    EGE_CHECK(ege::getbuffer(image.value) == originalBuffer);
    EGE_CHECK(ege::getpixel(1, 1, image.value) == kLine);
}

void testBufferAndStorageErrors()
{
    ege_test::Image image(4, 4);
    const ege::color_t pixel = kBlue;
    EGE_CHECK(ege::getimagestoragemode(image.value) == ege::IMAGE_STORAGE_CPU_BITMAP);
    EGE_CHECK(ege::setimagestoragemode(image.value, ege::IMAGE_STORAGE_CPU_BITMAP) == ege::grOk);
    EGE_CHECK(ege::setimagestoragemode(image.value, ege::IMAGE_STORAGE_GPU) == ege::grInvalidMode);
    EGE_CHECK(ege::setimagestoragemode(image.value,
        static_cast<ege::image_storage_mode>(99)) == ege::grParamError);

    EGE_CHECK(ege::getbuffer(image.value,
        static_cast<ege::image_buffer_access>(99)) == nullptr);
    EGE_CHECK(ege::updatebuffer(image.value, 0, 0, 1, 1, nullptr) == ege::grNullPointer);
    EGE_CHECK(ege::updatebuffer(image.value, -1, 0, 1, 1, &pixel) == ege::grInvalidRegion);
    EGE_CHECK(ege::updatebuffer(image.value, 3, 3, 2, 2, &pixel) == ege::grInvalidRegion);
    EGE_CHECK(ege::updatebuffer(image.value, 0, 0, 1, 1, &pixel, 1) == ege::grParamError);
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

    ege_test::Image scaled(1, 1);
    EGE_CHECK(ege::getimage(scaled.value, png.string().c_str(), 48, 32) == ege::grOk);
    EGE_CHECK(ege::getwidth(scaled.value) == 48 && ege::getheight(scaled.value) == 32);
    EGE_CHECK(ege::getimage(scaled.value, png.string().c_str(), -1, 32) == ege::grParamError);
}

} // namespace

int main()
{
    testRasterAndPixelAccess();
    testCopyAndRasterOps();
    testBasicLineAntialiasingAndXor();
    testAllPrimitiveRasterOps();
    testRasterOpMiterBoundsUnderTransform();
    testRasterOpPreservesPremultipliedAlpha();
    testPatternFillContract();
    testFailedResizePreservesImage();
    testBufferAndStorageErrors();
    testSaveDecodeAndVisualRecognition();
    return ege_test::finish("EGE raster/image contract");
}
