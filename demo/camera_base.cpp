/**
 * @file camera_base.cpp
 * @author wysaid (this@xege.org)
 * @brief 一个使用 ege 内置的相机模块打开相机的例子
 * @date 2025-05-18
 *
 */

#define SHOW_CONSOLE 1

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <cstdio>

#include <graphics.h>
#include <ege/camera_capture.h>

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

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
#define TEXT_CAMERA_LIST_TITLE "可用相机设备:"
#define TEXT_CAMERA_LIST_ITEM  "  [%d] %s"
#define TEXT_CAMERA_SWITCH     "按空格键切换相机，或按数字键选择 | 当前: [%d] %s"
#define TEXT_SWITCHING_CAMERA  "正在切换到相机 %d..."
#define TEXT_RESOLUTION_LIST_TITLE "支持的分辨率:"
#define TEXT_RESOLUTION_ITEM       "  %dx%d"
#define TEXT_RESOLUTION_CURRENT    " <-当前"
#define TEXT_RESOLUTION_SWITCH     "按 ↑/↓ 箭头键切换分辨率"
#define TEXT_SWITCHING_RESOLUTION  "正在切换到分辨率 %dx%d..."
#define TEXT_WINDOW_RESIZED        "窗口大小已调整为 %dx%d"
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
#define TEXT_CAMERA_LIST_TITLE "Available cameras:"
#define TEXT_CAMERA_LIST_ITEM  "  [%d] %s"
#define TEXT_CAMERA_SWITCH     "Press SPACE to switch camera, or press number key | Current: [%d] %s"
#define TEXT_SWITCHING_CAMERA  "Switching to camera %d..."
#define TEXT_RESOLUTION_LIST_TITLE "Supported resolutions:"
#define TEXT_RESOLUTION_ITEM       "  %dx%d"
#define TEXT_RESOLUTION_CURRENT    " <-Current"
#define TEXT_RESOLUTION_SWITCH     "Press UP/DOWN arrow to switch resolution"
#define TEXT_SWITCHING_RESOLUTION  "Switching to resolution %dx%d..."
#define TEXT_WINDOW_RESIZED        "Window resized to %dx%d"
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

// 窗口尺寸限制
#define MIN_LONG_EDGE  640   // 窗口长边最小值
#define MAX_LONG_EDGE  1920  // 窗口长边最大值

// 根据相机分辨率调整窗口大小，保持比例一致
// 返回 true 表示窗口大小发生了变化
bool adjustWindowToCamera(int cameraWidth, int cameraHeight)
{
    int windowWidth = getwidth();
    int windowHeight = getheight();

    // 计算相机的长边和短边
    int cameraLongEdge = (std::max)(cameraWidth, cameraHeight);
    int cameraShortEdge = (std::min)(cameraWidth, cameraHeight);
    float cameraRatio = (float)cameraWidth / cameraHeight;

    // 获取屏幕可用区域（考虑任务栏）
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    int screenAvailWidth = workArea.right - workArea.left;
    int screenAvailHeight = workArea.bottom - workArea.top;
    // 留一点边距，避免窗口贴边
    screenAvailWidth -= 20;
    screenAvailHeight -= 40;

    // 计算目标窗口长边
    int targetLongEdge = cameraLongEdge;

    // 限制1：长边不能小于 MIN_LONG_EDGE
    if (targetLongEdge < MIN_LONG_EDGE) {
        targetLongEdge = MIN_LONG_EDGE;
    }

    // 限制2：长边不能大于 MAX_LONG_EDGE
    if (targetLongEdge > MAX_LONG_EDGE) {
        targetLongEdge = MAX_LONG_EDGE;
    }

    // 根据长边和比例计算窗口尺寸
    int newWidth, newHeight;
    if (cameraWidth >= cameraHeight) {
        // 横向视频，宽度是长边
        newWidth = targetLongEdge;
        newHeight = (int)(newWidth / cameraRatio);
    } else {
        // 纵向视频，高度是长边
        newHeight = targetLongEdge;
        newWidth = (int)(newHeight * cameraRatio);
    }

    // 限制3：不能超过屏幕可用区域
    if (newWidth > screenAvailWidth) {
        float scale = (float)screenAvailWidth / newWidth;
        newWidth = screenAvailWidth;
        newHeight = (int)(newHeight * scale);
    }
    if (newHeight > screenAvailHeight) {
        float scale = (float)screenAvailHeight / newHeight;
        newHeight = screenAvailHeight;
        newWidth = (int)(newWidth * scale);
    }

    // 如果新尺寸与当前尺寸相同，无需调整
    if (newWidth == windowWidth && newHeight == windowHeight) {
        return false;
    }

    // 调用 initgraph 调整窗口大小 (无需 closegraph)
    initgraph(newWidth, newHeight, INIT_RENDERMANUAL);
    setcaption(TEXT_WINDOW_TITLE);
    setbkmode(TRANSPARENT);

    printf(TEXT_WINDOW_RESIZED, newWidth, newHeight);
    printf("\n");

    return true;
}

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

// 切换相机设备的辅助函数
bool switchCamera(ege::CameraCapture& camera, int deviceIndex, int deviceCount, int resWidth = WINDOW_WIDTH, int resHeight = WINDOW_HEIGHT)
{
    if (deviceIndex < 0 || deviceIndex >= deviceCount) {
        return false;
    }

    // 先关闭当前相机
    if (camera.isStarted()) {
        camera.close();
    }

    // 设置相机分辨率
    camera.setFrameSize(resWidth, resHeight);
    camera.setFrameRate(30);

    // 打开新相机
    if (!camera.open(deviceIndex)) {
        return false;
    }

    camera.start();
    return true;
}

// 分辨率信息结构体
struct ResolutionItem
{
    int width;
    int height;
};

// 获取并保存分辨率列表
std::vector<ResolutionItem> getResolutionList(ege::CameraCapture& camera)
{
    std::vector<ResolutionItem> resolutions;
    auto resList = camera.getDeviceSupportedResolutions();
    if (resList.count > 0) {
        for (int i = 0; i < resList.count; ++i) {
            resolutions.push_back({resList.info[i].width, resList.info[i].height});
        }
    }
    return resolutions;
}

// 根据当前帧分辨率找到对应的分辨率索引
int findCurrentResolutionIndex(const std::vector<ResolutionItem>& resolutions, int width, int height)
{
    for (size_t i = 0; i < resolutions.size(); ++i) {
        if (resolutions[i].width == width && resolutions[i].height == height) {
            return static_cast<int>(i);
        }
    }
    return 0; // 如果找不到，返回第一个
}

int main()
{
    /// 在相机的高吞吐场景下, 不设置 RENDERMANUAL 会出现闪屏.
    initgraph(1280, 720, INIT_RENDERMANUAL);
    setcaption(TEXT_WINDOW_TITLE);
    setbkmode(TRANSPARENT);

    ege::CameraCapture camera;
    char               msgBuffer[1024];

    // 0: 不输出日志, 1: 输出警告日志, 2: 输出常规信息, 3: 输出调试信息, 超过 3 等同于 3.
    ege::enableCameraModuleLog(2);

    // 保存设备信息
    std::vector<std::string> deviceNames;
    int                      deviceCount        = 0;
    int                      currentDeviceIndex = 0;

    // 分辨率相关
    std::vector<ResolutionItem> resolutions;
    int                         currentResolutionIndex = 0;

    { /// 获取所有相机设备的名称
        auto devices = camera.findDeviceNames();

        if (devices.count == 0) {
            fputs(TEXT_ERROR_NO_DEVICE, stderr);
            showErrorWindow();
            return -1;
        }

        deviceCount = devices.count;
        for (int i = 0; i < devices.count; ++i) {
            deviceNames.push_back(devices.info[i].name);
            printf(TEXT_CAMERA_DEVICE, devices.info[i].name);
            printf("\n");
        }
    }

    // 打开第一个相机设备
    if (!switchCamera(camera, 0, deviceCount)) {
        fputs(TEXT_ERROR_OPEN_FAILED, stderr);
        return -1;
    }

    // 获取分辨率列表
    resolutions = getResolutionList(camera);

    for (; camera.isStarted() && is_run(); delay_fps(60)) {
        cleardevice();

        auto newFrame = camera.grabFrame(3000); // 最多等待 3 秒
        if (!newFrame) {
            fputs(TEXT_ERROR_GRAB_FAILED, stderr);
            break;
        }

        // 这里的 getImage 重复调用没有额外开销.
        auto* img = newFrame->getImage();
        if (img) {
            auto w = getwidth(img);
            auto h = getheight(img);
            putimage(0, 0, getwidth(), getheight(), img, 0, 0, w, h);

            // 更新当前分辨率索引
            currentResolutionIndex = findCurrentResolutionIndex(resolutions, w, h);
            // 使用 shared_ptr 版本无需手动调用 release()
        }

        // 处理键盘输入
        if (kbhit()) {
            auto keyMsg = getkey();
            int key = keyMsg.key;
            int newDeviceIndex = -1;
            int newResolutionIndex = -1;

            if (key == ' ') {
                // 空格键：切换到下一个相机
                newDeviceIndex = (currentDeviceIndex + 1) % deviceCount;
            } else if (key >= '0' && key <= '9') {
                // 数字键：切换到指定相机
                int requestedIndex = key - '0';
                if (requestedIndex < deviceCount) {
                    newDeviceIndex = requestedIndex;
                }
            } else if (key == '+' || key == '=') {
                // '+' 键：暂未使用
            } else if (key == '-' || key == '_') {
                // '-' 键：暂未使用
            } else if (key == key_up) {
                // 上箭头键：切换到上一个分辨率
                if (!resolutions.empty()) {
                    newResolutionIndex = (currentResolutionIndex - 1 + static_cast<int>(resolutions.size())) % static_cast<int>(resolutions.size());
                }
            } else if (key == key_down) {
                // 下箭头键：切换到下一个分辨率
                if (!resolutions.empty()) {
                    newResolutionIndex = (currentResolutionIndex + 1) % static_cast<int>(resolutions.size());
                }
            } else if (key == key_esc) {
                // ESC 键退出
                break;
            }

            // 切换相机设备
            if (newDeviceIndex >= 0 && newDeviceIndex != currentDeviceIndex) {
                printf(TEXT_SWITCHING_CAMERA, newDeviceIndex);
                printf("\n");
                if (switchCamera(camera, newDeviceIndex, deviceCount)) {
                    currentDeviceIndex = newDeviceIndex;
                    // 获取新相机的分辨率列表
                    resolutions = getResolutionList(camera);
                    currentResolutionIndex = 0;
                }
            }

            // 切换分辨率
            if (newResolutionIndex >= 0 && newResolutionIndex != currentResolutionIndex && !resolutions.empty()) {
                int newWidth = resolutions[newResolutionIndex].width;
                int newHeight = resolutions[newResolutionIndex].height;
                printf(TEXT_SWITCHING_RESOLUTION, newWidth, newHeight);
                printf("\n");
                if (switchCamera(camera, currentDeviceIndex, deviceCount, newWidth, newHeight)) {
                    currentResolutionIndex = newResolutionIndex;
                    // 调整窗口大小以匹配相机分辨率比例
                    adjustWindowToCamera(newWidth, newHeight);
                }
            }

            flushkey();
        }

        // 显示相机设备列表（仅当有多个设备时显示切换提示）
        int textY = 10;
        setcolor(YELLOW);

        // 显示设备列表标题
        outtextxy(10, textY, TEXT_CAMERA_LIST_TITLE);
        textY += 20;

        // 显示每个设备
        for (int i = 0; i < deviceCount; ++i) {
            if (i == currentDeviceIndex) {
                setcolor(LIGHTGREEN);
            } else {
                setcolor(WHITE);
            }
            sprintf(msgBuffer, TEXT_CAMERA_LIST_ITEM, i, deviceNames[i].c_str());
            outtextxy(10, textY, msgBuffer);
            textY += 18;
        }

        // 如果有多个相机，显示切换提示
        if (deviceCount > 1) {
            textY += 5;
            setcolor(CYAN);
            sprintf(msgBuffer, TEXT_CAMERA_SWITCH, currentDeviceIndex, deviceNames[currentDeviceIndex].c_str());
            outtextxy(10, textY, msgBuffer);
            textY += 20;
        }

        // 显示分辨率列表
        if (!resolutions.empty()) {
            textY += 10;
            setcolor(YELLOW);
            outtextxy(10, textY, TEXT_RESOLUTION_LIST_TITLE);
            textY += 20;

            // 最多显示8个分辨率，避免占用太多屏幕空间
            int displayCount = std::min(static_cast<int>(resolutions.size()), 8);
            int startIndex = 0;

            // 如果分辨率列表很长，以当前选中的为中心显示
            if (resolutions.size() > 8) {
                startIndex = std::max(0, currentResolutionIndex - 4);
                startIndex = std::min(startIndex, static_cast<int>(resolutions.size()) - 8);
            }

            for (int i = startIndex; i < startIndex + displayCount && i < static_cast<int>(resolutions.size()); ++i) {
                if (i == currentResolutionIndex) {
                    setcolor(LIGHTGREEN);
                    sprintf(msgBuffer, TEXT_RESOLUTION_ITEM TEXT_RESOLUTION_CURRENT, resolutions[i].width, resolutions[i].height);
                } else {
                    setcolor(WHITE);
                    sprintf(msgBuffer, TEXT_RESOLUTION_ITEM, resolutions[i].width, resolutions[i].height);
                }
                outtextxy(10, textY, msgBuffer);
                textY += 16;
            }

            // 显示分辨率切换提示
            textY += 5;
            setcolor(CYAN);
            outtextxy(10, textY, TEXT_RESOLUTION_SWITCH);
        }
    }

    fputs(TEXT_CAMERA_CLOSED, stderr);
    camera.close();

    return 0;
}

#endif
