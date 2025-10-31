/*
 * EGE 图形绘制基础功能测试
 * 
 * 测试基础图形绘制函数的正确性
 */

#define SHOW_CONSOLE 1
#include "ege.h"
#include "../test_framework.h"
#include "../image_generator.h"

#include <iostream>
#include <cmath>

using namespace ege;

// 辅助函数：验证像素颜色（容差比较）
bool verifyPixelColor(PCIMAGE img, int x, int y, color_t expectedColor, int tolerance = 5) {
    color_t actualColor = getpixel(x, y, img);
    int rDiff = abs(EGEGET_R(actualColor) - EGEGET_R(expectedColor));
    int gDiff = abs(EGEGET_G(actualColor) - EGEGET_G(expectedColor));
    int bDiff = abs(EGEGET_B(actualColor) - EGEGET_B(expectedColor));
    
    return (rDiff <= tolerance && gDiff <= tolerance && bDiff <= tolerance);
}

// 测试直线绘制
bool testLineDrawing() {
    std::cout << "Testing line drawing..." << std::endl;
    
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试水平线
    setcolor(RED);
    line(10, 50, 190, 50, img);
    
    // 验证线上的点
    bool horizontalLineOk = verifyPixelColor(img, 100, 50, RED, 10);
    
    // 测试垂直线
    setcolor(BLUE);
    line(100, 10, 100, 190, img);
    
    bool verticalLineOk = verifyPixelColor(img, 100, 100, BLUE, 10);
    
    // 测试对角线
    setcolor(GREEN);
    line(10, 10, 190, 190, img);
    
    bool diagonalLineOk = verifyPixelColor(img, 100, 100, GREEN, 10);
    
    // 测试浮点版本
    setcolor(YELLOW);
    line_f(10.5f, 150.5f, 190.5f, 150.5f, img);
    
    bool floatLineOk = verifyPixelColor(img, 100, 150, YELLOW, 10);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = horizontalLineOk && verticalLineOk && diagonalLineOk && floatLineOk;
    std::cout << "  Line drawing: " << (result ? "PASS" : "FAIL") << std::endl;
    return result;
}

// 测试矩形绘制
bool testRectangleDrawing() {
    std::cout << "Testing rectangle drawing..." << std::endl;
    
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试空心矩形
    setcolor(RED);
    rectangle(20, 20, 180, 180, img);
    
    bool outlineOk = verifyPixelColor(img, 20, 20, RED, 10);
    bool insideEmpty = verifyPixelColor(img, 100, 100, WHITE, 10);
    
    // 测试填充矩形
    setfillcolor(BLUE);
    fillrectangle(30, 30, 80, 80, img);
    
    bool fillOk = verifyPixelColor(img, 55, 55, BLUE, 10);
    
    // 测试浮点版本
    setcolor(GREEN);
    rectangle_f(100.5f, 100.5f, 150.5f, 150.5f, img);
    
    bool floatRectOk = verifyPixelColor(img, 100, 100, GREEN, 10);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = outlineOk && insideEmpty && fillOk && floatRectOk;
    std::cout << "  Rectangle drawing: " << (result ? "PASS" : "FAIL") << std::endl;
    return result;
}

// 测试圆形绘制
bool testCircleDrawing() {
    std::cout << "Testing circle drawing..." << std::endl;
    
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试空心圆
    setcolor(RED);
    circle(100, 100, 50, img);
    
    // 验证圆周上的点
    bool outlineOk = verifyPixelColor(img, 150, 100, RED, 10);
    
    // 验证圆内部应该是背景色
    bool insideEmpty = verifyPixelColor(img, 100, 100, WHITE, 10);
    
    // 测试填充圆
    setfillcolor(BLUE);
    fillcircle(100, 100, 30, img);
    
    bool fillOk = verifyPixelColor(img, 100, 100, BLUE, 10);
    
    // 测试浮点版本
    setcolor(GREEN);
    circle_f(150.5f, 150.5f, 20.5f, img);
    
    bool floatCircleOk = verifyPixelColor(img, 171, 150, GREEN, 10);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = outlineOk && insideEmpty && fillOk && floatCircleOk;
    std::cout << "  Circle drawing: " << (result ? "PASS" : "FAIL") << std::endl;
    return result;
}

// 测试椭圆绘制
bool testEllipseDrawing() {
    std::cout << "Testing ellipse drawing..." << std::endl;
    
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试椭圆
    setcolor(RED);
    ellipse(100, 100, 0, 360, 60, 40, img);
    
    bool ellipseOk = verifyPixelColor(img, 160, 100, RED, 10);
    
    // 测试填充椭圆
    setfillcolor(BLUE);
    fillellipse(100, 100, 30, 20, img);
    
    bool fillOk = verifyPixelColor(img, 100, 100, BLUE, 10);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = ellipseOk && fillOk;
    std::cout << "  Ellipse drawing: " << (result ? "PASS" : "FAIL") << std::endl;
    return result;
}

// 测试圆弧绘制
bool testArcDrawing() {
    std::cout << "Testing arc drawing..." << std::endl;
    
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试圆弧（0-90度）
    setcolor(RED);
    arc(100, 100, 0, 90, 50, img);
    
    // 圆弧上应该有颜色
    bool arcOk = verifyPixelColor(img, 150, 100, RED, 10);
    
    // 测试扇形
    setfillcolor(BLUE);
    setcolor(BLUE);
    pie(100, 100, 180, 270, 40, 40, img);
    
    settarget(nullptr);
    delimage(img);
    
    std::cout << "  Arc drawing: " << (arcOk ? "PASS" : "FAIL") << std::endl;
    return arcOk;
}

// 测试bar和bar3d
bool testBarDrawing() {
    std::cout << "Testing bar drawing..." << std::endl;
    
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试2D bar
    setfillcolor(RED);
    bar(20, 20, 80, 80, img);
    
    bool barOk = verifyPixelColor(img, 50, 50, RED, 10);
    
    // 测试3D bar
    setfillcolor(BLUE);
    bar3d(100, 100, 160, 160, 10, 1, img);
    
    bool bar3dOk = verifyPixelColor(img, 130, 130, BLUE, 10);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = barOk && bar3dOk;
    std::cout << "  Bar drawing: " << (result ? "PASS" : "FAIL") << std::endl;
    return result;
}

// 测试pie和扇形
bool testPieDrawing() {
    std::cout << "Testing pie drawing..." << std::endl;
    
    PIMAGE img = newimage(200, 200);
    settarget(img);
    setbkcolor(WHITE);
    cleardevice();
    
    // 测试空心扇形
    setcolor(RED);
    pie(100, 100, 0, 90, 50, 50, img);
    
    // 测试填充扇形
    setfillcolor(BLUE);
    fillpie(100, 100, 90, 180, 40, 40, img);
    
    bool fillPieOk = verifyPixelColor(img, 100, 140, BLUE, 10);
    
    // 测试实心扇形
    setfillcolor(GREEN);
    solidpie(100, 100, 180, 270, 30, 30, img);
    
    bool solidPieOk = verifyPixelColor(img, 70, 100, GREEN, 10);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = fillPieOk && solidPieOk;
    std::cout << "  Pie drawing: " << (result ? "PASS" : "FAIL") << std::endl;
    return result;
}

int main() {
    TestFramework framework;
    
    if (!framework.initialize(800, 600)) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    framework.hideWindow();
    
    std::cout << "==================================" << std::endl;
    std::cout << "Drawing Primitives Functional Test" << std::endl;
    std::cout << "==================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // 运行所有测试
    total++; if (testLineDrawing()) passed++;
    total++; if (testRectangleDrawing()) passed++;
    total++; if (testCircleDrawing()) passed++;
    total++; if (testEllipseDrawing()) passed++;
    total++; if (testArcDrawing()) passed++;
    total++; if (testBarDrawing()) passed++;
    total++; if (testPieDrawing()) passed++;
    
    std::cout << "\n==================================" << std::endl;
    std::cout << "Results: " << passed << "/" << total << " tests passed" << std::endl;
    std::cout << "Success rate: " << (100.0 * passed / total) << "%" << std::endl;
    std::cout << "==================================" << std::endl;
    
    framework.cleanup();
    
    return (passed == total) ? 0 : 1;
}
