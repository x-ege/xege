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
 * - Pens, patterns, affine transforms, enhanced primitives, image/texture
 *   transfer, and graphics paths have native CPU/render-target fallbacks.
 * - Geometry that has no portable platform primitive (for example path warp
 *   and text outlines) uses deterministic flattened CPU geometry.
 */

#include "ege_head.h"
#include "ege_graph.h"
#include "encodeconv.h"
#include "image.h"

#ifndef EGE_GDIPLUS

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

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

static inline ege_transform_matrix prepend_matrix(const ege_transform_matrix& current,
                                                   const ege_transform_matrix& operation)
{
    // Matrix layout follows ege.h docs:
    // x' = x*m11 + y*m21 + m31
    // y' = x*m12 + y*m22 + m32
    // GDI+ transform helpers default to MatrixOrderPrepend. With its row-vector
    // representation that means updated = operation * current.
    const ege_transform_matrix& a = current;
    const ege_transform_matrix& b = operation;
    ege_transform_matrix r;
    r.m11 = a.m11 * b.m11 + a.m21 * b.m12;
    r.m12 = a.m12 * b.m11 + a.m22 * b.m12;

    r.m21 = a.m11 * b.m21 + a.m21 * b.m22;
    r.m22 = a.m12 * b.m21 + a.m22 * b.m22;

    r.m31 = a.m11 * b.m31 + a.m21 * b.m32 + a.m31;
    r.m32 = a.m12 * b.m31 + a.m22 * b.m32 + a.m32;
    return r;
}

// IMAGE instances owned by graph_setting can outlive ordinary translation-unit
// statics during process shutdown. Keep this small registry alive until the OS
// reclaims the process so IMAGE destructors never lock a destroyed mutex.
static std::mutex& g_matrix_mutex = *new std::mutex;
static std::unordered_map<const void*, ege_transform_matrix>& g_transform_map =
    *new std::unordered_map<const void*, ege_transform_matrix>;

enum class PatternKind { None, Linear, Ellipse, Path, Texture };

struct TextureSnapshot {
    int width = 0;
    int height = 0;
    std::vector<color_t> pixels;
};

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
    std::vector<ege_point> boundaryPoints;
    std::vector<color_t> surroundColors;
    std::shared_ptr<const TextureSnapshot> texture;
};

static std::unordered_map<const void*, PatternState>& g_pattern_map =
    *new std::unordered_map<const void*, PatternState>;
static std::unordered_map<const void*, std::shared_ptr<const TextureSnapshot> >& g_texture_map =
    *new std::unordered_map<const void*, std::shared_ptr<const TextureSnapshot> >;

static PIMAGE resolve_target(PIMAGE pimg)
{
    // gettarget() intentionally returns only an explicitly selected target.
    // CONVERT_IMAGE_CONST also resolves the default active page, which is what
    // every public drawing API means when pimg is null.
    return const_cast<PIMAGE>(CONVERT_IMAGE_CONST(pimg));
}

static std::shared_ptr<const TextureSnapshot> make_texture_snapshot(PCIMAGE image)
{
    if (image == NULL || image->m_width <= 0 || image->m_height <= 0) {
        return std::shared_ptr<const TextureSnapshot>();
    }
    const color_t* source = image->getbuffer();
    if (source == NULL) {
        return std::shared_ptr<const TextureSnapshot>();
    }
    std::shared_ptr<TextureSnapshot> snapshot(new(std::nothrow) TextureSnapshot);
    if (!snapshot) {
        return std::shared_ptr<const TextureSnapshot>();
    }
    snapshot->width = image->m_width;
    snapshot->height = image->m_height;
    try {
        snapshot->pixels.assign(
            source, source + static_cast<std::size_t>(snapshot->width) * snapshot->height);
    } catch (...) {
        return std::shared_ptr<const TextureSnapshot>();
    }
    return snapshot;
}

static void release_fallback_state(const IMAGE* image)
{
    if (image == NULL) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    const void* key = static_cast<const void*>(image);
    g_transform_map.erase(key);
    g_pattern_map.erase(key);
    g_texture_map.erase(key);
}

static ege_transform_matrix get_transform_locked(PIMAGE pimg)
{
    PIMAGE target = resolve_target(pimg);
    if (target == NULL) return identity_matrix();
    const void* key = static_cast<const void*>(target);
    auto it = g_transform_map.find(key);
    if (it != g_transform_map.end()) {
        return it->second;
    }
    return identity_matrix();
}

static void set_transform_locked(PIMAGE pimg, const ege_transform_matrix& m)
{
    PIMAGE target = resolve_target(pimg);
    if (target == NULL) return;
    const void* key = static_cast<const void*>(target);
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

static float wrap_tile_amount(float amount)
{
    if (!std::isfinite(amount)) return 0.0f;
    amount -= std::floor(amount);
    return amount < 0.0f ? amount + 1.0f : amount;
}

static float wrap_tile_coordinate(float coordinate, float origin, float extent)
{
    extent = std::abs(extent);
    if (extent <= 1e-12f) return origin;
    return origin + wrap_tile_amount((coordinate - origin) / extent) * extent;
}

static float cross_product(float firstX, float firstY, float secondX, float secondY)
{
    return firstX * secondY - firstY * secondX;
}

static color_t path_surround_color(const PatternState& pattern, std::size_t index)
{
    if (pattern.surroundColors.empty()) return pattern.endColor;
    if (pattern.surroundColors.size() == 1) return pattern.surroundColors.front();
    return pattern.surroundColors[std::min(index, pattern.surroundColors.size() - 1)];
}

static color_t sample_path_gradient(const PatternState& pattern, float x, float y)
{
    const std::size_t count = pattern.boundaryPoints.size();
    if (count < 2) return pattern.startColor;

    // PathGradientBrush was created with WrapModeTile. Tile the complete brush
    // bounds before tracing a ray from its configured center to the polygon.
    x = wrap_tile_coordinate(x, pattern.x, pattern.width);
    y = wrap_tile_coordinate(y, pattern.y, pattern.height);
    const float directionX = x - pattern.start.x;
    const float directionY = y - pattern.start.y;
    if (directionX * directionX + directionY * directionY <= 1e-12f) {
        return pattern.startColor;
    }

    float closestAmount = std::numeric_limits<float>::infinity();
    float edgeAmount = 0.0f;
    std::size_t edgeIndex = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const ege_point& first = pattern.boundaryPoints[index];
        const ege_point& second = pattern.boundaryPoints[(index + 1) % count];
        const float edgeX = second.x - first.x;
        const float edgeY = second.y - first.y;
        const float denominator = cross_product(directionX, directionY, edgeX, edgeY);
        if (std::abs(denominator) <= 1e-12f) continue;
        const float offsetX = first.x - pattern.start.x;
        const float offsetY = first.y - pattern.start.y;
        const float rayAmount = cross_product(offsetX, offsetY, edgeX, edgeY) / denominator;
        const float segmentAmount = cross_product(offsetX, offsetY, directionX, directionY) /
            denominator;
        if (rayAmount >= 0.0f && segmentAmount >= -1e-5f && segmentAmount <= 1.0f + 1e-5f &&
            rayAmount < closestAmount) {
            closestAmount = rayAmount;
            edgeAmount = std::max(0.0f, std::min(1.0f, segmentAmount));
            edgeIndex = index;
        }
    }
    if (!std::isfinite(closestAmount) || closestAmount <= 1e-12f) {
        return pattern.endColor;
    }

    const color_t firstColor = path_surround_color(pattern, edgeIndex);
    const color_t secondColor = path_surround_color(pattern, (edgeIndex + 1) % count);
    const color_t boundaryColor = interpolate_color(firstColor, secondColor, edgeAmount);
    return interpolate_color(pattern.startColor, boundaryColor, 1.0f / closestAmount);
}

static color_t sample_ellipse_gradient(const PatternState& pattern, float x, float y)
{
    const float radiusX = std::abs(pattern.width) * 0.5f;
    const float radiusY = std::abs(pattern.height) * 0.5f;
    if (radiusX <= 1e-12f || radiusY <= 1e-12f) return pattern.endColor;
    const float ellipseCenterX = pattern.x + pattern.width * 0.5f;
    const float ellipseCenterY = pattern.y + pattern.height * 0.5f;
    const float directionX = x - pattern.start.x;
    const float directionY = y - pattern.start.y;
    if (directionX * directionX + directionY * directionY <= 1e-12f) {
        return pattern.startColor;
    }

    const float offsetX = pattern.start.x - ellipseCenterX;
    const float offsetY = pattern.start.y - ellipseCenterY;
    const float inverseRadiusX2 = 1.0f / (radiusX * radiusX);
    const float inverseRadiusY2 = 1.0f / (radiusY * radiusY);
    const float a = directionX * directionX * inverseRadiusX2 +
        directionY * directionY * inverseRadiusY2;
    const float b = 2.0f * (offsetX * directionX * inverseRadiusX2 +
        offsetY * directionY * inverseRadiusY2);
    const float c = offsetX * offsetX * inverseRadiusX2 +
        offsetY * offsetY * inverseRadiusY2 - 1.0f;
    const float discriminant = b * b - 4.0f * a * c;
    if (a <= 1e-12f || discriminant < 0.0f) return pattern.endColor;
    const float root = std::sqrt(std::max(0.0f, discriminant));
    const float first = (-b - root) / (2.0f * a);
    const float second = (-b + root) / (2.0f * a);
    float boundaryAmount = std::numeric_limits<float>::infinity();
    if (first > 1e-12f) boundaryAmount = first;
    if (second > 1e-12f) boundaryAmount = std::min(boundaryAmount, second);
    if (!std::isfinite(boundaryAmount)) return pattern.endColor;
    return interpolate_color(pattern.startColor, pattern.endColor, 1.0f / boundaryAmount);
}

static color_t sample_pattern(const PatternState& pattern, float x, float y)
{
    if (pattern.kind == PatternKind::Linear) {
        const float dx = pattern.end.x - pattern.start.x;
        const float dy = pattern.end.y - pattern.start.y;
        const float lengthSquared = dx * dx + dy * dy;
        const float amount = lengthSquared > 1e-12f
            ? ((x - pattern.start.x) * dx + (y - pattern.start.y) * dy) / lengthSquared : 0.0f;
        return interpolate_color(pattern.startColor, pattern.endColor, wrap_tile_amount(amount));
    }
    if (pattern.kind == PatternKind::Ellipse) return sample_ellipse_gradient(pattern, x, y);
    if (pattern.kind == PatternKind::Path) return sample_path_gradient(pattern, x, y);
    if (pattern.kind == PatternKind::Texture && pattern.texture &&
        pattern.texture->width > 0 && pattern.texture->height > 0 &&
        !pattern.texture->pixels.empty()) {
        const int cropLeft = std::max(0, static_cast<int>(std::floor(pattern.x)));
        const int cropTop = std::max(0, static_cast<int>(std::floor(pattern.y)));
        const int cropRight = std::min(pattern.texture->width,
            static_cast<int>(std::ceil(pattern.x + pattern.width)));
        const int cropBottom = std::min(pattern.texture->height,
            static_cast<int>(std::ceil(pattern.y + pattern.height)));
        const int cropWidth = cropRight - cropLeft;
        const int cropHeight = cropBottom - cropTop;
        if (cropWidth <= 0 || cropHeight <= 0) return 0;
        int sampleX = static_cast<int>(std::floor(x)) % cropWidth;
        int sampleY = static_cast<int>(std::floor(y)) % cropHeight;
        if (sampleX < 0) sampleX += cropWidth;
        if (sampleY < 0) sampleY += cropHeight;
        sampleX += cropLeft;
        sampleY += cropTop;
        return pattern.texture->pixels[static_cast<std::size_t>(sampleY) *
                                       pattern.texture->width + sampleX];
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

static color_t source_over_premultiplied(color_t destination, color_t source)
{
    const unsigned int alpha = EGEGET_A(source);
    if (alpha == 255) return source;
    if (alpha == 0) return destination;
    const unsigned int inverse = 255 - alpha;
    const unsigned int outA = alpha + EGEGET_A(destination) * inverse / 255;
    const unsigned int outR = EGEGET_R(source) + EGEGET_R(destination) * inverse / 255;
    const unsigned int outG = EGEGET_G(source) + EGEGET_G(destination) * inverse / 255;
    const unsigned int outB = EGEGET_B(source) + EGEGET_B(destination) * inverse / 255;
    return EGEARGB(outA, std::min(255U, outR), std::min(255U, outG), std::min(255U, outB));
}

static color_t scale_straight_alpha(color_t color, float coverage)
{
    const unsigned int alpha = static_cast<unsigned int>(std::lround(
        EGEGET_A(color) * std::max(0.0f, std::min(1.0f, coverage))));
    return EGEARGB(alpha, EGEGET_R(color), EGEGET_G(color), EGEGET_B(color));
}

static color_t scale_premultiplied(color_t color, float coverage)
{
    coverage = std::max(0.0f, std::min(1.0f, coverage));
    return EGEARGB(
        static_cast<unsigned int>(std::lround(EGEGET_A(color) * coverage)),
        static_cast<unsigned int>(std::lround(EGEGET_R(color) * coverage)),
        static_cast<unsigned int>(std::lround(EGEGET_G(color) * coverage)),
        static_cast<unsigned int>(std::lround(EGEGET_B(color) * coverage)));
}

template <typename Predicate>
static bool fill_cpu_shape(PIMAGE pimg, float left, float top, float right, float bottom,
                           Predicate predicate, bool requirePattern)
{
    PIMAGE target = resolve_target(pimg);
    if (!target) return false;
    PatternState pattern;
    ege_transform_matrix transform;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        auto found = g_pattern_map.find(static_cast<const void*>(target));
        if (found == g_pattern_map.end() || found->second.kind == PatternKind::None) {
            if (requirePattern) return false;
            pattern.kind = PatternKind::None;
            pattern.startColor = target->m_fillcolor;
        } else {
        pattern = found->second;
        }
        transform = get_transform_locked(target);
    }

    if (right < left) std::swap(left, right);
    if (bottom < top) std::swap(top, bottom);
    color_t* pixels = target->getbuffer(IMAGE_BUFFER_READ_WRITE);
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
    const int sampleGrid = target->isAntiAliasingEnabled() ? 4 : 1;
    const int sampleCount = sampleGrid * sampleGrid;
    for (int logicalY = logicalTop; logicalY < logicalBottom; ++logicalY) {
        const int physicalY = logicalY + target->m_vpt.top;
        if (physicalY < clipTop || physicalY >= clipBottom) continue;
        for (int logicalX = logicalLeft; logicalX < logicalRight; ++logicalX) {
            const int physicalX = logicalX + target->m_vpt.left;
            if (physicalX < clipLeft || physicalX >= clipRight) continue;
            int covered = 0;
            for (int sampleY = 0; sampleY < sampleGrid; ++sampleY) {
                for (int sampleX = 0; sampleX < sampleGrid; ++sampleX) {
                    const float offsetX = (sampleX + 0.5f) / sampleGrid;
                    const float offsetY = (sampleY + 0.5f) / sampleGrid;
                    const float transformedX = logicalX + offsetX - transform.m31;
                    const float transformedY = logicalY + offsetY - transform.m32;
                    const float sourceX = (transformedX * transform.m22 -
                        transformedY * transform.m21) / determinant;
                    const float sourceY = (transformedY * transform.m11 -
                        transformedX * transform.m12) / determinant;
                    if (predicate(sourceX, sourceY)) ++covered;
                }
            }
            if (covered == 0) continue;
            const float transformedX = logicalX + 0.5f - transform.m31;
            const float transformedY = logicalY + 0.5f - transform.m32;
            const float sourceX = (transformedX * transform.m22 - transformedY * transform.m21) /
                determinant;
            const float sourceY = (transformedY * transform.m11 - transformedX * transform.m12) /
                determinant;
            const float coverage = static_cast<float>(covered) / sampleCount;
            color_t& destination = pixels[physicalY * target->m_width + physicalX];
            const color_t sampled = sample_pattern(pattern, sourceX, sourceY);
            const color_t source = pattern.kind == PatternKind::Texture
                ? scale_premultiplied(sampled, coverage)
                : scale_straight_alpha(sampled, coverage);
            destination = pattern.kind == PatternKind::Texture
                ? source_over_premultiplied(destination, source)
                : source_over(destination, source);
        }
    }
    return true;
}

template <typename Predicate>
static bool fill_pattern_shape(PIMAGE pimg, float left, float top, float right, float bottom,
                               Predicate predicate)
{
    // Enhanced GDI+ fills always use SourceOver and are independent of the
    // legacy ROP selected by setwritemode. The CPU path provides those
    // semantics for both solid brushes and configured patterns.
    return fill_cpu_shape(pimg, left, top, right, bottom, predicate, false);
}

static bool stroke_dash_visible(int lineStyle, float distance, float lineWidth)
{
    const float unit = std::max(1.0f, lineWidth);
    float period = 0.0f;
    float firstEnd = 0.0f;
    float secondStart = 0.0f;
    float secondEnd = 0.0f;
    switch (lineStyle) {
    case PS_DASH:
        period = 4.0f * unit; firstEnd = 3.0f * unit; break;
    case PS_DOT:
        period = 2.0f * unit; firstEnd = unit; break;
    case PS_DASHDOT:
        period = 6.0f * unit; firstEnd = 3.0f * unit;
        secondStart = 4.0f * unit; secondEnd = 5.0f * unit; break;
    case PS_DASHDOTDOT:
        period = 8.0f * unit; firstEnd = 3.0f * unit;
        secondStart = 4.0f * unit; secondEnd = 7.0f * unit; break;
    default:
        return true;
    }
    const float position = wrap_tile_amount(distance / period) * period;
    return position < firstEnd || (secondEnd > secondStart &&
        position >= secondStart && position < secondEnd);
}

static bool point_in_triangle(float x, float y, const ege_point& first,
                              const ege_point& second, const ege_point& third)
{
    const float firstCross = cross_product(
        second.x - first.x, second.y - first.y, x - first.x, y - first.y);
    const float secondCross = cross_product(
        third.x - second.x, third.y - second.y, x - second.x, y - second.y);
    const float thirdCross = cross_product(
        first.x - third.x, first.y - third.y, x - third.x, y - third.y);
    const bool hasNegative = firstCross < -1e-5f || secondCross < -1e-5f || thirdCross < -1e-5f;
    const bool hasPositive = firstCross > 1e-5f || secondCross > 1e-5f || thirdCross > 1e-5f;
    return !(hasNegative && hasPositive);
}

static bool stroke_cap_covers(float x, float y, const ege_point& endpoint,
                              float directionX, float directionY, float halfWidth,
                              line_cap_type cap, bool startCap)
{
    if (cap == LINECAP_FLAT) return false;
    const float offsetX = x - endpoint.x;
    const float offsetY = y - endpoint.y;
    if (cap == LINECAP_ROUND) {
        return offsetX * offsetX + offsetY * offsetY <= halfWidth * halfWidth + 1e-5f;
    }
    if (cap != LINECAP_SQUARE) return false;
    const float along = (offsetX * directionX + offsetY * directionY) *
        (startCap ? -1.0f : 1.0f);
    const float perpendicular = std::abs(offsetX * -directionY + offsetY * directionX);
    return along >= -1e-5f && along <= halfWidth + 1e-5f &&
        perpendicular <= halfWidth + 1e-5f;
}

static bool stroke_join_covers(float x, float y, const ege_point& vertex,
                               float previousX, float previousY,
                               float nextX, float nextY, float halfWidth,
                               line_join_type join, float miterLimit)
{
    const float turn = cross_product(previousX, previousY, nextX, nextY);
    if (std::abs(turn) <= 1e-6f) return false;
    if (join == LINEJOIN_ROUND) {
        const float offsetX = x - vertex.x;
        const float offsetY = y - vertex.y;
        return offsetX * offsetX + offsetY * offsetY <= halfWidth * halfWidth + 1e-5f;
    }

    // The outside of a turn is opposite the normals pointing into the turn.
    const float outsideSign = turn > 0.0f ? -1.0f : 1.0f;
    const ege_point previousOuter = {
        vertex.x - previousY * outsideSign * halfWidth,
        vertex.y + previousX * outsideSign * halfWidth};
    const ege_point nextOuter = {
        vertex.x - nextY * outsideSign * halfWidth,
        vertex.y + nextX * outsideSign * halfWidth};
    if (point_in_triangle(x, y, vertex, previousOuter, nextOuter)) return true;
    if (join != LINEJOIN_MITER) return false;

    const float offsetX = nextOuter.x - previousOuter.x;
    const float offsetY = nextOuter.y - previousOuter.y;
    const float lineCross = cross_product(previousX, previousY, nextX, nextY);
    if (std::abs(lineCross) <= 1e-6f) return false;
    const float previousAmount = cross_product(offsetX, offsetY, nextX, nextY) / lineCross;
    const ege_point miterPoint = {
        previousOuter.x + previousAmount * previousX,
        previousOuter.y + previousAmount * previousY};
    const float miterX = miterPoint.x - vertex.x;
    const float miterY = miterPoint.y - vertex.y;
    const float maximumMiter = std::max(1.0f, miterLimit) * halfWidth;
    if (miterX * miterX + miterY * miterY > maximumMiter * maximumMiter + 1e-5f) {
        return false;
    }
    return point_in_triangle(x, y, previousOuter, miterPoint, nextOuter);
}

static bool stroke_cpu_polyline(PIMAGE pimg, const ege_point* points, int count, bool closed)
{
    PIMAGE target = resolve_target(pimg);
    if (target == NULL || points == NULL || count < 2 ||
        target->m_linestyle.linestyle == PS_NULL) return false;
    const bool duplicateClosingPoint = closed && count > 2 &&
        std::abs(points[0].x - points[count - 1].x) <= 1e-5f &&
        std::abs(points[0].y - points[count - 1].y) <= 1e-5f;
    const int effectiveCount = duplicateClosingPoint
        ? count - 1 : count;
    if (effectiveCount < 2) return false;
    const float lineWidth = std::max(0.01f, target->m_linewidth);
    const float halfWidth = lineWidth * 0.5f;
    const int edgeCount = closed ? effectiveCount : effectiveCount - 1;
    if (edgeCount <= 0) return false;
    const int lineStyle = target->m_linestyle.linestyle;
    const line_cap_type startCap = target->m_linestartcap;
    const line_cap_type endCap = target->m_lineendcap;
    const line_join_type lineJoin = target->m_linejoin;
    const float miterLimit = target->m_linejoinmiterlimit;

    ege_transform_matrix transform;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        transform = get_transform_locked(target);
    }
    const float determinant = transform.m11 * transform.m22 - transform.m21 * transform.m12;
    if (std::abs(determinant) < 1e-12f) return true;

    float sourceLeft = points[0].x;
    float sourceTop = points[0].y;
    float sourceRight = points[0].x;
    float sourceBottom = points[0].y;
    for (int index = 1; index < effectiveCount; ++index) {
        sourceLeft = std::min(sourceLeft, points[index].x);
        sourceTop = std::min(sourceTop, points[index].y);
        sourceRight = std::max(sourceRight, points[index].x);
        sourceBottom = std::max(sourceBottom, points[index].y);
    }
    float strokeExpansion = halfWidth * 1.415f;
    if (lineJoin == LINEJOIN_MITER && effectiveCount > 2) {
        strokeExpansion = std::max(strokeExpansion, halfWidth * std::max(1.0f, miterLimit));
    }
    sourceLeft -= strokeExpansion;
    sourceTop -= strokeExpansion;
    sourceRight += strokeExpansion;
    sourceBottom += strokeExpansion;
    const ege_point sourceCorners[4] = {
        {sourceLeft, sourceTop}, {sourceRight, sourceTop},
        {sourceRight, sourceBottom}, {sourceLeft, sourceBottom}};
    float left = std::numeric_limits<float>::max();
    float top = std::numeric_limits<float>::max();
    float right = std::numeric_limits<float>::lowest();
    float bottom = std::numeric_limits<float>::lowest();
    for (int index = 0; index < 4; ++index) {
        const float transformedX = sourceCorners[index].x * transform.m11 +
            sourceCorners[index].y * transform.m21 + transform.m31;
        const float transformedY = sourceCorners[index].x * transform.m12 +
            sourceCorners[index].y * transform.m22 + transform.m32;
        left = std::min(left, transformedX);
        top = std::min(top, transformedY);
        right = std::max(right, transformedX);
        bottom = std::max(bottom, transformedY);
    }

    std::vector<float> cumulative(static_cast<std::size_t>(edgeCount) + 1, 0.0f);
    for (int edge = 0; edge < edgeCount; ++edge) {
        const ege_point& first = points[edge];
        const ege_point& second = points[(edge + 1) % effectiveCount];
        const float dx = second.x - first.x;
        const float dy = second.y - first.y;
        cumulative[static_cast<std::size_t>(edge) + 1] = cumulative[edge] +
            std::sqrt(dx * dx + dy * dy);
    }

    color_t* pixels = target->getbuffer(IMAGE_BUFFER_READ_WRITE);
    if (pixels == NULL) return true;
    const int clipLeft = target->m_enableclip ? std::max(0, target->m_vpt.left) : 0;
    const int clipTop = target->m_enableclip ? std::max(0, target->m_vpt.top) : 0;
    const int clipRight = target->m_enableclip
        ? std::min(target->m_width, target->m_vpt.right) : target->m_width;
    const int clipBottom = target->m_enableclip
        ? std::min(target->m_height, target->m_vpt.bottom) : target->m_height;
    const auto coversStroke = [&](float sourceX, float sourceY) {
        for (int edge = 0; edge < edgeCount; ++edge) {
            const ege_point& first = points[edge];
            const ege_point& second = points[(edge + 1) % effectiveCount];
            const float dx = second.x - first.x;
            const float dy = second.y - first.y;
            const float length = cumulative[static_cast<std::size_t>(edge) + 1] - cumulative[edge];
            if (length <= 1e-6f) continue;
            const float directionX = dx / length;
            const float directionY = dy / length;
            const float offsetX = sourceX - first.x;
            const float offsetY = sourceY - first.y;
            const float along = offsetX * directionX + offsetY * directionY;
            const float perpendicular = std::abs(offsetX * -directionY + offsetY * directionX);
            if (along >= -1e-5f && along <= length + 1e-5f &&
                perpendicular <= halfWidth + 1e-5f &&
                stroke_dash_visible(lineStyle, cumulative[edge] +
                    std::max(0.0f, std::min(length, along)), lineWidth)) {
                return true;
            }
        }

        if (!closed) {
            const ege_point& first = points[0];
            const ege_point& second = points[1];
            const float firstDeltaX = second.x - first.x;
            const float firstDeltaY = second.y - first.y;
            const float firstLength = std::sqrt(
                firstDeltaX * firstDeltaX + firstDeltaY * firstDeltaY);
            if (firstLength > 1e-6f && stroke_dash_visible(lineStyle, 0.0f, lineWidth) &&
                stroke_cap_covers(sourceX, sourceY, first,
                    (second.x - first.x) / firstLength, (second.y - first.y) / firstLength,
                    halfWidth, startCap, true)) {
                return true;
            }
            const ege_point& beforeLast = points[effectiveCount - 2];
            const ege_point& last = points[effectiveCount - 1];
            const float lastDeltaX = last.x - beforeLast.x;
            const float lastDeltaY = last.y - beforeLast.y;
            const float lastLength = std::sqrt(
                lastDeltaX * lastDeltaX + lastDeltaY * lastDeltaY);
            const float totalLength = cumulative.back();
            const float endProbe = std::max(0.0f, totalLength - std::min(0.01f, lastLength * 0.25f));
            if (lastLength > 1e-6f && stroke_dash_visible(lineStyle, endProbe, lineWidth) &&
                stroke_cap_covers(sourceX, sourceY, last,
                    (last.x - beforeLast.x) / lastLength, (last.y - beforeLast.y) / lastLength,
                    halfWidth, endCap, false)) {
                return true;
            }
        }

        const int firstVertex = closed ? 0 : 1;
        const int vertexEnd = closed ? effectiveCount : effectiveCount - 1;
        for (int vertexIndex = firstVertex; vertexIndex < vertexEnd; ++vertexIndex) {
            const int previousEdge = (vertexIndex + edgeCount - 1) % edgeCount;
            const int nextEdge = vertexIndex % edgeCount;
            const ege_point& previousPoint = points[previousEdge];
            const ege_point& vertex = points[vertexIndex % effectiveCount];
            const ege_point& nextPoint = points[(vertexIndex + 1) % effectiveCount];
            const float previousDeltaX = vertex.x - previousPoint.x;
            const float previousDeltaY = vertex.y - previousPoint.y;
            const float nextDeltaX = nextPoint.x - vertex.x;
            const float nextDeltaY = nextPoint.y - vertex.y;
            const float previousLength = std::sqrt(
                previousDeltaX * previousDeltaX + previousDeltaY * previousDeltaY);
            const float nextLength = std::sqrt(nextDeltaX * nextDeltaX + nextDeltaY * nextDeltaY);
            if (previousLength <= 1e-6f || nextLength <= 1e-6f) continue;
            const float joinDistance = cumulative[nextEdge];
            const float probe = std::min(0.01f, std::min(previousLength, nextLength) * 0.25f);
            float beforeDistance = joinDistance - probe;
            if (closed && vertexIndex == 0) beforeDistance = cumulative.back() - probe;
            if (!stroke_dash_visible(lineStyle, std::max(0.0f, beforeDistance), lineWidth) ||
                !stroke_dash_visible(lineStyle, joinDistance + probe, lineWidth)) {
                continue;
            }
            if (stroke_join_covers(sourceX, sourceY, vertex,
                    (vertex.x - previousPoint.x) / previousLength,
                    (vertex.y - previousPoint.y) / previousLength,
                    (nextPoint.x - vertex.x) / nextLength,
                    (nextPoint.y - vertex.y) / nextLength,
                    halfWidth, lineJoin, miterLimit)) {
                return true;
            }
        }
        return false;
    };
    const int sampleGrid = target->isAntiAliasingEnabled() ? 4 : 1;
    const int sampleCount = sampleGrid * sampleGrid;
    for (int logicalY = static_cast<int>(std::floor(top));
         logicalY < static_cast<int>(std::ceil(bottom)); ++logicalY) {
        const int physicalY = logicalY + target->m_vpt.top;
        if (physicalY < clipTop || physicalY >= clipBottom) continue;
        for (int logicalX = static_cast<int>(std::floor(left));
             logicalX < static_cast<int>(std::ceil(right)); ++logicalX) {
            const int physicalX = logicalX + target->m_vpt.left;
            if (physicalX < clipLeft || physicalX >= clipRight) continue;
            int covered = 0;
            for (int sampleY = 0; sampleY < sampleGrid; ++sampleY) {
                for (int sampleX = 0; sampleX < sampleGrid; ++sampleX) {
                    const float offsetX = (sampleX + 0.5f) / sampleGrid;
                    const float offsetY = (sampleY + 0.5f) / sampleGrid;
                    const float transformedX = logicalX + offsetX - transform.m31;
                    const float transformedY = logicalY + offsetY - transform.m32;
                    const float sourceX = (transformedX * transform.m22 -
                        transformedY * transform.m21) / determinant;
                    const float sourceY = (transformedY * transform.m11 -
                        transformedX * transform.m12) / determinant;
                    if (coversStroke(sourceX, sourceY)) ++covered;
                }
            }
            if (covered == 0) continue;
            const float coverage = static_cast<float>(covered) / sampleCount;
            color_t& destination = pixels[physicalY * target->m_width + physicalX];
            destination = source_over(
                destination, scale_straight_alpha(target->m_linecolor, coverage));
        }
    }
    return true;
}

static inline int iround(float v)
{
    return static_cast<int>(std::lround(static_cast<double>(v)));
}

constexpr unsigned char kPathTypeStart = 0x00;
constexpr unsigned char kPathTypeLine = 0x01;
constexpr unsigned char kPathTypeBezier = 0x03;
constexpr unsigned char kPathTypeMask = 0x07;
constexpr unsigned char kPathTypeClose = 0x80;
constexpr float kPi = 3.14159265358979323846f;

struct PathData {
    std::vector<ege_point> points;
    std::vector<unsigned char> types;
    fill_mode fillMode = FILLMODE_ALTERNATE;
    bool startNewFigure = true;
};

struct FlatFigure {
    std::vector<ege_point> points;
    bool closed = false;
};

struct PathSegmentData {
    ege_point start{};
    ege_point control1{};
    ege_point control2{};
    ege_point end{};
    bool cubic = false;
};

struct PathFigureData {
    ege_point start;
    std::vector<PathSegmentData> segments;
    bool closed = false;
};

static PathData* path_data(ege_path* path)
{
    return path ? static_cast<PathData*>(path->data()) : NULL;
}

static const PathData* path_data(const ege_path* path)
{
    return path ? static_cast<const PathData*>(path->data()) : NULL;
}

static bool same_point(const ege_point& first, const ege_point& second)
{
    return std::abs(first.x - second.x) <= 1e-5f &&
           std::abs(first.y - second.y) <= 1e-5f;
}

static ege_point transform_point(const ege_point& point, const ege_transform_matrix& matrix)
{
    ege_point transformed;
    transformed.x = point.x * matrix.m11 + point.y * matrix.m21 + matrix.m31;
    transformed.y = point.x * matrix.m12 + point.y * matrix.m22 + matrix.m32;
    return transformed;
}

static bool inverse_transform_point(const ege_point& point,
                                    const ege_transform_matrix& matrix,
                                    ege_point& output)
{
    const float determinant = matrix.m11 * matrix.m22 - matrix.m21 * matrix.m12;
    if (std::abs(determinant) <= 1e-12f) return false;
    const float x = point.x - matrix.m31;
    const float y = point.y - matrix.m32;
    output.x = (x * matrix.m22 - y * matrix.m21) / determinant;
    output.y = (y * matrix.m11 - x * matrix.m12) / determinant;
    return true;
}

static bool perspective_warp_point(float u, float v, const ege_point points[4],
                                   ege_point& output)
{
    // GDI+ orders the destination corners TL, TR, BL, BR. Solve the unit
    // square-to-quadrilateral homography; a parallelogram naturally reduces
    // to the affine case when the projective coefficients are zero.
    const float dx1 = points[1].x - points[3].x;
    const float dx2 = points[2].x - points[3].x;
    const float dx3 = points[0].x - points[1].x - points[2].x + points[3].x;
    const float dy1 = points[1].y - points[3].y;
    const float dy2 = points[2].y - points[3].y;
    const float dy3 = points[0].y - points[1].y - points[2].y + points[3].y;

    float projectiveX = 0.0f;
    float projectiveY = 0.0f;
    if (std::abs(dx3) > 1e-12f || std::abs(dy3) > 1e-12f) {
        const float determinant = dx1 * dy2 - dx2 * dy1;
        if (std::abs(determinant) <= 1e-12f) return false;
        projectiveX = (dx3 * dy2 - dx2 * dy3) / determinant;
        projectiveY = (dx1 * dy3 - dx3 * dy1) / determinant;
    }

    const float denominator = 1.0f + projectiveX * u + projectiveY * v;
    if (std::abs(denominator) <= 1e-12f) return false;
    const float coefficientXU = points[1].x - points[0].x + projectiveX * points[1].x;
    const float coefficientXV = points[2].x - points[0].x + projectiveY * points[2].x;
    const float coefficientYU = points[1].y - points[0].y + projectiveX * points[1].y;
    const float coefficientYV = points[2].y - points[0].y + projectiveY * points[2].y;
    output.x = (points[0].x + coefficientXU * u + coefficientXV * v) / denominator;
    output.y = (points[0].y + coefficientYU * u + coefficientYV * v) / denominator;
    return std::isfinite(output.x) && std::isfinite(output.y);
}

static float point_distance(const ege_point& first, const ege_point& second)
{
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    return std::sqrt(dx * dx + dy * dy);
}

static ege_point cubic_point(const ege_point& start, const ege_point& control1,
                             const ege_point& control2, const ege_point& end,
                             float amount)
{
    const float inverse = 1.0f - amount;
    const float inverse2 = inverse * inverse;
    const float amount2 = amount * amount;
    ege_point point;
    point.x = inverse2 * inverse * start.x + 3.0f * inverse2 * amount * control1.x +
              3.0f * inverse * amount2 * control2.x + amount2 * amount * end.x;
    point.y = inverse2 * inverse * start.y + 3.0f * inverse2 * amount * control1.y +
              3.0f * inverse * amount2 * control2.y + amount2 * amount * end.y;
    return point;
}

static void append_cubic_samples(std::vector<ege_point>& output,
                                 ege_point start, ege_point control1,
                                 ege_point control2, ege_point end,
                                 float flatness)
{
    const float estimatedLength = point_distance(start, control1) +
        point_distance(control1, control2) + point_distance(control2, end);
    const float stepLength = std::max(0.5f, flatness * 4.0f);
    const int segments = std::max(4, std::min(256,
        static_cast<int>(std::ceil(estimatedLength / stepLength))));
    for (int segment = 1; segment <= segments; ++segment) {
        output.push_back(cubic_point(start, control1, control2, end,
            static_cast<float>(segment) / segments));
    }
}

static std::vector<ege_point> sample_arc(float x, float y, float width, float height,
                                         float startAngle, float sweepAngle)
{
    std::vector<ege_point> points;
    if (width <= 0.0f || height <= 0.0f || std::abs(sweepAngle) <= 1e-6f) {
        return points;
    }
    // GDI+ AddArc/DrawArc clamp sweeps to one revolution rather than tracing
    // repeated coincident loops for values outside [-360, 360].
    sweepAngle = std::max(-360.0f, std::min(360.0f, sweepAngle));
    const int segments = std::max(1, std::min(720,
        static_cast<int>(std::ceil(std::abs(sweepAngle) / 4.0f))));
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    const float radiusX = width * 0.5f;
    const float radiusY = height * 0.5f;
    points.reserve(static_cast<std::size_t>(segments) + 1);
    for (int segment = 0; segment <= segments; ++segment) {
        const float angle = (startAngle + sweepAngle *
            static_cast<float>(segment) / segments) * kPi / 180.0f;
        points.push_back({centerX + std::cos(angle) * radiusX,
                          centerY + std::sin(angle) * radiusY});
    }
    return points;
}

static std::vector<ege_point> sample_cardinal_curve(int count, const ege_point* points,
                                                     float tension, bool closed)
{
    std::vector<ege_point> output;
    if (points == NULL || count < 2) return output;
    if (tension <= 0.0f) {
        output.assign(points, points + count);
        if (closed) output.push_back(points[0]);
        return output;
    }

    const int curveCount = closed ? count : count - 1;
    output.reserve(static_cast<std::size_t>(curveCount) * 12 + 1);
    output.push_back(points[0]);
    for (int curve = 0; curve < curveCount; ++curve) {
        const int first = curve;
        const int second = (curve + 1) % count;
        const int before = closed ? (curve + count - 1) % count : std::max(0, curve - 1);
        const int after = closed ? (curve + 2) % count : std::min(count - 1, curve + 2);
        const ege_point& p0 = points[before];
        const ege_point& p1 = points[first];
        const ege_point& p2 = points[second];
        const ege_point& p3 = points[after];
        const float estimatedLength = point_distance(p1, p2);
        const int segments = std::max(6, std::min(64,
            static_cast<int>(std::ceil(estimatedLength / 3.0f))));
        for (int segment = 1; segment <= segments; ++segment) {
            const float t = static_cast<float>(segment) / segments;
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            const float h10 = t3 - 2.0f * t2 + t;
            const float h01 = -2.0f * t3 + 3.0f * t2;
            const float h11 = t3 - t2;
            const float tangent1X = tension * (p2.x - p0.x);
            const float tangent1Y = tension * (p2.y - p0.y);
            const float tangent2X = tension * (p3.x - p1.x);
            const float tangent2Y = tension * (p3.y - p1.y);
            output.push_back({h00 * p1.x + h10 * tangent1X + h01 * p2.x + h11 * tangent2X,
                              h00 * p1.y + h10 * tangent1Y + h01 * p2.y + h11 * tangent2Y});
        }
    }
    return output;
}

static void path_begin_figure(PathData& data, const ege_point& point)
{
    data.points.push_back(point);
    data.types.push_back(kPathTypeStart);
    data.startNewFigure = false;
}

static void path_line_to(PathData& data, const ege_point& point)
{
    if (data.points.empty() || data.startNewFigure) {
        path_begin_figure(data, point);
        return;
    }
    data.points.push_back(point);
    data.types.push_back(kPathTypeLine);
}

static void path_close_figure(PathData& data)
{
    if (!data.types.empty() && !data.startNewFigure) {
        data.types.back() = static_cast<unsigned char>(data.types.back() | kPathTypeClose);
        data.startNewFigure = true;
    }
}

static void path_add_polyline(PathData& data, int count, const ege_point* points,
                              bool closed, bool forceNewFigure)
{
    if (points == NULL || count <= 0) return;
    if (forceNewFigure) data.startNewFigure = true;
    if (data.points.empty() || data.startNewFigure) {
        path_begin_figure(data, points[0]);
    } else if (!same_point(data.points.back(), points[0])) {
        path_line_to(data, points[0]);
    }
    for (int i = 1; i < count; ++i) path_line_to(data, points[i]);
    if (closed) path_close_figure(data);
}

static void path_add_beziers(PathData& data, int count, const ege_point* points)
{
    if (points == NULL || count < 4 || ((count - 1) % 3) != 0) return;
    if (data.points.empty() || data.startNewFigure) {
        path_begin_figure(data, points[0]);
    } else if (!same_point(data.points.back(), points[0])) {
        path_line_to(data, points[0]);
    }
    for (int i = 1; i < count; ++i) {
        data.points.push_back(points[i]);
        data.types.push_back(kPathTypeBezier);
    }
    data.startNewFigure = false;
}

static std::vector<FlatFigure> flatten_figures(const PathData& data, float flatness = 0.25f)
{
    std::vector<FlatFigure> figures;
    FlatFigure current;
    const std::size_t count = std::min(data.points.size(), data.types.size());
    for (std::size_t index = 0; index < count; ++index) {
        const unsigned char type = data.types[index];
        const unsigned char baseType = static_cast<unsigned char>(type & kPathTypeMask);
        if (baseType == kPathTypeStart) {
            if (!current.points.empty()) figures.push_back(current);
            current = FlatFigure();
            current.points.push_back(data.points[index]);
        } else if (baseType == kPathTypeBezier && !current.points.empty() && index + 2 < count &&
                   (data.types[index + 1] & kPathTypeMask) == kPathTypeBezier &&
                   (data.types[index + 2] & kPathTypeMask) == kPathTypeBezier) {
            append_cubic_samples(current.points, current.points.back(), data.points[index],
                                 data.points[index + 1], data.points[index + 2], flatness);
            const bool closes = (data.types[index + 2] & kPathTypeClose) != 0;
            index += 2;
            if (closes) {
                current.closed = true;
                figures.push_back(current);
                current = FlatFigure();
            }
            continue;
        } else {
            if (current.points.empty()) current.points.push_back(data.points[index]);
            else current.points.push_back(data.points[index]);
        }
        if ((type & kPathTypeClose) != 0) {
            current.closed = true;
            figures.push_back(current);
            current = FlatFigure();
        }
    }
    if (!current.points.empty()) figures.push_back(current);
    return figures;
}

static void assign_flattened(PathData& data, const std::vector<FlatFigure>& figures)
{
    data.points.clear();
    data.types.clear();
    for (std::size_t figureIndex = 0; figureIndex < figures.size(); ++figureIndex) {
        const FlatFigure& figure = figures[figureIndex];
        if (figure.points.empty()) continue;
        for (std::size_t pointIndex = 0; pointIndex < figure.points.size(); ++pointIndex) {
            data.points.push_back(figure.points[pointIndex]);
            data.types.push_back(pointIndex == 0 ? kPathTypeStart : kPathTypeLine);
        }
        if (figure.closed) data.types.back() = static_cast<unsigned char>(data.types.back() | kPathTypeClose);
    }
    data.startNewFigure = data.points.empty() ||
        (!data.types.empty() && (data.types.back() & kPathTypeClose) != 0);
}

static std::vector<PathFigureData> decode_path_figures(const PathData& data)
{
    std::vector<PathFigureData> figures;
    PathFigureData currentFigure;
    ege_point currentPoint = {0.0f, 0.0f};
    bool hasFigure = false;
    const std::size_t count = std::min(data.points.size(), data.types.size());
    for (std::size_t index = 0; index < count; ++index) {
        const unsigned char type = data.types[index];
        const unsigned char baseType = static_cast<unsigned char>(type & kPathTypeMask);
        if (baseType == kPathTypeStart) {
            if (hasFigure) figures.push_back(currentFigure);
            currentFigure = PathFigureData();
            currentFigure.start = data.points[index];
            currentPoint = data.points[index];
            hasFigure = true;
        } else if (hasFigure && baseType == kPathTypeBezier && index + 2 < count &&
                   (data.types[index + 1] & kPathTypeMask) == kPathTypeBezier &&
                   (data.types[index + 2] & kPathTypeMask) == kPathTypeBezier) {
            PathSegmentData segment;
            segment.start = currentPoint;
            segment.control1 = data.points[index];
            segment.control2 = data.points[index + 1];
            segment.end = data.points[index + 2];
            segment.cubic = true;
            currentFigure.segments.push_back(segment);
            currentPoint = segment.end;
            const bool closes = (data.types[index + 2] & kPathTypeClose) != 0;
            index += 2;
            if (closes) {
                currentFigure.closed = true;
                figures.push_back(currentFigure);
                currentFigure = PathFigureData();
                hasFigure = false;
            }
        } else if (hasFigure) {
            PathSegmentData segment;
            segment.start = currentPoint;
            segment.end = data.points[index];
            currentFigure.segments.push_back(segment);
            currentPoint = segment.end;
            if ((type & kPathTypeClose) != 0) {
                currentFigure.closed = true;
                figures.push_back(currentFigure);
                currentFigure = PathFigureData();
                hasFigure = false;
            }
        }
    }
    if (hasFigure) figures.push_back(currentFigure);
    return figures;
}

static void assign_reversed(PathData& data, const std::vector<PathFigureData>& figures)
{
    data.points.clear();
    data.types.clear();
    for (std::vector<PathFigureData>::const_reverse_iterator figureIt = figures.rbegin();
         figureIt != figures.rend(); ++figureIt) {
        const PathFigureData& figure = *figureIt;
        const ege_point reversedStart = figure.segments.empty()
            ? figure.start : figure.segments.back().end;
        data.points.push_back(reversedStart);
        data.types.push_back(kPathTypeStart);
        for (std::vector<PathSegmentData>::const_reverse_iterator segmentIt =
                 figure.segments.rbegin(); segmentIt != figure.segments.rend(); ++segmentIt) {
            if (segmentIt->cubic) {
                data.points.push_back(segmentIt->control2);
                data.points.push_back(segmentIt->control1);
                data.points.push_back(segmentIt->start);
                data.types.push_back(kPathTypeBezier);
                data.types.push_back(kPathTypeBezier);
                data.types.push_back(kPathTypeBezier);
            } else {
                data.points.push_back(segmentIt->start);
                data.types.push_back(kPathTypeLine);
            }
        }
        if (figure.closed && !data.types.empty()) {
            data.types.back() = static_cast<unsigned char>(data.types.back() | kPathTypeClose);
        }
    }
    data.startNewFigure = data.points.empty() ||
        (!data.types.empty() && (data.types.back() & kPathTypeClose) != 0);
}

static float edge_cross(const ege_point& first, const ege_point& second, float x, float y)
{
    return (second.x - first.x) * (y - first.y) -
           (second.y - first.y) * (x - first.x);
}

static bool point_in_figures(const std::vector<FlatFigure>& figures, fill_mode mode,
                             float x, float y)
{
    bool alternateInside = false;
    int winding = 0;
    for (std::size_t figureIndex = 0; figureIndex < figures.size(); ++figureIndex) {
        const std::vector<ege_point>& points = figures[figureIndex].points;
        if (points.size() < 3) continue;
        for (std::size_t index = 0, previous = points.size() - 1;
             index < points.size(); previous = index++) {
            const ege_point& first = points[previous];
            const ege_point& second = points[index];
            const bool crosses = ((first.y > y) != (second.y > y)) &&
                (x < (second.x - first.x) * (y - first.y) /
                     (second.y - first.y) + first.x);
            if (crosses) alternateInside = !alternateInside;
            if (first.y <= y) {
                if (second.y > y && edge_cross(first, second, x, y) > 0.0f) ++winding;
            } else if (second.y <= y && edge_cross(first, second, x, y) < 0.0f) {
                --winding;
            }
        }
    }
    return mode == FILLMODE_WINDING ? winding != 0 : alternateInside;
}

static ege_rect figure_bounds(const std::vector<FlatFigure>& figures)
{
    ege_rect bounds = {0.0f, 0.0f, 0.0f, 0.0f};
    bool initialized = false;
    float right = 0.0f;
    float bottom = 0.0f;
    for (std::size_t figureIndex = 0; figureIndex < figures.size(); ++figureIndex) {
        for (std::size_t pointIndex = 0; pointIndex < figures[figureIndex].points.size(); ++pointIndex) {
            const ege_point& point = figures[figureIndex].points[pointIndex];
            if (!initialized) {
                bounds.x = right = point.x;
                bounds.y = bottom = point.y;
                initialized = true;
            } else {
                bounds.x = std::min(bounds.x, point.x);
                bounds.y = std::min(bounds.y, point.y);
                right = std::max(right, point.x);
                bottom = std::max(bottom, point.y);
            }
        }
    }
    if (initialized) {
        bounds.w = right - bounds.x;
        bounds.h = bottom - bounds.y;
    }
    return bounds;
}

static float distance_to_segment(float x, float y, const ege_point& start, const ege_point& end)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float lengthSquared = dx * dx + dy * dy;
    float amount = lengthSquared > 1e-12f
        ? ((x - start.x) * dx + (y - start.y) * dy) / lengthSquared : 0.0f;
    amount = std::max(0.0f, std::min(1.0f, amount));
    const float closestX = start.x + amount * dx;
    const float closestY = start.y + amount * dy;
    const float offsetX = x - closestX;
    const float offsetY = y - closestY;
    return std::sqrt(offsetX * offsetX + offsetY * offsetY);
}

static void offset_figures(std::vector<FlatFigure>& figures, float x, float y)
{
    for (std::size_t figureIndex = 0; figureIndex < figures.size(); ++figureIndex) {
        for (std::size_t pointIndex = 0; pointIndex < figures[figureIndex].points.size(); ++pointIndex) {
            figures[figureIndex].points[pointIndex].x += x;
            figures[figureIndex].points[pointIndex].y += y;
        }
    }
}

static void transform_figures(std::vector<FlatFigure>& figures, const ege_transform_matrix* matrix)
{
    if (matrix == NULL) return;
    for (std::size_t figureIndex = 0; figureIndex < figures.size(); ++figureIndex) {
        for (std::size_t pointIndex = 0; pointIndex < figures[figureIndex].points.size(); ++pointIndex) {
            figures[figureIndex].points[pointIndex] =
                transform_point(figures[figureIndex].points[pointIndex], *matrix);
        }
    }
}

#if defined(__APPLE__)
static CGMutablePathRef create_cg_path(const PathData& data)
{
    CGMutablePathRef path = CGPathCreateMutable();
    if (path == NULL) return NULL;
    const std::size_t count = std::min(data.points.size(), data.types.size());
    for (std::size_t index = 0; index < count; ++index) {
        const unsigned char type = data.types[index];
        const unsigned char baseType = static_cast<unsigned char>(type & kPathTypeMask);
        if (baseType == kPathTypeStart) {
            CGPathMoveToPoint(path, NULL, data.points[index].x, data.points[index].y);
        } else if (baseType == kPathTypeBezier && index + 2 < count &&
                   (data.types[index + 1] & kPathTypeMask) == kPathTypeBezier &&
                   (data.types[index + 2] & kPathTypeMask) == kPathTypeBezier) {
            CGPathAddCurveToPoint(path, NULL,
                data.points[index].x, data.points[index].y,
                data.points[index + 1].x, data.points[index + 1].y,
                data.points[index + 2].x, data.points[index + 2].y);
            const bool closes = (data.types[index + 2] & kPathTypeClose) != 0;
            index += 2;
            if (closes) CGPathCloseSubpath(path);
            continue;
        } else {
            CGPathAddLineToPoint(path, NULL, data.points[index].x, data.points[index].y);
        }
        if ((type & kPathTypeClose) != 0) CGPathCloseSubpath(path);
    }
    return path;
}

static CGAffineTransform cg_affine_transform(const ege_transform_matrix& matrix)
{
    return CGAffineTransformMake(matrix.m11, matrix.m12, matrix.m21, matrix.m22,
        matrix.m31, matrix.m32);
}

static CGLineCap cg_line_cap(line_cap_type cap)
{
    switch (cap) {
    case LINECAP_SQUARE: return kCGLineCapSquare;
    case LINECAP_ROUND: return kCGLineCapRound;
    default: return kCGLineCapButt;
    }
}

static CGLineJoin cg_line_join(line_join_type join)
{
    switch (join) {
    case LINEJOIN_BEVEL: return kCGLineJoinBevel;
    case LINEJOIN_ROUND: return kCGLineJoinRound;
    default: return kCGLineJoinMiter;
    }
}

static CGPathRef create_dashed_cg_path(CGPathRef source, int lineStyle, float lineWidth)
{
    const CGFloat unit = std::max(1.0f, lineWidth);
    CGFloat lengths[6] = {};
    std::size_t count = 0;
    switch (lineStyle) {
    case PS_DASH:
        lengths[0] = 3.0 * unit; lengths[1] = unit; count = 2; break;
    case PS_DOT:
        lengths[0] = unit; lengths[1] = unit; count = 2; break;
    case PS_DASHDOT:
        lengths[0] = 3.0 * unit; lengths[1] = unit;
        lengths[2] = unit; lengths[3] = unit; count = 4; break;
    case PS_DASHDOTDOT:
        lengths[0] = 3.0 * unit; lengths[1] = unit;
        lengths[2] = unit; lengths[3] = unit;
        lengths[4] = unit; lengths[5] = unit; count = 6; break;
    default:
        return CGPathCreateCopy(source);
    }
    return CGPathCreateCopyByDashingPath(source, NULL, 0.0, lengths, count);
}

static CGPathRef create_pen_outline_cg_path(const PathData& data, const IMAGE& image)
{
    CGMutablePathRef source = create_cg_path(data);
    if (source == NULL) return NULL;
    CGPathRef dashed = create_dashed_cg_path(
        source, image.m_linestyle.linestyle, std::max(0.01f, image.m_linewidth));
    CGPathRelease(source);
    if (dashed == NULL) return NULL;
    CGPathRef outline = CGPathCreateCopyByStrokingPath(dashed, NULL,
        std::max(0.01f, image.m_linewidth), cg_line_cap(image.m_linestartcap),
        cg_line_join(image.m_linejoin), std::max(1.0f, image.m_linejoinmiterlimit));
    CGPathRelease(dashed);
    return outline;
}

struct CGPathImportContext {
    PathData* data;
    ege_point current;
    ege_point figureStart;
    bool hasCurrent;
};

static void import_cg_path_element(void* rawContext, const CGPathElement* element)
{
    CGPathImportContext* context = static_cast<CGPathImportContext*>(rawContext);
    PathData& data = *context->data;
    switch (element->type) {
    case kCGPathElementMoveToPoint: {
        const ege_point point = {
            static_cast<float>(element->points[0].x),
            static_cast<float>(element->points[0].y)};
        data.startNewFigure = true;
        path_begin_figure(data, point);
        context->current = context->figureStart = point;
        context->hasCurrent = true;
        break;
    }
    case kCGPathElementAddLineToPoint: {
        const ege_point point = {
            static_cast<float>(element->points[0].x),
            static_cast<float>(element->points[0].y)};
        path_line_to(data, point);
        context->current = point;
        context->hasCurrent = true;
        break;
    }
    case kCGPathElementAddQuadCurveToPoint: {
        if (!context->hasCurrent) break;
        const ege_point control = {
            static_cast<float>(element->points[0].x),
            static_cast<float>(element->points[0].y)};
        const ege_point end = {
            static_cast<float>(element->points[1].x),
            static_cast<float>(element->points[1].y)};
        const ege_point control1 = {
            context->current.x + (control.x - context->current.x) * (2.0f / 3.0f),
            context->current.y + (control.y - context->current.y) * (2.0f / 3.0f)};
        const ege_point control2 = {
            end.x + (control.x - end.x) * (2.0f / 3.0f),
            end.y + (control.y - end.y) * (2.0f / 3.0f)};
        data.points.push_back(control1);
        data.points.push_back(control2);
        data.points.push_back(end);
        data.types.push_back(kPathTypeBezier);
        data.types.push_back(kPathTypeBezier);
        data.types.push_back(kPathTypeBezier);
        data.startNewFigure = false;
        context->current = end;
        break;
    }
    case kCGPathElementAddCurveToPoint: {
        const ege_point control1 = {
            static_cast<float>(element->points[0].x),
            static_cast<float>(element->points[0].y)};
        const ege_point control2 = {
            static_cast<float>(element->points[1].x),
            static_cast<float>(element->points[1].y)};
        const ege_point end = {
            static_cast<float>(element->points[2].x),
            static_cast<float>(element->points[2].y)};
        data.points.push_back(control1);
        data.points.push_back(control2);
        data.points.push_back(end);
        data.types.push_back(kPathTypeBezier);
        data.types.push_back(kPathTypeBezier);
        data.types.push_back(kPathTypeBezier);
        data.startNewFigure = false;
        context->current = end;
        context->hasCurrent = true;
        break;
    }
    case kCGPathElementCloseSubpath:
        path_close_figure(data);
        context->current = context->figureStart;
        context->hasCurrent = true;
        break;
    }
}

static void assign_cg_path(PathData& data, CGPathRef path)
{
    data.points.clear();
    data.types.clear();
    data.startNewFigure = true;
    if (path == NULL) return;
    CGPathImportContext context = {&data, {0.0f, 0.0f}, {0.0f, 0.0f}, false};
    CGPathApply(path, &context, import_cg_path_element);
}

static CFStringRef create_cf_string_utf8(const std::string& utf8)
{
    return CFStringCreateWithBytes(kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(utf8.data()),
        static_cast<CFIndex>(utf8.size()), kCFStringEncodingUTF8, false);
}

static CTFontRef create_text_path_font(const std::string& typeface, float height, int fontStyle)
{
    CTFontRef font = NULL;
    if (!typeface.empty()) {
        CFStringRef name = create_cf_string_utf8(typeface);
        if (name != NULL) {
            font = CTFontCreateWithName(name, height, NULL);
            CFRelease(name);
        }
    }
    if (font == NULL) {
        font = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, height, NULL);
    }
    if (font == NULL) return NULL;

    CTFontSymbolicTraits desired = 0;
    CTFontSymbolicTraits mask = 0;
    if ((fontStyle & FONTSTYLE_BOLD) != 0) {
        desired |= kCTFontBoldTrait;
        mask |= kCTFontBoldTrait;
    }
    if ((fontStyle & FONTSTYLE_ITALIC) != 0) {
        desired |= kCTFontItalicTrait;
        mask |= kCTFontItalicTrait;
    }
    if (mask != 0) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(font, height, NULL, desired, mask);
        if (styled != NULL) {
            CFRelease(font);
            font = styled;
        }
    }
    return font;
}

static bool add_coretext_outlines(ege_path* path, float x, float y,
                                  const std::string& utf8, float height,
                                  const std::string& typeface, int fontStyle)
{
    PathData* data = path_data(path);
    if (data == NULL || utf8.empty() || height <= 0.0f) return false;
    CFStringRef string = create_cf_string_utf8(utf8);
    CTFontRef font = create_text_path_font(typeface, height, fontStyle);
    if (string == NULL || font == NULL) {
        if (string != NULL) CFRelease(string);
        if (font != NULL) CFRelease(font);
        return false;
    }

    const void* keys[] = {kCTFontAttributeName};
    const void* values[] = {font};
    CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef attributed = attributes != NULL
        ? CFAttributedStringCreate(kCFAllocatorDefault, string, attributes) : NULL;
    CTLineRef line = attributed != NULL ? CTLineCreateWithAttributedString(attributed) : NULL;
    bool appended = false;
    if (line != NULL) {
        const CGFloat baseline = static_cast<CGFloat>(y) + CTFontGetAscent(font);
        CFArrayRef runs = CTLineGetGlyphRuns(line);
        const CFIndex runCount = runs != NULL ? CFArrayGetCount(runs) : 0;
        for (CFIndex runIndex = 0; runIndex < runCount; ++runIndex) {
            CTRunRef run = static_cast<CTRunRef>(const_cast<void*>(CFArrayGetValueAtIndex(runs, runIndex)));
            const CFIndex glyphCount = CTRunGetGlyphCount(run);
            if (glyphCount <= 0) continue;
            std::vector<CGGlyph> glyphs(static_cast<std::size_t>(glyphCount));
            std::vector<CGPoint> positions(static_cast<std::size_t>(glyphCount));
            CTRunGetGlyphs(run, CFRangeMake(0, 0), glyphs.data());
            CTRunGetPositions(run, CFRangeMake(0, 0), positions.data());
            CFDictionaryRef runAttributes = CTRunGetAttributes(run);
            CTFontRef runFont = runAttributes != NULL
                ? static_cast<CTFontRef>(const_cast<void*>(
                    CFDictionaryGetValue(runAttributes, kCTFontAttributeName))) : NULL;
            if (runFont == NULL) runFont = font;
            for (CFIndex glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
                const CGPoint position = positions[static_cast<std::size_t>(glyphIndex)];
                CGAffineTransform transform = CGAffineTransformMake(
                    1.0, 0.0, 0.0, -1.0,
                    static_cast<CGFloat>(x) + position.x,
                    baseline - position.y);
                CGPathRef glyphPath = CTFontCreatePathForGlyph(
                    runFont, glyphs[static_cast<std::size_t>(glyphIndex)], &transform);
                if (glyphPath == NULL) continue;
                CGPathImportContext context = {data, {0.0f, 0.0f}, {0.0f, 0.0f}, false};
                CGPathApply(glyphPath, &context, import_cg_path_element);
                appended = true;
                CGPathRelease(glyphPath);
            }
        }

        const CGFloat typographicWidth = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
        if (typographicWidth > 0.0) {
            if ((fontStyle & FONTSTYLE_UNDERLINE) != 0) {
                const float thickness = std::max(1.0f,
                    static_cast<float>(CTFontGetUnderlineThickness(font)));
                const float underlineY = static_cast<float>(baseline - CTFontGetUnderlinePosition(font)) -
                    thickness * 0.5f;
                ege_path_addrect(path, x, underlineY, static_cast<float>(typographicWidth), thickness);
                appended = true;
            }
            if ((fontStyle & FONTSTYLE_STRIKEOUT) != 0) {
                const float thickness = std::max(1.0f, height * 0.05f);
                const float strikeY = static_cast<float>(baseline) -
                    static_cast<float>(CTFontGetAscent(font)) * 0.35f;
                ege_path_addrect(path, x, strikeY, static_cast<float>(typographicWidth), thickness);
                appended = true;
            }
        }
    }

    if (line != NULL) CFRelease(line);
    if (attributed != NULL) CFRelease(attributed);
    if (attributes != NULL) CFRelease(attributes);
    CFRelease(font);
    CFRelease(string);
    return appended;
}
#endif

struct BoundaryEdge {
    ege_point first;
    ege_point second;
};

struct CountedBoundaryEdge {
    ege_point first;
    ege_point second;
    int winding = 1;
    int occurrences = 1;
};

static std::vector<FlatFigure> external_boundary_figures(
    const std::vector<FlatFigure>& input, fill_mode mode)
{
    std::vector<CountedBoundaryEdge> countedEdges;
    for (std::size_t figureIndex = 0; figureIndex < input.size(); ++figureIndex) {
        const std::vector<ege_point>& points = input[figureIndex].points;
        if (points.size() < 2) continue;
        const std::size_t edgeCount =
            input[figureIndex].closed ? points.size() : points.size() - 1;
        for (std::size_t edgeIndex = 0; edgeIndex < edgeCount; ++edgeIndex) {
            const ege_point& first = points[edgeIndex];
            const ege_point& second = points[(edgeIndex + 1) % points.size()];
            if (same_point(first, second)) continue;
            bool counted = false;
            for (std::size_t existing = 0; existing < countedEdges.size(); ++existing) {
                if (same_point(countedEdges[existing].first, first) &&
                    same_point(countedEdges[existing].second, second)) {
                    ++countedEdges[existing].winding;
                    ++countedEdges[existing].occurrences;
                    counted = true;
                    break;
                }
                if (same_point(countedEdges[existing].first, second) &&
                    same_point(countedEdges[existing].second, first)) {
                    --countedEdges[existing].winding;
                    ++countedEdges[existing].occurrences;
                    counted = true;
                    break;
                }
            }
            if (!counted) countedEdges.push_back({first, second, 1, 1});
        }
    }

    std::vector<BoundaryEdge> edges;
    edges.reserve(countedEdges.size());
    for (std::size_t index = 0; index < countedEdges.size(); ++index) {
        const CountedBoundaryEdge& edge = countedEdges[index];
        if (mode == FILLMODE_WINDING) {
            if (edge.winding > 0) edges.push_back({edge.first, edge.second});
            else if (edge.winding < 0) edges.push_back({edge.second, edge.first});
        } else if ((edge.occurrences & 1) != 0) {
            edges.push_back({edge.first, edge.second});
        }
    }

    std::vector<FlatFigure> output;
    while (!edges.empty()) {
        FlatFigure figure;
        figure.points.push_back(edges.back().first);
        ege_point current = edges.back().second;
        figure.points.push_back(current);
        edges.pop_back();
        while (!same_point(current, figure.points.front())) {
            std::size_t next = edges.size();
            bool reverse = false;
            for (std::size_t index = 0; index < edges.size(); ++index) {
                if (same_point(edges[index].first, current)) {
                    next = index;
                    break;
                }
                if (same_point(edges[index].second, current)) {
                    next = index;
                    reverse = true;
                    break;
                }
            }
            if (next == edges.size()) break;
            current = reverse ? edges[next].first : edges[next].second;
            if (!same_point(current, figure.points.front())) figure.points.push_back(current);
            edges.erase(edges.begin() + static_cast<std::ptrdiff_t>(next));
        }
        figure.closed = figure.points.size() >= 3 && same_point(current, figure.points.front());
        if (figure.points.size() >= 2) output.push_back(figure);
    }
    return output;
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
    const ege_point firstPoint = points.front();
    points.push_back(firstPoint);
    return true;
}

} // anonymous namespace

void releaseNativeFallbackState(const IMAGE* image)
{
    release_fallback_state(image);
}

void clearNativeFallbackPattern(IMAGE* image)
{
    if (image == NULL) return;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map.erase(static_cast<const void*>(image));
}

bool updateNativeFallbackTexture(IMAGE* image, bool generate)
{
    if (image == NULL) return false;
    std::shared_ptr<const TextureSnapshot> snapshot;
    if (generate) {
        snapshot = make_texture_snapshot(image);
    }
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    const void* key = static_cast<const void*>(image);
    if (generate && snapshot) {
        g_texture_map[key] = snapshot;
        return true;
    }
    g_texture_map.erase(key);
    return !generate;
}

void ege_line(float x1, float y1, float x2, float y2, PIMAGE pimg)
{
    const ege_point points[2] = {{x1, y1}, {x2, y2}};
    stroke_cpu_polyline(pimg, points, 2, false);
}

void ege_drawpoly(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    if (numOfPoints <= 0 || points == NULL) {
        return;
    }

    if (numOfPoints > 3 && same_point(points[0], points[numOfPoints - 1])) {
        ege_polygon(numOfPoints - 1, points, pimg);
    } else {
        ege_polyline(numOfPoints, points, pimg);
    }
}

void ege_polyline(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    stroke_cpu_polyline(pimg, points, numOfPoints, false);
}

void ege_polygon(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    stroke_cpu_polyline(pimg, points, numOfPoints, true);
}

void ege_bezier(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    ege_drawbezier(numOfPoints, points, pimg);
}

void ege_drawbezier(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    if (points == NULL || numOfPoints < 4 || ((numOfPoints - 1) % 3) != 0) return;
    std::vector<ege_point> sampled;
    sampled.push_back(points[0]);
    for (int index = 1; index + 2 < numOfPoints; index += 3) {
        append_cubic_samples(sampled, sampled.back(), points[index], points[index + 1],
                             points[index + 2], 0.25f);
    }
    ege_polyline(static_cast<int>(sampled.size()), sampled.data(), pimg);
}

void ege_drawcurve(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    ege_drawcurve(numOfPoints, points, 0.5f, pimg);
}

void ege_drawcurve(int numOfPoints, const ege_point* points, float tension, PIMAGE pimg)
{
    const std::vector<ege_point> sampled =
        sample_cardinal_curve(numOfPoints, points, tension, false);
    if (sampled.size() >= 2) ege_polyline(static_cast<int>(sampled.size()), sampled.data(), pimg);
}

void ege_drawclosedcurve(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    ege_drawclosedcurve(numOfPoints, points, 0.5f, pimg);
}

void ege_drawclosedcurve(int numOfPoints, const ege_point* points, float tension, PIMAGE pimg)
{
    const std::vector<ege_point> sampled =
        sample_cardinal_curve(numOfPoints, points, tension, true);
    if (sampled.size() >= 2) ege_polyline(static_cast<int>(sampled.size()), sampled.data(), pimg);
}

void ege_fillclosedcurve(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    ege_fillclosedcurve(numOfPoints, points, 0.5f, pimg);
}

void ege_fillclosedcurve(int numOfPoints, const ege_point* points, float tension, PIMAGE pimg)
{
    const std::vector<ege_point> sampled =
        sample_cardinal_curve(numOfPoints, points, tension, true);
    if (sampled.size() >= 3) ege_fillpoly(static_cast<int>(sampled.size()), sampled.data(), pimg);
}

void ege_rectangle(float x, float y, float w, float h, PIMAGE pimg)
{
    if (w <= 0.0f || h <= 0.0f) return;
    const ege_point points[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    ege_polygon(4, points, pimg);
}

void ege_arc(float x, float y, float w, float h, float startAngle, float sweepAngle,
             PIMAGE pimg)
{
    const std::vector<ege_point> points = sample_arc(x, y, w, h, startAngle, sweepAngle);
    if (points.size() >= 2) ege_polyline(static_cast<int>(points.size()), points.data(), pimg);
}

void ege_pie(float x, float y, float w, float h, float startAngle, float sweepAngle,
             PIMAGE pimg)
{
    std::vector<ege_point> points = sample_arc(x, y, w, h, startAngle, sweepAngle);
    if (points.empty()) return;
    points.insert(points.begin(), {x + w * 0.5f, y + h * 0.5f});
    ege_polygon(static_cast<int>(points.size()), points.data(), pimg);
}

void ege_fillpie(float x, float y, float w, float h, float startAngle, float sweepAngle,
                 PIMAGE pimg)
{
    std::vector<ege_point> points = sample_arc(x, y, w, h, startAngle, sweepAngle);
    if (points.empty()) return;
    points.insert(points.begin(), {x + w * 0.5f, y + h * 0.5f});
    ege_fillpoly(static_cast<int>(points.size()), points.data(), pimg);
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
    ege_ellipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, pimg);
}

void ege_fillcircle(float x, float y, float radius, PIMAGE pimg)
{
    ege_fillellipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f, pimg);
}

void ege_ellipse(float x, float y, float w, float h, PIMAGE pimg)
{
    const std::vector<ege_point> points = sample_arc(x, y, w, h, 0.0f, 360.0f);
    if (points.size() >= 2) {
        stroke_cpu_polyline(pimg, points.data(), static_cast<int>(points.size()), true);
    }
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
    if (target == NULL) return;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map.erase(static_cast<const void*>(target));
}

void ege_setpattern_lineargradient(float x1, float y1, color_t c1,
                                   float x2, float y2, color_t c2, PIMAGE pimg)
{
    PIMAGE target = resolve_target(pimg);
    if (target == NULL) return;
    PatternState pattern;
    pattern.kind = PatternKind::Linear;
    pattern.start = {x1, y1}; pattern.end = {x2, y2};
    pattern.startColor = c1; pattern.endColor = c2;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(target)] = pattern;
}

void ege_setpattern_pathgradient(ege_point center, color_t centerColor,
    int count, const ege_point* points, int colorCount, const color_t* pointColors, PIMAGE pimg)
{
    PIMAGE target = resolve_target(pimg);
    if (target == NULL || points == NULL || count < 2) return;
    PatternState pattern;
    pattern.kind = PatternKind::Path;
    pattern.start = center;
    pattern.startColor = centerColor;
    pattern.endColor = colorCount > 0 && pointColors ? pointColors[0] : centerColor;
    float right = points[0].x;
    float bottom = points[0].y;
    pattern.x = points[0].x;
    pattern.y = points[0].y;
    for (int i = 1; i < count; ++i) {
        pattern.x = std::min(pattern.x, points[i].x);
        pattern.y = std::min(pattern.y, points[i].y);
        right = std::max(right, points[i].x);
        bottom = std::max(bottom, points[i].y);
    }
    pattern.width = right - pattern.x;
    pattern.height = bottom - pattern.y;
    try {
        pattern.boundaryPoints.assign(points, points + count);
        if (pointColors != NULL && colorCount > 0) {
            pattern.surroundColors.assign(
                pointColors, pointColors + std::min(count, colorCount));
        }
    } catch (...) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(target)] = pattern;
}

void ege_setpattern_ellipsegradient(ege_point center, color_t centerColor,
    float x, float y, float w, float h, color_t outerColor, PIMAGE pimg)
{
    PIMAGE target = resolve_target(pimg);
    if (target == NULL || std::abs(w) <= 1e-12f || std::abs(h) <= 1e-12f) return;
    PatternState pattern;
    pattern.kind = PatternKind::Ellipse;
    pattern.start = center;
    pattern.startColor = centerColor; pattern.endColor = outerColor;
    if (w < 0.0f) { x += w; w = -w; }
    if (h < 0.0f) { y += h; h = -h; }
    pattern.x = x; pattern.y = y; pattern.width = w; pattern.height = h;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(target)] = pattern;
}

void ege_setpattern_texture(PIMAGE srcimg, float x, float y, float w, float h, PIMAGE pimg)
{
    PCIMAGE source = CONVERT_IMAGE_CONST(srcimg);
    PIMAGE target = resolve_target(pimg);
    if (source == NULL || target == NULL || w <= 0.0f || h <= 0.0f) return;

    std::shared_ptr<const TextureSnapshot> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        auto found = g_texture_map.find(static_cast<const void*>(source));
        if (found == g_texture_map.end()) return;
        const std::shared_ptr<const TextureSnapshot> refreshed = make_texture_snapshot(source);
        if (refreshed) found->second = refreshed;
        snapshot = found->second;
    }
    if (!snapshot) return;

    PatternState pattern;
    pattern.kind = PatternKind::Texture;
    pattern.texture = snapshot;
    pattern.x = x; pattern.y = y; pattern.width = w; pattern.height = h;
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    g_pattern_map[static_cast<const void*>(target)] = pattern;
}

void ege_gentexture(bool generate, PIMAGE pimg)
{
    PIMAGE image = resolve_target(pimg);
    if (image == NULL) return;
    image->gentexture(generate);
}

static void put_texture_snapshot(PCIMAGE srcimg, const ege_rect& destination,
                                 const ege_rect& sourceRect, PIMAGE pimg)
{
    PCIMAGE sourceImage = CONVERT_IMAGE_CONST(srcimg);
    PIMAGE target = resolve_target(pimg);
    if (sourceImage == NULL || target == NULL || destination.w <= 0.0f ||
        destination.h <= 0.0f || sourceRect.w <= 0.0f || sourceRect.h <= 0.0f) {
        return;
    }

    std::shared_ptr<const TextureSnapshot> texture;
    ege_transform_matrix transform;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        auto found = g_texture_map.find(static_cast<const void*>(sourceImage));
        if (found == g_texture_map.end() || !found->second) return;
        const std::shared_ptr<const TextureSnapshot> refreshed =
            make_texture_snapshot(sourceImage);
        if (refreshed) found->second = refreshed;
        texture = found->second;
        transform = get_transform_locked(target);
    }

    const float determinant = transform.m11 * transform.m22 - transform.m21 * transform.m12;
    if (std::abs(determinant) < 1e-12f) return;
    const ege_point logicalCorners[4] = {
        {destination.x, destination.y},
        {destination.x + destination.w, destination.y},
        {destination.x + destination.w, destination.y + destination.h},
        {destination.x, destination.y + destination.h}};
    float left = std::numeric_limits<float>::max();
    float top = std::numeric_limits<float>::max();
    float right = std::numeric_limits<float>::lowest();
    float bottom = std::numeric_limits<float>::lowest();
    for (int index = 0; index < 4; ++index) {
        const ege_point corner = transform_point(logicalCorners[index], transform);
        left = std::min(left, corner.x);
        top = std::min(top, corner.y);
        right = std::max(right, corner.x);
        bottom = std::max(bottom, corner.y);
    }

    color_t* pixels = target->getbuffer(IMAGE_BUFFER_READ_WRITE);
    if (pixels == NULL) return;
    const int clipLeft = target->m_enableclip ? std::max(0, target->m_vpt.left) : 0;
    const int clipTop = target->m_enableclip ? std::max(0, target->m_vpt.top) : 0;
    const int clipRight = target->m_enableclip
        ? std::min(target->m_width, target->m_vpt.right) : target->m_width;
    const int clipBottom = target->m_enableclip
        ? std::min(target->m_height, target->m_vpt.bottom) : target->m_height;
    for (int logicalY = static_cast<int>(std::floor(top));
         logicalY < static_cast<int>(std::ceil(bottom)); ++logicalY) {
        const int physicalY = logicalY + target->m_vpt.top;
        if (physicalY < clipTop || physicalY >= clipBottom) continue;
        for (int logicalX = static_cast<int>(std::floor(left));
             logicalX < static_cast<int>(std::ceil(right)); ++logicalX) {
            const int physicalX = logicalX + target->m_vpt.left;
            if (physicalX < clipLeft || physicalX >= clipRight) continue;
            const float transformedX = logicalX + 0.5f - transform.m31;
            const float transformedY = logicalY + 0.5f - transform.m32;
            const float x = (transformedX * transform.m22 - transformedY * transform.m21) /
                determinant;
            const float y = (transformedY * transform.m11 - transformedX * transform.m12) /
                determinant;
            const float u = (x - destination.x) / destination.w;
            const float v = (y - destination.y) / destination.h;
            if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) continue;
            const int sourceX = static_cast<int>(std::floor(sourceRect.x + u * sourceRect.w));
            const int sourceY = static_cast<int>(std::floor(sourceRect.y + v * sourceRect.h));
            if (sourceX < 0 || sourceY < 0 || sourceX >= texture->width ||
                sourceY >= texture->height) continue;
            const color_t sourceColor = texture->pixels[static_cast<std::size_t>(sourceY) *
                                                        texture->width + sourceX];
            color_t& destinationColor = pixels[physicalY * target->m_width + physicalX];
            destinationColor = source_over_premultiplied(destinationColor, sourceColor);
        }
    }
}

void ege_puttexture(PCIMAGE imgSrc, float x, float y, float w, float h, PIMAGE pimg)
{
    const ege_rect destination = {x, y, w, h};
    ege_puttexture(imgSrc, destination, pimg);
}

void ege_puttexture(PCIMAGE imgSrc, ege_rect destination, PIMAGE pimg)
{
    PCIMAGE source = CONVERT_IMAGE_CONST(imgSrc);
    if (source == NULL) return;
    const ege_rect sourceRect = {0.0f, 0.0f,
        static_cast<float>(source->m_width), static_cast<float>(source->m_height)};
    put_texture_snapshot(source, destination, sourceRect, pimg);
}

void ege_puttexture(PCIMAGE imgSrc, ege_rect destination, ege_rect source, PIMAGE pimg)
{
    put_texture_snapshot(imgSrc, destination, source, pimg);
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

ege_path::ege_path()
    : m_data(new(std::nothrow) PathData)
{
}

ege_path::ege_path(const ege_point* points, const unsigned char* types, int count)
    : m_data(new(std::nothrow) PathData)
{
    PathData* data = path_data(this);
    if (data == NULL || points == NULL || count <= 0) return;
    try {
        data->points.assign(points, points + count);
        if (types != NULL) {
            data->types.assign(types, types + count);
        } else {
            data->types.resize(static_cast<std::size_t>(count), kPathTypeLine);
            data->types[0] = kPathTypeStart;
        }
        data->startNewFigure = (data->types.back() & kPathTypeClose) != 0;
    } catch (...) {
        data->points.clear();
        data->types.clear();
        data->startNewFigure = true;
    }
}

ege_path::ege_path(const ege_path& path)
    : m_data(NULL)
{
    const PathData* source = path_data(&path);
    if (source != NULL) m_data = new(std::nothrow) PathData(*source);
}

ege_path::~ege_path()
{
    delete path_data(this);
    m_data = NULL;
}

const void* ege_path::data() const
{
    return m_data;
}

void* ege_path::data()
{
    return m_data;
}

ege_path& ege_path::operator=(const ege_path& path)
{
    if (this == &path) return *this;
    const PathData* source = path_data(&path);
    PathData* replacement = source ? new(std::nothrow) PathData(*source) : NULL;
    if (source != NULL && replacement == NULL) return *this;
    delete path_data(this);
    m_data = replacement;
    return *this;
}

static void draw_path_figures(const ege_path* path, float offsetX, float offsetY, PIMAGE pimg)
{
    const PathData* data = path_data(path);
    if (data == NULL) return;
    std::vector<FlatFigure> figures = flatten_figures(*data);
    offset_figures(figures, offsetX, offsetY);
    for (std::size_t index = 0; index < figures.size(); ++index) {
        std::vector<ege_point> points = figures[index].points;
        if (points.size() < 2) continue;
        if (figures[index].closed && !same_point(points.front(), points.back())) {
            const ege_point firstPoint = points.front();
            points.push_back(firstPoint);
        }
        ege_polyline(static_cast<int>(points.size()), points.data(), pimg);
    }
}

static void fill_path_figures(const ege_path* path, float offsetX, float offsetY, PIMAGE pimg)
{
    const PathData* data = path_data(path);
    if (data == NULL) return;
    std::vector<FlatFigure> figures = flatten_figures(*data);
    offset_figures(figures, offsetX, offsetY);
    const ege_rect bounds = figure_bounds(figures);
    if (figures.empty()) return;
    fill_cpu_shape(pimg, bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h,
        [&](float x, float y) { return point_in_figures(figures, data->fillMode, x, y); },
        false);
}

void ege_drawpath(const ege_path* path, PIMAGE pimg)
{
    draw_path_figures(path, 0.0f, 0.0f, pimg);
}

void ege_fillpath(const ege_path* path, PIMAGE pimg)
{
    fill_path_figures(path, 0.0f, 0.0f, pimg);
}

void ege_drawpath(const ege_path* path, float x, float y, PIMAGE pimg)
{
    draw_path_figures(path, x, y, pimg);
}

void ege_fillpath(const ege_path* path, float x, float y, PIMAGE pimg)
{
    fill_path_figures(path, x, y, pimg);
}

ege_path* ege_path_create()
{
    return new(std::nothrow) ege_path;
}

ege_path* ege_path_createfrom(const ege_point* points, const unsigned char* types, int count)
{
    return new(std::nothrow) ege_path(points, types, count);
}

ege_path* ege_path_clone(const ege_path* path)
{
    return path ? new(std::nothrow) ege_path(*path) : NULL;
}

void ege_path_destroy(const ege_path* path)
{
    delete path;
}

void ege_path_start(ege_path* path)
{
    PathData* data = path_data(path);
    if (data != NULL) data->startNewFigure = true;
}

void ege_path_close(ege_path* path)
{
    PathData* data = path_data(path);
    if (data != NULL) path_close_figure(*data);
}

void ege_path_closeall(ege_path* path)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    for (std::size_t index = 1; index < data->types.size(); ++index) {
        if ((data->types[index] & kPathTypeMask) == kPathTypeStart) {
            data->types[index - 1] = static_cast<unsigned char>(data->types[index - 1] | kPathTypeClose);
        }
    }
    if (!data->types.empty()) {
        data->types.back() = static_cast<unsigned char>(data->types.back() | kPathTypeClose);
    }
    data->startNewFigure = true;
}

void ege_path_setfillmode(ege_path* path, fill_mode mode)
{
    PathData* data = path_data(path);
    if (data != NULL && (mode == FILLMODE_ALTERNATE || mode == FILLMODE_WINDING ||
                         mode == FILLMODE_DEFAULT)) {
        data->fillMode = mode == FILLMODE_DEFAULT ? FILLMODE_ALTERNATE : mode;
    }
}

void ege_path_reset(ege_path* path)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    data->points.clear();
    data->types.clear();
    data->fillMode = FILLMODE_ALTERNATE;
    data->startNewFigure = true;
}

void ege_path_reverse(ege_path* path)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    assign_reversed(*data, decode_path_figures(*data));
}

void ege_path_widen(ege_path* path, float lineWidth, const ege_transform_matrix* matrix)
{
    ege_path_widen(path, lineWidth, matrix, 0.25f);
}

void ege_path_widen(ege_path* path, float lineWidth, const ege_transform_matrix* matrix,
                    float flatness)
{
    PathData* data = path_data(path);
    if (data == NULL || lineWidth <= 0.0f) return;
    std::vector<FlatFigure> source = flatten_figures(*data, std::max(0.01f, flatness));
    transform_figures(source, matrix);
#if defined(__APPLE__)
    PathData flattened;
    assign_flattened(flattened, source);
    CGMutablePathRef sourcePath = create_cg_path(flattened);
    if (sourcePath != NULL) {
        CGPathRef widenedPath = CGPathCreateCopyByStrokingPath(sourcePath, NULL, lineWidth,
            kCGLineCapButt, kCGLineJoinMiter, 10.0f);
        CGPathRelease(sourcePath);
        if (widenedPath != NULL) {
            assign_cg_path(*data, widenedPath);
            CGPathRelease(widenedPath);
            data->fillMode = FILLMODE_WINDING;
            return;
        }
    }
#endif
    std::vector<FlatFigure> widened;
    const float halfWidth = lineWidth * 0.5f;
    for (std::size_t figureIndex = 0; figureIndex < source.size(); ++figureIndex) {
        const FlatFigure& figure = source[figureIndex];
        if (figure.points.empty()) continue;
        const std::size_t edgeCount = figure.closed ? figure.points.size() : figure.points.size() - 1;
        for (std::size_t edge = 0; edge < edgeCount; ++edge) {
            const ege_point& first = figure.points[edge];
            const ege_point& second = figure.points[(edge + 1) % figure.points.size()];
            const float dx = second.x - first.x;
            const float dy = second.y - first.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length <= 1e-6f) continue;
            const float nx = -dy * halfWidth / length;
            const float ny = dx * halfWidth / length;
            FlatFigure quad;
            quad.closed = true;
            quad.points.push_back({first.x + nx, first.y + ny});
            quad.points.push_back({second.x + nx, second.y + ny});
            quad.points.push_back({second.x - nx, second.y - ny});
            quad.points.push_back({first.x - nx, first.y - ny});
            widened.push_back(quad);
        }
        for (std::size_t vertex = 0; vertex < figure.points.size(); ++vertex) {
            FlatFigure cap;
            cap.closed = true;
            const int segments = 16;
            for (int segment = 0; segment < segments; ++segment) {
                const float angle = 2.0f * kPi * segment / segments;
                cap.points.push_back({figure.points[vertex].x + std::cos(angle) * halfWidth,
                                      figure.points[vertex].y + std::sin(angle) * halfWidth});
            }
            widened.push_back(cap);
        }
    }
    assign_flattened(*data, widened);
    data->fillMode = FILLMODE_WINDING;
}

void ege_path_flatten(ege_path* path, const ege_transform_matrix* matrix)
{
    ege_path_flatten(path, matrix, 0.25f);
}

void ege_path_flatten(ege_path* path, const ege_transform_matrix* matrix, float flatness)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    std::vector<FlatFigure> figures = flatten_figures(*data, std::max(0.01f, flatness));
    transform_figures(figures, matrix);
    assign_flattened(*data, figures);
}

void ege_path_warp(ege_path* path, const ege_point* points, int count, const ege_rect* rect,
                   const ege_transform_matrix* matrix)
{
    ege_path_warp(path, points, count, rect, matrix, 0.25f);
}

void ege_path_warp(ege_path* path, const ege_point* points, int count, const ege_rect* rect,
                   const ege_transform_matrix* matrix, float flatness)
{
    PathData* data = path_data(path);
    if (data == NULL || points == NULL || rect == NULL || (count != 3 && count != 4) ||
        std::abs(rect->w) <= 1e-12f || std::abs(rect->h) <= 1e-12f) return;
    std::vector<FlatFigure> figures = flatten_figures(*data, std::max(0.01f, flatness));
    transform_figures(figures, matrix);
    const ege_point destination[4] = {
        points[0], points[1], points[2],
        count == 4 ? points[3] : ege_point{
            points[1].x + points[2].x - points[0].x,
            points[1].y + points[2].y - points[0].y}};
    for (std::size_t figureIndex = 0; figureIndex < figures.size(); ++figureIndex) {
        for (std::size_t pointIndex = 0; pointIndex < figures[figureIndex].points.size(); ++pointIndex) {
            const ege_point source = figures[figureIndex].points[pointIndex];
            const float u = (source.x - rect->x) / rect->w;
            const float v = (source.y - rect->y) / rect->h;
            ege_point warped;
            if (perspective_warp_point(u, v, destination, warped)) {
                figures[figureIndex].points[pointIndex] = warped;
            }
        }
    }
    assign_flattened(*data, figures);
}

void ege_path_outline(ege_path* path, const ege_transform_matrix* matrix)
{
    ege_path_outline(path, matrix, 0.25f);
}

void ege_path_outline(ege_path* path, const ege_transform_matrix* matrix, float flatness)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    std::vector<FlatFigure> figures = flatten_figures(*data, std::max(0.01f, flatness));
    transform_figures(figures, matrix);
    for (std::size_t index = 0; index < figures.size(); ++index) {
        if (figures[index].points.size() >= 3) figures[index].closed = true;
    }
    assign_flattened(*data, external_boundary_figures(figures, data->fillMode));
}

bool ege_path_inpath(const ege_path* path, float x, float y)
{
    const PathData* data = path_data(path);
    return data != NULL && point_in_figures(flatten_figures(*data), data->fillMode, x, y);
}

bool ege_path_inpath(const ege_path* path, float x, float y, PCIMAGE pimg)
{
    const PathData* data = path_data(path);
    PIMAGE image = resolve_target(const_cast<PIMAGE>(pimg));
    if (data == NULL || image == NULL) return false;
    ege_transform_matrix transform;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        transform = get_transform_locked(image);
    }
    // The Graphics overload receives device coordinates. EGE's viewport is
    // appended after the enhanced world transform, matching syncGraphicsViewport().
    const ege_point viewportLocal = {
        x - static_cast<float>(image->m_vpt.left),
        y - static_cast<float>(image->m_vpt.top)};
    ege_point source;
    return inverse_transform_point(viewportLocal, transform, source) &&
        point_in_figures(flatten_figures(*data), data->fillMode, source.x, source.y);
}

static bool path_point_in_stroke(const ege_path* path, float x, float y, float lineWidth)
{
    const PathData* data = path_data(path);
    if (data == NULL) return false;
    const std::vector<FlatFigure> figures = flatten_figures(*data);
    const float tolerance = std::max(0.5f, lineWidth * 0.5f);
    for (std::size_t figureIndex = 0; figureIndex < figures.size(); ++figureIndex) {
        const FlatFigure& figure = figures[figureIndex];
        if (figure.points.size() < 2) continue;
        const std::size_t edgeCount = figure.closed ? figure.points.size() : figure.points.size() - 1;
        for (std::size_t edge = 0; edge < edgeCount; ++edge) {
            if (distance_to_segment(x, y, figure.points[edge],
                figure.points[(edge + 1) % figure.points.size()]) <= tolerance) return true;
        }
    }
    return false;
}

bool ege_path_instroke(const ege_path* path, float x, float y)
{
    return path_point_in_stroke(path, x, y, 1.0f);
}

bool ege_path_instroke(const ege_path* path, float x, float y, PCIMAGE pimg)
{
    const PathData* data = path_data(path);
    PIMAGE image = resolve_target(const_cast<PIMAGE>(pimg));
    if (data == NULL || image == NULL || image->m_linestyle.linestyle == PS_NULL) return false;
    ege_transform_matrix transform;
    {
        std::lock_guard<std::mutex> lock(g_matrix_mutex);
        transform = get_transform_locked(image);
    }
#if defined(__APPLE__)
    CGPathRef penOutline = create_pen_outline_cg_path(*data, *image);
    if (penOutline == NULL) return false;
    const CGAffineTransform affine = cg_affine_transform(transform);
    CGPathRef transformed = CGPathCreateCopyByTransformingPath(penOutline, &affine);
    CGPathRelease(penOutline);
    if (transformed == NULL) return false;
    const bool visible = CGPathContainsPoint(transformed, NULL,
        CGPointMake(x - image->m_vpt.left, y - image->m_vpt.top), false);
    CGPathRelease(transformed);
    return visible;
#else
    const ege_point viewportLocal = {
        x - static_cast<float>(image->m_vpt.left),
        y - static_cast<float>(image->m_vpt.top)};
    ege_point source;
    return inverse_transform_point(viewportLocal, transform, source) &&
        path_point_in_stroke(path, source.x, source.y, image->m_linewidth);
#endif
}

ege_point ege_path_lastpoint(const ege_path* path)
{
    const PathData* data = path_data(path);
    return data != NULL && !data->points.empty() ? data->points.back() : ege_point{0.0f, 0.0f};
}

int ege_path_pointcount(const ege_path* path)
{
    const PathData* data = path_data(path);
    return data ? static_cast<int>(data->points.size()) : 0;
}

ege_rect ege_path_getbounds(const ege_path* path, const ege_transform_matrix* matrix)
{
    const PathData* data = path_data(path);
    if (data == NULL) return ege_rect{0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<FlatFigure> figures = flatten_figures(*data);
    transform_figures(figures, matrix);
    return figure_bounds(figures);
}

ege_rect ege_path_getbounds(const ege_path* path, const ege_transform_matrix* matrix, PCIMAGE pimg)
{
    const PathData* data = path_data(path);
    PIMAGE image = resolve_target(const_cast<PIMAGE>(pimg));
    if (data == NULL || image == NULL || data->points.empty()) {
        return ege_rect{0.0f, 0.0f, 0.0f, 0.0f};
    }
#if defined(__APPLE__)
    CGPathRef penOutline = create_pen_outline_cg_path(*data, *image);
    if (penOutline != NULL) {
        CGPathRef explicitlyTransformed = penOutline;
        if (matrix != NULL) {
            const CGAffineTransform explicitAffine = cg_affine_transform(*matrix);
            explicitlyTransformed = CGPathCreateCopyByTransformingPath(
                penOutline, &explicitAffine);
            CGPathRelease(penOutline);
        }
        if (explicitlyTransformed != NULL) {
            const CGRect rectangle = CGPathGetBoundingBox(explicitlyTransformed);
            CGPathRelease(explicitlyTransformed);
            return ege_rect{static_cast<float>(rectangle.origin.x),
                static_cast<float>(rectangle.origin.y),
                static_cast<float>(rectangle.size.width),
                static_cast<float>(rectangle.size.height)};
        }
    }
#endif
    std::vector<FlatFigure> figures = flatten_figures(*data);
    transform_figures(figures, matrix);
    ege_rect bounds = figure_bounds(figures);
    const float halfWidth = std::max(0.0f, image->m_linewidth * 0.5f);
    bounds.x -= halfWidth;
    bounds.y -= halfWidth;
    bounds.w += halfWidth * 2.0f;
    bounds.h += halfWidth * 2.0f;
    return bounds;
}

ege_point* ege_path_getpathpoints(const ege_path* path, ege_point* points)
{
    const PathData* data = path_data(path);
    if (data == NULL || data->points.empty()) return NULL;
    if (points == NULL) points = new(std::nothrow) ege_point[data->points.size()];
    if (points != NULL) std::copy(data->points.begin(), data->points.end(), points);
    return points;
}

unsigned char* ege_path_getpathtypes(const ege_path* path, unsigned char* types)
{
    const PathData* data = path_data(path);
    if (data == NULL || data->types.empty()) return NULL;
    if (types == NULL) types = new(std::nothrow) unsigned char[data->types.size()];
    if (types != NULL) std::copy(data->types.begin(), data->types.end(), types);
    return types;
}

void ege_path_transform(ege_path* path, const ege_transform_matrix* matrix)
{
    PathData* data = path_data(path);
    if (data == NULL || matrix == NULL) return;
    for (std::size_t index = 0; index < data->points.size(); ++index) {
        data->points[index] = transform_point(data->points[index], *matrix);
    }
}

void ege_path_addpath(ege_path* destinationPath, const ege_path* sourcePath, bool connect)
{
    PathData* destination = path_data(destinationPath);
    const PathData* source = path_data(sourcePath);
    if (destination == NULL || source == NULL || source->points.empty()) return;
    const PathData sourceCopy = *source;
    for (std::size_t index = 0; index < sourceCopy.points.size(); ++index) {
        unsigned char type = sourceCopy.types[index];
        if ((type & kPathTypeMask) == kPathTypeStart && index == 0 && connect &&
            !destination->points.empty() && !destination->startNewFigure) {
            type = static_cast<unsigned char>((type & ~kPathTypeMask) | kPathTypeLine);
        }
        destination->points.push_back(sourceCopy.points[index]);
        destination->types.push_back(type);
    }
    destination->startNewFigure =
        (destination->types.back() & kPathTypeClose) != 0;
}

void ege_path_addline(ege_path* path, float x1, float y1, float x2, float y2)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    const ege_point points[2] = {{x1, y1}, {x2, y2}};
    path_add_polyline(*data, 2, points, false, false);
}

void ege_path_addarc(ege_path* path, float x, float y, float width, float height,
                     float startAngle, float sweepAngle)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    const std::vector<ege_point> points = sample_arc(x, y, width, height, startAngle, sweepAngle);
    path_add_polyline(*data, static_cast<int>(points.size()), points.data(), false, false);
}

void ege_path_addpolyline(ege_path* path, int numOfPoints, const ege_point* points)
{
    PathData* data = path_data(path);
    if (data != NULL) path_add_polyline(*data, numOfPoints, points, false, false);
}

void ege_path_addbezier(ege_path* path, int numOfPoints, const ege_point* points)
{
    PathData* data = path_data(path);
    if (data != NULL) path_add_beziers(*data, numOfPoints, points);
}

void ege_path_addbezier(ege_path* path, float x1, float y1, float x2, float y2,
                        float x3, float y3, float x4, float y4)
{
    const ege_point points[4] = {{x1, y1}, {x2, y2}, {x3, y3}, {x4, y4}};
    ege_path_addbezier(path, 4, points);
}

void ege_path_addcurve(ege_path* path, int numOfPoints, const ege_point* points)
{
    ege_path_addcurve(path, numOfPoints, points, 0.5f);
}

void ege_path_addcurve(ege_path* path, int numOfPoints, const ege_point* points, float tension)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    const std::vector<ege_point> sampled = sample_cardinal_curve(numOfPoints, points, tension, false);
    path_add_polyline(*data, static_cast<int>(sampled.size()), sampled.data(), false, false);
}

void ege_path_addcircle(ege_path* path, float x, float y, float radius)
{
    ege_path_addellipse(path, x - radius, y - radius, radius * 2.0f, radius * 2.0f);
}

void ege_path_addrect(ege_path* path, float x, float y, float width, float height)
{
    PathData* data = path_data(path);
    if (data == NULL || width <= 0.0f || height <= 0.0f) return;
    const ege_point points[4] = {
        {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
    path_add_polyline(*data, 4, points, true, true);
}

void ege_path_addellipse(ege_path* path, float x, float y, float width, float height)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    const std::vector<ege_point> points = sample_arc(x, y, width, height, 0.0f, 360.0f);
    path_add_polyline(*data, static_cast<int>(points.size()), points.data(), true, true);
}

void ege_path_addpie(ege_path* path, float x, float y, float width, float height,
                     float startAngle, float sweepAngle)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    std::vector<ege_point> points = sample_arc(x, y, width, height, startAngle, sweepAngle);
    if (points.empty()) return;
    points.insert(points.begin(), {x + width * 0.5f, y + height * 0.5f});
    path_add_polyline(*data, static_cast<int>(points.size()), points.data(), true, true);
}

static void add_text_rectangles(ege_path* path, float x, float y, std::size_t length,
                                float height, const bool* drawable, int fontStyle)
{
    if (path == NULL || drawable == NULL || height <= 0.0f) return;
    const float advance = height * 0.6f;
    const float originX = x;
    for (std::size_t index = 0; index < length; ++index) {
        if (!drawable[index]) {
            x += advance;
            continue;
        }
        ege_path_addrect(path, x, y, advance * 0.85f, height);
        x += advance;
    }
    if ((fontStyle & FONTSTYLE_UNDERLINE) != 0 && x > originX) {
        ege_path_addrect(path, originX, y + height * 0.9f, x - originX, std::max(1.0f, height * 0.06f));
    }
    if ((fontStyle & FONTSTYLE_STRIKEOUT) != 0 && x > originX) {
        ege_path_addrect(path, originX, y + height * 0.5f, x - originX, std::max(1.0f, height * 0.06f));
    }
}

void ege_path_addtext(ege_path* path, float x, float y, const char* text, float height,
                      int length, const char* typeface, int fontStyle)
{
    if (path == NULL || text == NULL || length == 0) return;
    const std::size_t count = length < 0 ? std::strlen(text) : static_cast<std::size_t>(length);
#if defined(__APPLE__)
    const std::string utf8(text, text + count);
    const std::string fontName = typeface != NULL ? std::string(typeface) : std::string();
    if (add_coretext_outlines(path, x, y, utf8, height, fontName, fontStyle)) return;
#else
    (void)typeface;
#endif
    std::unique_ptr<bool[]> drawable(new(std::nothrow) bool[count]);
    if (!drawable && count != 0) return;
    for (std::size_t index = 0; index < count; ++index) {
        drawable[index] = text[index] != ' ' && text[index] != '\t' && text[index] != '\r' && text[index] != '\n';
    }
    add_text_rectangles(path, x, y, count, height, drawable.get(), fontStyle);
}

void ege_path_addtext(ege_path* path, float x, float y, const wchar_t* text, float height,
                      int length, const wchar_t* typeface, int fontStyle)
{
    if (path == NULL || text == NULL || length == 0) return;
    const std::size_t count = length < 0 ? std::wcslen(text) : static_cast<std::size_t>(length);
#if defined(__APPLE__)
    const std::wstring wideText(text, text + count);
    const std::wstring wideTypeface = typeface != NULL ? std::wstring(typeface) : std::wstring();
    if (add_coretext_outlines(path, x, y, w2utf8(wideText.c_str()), height,
        wideTypeface.empty() ? std::string() : w2utf8(wideTypeface.c_str()), fontStyle)) return;
#else
    (void)typeface;
#endif
    std::unique_ptr<bool[]> drawable(new(std::nothrow) bool[count]);
    if (!drawable && count != 0) return;
    for (std::size_t index = 0; index < count; ++index) {
        drawable[index] = text[index] != L' ' && text[index] != L'\t' &&
            text[index] != L'\r' && text[index] != L'\n';
    }
    add_text_rectangles(path, x, y, count, height, drawable.get(), fontStyle);
}

void ege_path_addpolygon(ege_path* path, int numOfPoints, const ege_point* points)
{
    PathData* data = path_data(path);
    if (data != NULL) path_add_polyline(*data, numOfPoints, points, true, true);
}

void ege_path_addclosedcurve(ege_path* path, int numOfPoints, const ege_point* points)
{
    ege_path_addclosedcurve(path, numOfPoints, points, 0.5f);
}

void ege_path_addclosedcurve(ege_path* path, int numOfPoints, const ege_point* points,
                             float tension)
{
    PathData* data = path_data(path);
    if (data == NULL) return;
    std::vector<ege_point> sampled = sample_cardinal_curve(numOfPoints, points, tension, true);
    if (sampled.size() > 1 && same_point(sampled.front(), sampled.back())) sampled.pop_back();
    path_add_polyline(*data, static_cast<int>(sampled.size()), sampled.data(), true, true);
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
        updated = prepend_matrix(get_transform_locked(pimg), r);
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
        updated = prepend_matrix(get_transform_locked(pimg), t);
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
        updated = prepend_matrix(get_transform_locked(pimg), s);
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
    // Match GDI+: a null matrix is an invalid input and leaves the existing
    // world transform untouched. Reset is provided by ege_transform_reset().
    if (matrix == NULL) return;
    const ege_transform_matrix value = *matrix;
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
