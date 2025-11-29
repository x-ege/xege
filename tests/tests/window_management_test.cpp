/*
 * EGE 窗口管理功能测试
 * 
 * 测试窗口初始化、尺寸、标题、可见性等功能
 */

#define SHOW_CONSOLE 1
#include "ege.h"
#include "../test_framework.h"

#include <iostream>
#include <string>

using namespace ege;

// 测试基础窗口操作（使用TestFramework）
bool testBasicWindowOperations() {
    std::cout << "Testing basic window operations..." << std::endl;
    
    // TestFramework已经初始化了窗口，我们测试is_run
    bool isRunning = is_run();
    
    std::cout << "  Basic window operations: " << (isRunning ? "PASS" : "FAIL") << std::endl;
    std::cout << "    is_run: " << (isRunning ? "✓" : "✗") << std::endl;
    
    return isRunning;
}

// 测试窗口标题设置
bool testWindowCaption() {
    std::cout << "Testing window caption..." << std::endl;
    
    // 设置窗口标题（ASCII）
    setcaption("EGE Window Management Test");
    
    // 设置窗口标题（Unicode）
    setcaption(L"EGE 窗口管理测试");
    
    // 如果没有崩溃，就算成功
    bool result = true;
    
    std::cout << "  Window caption: " << (result ? "PASS" : "FAIL") << std::endl;
    
    return result;
}

// 测试窗口显示/隐藏
bool testWindowVisibility() {
    std::cout << "Testing window visibility..." << std::endl;
    
    // 隐藏窗口
    hidewindow();
    
    // 稍作延迟
    Sleep(100);
    
    // 显示窗口
    showwindow();
    
    // 稍作延迟
    Sleep(100);
    
    // 再次隐藏（为了不干扰其他测试）
    hidewindow();
    
    // 如果执行完成没有错误，算成功
    bool result = true;
    
    std::cout << "  Window visibility: " << (result ? "PASS" : "FAIL") << std::endl;
    
    return result;
}

// 测试窗口移动
bool testWindowMove() {
    std::cout << "Testing window move..." << std::endl;
    
    // 移动窗口到不同位置
    movewindow(100, 100, false);
    Sleep(50);
    
    movewindow(200, 200, false);
    Sleep(50);
    
    // 恢复原位置
    movewindow(100, 100, false);
    
    // 如果执行完成没有错误，算成功
    bool result = true;
    
    std::cout << "  Window move: " << (result ? "PASS" : "FAIL") << std::endl;
    
    return result;
}

// 测试窗口尺寸调整
bool testWindowResize() {
    std::cout << "Testing window resize..." << std::endl;
    
    // 调整窗口大小
    resizewindow(640, 480);
    Sleep(100);
    
    // 恢复原大小
    resizewindow(800, 600);
    Sleep(100);
    
    // 如果执行完成没有错误，算成功
    bool result = true;
    
    std::cout << "  Window resize: " << (result ? "PASS" : "FAIL") << std::endl;
    
    return result;
}

// 测试视口设置
bool testViewport() {
    std::cout << "Testing viewport..." << std::endl;
    
    // 获取当前视口
    int left1, top1, right1, bottom1, clip1;
    getviewport(&left1, &top1, &right1, &bottom1, &clip1);
    
    // 设置新视口
    setviewport(50, 50, 750, 550, 1);
    
    // 获取新视口
    int left2, top2, right2, bottom2, clip2;
    getviewport(&left2, &top2, &right2, &bottom2, &clip2);
    
    bool viewportSet = (left2 == 50 && top2 == 50 && right2 == 750 && bottom2 == 550);
    
    // 恢复默认视口
    setviewport(0, 0, 800, 600, 1);
    
    std::cout << "  Viewport: " << (viewportSet ? "PASS" : "FAIL") << std::endl;
    std::cout << "    Set viewport: " << (viewportSet ? "✓" : "✗") << std::endl;
    
    return viewportSet;
}

// 测试窗口视口
bool testWindowViewport() {
    std::cout << "Testing window viewport..." << std::endl;
    
    // 设置窗口视口
    window_setviewport(100, 100, 700, 500);
    
    // 获取窗口视口
    int left, top, right, bottom;
    window_getviewport(&left, &top, &right, &bottom);
    
    bool viewportSet = (left == 100 && top == 100 && right == 700 && bottom == 500);
    
    // 恢复默认
    window_setviewport(0, 0, 800, 600);
    
    std::cout << "  Window viewport: " << (viewportSet ? "PASS" : "FAIL") << std::endl;
    
    return viewportSet;
}

// 测试cleardevice
bool testClearDevice() {
    std::cout << "Testing cleardevice..." << std::endl;
    
    // 设置背景色并清空
    setbkcolor(RED);
    cleardevice();
    
    // 验证屏幕中心点是否为红色
    color_t centerColor = getpixel(400, 300);
    int r = EGEGET_R(centerColor);
    bool isRed = (r > 200); // 允许一些误差
    
    // 换个颜色再测试
    setbkcolor(BLUE);
    cleardevice();
    
    centerColor = getpixel(400, 300);
    int b = EGEGET_B(centerColor);
    bool isBlue = (b > 200);
    
    // 恢复默认
    setbkcolor(BLACK);
    cleardevice();
    
    bool result = isRed && isBlue;
    std::cout << "  cleardevice: " << (result ? "PASS" : "FAIL") << std::endl;
    
    return result;
}

// 测试settarget（默认窗口）
bool testSettarget() {
    std::cout << "Testing settarget..." << std::endl;
    
    // 创建一个图像
    PIMAGE img = newimage(200, 200);
    
    // 设置为目标
    int result1 = settarget(img);
    setbkcolor(RED);
    cleardevice();
    
    // 恢复默认目标（窗口）
    int result2 = settarget(nullptr);
    setbkcolor(BLUE);
    cleardevice();
    
    // 验证图像内容
    settarget(img);
    color_t imgColor = getpixel(100, 100, img);
    settarget(nullptr);
    
    int r = EGEGET_R(imgColor);
    bool imgIsRed = (r > 200);
    
    delimage(img);
    
    // 恢复默认背景
    setbkcolor(BLACK);
    cleardevice();
    
    std::cout << "  settarget: " << (imgIsRed ? "PASS" : "FAIL") << std::endl;
    
    return imgIsRed;
}

// 测试getwidth/getheight（默认窗口）
bool testWindowSize() {
    std::cout << "Testing window size..." << std::endl;
    
    // 获取默认窗口尺寸
    int w = getwidth();
    int h = getheight();
    
    // 应该是800x600（TestFramework初始化的尺寸）
    bool sizeOk = (w == 800 && h == 600);
    
    std::cout << "  Window size: " << (sizeOk ? "PASS" : "FAIL") << std::endl;
    std::cout << "    Size: " << w << "x" << h << std::endl;
    
    return sizeOk;
}

int main() {
    TestFramework framework;
    
    if (!framework.initialize(800, 600)) {
        std::cerr << "Failed to initialize framework" << std::endl;
        return 1;
    }
    
    // 初始时隐藏窗口
    framework.hideWindow();
    
    std::cout << "==================================" << std::endl;
    std::cout << "Window Management Functional Test" << std::endl;
    std::cout << "==================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // 运行所有测试
    total++; if (testBasicWindowOperations()) passed++;
    total++; if (testWindowCaption()) passed++;
    total++; if (testWindowVisibility()) passed++;
    total++; if (testWindowMove()) passed++;
    total++; if (testWindowResize()) passed++;
    total++; if (testViewport()) passed++;
    total++; if (testWindowViewport()) passed++;
    total++; if (testClearDevice()) passed++;
    total++; if (testSettarget()) passed++;
    total++; if (testWindowSize()) passed++;
    
    std::cout << "\n==================================" << std::endl;
    std::cout << "Results: " << passed << "/" << total << " tests passed" << std::endl;
    std::cout << "Success rate: " << (100.0 * passed / total) << "%" << std::endl;
    std::cout << "==================================" << std::endl;
    
    framework.cleanup();
    
    return (passed == total) ? 0 : 1;
}
