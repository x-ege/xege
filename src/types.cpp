#include "ege_head.h"
#include "ege_common.h"
#include "types.h"

namespace ege
{

//------------------------------------------------------------------------------
//                                    Rect
//------------------------------------------------------------------------------

Rect normalize(const Rect& rect)
{
    return Rect(rect).normalize();
}

Rect intersect(const Rect& a, const Rect& b)
{
    return Rect(a).intersect(b);
}

Rect& Rect::intersect(const Rect& rect)
{
    if (isNull() || rect.isNull())
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

Rect& Rect::unite(const Rect& rect)
{
    if (rect.isNull())
        return *this;;

    if (isNull())
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
    if (isNull()) {
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

    Point leftTop(xMin, yMin);
    Point bottomRight(xMax + 1, yMax + 1);

    return Rect(leftTop, bottomRight, false);
}

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

    Point leftTop(xMin, yMin);
    Point bottomRight(xMax + 1, yMax + 1);

    return Rect(leftTop, bottomRight, false);
}

//------------------------------------------------------------------------------

}
