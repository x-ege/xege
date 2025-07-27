/**
 * @file camera_base.cpp
 * @author wysaid (this@xege.org)
 * @brief 一个使用 ege 内置的相机模块打开相机的例子
 * @date 2025-05-18
 *
 */

#define SHOW_CONSOLE 1

#include <cstdio>

#include <graphics.h>
#include <ege/camera_capture.h>

#include <vector>
#include <string>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_WINDOW_TITLE    "EGE 相机演示"
#define TEXT_ERROR_NO_CAMERA "此演示需要相机设备才能运行。\n请连接相机设备后重试。"
#define TEXT_ERROR_NO_CAMERA_FEATURE \
    "当前 ege 版本不支持相机功能，请使用支持 C++17 的编译器。 如果是 MSVC 编译器，请使用 MSVC 2022 或更高版本。"
#define TEXT_ERROR_EXIT_HINT   "按任意键退出。"
#define TEXT_ERROR_NO_DEVICE   "未找到相机设备！"
#define TEXT_ERROR_OPEN_FAILED "打开相机设备失败！"
#define TEXT_ERROR_GRAB_FAILED "获取帧数据失败！"
#define TEXT_CAMERA_CLOSED     "相机设备已关闭！"
#define TEXT_CAMERA_DEVICE     "相机设备: %s"
#define TEXT_CPP11_REQUIRED    "需要 C++11 或更高版本。"
#else
// 非MSVC编译器使用英文文案
#define TEXT_WINDOW_TITLE    "EGE Camera Demo"
#define TEXT_ERROR_NO_CAMERA "This demo requires a camera device to run.\nPlease connect a camera and try again."
#define TEXT_ERROR_NO_CAMERA_FEATURE \
    "The current version of EGE does not support camera features. Please use a compiler that supports C++17. If using MSVC, please use MSVC 2022 or later."
#define TEXT_ERROR_EXIT_HINT   "Press any key to exit."
#define TEXT_ERROR_NO_DEVICE   "No camera device found!!"
#define TEXT_ERROR_OPEN_FAILED "Failed to open camera device!!"
#define TEXT_ERROR_GRAB_FAILED "Failed to grab frame!!"
#define TEXT_CAMERA_CLOSED     "Camera device closed!!"
#define TEXT_CAMERA_DEVICE     "Camera device: %s"
#define TEXT_CPP11_REQUIRED    "C++11 or higher is required."
#endif

// 判断一下 C++ 版本, 低于 C++11 的编译器不支持
#if __cplusplus < 201103L
#pragma message("C++11 or higher is required.")

int main()
{
    fputs(TEXT_CPP11_REQUIRED, stderr);
    return 0;
}
#else

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

void showErrorWindow()
{
    settarget(nullptr);
    setbkcolor(BLACK);
    cleardevice();
    setcolor(RED);
    if (hasCameraCaptureModule()) {
        outtextrect(0, 0, getwidth(), getheight(), TEXT_ERROR_NO_CAMERA);
    } else {
        outtextrect(0, 0, getwidth(), getheight(), TEXT_ERROR_NO_CAMERA_FEATURE);
    }
    outtextxy(10, 30, TEXT_ERROR_EXIT_HINT);
    getch();
    closegraph();
}

int main()
{
    /// 在相机的高吞吐场景下, 不设置 RENDERMANUAL 会出现闪屏.
    initgraph(1280, 720, INIT_RENDERMANUAL);
    setcaption(TEXT_WINDOW_TITLE);

    ege::CameraCapture camera;

    // 0: 不输出日志, 1: 输出警告日志, 2: 输出常规信息, 3: 输出调试信息, 超过 3 等同于 3.
    ege::enableCameraModuleLog(2);

    { /// 打印一下所有相机设备的名称
        auto devices = camera.findDeviceNames();

        if (devices.count == 0) {
            fputs(TEXT_ERROR_NO_DEVICE, stderr);
            showErrorWindow();
            return -1;
        }

        for (int i = 0; i < devices.count; ++i) {
            printf(TEXT_CAMERA_DEVICE, devices.info[i].name);
            printf("\n");
        }
    }

    { /// 设置一下相机分辨率
        camera.setFrameSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        camera.setFrameRate(30);
    }

    // 这里打开第一个相机设备
    if (!camera.open(0)) {
        fputs(TEXT_ERROR_OPEN_FAILED, stderr);
        return -1;
    }

    camera.start();

    for (; camera.isStarted() && is_run(); delay_fps(60)) {
        cleardevice();

        CameraFrame* newFrame = camera.grabFrame(3000); // 最多等待 3 秒
        if (!newFrame) {
            fputs(TEXT_ERROR_GRAB_FAILED, stderr);
            break;
        }

        // 这里的 getImage 重复调用没有额外开销.
        auto* img = newFrame->getImage();
        if (img) {
            auto w = getwidth(img);
            auto h = getheight(img);
            putimage(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, img, 0, 0, w, h);
            newFrame->release(); // 释放帧数据
        }
    }

    fputs(TEXT_CAMERA_CLOSED, stderr);
    camera.close();

    return 0;
}

#endif
