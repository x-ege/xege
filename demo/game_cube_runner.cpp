// Cube Runner - a compact software-rendered 3D game for XEGE.
//
// Inspired by https://www.game5.com.de/cuberunner/index.html. The geometry,
// rasterizer, HUD, and game logic below are original and use no external assets.

#ifndef CUBE_RUNNER_HEADLESS
#include <graphics.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Total render threads, including the main thread. Set to 1 to disable parallel rendering.
#ifndef CUBE_RUNNER_MAX_RENDER_THREADS
#define CUBE_RUNNER_MAX_RENDER_THREADS 4
#endif

// 文本本地化宏定义
// 与其它 demo 一致: MSVC 编译器使用中文文案, 其它编译器使用英文文案.
// 无头模式 (CUBE_RUNNER_HEADLESS) 始终使用英文, 因为内置位图字体仅支持 ASCII,
// 且无头模式不依赖 EGE 字体系统.
#if defined(_MSC_VER) && !defined(CUBE_RUNNER_HEADLESS)
// MSVC 编译器使用中文文案
#define TEXT_WINDOW_TITLE   "XEGE - 立方体跑酷"
#define TEXT_HUD_TITLE      "立方体跑酷"
#define TEXT_SCORE          "得分 "
#define TEXT_LIVES          "生命 "
#define TEXT_STAGE          "关卡 "
#define TEXT_FPS            "帧率 "
#define TEXT_CONTROLS       "[A/D] 或拖动转向   [P] 暂停   [R] 重新开始"
#define TEXT_GAME_OVER      "游戏结束"
#define TEXT_PAUSED         "已暂停"
#define TEXT_ACTION_RESTART "[R] 重新开始"
#define TEXT_ACTION_RESUME  "[P] 继续"
#define TEXT_FONT_NAME      "宋体"
#else
// 非MSVC编译器 (或无头模式) 使用英文文案
#define TEXT_WINDOW_TITLE   "XEGE - Cube Runner"
#define TEXT_HUD_TITLE      "CUBE RUNNER"
#define TEXT_SCORE          "SCORE "
#define TEXT_LIVES          "LIVES "
#define TEXT_STAGE          "STAGE "
#define TEXT_FPS            "FPS "
#define TEXT_CONTROLS       "[A/D] OR DRAG TO STEER   [P] PAUSE   [R] RESTART"
#define TEXT_GAME_OVER      "GAME OVER"
#define TEXT_PAUSED         "PAUSED"
#define TEXT_ACTION_RESTART "[R] RESTART"
#define TEXT_ACTION_RESUME  "[P] RESUME"
#define TEXT_FONT_NAME      "Arial"
#endif

namespace cube_runner {

constexpr int   kDefaultWidth        = 1280;
constexpr int   kDefaultHeight       = 720;
constexpr int   kTracks              = 12;
constexpr int   kTubeRows            = 24;
constexpr int   kStageCount          = 8;
constexpr float kPi                  = 3.14159265358979323846f;
constexpr float kTrackAngle          = 2.0f * kPi / kTracks;
const     float kTileSize            = 2.0f * std::sin(kPi / kTracks);
constexpr float kTubeRadius          = 1.0f;
constexpr float kNearPlane           = 0.18f;
const     float kFarPlane            = kTileSize * kTubeRows;
const     float kFogNear             = kFarPlane * 0.25f;
constexpr float kCameraY             = -0.5f;
constexpr float kPlayerAngle         = -kPi * 0.5f;
constexpr float kVerticalFieldOfView = 75.0f;
// The browser game advances 0.14 world units per 60 Hz frame and adds
// 0.02 units per frame after each complete eight-stage level.
constexpr float kReferenceFps         = 60.0f;
constexpr float kBaseSpeed            = 0.14f * kReferenceFps;
constexpr float kLevelSpeedStep       = 0.02f * kReferenceFps;
constexpr float kStageDuration        = 30.0f;
constexpr float kTubeStart            = 0.42f;
constexpr float kKeyboardTurnSpeed    = kTrackAngle * 8.0f;
constexpr float kMouseDragSensitivity = kTrackAngle / 56.0f;

static_assert(CUBE_RUNNER_MAX_RENDER_THREADS >= 1,
    "CUBE_RUNNER_MAX_RENDER_THREADS must be at least one");

thread_local int gRenderBandTop = 0;
thread_local int gRenderBandBottom = std::numeric_limits<int>::max();

class RenderBandScope {
public:
    RenderBandScope(int top, int bottom)
        : previousTop(gRenderBandTop),
          previousBottom(gRenderBandBottom)
    {
        gRenderBandTop = top;
        gRenderBandBottom = bottom;
    }

    ~RenderBandScope()
    {
        gRenderBandTop = previousTop;
        gRenderBandBottom = previousBottom;
    }

private:
    int previousTop;
    int previousBottom;
};

int chooseRenderThreadCount()
{
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    const int automaticCount = hardwareThreads == 0
        ? 2 : (hardwareThreads >= 8 ? 4 : (hardwareThreads > 2 ? 2 : 1));
    return std::max(1, std::min({automaticCount, CUBE_RUNNER_MAX_RENDER_THREADS, 4}));
}

class KeyPressLatch {
public:
    bool update(bool down, int pressCount)
    {
        const bool pressed = pressCount > 0 || (down && !wasDown);
        wasDown = down;
        return pressed;
    }

private:
    bool wasDown {false};
};

class RenderExecutor {
public:
    RenderExecutor()
        : totalThreads(chooseRenderThreadCount())
    {
        for (int workerIndex = 1; workerIndex < totalThreads; ++workerIndex) {
            workers.emplace_back(&RenderExecutor::workerLoop, this, workerIndex);
        }
    }

    ~RenderExecutor()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
            ++generation;
        }
        workReady.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    int threadCount() const { return totalThreads; }

    void execute(const std::function<void(int)>& task)
    {
        if (totalThreads == 1) {
            task(0);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            frameTask = task;
            remainingWorkers = totalThreads - 1;
            ++generation;
        }
        workReady.notify_all();
        task(0);

        std::unique_lock<std::mutex> lock(mutex);
        frameFinished.wait(lock, [&] { return remainingWorkers == 0; });
        frameTask = nullptr;
    }

private:
    void workerLoop(int workerIndex)
    {
        std::size_t completedGeneration = 0;
        for (;;) {
            std::function<void(int)> task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                workReady.wait(lock, [&] {
                    return stopping || generation != completedGeneration;
                });
                if (stopping) {
                    return;
                }
                completedGeneration = generation;
                task = frameTask;
            }

            task(workerIndex);

            {
                std::lock_guard<std::mutex> lock(mutex);
                --remainingWorkers;
                if (remainingWorkers == 0) {
                    frameFinished.notify_one();
                }
            }
        }
    }

    const int                     totalThreads;
    std::vector<std::thread>      workers;
    std::mutex                    mutex;
    std::condition_variable       workReady;
    std::condition_variable       frameFinished;
    std::function<void(int)>      frameTask;
    std::size_t                   generation {0};
    int                           remainingWorkers {0};
    bool                          stopping {false};
};

RenderExecutor& renderExecutor()
{
    static RenderExecutor executor;
    return executor;
}

int renderThreadCount()
{
    return renderExecutor().threadCount();
}

struct Vec3 {
    float x;
    float y;
    float z;

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float value) const { return {x * value, y * value, z * value}; }
};

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 normalize(const Vec3& value)
{
    const float length = std::sqrt(std::max(dot(value, value), 0.000001f));
    return value * (1.0f / length);
}

struct Color {
    int r;
    int g;
    int b;
};

Color scaleColor(Color color, float scale)
{
    return {
        std::clamp(static_cast<int>(color.r * scale), 0, 255),
        std::clamp(static_cast<int>(color.g * scale), 0, 255),
        std::clamp(static_cast<int>(color.b * scale), 0, 255),
    };
}

Color mixColor(Color a, Color b, float amount)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    return {
        static_cast<int>(a.r + (b.r - a.r) * amount),
        static_cast<int>(a.g + (b.g - a.g) * amount),
        static_cast<int>(a.b + (b.b - a.b) * amount),
    };
}

std::uint32_t packColor(Color color)
{
    return 0xff000000u
        | (static_cast<std::uint32_t>(std::clamp(color.r, 0, 255)) << 16)
        | (static_cast<std::uint32_t>(std::clamp(color.g, 0, 255)) << 8)
        | static_cast<std::uint32_t>(std::clamp(color.b, 0, 255));
}

class Surface {
public:
    explicit Surface(int width = kDefaultWidth, int height = kDefaultHeight)
        : width(width),
          height(height),
          pixels(static_cast<std::size_t>(width) * height),
          depth(static_cast<std::size_t>(width) * height)
    {
    }

    void clear(Color color)
    {
        const int top = std::clamp(gRenderBandTop, 0, height);
        const int bottom = std::clamp(gRenderBandBottom, top, height);
        const std::size_t first = static_cast<std::size_t>(top) * width;
        const std::size_t last = static_cast<std::size_t>(bottom) * width;
        std::fill(pixels.begin() + first, pixels.begin() + last, packColor(color));
        std::fill(depth.begin() + first, depth.begin() + last,
            std::numeric_limits<float>::infinity());
    }

    void blendPixel(int x, int y, Color color, float alpha)
    {
        if (x < 0 || x >= width || y < 0 || y >= height
            || y < gRenderBandTop || y >= gRenderBandBottom) {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(y) * width + x;
        const std::uint32_t old = pixels[index];
        const Color background {
            static_cast<int>((old >> 16) & 0xff),
            static_cast<int>((old >> 8) & 0xff),
            static_cast<int>(old & 0xff),
        };
        pixels[index] = packColor(mixColor(background, color, alpha));
    }

    void fillRect(int x, int y, int rectWidth, int rectHeight, Color color, float alpha = 1.0f)
    {
        const int left   = std::max(0, x);
        const int top    = std::max({0, y, gRenderBandTop});
        const int right  = std::min(width, x + rectWidth);
        const int bottom = std::min({height, y + rectHeight, gRenderBandBottom});

        for (int py = top; py < bottom; ++py) {
            for (int px = left; px < right; ++px) {
                blendPixel(px, py, color, alpha);
            }
        }
    }

    int                        width;
    int                        height;
    std::vector<std::uint32_t> pixels;
    std::vector<float>         depth;
};

struct Projected {
    float x;
    float y;
    float inverseDepth;
    bool  valid;
};

Projected project(const Surface& surface, const Vec3& point)
{
    if (point.z <= kNearPlane) {
        return {0.0f, 0.0f, 0.0f, false};
    }

    const float inverseDepth = 1.0f / point.z;
    const float focalLength = (surface.height * 0.5f)
        / std::tan(kVerticalFieldOfView * kPi / 360.0f);
    return {
        surface.width * 0.5f + point.x * focalLength * inverseDepth,
        surface.height * 0.5f - (point.y - kCameraY) * focalLength * inverseDepth,
        inverseDepth,
        true,
    };
}

float edge(float ax, float ay, float bx, float by, float px, float py)
{
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

Color applyFog(Color color, float z)
{
    const float visibility = std::clamp((kFarPlane - z) / (kFarPlane - kFogNear), 0.0f, 1.0f);
    return scaleColor(color, visibility);
}

void rasterTriangle(Surface& surface, const Vec3& a, const Vec3& b, const Vec3& c, Color color,
    bool fog = true, float alpha = 1.0f)
{
    const Projected p0 = project(surface, a);
    const Projected p1 = project(surface, b);
    const Projected p2 = project(surface, c);
    if (!p0.valid || !p1.valid || !p2.valid) {
        return;
    }

    const float area = edge(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
    if (std::abs(area) < 0.001f) {
        return;
    }

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
    const int maxX = std::min(surface.width - 1,
        static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
    const int minY = std::max({0, gRenderBandTop,
        static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y})))});
    const int maxY = std::min({surface.height - 1, gRenderBandBottom - 1,
        static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y})))});
    const float inverseArea = 1.0f / area;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = x + 0.5f;
            const float py = y + 0.5f;
            const float w0 = edge(p1.x, p1.y, p2.x, p2.y, px, py) * inverseArea;
            const float w1 = edge(p2.x, p2.y, p0.x, p0.y, px, py) * inverseArea;
            const float w2 = 1.0f - w0 - w1;

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            const float inverseDepth =
                w0 * p0.inverseDepth + w1 * p1.inverseDepth + w2 * p2.inverseDepth;
            if (inverseDepth <= 0.0f) {
                continue;
            }

            const float z = 1.0f / inverseDepth;
            const std::size_t index = static_cast<std::size_t>(y) * surface.width + x;
            if (z >= surface.depth[index]) {
                continue;
            }

            const Color shaded = fog ? applyFog(color, z) : color;
            if (alpha >= 0.999f) {
                surface.pixels[index] = packColor(shaded);
            } else {
                surface.blendPixel(x, y, shaded, alpha);
            }
            surface.depth[index] = z;
        }
    }
}

void rasterLine(Surface& surface, const Vec3& a, const Vec3& b, Color color)
{
    const Projected p0 = project(surface, a);
    const Projected p1 = project(surface, b);
    if (!p0.valid || !p1.valid) {
        return;
    }

    const float dx = p1.x - p0.x;
    const float dy = p1.y - p0.y;
    const bool xMajor = std::abs(dx) >= std::abs(dy);
    const float lineWidth = std::clamp(surface.height / 480.0f, 1.35f, 2.0f);
    const float halfWidth = lineWidth * 0.5f;

    auto drawSample = [&](int majorPixel, float minor, float t) {
        const float inverseDepth = p0.inverseDepth + (p1.inverseDepth - p0.inverseDepth) * t;
        const float z = 0.997f / std::max(inverseDepth, 0.00001f);
        const Color shaded = applyFog(color, z);

        const int centerMinorPixel = static_cast<int>(std::floor(minor));
        for (int offset = -1; offset <= 1; ++offset) {
            const int minorPixel = centerMinorPixel + offset;
            const float pixelCenter = minorPixel + 0.5f;
            const float coverage = std::clamp(
                halfWidth + 0.5f - std::abs(pixelCenter - minor), 0.0f, 1.0f);
            if (coverage <= 0.0f) {
                continue;
            }

            const int x = xMajor ? majorPixel : minorPixel;
            const int y = xMajor ? minorPixel : majorPixel;
            if (x < 0 || x >= surface.width || y < 0 || y >= surface.height
                || y < gRenderBandTop || y >= gRenderBandBottom) {
                continue;
            }

            const std::size_t index = static_cast<std::size_t>(y) * surface.width + x;
            if (z < surface.depth[index]) {
                if (coverage >= 0.999f) {
                    surface.pixels[index] = packColor(shaded);
                } else {
                    surface.blendPixel(x, y, shaded, coverage);
                }
                surface.depth[index] = z;
            }
        }
    };

    if (xMajor) {
        const int first = std::max(0,
            static_cast<int>(std::floor(std::min(p0.x, p1.x))));
        const int last = std::min(surface.width - 1,
            static_cast<int>(std::floor(std::max(p0.x, p1.x))));
        for (int x = first; x <= last; ++x) {
            const float sampleX = x + 0.5f;
            const float t = std::clamp(
                std::abs(dx) > 0.00001f ? (sampleX - p0.x) / dx : 0.0f, 0.0f, 1.0f);
            drawSample(x, p0.y + dy * t, t);
        }
    } else {
        const int first = std::max({0, gRenderBandTop,
            static_cast<int>(std::floor(std::min(p0.y, p1.y)))});
        const int last = std::min({surface.height - 1, gRenderBandBottom - 1,
            static_cast<int>(std::floor(std::max(p0.y, p1.y)))});
        for (int y = first; y <= last; ++y) {
            const float sampleY = y + 0.5f;
            const float t = std::clamp(
                std::abs(dy) > 0.00001f ? (sampleY - p0.y) / dy : 0.0f, 0.0f, 1.0f);
            drawSample(y, p0.x + dx * t, t);
        }
    }
}

Vec3 curveCenter(float z)
{
    const float distance = std::max(0.0f, z - 6.0f);
    return {0.0f, -(distance * distance) / 20.0f, z};
}

std::array<std::uint8_t, 7> glyph(char character)
{
    switch (character) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {14, 4, 4, 4, 4, 4, 14};
    case 'J': return {1, 1, 1, 1, 17, 17, 14};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '/': return {1, 1, 2, 4, 8, 16, 16};
    case '[': return {14, 8, 8, 8, 8, 8, 14};
    case ']': return {14, 2, 2, 2, 2, 2, 14};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    default:  return {0, 0, 0, 0, 0, 0, 0};
    }
}

void drawText(Surface& surface, int x, int y, const std::string& text, int scale, Color color,
    bool shadow = true)
{
    auto drawGlyph = [&](int glyphX, int glyphY, char character, Color glyphColor) {
        const auto rows = glyph(character);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1u << (4 - column))) != 0) {
                    surface.fillRect(
                        glyphX + column * scale, glyphY + row * scale, scale, scale, glyphColor);
                }
            }
        }
    };

    int cursor = x;
    for (char character : text) {
        if (shadow) {
            drawGlyph(cursor + scale, y + scale, character, {0, 0, 0});
        }
        drawGlyph(cursor, y, character, color);
        cursor += 6 * scale;
    }
}

// The HUD text is rendered through a small canvas abstraction so the same
// layout code can drive either the software bitmap font (headless builds, which
// have no EGE font system) or EGE's native fonts (windowed builds, which need
// CJK support). Both backends implement draw() and width() with matching
// signatures; the Game class instantiates them via templates.

// Bitmap font backend: renders into a software Surface using the 5x7 glyphs
// above. Always available, used by the headless self-test / benchmark / PPM
// paths so their output stays deterministic and free of system-font coupling.
class BitmapTextCanvas {
public:
    explicit BitmapTextCanvas(Surface& surface) : surface(surface) {}

    void draw(int x, int y, const std::string& text, int scale, Color color, bool shadow = true) const
    {
        drawText(surface, x, y, text, scale, color, shadow);
    }

    int width(const std::string& text, int scale) const
    {
        return static_cast<int>(text.size()) * 6 * scale;
    }

private:
    Surface& surface;
};

#ifndef CUBE_RUNNER_HEADLESS
// EGE native font backend: renders with the window's font system so the HUD can
// display CJK glyphs. Used by the windowed build only. Font height tracks the
// bitmap font's 7-pixel glyph rows so the HUD layout is preserved.
class EgeTextCanvas {
public:
    EgeTextCanvas() = default;

    void draw(int x, int y, const std::string& text, int scale, Color color, bool shadow = true) const
    {
        setfont(fontHeight(scale), 0, TEXT_FONT_NAME);
        setbkmode(TRANSPARENT);
        if (shadow) {
            setcolor(EGERGB(0, 0, 0));
            outtextxy(x + scale, y + scale, text.c_str());
        }
        setcolor(EGERGB(color.r, color.g, color.b));
        outtextxy(x, y, text.c_str());
    }

    int width(const std::string& text, int scale) const
    {
        setfont(fontHeight(scale), 0, TEXT_FONT_NAME);
        return textwidth(text.c_str());
    }

private:
    static int fontHeight(int scale) { return std::max(7, 7 * scale); }
};
#endif // CUBE_RUNNER_HEADLESS

class Random {
public:
    explicit Random(std::uint32_t seed)
        : state(seed)
    {
    }

    std::uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    int range(int limit) { return static_cast<int>(next() % static_cast<std::uint32_t>(limit)); }

private:
    std::uint32_t state;
};

struct Obstacle {
    float z;
    int   track;
    Color color;
    float tangentSize;
    float radialSize;
    float depthSize;
    float speedScale;
};

constexpr std::array<Color, 13> kObstacleColors {{
    {255, 153, 204},
    {153, 255, 204},
    {153, 204, 255},
    {191, 191, 191},
    {128, 128, 128},
    {64, 64, 64},
    {255, 40, 40},
    {40, 255, 80},
    {255, 128, 20},
    {255, 45, 45},
    {40, 255, 90},
    {255, 150, 20},
    {30, 235, 255},
}};

float wrapAngle(float angle)
{
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

class Game {
public:
    explicit Game(bool collisionsEnabled = true)
        : random(0x58454745u),
          collisionsEnabled(collisionsEnabled)
    {
        reset();
    }

    void reset()
    {
        obstacles.clear();
        elapsed         = 0.0f;
        score           = 0.0f;
        forwardDistance = 0.0;
        rotation        = 0.0f;
        angularVelocity = 0.0f;
        currentSpeed    = kBaseSpeed;
        spawnDistance   = 6.0 * kTileSize;
        invincible      = 0.0f;
        lives           = 3;
        stage           = 0;
        level           = 0;
        paused          = false;
        gameOver        = false;
    }

    void togglePause()
    {
        if (!gameOver) paused = !paused;
    }

    void update(float deltaTime, bool turnLeft, bool turnRight, float dragRotation = 0.0f)
    {
        if (paused || gameOver) {
            return;
        }

        elapsed += deltaTime;
        const int stageProgress = static_cast<int>(elapsed / kStageDuration);
        stage = stageProgress % kStageCount;
        level = stageProgress / kStageCount;
        currentSpeed = kBaseSpeed + kLevelSpeedStep * level;
        const float frameDistance = currentSpeed * deltaTime;
        forwardDistance += frameDistance;
        score += kReferenceFps * deltaTime;
        invincible = std::max(0.0f, invincible - deltaTime);

        const float steering = (turnRight ? 1.0f : 0.0f) - (turnLeft ? 1.0f : 0.0f);
        const float targetVelocity = steering * kKeyboardTurnSpeed;
        angularVelocity +=
            (targetVelocity - angularVelocity) * std::min(1.0f, deltaTime * 18.0f);
        if (!turnLeft && !turnRight) {
            angularVelocity *= std::pow(0.12f, deltaTime);
        }
        rotation = wrapAngle(rotation + angularVelocity * deltaTime + dragRotation);

        for (Obstacle& obstacle : obstacles) {
            obstacle.z -= frameDistance * obstacle.speedScale;
        }

        checkCollisions();
        obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
                            [](const Obstacle& obstacle) { return obstacle.z < 0.22f; }),
            obstacles.end());

        spawnDistance -= frameDistance;
        while (spawnDistance <= 0.0) {
            spawnPattern();
        }
    }

    float speed() const { return currentSpeed; }
    double distance() const { return forwardDistance; }
    float rotationAngle() const { return rotation; }
    int currentStage() const { return stage; }
    int currentLevel() const { return level; }
    int livesRemaining() const { return lives; }
    bool isGameOver() const { return gameOver; }
    std::size_t obstacleCount() const { return obstacles.size(); }

    void render(Surface& surface, float framesPerSecond = 0.0f) const
    {
        RenderExecutor& executor = renderExecutor();
        const int threadCount = executor.threadCount();
        executor.execute([&](int threadIndex) {
            const int top = surface.height * threadIndex / threadCount;
            const int bottom = surface.height * (threadIndex + 1) / threadCount;
            RenderBandScope band(top, bottom);
            renderBand(surface, framesPerSecond);
        });
    }

    // Draws the HUD and overlay text. Windowed builds pass an EgeTextCanvas (EGE
    // native fonts, supports CJK); headless builds render text inside renderBand
    // with the bitmap font instead, so they do not call this method.
    template <typename Canvas>
    void drawText(Canvas& canvas, const Surface& surface, float framesPerSecond) const
    {
        drawHudText(canvas, surface, framesPerSecond);
        drawOverlayText(canvas, surface);
    }

private:
    void renderBand(Surface& surface, float framesPerSecond) const
    {
        surface.clear({2, 4, 10});
        drawTube(surface);

        for (const Obstacle& obstacle : obstacles) {
            drawObstacle(surface, obstacle);
        }

        if (invincible <= 0.0f || (static_cast<int>(invincible * 12.0f) & 1) == 0) {
            drawPlayer(surface);
        }
        drawHudBackground(surface);

        if (invincible > 0.8f) {
            surface.fillRect(0, 0, surface.width, surface.height, {255, 215, 30}, 0.16f);
        }

        if (paused || gameOver) {
            surface.fillRect(0, 0, surface.width, surface.height, {0, 0, 0}, 0.62f);
        }

#ifdef CUBE_RUNNER_HEADLESS
        // Headless builds render text directly into the surface with the built-in
        // bitmap font. Windowed builds defer text to drawText() so the HUD can use
        // EGE's native fonts (and CJK glyphs) after the frame is blitted.
        BitmapTextCanvas canvas(surface);
        drawHudText(canvas, surface, framesPerSecond);
        drawOverlayText(canvas, surface);
#else
        (void)framesPerSecond;
#endif
    }

    static int hudScale(const Surface& surface)
    {
        const float resolutionScale = std::min(
            surface.width / 640.0f, surface.height / 480.0f);
        return std::clamp(static_cast<int>(resolutionScale + 0.5f), 1, 3);
    }

    void spawnOne(int track, int colorIndex, float tangentScale = 0.78f,
        float radialScale = 0.72f, float depthScale = 0.86f, float speedScale = 1.0f,
        float zOffset = 0.0f)
    {
        obstacles.push_back({
            kFarPlane - 0.3f + zOffset,
            track % kTracks,
            kObstacleColors[colorIndex],
            kTileSize * tangentScale,
            kTileSize * radialScale,
            kTileSize * depthScale,
            speedScale,
        });
    }

    void spawnPattern()
    {
        const int track = random.range(kTracks);

        switch (stage) {
        case 0:
            spawnOne(track, random.range(3));
            break;
        case 1:
            spawnOne(track, 6, 0.76f, 0.72f, 0.82f, 1.28f);
            break;
        case 2:
            spawnOne(track, 3 + random.range(3), 0.82f, 1.18f, 0.92f);
            break;
        case 3:
            spawnOne(track, 8, 0.82f, 1.75f, 0.88f);
            break;
        case 4:
            spawnOne(track, random.range(3));
            if (random.range(4) == 0) {
                spawnOne((track + 3 + random.range(4)) % kTracks, 11, 0.78f, 1.55f, 0.86f);
            }
            break;
        case 5: {
            const int count = 4 + 2 * random.range(3);
            for (int i = 0; i < count; ++i) {
                spawnOne((track + i) % kTracks, 12);
            }
            break;
        }
        case 6:
            spawnOne(track, random.range(3));
            if (random.range(3) == 0) {
                spawnOne((track + 1) % kTracks, 12, 0.75f, 0.75f, 0.82f, 1.0f, 0.32f);
                spawnOne((track + 2) % kTracks, 12, 0.75f, 0.75f, 0.82f, 1.0f, 0.64f);
            }
            break;
        default:
            spawnOne(track, 9, 2.65f, 1.7f, 0.9f);
            spawnOne((track + 5 + random.range(3)) % kTracks, 7, 0.76f, 0.76f, 0.82f, 1.2f, 0.5f);
            break;
        }

        float spacingInTiles = 6.0f;
        switch (stage) {
        case 2: spacingInTiles = 8.0f + 2.0f * level; break;
        case 3:
        case 7: spacingInTiles = 10.0f; break;
        case 4: spacingInTiles = 6.0f + 2.0f * level; break;
        case 6: spacingInTiles = 6.0f + 3.0f * level; break;
        default: break;
        }
        spawnDistance += spacingInTiles * kTileSize;
    }

    void checkCollisions()
    {
        if (!collisionsEnabled || invincible > 0.0f) {
            return;
        }

        for (Obstacle& obstacle : obstacles) {
            if (obstacle.z < 0.72f || obstacle.z > 1.36f) {
                continue;
            }

            // The player sits at a fixed kPlayerAngle, so only the obstacle's
            // track offset and the current rotation determine the relative angle.
            // (kPlayerAngle previously added and subtracted here cancelled out.)
            const float relativeAngle = obstacle.track * kTrackAngle + rotation;
            const float angularExtent = (obstacle.tangentSize / (2.0f * 0.72f)) + 0.12f;
            if (std::abs(wrapAngle(relativeAngle)) < angularExtent) {
                obstacle.z = 0.15f;
                invincible = 1.25f;
                --lives;
                if (lives <= 0) {
                    gameOver = true;
                }
                break;
            }
        }
    }

    Vec3 tubePoint(int trackEdge, float z) const
    {
        const float angle =
            kPlayerAngle - kTrackAngle * 0.5f + trackEdge * kTrackAngle + rotation;
        const Vec3 center = curveCenter(z);
        return {
            center.x + std::cos(angle) * kTubeRadius,
            center.y + std::sin(angle) * kTubeRadius,
            z,
        };
    }

    void drawTube(Surface& surface) const
    {
        const bool brightStage = (stage % 2) == 0;
        const float brightness = brightStage ? 1.0f : 0.34f;
        const double tilePosition = forwardDistance / kTileSize;
        const long long passedTiles = static_cast<long long>(std::floor(tilePosition));
        const float tubeOffset = static_cast<float>(
            forwardDistance - static_cast<double>(passedTiles) * kTileSize);
        const float firstRowZ = kTubeStart - tubeOffset;
        const float clippedNear = kNearPlane + 0.001f;

        // Start one row behind the near plane. That extra row replaces the one
        // that just passed the player and keeps the tunnel continuous when the
        // offset wraps at a tile boundary.
        for (int row = -1; row < kTubeRows; ++row) {
            const float rawZ0 = firstRowZ + row * kTileSize;
            const float rawZ1 = rawZ0 + kTileSize;
            if (rawZ1 <= clippedNear) {
                continue;
            }
            const float z0 = std::max(rawZ0, clippedNear);
            const float z1 = rawZ1;
            for (int track = 0; track < kTracks; ++track) {
                const Vec3 p00 = tubePoint(track, z0);
                const Vec3 p10 = tubePoint(track + 1, z0);
                const Vec3 p11 = tubePoint(track + 1, z1);
                const Vec3 p01 = tubePoint(track, z1);
                const bool alternate = ((track + row + passedTiles) & 1LL) != 0;
                const float checker = alternate ? 0.93f : 1.0f;
                const Color tile = scaleColor({224, 234, 244}, brightness * checker);
                rasterTriangle(surface, p00, p10, p11, tile);
                rasterTriangle(surface, p00, p11, p01, tile);
            }
        }

        const Color grid = brightStage ? Color {25, 42, 58} : Color {4, 8, 13};
        for (int row = -1; row <= kTubeRows; ++row) {
            const float z = firstRowZ + row * kTileSize;
            if (z <= clippedNear) {
                continue;
            }
            for (int track = 0; track < kTracks; ++track) {
                rasterLine(surface, tubePoint(track, z), tubePoint(track + 1, z), grid);
            }
        }
        for (int row = -1; row < kTubeRows; ++row) {
            const float rawZ0 = firstRowZ + row * kTileSize;
            const float rawZ1 = rawZ0 + kTileSize;
            if (rawZ1 <= clippedNear) {
                continue;
            }
            const float z0 = std::max(rawZ0, clippedNear);
            const float z1 = rawZ1;
            for (int track = 0; track < kTracks; ++track) {
                rasterLine(surface, tubePoint(track, z0), tubePoint(track, z1), grid);
            }
        }
    }

    void drawBox(Surface& surface, Vec3 center, const Vec3& tangent, const Vec3& radial,
        float tangentSize, float radialSize, float depthSize, Color color, float alpha = 1.0f) const
    {
        const Vec3 t = tangent * (tangentSize * 0.5f);
        const Vec3 r = radial * (radialSize * 0.5f);
        const Vec3 d {0.0f, 0.0f, depthSize * 0.5f};
        const std::array<Vec3, 8> vertices {{
            center - t - r - d,
            center + t - r - d,
            center + t + r - d,
            center - t + r - d,
            center - t - r + d,
            center + t - r + d,
            center + t + r + d,
            center - t + r + d,
        }};
        constexpr std::array<std::array<int, 4>, 6> faces {{
            {{0, 1, 2, 3}},
            {{5, 4, 7, 6}},
            {{4, 0, 3, 7}},
            {{1, 5, 6, 2}},
            {{3, 2, 6, 7}},
            {{4, 5, 1, 0}},
        }};
        const Vec3 light = normalize({-0.35f, 0.65f, -0.65f});

        for (const auto& face : faces) {
            const Vec3 normal = normalize(cross(
                vertices[face[1]] - vertices[face[0]], vertices[face[2]] - vertices[face[0]]));
            const float lightAmount = 0.42f + 0.58f * std::abs(dot(normal, light));
            const Color faceColor = scaleColor(color, lightAmount);
            rasterTriangle(surface, vertices[face[0]], vertices[face[1]], vertices[face[2]],
                faceColor, true, alpha);
            rasterTriangle(surface, vertices[face[0]], vertices[face[2]], vertices[face[3]],
                faceColor, true, alpha);
        }

        const Color edgeColor = scaleColor(color, 0.24f);
        constexpr std::array<std::array<int, 2>, 12> edges {{
            {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
            {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
            {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}},
        }};
        for (const auto& edge : edges) {
            rasterLine(surface, vertices[edge[0]], vertices[edge[1]], edgeColor);
        }
    }

    void drawObstacle(Surface& surface, const Obstacle& obstacle) const
    {
        const float angle = kPlayerAngle + obstacle.track * kTrackAngle + rotation;
        const Vec3 radial {std::cos(angle), std::sin(angle), 0.0f};
        const Vec3 tangent {-std::sin(angle), std::cos(angle), 0.0f};
        Vec3 center = curveCenter(obstacle.z);
        center = center + radial * 0.72f;
        drawBox(surface, center, tangent, radial, obstacle.tangentSize, obstacle.radialSize,
            obstacle.depthSize, obstacle.color);
    }

    void drawPlayer(Surface& surface) const
    {
        const Vec3 radial {0.0f, -1.0f, 0.0f};
        const Vec3 tangent {1.0f, 0.0f, 0.0f};
        const Vec3 center {0.0f, -0.80f, 0.72f};
        drawBox(surface, center, tangent, radial, 0.20f, 0.20f, 0.20f,
            {255, 226, 35}, 0.94f);
    }

    void drawHudBackground(Surface& surface) const
    {
        const int uiScale = hudScale(surface);
        surface.fillRect(14 * uiScale, 14 * uiScale, 214 * uiScale, 62 * uiScale,
            {3, 8, 16}, 0.72f);
        surface.fillRect(14 * uiScale, 14 * uiScale, 214 * uiScale, 2 * uiScale,
            {70, 225, 255}, 0.88f);
        surface.fillRect(0, surface.height - 31 * uiScale, surface.width, 31 * uiScale,
            {2, 5, 11}, 0.72f);
    }

    template <typename Canvas>
    void drawHudText(Canvas& canvas, const Surface& surface, float framesPerSecond) const
    {
        const int uiScale = hudScale(surface);
        const int textScale = 2 * uiScale;
        canvas.draw(26 * uiScale, 24 * uiScale, TEXT_HUD_TITLE, textScale, {115, 235, 255});
        canvas.draw(26 * uiScale, 48 * uiScale,
            std::string(TEXT_SCORE) + std::to_string(static_cast<int>(score)), textScale,
            {245, 248, 255});

        const std::string status = std::string(TEXT_LIVES) + std::to_string(lives)
            + "   " + TEXT_STAGE + std::to_string(stage + 1);
        const int statusWidth = canvas.width(status, textScale);
        canvas.draw(surface.width - statusWidth - 18 * uiScale, 24 * uiScale,
            status, textScale, {255, 225, 70});

        const std::string fps = std::string(TEXT_FPS)
            + std::to_string(std::max(0, static_cast<int>(framesPerSecond + 0.5f)));
        const int fpsWidth = canvas.width(fps, textScale);
        canvas.draw(surface.width - fpsWidth - 18 * uiScale, 48 * uiScale,
            fps, textScale, {115, 235, 255});

        const int controlsScale = uiScale;
        const int controlsWidth = canvas.width(TEXT_CONTROLS, controlsScale);
        canvas.draw((surface.width - controlsWidth) / 2,
            surface.height - 22 * uiScale, TEXT_CONTROLS, controlsScale,
            {190, 220, 240}, false);
    }

    template <typename Canvas>
    void drawOverlayText(Canvas& canvas, const Surface& surface) const
    {
        if (!paused && !gameOver) {
            return;
        }

        const std::string message = gameOver ? TEXT_GAME_OVER : TEXT_PAUSED;
        const int uiScale = hudScale(surface);
        const int scale = 5 * uiScale;
        const int messageWidth = canvas.width(message, scale);
        const int messageY = static_cast<int>(surface.height * 0.40f);
        canvas.draw((surface.width - messageWidth) / 2, messageY, message, scale,
            gameOver ? Color {255, 80, 70} : Color {255, 220, 70});

        const std::string action = gameOver ? TEXT_ACTION_RESTART : TEXT_ACTION_RESUME;
        const int actionScale = 2 * uiScale;
        const int actionWidth = canvas.width(action, actionScale);
        canvas.draw((surface.width - actionWidth) / 2, messageY + 52 * uiScale,
            action, actionScale, {220, 235, 255});
    }

    Random                random;
    std::vector<Obstacle> obstacles;
    double                elapsed {0.0};
    float                 score {0.0f};
    double                forwardDistance {0.0};
    float                 rotation {0.0f};
    float                 angularVelocity {0.0f};
    float                 currentSpeed {kBaseSpeed};
    double                spawnDistance {0.0};
    float                 invincible {0.0f};
    int                   lives {3};
    int                   stage {0};
    int                   level {0};
    bool                  paused {false};
    bool                  gameOver {false};
    bool                  collisionsEnabled {true};
};

bool savePpm(const Surface& surface, const std::string& path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file << "P6\n" << surface.width << ' ' << surface.height << "\n255\n";
    for (std::uint32_t pixel : surface.pixels) {
        const std::array<char, 3> rgb {{
            static_cast<char>((pixel >> 16) & 0xff),
            static_cast<char>((pixel >> 8) & 0xff),
            static_cast<char>(pixel & 0xff),
        }};
        file.write(rgb.data(), rgb.size());
    }
    return static_cast<bool>(file);
}

bool parseResolution(const std::string& text, int& width, int& height)
{
    const std::size_t separator = text.find_first_of("xX");
    if (separator == std::string::npos) {
        return false;
    }

    const std::string widthText = text.substr(0, separator);
    const std::string heightText = text.substr(separator + 1);
    char* widthEnd = nullptr;
    char* heightEnd = nullptr;
    const long parsedWidth = std::strtol(widthText.c_str(), &widthEnd, 10);
    const long parsedHeight = std::strtol(heightText.c_str(), &heightEnd, 10);
    if (widthEnd == widthText.c_str() || *widthEnd != '\0'
        || heightEnd == heightText.c_str() || *heightEnd != '\0'
        || parsedWidth < 640 || parsedWidth > 3840
        || parsedHeight < 480 || parsedHeight > 2160) {
        return false;
    }

    width = static_cast<int>(parsedWidth);
    height = static_cast<int>(parsedHeight);
    return true;
}

} // namespace cube_runner

#ifdef CUBE_RUNNER_HEADLESS

namespace {

bool nearlyEqual(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

std::size_t changedPlayfieldPixels(
    const cube_runner::Surface& first, const cube_runner::Surface& second)
{
    std::size_t changed = 0;
    const int top = first.height * 90 / 480;
    const int bottom = first.height - first.height * 32 / 480;
    for (int y = top; y < bottom; ++y) {
        for (int x = 0; x < first.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * first.width + x;
            if (first.pixels[index] != second.pixels[index]) {
                ++changed;
            }
        }
    }
    return changed;
}

bool runSelfTest()
{
    constexpr float deltaTime = 1.0f / cube_runner::kReferenceFps;
    bool passed = true;
    auto require = [&](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "SELF-TEST FAILED: " << message << '\n';
            passed = false;
        }
    };

    cube_runner::Game movementGame(false);
    cube_runner::Surface before;
    cube_runner::Surface after;
    movementGame.render(before);
    movementGame.update(deltaTime, false, false);
    movementGame.render(after);

    require(nearlyEqual(movementGame.speed(), cube_runner::kBaseSpeed, 0.0001),
        "initial speed does not match the browser game");
    require(nearlyEqual(movementGame.distance(), 0.14, 0.0001),
        "one frame does not advance by 0.14 world units");
    require(changedPlayfieldPixels(before, after) > 500,
        "the tunnel playfield did not visibly move after one frame");

    cube_runner::Surface lineSurface(640, 480);
    lineSurface.clear({255, 255, 255});
    cube_runner::rasterLine(lineSurface, {-0.5f, 0.0f, 2.0f}, {0.5f, 0.25f, 2.0f},
        {0, 0, 0});
    const std::size_t blendedLinePixels = static_cast<std::size_t>(std::count_if(
        lineSurface.pixels.begin(), lineSurface.pixels.end(), [](std::uint32_t pixel) {
            const std::uint32_t rgb = pixel & 0x00ffffffu;
            return rgb != 0x00ffffffu && rgb != 0u;
        }));
    require(blendedLinePixels > 0,
        "line rasterization did not produce antialiased edge coverage");

    cube_runner::Surface fpsZero(800, 600);
    cube_runner::Surface fpsSixty(800, 600);
    movementGame.render(fpsZero, 0.0f);
    movementGame.render(fpsSixty, 60.0f);
    require(fpsZero.pixels != fpsSixty.pixels,
        "the FPS value was not drawn into the HUD");

    int parsedWidth = 0;
    int parsedHeight = 0;
    require(cube_runner::parseResolution("1280x720", parsedWidth, parsedHeight)
            && parsedWidth == 1280 && parsedHeight == 720,
        "a valid runtime resolution was rejected");
    require(!cube_runner::parseResolution("1280-by-720", parsedWidth, parsedHeight),
        "an invalid runtime resolution was accepted");
    require(cube_runner::renderThreadCount() >= 1
            && cube_runner::renderThreadCount() <= 4
            && cube_runner::renderThreadCount() <= CUBE_RUNNER_MAX_RENDER_THREADS,
        "the render worker count is outside the configured limit");

    cube_runner::KeyPressLatch keyLatch;
    require(keyLatch.update(false, 1),
        "a short key press between frames was lost");
    require(!keyLatch.update(false, 0),
        "a consumed short key press was repeated");
    require(keyLatch.update(true, 0) && !keyLatch.update(true, 0),
        "a held key did not generate exactly one down edge");
    require(!keyLatch.update(false, 0) && keyLatch.update(true, 0),
        "a released key could not be pressed again");

    for (int frame = 1; frame < static_cast<int>(cube_runner::kReferenceFps); ++frame) {
        movementGame.update(deltaTime, false, false);
    }
    require(nearlyEqual(movementGame.distance(), cube_runner::kBaseSpeed, 0.001),
        "one second of simulation did not cover the expected distance");
    require(movementGame.obstacleCount() > 0,
        "distance-based spawning did not create an obstacle");

    cube_runner::Game keyboardGame(false);
    for (int frame = 0; frame < 30; ++frame) {
        keyboardGame.update(deltaTime, false, true);
    }
    require(keyboardGame.rotationAngle() > cube_runner::kTrackAngle * 2.0f,
        "holding a turn key does not rotate the tunnel responsively");

    cube_runner::Game dragGame(false);
    dragGame.update(deltaTime, false, false, cube_runner::kTrackAngle);
    require(nearlyEqual(dragGame.rotationAngle(), cube_runner::kTrackAngle, 0.0001),
        "mouse drag rotation was not applied directly");

    const double distanceBeforePause = movementGame.distance();
    movementGame.togglePause();
    movementGame.update(1.0f, false, false);
    require(nearlyEqual(movementGame.distance(), distanceBeforePause, 0.000001),
        "pausing did not stop forward movement");

    cube_runner::Game resetGame(false);
    for (int frame = 0; frame < 120; ++frame) {
        resetGame.update(deltaTime, false, false);
    }
    resetGame.reset();
    require(!resetGame.isGameOver() && resetGame.livesRemaining() == 3
            && nearlyEqual(resetGame.distance(), 0.0, 0.000001)
            && resetGame.obstacleCount() == 0,
        "reset did not restore a fresh playable game");

    cube_runner::Game gameOverResetGame;
    for (int frame = 0; frame < 18000 && !gameOverResetGame.isGameOver(); ++frame) {
        gameOverResetGame.update(deltaTime, false, false);
    }
    require(gameOverResetGame.isGameOver(),
        "the deterministic collision scenario did not reach game over");
    gameOverResetGame.reset();
    require(!gameOverResetGame.isGameOver() && gameOverResetGame.livesRemaining() == 3,
        "reset did not recover from game over");

    cube_runner::Game stageGame(false);
    const int framesPerStage = static_cast<int>(
        cube_runner::kStageDuration * cube_runner::kReferenceFps) + 1;
    for (int frame = 0; frame < framesPerStage; ++frame) {
        stageGame.update(deltaTime, false, false);
    }
    require(stageGame.currentStage() == 1 && stageGame.currentLevel() == 0,
        "the first stage did not last about 30 seconds");

    cube_runner::Game levelGame(false);
    const int framesPerLevel = static_cast<int>(cube_runner::kStageDuration
        * cube_runner::kStageCount * cube_runner::kReferenceFps) + 1;
    for (int frame = 0; frame < framesPerLevel; ++frame) {
        levelGame.update(deltaTime, false, false);
    }
    require(levelGame.currentStage() == 0 && levelGame.currentLevel() == 1,
        "eight stages did not advance to the next level");
    require(nearlyEqual(levelGame.speed(),
                cube_runner::kBaseSpeed + cube_runner::kLevelSpeedStep, 0.0001),
        "level speed increase does not match the browser game");

    if (passed) {
        std::cout << "Cube Runner self-test passed: motion, controls, antialiased lines, "
                     "FPS HUD, runtime resolution, spawning, and progression\n";
    }
    return passed;
}

int runBenchmark(int width, int height, int frameCount)
{
    constexpr float deltaTime = 1.0f / cube_runner::kReferenceFps;
    cube_runner::Game game(false);
    cube_runner::Surface surface(width, height);

    for (int frame = 0; frame < 30; ++frame) {
        game.update(deltaTime, false, false);
        game.render(surface);
    }

    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < frameCount; ++frame) {
        const bool turnLeft = (frame % 240) >= 60 && (frame % 240) < 105;
        const bool turnRight = (frame % 240) >= 155 && (frame % 240) < 210;
        game.update(deltaTime, turnLeft, turnRight);
        game.render(surface);
    }
    const auto finish = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(finish - start).count();
    const double framesPerSecond = frameCount / seconds;
    const double millisecondsPerFrame = seconds * 1000.0 / frameCount;

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < surface.pixels.size(); index += 997) {
        checksum = checksum * 131u + surface.pixels[index];
    }

    std::cout << "BENCHMARK " << width << 'x' << height
              << " frames=" << frameCount
              << " threads=" << cube_runner::renderThreadCount()
              << std::fixed << std::setprecision(2)
              << " fps=" << framesPerSecond
              << " ms/frame=" << millisecondsPerFrame
              << " checksum=" << checksum << '\n';
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (argc > 1 && std::string(argv[1]) == "--benchmark") {
        int width = cube_runner::kDefaultWidth;
        int height = cube_runner::kDefaultHeight;
        if (argc < 3 || !cube_runner::parseResolution(argv[2], width, height)) {
            std::cerr << "Usage: --benchmark WIDTHxHEIGHT [frames]\n";
            return EXIT_FAILURE;
        }
        const int frameCount = argc > 3 ? std::max(1, std::atoi(argv[3])) : 240;
        return runBenchmark(width, height, frameCount);
    }

    int width = cube_runner::kDefaultWidth;
    int height = cube_runner::kDefaultHeight;
    int argumentOffset = 0;
    if (argc > 1 && std::string(argv[1]) == "--render") {
        if (argc < 3 || !cube_runner::parseResolution(argv[2], width, height)) {
            std::cerr << "Usage: --render WIDTHxHEIGHT [output.ppm] [frames]\n";
            return EXIT_FAILURE;
        }
        argumentOffset = 2;
    } else if (argc > 1 && argv[1][0] == '-') {
        // Reject unknown options instead of silently treating them as the output
        // filename, mirroring the interactive main's argument validation.
        std::cerr << "Unknown argument: " << argv[1] << '\n';
        std::cerr << "Usage: --self-test | --benchmark WIDTHxHEIGHT [frames]"
                     " | --render WIDTHxHEIGHT [output.ppm] [frames] | [output.ppm]\n";
        return EXIT_FAILURE;
    }

    const std::string outputPath = argc > argumentOffset + 1
        ? argv[argumentOffset + 1] : "cube_runner.ppm";
    const int frameCount = argc > argumentOffset + 2
        ? std::max(1, std::atoi(argv[argumentOffset + 2])) : 360;

    cube_runner::Game game(false);
    for (int frame = 0; frame < frameCount; ++frame) {
        const bool turnLeft  = (frame > 160 && frame < 205);
        const bool turnRight = (frame > 275 && frame < 330);
        game.update(1.0f / 60.0f, turnLeft, turnRight);
    }

    cube_runner::Surface surface(width, height);
    game.render(surface);
    if (!cube_runner::savePpm(surface, outputPath)) {
        std::cerr << "Failed to save " << outputPath << '\n';
        return 1;
    }

    std::cout << "Saved " << outputPath << " (" << surface.width << 'x'
              << surface.height << ")\n";
    return 0;
}

#else

int main(int argc, char** argv)
{
    using namespace ege;
    using namespace cube_runner;

    int width = kDefaultWidth;
    int height = kDefaultHeight;
    int exitAfterFrames = 0;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--resolution" && index + 1 < argc) {
            if (!parseResolution(argv[++index], width, height)) {
                std::cerr << "Invalid resolution. Expected WIDTHxHEIGHT between 640x480 and 3840x2160.\n";
                return EXIT_FAILURE;
            }
        } else if (argument == "--exit-after" && index + 1 < argc) {
            exitAfterFrames = std::max(1, std::atoi(argv[++index]));
        } else if (argument == "--help") {
            std::cout << "Usage: game_cube_runner [--resolution WIDTHxHEIGHT]"
                         " [--exit-after frames]\n";
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Unknown argument: " << argument << '\n';
            return EXIT_FAILURE;
        }
    }

    initgraph(width, height, INIT_ANIMATION);
    setcaption(TEXT_WINDOW_TITLE);
    setrendermode(RENDER_MANUAL);

    auto closeWindow = [] {
        const HWND window = getHWnd();
        SetCloseHandler(nullptr);
        if (::IsWindow(window)) {
            ::SendMessageW(window, WM_CLOSE, 0, 0);
        }
    };

    PIMAGE frameImage = newimage(width, height);
    if (frameImage == nullptr) {
        std::cerr << "Failed to create the " << width << 'x' << height << " frame buffer.\n";
        closeWindow();
        return EXIT_FAILURE;
    }
    Surface surface(width, height);
    Game game;
    EgeTextCanvas textCanvas;
    bool dragging = false;
    int lastMouseX = 0;
    int renderedFrames = 0;
    KeyPressLatch escapeKey;
    KeyPressLatch pauseKey;
    KeyPressLatch restartKey;
    const float dragSensitivity = kMouseDragSensitivity * 640.0f / width;

    while (is_run()) {
        if (escapeKey.update(keystate(key_esc), keypress(key_esc))) {
            break;
        }
        if (pauseKey.update(keystate(key_P), keypress(key_P))) {
            game.togglePause();
        }
        if (restartKey.update(keystate(key_R), keypress(key_R))) {
            game.reset();
        }

        float dragRotation = 0.0f;
        while (mousemsg()) {
            const mouse_msg message = getmouse();
            if (message.is_down() && message.is_left()) {
                dragging = true;
                lastMouseX = message.x;
            } else if (message.is_up() && message.is_left()) {
                dragging = false;
            } else if (message.is_move() && dragging) {
                dragRotation += (message.x - lastMouseX) * dragSensitivity;
                lastMouseX = message.x;
            }
        }
        if (dragging && !keystate(key_mouse_l)) {
            dragging = false;
        }

        const bool turnLeft  = keystate(key_left) || keystate(key_A);
        const bool turnRight = keystate(key_right) || keystate(key_D);
        const float framesPerSecond = getfps();
        game.update(1.0f / 60.0f, turnLeft, turnRight, dragRotation);
        game.render(surface, framesPerSecond);

        color_t* destination = getbuffer(frameImage);
        std::copy(surface.pixels.begin(), surface.pixels.end(), destination);
        putimage(0, 0, frameImage);
        game.drawText(textCanvas, surface, framesPerSecond);
        ++renderedFrames;
        if (exitAfterFrames > 0 && renderedFrames >= exitAfterFrames) {
            break;
        }
        delay_fps(60);
    }

    delimage(frameImage);
    closeWindow();
    return 0;
}

#endif
