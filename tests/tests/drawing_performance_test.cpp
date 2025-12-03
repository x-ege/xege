/*
 * EGE 图形绘制性能测试
 * 
 * 测试各种图形绘制函数的性能
 */

#define SHOW_CONSOLE 1
#include "ege.h"
#include "../test_framework.h"
#include "../performance_timer.h"

#include <iostream>
#include <iomanip>

using namespace ege;

const int ITERATIONS = 1000;
const int WARMUP_ITERATIONS = 100;

// 性能测试辅助函数
void printPerformanceResult(const std::string& testName, const BatchPerformanceTest& test) {
    std::cout << "  " << std::setw(40) << std::left << testName
              << " Avg: " << std::setw(8) << std::fixed << std::setprecision(3) << test.getAverageMs() << " ms"
              << " | Ops/sec: " << std::setw(10) << std::fixed << std::setprecision(1) << test.getOperationsPerSecond()
              << " | Min: " << std::setw(6) << std::fixed << std::setprecision(3) << test.getMinMs() << " ms"
              << " | Max: " << std::setw(6) << std::fixed << std::setprecision(3) << test.getMaxMs() << " ms"
              << std::endl;
}

// 测试line性能
void testLinePerformance() {
    std::cout << "\n=== Line Drawing Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    
    // 水平线
    BatchPerformanceTest test1("Horizontal line", ITERATIONS);
    test1.runBatch([&]() {
        line(100, 300, 700, 300, img);
    }, ITERATIONS);
    printPerformanceResult("Horizontal line", test1);
    
    // 垂直线
    BatchPerformanceTest test2("Vertical line", ITERATIONS);
    test2.runBatch([&]() {
        line(400, 100, 400, 500, img);
    }, ITERATIONS);
    printPerformanceResult("Vertical line", test2);
    
    // 对角线
    BatchPerformanceTest test3("Diagonal line", ITERATIONS);
    test3.runBatch([&]() {
        line(100, 100, 700, 500, img);
    }, ITERATIONS);
    printPerformanceResult("Diagonal line", test3);
    
    // 浮点版本
    BatchPerformanceTest test4("line_f", ITERATIONS);
    test4.runBatch([&]() {
        line_f(100.5f, 100.5f, 700.5f, 500.5f, img);
    }, ITERATIONS);
    printPerformanceResult("Floating-point line", test4);
    
    settarget(nullptr);
    delimage(img);
}

// 测试rectangle性能
void testRectanglePerformance() {
    std::cout << "\n=== Rectangle Drawing Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    
    // 空心矩形
    BatchPerformanceTest test1("Rectangle outline", ITERATIONS);
    test1.runBatch([&]() {
        rectangle(100, 100, 700, 500, img);
    }, ITERATIONS);
    printPerformanceResult("Rectangle outline", test1);
    
    // 小矩形
    BatchPerformanceTest test2("Small rectangle", ITERATIONS);
    test2.runBatch([&]() {
        rectangle(300, 250, 500, 350, img);
    }, ITERATIONS);
    printPerformanceResult("Small rectangle (200x100)", test2);
    
    // 填充矩形
    BatchPerformanceTest test3("Filled rectangle", ITERATIONS);
    test3.runBatch([&]() {
        fillrectangle(100, 100, 700, 500, img);
    }, ITERATIONS);
    printPerformanceResult("Filled rectangle", test3);
    
    // 小填充矩形
    BatchPerformanceTest test4("Small filled rect", ITERATIONS);
    test4.runBatch([&]() {
        fillrectangle(300, 250, 500, 350, img);
    }, ITERATIONS);
    printPerformanceResult("Small filled rect (200x100)", test4);
    
    // bar
    BatchPerformanceTest test5("bar", ITERATIONS);
    test5.runBatch([&]() {
        bar(100, 100, 700, 500, img);
    }, ITERATIONS);
    printPerformanceResult("bar", test5);
    
    // bar3d
    BatchPerformanceTest test6("bar3d", ITERATIONS);
    test6.runBatch([&]() {
        bar3d(100, 100, 700, 500, 10, 1, img);
    }, ITERATIONS);
    printPerformanceResult("bar3d", test6);
    
    settarget(nullptr);
    delimage(img);
}

// 测试circle性能
void testCirclePerformance() {
    std::cout << "\n=== Circle Drawing Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    
    // 空心圆
    BatchPerformanceTest test1("Circle outline", ITERATIONS);
    test1.runBatch([&]() {
        circle(400, 300, 150, img);
    }, ITERATIONS);
    printPerformanceResult("Circle outline (r=150)", test1);
    
    // 小圆
    BatchPerformanceTest test2("Small circle", ITERATIONS);
    test2.runBatch([&]() {
        circle(400, 300, 50, img);
    }, ITERATIONS);
    printPerformanceResult("Small circle (r=50)", test2);
    
    // 填充圆
    BatchPerformanceTest test3("Filled circle", ITERATIONS);
    test3.runBatch([&]() {
        fillcircle(400, 300, 150, img);
    }, ITERATIONS);
    printPerformanceResult("Filled circle (r=150)", test3);
    
    // 小填充圆
    BatchPerformanceTest test4("Small filled", ITERATIONS);
    test4.runBatch([&]() {
        fillcircle(400, 300, 50, img);
    }, ITERATIONS);
    printPerformanceResult("Small filled circle (r=50)", test4);
    
    settarget(nullptr);
    delimage(img);
}

// 测试ellipse性能
void testEllipsePerformance() {
    std::cout << "\n=== Ellipse Drawing Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    
    // 完整椭圆
    BatchPerformanceTest test1("Full ellipse", ITERATIONS);
    test1.runBatch([&]() {
        ellipse(400, 300, 0, 360, 200, 100, img);
    }, ITERATIONS);
    printPerformanceResult("Full ellipse", test1);
    
    // 椭圆弧
    BatchPerformanceTest test2("Ellipse arc", ITERATIONS);
    test2.runBatch([&]() {
        ellipse(400, 300, 0, 90, 200, 100, img);
    }, ITERATIONS);
    printPerformanceResult("Ellipse arc (0-90°)", test2);
    
    // 填充椭圆
    BatchPerformanceTest test3("Filled ellipse", ITERATIONS);
    test3.runBatch([&]() {
        fillellipse(400, 300, 200, 100, img);
    }, ITERATIONS);
    printPerformanceResult("Filled ellipse", test3);
    
    settarget(nullptr);
    delimage(img);
}

// 测试arc和pie性能
void testArcPiePerformance() {
    std::cout << "\n=== Arc and Pie Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    
    // 圆弧
    BatchPerformanceTest test1("Arc", ITERATIONS);
    test1.runBatch([&]() {
        arc(400, 300, 0, 90, 150, img);
    }, ITERATIONS);
    printPerformanceResult("Arc (0-90°)", test1);
    
    // 扇形
    BatchPerformanceTest test2("Pie", ITERATIONS);
    test2.runBatch([&]() {
        pie(400, 300, 0, 90, 150, 150, img);
    }, ITERATIONS);
    printPerformanceResult("Pie (0-90°)", test2);
    
    // 填充扇形
    BatchPerformanceTest test3("Filled pie", ITERATIONS);
    test3.runBatch([&]() {
        fillpie(400, 300, 0, 90, 150, 150, img);
    }, ITERATIONS);
    printPerformanceResult("Filled pie (0-90°)", test3);
    
    // 实心扇形
    BatchPerformanceTest test4("Solid pie", ITERATIONS);
    test4.runBatch([&]() {
        solidpie(400, 300, 0, 90, 150, 150, img);
    }, ITERATIONS);
    printPerformanceResult("Solid pie (0-90°)", test4);
    
    // pieslice
    BatchPerformanceTest test5("Pieslice", ITERATIONS);
    test5.runBatch([&]() {
        pieslice(400, 300, 0, 90, 150, img);
    }, ITERATIONS);
    printPerformanceResult("Pieslice (0-90°)", test5);
    
    settarget(nullptr);
    delimage(img);
}

// 测试复杂场景性能
void testComplexScenePerformance() {
    std::cout << "\n=== Complex Scene Performance ===" << std::endl;
    
    PIMAGE img = newimage(800, 600);
    settarget(img);
    
    // 多个图形组合
    BatchPerformanceTest test1("Complex scene", 100);
    test1.runBatch([&]() {
        cleardevice();
        for (int i = 0; i < 10; i++) {
            setcolor(RGB(i * 25, 128, 255 - i * 25));
            circle(400, 300, 50 + i * 10, img);
        }
        for (int i = 0; i < 5; i++) {
            setfillcolor(RGB(255 - i * 50, i * 50, 128));
            fillrectangle(100 + i * 50, 100, 150 + i * 50, 500, img);
        }
    }, 100);
    printPerformanceResult("Complex scene (10 circles + 5 rects)", test1);
    
    settarget(nullptr);
    delimage(img);
}

int main() {
    TestFramework framework;
    
    if (!framework.initialize(800, 600)) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    framework.hideWindow();
    
    std::cout << "======================================" << std::endl;
    std::cout << "Drawing Primitives Performance Test" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Iterations per test: " << ITERATIONS << std::endl;
    
    testLinePerformance();
    testRectanglePerformance();
    testCirclePerformance();
    testEllipsePerformance();
    testArcPiePerformance();
    testComplexScenePerformance();
    
    std::cout << "\n======================================" << std::endl;
    std::cout << "Performance test completed" << std::endl;
    std::cout << "======================================" << std::endl;
    
    framework.cleanup();
    
    return 0;
}
