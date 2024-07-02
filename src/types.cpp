#include "ege_head.h"
#include "ege_common.h"
#include "types.h"

#include <assert.h>

namespace ege
{

//------------------------------------------------------------------------------
//                                    Rect
//------------------------------------------------------------------------------

/**
 * @brief 将起点在 x 长度为 length(整数范围)的线段进行裁剪，使线段在限制范围内
 * @note 线段起点为 x (包含), 终点为 x + length (不包含)
 *
 * @param[in, out] x       起始点
 * @param[in, out] length  线段长度(可正可负)
 * @param[in] minval 裁剪后线段的最小值
 * @param[in] maxval 裁剪后线段的最大值
 */
static void clipByLimits(int& x,  int& length, int minval, int maxval)
{
    if (minval > maxval) {
        length = 0;
    } else if (length == 0) {
        x = minval;
    } else {
        /* 指示范围的端点(包含) */
        int start, end;
        if (length > 0) {         // x ----xMin ---- xMax ----> right()
            /* 右边界截断到 INT_MAX */
            end   = ((unsigned int)(length - 1) > (unsigned int)(INT_MAX - x)) ? INT_MAX : (x + length - 1);
            start = MAX(x, minval);
            end   = MIN(end, maxval);

            x = start;
            length = (start > end) ? 0 : (end - start + 1);
        } else if (length < 0) { // left <----xMin ---- xMax ---- x
            /* 左边界截断到 INT_MIN */
            end   = ((unsigned int)(-length - 1) > (unsigned int)(x - INT_MIN)) ? INT_MIN : (x + length + 1);
            start = MIN(x, maxval);
            end   = MAX(end, minval);

            x = start;
            length = (start < end) ? 0 : (end - start - 1);
        }
    }
}

Rect normalize(const Rect& rect)
{
    return Rect(rect).normalize();
}

Rect intersect(const Rect& a, const Rect& b)
{
    return Rect(a).intersect(b);
}

/**
 * @brief 求原对象与 rect 的交集，结果写入原对象
 *
 * @param rect
 * @return Rect& 原对象的引用
 * @note  目前求交集需要先求取两个矩形的边界，计算时可能会由于溢出而得到
 *        错误的结果，所以要求矩形边界在 int 范围内。
 */
Rect& Rect::intersect(const Rect& rect)
{
    if (isEmpty() || rect.isEmpty())
        return setEmpty();

    Rect a(*this), b(rect);
    a.normalize();
    b.normalize();

    if (   (a.right()  <= b.left()) || (b.right()  <= a.left())
        || (a.bottom() <= b.top())  || (b.bottom() <= a.top() )) {
        return setEmpty();
    }

    int left   = MAX(a.left(),   b.left());
    int top    = MAX(a.top(),    b.top());
    int right  = MIN(a.right(),  b.right());
    int bottom = MIN(a.bottom(), b.bottom());

    setBounds(left, top, right, bottom);

    return *this;
}

/**
 * @brief 求原对象与 rect 的并集，结果写入原对象
 *
 * @param rect
 * @return Rect& 原对象的引用
 * @note  目前求并集需要先求取两个矩形的边界，计算时可能会由于溢出而得到
 *        错误的结果，所以要求矩形边界在 int 范围内。
 */
Rect& Rect::unite(const Rect& rect)
{
    if (rect.isEmpty())
        return *this;;

    if (isEmpty())
        return *this = rect;

    Rect a(*this), b(rect);
    a.normalize();
    b.normalize();

    int left   = MIN(a.left(),   b.left());
    int top    = MIN(a.top(),    b.top());
    int right  = MAX(a.right(),  b.right());
    int bottom = MAX(a.bottom(), b.bottom());

    setBounds(left, top, right, bottom);
    return *this;
}

Rect& Rect::unite(int x, int y, int width, int height)
{
    unite(Rect(x, y, width, height));
    return *this;
}

Rect& Rect::unite(const Point& point)
{
    if (isEmpty()) {
        set(point, Size(1, 1));
    } else {
        if (point.x < left()) {
            setLeft(point.x);
        } else if (point.x >= right()) {
            setRight(point.x + 1);
        }

        if (point.y < top()) {
            setTop(point.y);
        } else if (point.y >= bottom()) {
            setBottom(point.y + 1);
        }
    }

    return *this;
}

Rect& Rect::unite(const Point points[], int count)
{
    unite(bounds(points, count));
    return *this;
}

Rect unite(const Rect& a, const Rect& b)
{
    return Rect(a).unite(b);
}

/**
 * @brief 求包含所有点的最小包围矩形
 * @param a
 * @param b
 * @return 包含所有点的包围矩形
 * @warning 矩形的宽高为 int 型，因此要求所有点在 x 和 y 上的最大最小值之差
 *          需要小于 INT_MAX，否则会得到错误的结果
 */
Rect bounds(const Point points[], int count)
{
    if (count <= 0)
        return Rect();

    int xMin = points[0].x, xMax = xMin;
    int yMin = points[0].y, yMax = yMin;

    for (int i = 1; i < count; i++) {
        if (points[i].x < xMin) {
            xMin = points[i].x;
        } else if (points[i].x > xMax) {
            xMax = points[i].x;
        }

        if (points[i].y < yMin) {
            yMin = points[i].y;
        } else if (points[i].y > yMax) {
            yMax = points[i].y;
        }
    }

    assert((unsigned int)(xMax - xMin) < INT_MAX);
    assert((unsigned int)(yMax - xMin) < INT_MAX);

    Point leftTop(xMin, yMin);
    Point bottomRight(xMax + 1, yMax + 1);

    return Rect(leftTop, bottomRight, false);
}

/**
 * @brief 求包含两个点的最小矩形
 * @param a
 * @param b
 * @return 矩形
 * @warning 矩形的宽高为 int 型，因此两个点的在 x 和 y 上的差值需要小于 INT_MAX
 */
Rect bounds(const Point& a, const Point& b)
{
    int xMin, xMax;
    if (a.x <= b.x) {
        xMin = a.x;
        xMax = b.x;
    } else {
        xMin = b.x;
        xMax = a.x;
    }

    int yMin, yMax;
    if (a.y <= b.y) {
        yMin = a.y;
        yMax = b.y;
    } else {
        yMin = b.y;
        yMax = a.y;
    }

    assert((unsigned int)(xMax - xMin) < INT_MAX);
    assert((unsigned int)(yMax - xMin) < INT_MAX);

    Point leftTop(xMin, yMin);
    Point bottomRight(xMax + 1, yMax + 1);

    return Rect(leftTop, bottomRight, false);
}

Rect& Rect::clip(int xMin, int xMax, int yMin, int yMax)
{
    return clipX(xMin, xMax).clipY(yMin, yMax);
}

Rect& Rect::clipX(int xMin, int xMax)
{
    clipByLimits(x, width, xMin, xMax);
    return *this;
}

Rect& Rect::clipY(int yMin, int yMax)
{
    clipByLimits(y, height, yMin, yMax);
    return *this;
}

Rect& Rect::clip ()
{
    return clipX().clipY();

}
Rect& Rect::clipX()
{
    if (width > 0) {
        if ((unsigned int)(width - 1) > (unsigned int)(INT_MAX - x))
            width = INT_MAX - x + 1;
    } else if (width < 0) {
        if ((unsigned)(x - INT_MIN) < (unsigned)(-width - 1))
            width = INT_MIN - x - 1;
    }

    return *this;
}

Rect& Rect::clipY()
{
    if (height > 0) {
        if ((unsigned int)(height - 1) > (unsigned int)(INT_MAX - x))
            height = INT_MAX - y + 1;
    } else if (height < 0) {
        if ((unsigned)(y - INT_MIN) < (unsigned)(-height - 1))
            height = INT_MIN - y - 1;
    }

    return *this;
}

//------------------------------------------------------------------------------

}
