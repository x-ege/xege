/*
 * EGE (Easy Graphics Engine)
 * filename  ege_gdiplus_fallback.cpp
 *
 * Cross-platform fallback implementations for the portable subset of the
 * "EGE_GDIPLUS" enhanced APIs.
 *
 * Motivation:
 * - Many demos call ege_* enhanced helpers (ege_circle/ege_drawimage/transform...).
 * - On non-Windows builds EGE_GDIPLUS is not available, but we still want the
 *   demos to compile and run with compatible core drawing behavior.
 *
 * Notes:
 * - Pens, solid/hatch/texture/linear-gradient brushes, affine transforms, core
 *   primitives, image transfer and text are implemented by the native backend.
 * - Advanced GDI+ path objects and platform-specific controls remain outside
 *   this fallback; see PRD.md for the current compatibility boundary.
 */

#include "ege_head.h"
#include "image.h"

#ifndef EGE_GDIPLUS

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ege
{

namespace
{

static inline ege_transform_matrix identity_matrix()
{
    ege_transform_matrix m;
    m.m11 = 1.0f; m.m12 = 0.0f;
    m.m21 = 0.0f; m.m22 = 1.0f;
    m.m31 = 0.0f; m.m32 = 0.0f;
    return m;
}

static inline ege_transform_matrix mul_matrix(const ege_transform_matrix& a, const ege_transform_matrix& b)
{
    // Matrix layout follows ege.h docs:
    // x' = x*m11 + y*m21 + m31
    // y' = x*m12 + y*m22 + m32
    ege_transform_matrix r;
    r.m11 = a.m11 * b.m11 + a.m21 * b.m12;
    r.m12 = a.m12 * b.m11 + a.m22 * b.m12;

    r.m21 = a.m11 * b.m21 + a.m21 * b.m22;
    r.m22 = a.m12 * b.m21 + a.m22 * b.m22;

    r.m31 = a.m11 * b.m31 + a.m21 * b.m32 + a.m31;
    r.m32 = a.m12 * b.m31 + a.m22 * b.m32 + a.m32;
    return r;
}

static std::mutex g_matrix_mutex;
static std::unordered_map<const void*, ege_transform_matrix> g_transform_map;

enum class PatternKind { None, Linear, Ellipse, Path, Texture };

struct PatternState {
    PatternKind kind = PatternKind::None;
    ege_point start = {0.0f, 0.0f};
    ege_point end = {0.0f, 0.0f};
    color_t startColor = 0;
    color_t endColor = 0;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    PCIMAGE texture = NULL;
};

static std::unordered_map<const void*, PatternState> g_pattern_map;

static PIMAGE resolve_target(PIMAGE pimg)
{
    return pimg ? pimg : gettarget();
}

static ege_transform_matrix get_transform_locked(PIMAGE pimg)
{
    const void* key = static_cast<const void*>(resolve_target(pimg));
    auto it = g_transform_map.find(key);
    if (it != g_transform_map.end()) {
        return it->second;
    }
    return identity_matrix();
}

static void set_transform_locked(PIMAGE pimg, const ege_transform_matrix& m)
{
    const void* key = static_cast<const void*>(resolve_target(pimg));
    g_transform_map[key] = m;
}

static void sync_render_target_transform(PIMAGE pimg, const ege_transform_matrix& matrix)
{
    PIMAGE target = resolve_target(pimg);
    if (!target || !target->m_renderTarget) return;
    const float nativeMatrix[9] = {
        matrix.m11, matrix.m12, 0.0f,
        matrix.m21, matrix.m22, 0.0f,
        matrix.m31, matrix.m32, 1.0f};
    target->m_renderTarget->setTransformMatrix(nativeMatrix);
}

static color_t interpolate_color(color_t first, color_t second, float amount)
{
    amount = std::max(0.0f, std::min(1.0f, amount));
    const float inverse = 1.0f - amount;
    const unsigned int a = (unsigned int)std::lround(EGEGET_A(first) * inverse + EGEGET_A(second) * amount);
    const unsigned int r = (unsigned int)std::lround(EGEGET_R(first) * inverse + EGEGET_R(second) * amount);
    const unsigned int g = (unsigned int)std::lround(EGEGET_G(first) * inverse + EGEGET_G(second) * amount);
    const unsigned int b = (unsigned int)std::lround(EGEGET_B(first) * inverse + EGEGET_B(second) * amount);
    return EGEARGB(a, r, g, b);
}

static color_t sample_pattern(const PatternState& pattern, float x, float y)
{
    if (pattern.kind == PatternKind::Linear) {
        const float dx = pattern.end.x - pattern.start.x;
        const float dy = pattern.end.y - pattern.start.y;
        const float lengthSquared = dx * dx + dy * dy;
        const float amount = lengthSquared > 1e-12f
            ? ((x - pattern.start.x) * dx + (y - pattern.start.y) * dy) / lengthSquared : 0.0f;
        return interpolate_color(pattern.startColor, pattern.endColor, amount);
    }
    if (pattern.kind == PatternKind::Ellipse || pattern.kind == PatternKind::Path) {
        const float radiusX = std::max(0.5f, std::abs(pattern.width) * 0.5f);
        const float radiusY = std::max(0.5f, std::abs(pattern.height) * 0.5f);
        const float dx = (x - pattern.start.x) / radiusX;
        const float dy = (y - pattern.start.y) / radiusY;
        return interpolate_color(pattern.startColor, pattern.endColor, std::sqrt(dx * dx + dy * dy));
    }
    if (pattern.kind == PatternKind::Texture && pattern.texture &&
        pattern.texture->m_width > 0 && pattern.texture->m_height > 0) {
        const color_t* pixels = pattern.texture->getbuffer();
        int sampleX = (int)std::floor(x - pattern.x);
        int sampleY = (int)std::floor(y - pattern.y);
        sampleX %= pattern.texture->m_width;
        sampleY %= pattern.texture->m_height;
        if (sampleX < 0) sampleX += pattern.texture->m_width;
        if (sampleY < 0) sampleY += pattern.texture->m_height;
        return pixels[sampleY * pattern.texture->m_width + sampleX];
    }
    return pattern.startColor;
}

static color_t source_over(color_t destination, color_t source)
{
    const unsigned int alpha = EGEGET_A(source);
    if (alpha == 255) return source;
    if (alpha == 0) return destination;
    const unsigned int inverse = 255 - alpha;
    const unsigned int sourceR = EGEGET_R(source) * alpha / 255;
    const unsigned int sourceG = EGEGET_G(source) * alpha / 255;
    const unsigned int sourceB = EGEGET_B(source) * alpha / 255;
    const unsigned int outA = alpha + EGEGET_A(destination) * inverse / 255;
    const unsigned int outR = sourceR + EGEGET_R(destination) * inverse / 255;
    const unsigned int outG = sourceG + EGEGET_G(destination) * inverse / 255;
    const unsigned int outB = sourceB + EGEGET_B(destination) * inverse / 255;
    return EGEARGB(outA, std::min(255U, outR), std::min(255U, outG), std::min(255U, outB));
}

template <typename Predicate>
static bool fill_pattern_shape(PIMAGE pimg, float left, float top, float right, float bottom, Predicate predicate)
{
    PIMAGE target = resolve_target(pimg);
    if (!target) return false;
    PatternState pattern;
    ege_transform_matrix transform;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        auto found = g_pattern_map.find(static_cast<const void*>(target));
        if (found == g_pattern_map.end() || found->second.kind == PatternKind::None) return false;
        pattern = found->second;
        transform = get_transform_locked(target);
    }

    color_t* pixels = target->getbuffer();
    if (!pixels) return true;
    const float corners[4][2] = {
        {left, top}, {right, top}, {right, bottom}, {left, bottom}};
    float transformedLeft = std::numeric_limits<float>::max();
    float transformedTop = std::numeric_limits<float>::max();
    float transformedRight = std::numeric_limits<float>::lowest();
    float transformedBottom = std::numeric_limits<float>::lowest();
    for (const auto& corner : corners) {
        const float x = corner[0] * transform.m11 + corner[1] * transform.m21 + transform.m31;
        const float y = corner[0] * transform.m12 + corner[1] * transform.m22 + transform.m32;
        transformedLeft = std::min(transformedLeft, x);
        transformedTop = std::min(transformedTop, y);
        transformedRight = std::max(transformedRight, x);
        transformedBottom = std::max(transformedBottom, y);
    }
    const float determinant = transform.m11 * transform.m22 - transform.m21 * transform.m12;
    if (std::abs(determinant) < 1e-12f) return true;

    const int logicalLeft = (int)std::floor(transformedLeft);
    const int logicalTop = (int)std::floor(transformedTop);
    const int logicalRight = (int)std::ceil(transformedRight);
    const int logicalBottom = (int)std::ceil(transformedBottom);
    const int clipLeft = target->m_enableclip ? std::max(0, target->m_vpt.left) : 0;
    const int clipTop = target->m_enableclip ? std::max(0, target->m_vpt.top) : 0;
    const int clipRight = target->m_enableclip ? std::min(target->m_width, target->m_vpt.right) : target->m_width;
    const int clipBottom = target->m_enableclip ? std::min(target->m_height, target->m_vpt.bottom) : target->m_height;
    for (int logicalY = logicalTop; logicalY < logicalBottom; ++logicalY) {
        const int physicalY = logicalY + target->m_vpt.top;
        if (physicalY < clipTop || physicalY >= clipBottom) continue;
        for (int logicalX = logicalLeft; logicalX < logicalRight; ++logicalX) {
            const int physicalX = logicalX + target->m_vpt.left;
            if (physicalX < clipLeft || physicalX >= clipRight) continue;
            const float transformedX = logicalX + 0.5f - transform.m31;
            const float transformedY = logicalY + 0.5f - transform.m32;
            const float sourceX = (transformedX * transform.m22 - transformedY * transform.m21) / determinant;
            const float sourceY = (transformedY * transform.m11 - transformedX * transform.m12) / determinant;
            if (!predicate(sourceX, sourceY)) continue;
            color_t& destination = pixels[physicalY * target->m_width + physicalX];
            destination = source_over(destination, sample_pattern(pattern, sourceX, sourceY));
        }
    }
    return true;
}

static inline int iround(float v)
{
    return static_cast<int>(std::lround(static_cast<double>(v)));
}

static void append_round_rect_arc(std::vector<ege_point>& points,
                                  float centerX, float centerY, float radius,
                                  float startAngle, float endAngle)
{
    constexpr float kPi = 3.14159265358979323846f;
    if (radius <= 0.0f) {
        const float angle = endAngle * kPi / 180.0f;
        points.push_back({centerX + std::cos(angle) * radius,
                          centerY + std::sin(angle) * radius});
        return;
    }

    const int segments = std::max(3, std::min(32,
        static_cast<int>(std::ceil(radius * kPi / 4.0f))));
    for (int i = 0; i <= segments; ++i) {
        const float amount = static_cast<float>(i) / static_cast<float>(segments);
        const float angle = (startAngle + (endAngle - startAngle) * amount) * kPi / 180.0f;
        points.push_back({centerX + std::cos(angle) * radius,
                          centerY + std::sin(angle) * radius});
    }
}

static bool build_round_rect_path(float x, float y, float width, float height,
                                  float radius1, float radius2,
                                  float radius3, float radius4,
                                  std::vector<ege_point>& points)
{
    if (width <= 0.0f || height <= 0.0f) return false;

    const auto clampRadius = [](float value, float maximum) {
        return std::max(0.0f, std::min(value, std::max(0.0f, maximum)));
    };
    radius1 = clampRadius(radius1, std::min(width, height));
    radius2 = clampRadius(radius2, std::min(width - radius1, height));
    radius3 = clampRadius(radius3, std::min(height - radius2, width));
    radius4 = clampRadius(radius4, std::min(height - radius1, width - radius3));

    points.clear();
    points.reserve(64);
    points.push_back({x + radius1, y});
    append_round_rect_arc(points, x + width - radius2, y + radius2,
                          radius2, -90.0f, 0.0f);
    append_round_rect_arc(points, x + width - radius3, y + height - radius3,
                          radius3, 0.0f, 90.0f);
    append_round_rect_arc(points, x + radius4, y + height - radius4,
                          radius4, 90.0f, 180.0f);
    append_round_rect_arc(points, x + radius1, y + radius1,
                          radius1, 180.0f, 270.0f);
    points.push_back(points.front());
    return true;
}

} // anonymous namespace

void ege_line(float x1, float y1, float x2, float y2, PIMAGE pimg)
{
    line(iround(x1), iround(y1), iround(x2), iround(y2), pimg);
}

void ege_drawpoly(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    if (numOfPoints <= 0 || points == NULL) {
        return;
    }

    std::vector<int> ipoints;
    ipoints.resize(static_cast<size_t>(numOfPoints) * 2);
    for (int i = 0; i < numOfPoints; ++i) {
        ipoints[static_cast<size_t>(i) * 2 + 0] = iround(points[i].x);
        ipoints[static_cast<size_t>(i) * 2 + 1] = iround(points[i].y);
    }
    drawpoly(numOfPoints, ipoints.data(), pimg);
}

void ege_fillpoly(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    if (numOfPoints <= 0 || points == NULL) {
        return;
    }

    float minX = points[0].x, maxX = points[0].x;
    float minY = points[0].y, maxY = points[0].y;
    for (int i = 1; i < numOfPoints; ++i) {
        minX = std::min(minX, points[i].x); maxX = std::max(maxX, points[i].x);
        minY = std::min(minY, points[i].y); maxY = std::max(maxY, points[i].y);
    }
    if (fill_pattern_shape(pimg, minX, minY, maxX, maxY, [&](float x, float y) {
        bool inside = false;
        for (int i = 0, j = numOfPoints - 1; i < numOfPoints; j = i++) {
            const bool crosses = ((points[i].y > y) != (points[j].y > y)) &&
                (x < (points[j].x - points[i].x) * (y - points[i].y) /
                     (points[j].y - points[i].y) + points[i].x);
            if (crosses) inside = !inside;
        }
        return inside;
    })) return;

    std::vector<int> ipoints;
    ipoints.resize(static_cast<size_t>(numOfPoints) * 2);
    for (int i = 0; i < numOfPoints; ++i) {
        ipoints[static_cast<size_t>(i) * 2 + 0] = iround(points[i].x);
        ipoints[static_cast<size_t>(i) * 2 + 1] = iround(points[i].y);
    }
    solidpoly(numOfPoints, ipoints.data(), pimg);
}

void ege_fillrect(float x, float y, float w, float h, PIMAGE pimg)
{
    if (w <= 0.0f || h <= 0.0f) return;
    if (fill_pattern_shape(pimg, x, y, x + w, y + h,
                           [&](float, float) { return true; })) return;
    const int left = iround(x);
    const int top = iround(y);
    const int right = iround(x + w);
    const int bottom = iround(y + h);
    solidrect(left, top, right, bottom, pimg);
}

void ege_circle(float x, float y, float radius, PIMAGE pimg)
{
    circle(iround(x), iround(y), iround(radius), pimg);
}

void ege_fillcircle(float x, float y, float radius, PIMAGE pimg)
{
    ege_fillellipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, pimg);
}

void ege_ellipse(float x, float y, float w, float h, PIMAGE pimg)
{
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float rx = w * 0.5f;
    const float ry = h * 0.5f;
    ellipsef(cx, cy, 0.0f, 360.0f, rx, ry, pimg);
}

void ege_fillellipse(float x, float y, float w, float h, PIMAGE pimg)
{
    if (w <= 0.0f || h <= 0.0f) return;
    const float radiusX = std::abs(w) * 0.5f;
    const float radiusY = std::abs(h) * 0.5f;
    const float centerX = x + w * 0.5f;
    const float centerY = y + h * 0.5f;
    if (radiusX > 0.0f && radiusY > 0.0f &&
        fill_pattern_shape(pimg, x, y, x + w, y + h, [&](float px, float py) {
            const float dx = (px - centerX) / radiusX;
            const float dy = (py - centerY) / radiusY;
            return dx * dx + dy * dy <= 1.0f;
        })) return;
    solidellipsef(centerX, centerY, radiusX, radiusY, pimg);
}

void ege_roundrect(float x, float y, float w, float h, float radius, PIMAGE pimg)
{
    ege_roundrect(x, y, w, h, radius, radius, radius, radius, pimg);
}

void ege_fillroundrect(float x, float y, float w, float h, float radius, PIMAGE pimg)
{
    ege_fillroundrect(x, y, w, h, radius, radius, radius, radius, pimg);
}

void ege_roundrect(float x, float y, float w, float h, float radius1, float radius2, float radius3, float radius4, PIMAGE pimg)
{
    std::vector<ege_point> points;
    if (build_round_rect_path(x, y, w, h, radius1, radius2, radius3, radius4, points)) {
        ege_drawpoly(static_cast<int>(points.size()), points.data(), pimg);
    }
}

void ege_fillroundrect(float x, float y, float w, float h, float radius1, float radius2, float radius3, float radius4, PIMAGE pimg)
{
    std::vector<ege_point> points;
    if (build_round_rect_path(x, y, w, h, radius1, radius2, radius3, radius4, points)) {
        ege_fillpoly(static_cast<int>(points.size()), points.data(), pimg);
    }
}

void ege_setpattern_none(PIMAGE pimg)
{
    PIMAGE target = resolve_target(pimg);
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(target)] = PatternState();
}

void ege_setpattern_lineargradient(float x1, float y1, color_t c1,
                                   float x2, float y2, color_t c2, PIMAGE pimg)
{
    PatternState pattern;
    pattern.kind = PatternKind::Linear;
    pattern.start = {x1, y1}; pattern.end = {x2, y2};
    pattern.startColor = c1; pattern.endColor = c2;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(resolve_target(pimg))] = pattern;
}

void ege_setpattern_pathgradient(ege_point center, color_t centerColor,
    int count, const ege_point* points, int colorCount, const color_t* pointColors, PIMAGE pimg)
{
    PatternState pattern;
    pattern.kind = PatternKind::Path;
    pattern.start = center;
    pattern.startColor = centerColor;
    pattern.endColor = colorCount > 0 && pointColors ? pointColors[0] : centerColor;
    float radiusX = 1.0f, radiusY = 1.0f;
    for (int i = 0; points && i < count; ++i) {
        radiusX = std::max(radiusX, std::abs(points[i].x - center.x));
        radiusY = std::max(radiusY, std::abs(points[i].y - center.y));
    }
    pattern.width = radiusX * 2.0f; pattern.height = radiusY * 2.0f;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(resolve_target(pimg))] = pattern;
}

void ege_setpattern_ellipsegradient(ege_point center, color_t centerColor,
    float x, float y, float w, float h, color_t outerColor, PIMAGE pimg)
{
    PatternState pattern;
    pattern.kind = PatternKind::Ellipse;
    pattern.start = center;
    pattern.startColor = centerColor; pattern.endColor = outerColor;
    pattern.x = x; pattern.y = y; pattern.width = w; pattern.height = h;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(resolve_target(pimg))] = pattern;
}

void ege_setpattern_texture(PIMAGE srcimg, float x, float y, float w, float h, PIMAGE pimg)
{
    PatternState pattern;
    pattern.kind = PatternKind::Texture;
    pattern.texture = srcimg;
    pattern.x = x; pattern.y = y; pattern.width = w; pattern.height = h;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(resolve_target(pimg))] = pattern;
}

void ege_drawimage(PCIMAGE imgSrc, int xDest, int yDest, PIMAGE pimg)
{
    PIMAGE target = resolve_target(pimg);
    if (!target || !imgSrc) return;
    RenderTarget* sourceTarget = target->m_renderTarget
        ? imgSrc->getRenderTargetForSampling() : NULL;
    if (target->m_renderTarget && sourceTarget) {
        const ege_point corners[4] = {
            ege_transform_calc((float)xDest, (float)yDest, target),
            ege_transform_calc((float)(xDest + imgSrc->getwidth()), (float)yDest, target),
            ege_transform_calc((float)(xDest + imgSrc->getwidth()),
                               (float)(yDest + imgSrc->getheight()), target),
            ege_transform_calc((float)xDest, (float)(yDest + imgSrc->getheight()), target)};
        const float destinationPoints[8] = {
            corners[0].x, corners[0].y, corners[1].x, corners[1].y,
            corners[2].x, corners[2].y, corners[3].x, corners[3].y};
        target->m_renderTarget->blitAffine(sourceTarget,
                                           0, 0, imgSrc->getwidth(), imgSrc->getheight(),
                                           destinationPoints, true, false);
        return;
    }
    putimage(target, xDest, yDest, imgSrc, SRCCOPY);
}

void ege_drawimage(PCIMAGE imgSrc,
    int xDest, int yDest, int widthDest, int heightDest,
    int xSrc,  int ySrc,  int widthSrc,  int heightSrc,
    PIMAGE pimg)
{
    PIMAGE target = resolve_target(pimg);
    if (!target || !imgSrc || widthSrc <= 0 || heightSrc <= 0) return;
    RenderTarget* sourceTarget = target->m_renderTarget
        ? imgSrc->getRenderTargetForSampling() : NULL;
    if (target->m_renderTarget && sourceTarget) {
        const ege_point corners[4] = {
            ege_transform_calc((float)xDest, (float)yDest, target),
            ege_transform_calc((float)(xDest + widthDest), (float)yDest, target),
            ege_transform_calc((float)(xDest + widthDest), (float)(yDest + heightDest), target),
            ege_transform_calc((float)xDest, (float)(yDest + heightDest), target)};
        const float destinationPoints[8] = {
            corners[0].x, corners[0].y, corners[1].x, corners[1].y,
            corners[2].x, corners[2].y, corners[3].x, corners[3].y};
        target->m_renderTarget->blitAffine(sourceTarget,
                                           xSrc, ySrc, widthSrc, heightSrc,
                                           destinationPoints, true, false);
        return;
    }
    putimage(target, xDest, yDest, widthDest, heightDest,
             imgSrc, xSrc, ySrc, widthSrc, heightSrc, SRCCOPY);
}

void ege_transform_rotate(float angle, PIMAGE pimg)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float rad = angle * kPi / 180.0f;
    const float c = std::cos(rad);
    const float s = std::sin(rad);

    ege_transform_matrix r;
    r.m11 = c;  r.m12 = s;
    r.m21 = -s; r.m22 = c;
    r.m31 = 0.0f; r.m32 = 0.0f;

    ege_transform_matrix updated;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        updated = mul_matrix(get_transform_locked(pimg), r);
        set_transform_locked(pimg, updated);
    }
    sync_render_target_transform(pimg, updated);
}

void ege_transform_translate(float x, float y, PIMAGE pimg)
{
    ege_transform_matrix t = identity_matrix();
    t.m31 = x;
    t.m32 = y;

    ege_transform_matrix updated;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        updated = mul_matrix(get_transform_locked(pimg), t);
        set_transform_locked(pimg, updated);
    }
    sync_render_target_transform(pimg, updated);
}

void ege_transform_scale(float xScale, float yScale, PIMAGE pimg)
{
    ege_transform_matrix s = identity_matrix();
    s.m11 = xScale;
    s.m22 = yScale;

    ege_transform_matrix updated;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        updated = mul_matrix(get_transform_locked(pimg), s);
        set_transform_locked(pimg, updated);
    }
    sync_render_target_transform(pimg, updated);
}

void ege_transform_reset(PIMAGE pimg)
{
    const ege_transform_matrix identity = identity_matrix();
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        set_transform_locked(pimg, identity);
    }
    sync_render_target_transform(pimg, identity);
}

void ege_get_transform(ege_transform_matrix* matrix, PIMAGE pimg)
{
    if (!matrix) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    *matrix = get_transform_locked(pimg);
}

void ege_set_transform(const ege_transform_matrix* matrix, PIMAGE pimg)
{
    const ege_transform_matrix value = matrix ? *matrix : identity_matrix();
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        set_transform_locked(pimg, value);
    }
    sync_render_target_transform(pimg, value);
}

ege_point ege_transform_calc(ege_point p, PIMAGE pimg)
{
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    const ege_transform_matrix m = get_transform_locked(pimg);

    ege_point out;
    out.x = p.x * m.m11 + p.y * m.m21 + m.m31;
    out.y = p.x * m.m12 + p.y * m.m22 + m.m32;
    return out;
}

ege_point ege_transform_calc(float x, float y, PIMAGE pimg)
{
    ege_point p;
    p.x = x;
    p.y = y;
    return ege_transform_calc(p, pimg);
}

} // namespace ege

#endif // !EGE_GDIPLUS
