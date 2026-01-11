/*
 * EGE (Easy Graphics Engine)
 * filename  ege_gdiplus_fallback.cpp
 *
 * Cross-platform fallback implementations for a small subset of "EGE_GDIPLUS"
 * enhanced APIs.
 *
 * Motivation:
 * - Many demos call ege_* enhanced helpers (ege_circle/ege_drawimage/transform...).
 * - On non-Windows builds EGE_GDIPLUS is not available, but we still want the
 *   demos to compile and run with a best-effort behavior.
 *
 * Notes:
 * - Pattern/gradient APIs are currently no-ops.
 * - Transform APIs maintain an internal affine matrix state, but drawing
 *   operations in this fallback do not apply the transform automatically.
 */

#include "ege_head.h"

#ifndef EGE_GDIPLUS

#include <cmath>
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

static ege_transform_matrix get_transform_locked(PIMAGE pimg)
{
    const void* key = static_cast<const void*>(pimg);
    auto it = g_transform_map.find(key);
    if (it != g_transform_map.end()) {
        return it->second;
    }
    return identity_matrix();
}

static void set_transform_locked(PIMAGE pimg, const ege_transform_matrix& m)
{
    const void* key = static_cast<const void*>(pimg);
    g_transform_map[key] = m;
}

static inline int iround(float v)
{
    return static_cast<int>(std::lround(static_cast<double>(v)));
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

    std::vector<int> ipoints;
    ipoints.resize(static_cast<size_t>(numOfPoints) * 2);
    for (int i = 0; i < numOfPoints; ++i) {
        ipoints[static_cast<size_t>(i) * 2 + 0] = iround(points[i].x);
        ipoints[static_cast<size_t>(i) * 2 + 1] = iround(points[i].y);
    }
    fillpoly(numOfPoints, ipoints.data(), pimg);
}

void ege_fillrect(float x, float y, float w, float h, PIMAGE pimg)
{
    const int left = iround(x);
    const int top = iround(y);
    const int right = iround(x + w);
    const int bottom = iround(y + h);
    fillrect(left, top, right, bottom, pimg);
}

void ege_circle(float x, float y, float radius, PIMAGE pimg)
{
    circle(iround(x), iround(y), iround(radius), pimg);
}

void ege_fillcircle(float x, float y, float radius, PIMAGE pimg)
{
    fillcircle(iround(x), iround(y), iround(radius), pimg);
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
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float rx = w * 0.5f;
    const float ry = h * 0.5f;
    fillellipsef(cx, cy, rx, ry, pimg);
}

void ege_roundrect(float x, float y, float w, float h, float radius, PIMAGE pimg)
{
    const int left = iround(x);
    const int top = iround(y);
    const int right = iround(x + w);
    const int bottom = iround(y + h);
    const int r = (radius > 0.0f) ? iround(radius) : 0;
    roundrect(left, top, right, bottom, r, pimg);
}

void ege_fillroundrect(float x, float y, float w, float h, float radius, PIMAGE pimg)
{
    const int left = iround(x);
    const int top = iround(y);
    const int right = iround(x + w);
    const int bottom = iround(y + h);
    const int r = (radius > 0.0f) ? iround(radius) : 0;
    fillroundrect(left, top, right, bottom, r, pimg);
}

void ege_roundrect(float x, float y, float w, float h, float radius1, float radius2, float radius3, float radius4, PIMAGE pimg)
{
    // Best-effort: choose a single radius to approximate per-corner radii.
    float r = radius1;
    if (radius2 < r) r = radius2;
    if (radius3 < r) r = radius3;
    if (radius4 < r) r = radius4;
    ege_roundrect(x, y, w, h, r, pimg);
}

void ege_fillroundrect(float x, float y, float w, float h, float radius1, float radius2, float radius3, float radius4, PIMAGE pimg)
{
    float r = radius1;
    if (radius2 < r) r = radius2;
    if (radius3 < r) r = radius3;
    if (radius4 < r) r = radius4;
    ege_fillroundrect(x, y, w, h, r, pimg);
}

void ege_setpattern_none(PIMAGE)
{
    // no-op fallback
}

void ege_setpattern_lineargradient(float, float, color_t, float, float, color_t, PIMAGE)
{
    // no-op fallback
}

void ege_setpattern_pathgradient(ege_point, color_t, int, const ege_point*, int, const color_t*, PIMAGE)
{
    // no-op fallback
}

void ege_setpattern_ellipsegradient(ege_point, color_t, float, float, float, float, color_t, PIMAGE)
{
    // no-op fallback
}

void ege_setpattern_texture(PIMAGE, float, float, float, float, PIMAGE)
{
    // no-op fallback
}

void ege_drawimage(PCIMAGE imgSrc, int xDest, int yDest, PIMAGE pimg)
{
    if (pimg) {
        putimage(pimg, xDest, yDest, imgSrc, SRCCOPY);
    } else {
        putimage(xDest, yDest, imgSrc, SRCCOPY);
    }
}

void ege_drawimage(PCIMAGE imgSrc,
    int xDest, int yDest, int widthDest, int heightDest,
    int xSrc,  int ySrc,  int widthSrc,  int heightSrc,
    PIMAGE pimg)
{
    if (pimg) {
        putimage(pimg, xDest, yDest, widthDest, heightDest, imgSrc, xSrc, ySrc, widthSrc, heightSrc, SRCCOPY);
    } else {
        putimage(xDest, yDest, widthDest, heightDest, imgSrc, xSrc, ySrc, widthSrc, heightSrc, SRCCOPY);
    }
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

    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    const ege_transform_matrix cur = get_transform_locked(pimg);
    set_transform_locked(pimg, mul_matrix(cur, r));
}

void ege_transform_translate(float x, float y, PIMAGE pimg)
{
    ege_transform_matrix t = identity_matrix();
    t.m31 = x;
    t.m32 = y;

    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    const ege_transform_matrix cur = get_transform_locked(pimg);
    set_transform_locked(pimg, mul_matrix(cur, t));
}

void ege_transform_scale(float xScale, float yScale, PIMAGE pimg)
{
    ege_transform_matrix s = identity_matrix();
    s.m11 = xScale;
    s.m22 = yScale;

    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    const ege_transform_matrix cur = get_transform_locked(pimg);
    set_transform_locked(pimg, mul_matrix(cur, s));
}

void ege_transform_reset(PIMAGE pimg)
{
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    set_transform_locked(pimg, identity_matrix());
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
    std::lock_guard<std::mutex> lock(g_matrix_mutex);
    if (matrix) {
        set_transform_locked(pimg, *matrix);
    } else {
        set_transform_locked(pimg, identity_matrix());
    }
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
