/*
 * EGE 颜色操作功能测试
 * 
 * 测试颜色设置、获取和转换函数的正确性
 */

#define SHOW_CONSOLE 1
#include "ege.h"
#include "../test_framework.h"

#include <iostream>
#include <cmath>
#include <cstdlib>

using namespace ege;

// 测试基础颜色设置和获取
bool testBasicColorOperations() {
    std::cout << "Testing basic color operations..." << std::endl;
    
    PIMAGE img = newimage(100, 100);
    settarget(img);
    
    // 测试setcolor/getcolor
    setcolor(RED, img);
    color_t c1 = getcolor(img);
    bool redOk = (c1 == RED);
    
    // 测试setlinecolor/getlinecolor
    setlinecolor(BLUE, img);
    color_t c2 = getlinecolor(img);
    bool lineColorOk = (c2 == BLUE);
    
    // 测试setfillcolor/getfillcolor
    setfillcolor(GREEN, img);
    color_t c3 = getfillcolor(img);
    bool fillColorOk = (c3 == GREEN);
    
    // 测试setbkcolor/getbkcolor
    setbkcolor(YELLOW, img);
    color_t c4 = getbkcolor(img);
    bool bkColorOk = (c4 == YELLOW);
    
    // 测试settextcolor
    settextcolor(CYAN, img);
    
    // 测试setfontbkcolor
    setfontbkcolor(MAGENTA, img);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = redOk && lineColorOk && fillColorOk && bkColorOk;
    std::cout << "  Basic color operations: " << (result ? "PASS" : "FAIL") << std::endl;
    std::cout << "    setcolor/getcolor: " << (redOk ? "✓" : "✗") << std::endl;
    std::cout << "    setlinecolor/getlinecolor: " << (lineColorOk ? "✓" : "✗") << std::endl;
    std::cout << "    setfillcolor/getfillcolor: " << (fillColorOk ? "✓" : "✗") << std::endl;
    std::cout << "    setbkcolor/getbkcolor: " << (bkColorOk ? "✓" : "✗") << std::endl;
    
    return result;
}

// 测试RGB颜色创建和分解
bool testRGBOperations() {
    std::cout << "Testing RGB operations..." << std::endl;
    
    // 测试RGB宏
    color_t red = RGB(255, 0, 0);
    color_t green = RGB(0, 255, 0);
    color_t blue = RGB(0, 0, 255);
    color_t custom = RGB(128, 64, 192);
    
    // 测试EGERGB
    color_t egered = EGERGB(255, 0, 0);
    bool egergbOk = (egered == red);
    
    // 测试EGERGBA (带alpha)
    color_t colorWithAlpha = EGERGBA(255, 0, 0, 128);
    
    // 测试EGEARGB
    color_t argbColor = EGEARGB(128, 255, 0, 0);
    
    // 测试颜色分量提取 (alpha)
    int alpha = EGEGET_A(colorWithAlpha);
    bool alphaOk = (alpha == 128);
    
    // 测试颜色分量提取
    int r = EGEGET_R(custom);
    int g = EGEGET_G(custom);
    int b = EGEGET_B(custom);
    
    bool rgbExtractOk = (r == 128 && g == 64 && b == 192);
    
    // 测试alpha分量
    color_t alphaTest = EGERGBA(100, 150, 200, 250);
    int aTest = EGEGET_A(alphaTest);
    bool alphaExtractOk = (aTest == 250);
    
    bool result = egergbOk && alphaOk && rgbExtractOk && alphaExtractOk;
    std::cout << "  RGB operations: " << (result ? "PASS" : "FAIL") << std::endl;
    std::cout << "    EGERGB: " << (egergbOk ? "✓" : "✗") << std::endl;
    std::cout << "    Alpha extraction: " << (alphaOk ? "✓" : "✗") << std::endl;
    std::cout << "    RGB extraction: " << (rgbExtractOk ? "✓" : "✗") << std::endl;
    std::cout << "    Alpha component: " << (alphaExtractOk ? "✓" : "✗") << std::endl;
    
    return result;
}

// 辅助函数：比较浮点数（带容差）
bool floatEqual(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) < epsilon;
}

// 测试HSV颜色空间转换
bool testHSVConversion() {
    std::cout << "Testing HSV color space conversion..." << std::endl;
    
    // 测试纯红色 RGB(255, 0, 0) -> HSV(0, 1, 1)
    float h, s, v;
    ege::rgb2hsv(RGB(255, 0, 0), &h, &s, &v);
    bool redHSVOk = floatEqual(h, 0.0f) && floatEqual(s, 1.0f) && floatEqual(v, 1.0f);
    
    // 测试纯绿色 RGB(0, 255, 0) -> HSV(120, 1, 1)
    ege::rgb2hsv(RGB(0, 255, 0), &h, &s, &v);
    bool greenHSVOk = floatEqual(h, 120.0f) && floatEqual(s, 1.0f) && floatEqual(v, 1.0f);
    
    // 测试纯蓝色 RGB(0, 0, 255) -> HSV(240, 1, 1)
    ege::rgb2hsv(RGB(0, 0, 255), &h, &s, &v);
    bool blueHSVOk = floatEqual(h, 240.0f) && floatEqual(s, 1.0f) && floatEqual(v, 1.0f);
    
    // 测试灰色 RGB(128, 128, 128) -> HSV(0, 0, ~0.5)
    ege::rgb2hsv(RGB(128, 128, 128), &h, &s, &v);
    bool grayHSVOk = floatEqual(s, 0.0f);
    
    // 测试逆转换 HSV -> RGB
    color_t rgbColor = ege::hsv2rgb(0.0f, 1.0f, 1.0f);
    int r = EGEGET_R(rgbColor);
    int g = EGEGET_G(rgbColor);
    int b = EGEGET_B(rgbColor);
    bool hsvToRgbOk = (r == 255 && g == 0 && b == 0);
    
    bool result = redHSVOk && greenHSVOk && blueHSVOk && grayHSVOk && hsvToRgbOk;
    std::cout << "  HSV conversion: " << (result ? "PASS" : "FAIL") << std::endl;
    std::cout << "    RGB to HSV (red): " << (redHSVOk ? "✓" : "✗") << std::endl;
    std::cout << "    RGB to HSV (green): " << (greenHSVOk ? "✓" : "✗") << std::endl;
    std::cout << "    RGB to HSV (blue): " << (blueHSVOk ? "✓" : "✗") << std::endl;
    std::cout << "    RGB to HSV (gray): " << (grayHSVOk ? "✓" : "✗") << std::endl;
    std::cout << "    HSV to RGB: " << (hsvToRgbOk ? "✓" : "✗") << std::endl;
    
    return result;
}

// 测试HSL颜色空间转换
bool testHSLConversion() {
    std::cout << "Testing HSL color space conversion..." << std::endl;
    
    // 测试纯红色 RGB(255, 0, 0) -> HSL(0, 1, 0.5)
    float h, s, l;
    ege::rgb2hsl(RGB(255, 0, 0), &h, &s, &l);
    bool redHSLOk = floatEqual(h, 0.0f) && floatEqual(s, 1.0f) && floatEqual(l, 0.5f);
    
    // 测试灰色 RGB(128, 128, 128) -> HSL(0, 0, 0.5)
    ege::rgb2hsl(RGB(128, 128, 128), &h, &s, &l);
    bool grayHSLOk = floatEqual(s, 0.0f) && floatEqual(l, 0.5f, 0.05f);
    
    // 测试逆转换 HSL -> RGB
    color_t rgbColor1 = ege::hsl2rgb(0.0f, 1.0f, 0.5f);
    int r1 = EGEGET_R(rgbColor1);
    int g1 = EGEGET_G(rgbColor1);
    int b1 = EGEGET_B(rgbColor1);
    bool hslToRgbOk = (r1 == 255 && g1 == 0 && b1 == 0);
    
    // 测试白色
    color_t rgbColor2 = ege::hsl2rgb(0.0f, 0.0f, 1.0f);
    int r2 = EGEGET_R(rgbColor2);
    int g2 = EGEGET_G(rgbColor2);
    int b2 = EGEGET_B(rgbColor2);
    bool whiteOk = (r2 == 255 && g2 == 255 && b2 == 255);
    
    // 测试黑色
    color_t rgbColor3 = ege::hsl2rgb(0.0f, 0.0f, 0.0f);
    int r3 = EGEGET_R(rgbColor3);
    int g3 = EGEGET_G(rgbColor3);
    int b3 = EGEGET_B(rgbColor3);
    bool blackOk = (r3 == 0 && g3 == 0 && b3 == 0);
    
    bool result = redHSLOk && grayHSLOk && hslToRgbOk && whiteOk && blackOk;
    std::cout << "  HSL conversion: " << (result ? "PASS" : "FAIL") << std::endl;
    std::cout << "    RGB to HSL (red): " << (redHSLOk ? "✓" : "✗") << std::endl;
    std::cout << "    RGB to HSL (gray): " << (grayHSLOk ? "✓" : "✗") << std::endl;
    std::cout << "    HSL to RGB (red): " << (hslToRgbOk ? "✓" : "✗") << std::endl;
    std::cout << "    HSL to RGB (white): " << (whiteOk ? "✓" : "✗") << std::endl;
    std::cout << "    HSL to RGB (black): " << (blackOk ? "✓" : "✗") << std::endl;
    
    return result;
}

// 测试预定义颜色常量
bool testPredefinedColors() {
    std::cout << "Testing predefined colors..." << std::endl;
    
    // 验证一些基本颜色常量是否正确定义
    bool blackOk = (BLACK == RGB(0, 0, 0));
    bool whiteOk = (WHITE == RGB(255, 255, 255));
    bool redOk = (RED == RGB(255, 0, 0));
    bool greenOk = (GREEN == RGB(0, 255, 0));
    bool blueOk = (BLUE == RGB(0, 0, 255));
    bool yellowOk = (YELLOW == RGB(255, 255, 0));
    bool cyanOk = (CYAN == RGB(0, 255, 255));
    bool magentaOk = (MAGENTA == RGB(255, 0, 255));
    
    bool result = blackOk && whiteOk && redOk && greenOk && 
                  blueOk && yellowOk && cyanOk && magentaOk;
    
    std::cout << "  Predefined colors: " << (result ? "PASS" : "FAIL") << std::endl;
    
    return result;
}

// 测试边界条件
bool testBoundaryConditions() {
    std::cout << "Testing boundary conditions..." << std::endl;
    
    // 测试最大最小值
    color_t minColor = RGB(0, 0, 0);
    color_t maxColor = RGB(255, 255, 255);
    
    int r1 = EGEGET_R(minColor);
    int g1 = EGEGET_G(minColor);
    int b1 = EGEGET_B(minColor);
    bool minOk = (r1 == 0 && g1 == 0 && b1 == 0);
    
    int r2 = EGEGET_R(maxColor);
    int g2 = EGEGET_G(maxColor);
    int b2 = EGEGET_B(maxColor);
    bool maxOk = (r2 == 255 && g2 == 255 && b2 == 255);
    
    // 测试alpha边界
    color_t alphaMin = EGERGBA(100, 100, 100, 0);
    color_t alphaMax = EGERGBA(100, 100, 100, 255);
    
    bool alphaMinOk = (EGEGET_A(alphaMin) == 0);
    bool alphaMaxOk = (EGEGET_A(alphaMax) == 255);
    
    bool result = minOk && maxOk && alphaMinOk && alphaMaxOk;
    std::cout << "  Boundary conditions: " << (result ? "PASS" : "FAIL") << std::endl;
    
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
    std::cout << "Color Operations Functional Test" << std::endl;
    std::cout << "==================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // 运行所有测试
    total++; if (testBasicColorOperations()) passed++;
    total++; if (testRGBOperations()) passed++;
    total++; if (testHSVConversion()) passed++;
    total++; if (testHSLConversion()) passed++;
    total++; if (testPredefinedColors()) passed++;
    total++; if (testBoundaryConditions()) passed++;
    
    std::cout << "\n==================================" << std::endl;
    std::cout << "Results: " << passed << "/" << total << " tests passed" << std::endl;
    std::cout << "Success rate: " << (100.0 * passed / total) << "%" << std::endl;
    std::cout << "==================================" << std::endl;
    
    framework.cleanup();
    
    return (passed == total) ? 0 : 1;
}
