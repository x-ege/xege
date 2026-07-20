#define SHOW_CONSOLE 1
#include "ege.h"
#include "../performance_timer.h"
#include "../test_framework.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

const int kWidth = 1024;
const int kHeight = 1024;
const int kSamples = 21;
const int kWarmups = 3;

volatile ege::color_t observedPixel = 0;

struct BenchmarkResult {
    std::string metric;
    int operationsPerSample;
    std::vector<double> samplesMs;
};

std::string backendName()
{
#if !defined(_WIN32)
    return "opengl";
#elif defined(EGE_BUILD_OPENGL)
    const char* openGlMode = std::getenv("EGE_TEST_OPENGL");
    return openGlMode != nullptr && openGlMode[0] == '1' ? "opengl" : "gdi";
#else
    return "gdi";
#endif
}

void clearImage(ege::PIMAGE image, ege::color_t color)
{
    ege::setbkcolor(color, image);
    ege::cleardevice(image);
}

const ege::color_t* synchronizeImage(ege::PIMAGE image, bool& passed)
{
    ege::PCIMAGE readOnlyImage = image;
    const ege::color_t* pixels = ege::getbuffer(readOnlyImage);
    passed = passed && pixels != nullptr;
    if (pixels) {
        observedPixel ^= pixels[(kHeight / 2) * kWidth + kWidth / 2];
    }
    return pixels;
}

bool sameRgb(ege::color_t actual, ege::color_t expected)
{
    return (actual & 0x00FFFFFFU) == (expected & 0x00FFFFFFU);
}

void expectPixel(ege::PIMAGE image, int x, int y,
                 ege::color_t expected, bool& passed)
{
    const ege::color_t actual = ege::getpixel_f(x, y, image);
    observedPixel ^= actual;
    passed = passed && sameRgb(actual, expected);
}

template<typename Setup, typename Body, typename Verify>
BenchmarkResult runBenchmark(const char* metric, int operationsPerSample,
                             Setup setup, Body body, Verify verify)
{
    BenchmarkResult result;
    result.metric = metric;
    result.operationsPerSample = operationsPerSample;
    result.samplesMs.reserve(kSamples);

    for (int sample = -kWarmups; sample < kSamples; ++sample) {
        setup();
        PerformanceTimer timer(metric);
        const double elapsedMs = timer.measureMs(body);
        verify();
        if (sample >= 0) {
            result.samplesMs.push_back(elapsedMs);
        }
    }
    return result;
}

double percentile(const std::vector<double>& values, double fraction)
{
    if (values.empty()) return 0.0;
    std::vector<double> sorted(values);
    std::sort(sorted.begin(), sorted.end());
    const size_t index = static_cast<size_t>(
        std::ceil(fraction * static_cast<double>(sorted.size()))) - 1;
    return sorted[std::min(index, sorted.size() - 1)];
}

void printResult(const std::string& backend, const BenchmarkResult& result)
{
    if (result.samplesMs.empty()) return;
    const double medianMs = percentile(result.samplesMs, 0.5);
    const double p95Ms = percentile(result.samplesMs, 0.95);
    const double minimumMs = *std::min_element(
        result.samplesMs.begin(), result.samplesMs.end());
    const double maximumMs = *std::max_element(
        result.samplesMs.begin(), result.samplesMs.end());
    double meanMs = 0.0;
    for (std::vector<double>::const_iterator sample = result.samplesMs.begin();
         sample != result.samplesMs.end(); ++sample) {
        meanMs += *sample;
    }
    meanMs /= result.samplesMs.size();
    double variance = 0.0;
    for (std::vector<double>::const_iterator sample = result.samplesMs.begin();
         sample != result.samplesMs.end(); ++sample) {
        const double difference = *sample - meanMs;
        variance += difference * difference;
    }
    const double standardDeviationMs = result.samplesMs.size() > 1
        ? std::sqrt(variance / (result.samplesMs.size() - 1)) : 0.0;
    const double nanosecondsPerOperation = result.operationsPerSample > 0
        ? medianMs * 1000000.0 / result.operationsPerSample : 0.0;

    std::cout << std::fixed << std::setprecision(6)
              << "PIXEL_PERF"
              << " backend=" << backend
              << " metric=" << result.metric
              << " samples=" << result.samplesMs.size()
              << " operations_per_sample=" << result.operationsPerSample
              << " median_ms=" << medianMs
              << " mean_ms=" << meanMs
              << " stddev_ms=" << standardDeviationMs
              << " p95_ms=" << p95Ms
              << " min_ms=" << minimumMs
              << " max_ms=" << maximumMs
              << " ns_per_op=" << nanosecondsPerOperation
              << '\n';
}

} // namespace

int main()
{
    TestFramework framework;
    if (!framework.initialize(64, 64)) {
        std::cerr << "Unable to initialize the pixel-access performance context\n";
        return EXIT_FAILURE;
    }
    framework.hideWindow();

    bool passed = true;
    ege::PIMAGE source = ege::newimage(kWidth, kHeight);
    ege::PIMAGE destination = ege::newimage(kWidth, kHeight);
    if (!source || !destination) {
        std::cerr << "Unable to allocate pixel-access performance fixtures\n";
        if (destination) ege::delimage(destination);
        if (source) ege::delimage(source);
        framework.cleanup();
        return EXIT_FAILURE;
    }

    const std::string backend = backendName();
    std::vector<BenchmarkResult> results;
    const auto resetFixtures = [&]() {
        clearImage(source, ege::BLUE);
        clearImage(destination, ege::BLACK);
        synchronizeImage(source, passed);
        synchronizeImage(destination, passed);
    };

    const int cachedReadOperations = 200000;
    results.push_back(runBenchmark(
        "getpixel_cached", cachedReadOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < cachedReadOperations; ++i) {
                observedPixel ^= ege::getpixel(
                    (i * 37) % kWidth, (i * 61) % kHeight, source);
            }
        },
        [&]() { expectPixel(source, 0, 0, ege::BLUE, passed); }));

    results.push_back(runBenchmark(
        "getpixel_f_cached", cachedReadOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < cachedReadOperations; ++i) {
                observedPixel ^= ege::getpixel_f(
                    (i * 37) % kWidth, (i * 61) % kHeight, source);
            }
        },
        [&]() { expectPixel(source, 0, 0, ege::BLUE, passed); }));

    const int readAfterDrawOperations = 64;
    results.push_back(runBenchmark(
        "putpixel_getpixel_cycle", readAfterDrawOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < readAfterDrawOperations; ++i) {
                const int x = (i * 37) % kWidth;
                const int y = (i * 61) % kHeight;
                ege::putpixel(x, y, ege::RED, source);
                observedPixel ^= ege::getpixel(x, y, source);
            }
        },
        [&]() {
            const int last = readAfterDrawOperations - 1;
            expectPixel(source, (last * 37) % kWidth,
                        (last * 61) % kHeight, ege::RED, passed);
        }));

    const int pixelWriteOperations = 20000;
    const int lastPixelWrite = pixelWriteOperations - 1;
    results.push_back(runBenchmark(
        "putpixel_committed", pixelWriteOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < pixelWriteOperations; ++i) {
                ege::putpixel((i * 37) % kWidth, (i * 61) % kHeight,
                              ege::GREEN, source);
            }
            synchronizeImage(source, passed);
        },
        [&]() {
            expectPixel(source, (lastPixelWrite * 37) % kWidth,
                        (lastPixelWrite * 61) % kHeight, ege::GREEN, passed);
        }));

    results.push_back(runBenchmark(
        "putpixel_f_committed", pixelWriteOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < pixelWriteOperations; ++i) {
                ege::putpixel_f((i * 37) % kWidth, (i * 61) % kHeight,
                                ege::CYAN, source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, (lastPixelWrite * 37) % kWidth,
                        (lastPixelWrite * 61) % kHeight, ege::CYAN, passed);
        }));

    std::vector<int> pointTriples(static_cast<size_t>(pixelWriteOperations) * 3);
    for (int i = 0; i < pixelWriteOperations; ++i) {
        pointTriples[static_cast<size_t>(i) * 3] = (i * 37) % kWidth;
        pointTriples[static_cast<size_t>(i) * 3 + 1] = (i * 61) % kHeight;
        pointTriples[static_cast<size_t>(i) * 3 + 2] = ege::YELLOW;
    }
    results.push_back(runBenchmark(
        "putpixels_committed", pixelWriteOperations,
        resetFixtures,
        [&]() {
            ege::putpixels(pixelWriteOperations, pointTriples.data(), source);
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, (lastPixelWrite * 37) % kWidth,
                        (lastPixelWrite * 61) % kHeight, ege::YELLOW, passed);
        }));

    const int regionalUploadOperations = 32;
    const int regionalY = 7;
    results.push_back(runBenchmark(
        "getbuffer_legacy_1px_committed", regionalUploadOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < regionalUploadOperations; ++i) {
                ege::color_t* pixels = ege::getbuffer(source);
                passed = passed && pixels != nullptr;
                if (pixels) pixels[regionalY * kWidth + i] = ege::RED;
                ege::putimage(destination, 0, 0, source);
            }
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, regionalUploadOperations - 1,
                        regionalY, ege::RED, passed);
        }));

    results.push_back(runBenchmark(
        "getbuffer_marked_1px_committed", regionalUploadOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < regionalUploadOperations; ++i) {
                ege::color_t* pixels = ege::getbuffer(source);
                passed = passed && pixels != nullptr;
                if (pixels) {
                    pixels[regionalY * kWidth + i] = ege::GREEN;
                    ege::markbufferdirty(source, i, regionalY, 1, 1);
                }
                ege::putimage(destination, 0, 0, source);
            }
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, regionalUploadOperations - 1,
                        regionalY, ege::GREEN, passed);
        }));

    results.push_back(runBenchmark(
        "updatebuffer_1px_committed", regionalUploadOperations,
        resetFixtures,
        [&]() {
            const ege::color_t pixel = ege::MAGENTA;
            for (int i = 0; i < regionalUploadOperations; ++i) {
                passed = passed &&
                    ege::updatebuffer(source, i, regionalY, 1, 1, &pixel) == ege::grOk;
                ege::putimage(destination, 0, 0, source);
            }
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, regionalUploadOperations - 1,
                        regionalY, ege::MAGENTA, passed);
        }));

    const int blockSize = 64;
    std::vector<ege::color_t> blockPixels(
        static_cast<size_t>(blockSize) * blockSize, ege::LIGHTGREEN);
    results.push_back(runBenchmark(
        "updatebuffer_64x64_committed", regionalUploadOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < regionalUploadOperations; ++i) {
                const int x = (i * 23) % (kWidth - blockSize);
                const int y = (i * 29) % (kHeight - blockSize);
                passed = passed && ege::updatebuffer(
                    source, x, y, blockSize, blockSize,
                    blockPixels.data()) == ege::grOk;
                ege::putimage(destination, 0, 0, source);
            }
            synchronizeImage(destination, passed);
        },
        [&]() {
            const int last = regionalUploadOperations - 1;
            expectPixel(destination, (last * 23) % (kWidth - blockSize),
                        (last * 29) % (kHeight - blockSize),
                        ege::LIGHTGREEN, passed);
        }));

    const int fullFrameOperations = 8;
    std::vector<ege::color_t> fullFramePixels(
        static_cast<size_t>(kWidth) * kHeight, ege::LIGHTMAGENTA);
    results.push_back(runBenchmark(
        "updatebuffer_full_frame_committed", fullFrameOperations,
        resetFixtures,
        [&]() {
            for (int i = 0; i < fullFrameOperations; ++i) {
                passed = passed && ege::updatebuffer(
                    source, 0, 0, kWidth, kHeight,
                    fullFramePixels.data()) == ege::grOk;
                ege::putimage(destination, 0, 0, source);
            }
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, kWidth / 2, kHeight / 2,
                        ege::LIGHTMAGENTA, passed);
        }));

    std::cout << "PIXEL_PERF_CONFIG"
              << " backend=" << backend
              << " width=" << kWidth
              << " height=" << kHeight
              << " samples=" << kSamples
              << " warmups=" << kWarmups
              << '\n';
    for (std::vector<BenchmarkResult>::const_iterator result = results.begin();
         result != results.end(); ++result) {
        printResult(backend, *result);
    }
    std::cout << "PIXEL_PERF_CHECKSUM value=" << observedPixel << '\n';

    ege::delimage(destination);
    ege::delimage(source);
    framework.cleanup();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
