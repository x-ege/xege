// Cube Runner - a compact software-rendered 3D game for XEGE.
//
// Inspired by https://www.game5.com.de/cuberunner/index.html. The geometry,
// rasterizer, HUD, and game logic below are original and use no external assets.

#include <graphics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <functional>
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
#ifdef _MSC_VER
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
// 非MSVC编译器使用英文文案
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
    // Borrows an externally-owned color buffer (typically the EGE PIMAGE buffer
    // returned by getbuffer(); color_t is uint32_t and packColor already emits
    // 0xAARRGGBB, so no conversion is needed). The z-buffer remains owned.
    Surface(int width, int height, std::uint32_t* pixels)
        : width(width),
          height(height),
          pixels(pixels),
          depth(static_cast<std::size_t>(width) * height)
    {
    }

    void clear(Color color)
    {
        const int top = std::clamp(gRenderBandTop, 0, height);
        const int bottom = std::clamp(gRenderBandBottom, top, height);
        const std::size_t first = static_cast<std::size_t>(top) * width;
        const std::size_t last = static_cast<std::size_t>(bottom) * width;
        std::fill(pixels + first, pixels + last, packColor(color));
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

    int                width;
    int                height;
    std::uint32_t*     pixels;
    std::vector<float> depth;
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

// HUD text helpers wrapping EGE's native font API, so the HUD can render CJK
// (宋体 under MSVC) as well as ASCII. The font height tracks the old 5x7 bitmap
// font's row count to preserve the original HUD layout.
int textFontHeight(int scale)
{
    return std::max(7, 7 * scale);
}

void drawTextLine(int x, int y, const std::string& text, int scale, Color color, bool shadow = true)
{
    setfont(textFontHeight(scale), 0, TEXT_FONT_NAME);
    setbkmode(TRANSPARENT);
    if (shadow) {
        setcolor(EGERGB(0, 0, 0));
        outtextxy(x + scale, y + scale, text.c_str());
    }
    setcolor(EGERGB(color.r, color.g, color.b));
    outtextxy(x, y, text.c_str());
}

int textLineWidth(const std::string& text, int scale)
{
    setfont(textFontHeight(scale), 0, TEXT_FONT_NAME);
    return textwidth(text.c_str());
}

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

    void render(Surface& surface) const
    {
        RenderExecutor& executor = renderExecutor();
        const int threadCount = executor.threadCount();
        executor.execute([&](int threadIndex) {
            const int top = surface.height * threadIndex / threadCount;
            const int bottom = surface.height * (threadIndex + 1) / threadCount;
            RenderBandScope band(top, bottom);
            renderBand(surface);
        });
    }

    // Draws the HUD and overlay text with EGE's native fonts (so the HUD can
    // show CJK under MSVC). Called by the windowed main after the frame is
    // blitted; the text lands on the device backbuffer on top of the image.
    void drawHud(const Surface& surface, float framesPerSecond) const;
    void drawOverlay(const Surface& surface) const;

private:
    void renderBand(Surface& surface) const
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

void Game::drawHud(const Surface& surface, float framesPerSecond) const
{
    const int uiScale = hudScale(surface);
    const int textScale = 2 * uiScale;
    drawTextLine(26 * uiScale, 24 * uiScale, TEXT_HUD_TITLE, textScale, {115, 235, 255});
    drawTextLine(26 * uiScale, 48 * uiScale,
        std::string(TEXT_SCORE) + std::to_string(static_cast<int>(score)), textScale,
        {245, 248, 255});

    const std::string status = std::string(TEXT_LIVES) + std::to_string(lives)
        + "   " + TEXT_STAGE + std::to_string(stage + 1);
    const int statusWidth = textLineWidth(status, textScale);
    drawTextLine(surface.width - statusWidth - 18 * uiScale, 24 * uiScale,
        status, textScale, {255, 225, 70});

    const std::string fps = std::string(TEXT_FPS)
        + std::to_string(std::max(0, static_cast<int>(framesPerSecond + 0.5f)));
    const int fpsWidth = textLineWidth(fps, textScale);
    drawTextLine(surface.width - fpsWidth - 18 * uiScale, 48 * uiScale,
        fps, textScale, {115, 235, 255});

    const int controlsScale = uiScale;
    const int controlsWidth = textLineWidth(TEXT_CONTROLS, controlsScale);
    drawTextLine((surface.width - controlsWidth) / 2,
        surface.height - 22 * uiScale, TEXT_CONTROLS, controlsScale,
        {190, 220, 240}, false);
}

void Game::drawOverlay(const Surface& surface) const
{
    if (!paused && !gameOver) {
        return;
    }

    const std::string message = gameOver ? TEXT_GAME_OVER : TEXT_PAUSED;
    const int uiScale = hudScale(surface);
    const int scale = 5 * uiScale;
    const int messageWidth = textLineWidth(message, scale);
    const int messageY = static_cast<int>(surface.height * 0.40f);
    drawTextLine((surface.width - messageWidth) / 2, messageY, message, scale,
        gameOver ? Color {255, 80, 70} : Color {255, 220, 70});

    const std::string action = gameOver ? TEXT_ACTION_RESTART : TEXT_ACTION_RESUME;
    const int actionScale = 2 * uiScale;
    const int actionWidth = textLineWidth(action, actionScale);
    drawTextLine((surface.width - actionWidth) / 2, messageY + 52 * uiScale,
        action, actionScale, {220, 235, 255});
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
        SetCloseHandler(nullptr);
#ifdef _WIN32
        const HWND window = getHWnd();
        if (::IsWindow(window)) {
            ::SendMessageW(window, WM_CLOSE, 0, 0);
        }
#else
        closegraph();
#endif
    };

    PIMAGE frameImage = newimage(width, height);
    if (frameImage == nullptr) {
        std::cerr << "Failed to create the " << width << 'x' << height << " frame buffer.\n";
        closeWindow();
        return EXIT_FAILURE;
    }
    Surface surface(width, height, getbuffer(frameImage));
    Game game;
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
        // render() writes directly into frameImage's buffer (Surface borrows it),
        // so there is no per-frame copy before the blit.
        game.render(surface);
        putimage(0, 0, frameImage);
        game.drawHud(surface, framesPerSecond);
        game.drawOverlay(surface);
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
