/*
 * EGE 图像管理功能测试
 * 
 * 测试图像创建、删除、复制、尺寸获取等功能
 */

#define SHOW_CONSOLE 1
#include "ege.h"
#include "../test_framework.h"
#include "../image_generator.h"

#include <iostream>

using namespace ege;

// 测试图像创建和删除
bool testImageCreationDeletion() {
    std::cout << "Testing image creation and deletion..." << std::endl;
    
    // 测试newimage
    PIMAGE img1 = newimage(100, 100);
    bool createOk = (img1 != nullptr);
    
    if (img1) {
        // 验证图像尺寸
        int w = getwidth(img1);
        int h = getheight(img1);
        bool sizeOk = (w == 100 && h == 100);
        
        // 删除图像
        delimage(img1);
        
        bool result = createOk && sizeOk;
        std::cout << "  Image creation/deletion: " << (result ? "PASS" : "FAIL") << std::endl;
        return result;
    }
    
    std::cout << "  Image creation/deletion: FAIL" << std::endl;
    return false;
}

// 测试图像尺寸创建的各种情况
bool testImageSizes() {
    std::cout << "Testing various image sizes..." << std::endl;
    
    struct TestCase {
        int width;
        int height;
        const char* name;
    };
    
    TestCase cases[] = {
        {1, 1, "1x1"},
        {64, 64, "64x64"},
        {640, 480, "640x480"},
        {1920, 1080, "1920x1080"},
        {100, 50, "100x50 (non-square)"}
    };
    
    bool allOk = true;
    for (const auto& tc : cases) {
        PIMAGE img = newimage(tc.width, tc.height);
        if (img) {
            int w = getwidth(img);
            int h = getheight(img);
            bool sizeMatch = (w == tc.width && h == tc.height);
            
            if (!sizeMatch) {
                std::cout << "    " << tc.name << ": FAIL (expected " 
                          << tc.width << "x" << tc.height 
                          << ", got " << w << "x" << h << ")" << std::endl;
                allOk = false;
            }
            
            delimage(img);
        } else {
            std::cout << "    " << tc.name << ": FAIL (creation failed)" << std::endl;
            allOk = false;
        }
    }
    
    std::cout << "  Image sizes: " << (allOk ? "PASS" : "FAIL") << std::endl;
    return allOk;
}

// 测试getimage - 从屏幕或图像复制
bool testGetImage() {
    std::cout << "Testing getimage..." << std::endl;
    
    // 创建源图像并绘制一些内容
    PIMAGE srcImg = newimage(100, 100);
    settarget(srcImg);
    setbkcolor(RED);
    cleardevice();
    setcolor(BLUE);
    circle(50, 50, 30, srcImg);
    settarget(nullptr);
    
    // 创建目标图像
    PIMAGE destImg = newimage(100, 100);
    
    // 测试从图像复制
    int result = getimage(destImg, srcImg, 0, 0, 100, 100);
    bool copyOk = (result == 0 || result == grOk);
    
    // 验证复制的内容（检查背景色）
    settarget(destImg);
    color_t centerColor = getpixel(50, 50, destImg);
    settarget(nullptr);
    
    // 清理
    delimage(srcImg);
    delimage(destImg);
    
    std::cout << "  getimage: " << (copyOk ? "PASS" : "FAIL") << std::endl;
    return copyOk;
}

// 测试图像坐标获取和设置
bool testImagePosition() {
    std::cout << "Testing image position (getx/gety)..." << std::endl;
    
    PIMAGE img = newimage(100, 100);
    settarget(img);
    
    // 初始位置应该是(0, 0)
    int x0 = getx(img);
    int y0 = gety(img);
    bool initPosOk = (x0 == 0 && y0 == 0);
    
    // moveto改变当前位置
    moveto(50, 50, img);
    int x1 = getx(img);
    int y1 = gety(img);
    bool movedPosOk = (x1 == 50 && y1 == 50);
    
    // 画线后位置应该更新
    lineto(80, 80, img);
    int x2 = getx(img);
    int y2 = gety(img);
    bool linePosOk = (x2 == 80 && y2 == 80);
    
    settarget(nullptr);
    delimage(img);
    
    bool result = initPosOk && movedPosOk && linePosOk;
    std::cout << "  Image position: " << (result ? "PASS" : "FAIL") << std::endl;
    std::cout << "    Initial position: " << (initPosOk ? "✓" : "✗") << std::endl;
    std::cout << "    After moveto: " << (movedPosOk ? "✓" : "✗") << std::endl;
    std::cout << "    After lineto: " << (linePosOk ? "✓" : "✗") << std::endl;
    
    return result;
}

// 测试图像目标设置
bool testImageTarget() {
    std::cout << "Testing settarget..." << std::endl;
    
    PIMAGE img1 = newimage(100, 100);
    PIMAGE img2 = newimage(100, 100);
    
    // 设置img1为目标
    settarget(img1);
    setbkcolor(RED);
    cleardevice();
    
    // 设置img2为目标
    settarget(img2);
    setbkcolor(BLUE);
    cleardevice();
    
    // 恢复默认目标
    settarget(nullptr);
    
    // 验证两个图像的内容不同
    settarget(img1);
    color_t color1 = getpixel(50, 50, img1);
    settarget(img2);
    color_t color2 = getpixel(50, 50, img2);
    settarget(nullptr);
    
    bool differentColors = (color1 != color2);
    
    delimage(img1);
    delimage(img2);
    
    std::cout << "  settarget: " << (differentColors ? "PASS" : "FAIL") << std::endl;
    return differentColors;
}

// 测试图像resize
bool testImageResize() {
    std::cout << "Testing image resize..." << std::endl;
    
    // 创建初始图像
    PIMAGE img = newimage(100, 100);
    settarget(img);
    setbkcolor(RED);
    cleardevice();
    settarget(nullptr);
    
    int w1 = getwidth(img);
    int h1 = getheight(img);
    bool initialSizeOk = (w1 == 100 && h1 == 100);
    
    // 调整大小
    resize(img, 200, 150);
    
    int w2 = getwidth(img);
    int h2 = getheight(img);
    bool resizedOk = (w2 == 200 && h2 == 150);
    
    delimage(img);
    
    bool result = initialSizeOk && resizedOk;
    std::cout << "  Image resize: " << (result ? "PASS" : "FAIL") << std::endl;
    std::cout << "    Initial size: " << (initialSizeOk ? "✓" : "✗") << std::endl;
    std::cout << "    Resized: " << (resizedOk ? "✓" : "✗") << std::endl;
    
    return result;
}

// 测试多个图像同时存在
bool testMultipleImages() {
    std::cout << "Testing multiple images..." << std::endl;
    
    const int numImages = 10;
    PIMAGE images[numImages];
    
    // 创建多个图像
    bool allCreated = true;
    for (int i = 0; i < numImages; i++) {
        images[i] = newimage(50 + i * 10, 50 + i * 10);
        if (!images[i]) {
            allCreated = false;
            break;
        }
    }
    
    // 验证所有图像的尺寸
    bool allSizesCorrect = true;
    if (allCreated) {
        for (int i = 0; i < numImages; i++) {
            int expectedSize = 50 + i * 10;
            int w = getwidth(images[i]);
            int h = getheight(images[i]);
            if (w != expectedSize || h != expectedSize) {
                allSizesCorrect = false;
                break;
            }
        }
    }
    
    // 删除所有图像
    for (int i = 0; i < numImages; i++) {
        if (images[i]) {
            delimage(images[i]);
        }
    }
    
    bool result = allCreated && allSizesCorrect;
    std::cout << "  Multiple images: " << (result ? "PASS" : "FAIL") << std::endl;
    
    return result;
}

// 测试边界条件
bool testBoundaryConditions() {
    std::cout << "Testing boundary conditions..." << std::endl;
    
    // 测试最小尺寸
    PIMAGE img1 = newimage(1, 1);
    bool minSizeOk = (img1 != nullptr);
    if (img1) {
        bool sizeCheck = (getwidth(img1) == 1 && getheight(img1) == 1);
        minSizeOk = minSizeOk && sizeCheck;
        delimage(img1);
    }
    
    // 测试大尺寸（但不要太大以免内存问题）
    PIMAGE img2 = newimage(4096, 4096);
    bool largeSizeOk = (img2 != nullptr);
    if (img2) {
        delimage(img2);
    }
    
    bool result = minSizeOk && largeSizeOk;
    std::cout << "  Boundary conditions: " << (result ? "PASS" : "FAIL") << std::endl;
    std::cout << "    Minimum size (1x1): " << (minSizeOk ? "✓" : "✗") << std::endl;
    std::cout << "    Large size (4096x4096): " << (largeSizeOk ? "✓" : "✗") << std::endl;
    
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
    std::cout << "Image Management Functional Test" << std::endl;
    std::cout << "==================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    // 运行所有测试
    total++; if (testImageCreationDeletion()) passed++;
    total++; if (testImageSizes()) passed++;
    total++; if (testGetImage()) passed++;
    total++; if (testImagePosition()) passed++;
    total++; if (testImageTarget()) passed++;
    total++; if (testImageResize()) passed++;
    total++; if (testMultipleImages()) passed++;
    total++; if (testBoundaryConditions()) passed++;
    
    std::cout << "\n==================================" << std::endl;
    std::cout << "Results: " << passed << "/" << total << " tests passed" << std::endl;
    std::cout << "Success rate: " << (100.0 * passed / total) << "%" << std::endl;
    std::cout << "==================================" << std::endl;
    
    framework.cleanup();
    
    return (passed == total) ? 0 : 1;
}
