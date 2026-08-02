#pragma once

#include <graphics.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace ege {
namespace test {

class Image final {
public:
    Image(int width, int height) : value(ege::newimage(width, height)) {}
    ~Image() { ege::delimage(value); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    ege::PIMAGE value = nullptr;
};

inline int& failures()
{
    static int value = 0;
    return value;
}

inline void check(bool condition, const char* expression, const char* file, int line)
{
    if (!condition) {
        std::cerr << file << ':' << line << ": check failed: " << expression << '\n';
        ++failures();
    }
}

#define EGE_CHECK(condition) ::ege::test::check((condition), #condition, __FILE__, __LINE__)

inline std::uint64_t checksum(ege::PCIMAGE image)
{
    const int width = ege::getwidth(image);
    const int height = ege::getheight(image);
    const ege::color_t* pixels = ege::getbuffer(image);
    std::uint64_t hash = 1469598103934665603ULL;
    for (int index = 0; index < width * height; ++index) {
        hash ^= static_cast<std::uint32_t>(pixels[index]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline int countPixels(ege::PCIMAGE image, ege::color_t color)
{
    const int width = ege::getwidth(image);
    const int height = ege::getheight(image);
    const ege::color_t* pixels = ege::getbuffer(image);
    int result = 0;
    for (int index = 0; index < width * height; ++index) {
        result += pixels[index] == color;
    }
    return result;
}

inline std::filesystem::path artifacts()
{
#ifdef EGE_TEST_ARTIFACT_DIR
    const std::filesystem::path output(EGE_TEST_ARTIFACT_DIR);
#else
    const std::filesystem::path output = std::filesystem::current_path() / "test-artifacts";
#endif
    std::filesystem::create_directories(output);
    return output;
}

inline int finish(const char* suite)
{
    if (failures() != 0) {
        std::cerr << failures() << ' ' << suite << " check(s) failed\n";
        return 1;
    }
    std::cout << suite << " checks passed\n";
    return 0;
}

} // namespace test
} // namespace ege

// Preserve the existing test-only call sites while keeping every
// implementation inside the repository's ege namespace.
namespace ege_test = ::ege::test;
