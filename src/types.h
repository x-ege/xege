#pragma once

// MSVC 从 10.0（VS2010）开始有 stdint.h
// GCC 从 4.5 开始有 stdint.h
#if _MSC_VER >= 1600 || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#include <stdint.h>
#elif !defined(_MSC_VER) || _MSC_VER > 1300
#include "stdint.h"
#else
typedef unsigned uint32_t;
#endif

#include <limits.h>

#include <windows.h>

#if !defined(EGE_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && _MSC_VER >= 1300
#define EGE_W64 __w64
#else
#define EGE_W64
#endif
#endif

#ifndef __int3264
#if defined(_WIN64)
typedef __int64          LONG_PTR, *PLONG_PTR;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;

#define __int3264 __int64

#else
typedef EGE_W64 long          LONG_PTR, *PLONG_PTR;
typedef EGE_W64 unsigned long ULONG_PTR, *PULONG_PTR;

#define __int3264 __int32

#endif
#endif

typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;

typedef unsigned int uint32;

#if !defined(_MSC_VER) || _MSC_VER > 1200
typedef intptr_t POINTER_SIZE;
#else
typedef long POINTER_SIZE;
#endif

namespace ege
{

#ifndef EGE_BYTE_TYPEDEF
#define EGE_BYTE_TYPEDEF
typedef unsigned char byte;
#endif

//------------------------------------------------------------------------------
//                                    Point
//------------------------------------------------------------------------------

#ifndef EGE_Point_TYPEDEF
#define EGE_Point_TYPEDEF
struct Point
{
    int x;
    int y;

public:
    Point() : x(0), y(0) {}
    Point(int x, int y) : x(x), y(y) {}

    Point& set(int x, int y);
    Point& offset(int dx, int dy);
};

inline Point& Point::set(int x, int y)
{
    this->x = x;
    this->y = y;
    return *this;
}

inline Point& Point::offset(int dx, int dy)
{
    x += dx;
    y += dy;
    return *this;
}
#endif


//------------------------------------------------------------------------------
//                                    Size
//------------------------------------------------------------------------------

#ifndef EGE_Size_TYPEDEF
#define EGE_Size_TYPEDEF
struct Size
{
    int width;
    int height;

public:
    Size() : width(0), height(0) {}
    Size(int width, int height) : width(width), height(height) {}

    Size& set(int width, int height);
    Size& setEmpty();

    bool isNull()       const { return (width == 0) && (height == 0); }
    bool isEmpty()      const { return (width <= 0) || (height <= 0); }
    bool isValid()      const { return (width >  0) && (height >  0); }
    bool isNormalized() const { return (width >= 0) && (height >= 0); }

    Size& tranpose();
    Size& normalize();
};

bool operator== (const Size& a, const Size& b);
bool operator!= (const Size& a, const Size& b);

Size normalize(const Size& size);

//------------------------------------------------------------------------------

inline Size& Size::set(int width, int height)
{
    this->width  = width;
    this->height = height;
    return *this;
}

inline Size& Size::setEmpty()
{
    return set(0, 0);
}

inline bool operator== (const Size& a, const Size& b)
{
    return (a.width == b.width) && (a.height == b.height);
}

inline bool operator!= (const Size& a, const Size& b)
{
    return !(a == b);
}

inline Size& Size::tranpose()
{
    int temp = width;
    width = height;
    height = temp;
    return *this;
}

inline Size& Size::normalize()
{
    if (width  < 0) {
        width  = -width;
    }

    if (height < 0) {
        height = -height;
    }

    return *this;
}

inline Size normalize(const Size& size)
{
    return Size(size).tranpose();
}

#endif

//------------------------------------------------------------------------------
//                                    Rect
//------------------------------------------------------------------------------
#ifndef EGE_Rect_TYPEDEF
#define EGE_Rect_TYPEDEF
struct Rect
{
    int x;
    int y;
    int width;
    int height;

public:
    Rect();
    Rect(int x, int y, int width, int height);
    Rect(const Point& topLeft, const Size& size);
    Rect(const Point& topLeft, const Point& bottomRight, bool normalize = false);

    int left()   const;
    int top()    const;
    int right()  const;
    int bottom() const;

    Point topLeft() const;
    Point bottomRight() const;
    Point center()  const;
    Size  size()    const;

	Rect& setTopLeft(const Point& topLeft);
    Rect& setTopLeft(int x, int y);
    Rect& setBottomRight(const Point& bottomRight);
    Rect& setBottomRight(int x, int y);
    Rect& setLeftRight(int left, int right);
    Rect& setTopBottom(int top,  int bottom);
	Rect& setSize(const Size& size);
    Rect& setSize(int width, int height);
    Rect& setX(int x);
    Rect& setY(int y);
    Rect& setWidth(int width);
    Rect& setHeight(int height);
    Rect& setLeft(int left);
    Rect& setTop(int top);
    Rect& setRight(int right);
    Rect& setBottom(int bottom);
    Rect& set(int x, int y, int width, int height);
    Rect& set(const Point& topLeft, const Size& size);
    Rect& setBounds(const Point& topLeft, const Point& bottomRight, bool normalize = true);
    Rect& setBounds(int left, int top, int right, int bottom, bool normalize = true);
    Rect& setEmpty();

    Rect& normalize();
    Rect& transpose();
    Rect& offset  (int dx, int dy);
    Rect& offsetTo(int  x, int  y);
    Rect& inset   (int dx, int dy);
    Rect& outset  (int dx, int dy);

    Rect& intersect(const Rect& rect);
    Rect& unite(const Rect& rect);
    Rect& unite(int x, int y, int width, int height);
    Rect& unite(const Point& point);
    Rect& unite(const Point points[], int count);

    Rect& clip (int xMin, int xMax, int yMin, int yMax);
    Rect& clipX(int xMin, int xMax);
    Rect& clipY(int yMin, int yMax);
    Rect& clip ();
    Rect& clipX();
    Rect& clipY();

    bool isNull()       const;
    bool isEmpty()      const;
    bool isValid()      const;
    bool isNormalized() const;

    bool contains (int x, int y)       const;
    bool contains (const Point& point) const;
    bool contains (const Rect& rect)   const;
    bool isOverlap(const Rect& rect)   const;

    bool isXOutOfRange(int xMin, int xMax) const;
    bool isYOutOfRange(int yMin, int yMax) const;
    bool isXOutOfRange() const;
    bool isYOutOfRange() const;
    bool isOutOfRange (int xMin, int xMax, int yMin, int yMax) const;
    bool isOutOfRange() const;
}; // Rect

Rect normalize(const Rect& rect);

Rect intersect(const Rect& a, const Rect& b);

Rect unite(const Rect& a, const Rect& b);

Rect bounds(const Point point[], int count);

Rect bounds(const Point& a, const Point& b);

//------------------------------------------------------------------------------

inline Rect::Rect(): x(0), y(0), width(0), height(0) {}

inline Rect::Rect(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}

inline Rect::Rect(const Point& topLeft, const Size& size)
    : x(topLeft.x), y(topLeft.y), width(size.width), height(size.height)
{ }

inline Rect::Rect(const Point& topLeft, const Point& bottomRight, bool normalize)
    : x(topLeft.x), y(topLeft.y), width(bottomRight.x - topLeft.x), height(bottomRight.y - topLeft.y)
{
    if (normalize)
        this->normalize();
}

inline int Rect::left()   const { return x; }
inline int Rect::top()    const { return y; }
inline int Rect::right()  const { return x + width; }
inline int Rect::bottom() const { return y + height; }

inline Point Rect::topLeft()     const { return Point(x, y); }
inline Point Rect::bottomRight() const { return Point(x + width, y + height); }
inline Size  Rect::size()        const { return Size(width, height); }
inline Point Rect::center()      const { return Point(x + width / 2, y + height / 2);}

inline Rect& Rect::setX(int x)           { this->x = x; return *this;}
inline Rect& Rect::setY(int y)           { this->x = y; return *this; }
inline Rect& Rect::setWidth(int width)   { this->width  = width;  return *this;}
inline Rect& Rect::setHeight(int height) { this->height = height; return *this;}

inline Rect& Rect::setLeft(int left)
{
    int diff = left - this->left();
    width -= diff;
    x = left;

    return *this;
}

inline Rect& Rect::setTop(int top)
{
    int diff = top - this->top();
    height -= diff;
    y = top;
    return *this;
}

inline Rect& Rect::setRight(int right)
{
    width  = right - left();
    return *this;
}

inline Rect& Rect::setBottom(int bottom)
{
    height = bottom - top();
    return *this;
}

inline Rect& Rect::setSize(const Size& size)
{
    setSize(size.width, size.height);
    return *this;
}

inline Rect& Rect::setSize(int width, int height)
{
    this->width  = width;
    this->height = height;
    return *this;
}

inline Rect& Rect::setTopLeft(const Point& topLeft)
{
    setTopLeft(topLeft.x, topLeft.y);
    return *this;
}

inline Rect& Rect::setTopLeft(int x, int y)
{
    setBounds(Point(x, y), bottomRight(), false);
    return *this;
}

inline Rect& Rect::setBottomRight(const Point& bottomRight)
{
    return setBottomRight(bottomRight.x, bottomRight.y);
}

inline Rect& Rect::setBottomRight(int x, int y)
{
    setRight(x);
    setBottom(y);
    return *this;
}

inline Rect& Rect::setLeftRight(int left, int right)
{
    x = left;
    width = right - left;
    return *this;
}

inline Rect& Rect::setTopBottom(int top,  int bottom)
{
    y = top;
    height = bottom - top;
    return *this;
}

inline Rect& Rect::set(int x, int y, int width, int height)
{
    this->x = x;
    this->y = y;
    this->width  = width;
    this->height = height;

    return *this;
}

inline Rect& Rect::set(const Point& topLeft, const Size& size)
{
    setTopLeft(topLeft);
    setSize(size);
    return *this;
}

inline Rect& Rect::setBounds(const Point& topLeft, const Point& bottomRight, bool normalize)
{
    x = topLeft.x;
    y = topLeft.y;
    width  = bottomRight.x - topLeft.x;
    height = bottomRight.y - topLeft.y;

    if (normalize)
        this->normalize();

    return *this;
}

inline Rect& Rect::setBounds(int left, int top, int right, int bottom, bool normalize)
{
    x = left;
    y = top;
    setRight(right);
    setBottom(bottom);

    if (normalize)
        this->normalize();

    return *this;
}

inline Rect& Rect::setEmpty()
{
    x = y = width = height = 0;
    return *this;
}

inline Rect& Rect::normalize()
{
    if (width < 0) {
        x += width + 1;
        width = -width;
    }

    if (height < 0) {
        y += height + 1;
        height = -height;
    }

    return *this;
}

inline Rect& Rect::transpose()
{
    int temp = width;
    width = height;
    height = temp;
    return *this;
}

inline Rect& Rect::offset(int dx, int dy)
{
    x += dx;
    y += dy;
    return *this;
}

inline Rect& Rect::offsetTo(int x, int y)
{
    this->x = x;
    this->y = y;
    return *this;
}

inline Rect& Rect::inset(int dx, int dy)
{
    x += dx;
    y += dy;
    width += 2 * dx;
    height += 2 * dy;
    return *this;
}

inline Rect& Rect::outset(int dx, int dy)
{
    inset(-dx, -dy);
    return *this;
}

inline bool Rect::isNull()       const { return (width == 0) && (height == 0); }
inline bool Rect::isEmpty()      const { return (width == 0) || (height == 0); }
inline bool Rect::isValid()      const { return (width >  0) && (height >  0); }
inline bool Rect::isNormalized() const { return (width >= 0) && (height >= 0); }

inline bool Rect::isOutOfRange() const
{
    return isOutOfRange(INT_MIN, INT_MAX, INT_MIN, INT_MAX);
}

inline bool Rect::isXOutOfRange(int xMin, int xMax) const
{
    if ((xMin > xMax) || ((x < xMin) || (x > xMax)))
        return true;

    if (width > 0) {
        if ((unsigned)(xMax - x) < (unsigned)(width - 1))
            return true;
    } else if (width < 0) {
        if ((unsigned)(x - xMin) < (unsigned)(-width - 1))
            return true;
    }

    return false;
}

inline bool Rect::isXOutOfRange() const
{
    return isXOutOfRange(INT_MIN, INT_MAX);
}

inline bool Rect::isYOutOfRange() const
{
    return isYOutOfRange(INT_MIN, INT_MAX);
}

inline bool Rect::isYOutOfRange(int yMin, int yMax) const
{
    if (yMin > yMax)
        return true;

    if ((y < yMin) || (y > yMax))
        return true;

    if (height > 0) {
        if ((unsigned)(yMax - y) < (unsigned)(height - 1))
            return true;
    } else if (height < 0) {
        if ((unsigned)(y - yMin) < (unsigned)(-height - 1))
            return true;
    }

    return false;
}

inline bool Rect::isOutOfRange(int xMin, int xMax, int yMin, int yMax) const
{
    return isXOutOfRange(xMin, xMax) || isYOutOfRange(yMin, yMax);
}

inline bool Rect::contains(int x, int y) const
{
    return ((x >= left()) && (x < right()) && (y >= top() && y < bottom()));
}

inline bool Rect::contains(const Point& point) const
{
    return contains(point.x, point.y);
}

inline bool Rect::contains(const Rect& rect) const
{
    return ( left() <= rect.left())  && (   top() <= rect.top())
        && (right() >= rect.right()) && (bottom() >= rect.bottom());
}

inline bool Rect::isOverlap(const Rect& rect) const
{
    if (isEmpty() || rect.isEmpty())
        return false;

    if ((left() >= rect.right()) || (top() >= rect.bottom()) || (right() <= rect.left() || bottom() <= rect.top()))
        return false;

    return true;
}

inline bool operator== (const Rect& a, const Rect& b)
{
    return (a.x == b.x) && (a.y == b.y) && (a.width == b.width) && (a.height == b.height);
}

inline bool operator!= (const Rect& a, const Rect& b)
{
    return !(a == b);
}
#endif
//------------------------------------------------------------------------------


}
