#define SHOW_CONSOLE 1
#include "ege.h"
#include "../performance_timer.h"
#include "../test_framework.h"

#include <cstdlib>
#include <iostream>

namespace {

volatile ege::color_t observedPixel = 0;

void clearImage(ege::PIMAGE image, ege::color_t color)
{
    ege::setbkcolor(color, image);
    ege::cleardevice(image);
}

} // namespace

int main()
{
    TestFramework framework;
    if (!framework.initialize(64, 64)) {
        std::cerr << "Unable to initialize the image-buffer performance context\n";
        return EXIT_FAILURE;
    }
    framework.hideWindow();

    const int width = 1024;
    const int height = 1024;
    const int readbackIterations = 8;
    const int cachedReadIterations = 1000;
    const int uploadIterations = 20;
    const int regionalUploadIterations = 20;
    const int copyIterations = 20;
    bool passed = true;

    ege::PIMAGE source = ege::newimage(width, height);
    ege::PIMAGE destination = ege::newimage(width, height);
    if (!source || !destination) {
        std::cerr << "Unable to allocate image-buffer performance fixtures\n";
        if (destination) ege::delimage(destination);
        if (source) ege::delimage(source);
        framework.cleanup();
        return EXIT_FAILURE;
    }

    clearImage(source, ege::BLUE);
    clearImage(destination, ege::BLACK);

    ege::PCIMAGE readOnlySource = source;
    PerformanceTimer firstReadTimer("first synchronized getbuffer read");
    const double firstReadMs = firstReadTimer.measureMs([&]() {
        const ege::color_t* pixels = ege::getbuffer(readOnlySource);
        passed = passed && pixels != nullptr;
        if (pixels) observedPixel ^= pixels[(height / 2) * width + width / 2];
    });

    PerformanceTimer cachedReadTimer("cached const getbuffer reads");
    const double cachedReadMs = cachedReadTimer.measureMs([&]() {
        for (int i = 0; i < cachedReadIterations; ++i) {
            const ege::color_t* pixels = ege::getbuffer(readOnlySource);
            passed = passed && pixels != nullptr;
            if (pixels) observedPixel ^= pixels[i % (width * height)];
        }
    });

    PerformanceTimer repeatedReadbackTimer("GPU draw and getbuffer readback");
    const double repeatedReadbackMs = repeatedReadbackTimer.measureMs([&]() {
        for (int i = 0; i < readbackIterations; ++i) {
            ege::putpixel(i, i, ege::RED, source);
            const ege::color_t* pixels = ege::getbuffer(readOnlySource);
            passed = passed && pixels != nullptr;
            if (pixels) observedPixel ^= pixels[i * width + i];
        }
    });

    PerformanceTimer uploadTimer("getbuffer edit and GPU upload");
    const double uploadMs = uploadTimer.measureMs([&]() {
        for (int i = 0; i < uploadIterations; ++i) {
            ege::color_t* pixels = ege::getbuffer(source);
            passed = passed && pixels != nullptr;
            if (pixels) pixels[i] = ege::GREEN;
            ege::putimage(destination, 0, 0, source);
        }
        ege::PCIMAGE readOnlyDestination = destination;
        const ege::color_t* pixels = ege::getbuffer(readOnlyDestination);
        passed = passed && pixels != nullptr;
        if (pixels) observedPixel ^= pixels[uploadIterations - 1];
    });

    PerformanceTimer markedUploadTimer("marked getbuffer edit and regional GPU upload");
    const double markedUploadMs = markedUploadTimer.measureMs([&]() {
        for (int i = 0; i < regionalUploadIterations; ++i) {
            const int x = i % width;
            const int y = 1;
            ege::color_t* pixels = ege::getbuffer(source);
            passed = passed && pixels != nullptr;
            if (pixels) {
                pixels[y * width + x] = ege::YELLOW;
                ege::markbufferdirty(source, x, y, 1, 1);
            }
            ege::putimage(destination, 0, 0, source);
        }
        ege::PCIMAGE readOnlyDestination = destination;
        const ege::color_t* pixels = ege::getbuffer(readOnlyDestination);
        passed = passed && pixels != nullptr;
        if (pixels) {
            observedPixel ^= pixels[width + regionalUploadIterations - 1];
            passed = passed && pixels[width + regionalUploadIterations - 1] == ege::YELLOW;
        }
    });

    PerformanceTimer directUpdateTimer("updatebuffer regional GPU upload");
    const double directUpdateMs = directUpdateTimer.measureMs([&]() {
        for (int i = 0; i < regionalUploadIterations; ++i) {
            const int x = i % width;
            const int y = 2;
            const ege::color_t pixel = ege::MAGENTA;
            passed = passed &&
                ege::updatebuffer(source, x, y, 1, 1, &pixel) == ege::grOk;
            ege::putimage(destination, 0, 0, source);
        }
        ege::PCIMAGE readOnlyDestination = destination;
        const ege::color_t* pixels = ege::getbuffer(readOnlyDestination);
        passed = passed && pixels != nullptr;
        if (pixels) {
            observedPixel ^= pixels[2 * width + regionalUploadIterations - 1];
            passed = passed && pixels[2 * width + regionalUploadIterations - 1] == ege::MAGENTA;
        }
    });

    clearImage(source, ege::CYAN);
    PerformanceTimer gpuCopyTimer("GPU-to-GPU getimage copies");
    const double gpuCopyMs = gpuCopyTimer.measureMs([&]() {
        for (int i = 0; i < copyIterations; ++i) {
            passed = passed &&
                ege::getimage(destination, source, 0, 0, width, height) == ege::grOk;
        }
        ege::PCIMAGE readOnlyDestination = destination;
        const ege::color_t* pixels = ege::getbuffer(readOnlyDestination);
        passed = passed && pixels != nullptr;
        if (pixels) observedPixel ^= pixels[(height / 2) * width + width / 2];
    });

    std::cout << "image-buffer performance (" << width << 'x' << height << ")\n"
              << "  first synchronized read: " << firstReadMs << " ms\n"
              << "  cached const reads (" << cachedReadIterations << "): "
              << cachedReadMs << " ms\n"
              << "  draw/readback cycles (" << readbackIterations << "): "
              << repeatedReadbackMs << " ms\n"
              << "  edit/upload cycles (" << uploadIterations << "): "
              << uploadMs << " ms\n"
              << "  marked regional edit/upload cycles (" << regionalUploadIterations << "): "
              << markedUploadMs << " ms\n"
              << "  updatebuffer regional upload cycles (" << regionalUploadIterations << "): "
              << directUpdateMs << " ms\n"
              << "  GPU getimage copies (" << copyIterations << "): "
              << gpuCopyMs << " ms\n";

    ege::delimage(destination);
    ege::delimage(source);
    framework.cleanup();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
