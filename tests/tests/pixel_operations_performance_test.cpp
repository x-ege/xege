/*
 * EGE 像素操作性能测试
 * 
 * 测试像素级操作的性能
 */

#define SHOW_CONSOLE 1
#include "ege.h"
#include "../test_framework.h"
#include "../performance_timer.h"

#include <iostream>
#include <iomanip>

using namespace ege;

const int PIXEL_ITERATIONS = 10000;
const int BATCH_ITERATIONS = 100;

// 性能测试辅助函数
void printPerformanceResult(const std::string& testName, const BatchPerformanceTest& test) {
    std::cout << "  " << std::setw(45) << std::left << testName
              << " Avg: " << std::setw(8) << std::fixed << std::setprecision(3) << test.getAverageMs() << " ms"
              << " | Ops/sec: " << std::setw(10) << std::fixed << std::setprecision(1) << test.getOperationsPerSecond()
              << std::endl;
}

// 测试单像素读写性能
void testSinglePixelOperations() {
    std::cout << "\n=== Single Pixel Operations ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    setbkcolor(BLACK);
    cleardevice();
    
    // 测试putpixel
    BatchPerformanceTest test1("putpixel", PIXEL_ITERATIONS);
    test1.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            putpixel(i % 800, (i / 800) % 600, RED, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("putpixel (10000 pixels)", test1);
    
    // 测试putpixel_f
    BatchPerformanceTest test2("putpixel_f", PIXEL_ITERATIONS);
    test2.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            putpixel_f(i % 800, (i / 800) % 600, GREEN, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("putpixel_f (10000 pixels)", test2);
    
    // 测试getpixel
    BatchPerformanceTest test3("getpixel", PIXEL_ITERATIONS);
    volatile color_t dummy;
    test3.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            dummy = getpixel(i % 800, (i / 800) % 600, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("getpixel (10000 pixels)", test3);
    
    // 测试getpixel_f
    BatchPerformanceTest test4("getpixel_f", PIXEL_ITERATIONS);
    test4.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            dummy = getpixel_f(i % 800, (i / 800) % 600, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("getpixel_f (10000 pixels)", test4);
    
    settarget(nullptr);
    delimage(img);
}

// 测试Alpha混合像素操作
void testAlphaBlendPixelOperations() {
    std::cout << "\n=== Alpha Blend Pixel Operations ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试putpixel_withalpha
    BatchPerformanceTest test1("putpixel_withalpha", PIXEL_ITERATIONS);
    test1.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            putpixel_withalpha(i % 800, (i / 800) % 600, EGERGBA(255, 0, 0, 128), img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("putpixel_withalpha (10000 pixels)", test1);
    
    // 测试putpixel_withalpha_f
    BatchPerformanceTest test2("putpixel_withalpha_f", PIXEL_ITERATIONS);
    test2.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            putpixel_withalpha_f(i % 800, (i / 800) % 600, EGERGBA(0, 255, 0, 128), img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("putpixel_withalpha_f (10000 pixels)", test2);
    
    // 测试putpixel_alphablend (带alpha参数)
    BatchPerformanceTest test3("putpixel_alphablend", PIXEL_ITERATIONS);
    test3.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            putpixel_alphablend(i % 800, (i / 800) % 600, RGB(0, 0, 255), 128, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("putpixel_alphablend (10000 pixels)", test3);
    
    // 测试putpixel_alphablend_f
    BatchPerformanceTest test4("putpixel_alphablend_f", PIXEL_ITERATIONS);
    test4.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            putpixel_alphablend_f(i % 800, (i / 800) % 600, RGB(255, 255, 0), 128, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("putpixel_alphablend_f (10000 pixels)", test4);
    
    settarget(nullptr);
    delimage(img);
}

// 测试不同Alpha值的性能
void testAlphaValuePerformance() {
    std::cout << "\n=== Alpha Value Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    const int alphaValues[] = {0, 64, 128, 192, 255};
    
    for (int alpha : alphaValues) {
        std::string testName = "Alpha=" + std::to_string(alpha);
        BatchPerformanceTest test(testName, PIXEL_ITERATIONS);
        test.runBatch([&]() {
            for (int i = 0; i < PIXEL_ITERATIONS; i++) {
                putpixel_alphablend(i % 800, (i / 800) % 600, RED, (unsigned char)alpha, img);
            }
        }, BATCH_ITERATIONS);
        printPerformanceResult(testName + " (10000 pixels)", test);
    }
    
    settarget(nullptr);
    delimage(img);
}

// 测试连续区域填充性能
void testAreaFillPerformance() {
    std::cout << "\n=== Area Fill Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    
    // 填充整个图像 (逐像素)
    BatchPerformanceTest test1("Full image fill", 1);
    test1.runBatch([&]() {
        for (int y = 0; y < 600; y++) {
            for (int x = 0; x < 800; x++) {
                putpixel(x, y, RGB(x % 256, y % 256, (x + y) % 256), img);
            }
        }
    }, 10);
    printPerformanceResult("Full image fill (800x600, pixel-by-pixel)", test1);
    
    // 填充一个小区域
    BatchPerformanceTest test2("Small area fill", 1);
    test2.runBatch([&]() {
        for (int y = 200; y < 400; y++) {
            for (int x = 300; x < 500; x++) {
                putpixel(x, y, BLUE, img);
            }
        }
    }, 100);
    printPerformanceResult("Small area fill (200x200, pixel-by-pixel)", test2);
    
    settarget(nullptr);
    delimage(img);
}

// 测试随机访问vs顺序访问
void testAccessPatternPerformance() {
    std::cout << "\n=== Access Pattern Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    setbkcolor(BLACK);
    cleardevice();
    
    // 顺序访问
    BatchPerformanceTest test1("Sequential access", PIXEL_ITERATIONS);
    test1.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            int x = i % 800;
            int y = (i / 800) % 600;
            putpixel(x, y, RED, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("Sequential access (10000 pixels)", test1);
    
    // 随机访问
    BatchPerformanceTest test2("Random access", PIXEL_ITERATIONS);
    test2.runBatch([&]() {
        for (int i = 0; i < PIXEL_ITERATIONS; i++) {
            int x = (i * 7919) % 800;  // 使用质数产生伪随机
            int y = (i * 7927) % 600;
            putpixel(x, y, BLUE, img);
        }
    }, BATCH_ITERATIONS);
    printPerformanceResult("Random access (10000 pixels)", test2);
    
    settarget(nullptr);
    delimage(img);
}

// 测试不同分辨率的性能
void testResolutionPerformance() {
    std::cout << "\n=== Resolution Performance ===" << std::endl;
    
    struct ResolutionTest {
        int width;
        int height;
        std::string name;
    };
    
    ResolutionTest resolutions[] = {
        {320, 240, "320x240"},
        {640, 480, "640x480"},
        {800, 600, "800x600"},
        {1280, 720, "1280x720"},
        {1920, 1080, "1920x1080"}
    };
    
    for (const auto& res : resolutions) {
        PIMAGE img = newimage(res.width, res.height);
        settarget(img);
        
        BatchPerformanceTest test(res.name, 1);
        test.runBatch([&]() {
            for (int i = 0; i < PIXEL_ITERATIONS; i++) {
                putpixel(i % res.width, (i / res.width) % res.height, RED, img);
            }
        }, 50);
        
        printPerformanceResult(res.name + " (10000 pixels)", test);
        
        settarget(nullptr);
        delimage(img);
    }
}

int main() {
    TestFramework framework;
    
    if (!framework.initialize(800, 600)) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    framework.hideWindow();
    
    std::cout << "========================================" << std::endl;
    std::cout << "Pixel Operations Performance Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Pixels per batch: " << PIXEL_ITERATIONS << std::endl;
    std::cout << "Batch iterations: " << BATCH_ITERATIONS << std::endl;
    
    testSinglePixelOperations();
    testAlphaBlendPixelOperations();
    testAlphaValuePerformance();
    testAreaFillPerformance();
    testAccessPatternPerformance();
    testResolutionPerformance();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Performance test completed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    framework.cleanup();
    
    return 0;
}
