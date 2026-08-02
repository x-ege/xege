#include "test_support.h"
#include "encodeconv.h"

#include <array>
#include <cmath>
#include <cstring>

namespace {

void testPublicTextApi()
{
    ege_test::Image image(320, 96);
    EGE_CHECK(image.value != nullptr);
    ege::setbkcolor(BLACK, image.value);
    ege::cleardevice(image.value);
    ege::setfont(24, 0, "Helvetica", image.value);
    ege::settextcolor(WHITE, image.value);
    EGE_CHECK(ege::textwidth("native text", image.value) > 40);
    EGE_CHECK(ege::textheight("native text", image.value) > 10);
    EGE_CHECK(ege::textwidth(L"原生文字", image.value) > 10);
    ege::outtextxy(4, 4, "native text", image.value);
    ege::outtextxy(4, 40, L"原生文字", image.value);
    EGE_CHECK(ege_test::countPixels(image.value, BLACK) < 320 * 96 - 100);
}

void testColorRandomAndCompressionApi()
{
    float h = 0.0f, s = 0.0f, v = 0.0f;
    ege::rgb2hsv(EGERGB(255, 0, 0), &h, &s, &v);
    EGE_CHECK(std::abs(h) < 0.1f && s > 0.99f && v > 0.99f);
    EGE_CHECK(ege::hsv2rgb(h, s, v) == EGERGB(255, 0, 0));
    EGE_CHECK(EGEGET_R(ege::rgb2gray(EGERGB(255, 0, 0))) == EGEGET_G(ege::rgb2gray(EGERGB(255, 0, 0))));
    EGE_CHECK(EGEGET_A(ege::color_unpremultiply(ege::color_premultiply(EGEARGB(127, 80, 40, 20)))) == 127);

    ege::randomize(12345);
    const unsigned int first = ege::random(1000000);
    ege::randomize(12345);
    EGE_CHECK(ege::random(1000000) == first);

    constexpr std::array<unsigned char, 31> payload = {
        0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 0, 7, 7,
        7, 9, 9, 9, 42, 42, 42, 42, 0, 255, 254, 253, 252, 251, 250};
    std::array<unsigned char, 128> compressed{};
    uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
    EGE_CHECK(ege::ege_compress_bound(static_cast<uint32_t>(payload.size())) <= compressed.size());
    EGE_CHECK(ege::ege_compress(compressed.data(), &compressedSize, payload.data(), static_cast<uint32_t>(payload.size())) == 0);
    EGE_CHECK(ege::ege_uncompress_size(compressed.data(), compressedSize) == payload.size());
    std::array<unsigned char, payload.size()> decoded{};
    uint32_t decodedSize = static_cast<uint32_t>(decoded.size());
    EGE_CHECK(ege::ege_uncompress(decoded.data(), &decodedSize, compressed.data(), compressedSize) == 0);
    EGE_CHECK(decodedSize == payload.size() && decoded == payload);
}

void testInvalidUtf8Replacement()
{
    const char invalidContinuation[] = {
        static_cast<char>(0xC2), 'A', '\0'};
    const std::wstring decoded = ege::mb2w(invalidContinuation);
    EGE_CHECK(decoded.size() == 2);
    EGE_CHECK(static_cast<std::uint32_t>(decoded[0]) == 0xFFFDU);
    EGE_CHECK(decoded[1] == L'A');
}

} // namespace

int main()
{
    testPublicTextApi();
    testColorRandomAndCompressionApi();
    testInvalidUtf8Replacement();
    return ege_test::finish("EGE text/utility contract");
}
