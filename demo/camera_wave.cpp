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

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <cstdio>

#include <graphics.h>
#include <ege/camera_capture.h>

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>
#include <algorithm>

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
#define TEXT_INFO_MSG          "按 '+' 或 '-' 调整弹性, ↑/↓ 切换分辨率。作者: wysaid: http://xege.org"
#define TEXT_CAMERA_LIST_TITLE "可用相机设备:"
#define TEXT_CAMERA_LIST_ITEM  "  [%d] %s"
#define TEXT_CAMERA_SWITCH     "按空格键切换相机，或按数字键选择 | 当前: [%d] %s"
#define TEXT_SWITCHING_CAMERA  "正在切换到相机 %d..."
#define TEXT_RESOLUTION_LIST_TITLE "支持的分辨率:"
#define TEXT_RESOLUTION_ITEM       "  %dx%d"
#define TEXT_RESOLUTION_CURRENT    " <-当前"
#define TEXT_SWITCHING_RESOLUTION  "正在切换到分辨率 %dx%d..."
#define TEXT_WINDOW_RESIZED        "窗口大小已调整为 %dx%d"
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
#define TEXT_INFO_MSG          "Press '+'/'-' for elasticity, UP/DOWN for resolution. By wysaid: http://xege.org"
#define TEXT_CAMERA_LIST_TITLE "Available cameras:"
#define TEXT_CAMERA_LIST_ITEM  "  [%d] %s"
#define TEXT_CAMERA_SWITCH     "Press SPACE to switch camera, or press number key | Current: [%d] %s"
#define TEXT_SWITCHING_CAMERA  "Switching to camera %d..."
#define TEXT_RESOLUTION_LIST_TITLE "Supported resolutions:"
#define TEXT_RESOLUTION_ITEM       "  %dx%d"
#define TEXT_RESOLUTION_CURRENT    " <-Current"
#define TEXT_SWITCHING_RESOLUTION  "Switching to resolution %dx%d..."
#define TEXT_WINDOW_RESIZED        "Window resized to %dx%d"
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

// 窗口尺寸限制
#define MIN_LONG_EDGE  640   // 窗口长边最小值
#define MAX_LONG_EDGE  1920  // 窗口长边最大值

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

// 根据相机分辨率调整窗口大小，保持比例一致
// 返回 true 表示窗口大小发生了变化
bool adjustWindowToCamera(int cameraWidth, int cameraHeight, PIMAGE& target, Net& net)
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

    // 重新创建目标图像并重新初始化网格
    delimage(target);
    target = newimage(newWidth, newHeight);
    net.setOutputTarget(target);

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
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, INIT_RENDERMANUAL);
    setcaption(TEXT_WINDOW_TITLE);

    Net                net;
    ege::CameraCapture camera;
    PIMAGE             target = newimage(getwidth(), getheight());
    char               msgBuffer[1024];
    char               intensityBuffer[256];

    setbkmode(TRANSPARENT);
    setcolor(YELLOW, target);
    sprintf(intensityBuffer, TEXT_INTENSITY_RULE, net.getIntensity());

    net.initNet(80, 60, nullptr, target);

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

    std::shared_ptr<CameraFrame> frame;

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

        // 0 表示不等待, 直接返回, 要处理一下返回值为空的情况.
        auto newFrame = camera.grabFrame(0);
        if (newFrame) {
            // 使用 shared_ptr，无需手动释放旧帧
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
            net.catchPoint(x / (float)getwidth(), y / (float)getheight());
        } else {
            net.releasePoint();
        }

        if (kbhit()) {
            auto keyMsg = getkey();
            int key    = keyMsg.key;
            int newDeviceIndex = -1;
            int newResolutionIndex = -1;

            switch (key) {
            case '+':
            case '=':
                net.intensityInc(0.005f);
                sprintf(intensityBuffer, TEXT_INTENSITY_RULE, net.getIntensity());
                break;
            case '-':
            case '_':
                net.intensityDec(0.005f);
                sprintf(intensityBuffer, TEXT_INTENSITY_RULE, net.getIntensity());
                break;
            case key_up:
                // 上箭头键：切换到上一个分辨率
                if (!resolutions.empty()) {
                    newResolutionIndex = (currentResolutionIndex - 1 + static_cast<int>(resolutions.size())) % static_cast<int>(resolutions.size());
                }
                break;
            case key_down:
                // 下箭头键：切换到下一个分辨率
                if (!resolutions.empty()) {
                    newResolutionIndex = (currentResolutionIndex + 1) % static_cast<int>(resolutions.size());
                }
                break;
            case ' ':
                // 空格键：切换到下一个相机
                newDeviceIndex = (currentDeviceIndex + 1) % deviceCount;
                break;
            case key_esc:
                exit(0);
            default:
                // 数字键：切换到指定相机
                if (key >= '0' && key <= '9') {
                    int requestedIndex = key - '0';
                    if (requestedIndex < deviceCount) {
                        newDeviceIndex = requestedIndex;
                    }
                }
                break;
            }

            // 切换相机设备
            if (newDeviceIndex >= 0 && newDeviceIndex != currentDeviceIndex) {
                printf(TEXT_SWITCHING_CAMERA, newDeviceIndex);
                printf("\n");

                // 使用 shared_ptr，释放引用即可
                frame.reset();

                if (switchCamera(camera, newDeviceIndex, deviceCount)) {
                    currentDeviceIndex = newDeviceIndex;
                    // 获取新相机的分辨率列表
                    resolutions = getResolutionList(camera);
                    currentResolutionIndex = 0;
                    // 获取新相机的第一帧
                    frame = camera.grabFrame(5000);
                    if (frame) {
                        net.setTextureImage(frame->getImage());
                    }
                }
            }

            // 切换分辨率
            if (newResolutionIndex >= 0 && newResolutionIndex != currentResolutionIndex && !resolutions.empty()) {
                int newWidth = resolutions[newResolutionIndex].width;
                int newHeight = resolutions[newResolutionIndex].height;
                printf(TEXT_SWITCHING_RESOLUTION, newWidth, newHeight);
                printf("\n");

                frame.reset();

                if (switchCamera(camera, currentDeviceIndex, deviceCount, newWidth, newHeight)) {
                    currentResolutionIndex = newResolutionIndex;
                    // 调整窗口大小以匹配相机分辨率比例
                    adjustWindowToCamera(newWidth, newHeight, target, net);
                    // 获取新分辨率的第一帧
                    frame = camera.grabFrame(5000);
                    if (frame) {
                        net.setTextureImage(frame->getImage());
                    }
                }
            }

            flushkey();
        }

        net.drawNet();
        net.update();
        putimage(0, 0, target);

        // 显示相机设备列表
        int textY = 10;
        setcolor(0x00ff0000); // 红色
        outtextxy(10, textY, TEXT_INFO_MSG);
        textY += 20;
        outtextxy(10, textY, intensityBuffer);
        textY += 25;

        // 显示设备列表标题
        setcolor(YELLOW);
        outtextxy(10, textY, TEXT_CAMERA_LIST_TITLE);
        textY += 18;

        // 显示每个设备
        for (int i = 0; i < deviceCount; ++i) {
            if (i == currentDeviceIndex) {
                setcolor(LIGHTGREEN);
            } else {
                setcolor(WHITE);
            }
            sprintf(msgBuffer, TEXT_CAMERA_LIST_ITEM, i, deviceNames[i].c_str());
            outtextxy(10, textY, msgBuffer);
            textY += 16;
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
            textY += 18;

            // 获取当前帧的分辨率来更新索引
            if (frame && frame->getImage()) {
                currentResolutionIndex = findCurrentResolutionIndex(resolutions, frame->getWidth(), frame->getHeight());
            }

            // 最多显示6个分辨率，避免占用太多屏幕空间
            int displayCount = std::min(static_cast<int>(resolutions.size()), 6);
            int startIndex = 0;

            // 如果分辨率列表很长，以当前选中的为中心显示
            if (resolutions.size() > 6) {
                startIndex = std::max(0, currentResolutionIndex - 3);
                startIndex = std::min(startIndex, static_cast<int>(resolutions.size()) - 6);
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
                textY += 14;
            }
        }
    }

    fputs(TEXT_CAMERA_CLOSED, stderr);
    camera.close();
    closegraph();

    return 0;
}

#endif
