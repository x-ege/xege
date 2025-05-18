/**
 * @file ege_camera.cpp
 * @author wysaid (this@xege.org)
 * @brief 一个使用 ege 内置的相机模块打开相机的例子
 * @date 2025-05-18
 *
 */

#define SHOW_CONSOLE 1

#include <cstdio>

#include <graphics.h>
#include <ege/camera.h>

#include <vector>
#include <string>

// 判断一下 C++ 版本, 低于 C++11 的编译器不支持
#if __cplusplus < 201103L
#pragma message("C++11 or higher is required.")

int main()
{
    fputs("C++11 or higher is required.", stderr);
    return 0;
}
#else

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

int main()
{
    /// 在相机的高吞吐场景下, 不设置 RENDERMANUAL 会出现闪屏.
    initgraph(1280, 720, INIT_RENDERMANUAL);
    setcaption("EGE Camera Demo");

    ege::Camera camera;

    // 0: 不输出日志, 1: 输出警告日志, 2: 输出常规信息, 3: 输出调试信息, 超过 3 等同于 3.
    ege::enableCameraModuleLog(2);

    { /// 打印一下所有相机设备的名称
        std::vector<std::string> cameraNames = camera.findDeviceNames();

        if (cameraNames.empty()) {
            fputs("No camera device found!!", stderr);
            return -1;
        }

        for (const auto& name : cameraNames) {
            printf("Camera device: %s\n", name.c_str());
        }
    }

    { /// 设置一下相机分辨率
        camera.setFrameSize(WINDOW_WIDTH, WINDOW_HEIGHT);
        camera.setFrameRate(30);
    }

    // 这里打开第一个相机设备
    if (!camera.open(0)) {
        fputs("Failed to open camera device!!", stderr);
        return -1;
    }

    camera.start();

    for (; camera.isStarted() && is_run(); delay_fps(60)) {
        cleardevice();

        CameraFrame* newFrame = camera.grabFrame(3000); // 最多等待 3 秒
        if (!newFrame) {
            break;
        }

        if (newFrame) {
            // 这里的 getImage 重复调用没有额外开销.
            auto* img = newFrame->getImage();
            if (img) {
                auto w = getwidth(img);
                auto h = getheight(img);
                putimage(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, img, 0, 0, w, h);
                newFrame->release(); // 释放帧数据
            }
        } else {
            fputs("Failed to grab frame!!", stderr);
            break;
        }
    }

    fputs("Camera device closed!!", stderr);
    camera.close();

    return 0;
}

#endif
