/**
 * @file camera_wave.cpp
 * @author wysaid (this@xege.org)
 * @brief 一个使用 ege 内置的相机模块打开相机的例子
 * @date 2025-05-18
 *
 */

#define SHOW_CONSOLE 1

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <cstdio>

#include <graphics.h>
#include <ege/camera_capture.h>

#include <vector>
#include <string>
#include <cassert>
#include <cmath>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_WINDOW_TITLE      "EGE 相机水波特效演示 - 作者: wysaid - 2025"
#define TEXT_ERROR_NO_CAMERA   "此演示需要相机设备才能运行。\n请连接相机设备后重试。"
#define TEXT_ERROR_NO_CAMERA_FEATURE \
    "当前 ege 版本不支持相机功能，请使用支持 C++17 的编译器。 如果是 MSVC 编译器，请使用 MSVC 2022 或更高版本。"
#define TEXT_ERROR_EXIT_HINT   "按任意键退出。"
#define TEXT_ERROR_NO_DEVICE   "未找到相机设备！"
#define TEXT_ERROR_OPEN_FAILED "打开相机设备失败！"
#define TEXT_ERROR_GRAB_FAILED "获取帧数据失败！"
#define TEXT_CAMERA_CLOSED     "相机设备已关闭！"
#define TEXT_CAMERA_DEVICE     "相机设备: %s"
#define TEXT_CPP11_REQUIRED    "需要 C++11 或更高版本。"
#define TEXT_INTENSITY_RULE    "拖拽变形网格。弹性强度: %g"
#define TEXT_INFO_MSG          "按 '+' 或 '-' 调整弹性。作者: wysaid: http://xege.org"
#else
// 非MSVC编译器使用英文文案
#define TEXT_WINDOW_TITLE      "EGE camera wave By wysaid - 2025"
#define TEXT_ERROR_NO_CAMERA   "This demo requires a camera device to run.\nPlease connect a camera and try again."
#define TEXT_ERROR_NO_CAMERA_FEATURE \
    "The current version of EGE does not support camera features. Please use a compiler that supports C++17. If using MSVC, please use MSVC 2022 or later."
#define TEXT_ERROR_EXIT_HINT   "Press any key to exit."
#define TEXT_ERROR_NO_DEVICE   "No camera device found!!"
#define TEXT_ERROR_OPEN_FAILED "Failed to open camera device!!"
#define TEXT_ERROR_GRAB_FAILED "Failed to grab frame!!"
#define TEXT_CAMERA_CLOSED     "Camera device closed!!"
#define TEXT_CAMERA_DEVICE     "Camera device: %s"
#define TEXT_CPP11_REQUIRED    "C++11 or higher is required."
#define TEXT_INTENSITY_RULE    "Drag to deform mesh. Intensity: %g"
#define TEXT_INFO_MSG          "Press '+' or '-' to adjust elasticity. By wysaid: http://xege.org"
#endif

// 判断一下 C++ 版本, 低于 C++11 的编译器不支持
#if __cplusplus < 201103L
#pragma message("C++11 or higher is required.")

int main()
{
    return 0;
}
#else

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/// 结合水波荡漾的 Demo, 给一个相机的版本

struct Point
{
    Point() : x(0), y(0), dx(0), dy(0) {}

    Point(float _x, float _y, float _u, float _v) : x(_x), y(_y), dx(0), dy(0), u(_u), v(_v) {}

    float x, y;
    float dx, dy;
    float u, v;
};

void my_line(int* data, int width, int height, int pnt1x, int pnt1y, int pnt2x, int pnt2y, int color)
{
    int   dx   = pnt2x - pnt1x;
    int   dy   = pnt2y - pnt1y;
    float x    = pnt1x;
    float y    = pnt1y;
    int   step = 0;
    if (abs(dx) > abs(dy)) {
        step = abs(dx);
    } else {
        step = abs(dy);
    }

    // 修复：添加除零检查
    if (step == 0) {
        // 起点和终点相同，直接绘制单点
        if (pnt1x >= 0 && pnt1y >= 0 && pnt1x < width && pnt1y < height) {
            data[pnt1x + pnt1y * width] = color;
        }
        return;
    }

    float xStep = dx / (float)step;
    float yStep = dy / (float)step;
    for (int i = 0; i <= step; i++) {                      // 修复：应该包含终点，使用 <=
        if (x >= 0 && y >= 0 && x < width && y < height) { // 修复：边界检查
            data[(int)x + (int)y * width] = color;
        }
        x += xStep;
        y += yStep;
    }
}

class Net
{
public:
    Net() : m_index(0), m_intensity(0.2f), m_lastIndex(-1) {}

    ~Net() {}

    void setTextureImage(PIMAGE texture)
    {
        m_texture     = texture;
        m_texWidth    = getwidth(texture);
        m_texHeight   = getheight(texture);
        m_textureData = (color_t*)getbuffer(m_texture);
    }

    void setOutputTarget(PIMAGE target)
    {
        m_outputTarget = target;
        m_outputWidth  = getwidth(target);
        m_outputHeight = getheight(target);
    }

    // 网格的分辨率 w x h
    bool initNet(int w, int h, PIMAGE inputTexture, PIMAGE outputTarget)
    {
        if (w < 2 || h < 2) {
            return false;
        }

        if (inputTexture) {
            setTextureImage(inputTexture);
        }

        if (outputTarget) {
            setOutputTarget(outputTarget);
        }

        m_width  = w;
        m_height = h;

        m_vec[0].resize(w * h);
        m_vec[1].resize(w * h);
        float widthStep  = 1.0f / (w - 1);
        float heightStep = 1.0f / (h - 1);

        for (int i = 0; i != h; ++i) {
            const float heightI = i * heightStep;
            int         index   = w * i;
            for (int j = 0; j != w; ++j) {
                const float widthJ = j * widthStep;
                m_vec[0][index]    = Point(widthJ, heightI, widthJ, heightI);
                m_vec[1][index]    = Point(widthJ, heightI, widthJ, heightI);

                ++index;
            }
        }
        return true;
    }

    void update()
    {
        const float widthStep  = 1.0f / (m_width - 1.0f);
        const float heightStep = 1.0f / (m_height - 1.0f);
        int         index      = (m_index + 1) % 2;

        for (int i = 1; i < m_height - 1; ++i) {
            const int k = m_width * i;
            for (int j = 1; j < m_width - 1; ++j) {
                const int h = k + j;
                float     dx, dy;
                dx = (m_vec[m_index][h - 1].x + m_vec[m_index][h + 1].x - m_vec[m_index][h].x * 2.0f);
                dy = (m_vec[m_index][h - 1].y + m_vec[m_index][h + 1].y - m_vec[m_index][h].y * 2.0f);

                dx += (m_vec[m_index][h - m_width].x + m_vec[m_index][h + m_width].x - m_vec[m_index][h].x * 2.0f);
                dy += (m_vec[m_index][h - m_width].y + m_vec[m_index][h + m_width].y - m_vec[m_index][h].y * 2.0f);

                // 模拟能量损失， 当加速度方向与速度方向相反时，加快减速
                if (std::signbit(dx) != std::signbit(m_vec[m_index][h].dx)) {
                    dx *= 1.0f + m_intensity;
                }

                if (std::signbit(dy) != std::signbit(m_vec[m_index][h].dy)) {
                    dy *= 1.0f + m_intensity;
                }

                m_vec[m_index][h].dx += dx * m_intensity;
                m_vec[m_index][h].dy += dy * m_intensity;
                m_vec[index][h].dx    = m_vec[m_index][h].dx;
                m_vec[index][h].dy    = m_vec[m_index][h].dy;

                m_vec[index][h].x = m_vec[m_index][h].x + m_vec[index][h].dx;
                m_vec[index][h].y = m_vec[m_index][h].y + m_vec[index][h].dy;
            }
        }
        m_index = index;
    }

    void catchPoint(float x, float y)
    {
        int index;

        if (m_lastIndex < 0) {
            float mdis = 1e9f;
            for (int i = 1; i < m_height - 1; ++i) {
                const int k = m_width * i;
                for (int j = 1; j < m_width - 1; ++j) {
                    const int   h   = k + j;
                    const float dis = fabsf(x - m_vec[m_index][h].x) + fabsf(y - m_vec[m_index][h].y);
                    if (dis < mdis) {
                        index = h;
                        mdis  = dis;
                    }
                }
            }
            m_lastIndex = index;
        } else {
            index = m_lastIndex;
        }

        m_vec[0][index].x  = x;
        m_vec[0][index].y  = y;
        m_vec[1][index].x  = x;
        m_vec[1][index].y  = y;
        m_vec[0][index].dx = 0.0f;
        m_vec[0][index].dy = 0.0f;
        m_vec[1][index].dx = 0.0f;
        m_vec[1][index].dy = 0.0f;
    }

    void releasePoint() { m_lastIndex = -1; }

    template <class Type> inline void fillTriangle(const Type& v0, const Type& v1, const Type& v2)
    {
        if (v0.y == v2.y) {
            _fillSimpleTriangle(v0, v1, v2);
        } else if (v1.y == v2.y) {
            _fillSimpleTriangle(v1, v0, v2);
        } else if (v0.y == v1.y) {
            _fillSimpleTriangle(v0, v2, v1);
        } else {
            _fillNormalTriangle(v0, v1, v2);
        }
    }

    template <class Type> inline void _fillSimpleTriangle(const Type& vv0, const Type& v1, const Type& vv2)
    {
        assert(vv0.y == vv2.y);
        bool        isOK = (vv0.x < vv2.x);
        const Type& v0   = isOK ? vv0 : vv2;
        const Type& v2   = isOK ? vv2 : vv0;

        float h = v1.y - v0.y;

        // 修复：添加除零检查
        if (fabs(h) < 1e-6f) {
            return; // 三角形退化为线段，跳过
        }

        float dL = (v1.x - v0.x) / h;
        float dR = (v1.x - v2.x) / h;

        float dUL = (v1.u - v0.u) / h;
        float dUR = (v1.u - v2.u) / h;

        float dVL = (v1.v - v0.v) / h;
        float dVR = (v1.v - v2.v) / h;

        float xL = v0.x, xR = v2.x;
        float uL = v0.u, uR = v2.u;
        float vL = v0.v, vR = v2.v;

        const color_t* data         = m_textureData;
        color_t*       outputBuffer = (color_t*)getbuffer(m_outputTarget);

        if (v0.y < v1.y) {
            for (int i = v0.y; i < v1.y; ++i) {
                float len  = xR - xL;
                float uLen = uR - uL;
                float vLen = vR - vL;

                // 修复：添加除零检查
                if (fabs(len) > 1e-6f) {
                    for (int j = xL; j < xR; ++j) {
                        float percent = (j - xL) / len;
                        float u       = uL + uLen * percent;
                        float v       = vL + vLen * percent;
                        if (u < 0 || v < 0 || u > 1 || v > 1 || i < 0 || j < 0 || i >= m_outputHeight ||
                            j >= m_outputWidth)
                        {
                            continue;
                        }

                        // 修复：添加纹理边界检查
                        int ww = u * (m_texWidth - 1);
                        int hh = v * (m_texHeight - 1);

                        int index                 = ww + hh * m_texWidth;
                        int outputIndex           = j + i * m_outputWidth;
                        outputBuffer[outputIndex] = data[index];
                    }
                }
                xL += dL;
                xR += dR;
                uL += dUL;
                uR += dUR;
                vL += dVL;
                vR += dVR;
            }
        } else {
            for (int i = v0.y; i > v1.y; --i) {
                float len  = xR - xL;
                float uLen = uR - uL;
                float vLen = vR - vL;

                // 修复：添加除零检查
                if (fabs(len) > 1e-6f) {
                    for (int j = xL; j < xR; ++j) {
                        float percent = (j - xL) / len;
                        float u       = uL + uLen * percent;
                        float v       = vL + vLen * percent;
                        if (u < 0 || v < 0 || u > 1 || v > 1 || i < 0 || j < 0 || i >= m_outputHeight ||
                            j >= m_outputWidth)
                        {
                            continue;
                        }

                        int ww = u * (m_texWidth - 1);
                        int hh = v * (m_texHeight - 1);

                        int index                           = ww + hh * m_texWidth;
                        outputBuffer[j + i * m_outputWidth] = data[index];
                    }
                }

                xL -= dL;
                xR -= dR;
                uL -= dUL;
                uR -= dUR;
                vL -= dVL;
                vR -= dVR;
            }
        }
    }

    template <class Type> inline void _fillNormalTriangle(const Type& v0, const Type& v1, const Type& v2)
    {
        const Type* pnts[] = {&v0, &v1, &v2};

        if ((*pnts[0]).y > (*pnts[1]).y) {
            std::swap(pnts[0], pnts[1]);
        }

        if ((*pnts[0]).y > (*pnts[2]).y) {
            std::swap(pnts[0], pnts[2]);
        }

        if ((*pnts[1]).y > (*pnts[2]).y) {
            std::swap(pnts[1], pnts[2]);
        }

        const Type &vv0 = *pnts[0], &vv1 = *pnts[1], &vv2 = *pnts[2];

        // 修复：添加除零检查
        float heightDiff = vv2.y - vv0.y;
        if (fabs(heightDiff) < 1e-6f) {
            return; // 三角形退化，跳过
        }

        Type newPoint;

        float percent = (vv1.y - vv0.y) / heightDiff;

        newPoint.x = floorf(vv0.x + (vv2.x - vv0.x) * percent);
        newPoint.y = vv1.y;
        newPoint.u = vv0.u + (vv2.u - vv0.u) * percent;
        newPoint.v = vv0.v + (vv2.v - vv0.v) * percent;

        _fillSimpleTriangle(newPoint, vv0, vv1);
        _fillSimpleTriangle(newPoint, vv2, vv1);
    }

    void drawNet()
    {
        std::vector<Point>& vec = m_vec[m_index];
        int                 sz  = vec.size();
        int                 i; // i变量前置, 方便vc6.0 编译
        m_pointCache.resize(sz);
#if _MSC_VER < 1600 // 兼容vc6.0
        Point* v = &m_pointCache[0];
        memcpy(v, &vec[0], sz * sizeof(vec[0]));
#else
        Point* v = m_pointCache.data();
        memcpy(v, vec.data(), sz * sizeof(vec[0]));
#endif

        for (i = 0; i != sz; ++i) {
            v[i].x = floorf(v[i].x * m_outputWidth);
            v[i].y = floorf(v[i].y * m_outputHeight);
        }

        for (i = 1; i != m_height; ++i) {
            const int k1 = (i - 1) * m_width;
            const int k2 = i * m_width;

            for (int j = 1; j != m_width; ++j) {
                const int p1 = k1 + j - 1;
                const int p2 = k1 + j;
                const int p3 = k2 + j - 1;
                const int p4 = k2 + j;

                fillTriangle(v[p1], v[p2], v[p3]);
                fillTriangle(v[p3], v[p2], v[p4]);
            }
        }

        color_t* outputBuffer = (color_t*)getbuffer(m_outputTarget);

        if (m_lastIndex > 0) {
            for (i = 0; i != m_height; ++i) {
                const int k = i * m_width;
                for (int j = 1; j != m_width; ++j) {
                    const int h = k + j;
                    // line(v[h - 1].x, v[h - 1].y, v[h].x, v[h].y, m_outputTarget);
                    my_line((int*)outputBuffer, m_outputWidth, m_outputHeight, v[h - 1].x, v[h - 1].y, v[h].x, v[h].y,
                        0x00ffff00);
                }
            }

            for (i = 0; i != m_width; ++i) {
                for (int j = 1; j != m_height; ++j) {
                    const int h2 = j * m_width + i;
                    const int h1 = (j - 1) * m_width + i;
                    // line(v[h1].x, v[h1].y, v[h2].x, v[h2].y, m_outputTarget);
                    my_line((int*)outputBuffer, m_outputWidth, m_outputHeight, v[h1].x, v[h1].y, v[h2].x, v[h2].y,
                        0x00ffff00);
                }
            }
        }
    }

    void intensityInc(float f)
    {
        m_intensity += f;
        if (m_intensity > 0.3f) {
            m_intensity = 0.3f;
        }
    }

    void intensityDec(float f)
    {
        m_intensity -= f;
        if (m_intensity < 0.001f) {
            m_intensity = 0.001f;
        }
    }

    float getIntensity() { return m_intensity; }

private:
    std::vector<Point> m_vec[2];
    std::vector<Point> m_pointCache;
    int                m_index;
    int                m_width, m_height;
    float              m_intensity;
    int                m_lastIndex;

    PIMAGE   m_texture;
    int      m_texWidth, m_texHeight;
    color_t* m_textureData;

    PIMAGE m_outputTarget;
    int    m_outputWidth, m_outputHeight;
};

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
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, INIT_RENDERMANUAL);
    setcaption(TEXT_WINDOW_TITLE);

    Net                net;
    ege::CameraCapture camera;
    PIMAGE             target = newimage(getwidth(), getheight());
    char               msgBuffer[1024];

    setbkmode(TRANSPARENT);
    setcolor(YELLOW, target);
    sprintf(msgBuffer, TEXT_INTENSITY_RULE, net.getIntensity());

    net.initNet(80, 60, nullptr, target);

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
    CameraFrame* frame = nullptr;

    { // 尝试获取一帧数据, 最多等五秒, 如果失败, 直接退出.
        frame = camera.grabFrame(5000);

        if (!frame) {
            fputs(TEXT_ERROR_GRAB_FAILED, stderr);
            camera.close();
            return -1;
        }

        net.setTextureImage(frame->getImage());
    }

    for (; camera.isStarted() && is_run(); delay_fps(60)) {
        cleardevice();

        // 0 表示不等待, 直接返回, 要处理一下返回值为 nullptr 的情况.
        auto* newFrame = camera.grabFrame(0);
        if (newFrame) {
            if (frame) {
                frame->release();
            }
            frame = newFrame;
            net.setTextureImage(frame->getImage());
        }

        if (!frame) {
            fputs(TEXT_ERROR_GRAB_FAILED, stderr);
            break;
        }

        if (keystate(key_mouse_l)) {
            int x, y;
            mousepos(&x, &y);
            net.catchPoint(x / (float)WINDOW_WIDTH, y / (float)WINDOW_HEIGHT);
        } else {
            net.releasePoint();
        }

        if (kbhit()) {
            switch (getch()) {
            case '+':
                net.intensityInc(0.005f);
                break;
            case '-':
                net.intensityDec(0.005f);
                break;
            case 27:
                exit(0);
            }
            flushkey();
            sprintf(msgBuffer, TEXT_INTENSITY_RULE, net.getIntensity());
        }

        net.drawNet();
        net.update();
        putimage(0, 0, target);
        setcolor(0x00ff0000);
        outtextxy(10, 10, TEXT_INFO_MSG);
        outtextxy(10, 30, msgBuffer);
    }

    fputs(TEXT_CAMERA_CLOSED, stderr);
    camera.close();
    closegraph();

    return 0;
}

#endif
