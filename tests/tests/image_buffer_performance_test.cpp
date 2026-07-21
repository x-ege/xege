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
    const int copyIterations = 20;
    bool passed = true;

    ege::PIMAGE source = ege::newimage(width, height);
    ege::PIMAGE destination = ege::newimage(width, height);
    ege::PIMAGE gpuCopySource = ege::newimage(width, height);
    if (!source || !destination || !gpuCopySource) {
        std::cerr << "Unable to allocate image-buffer performance fixtures\n";
        if (gpuCopySource) ege::delimage(gpuCopySource);
        if (destination) ege::delimage(destination);
        if (source) ege::delimage(source);
        framework.cleanup();
        return EXIT_FAILURE;
    }

    clearImage(source, ege::BLUE);
    clearImage(destination, ege::BLACK);
    clearImage(gpuCopySource, ege::CYAN);

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

    ege::color_t* persistentPixels = nullptr;
    PerformanceTimer promotionTimer("writable getbuffer promotion");
    const double promotionMs = promotionTimer.measureMs([&]() {
        persistentPixels = ege::getbuffer(source);
        passed = passed && persistentPixels != nullptr;
#ifdef _WIN32
        passed = passed && ege::getimagestoragemode(source) ==
                               ege::IMAGE_STORAGE_CPU_BITMAP;
#endif
    });

    PerformanceTimer uploadTimer("retained CPU bitmap edit and GPU upload");
    const double uploadMs = uploadTimer.measureMs([&]() {
        for (int i = 0; i < uploadIterations; ++i) {
            if (persistentPixels) persistentPixels[i] = ege::GREEN;
            ege::putimage(destination, 0, 0, source);
        }
        ege::PCIMAGE readOnlyDestination = destination;
        const ege::color_t* pixels = ege::getbuffer(readOnlyDestination);
        passed = passed && pixels != nullptr;
        if (pixels) observedPixel ^= pixels[uploadIterations - 1];
    });

    PerformanceTimer gpuCopyTimer("GPU-to-GPU getimage copies");
    const double gpuCopyMs = gpuCopyTimer.measureMs([&]() {
        for (int i = 0; i < copyIterations; ++i) {
            passed = passed &&
                ege::getimage(destination, gpuCopySource, 0, 0, width, height) == ege::grOk;
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
              << "  writable promotion: " << promotionMs << " ms\n"
              << "  retained edit/upload cycles (" << uploadIterations << "): "
              << uploadMs << " ms\n"
              << "  GPU getimage copies (" << copyIterations << "): "
              << gpuCopyMs << " ms\n";

    ege::delimage(gpuCopySource);
    ege::delimage(destination);
    ege::delimage(source);
    framework.cleanup();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
