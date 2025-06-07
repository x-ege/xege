/*
 * putimage_alphablend 透明混合性能测试
 */

#define SHOW_CONSOLE 1
#include "ege.h"
#include "../image_generator.h"
#include "../performance_timer.h"
#include "../test_framework.h"

#include <iostream>

using namespace ege;

int main() {
    TestFramework framework;

    if (!framework.initialize()) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }

    framework.hideWindow();

    std::cout << "putimage_alphablend Performance Test" << std::endl;
    std::cout << "====================================" << std::endl;

    // 测试不同分辨率的alphablend性能
    auto resolutions = TestFramework::getTestResolutions();

    for (const auto& res : resolutions) {
        std::cout << "\nTesting resolution: " << res.width << "x" << res.height << std::endl;

        setcolor(BLUE);
        setbkcolor(WHITE);
        cleardevice(); // 生成测试图像
        auto srcImg = ImageGenerator::createGradientImage(res.width, res.height, RED, BLUE);
        auto bgImg = ImageGenerator::createSolidImage(res.width, res.height, WHITE); // 先绘制背景
        ege::putimage(0, 0, bgImg);

        // 测试不同alpha值的性能
        std::vector<int> alphaValues = { 64, 128, 192, 255 };

        for (int alpha : alphaValues) {
            std::cout << "  Alpha value: " << alpha << std::endl;

            PerformanceTimer timer;
            const int iterations = 100;
            timer.start();
            for (int i = 0; i < iterations; i++) {
                ege::putimage_alphablend(nullptr, srcImg, 0, 0, (unsigned char)alpha);
            }
            timer.stop();

            double avgTime = timer.getElapsedMs() / iterations;
            double fps = 1000.0 / avgTime;

            std::cout << "    Average time: " << avgTime << " ms" << std::endl;
            std::cout << "    FPS: " << fps << std::endl;
            std::cout << "    Operations/sec: " << (1000.0 / avgTime) << std::endl;
        }

        // 测试不同混合模式
        std::cout << "  Testing blend modes:" << std::endl;

        PerformanceTimer timer;
        const int iterations = 50;

        // Alpha blend with different positions
        timer.start();
        for (int i = 0; i < iterations; i++) {
            int x = i % 10;
            int y = i % 10;
            ege::putimage_alphablend(nullptr, srcImg, x, y, (unsigned char)128);
        }
        timer.stop();

        double avgTime = timer.getElapsedMs() / iterations;
        std::cout << "    Positioned alphablend - Avg time: " << avgTime << " ms" << std::endl;
        std::cout << "    Operations/sec: " << (1000.0 / avgTime) << std::endl;

        // Alpha blend with different source rectangles
        timer.reset();
        timer.start();
        for (int i = 0; i < iterations; i++) {
            int srcX = (i * 10) % (res.width / 2);
            int srcY = (i * 10) % (res.height / 2);
            ege::putimage_alphablend(nullptr, srcImg, srcX, srcY, (unsigned char)128);
        }
        timer.stop();        avgTime = timer.getElapsedMs() / iterations;
        std::cout << "    Partial alphablend - Avg time: " << avgTime << " ms" << std::endl;
        std::cout << "    Operations/sec: " << (1000.0 / avgTime) << std::endl;
    }
    
    std::cout << "\nAlphablend performance test completed!" << std::endl;

    framework.cleanup();
    return 0;
}
