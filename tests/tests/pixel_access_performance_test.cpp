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
    const ege::image_storage_mode initialSourceMode =
        ege::getimagestoragemode(source);
    std::vector<BenchmarkResult> results;
    const auto resetGpuFixtures = [&]() {
        clearImage(source, ege::BLUE);
        clearImage(destination, ege::BLACK);
        synchronizeImage(source, passed);
        synchronizeImage(destination, passed);
    };

    const int cachedReadOperations = 200000;
    results.push_back(runBenchmark(
        "getpixel_cached", cachedReadOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < cachedReadOperations; ++i) {
                observedPixel ^= ege::getpixel(
                    (i * 37) % kWidth, (i * 61) % kHeight, source);
            }
        },
        [&]() { expectPixel(source, 0, 0, ege::BLUE, passed); }));

    results.push_back(runBenchmark(
        "getpixel_f_cached", cachedReadOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < cachedReadOperations; ++i) {
                observedPixel ^= ege::getpixel_f(
                    (i * 37) % kWidth, (i * 61) % kHeight, source);
            }
        },
        [&]() { expectPixel(source, 0, 0, ege::BLUE, passed); }));

    const int synchronizationCycles = 32;
    results.push_back(runBenchmark(
        "putpixel_getpixel_cycle", synchronizationCycles,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < synchronizationCycles; ++i) {
                const int x = (i * 37) % kWidth;
                const int y = (i * 61) % kHeight;
                ege::putpixel(x, y, ege::RED, source);
                observedPixel ^= ege::getpixel(x, y, source);
            }
        },
        [&]() {
            const int last = synchronizationCycles - 1;
            expectPixel(source, (last * 37) % kWidth,
                        (last * 61) % kHeight, ege::RED, passed);
        }));

    results.push_back(runBenchmark(
        "getbuffer_read_after_draw_cycle", synchronizationCycles,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < synchronizationCycles; ++i) {
                const int x = (i * 37) % kWidth;
                const int y = (i * 61) % kHeight;
                ege::putpixel(x, y, ege::YELLOW, source);
                ege::color_t* pixels =
                    ege::getbuffer(source, ege::IMAGE_BUFFER_READ);
                passed = passed && pixels != nullptr;
                if (pixels) observedPixel ^= pixels[y * kWidth + x];
            }
        },
        [&]() {
            passed = passed &&
                ege::getimagestoragemode(source) == initialSourceMode;
        }));

    const int pixelWriteOperations = 20000;
    const int lastPixelWrite = pixelWriteOperations - 1;
    results.push_back(runBenchmark(
        "putpixel_committed", pixelWriteOperations,
        resetGpuFixtures,
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
        "putpixel_f_staging_committed", pixelWriteOperations,
        resetGpuFixtures,
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
            passed = passed &&
                ege::getimagestoragemode(source) == initialSourceMode;
        }));

    std::vector<int> pointTriples(static_cast<size_t>(pixelWriteOperations) * 3);
    for (int i = 0; i < pixelWriteOperations; ++i) {
        pointTriples[static_cast<size_t>(i) * 3] = (i * 37) % kWidth;
        pointTriples[static_cast<size_t>(i) * 3 + 1] = (i * 61) % kHeight;
        pointTriples[static_cast<size_t>(i) * 3 + 2] = ege::LIGHTRED;
    }
    results.push_back(runBenchmark(
        "putpixels_staging_committed", pixelWriteOperations,
        resetGpuFixtures,
        [&]() {
            ege::putpixels(pixelWriteOperations, pointTriples.data(), source);
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, (lastPixelWrite * 37) % kWidth,
                        (lastPixelWrite * 61) % kHeight,
                        ege::LIGHTRED, passed);
        }));

    results.push_back(runBenchmark(
        "putpixels_f_staging_committed", pixelWriteOperations,
        resetGpuFixtures,
        [&]() {
            ege::putpixels_f(pixelWriteOperations, pointTriples.data(), source);
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, (lastPixelWrite * 37) % kWidth,
                        (lastPixelWrite * 61) % kHeight,
                        ege::LIGHTRED, passed);
        }));

    const int alphaWriteOperations = 10000;
    const int lastAlphaWrite = alphaWriteOperations - 1;
    results.push_back(runBenchmark(
        "putpixel_withalpha_staging_committed", alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_withalpha(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    EGEARGB(128, 255, 0, 0), source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            const ege::color_t actual = ege::getpixel_f(
                (lastAlphaWrite * 37) % kWidth,
                (lastAlphaWrite * 61) % kHeight, destination);
            observedPixel ^= actual;
            passed = passed && !sameRgb(actual, ege::BLUE);
        }));

    results.push_back(runBenchmark(
        "putpixel_withalpha_f_staging_committed", alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_withalpha_f(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    EGEARGB(128, 0, 255, 0), source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            const ege::color_t actual = ege::getpixel_f(
                (lastAlphaWrite * 37) % kWidth,
                (lastAlphaWrite * 61) % kHeight, destination);
            observedPixel ^= actual;
            passed = passed && !sameRgb(actual, ege::BLUE);
        }));

    results.push_back(runBenchmark(
        "putpixel_savealpha_staging_committed", alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_savealpha(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    ege::MAGENTA, source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, (lastAlphaWrite * 37) % kWidth,
                        (lastAlphaWrite * 61) % kHeight,
                        ege::MAGENTA, passed);
        }));

    results.push_back(runBenchmark(
        "putpixel_savealpha_f_staging_committed", alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_savealpha_f(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    ege::MAGENTA, source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            expectPixel(destination, (lastAlphaWrite * 37) % kWidth,
                        (lastAlphaWrite * 61) % kHeight,
                        ege::MAGENTA, passed);
        }));

    results.push_back(runBenchmark(
        "putpixel_alphablend_staging_committed", alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_alphablend(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    EGEARGB(128, 255, 0, 0), source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            const ege::color_t actual = ege::getpixel_f(
                (lastAlphaWrite * 37) % kWidth,
                (lastAlphaWrite * 61) % kHeight, destination);
            observedPixel ^= actual;
            passed = passed && !sameRgb(actual, ege::BLUE);
        }));

    results.push_back(runBenchmark(
        "putpixel_alphablend_f_staging_committed", alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_alphablend_f(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    EGEARGB(128, 255, 0, 0), source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            const ege::color_t actual = ege::getpixel_f(
                (lastAlphaWrite * 37) % kWidth,
                (lastAlphaWrite * 61) % kHeight, destination);
            observedPixel ^= actual;
            passed = passed && !sameRgb(actual, ege::BLUE);
        }));

    results.push_back(runBenchmark(
        "putpixel_alphablend_factor_staging_committed",
        alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_alphablend(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    EGEARGB(192, 255, 0, 0), 96, source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            const ege::color_t actual = ege::getpixel_f(
                (lastAlphaWrite * 37) % kWidth,
                (lastAlphaWrite * 61) % kHeight, destination);
            observedPixel ^= actual;
            passed = passed && !sameRgb(actual, ege::BLUE);
        }));

    results.push_back(runBenchmark(
        "putpixel_alphablend_factor_f_staging_committed",
        alphaWriteOperations,
        resetGpuFixtures,
        [&]() {
            for (int i = 0; i < alphaWriteOperations; ++i) {
                ege::putpixel_alphablend_f(
                    (i * 37) % kWidth, (i * 61) % kHeight,
                    EGEARGB(192, 255, 0, 0), 96, source);
            }
            ege::putimage(destination, 0, 0, source);
            synchronizeImage(destination, passed);
        },
        [&]() {
            const ege::color_t actual = ege::getpixel_f(
                (lastAlphaWrite * 37) % kWidth,
                (lastAlphaWrite * 61) % kHeight, destination);
            observedPixel ^= actual;
            passed = passed && !sameRgb(actual, ege::BLUE);
        }));

    ege::PIMAGE accessImage = nullptr;
    ege::image_storage_mode accessModeBefore = ege::IMAGE_STORAGE_CPU_BITMAP;
    ege::color_t* accessPixels = nullptr;
    const auto resetAccessImage = [&]() {
        if (accessImage) ege::delimage(accessImage);
        accessImage = ege::newimage(kWidth, kHeight);
        passed = passed && accessImage != nullptr;
        if (accessImage) {
            clearImage(accessImage, ege::BLUE);
            accessModeBefore = ege::getimagestoragemode(accessImage);
        }
        accessPixels = nullptr;
    };

    results.push_back(runBenchmark(
        "getbuffer_read_first", 1,
        resetAccessImage,
        [&]() {
            if (accessImage) {
                accessPixels = ege::getbuffer(
                    accessImage, ege::IMAGE_BUFFER_READ);
                if (accessPixels) {
                    observedPixel ^= accessPixels[
                        (kHeight / 2) * kWidth + kWidth / 2];
                }
            }
        },
        [&]() {
            passed = passed && accessPixels != nullptr;
            if (accessImage) {
                passed = passed &&
                    ege::getimagestoragemode(accessImage) == accessModeBefore;
            }
        }));

    results.push_back(runBenchmark(
        "getbuffer_read_write_promotion", 1,
        resetAccessImage,
        [&]() {
            if (accessImage) {
                accessPixels = ege::getbuffer(
                    accessImage, ege::IMAGE_BUFFER_READ_WRITE);
                if (accessPixels) {
                    observedPixel ^= accessPixels[
                        (kHeight / 2) * kWidth + kWidth / 2];
                }
            }
        },
        [&]() {
            passed = passed && accessPixels != nullptr;
#ifdef _WIN32
            if (accessImage) {
                passed = passed && ege::getimagestoragemode(accessImage) ==
                                       ege::IMAGE_STORAGE_CPU_BITMAP;
            }
#else
            if (accessImage) {
                passed = passed &&
                    ege::getimagestoragemode(accessImage) == accessModeBefore;
            }
#endif
        }));

    results.push_back(runBenchmark(
        "getbuffer_write_discard_promotion", 1,
        resetAccessImage,
        [&]() {
            if (accessImage) {
                accessPixels = ege::getbuffer(
                    accessImage, ege::IMAGE_BUFFER_WRITE_DISCARD);
                if (accessPixels) {
                    accessPixels[(kHeight / 2) * kWidth + kWidth / 2] =
                        ege::WHITE;
                }
            }
        },
        [&]() {
            passed = passed && accessPixels != nullptr;
            if (accessImage && accessPixels) {
                expectPixel(accessImage, kWidth / 2, kHeight / 2,
                            ege::WHITE, passed);
            }
#ifdef _WIN32
            if (accessImage) {
                passed = passed && ege::getimagestoragemode(accessImage) ==
                                       ege::IMAGE_STORAGE_CPU_BITMAP;
            }
#endif
        }));

    if (accessImage) {
        ege::delimage(accessImage);
        accessImage = nullptr;
    }

    ege::PIMAGE cpuSource = ege::newimage(kWidth, kHeight);
    ege::PIMAGE cpuDestination = ege::newimage(kWidth, kHeight);
    ege::color_t* retainedPixels = cpuSource
        ? ege::getbuffer(cpuSource, ege::IMAGE_BUFFER_READ_WRITE) : nullptr;
    passed = passed && cpuSource != nullptr && cpuDestination != nullptr &&
             retainedPixels != nullptr;
    const bool hasPersistentCpuBitmap = cpuSource != nullptr &&
        ege::getimagestoragemode(cpuSource) == ege::IMAGE_STORAGE_CPU_BITMAP;

    if (hasPersistentCpuBitmap && cpuDestination && retainedPixels) {
        const auto resetCpuBitmapFixtures = [&]() {
            std::fill(retainedPixels,
                      retainedPixels + static_cast<size_t>(kWidth) * kHeight,
                      ege::BLUE);
            clearImage(cpuDestination, ege::BLACK);
            synchronizeImage(cpuDestination, passed);
        };

        results.push_back(runBenchmark(
            "cpu_bitmap_getpixel_cached", cachedReadOperations,
            resetCpuBitmapFixtures,
            [&]() {
                for (int i = 0; i < cachedReadOperations; ++i) {
                    observedPixel ^= ege::getpixel(
                        (i * 37) % kWidth, (i * 61) % kHeight, cpuSource);
                }
            },
            [&]() { expectPixel(cpuSource, 0, 0, ege::BLUE, passed); }));

        results.push_back(runBenchmark(
            "cpu_bitmap_putpixel_committed", pixelWriteOperations,
            resetCpuBitmapFixtures,
            [&]() {
                for (int i = 0; i < pixelWriteOperations; ++i) {
                    ege::putpixel((i * 37) % kWidth, (i * 61) % kHeight,
                                  ege::LIGHTGREEN, cpuSource);
                }
                ege::putimage(cpuDestination, 0, 0, cpuSource);
                synchronizeImage(cpuDestination, passed);
            },
            [&]() {
                expectPixel(cpuDestination,
                            (lastPixelWrite * 37) % kWidth,
                            (lastPixelWrite * 61) % kHeight,
                            ege::LIGHTGREEN, passed);
            }));

        results.push_back(runBenchmark(
            "cpu_bitmap_retained_writes_committed", pixelWriteOperations,
            resetCpuBitmapFixtures,
            [&]() {
                for (int i = 0; i < pixelWriteOperations; ++i) {
                    retainedPixels[((i * 61) % kHeight) * kWidth +
                                   ((i * 37) % kWidth)] = ege::LIGHTCYAN;
                }
                ege::putimage(cpuDestination, 0, 0, cpuSource);
                synchronizeImage(cpuDestination, passed);
            },
            [&]() {
                expectPixel(cpuDestination,
                            (lastPixelWrite * 37) % kWidth,
                            (lastPixelWrite * 61) % kHeight,
                            ege::LIGHTCYAN, passed);
            }));

        const int retainedUploadCycles = 20;
        results.push_back(runBenchmark(
            "cpu_bitmap_retained_1px_upload_cycles", retainedUploadCycles,
            resetCpuBitmapFixtures,
            [&]() {
                for (int i = 0; i < retainedUploadCycles; ++i) {
                    retainedPixels[7 * kWidth + i] = ege::LIGHTMAGENTA;
                    ege::putimage(cpuDestination, 0, 0, cpuSource);
                }
                synchronizeImage(cpuDestination, passed);
            },
            [&]() {
                expectPixel(cpuDestination, retainedUploadCycles - 1, 7,
                            ege::LIGHTMAGENTA, passed);
            }));
    }

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

    if (cpuDestination) ege::delimage(cpuDestination);
    if (cpuSource) ege::delimage(cpuSource);
    ege::delimage(destination);
    ege::delimage(source);
    framework.cleanup();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
