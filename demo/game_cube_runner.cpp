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
    Game()
        : random(0x58454745u)
    {
        reset();
    }

    void reset()
    {
        obstacles.clear();
        elapsed         = 0.0f;
        score           = 0.0f;
        rotation        = 0.0f;
        angularVelocity = 0.0f;
        spawnTimer      = 0.35f;
        invincible      = 0.0f;
        lives           = 3;
        stage           = 0;
        paused          = false;
        gameOver        = false;
    }

    void togglePause()
    {
        if (!gameOver) paused = !paused;
    }

    void update(float deltaTime, bool turnLeft, bool turnRight)
    {
        if (paused || gameOver) {
            return;
        }

        elapsed += deltaTime;
        stage = static_cast<int>(elapsed / 18.0f) % 8;
        const float speed = 2.55f + 0.16f * static_cast<int>(elapsed / 18.0f);
        score += speed * deltaTime * 10.0f;
        invincible = std::max(0.0f, invincible - deltaTime);

        const float targetVelocity = (turnRight ? 1.9f : 0.0f) - (turnLeft ? 1.9f : 0.0f);
        angularVelocity += (targetVelocity - angularVelocity) * std::min(1.0f, deltaTime * 8.0f);
        if (!turnLeft && !turnRight) {
            angularVelocity *= std::pow(0.12f, deltaTime);
        }
        rotation = wrapAngle(rotation + angularVelocity * deltaTime);

        for (Obstacle& obstacle : obstacles) {
            obstacle.z -= speed * obstacle.speedScale * deltaTime;
        }

        checkCollisions();
        obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
                            [](const Obstacle& obstacle) { return obstacle.z < 0.22f; }),
            obstacles.end());

        spawnTimer -= deltaTime;
        if (spawnTimer <= 0.0f) {
            spawnPattern();
        }
    }

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
            spawnTimer = 0.82f;
            break;
        case 1:
            spawnOne(track, 6, 0.76f, 0.72f, 0.82f, 1.28f);
            spawnTimer = 0.92f;
            break;
        case 2:
            spawnOne(track, 3 + random.range(3), 0.82f, 1.18f, 0.92f);
            spawnTimer = 1.15f;
            break;
        case 3:
            spawnOne(track, 8, 0.82f, 1.75f, 0.88f);
            spawnTimer = 1.25f;
            break;
        case 4:
            spawnOne(track, random.range(3));
            if (random.range(4) == 0) {
                spawnOne((track + 3 + random.range(4)) % kTracks, 11, 0.78f, 1.55f, 0.86f);
            }
            spawnTimer = 0.74f;
            break;
        case 5: {
            const int count = 4 + 2 * random.range(3);
            for (int i = 0; i < count; ++i) {
                spawnOne((track + i) % kTracks, 12);
            }
            spawnTimer = 1.55f;
            break;
        }
        case 6:
            spawnOne(track, random.range(3));
            if (random.range(3) == 0) {
                spawnOne((track + 1) % kTracks, 12, 0.75f, 0.75f, 0.82f, 1.0f, 0.32f);
                spawnOne((track + 2) % kTracks, 12, 0.75f, 0.75f, 0.82f, 1.0f, 0.64f);
            }
            spawnTimer = 0.64f;
            break;
        default:
            spawnOne(track, 9, 2.65f, 1.7f, 0.9f);
            spawnOne((track + 5 + random.range(3)) % kTracks, 7, 0.76f, 0.76f, 0.82f, 1.2f, 0.5f);
            spawnTimer = 1.42f;
            break;
        }
    }

    void checkCollisions()
    {
        if (invincible > 0.0f) {
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

        for (int row = 0; row < kTubeRows; ++row) {
            const float z0 = 0.42f + row * kTileSize;
            const float z1 = z0 + kTileSize;
            for (int track = 0; track < kTracks; ++track) {
                const Vec3 p00 = tubePoint(track, z0);
                const Vec3 p10 = tubePoint(track + 1, z0);
                const Vec3 p11 = tubePoint(track + 1, z1);
                const Vec3 p01 = tubePoint(track, z1);
                const float checker = ((track + row) & 1) ? 0.93f : 1.0f;
                const Color tile = scaleColor({224, 234, 244}, brightness * checker);
                rasterTriangle(surface, p00, p10, p11, tile);
                rasterTriangle(surface, p00, p11, p01, tile);
            }
        }

        const Color grid = brightStage ? Color {25, 42, 58} : Color {4, 8, 13};
        for (int row = 0; row <= kTubeRows; ++row) {
            const float z = 0.42f + row * kTileSize;
            for (int track = 0; track < kTracks; ++track) {
                rasterLine(surface, tubePoint(track, z), tubePoint(track + 1, z), grid);
            }
        }
        for (int row = 0; row < kTubeRows; ++row) {
            const float z0 = 0.42f + row * kTileSize;
            const float z1 = z0 + kTileSize;
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
        drawText(surface, 88, kHeight - 22, "[A/D] STEER   [P] PAUSE   [R] RESTART", 1,
            {190, 220, 240}, false);
    }

    Random                random;
    std::vector<Obstacle> obstacles;
    float                 elapsed {0.0f};
    float                 score {0.0f};
    float                 rotation {0.0f};
    float                 angularVelocity {0.0f};
    float                 spawnTimer {0.0f};
    float                 invincible {0.0f};
    int                   lives {3};
    int                   stage {0};
    bool                  paused {false};
    bool                  gameOver {false};
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

int main(int argc, char** argv)
{
    const std::string outputPath = argc > 1 ? argv[1] : "cube_runner.ppm";
    const int frameCount = argc > 2 ? std::max(1, std::atoi(argv[2])) : 360;

    cube_runner::Game game;
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

        const bool turnLeft  = keystate(key_left) || keystate(key_A);
        const bool turnRight = keystate(key_right) || keystate(key_D);
        game.update(1.0f / 60.0f, turnLeft, turnRight);
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
