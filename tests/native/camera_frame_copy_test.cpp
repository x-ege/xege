#include "camera_frame_copy.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool bytesEqual(const unsigned char* lhs, const unsigned char* rhs,
                std::size_t count)
{
    return std::memcmp(lhs, rhs, count) == 0;
}

bool bytesAre(const unsigned char* bytes, std::size_t count,
              unsigned char value)
{
    return std::all_of(bytes, bytes + count,
        [value](unsigned char byte) { return byte == value; });
}

} // namespace

int main()
{
    using ege::detail::BgraFrameView;
    using ege::detail::copyBgraFramePixels;
    using ege::detail::validateBgraFrameLayout;

    std::vector<unsigned char> paddedSource(32);
    for (std::size_t i = 0; i < paddedSource.size(); ++i) {
        paddedSource[i] = static_cast<unsigned char>(i + 1);
    }
    const BgraFrameView padded = {
        paddedSource.data(), paddedSource.size(), 3, 2, 16};

    std::size_t rowBytes = 0;
    std::size_t imageBytes = 0;
    expect(validateBgraFrameLayout(padded, &rowBytes, &imageBytes),
           "a padded two-row BGRA frame has a valid layout");
    expect(rowBytes == 12 && imageBytes == 24,
           "layout reports active row and destination byte counts");

    std::vector<unsigned char> paddedDestination(imageBytes + 8, 0xA5);
    expect(copyBgraFramePixels(paddedDestination.data(), imageBytes, padded),
           "padded-stride frame copies successfully");
    expect(bytesEqual(paddedDestination.data(), paddedSource.data(), rowBytes),
           "the first active row is copied without padding");
    expect(bytesEqual(paddedDestination.data() + rowBytes,
                      paddedSource.data() + padded.stride, rowBytes),
           "the second active row starts at the source stride");
    expect(bytesAre(paddedDestination.data() + imageBytes, 8, 0xA5),
           "padded-stride copy does not overwrite the destination guard");

    std::vector<unsigned char> tightSource(24);
    for (std::size_t i = 0; i < tightSource.size(); ++i) {
        tightSource[i] = static_cast<unsigned char>(0x40 + i);
    }
    const BgraFrameView tightWithTrailingBytes = {
        tightSource.data(), tightSource.size(), 2, 2, 8};
    std::vector<unsigned char> tightDestination(24, 0x5A);
    expect(copyBgraFramePixels(
               tightDestination.data(), 16, tightWithTrailingBytes),
           "tight-stride frame tolerates provider-owned trailing bytes");
    expect(bytesEqual(tightDestination.data(), tightSource.data(), 16),
           "tight-stride frame copies exactly the active pixels");
    expect(bytesAre(tightDestination.data() + 16, 8, 0x5A),
           "tight-stride copy never copies provider trailing bytes");

    std::vector<unsigned char> unchanged(24, 0xCC);
    BgraFrameView invalid = padded;
    invalid.stride = 11;
    expect(!copyBgraFramePixels(unchanged.data(), unchanged.size(), invalid),
           "stride shorter than an active row is rejected");
    expect(bytesAre(unchanged.data(), unchanged.size(), 0xCC),
           "invalid stride leaves the destination untouched");

    invalid = padded;
    invalid.dataSize = 27;
    expect(!copyBgraFramePixels(unchanged.data(), unchanged.size(), invalid),
           "a truncated final source row is rejected");
    expect(bytesAre(unchanged.data(), unchanged.size(), 0xCC),
           "truncated source leaves the destination untouched");

    invalid = padded;
    invalid.data = nullptr;
    expect(!validateBgraFrameLayout(invalid, nullptr, nullptr),
           "a null source pointer is rejected");
    expect(!copyBgraFramePixels(unchanged.data(), 23, padded),
           "a destination smaller than the packed image is rejected");
    expect(bytesAre(unchanged.data(), unchanged.size(), 0xCC),
           "a short destination leaves all bytes untouched");

    invalid = padded;
    invalid.width = 0;
    expect(!validateBgraFrameLayout(invalid, nullptr, nullptr),
           "zero width is rejected");
    invalid = padded;
    invalid.height = 0;
    expect(!validateBgraFrameLayout(invalid, nullptr, nullptr),
           "zero height is rejected");

    unsigned char dummy = 0;
    const std::uint32_t aboveIntMax =
        static_cast<std::uint32_t>(INT_MAX) + 1u;
    const BgraFrameView tooWide = {
        &dummy, std::numeric_limits<std::uint32_t>::max(), aboveIntMax, 1,
        std::numeric_limits<std::uint32_t>::max()};
    expect(!validateBgraFrameLayout(tooWide, nullptr, nullptr),
           "width that IMAGE cannot represent is rejected");

    const BgraFrameView tooTall = {
        &dummy, std::numeric_limits<std::uint32_t>::max(), 1, aboveIntMax, 4};
    expect(!validateBgraFrameLayout(tooTall, nullptr, nullptr),
           "height that IMAGE cannot represent is rejected");

    const BgraFrameView imageByteCountOverflowsInt = {
        &dummy, std::numeric_limits<std::uint32_t>::max(),
        32768, 16384, 131072};
    expect(!validateBgraFrameLayout(
               imageByteCountOverflowsInt, nullptr, nullptr),
           "a byte count that overflows IMAGE internals is rejected");

    if (failures != 0) {
        std::cerr << failures << " camera frame copy assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All camera frame copy assertions passed\n";
    return EXIT_SUCCESS;
}
