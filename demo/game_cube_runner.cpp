// Cube Runner - a compact software-rendered 3D game for XEGE.
//
// Inspired by https://www.game5.com.de/cuberunner/index.html. The geometry,
// rasterizer, HUD, and game logic below are original and use no external assets.

#ifndef CUBE_RUNNER_HEADLESS
#include <graphics.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace cube_runner {

constexpr int   kWidth       = 640;
constexpr int   kHeight      = 480;
constexpr int   kTracks      = 12;
constexpr int   kTubeRows    = 24;
constexpr int   kStageCount  = 8;
constexpr float kPi          = 3.14159265358979323846f;
constexpr float kTrackAngle  = 2.0f * kPi / kTracks;
const     float kTileSize    = 2.0f * std::sin(kPi / kTracks);
constexpr float kTubeRadius  = 1.0f;
constexpr float kNearPlane   = 0.18f;
const     float kFarPlane    = kTileSize * kTubeRows;
const     float kFogNear     = kFarPlane * 0.25f;
constexpr float kCameraY     = -0.5f;
constexpr float kPlayerAngle = -kPi * 0.5f;
const     float kFocalLength = (kHeight * 0.5f) / std::tan(75.0f * kPi / 360.0f);
// The browser game advances 0.14 world units per 60 Hz frame and adds
// 0.02 units per frame after each complete eight-stage level.
constexpr float kReferenceFps   = 60.0f;
constexpr float kBaseSpeed      = 0.14f * kReferenceFps;
constexpr float kLevelSpeedStep = 0.02f * kReferenceFps;
constexpr float kStageDuration  = 30.0f;
constexpr float kTubeStart      = 0.42f;
constexpr float kKeyboardTurnSpeed = kTrackAngle * 8.0f;
constexpr float kMouseDragSensitivity = kTrackAngle / 56.0f;

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
    Surface()
        : pixels(kWidth * kHeight),
          depth(kWidth * kHeight)
    {
    }

    void clear(Color color)
    {
        std::fill(pixels.begin(), pixels.end(), packColor(color));
        std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::infinity());
    }

    void blendPixel(int x, int y, Color color, float alpha)
    {
        if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
            return;
        }

        const std::size_t index = static_cast<std::size_t>(y) * kWidth + x;
        const std::uint32_t old = pixels[index];
        const Color background {
            static_cast<int>((old >> 16) & 0xff),
            static_cast<int>((old >> 8) & 0xff),
            static_cast<int>(old & 0xff),
        };
        pixels[index] = packColor(mixColor(background, color, alpha));
    }

    void fillRect(int x, int y, int width, int height, Color color, float alpha = 1.0f)
    {
        const int left   = std::max(0, x);
        const int top    = std::max(0, y);
        const int right  = std::min(kWidth, x + width);
        const int bottom = std::min(kHeight, y + height);

        for (int py = top; py < bottom; ++py) {
            for (int px = left; px < right; ++px) {
                blendPixel(px, py, color, alpha);
            }
        }
    }

    std::vector<std::uint32_t> pixels;
    std::vector<float>         depth;
};

struct Projected {
    float x;
    float y;
    float inverseDepth;
    bool  valid;
};

Projected project(const Vec3& point)
{
    if (point.z <= kNearPlane) {
        return {0.0f, 0.0f, 0.0f, false};
    }

    const float inverseDepth = 1.0f / point.z;
    return {
        kWidth * 0.5f + point.x * kFocalLength * inverseDepth,
        kHeight * 0.5f - (point.y - kCameraY) * kFocalLength * inverseDepth,
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
    const Projected p0 = project(a);
    const Projected p1 = project(b);
    const Projected p2 = project(c);
    if (!p0.valid || !p1.valid || !p2.valid) {
        return;
    }

    const float area = edge(p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
    if (std::abs(area) < 0.001f) {
        return;
    }

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
    const int maxX = std::min(kWidth - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
    const int maxY = std::min(kHeight - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));
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
            const std::size_t index = static_cast<std::size_t>(y) * kWidth + x;
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

void rasterLine(Surface& surface, const Vec3& a, const Vec3& b, Color color, int width = 1)
{
    const Projected p0 = project(a);
    const Projected p1 = project(b);
    if (!p0.valid || !p1.valid) {
        return;
    }

    const float dx = p1.x - p0.x;
    const float dy = p1.y - p0.y;
    const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))));

    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps;
        const int x = static_cast<int>(std::round(p0.x + dx * t));
        const int y = static_cast<int>(std::round(p0.y + dy * t));
        const float inverseDepth = p0.inverseDepth + (p1.inverseDepth - p0.inverseDepth) * t;
        const float z = 0.997f / std::max(inverseDepth, 0.00001f);
        const Color shaded = applyFog(color, z);

        for (int oy = -width / 2; oy <= width / 2; ++oy) {
            for (int ox = -width / 2; ox <= width / 2; ++ox) {
                const int px = x + ox;
                const int py = y + oy;
                if (px < 0 || px >= kWidth || py < 0 || py >= kHeight) {
                    continue;
                }
                const std::size_t index = static_cast<std::size_t>(py) * kWidth + px;
                if (z < surface.depth[index]) {
                    surface.depth[index] = z;
                    surface.pixels[index] = packColor(shaded);
                }
            }
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
    std::size_t obstacleCount() const { return obstacles.size(); }

    void render(Surface& surface) const
    {
        surface.clear({2, 4, 10});
        drawTube(surface);

        for (const Obstacle& obstacle : obstacles) {
            drawObstacle(surface, obstacle);
        }

        if (invincible <= 0.0f || (static_cast<int>(invincible * 12.0f) & 1) == 0) {
            drawPlayer(surface);
        }
        drawHud(surface);

        if (invincible > 0.8f) {
            surface.fillRect(0, 0, kWidth, kHeight, {255, 215, 30}, 0.16f);
        }

        if (paused || gameOver) {
            surface.fillRect(0, 0, kWidth, kHeight, {0, 0, 0}, 0.62f);
            const std::string message = gameOver ? "GAME OVER" : "PAUSED";
            const int scale = 5;
            const int textWidth = static_cast<int>(message.size()) * 6 * scale;
            drawText(surface, (kWidth - textWidth) / 2, 190, message, scale,
                gameOver ? Color {255, 80, 70} : Color {255, 220, 70});
            drawText(surface, 214, 242, gameOver ? "[R] RESTART" : "[P] RESUME", 2, {220, 235, 255});
        }
    }

private:
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

            const float angle = kPlayerAngle + obstacle.track * kTrackAngle + rotation;
            const float angularExtent = (obstacle.tangentSize / (2.0f * 0.72f)) + 0.12f;
            if (std::abs(wrapAngle(angle - kPlayerAngle)) < angularExtent) {
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

    void drawHud(Surface& surface) const
    {
        surface.fillRect(14, 14, 214, 62, {3, 8, 16}, 0.72f);
        surface.fillRect(14, 14, 214, 2, {70, 225, 255}, 0.88f);
        drawText(surface, 26, 24, "CUBE RUNNER", 2, {115, 235, 255});
        drawText(surface, 26, 48, "SCORE " + std::to_string(static_cast<int>(score)), 2,
            {245, 248, 255});

        const std::string status =
            "LIVES " + std::to_string(lives) + "   STAGE " + std::to_string(stage + 1);
        const int statusWidth = static_cast<int>(status.size()) * 12;
        drawText(surface, kWidth - statusWidth - 18, 24, status, 2, {255, 225, 70});

        surface.fillRect(0, kHeight - 31, kWidth, 31, {2, 5, 11}, 0.72f);
        const std::string controls =
            "[A/D] OR DRAG TO STEER   [P] PAUSE   [R] RESTART";
        const int controlsWidth = static_cast<int>(controls.size()) * 6;
        drawText(surface, (kWidth - controlsWidth) / 2, kHeight - 22, controls, 1,
            {190, 220, 240}, false);
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

    file << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
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

} // namespace cube_runner

#ifdef CUBE_RUNNER_HEADLESS

#include <cstdlib>
#include <iostream>

namespace {

bool nearlyEqual(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

std::size_t changedPlayfieldPixels(
    const cube_runner::Surface& first, const cube_runner::Surface& second)
{
    std::size_t changed = 0;
    for (int y = 90; y < cube_runner::kHeight - 32; ++y) {
        for (int x = 0; x < cube_runner::kWidth; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * cube_runner::kWidth + x;
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
        std::cout << "Cube Runner self-test passed: speed, tunnel motion, spawning, pause, "
                     "stages, and level progression\n";
    }
    return passed;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const std::string outputPath = argc > 1 ? argv[1] : "cube_runner.ppm";
    const int frameCount = argc > 2 ? std::max(1, std::atoi(argv[2])) : 360;

    cube_runner::Game game(false);
    for (int frame = 0; frame < frameCount; ++frame) {
        const bool turnLeft  = (frame > 160 && frame < 205);
        const bool turnRight = (frame > 275 && frame < 330);
        game.update(1.0f / 60.0f, turnLeft, turnRight);
    }

    cube_runner::Surface surface;
    game.render(surface);
    if (!cube_runner::savePpm(surface, outputPath)) {
        std::cerr << "Failed to save " << outputPath << '\n';
        return 1;
    }

    std::cout << "Saved " << outputPath << " (" << cube_runner::kWidth << 'x'
              << cube_runner::kHeight << ")\n";
    return 0;
}

#else

int main()
{
    using namespace ege;
    using namespace cube_runner;

    initgraph(kWidth, kHeight, INIT_RENDERMANUAL);
    setcaption("XEGE - Cube Runner");
    setrendermode(RENDER_MANUAL);

    PIMAGE frameImage = newimage(kWidth, kHeight);
    Surface surface;
    Game game;
    bool dragging = false;
    int lastMouseX = 0;

    for (; is_run(); delay_fps(60)) {
        if (keypress(key_esc)) {
            break;
        }
        if (keypress(key_P)) {
            game.togglePause();
        }
        if (keypress(key_R)) {
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
                dragRotation += (message.x - lastMouseX) * kMouseDragSensitivity;
                lastMouseX = message.x;
            }
        }
        if (dragging && !keystate(key_mouse_l)) {
            dragging = false;
        }

        const bool turnLeft  = keystate(key_left) || keystate(key_A);
        const bool turnRight = keystate(key_right) || keystate(key_D);
        game.update(1.0f / 60.0f, turnLeft, turnRight, dragRotation);
        game.render(surface);

        color_t* destination = getbuffer(frameImage);
        std::copy(surface.pixels.begin(), surface.pixels.end(), destination);
        putimage(0, 0, frameImage);
    }

    delimage(frameImage);
    closegraph();
    return 0;
}

#endif
