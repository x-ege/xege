/*
* EGE (Easy Graphics Engine)
* filename  egegapi.cpp

本文件汇集较独立的函数接口
*/

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifndef _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_NON_CONFORMING_SWPRINTFS
#endif

#include "ege_head.h"
#include "ege_common.h"
#include "ege_extension.h"
#include "gdi_conv.h"

#if defined(EGE_BACKEND_COREGRAPHICS)
#include "backend/macos/MacWindow.h"
#endif

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <vector>


namespace ege
{

bool is_run()
{
    struct _graph_setting* pg = &graph_setting;
    if (pg->exit_window || pg->exit_flag) {
        return false;
    }
    return true;
}

bool isinitialized()
{
    return graph_setting.init_sem.acquirable();
}

int showmouse(int bShow)
{
    struct _graph_setting* pg = &graph_setting;
    int ret = pg->mouse_show;
    pg->mouse_show = bShow;
    if (pg->window) {
        pg->window->setCursorVisible(bShow != 0);
    }
    return ret;
}

int mousepos(int* x, int* y)
{
    struct _graph_setting* pg = &graph_setting;
    *x = pg->mouse_pos.x;
    *y = pg->mouse_pos.y;
    return 0;
}

void setwritemode(int mode, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    img->m_writeMode = mode;
    if (img->m_renderTarget) {
        img->m_renderTarget->setRasterOp((RasterOp)mode);
    } else {
#ifdef _WIN32
        SetROP2(img->m_hDC, mode);
#endif
    }
    CONVERT_IMAGE_END;
}

static inline bool in_rect(int x, int y, int w, int h)
{
    return !((x < 0) || (y < 0) || (x >= w) || (y >= h));
}

color_t getpixel(int x, int y, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    CONVERT_IMAGE_END;

    // RenderTarget coordinates are viewport-relative.  Let the backend map
    // them to physical image coordinates exactly once.
    if (img->m_renderTarget) {
        return img->m_renderTarget->getPixel(x, y);
    }

    x += img->m_vpt.left;
    y += img->m_vpt.top;

    if (img->m_pBuffer != NULL && in_rect(x, y, img->m_width, img->m_height)) {
        return img->m_pBuffer[y * img->m_width + x];
    }

    // else
    return 0;
}

void putpixel(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    x += img->m_vpt.left;
    y += img->m_vpt.top;
    if (in_rect(x, y, img->m_vpt.right, img->m_vpt.bottom)) {
        color_t* const buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            buffer[y * img->m_width + x] = color;
        }
    }
    CONVERT_IMAGE_END;
}

void putpixels(int numOfPoints, const int* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img == NULL || points == NULL || numOfPoints <= 0) {
        CONVERT_IMAGE_END;
        return;
    }
    int x, y, c;
    int w = img->m_vpt.right - img->m_vpt.left, h = img->m_vpt.bottom - img->m_vpt.top;
    int tw = img->m_width;
    int dirtyLeft = w, dirtyTop = h, dirtyRight = 0, dirtyBottom = 0;
    const int* scan = points;
    for (int n = 0; n < numOfPoints; ++n, scan += 3) {
        x = scan[0], y = scan[1];
        if (in_rect(x, y, w, h)) {
            dirtyLeft = std::min(dirtyLeft, x);
            dirtyTop = std::min(dirtyTop, y);
            dirtyRight = std::max(dirtyRight, x + 1);
            dirtyBottom = std::max(dirtyBottom, y + 1);
        }
    }
    if (dirtyLeft >= dirtyRight || dirtyTop >= dirtyBottom) {
        CONVERT_IMAGE_END;
        return;
    }
    color_t* imageBuffer = img->getbuffer_for_write(
        img->m_vpt.left + dirtyLeft, img->m_vpt.top + dirtyTop,
        dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
    if (imageBuffer == NULL) {
        CONVERT_IMAGE_END;
        return;
    }
    PDWORD pb = reinterpret_cast<PDWORD>(imageBuffer) +
        img->m_vpt.top * img->m_width + img->m_vpt.left;
    for (int n = 0; n < numOfPoints; ++n, points += 3) {
        x = points[0], y = points[1], c = points[2];
        if (in_rect(x, y, w, h)) {
            pb[y * tw + x] = c;
        }
    }
    CONVERT_IMAGE_END;
}

void putpixels_f(int numOfPoints, const int* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img == NULL || points == NULL || numOfPoints <= 0) {
        CONVERT_IMAGE_END;
        return;
    }
    int x, y, c;
    int tw = img->m_width;
    int th = img->m_height;
    int dirtyLeft = tw, dirtyTop = th, dirtyRight = 0, dirtyBottom = 0;
    const int* scan = points;
    for (int n = 0; n < numOfPoints; ++n, scan += 3) {
        x = scan[0], y = scan[1];
        if (in_rect(x, y, tw, th)) {
            dirtyLeft = std::min(dirtyLeft, x);
            dirtyTop = std::min(dirtyTop, y);
            dirtyRight = std::max(dirtyRight, x + 1);
            dirtyBottom = std::max(dirtyBottom, y + 1);
        }
    }
    if (dirtyLeft >= dirtyRight || dirtyTop >= dirtyBottom) {
        CONVERT_IMAGE_END;
        return;
    }
    color_t* imageBuffer = img->getbuffer_for_write(
        dirtyLeft, dirtyTop, dirtyRight - dirtyLeft, dirtyBottom - dirtyTop);
    if (imageBuffer == NULL) {
        CONVERT_IMAGE_END;
        return;
    }
    for (int n = 0; n < numOfPoints; ++n, points += 3) {
        x = points[0], y = points[1], c = points[2];
        if (in_rect(x, y, tw, th)) {
            imageBuffer[y * tw + x] = c;
        }
    }
    CONVERT_IMAGE_END;
}

color_t getpixel_f(int x, int y, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_F_CONST(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        // Keep this physical-coordinate fast path read-only while preserving
        // the legacy getbuffer() synchronization of pending GDI/GDI+ drawing.
        const color_t* buffer = img->getbuffer();
        return buffer != NULL ? buffer[y * img->m_width + x] : 0;
    }
    return 0;
}

void putpixel_f(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_F(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            buffer[y * img->m_width + x] = color;
        }
    }
}

void putpixel_withalpha(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    x += img->m_vpt.left;
    y += img->m_vpt.top;
    if (in_rect(x, y, img->m_vpt.right, img->m_vpt.bottom)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = colorblend_inline(dst_color, color, EGEGET_A(color));
        }
    }
    CONVERT_IMAGE_END;
}

void putpixel_withalpha_f(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_F(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = colorblend_inline_fast(dst_color, color, EGEGET_A(color));
        }
    }
    CONVERT_IMAGE_END;
}

void putpixel_savealpha(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    x += img->m_vpt.left;
    y += img->m_vpt.top;
    if (in_rect(x, y, img->m_vpt.right, img->m_vpt.bottom)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = EGECOLORA(color, EGEGET_A(dst_color));
        }
    }
    CONVERT_IMAGE_END;
}

void putpixel_savealpha_f(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_F(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = EGECOLORA(color, EGEGET_A(dst_color));
        }
    }
    CONVERT_IMAGE_END;
}

void putpixel_alphablend(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    x += img->m_vpt.left;
    y += img->m_vpt.top;
    if (in_rect(x, y, img->m_vpt.right, img->m_vpt.bottom)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = alphablend_inline(dst_color, color);
        }
    }
    CONVERT_IMAGE_END;
}

void putpixel_alphablend_f(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = alphablend_inline(dst_color, color);
        }
    }
    CONVERT_IMAGE_END;
}

void putpixel_alphablend(int x, int y, color_t color, unsigned char alphaFactor, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    x += img->m_vpt.left;
    y += img->m_vpt.top;
    if (in_rect(x, y, img->m_vpt.right, img->m_vpt.bottom)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = alphablend_inline(dst_color, color, alphaFactor);
        }
    }
    CONVERT_IMAGE_END;
}

void putpixel_alphablend_f(int x, int y, color_t color, unsigned char alphaFactor, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        color_t* buffer = img->getbuffer_for_write(x, y, 1, 1);
        if (buffer != NULL) {
            color_t& dst_color = buffer[y * img->m_width + x];
            dst_color = alphablend_inline(dst_color, color, alphaFactor);
        }
    }
    CONVERT_IMAGE_END;
}

void moveto(int x, int y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img->m_renderTarget) {
        img->m_renderTarget->moveTo(x, y);
    } else {
#ifdef _WIN32
        MoveToEx(img->m_hDC, x, y, NULL);
#endif
    }
    CONVERT_IMAGE_END;
}

void moverel(int dx, int dy, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img->m_renderTarget) {
        img->m_renderTarget->moveRel(dx, dy);
    } else {
#ifdef _WIN32
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        dx += pt.x;
        dy += pt.y;
        MoveToEx(img->m_hDC, dx, dy, NULL);
#endif
    }
    CONVERT_IMAGE_END;
}

void line(int x1, int y1, int x2, int y2, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle != NULL_LINE) {
            if (img->m_renderTarget) {
                img->m_renderTarget->drawLine(x1, y1, x2, y2);
            } else {
#ifdef _WIN32
                MoveToEx(img->m_hDC, x1, y1, NULL);
                LineTo(img->m_hDC, x2, y2);
                MoveToEx(img->m_hDC, x1, y1, NULL);
#endif
            }
        }
    }
    CONVERT_IMAGE_END;
}

void linerel(int dx, int dy, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->lineRel(dx, dy);
        } else {
#ifdef _WIN32
            POINT pt;
            GetCurrentPositionEx(img->m_hDC, &pt);
            dx += pt.x;
            dy += pt.y;
            if (img->m_linestyle.linestyle != NULL_LINE) {
                LineTo(img->m_hDC, dx, dy);
            } else {
                MoveToEx(img->m_hDC, dx, dy, NULL);
            }
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void lineto(int x, int y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->lineTo(x, y);
        } else {
#ifdef _WIN32
            if (img->m_linestyle.linestyle != NULL_LINE) {
                LineTo(img->m_hDC, x, y);
            } else {
                MoveToEx(img->m_hDC, x, y, NULL);
            }
#endif
        }
    }
    CONVERT_IMAGE_END;
}

/* private function */
static void line_base(float x1, float y1, float x2, float y2, PIMAGE img)
{
    int bswap = 2;
    color_t col = getcolor(img);
    color_t endp = 0;
    color_t* pBuffer = (color_t*)img->m_pBuffer;
    int rw = img->m_width;
    if (x1 > x2) {
        float ft;
        SWAP(x1, x2, ft);
        SWAP(y1, y2, ft);
        if (bswap) {
            bswap ^= 3;
        }
    }
    if (x2 < img->m_vpt.left) {
        return;
    }
    if (x1 > img->m_vpt.right) {
        return;
    }
    if (x1 < img->m_vpt.left) {
        if (x2 - x1 < FLOAT_EPS) {
            return;
        }
        float d = (x2 - img->m_vpt.left) / (x2 - x1);
        y1 = (y1 - y2) * d + y2;
        x1 = (float)img->m_vpt.left;
        if (bswap == 1) {
            bswap = 0;
        }
    }
    if (x2 > img->m_vpt.right) {
        if (x2 - x1 < FLOAT_EPS) {
            return;
        }
        float d = (img->m_vpt.right - x1) / (x2 - x1);
        y2 = (y2 - y1) * d + y1;
        x2 = (float)img->m_vpt.right;
        if (bswap == 2) {
            bswap = 0;
        }
    }
    if (y1 > y2) {
        float ft;
        SWAP(x1, x2, ft);
        SWAP(y1, y2, ft);
        if (bswap) {
            bswap ^= 3;
        }
    }
    if (y2 < img->m_vpt.top) {
        return;
    }
    if (y1 > img->m_vpt.bottom) {
        return;
    }
    if (y1 < img->m_vpt.top) {
        if (y2 - y1 < FLOAT_EPS) {
            return;
        }
        float d = (y2 - img->m_vpt.top) / (y2 - y1);
        x1 = (x1 - x2) * d + x2;
        y1 = (float)img->m_vpt.top;
        if (bswap == 1) {
            bswap = 0;
        }
    }
    if (y2 > img->m_vpt.bottom) {
        if (y2 - y1 < FLOAT_EPS) {
            return;
        }
        float d = (img->m_vpt.bottom - y1) / (y2 - y1);
        x2 = (x2 - x1) * d + x1;
        y2 = (float)img->m_vpt.bottom;
        if (bswap == 2) {
            bswap = 0;
        }
    }
    if (bswap) {
        if (bswap == 1) {
            endp = pBuffer[(int)y1 * rw + (int)x1];
        } else {
            endp = pBuffer[(int)y2 * rw + (int)x2];
        }
    }
    if (y2 - y1 > fabs(x2 - x1)) {
        int y = (int)(y1 + 0.9f);
        int ye = (int)(y2);
        float x, dx;
        if (y < y1) {
            ++y;
        }
        dx = (x2 - x1) / (y2 - y1);
        x = (y - y1) * dx + x1 + 0.5f;
        if (ye >= img->m_vpt.bottom) {
            ye = img->m_vpt.bottom - 1;
        }
        if (ye < y2) {
            bswap = 0;
        }
        for (; y <= ye; ++y, x += dx) {
            pBuffer[y * rw + (int)x] = col;
        }
    } else {
        if (x1 > x2) {
            float ft;
            SWAP(x1, x2, ft);
            SWAP(y1, y2, ft);
            if (bswap) {
                bswap ^= 3;
            }
        }
        int x = (int)(x1 + 0.9f);
        int xe = (int)(x2);
        float y, dy;
        if (x < x1) {
            ++x;
        }
        dy = (y2 - y1) / (x2 - x1);
        y = (x - x1) * dy + y1 + 0.5f;
        if (xe >= img->m_vpt.right) {
            xe = img->m_vpt.right - 1;
        }
        if (xe < x2) {
            bswap = 0;
        }
        for (; x <= xe; ++x, y += dy) {
            pBuffer[(int)y * rw + x] = col;
        }
    }
    if (bswap) {
        if (bswap == 1) {
            pBuffer[(int)y1 * rw + (int)x1] = endp;
        } else {
            pBuffer[(int)y2 * rw + (int)x2] = endp;
        }
    }
}

void lineto_f(float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->lineTo((int)round(x), (int)round(y));
        } else {
#ifdef _WIN32
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        line_base((float)pt.x, (float)pt.y, x, y, img);
        MoveToEx(img->m_hDC, (int)round(x), (int)round(y), NULL);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void linerel_f(float dx, float dy, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->lineRel((int)round(dx), (int)round(dy));
        } else {
#ifdef _WIN32
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        float endX = (float)pt.x + dx, endY = (float)pt.y + dy;
        line_base((float)pt.x, (float)pt.y, endX, endY, img);
        MoveToEx(img->m_hDC, (int)round(endX), (int)round(endY), NULL);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void line_f(float x1, float y1, float x2, float y2, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->drawLineF(x1, y1, x2, y2);
        } else {
            line_base(x1, y1, x2, y2, img);
        }
    }
    CONVERT_IMAGE_END;
}

/*private function*/
static int saveBrush(PIMAGE img, int save) // 此函数调用前，已经有Lock
{
    struct _graph_setting* pg = &graph_setting;
#ifdef _WIN32
    if (save) {
        LOGBRUSH lbr = {0};

        lbr.lbColor = 0;
        lbr.lbStyle = BS_NULL;
        pg->savebrush_hbr = CreateBrushIndirect(&lbr);
        if (pg->savebrush_hbr) {
            pg->savebrush_hbr = (HBRUSH)SelectObject(img->m_hDC, pg->savebrush_hbr);
            return 1;
        }
    } else {
        if (pg->savebrush_hbr) {
            pg->savebrush_hbr = (HBRUSH)SelectObject(img->m_hDC, pg->savebrush_hbr);
            DeleteObject(pg->savebrush_hbr);
            pg->savebrush_hbr = NULL;
        }
    }
#endif
    return 0;
}

void rectangle(int left, int top, int right, int bottom, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img->m_renderTarget) {
        img->m_renderTarget->drawRect(left, top, right - left, bottom - top);
    } else {
#ifdef _WIN32
        if (saveBrush(img, 1)) {
            Rectangle(img->m_hDC, left, top, right, bottom);
            saveBrush(img, 0);
        }
#endif
    }
    CONVERT_IMAGE_END;
}

color_t getcolor(PCIMAGE pimg)
{
    return getlinecolor(pimg);
}

color_t getlinecolor(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        CONVERT_IMAGE_END;
        return img->m_linecolor;
    }
    CONVERT_IMAGE_END;
    return IMAGE::initial_line_color;
}

// 将描述线形的位模式转换为 style 数组
// 用于 ExtCreatePen 中。
// 返回值为用到的数组元素数。
static int upattern2array(unsigned short pattern, DWORD style[])
{
    int n, segments = 0, segmentLength = 1;
    int state = !!(pattern & 1);
    for (n = 1; n < 16; n++) {
        int currentBit = !!(pattern & (1 << n));
        if (state == currentBit) {
            segmentLength += 1;
        } else {
            state = currentBit;
            style[segments] = segmentLength;
            segments += 1;
            segmentLength = 1;
        }
    }
    style[segments] = segmentLength;
    segments += 1;

    // 若 pattern 以 0 开头且为偶数段
    if (!(pattern & 1) && segments % 2 == 0) {
        DWORD p0 = style[0];
        for (int i = 0; i < segments - 1; ++i) {
            style[i] = style[i + 1];
        }
        style[segments - 1] = p0;
    }

    return segments;
}

static void update_pen(PIMAGE img)
{
#ifdef _WIN32
    if (img->m_hDC) {
        const int linestyle = img->m_linestyle.linestyle;
        const unsigned short pattern = img->m_linestyle.upattern;
        const int thickness = img->m_linestyle.thickness;

        HPEN hpen;

        if ((thickness == 1) && ((linestyle == SOLID_LINE) || (linestyle == NULL_LINE))) {
            LOGPEN logPen;
            logPen.lopnStyle = linestyle; // Other styles may be drawn incorrectly
            logPen.lopnWidth.x = 1;       // Width
            logPen.lopnWidth.y = 1;       // Unuse
            logPen.lopnColor = ARGBTOZBGR(img->m_linecolor);

            hpen = CreatePenIndirect(&logPen);
        } else {
            unsigned int penStyle = linestyle;

            penStyle |= PS_GEOMETRIC;

            switch (img->m_linestartcap) {
                case LINECAP_FLAT :  penStyle |= PS_ENDCAP_FLAT;   break;
                case LINECAP_ROUND:  penStyle |= PS_ENDCAP_ROUND;  break;
                case LINECAP_SQUARE: penStyle |= PS_ENDCAP_SQUARE; break;
                default:             penStyle |= PS_ENDCAP_FLAT;   break;
            }

            switch(img->m_linejoin) {
                case LINEJOIN_MITER: penStyle |= PS_JOIN_MITER;    break;
                case LINEJOIN_BEVEL: penStyle |= PS_JOIN_BEVEL;    break;
                case LINEJOIN_ROUND: penStyle |= PS_JOIN_ROUND;    break;
                default:             penStyle |= PS_JOIN_MITER;    break;
            }

            LOGBRUSH lbr;
            lbr.lbColor = ARGBTOZBGR(img->m_linecolor);
            lbr.lbStyle = BS_SOLID;
            lbr.lbHatch = 0;

            if (linestyle == USERBIT_LINE) {
                DWORD style[20] = {0};
                int bn = upattern2array(pattern, style);
                hpen = ExtCreatePen(penStyle, thickness, &lbr, bn, style);
            } else {
                hpen = ExtCreatePen(penStyle, thickness, &lbr, 0, NULL);
            }
        }

        if (hpen) {
            DeleteObject(SelectObject(img->m_hDC, hpen));
        }

        SetMiterLimit(img->m_hDC, img->m_linejoinmiterlimit, NULL);
    }
#endif

    // why update pen not in IMAGE???
#ifdef EGE_GDIPLUS
    Gdiplus::Pen* pen = img->getPen();
    pen->SetColor(img->m_linecolor);
    pen->SetWidth(img->m_linewidth);
    pen->SetDashStyle(linestyle_to_dashstyle(img->m_linestyle.linestyle));

    pen->SetStartCap(convertToGdiplusLineCap(img->m_linestartcap));
    pen->SetEndCap(convertToGdiplusLineCap(img->m_lineendcap));
    pen->SetLineJoin(convertToGdiplusLineJoin(img->m_linejoin));
    pen->SetMiterLimit(img->m_linejoinmiterlimit);
#endif
}

void setcolor(color_t color, PIMAGE pimg)
{
    setlinecolor(color, pimg);
    settextcolor(color, pimg);
}

void setlinecolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        img->m_linecolor = color;
        if (img->m_renderTarget) {
            img->m_renderTarget->setLineColor(color);
        }
        // RenderTarget primitives and enhanced GDI+ routes share IMAGE state.
        update_pen(img);
    }
    CONVERT_IMAGE_END
}

void setfillcolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    img->m_fillcolor = color;
    img->m_fillstyle = SOLID_FILL;
    if (img->m_renderTarget) {
        // The Win32 backend replaces the current brush with a solid brush.
        // Preserve that observable behavior for the portable renderer too.
        img->m_renderTarget->setFillStyle(FILL_SOLID, color);
    } else {
#ifdef _WIN32
        HBRUSH hbr = CreateSolidBrush(ARGBTOZBGR(color));
        if (hbr) {
            DeleteObject(SelectObject(img->m_hDC, hbr));
        }
#endif
    }
#ifdef EGE_GDIPLUS
    img->set_pattern(NULL);
#else
    clearNativeFallbackPattern(img);
#endif
    CONVERT_IMAGE_END;
}

color_t getfillcolor(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        CONVERT_IMAGE_END;
        return img->m_fillcolor;
    }
    CONVERT_IMAGE_END;
    return IMAGE::initial_fill_color;
}

color_t getbkcolor(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        return img->m_bk_color;
    } else {
        _graph_setting* pg = &graph_setting;
        if (!pg->init_sem.acquirable()) {
            return pg->window_initial_color;
        }
    }

    CONVERT_IMAGE_END;

    return IMAGE::initial_bk_color;
}

color_t gettextcolor(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        return img->m_textcolor;
    }
    CONVERT_IMAGE_END;
    return IMAGE::initial_text_color;
}

void EGEAPI setbkcolor(color_t color, PIMAGE pimg)
{
    color_t oldBkColor = getbkcolor(pimg);
    setbkcolor_f(color, pimg);
    replacePixels(pimg, oldBkColor, color);
}

void EGEAPI setbkcolor_f(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        img->m_bk_color = color;
        img->m_fontBkColor = color;
        if (img->m_hDC) {
#ifdef _WIN32
            SetBkColor(img->m_hDC, ARGBTOZBGR(color));
#endif
        }
        if (img->m_renderTarget) {
            img->m_renderTarget->setBkColor(color);
        }
    } else {
        _graph_setting* pg = &graph_setting;
        if (!pg->init_sem.acquirable()) {
            pg->window_initial_color = color;
        }
    }

    CONVERT_IMAGE_END;
}

void settextcolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        img->m_textcolor = color;
#ifdef _WIN32
        if (img->m_hDC) {
            SetTextColor(img->m_hDC, ARGBTOZBGR(color));
        }
#endif
        if (img->m_renderTarget) {
            img->m_renderTarget->setTextColor(color);
        }
    }
    CONVERT_IMAGE_END;
}

void setfontbkcolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        img->m_fontBkColor = color;
    }
    if (img && img->m_hDC) {
#ifdef _WIN32
        SetBkColor(img->m_hDC, ARGBTOZBGR(color));
#endif
    }
    if (img && img->m_renderTarget) {
        img->m_renderTarget->setBkColor(color);
    }
    CONVERT_IMAGE_END;
}

void setbkmode(int bkMode, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        img->m_bkMode = bkMode;
    }
    if (img && img->m_hDC) {
#ifdef _WIN32
        SetBkMode(img->m_hDC, bkMode);
#endif
    }
    if (img && img->m_renderTarget) {
        img->m_renderTarget->setBkMode(bkMode != TRANSPARENT);
    }
    CONVERT_IMAGE_END;
}

PIMAGE gettarget()
{
    struct _graph_setting* pg = &graph_setting;
    return pg->imgtarget_set;
}

int settarget(PIMAGE pbuf)
{
    struct _graph_setting* pg = &graph_setting;
    pg->imgtarget_set = pbuf;
    if (pbuf == NULL) {
        pg->imgtarget = pg->img_page[graph_setting.active_page];
    } else {
        pg->imgtarget = pbuf;
    }
    return 0;
}

void cleardevice(PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        color_t c = getbkcolor(img);
        if (img->m_renderTarget) {
            img->m_renderTarget->clear(c);
        } else if (img->m_hDC) {
            for (color_t *p = (color_t*)img->getbuffer(), *e = (color_t*)&img->getbuffer()[img->m_width * img->m_height];
                 p != e;
                 ++p)
            {
                *p = c;
            }
        }
    }
    CONVERT_IMAGE_END;
}

void arc(int x, int y, int startAngle, int endAngle, int radius, PIMAGE pimg)
{
    ellipse(x, y, startAngle, endAngle, radius, radius, pimg);
}

void arcf(float x, float y, float startAngle, float endAngle, float radius, PIMAGE pimg)
{
    ellipsef(x, y, startAngle, endAngle, radius, radius, pimg);
}

void circle(int x, int y, int radius, PIMAGE pimg) { ellipse(x, y, 0, 360, radius, radius, pimg); }

void circlef(float x, float y, float radius, PIMAGE pimg) { ellipsef(x, y, 0.0f, 360.0f, radius, radius, pimg); }

void ellipse(int x, int y, int startAngle, int endAngle, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    double sr = startAngle / 180.0 * PI, er = endAngle / 180.0 * PI;

    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->drawEllipse(x - xRadius, y - yRadius, startAngle, endAngle, 2 * xRadius, 2 * yRadius);
        } else {
#ifdef _WIN32
            Arc(img->m_hDC,
                x - xRadius,
                y - yRadius,
                x + xRadius,
                y + yRadius,
                (int)(x + xRadius * cos(sr)),
                (int)(y - yRadius * sin(sr)),
                (int)(x + xRadius * cos(er)),
                (int)(y - yRadius * sin(er)));
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void ellipsef(float x, float y, float startAngle, float endAngle, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    double sr = startAngle / 180.0 * PI, er = endAngle / 180.0 * PI;

    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->drawEllipse((int)round(x - xRadius), (int)round(y - yRadius),
                                              (int)round(startAngle), (int)round(endAngle),
                                              (int)round(2 * xRadius), (int)round(2 * yRadius));
        } else {
#ifdef _WIN32
        Arc(img->m_hDC,
            (int)(x - xRadius),
            (int)(y - yRadius),
            (int)(x + xRadius),
            (int)(y + yRadius),
            (int)(x + xRadius * cos(sr)),
            (int)(y - yRadius * sin(sr)),
            (int)(x + xRadius * cos(er)),
            (int)(y - yRadius * sin(er)));
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void pieslice(int x, int y, int startAngle, int endAngle, int radius, PIMAGE pimg)
{
    sector(x, y, startAngle, endAngle, radius, radius, pimg);
}

void pieslicef(float x, float y, float startAngle, float endAngle, float radius, PIMAGE pimg)
{
    sectorf(x, y, startAngle, endAngle, radius, radius, pimg);
}

void sector(int x, int y, int startAngle, int endAngle, int xRadius, int yRadius, PIMAGE pimg)
{
    fillpie(x, y, startAngle, endAngle, xRadius, yRadius, pimg);
}

void sectorf(float x, float y, float startAngle, float endAngle, float xRadius, float yRadius, PIMAGE pimg)
{
    fillpief(x, y, startAngle, endAngle, xRadius, yRadius, pimg);
}

void pie(int x, int y, int startAngle, int endAngle, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->drawPie(x - xRadius, y - yRadius, startAngle, endAngle,
                                     2 * xRadius, 2 * yRadius);
    } else {
#ifdef _WIN32
    HBRUSH oldBrush = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_BRUSH));
    fillpie(x, y, startAngle, endAngle, xRadius, yRadius, pimg);
    SelectObject(img->m_hDC, oldBrush);
#endif
    }
    CONVERT_IMAGE_END
}

void pief(float x, float y, float startAngle, float endAngle, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->drawPie((int)round(x - xRadius), (int)round(y - yRadius),
                                     (int)round(startAngle), (int)round(endAngle),
                                     (int)round(2 * xRadius), (int)round(2 * yRadius));
    } else {
#ifdef _WIN32
    HBRUSH oldBrush = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_BRUSH));
    fillpief(x, y, startAngle, endAngle, xRadius, yRadius, pimg);
    SelectObject(img->m_hDC, oldBrush);
#endif
    }
    CONVERT_IMAGE_END
}

void fillpie(int x, int y, int startAngle, int endAngle, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    double sr = startAngle / 180.0 * PI, er = endAngle / 180.0 * PI;
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->fillPie(x - xRadius, y - yRadius, startAngle, endAngle, 2 * xRadius, 2 * yRadius);
            img->m_renderTarget->drawPie(x - xRadius, y - yRadius, startAngle, endAngle,
                                         2 * xRadius, 2 * yRadius);
        } else {
#ifdef _WIN32
            Pie(img->m_hDC,
                x - xRadius,
                y - yRadius,
                x + xRadius,
                y + yRadius,
                (int)round(x + xRadius * cos(sr)),
                (int)round(y - yRadius * sin(sr)),
                (int)round(x + xRadius * cos(er)),
                (int)round(y - yRadius * sin(er)));
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void fillpief(float x, float y, float startAngle, float endAngle, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    double sr = startAngle / 180.0 * PI, er = endAngle / 180.0 * PI;
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->fillPie((int)round(x - xRadius), (int)round(y - yRadius),
                                         (int)round(startAngle), (int)round(endAngle),
                                         (int)round(2 * xRadius), (int)round(2 * yRadius));
            img->m_renderTarget->drawPie((int)round(x - xRadius), (int)round(y - yRadius),
                                         (int)round(startAngle), (int)round(endAngle),
                                         (int)round(2 * xRadius), (int)round(2 * yRadius));
        } else {
#ifdef _WIN32
        Pie(img->m_hDC,
            (int)(x - xRadius),
            (int)(y - yRadius),
            (int)(x + xRadius),
            (int)(y + yRadius),
            (int)round(x + xRadius * cos(sr)),
            (int)round(y - yRadius * sin(sr)),
            (int)round(x + xRadius * cos(er)),
            (int)round(y - yRadius * sin(er)));
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void solidpie(int x, int y, int startAngle, int endAngle, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillPie(x - xRadius, y - yRadius, startAngle, endAngle,
                                     2 * xRadius, 2 * yRadius);
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillpie(x, y, startAngle, endAngle, xRadius, yRadius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void solidpief(float x, float y, float startAngle, float endAngle, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillPie((int)round(x - xRadius), (int)round(y - yRadius),
                                     (int)round(startAngle), (int)round(endAngle),
                                     (int)round(2 * xRadius), (int)round(2 * yRadius));
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillpief(x, y, startAngle, endAngle, xRadius, yRadius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void fillellipse(int x, int y, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->fillEllipse(x - xRadius, y - yRadius, 0, 360, 2 * xRadius, 2 * yRadius);
            img->m_renderTarget->drawEllipse(x - xRadius, y - yRadius, 0, 360,
                                             2 * xRadius, 2 * yRadius);
        } else {
#ifdef _WIN32
            Ellipse(img->m_hDC, x - xRadius, y - yRadius, x + xRadius, y + yRadius);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void fillellipsef(float x, float y, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->fillEllipse((int)round(x - xRadius), (int)round(y - yRadius),
                                              0, 360, (int)round(2 * xRadius), (int)round(2 * yRadius));
            img->m_renderTarget->drawEllipse((int)round(x - xRadius), (int)round(y - yRadius),
                                              0, 360, (int)round(2 * xRadius), (int)round(2 * yRadius));
        } else {
#ifdef _WIN32
        Ellipse(img->m_hDC, (int)(x - xRadius), (int)(y - yRadius), (int)(x + xRadius), (int)(y + yRadius));
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void solidellipse(int x, int y, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillEllipse(x - xRadius, y - yRadius, 0, 360,
                                         2 * xRadius, 2 * yRadius);
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillellipse(x, y, xRadius, yRadius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void solidellipsef(float x, float y, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillEllipse((int)round(x - xRadius), (int)round(y - yRadius), 0, 360,
                                         (int)round(2 * xRadius), (int)round(2 * yRadius));
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillellipsef(x, y, xRadius, yRadius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void fillcircle(int x, int y, int radius, PIMAGE pimg)
{
    fillellipse(x, y, radius, radius, pimg);
}

void fillcirclef(float x, float y, float radius, PIMAGE pimg)
{
    fillellipsef(x,y,radius,radius,pimg);
}

void solidcircle(int x, int y, int radius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillCircle(x, y, radius);
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillcircle(x, y, radius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void solidcirclef(float x, float y, float radius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillCircle((int)std::lround(x), (int)std::lround(y),
                                        (int)std::lround(radius));
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillcirclef(x, y, radius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void bar(int left, int top, int right, int bottom, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img->m_renderTarget) {
        img->m_renderTarget->fillRect(left, top, right - left, bottom - top);
    } else {
#ifdef _WIN32
        RECT rect = {left, top, right, bottom};
        HBRUSH hbr_last = (HBRUSH)GetCurrentObject(img->m_hDC, OBJ_BRUSH); //(HBRUSH)SelectObject(pg->g_hdc, hbr);

        if (img) {
            FillRect(img->m_hDC, &rect, hbr_last);
        }
#endif
    }
    CONVERT_IMAGE_END;
}

void roundrect(int left, int top, int right, int bottom, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->drawRoundRect(left, top, right - left, bottom - top, xRadius * 2, yRadius * 2);
        } else {
#ifdef _WIN32
            HBRUSH oldBrush = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_BRUSH));
            RoundRect(img->m_hDC, left, top, right, bottom, xRadius * 2 , yRadius * 2);
            SelectObject(img->m_hDC, oldBrush);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void roundrect(int left, int top, int right, int bottom, int radius,  PIMAGE pimg)
{
    roundrect(left, top, right, bottom, radius, radius, pimg);
}

void fillroundrect(int left, int top, int right, int bottom, int radius,  PIMAGE pimg)
{
    fillroundrect(left, top, right, bottom, radius, radius, pimg);
}

void solidroundrect(int left, int top, int right, int bottom, int radius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillRoundRect(left, top, right - left, bottom - top,
                                           radius * 2, radius * 2);
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillroundrect(left, top, right, bottom, radius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void fillroundrect(int left, int top, int right, int bottom, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->fillRoundRect(left, top, right - left, bottom - top, xRadius * 2, yRadius * 2);
            img->m_renderTarget->drawRoundRect(left, top, right - left, bottom - top,
                                               xRadius * 2, yRadius * 2);
        } else {
#ifdef _WIN32
            RoundRect(img->m_hDC, left, top, right, bottom, xRadius * 2, yRadius * 2);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void solidroundrect(int left, int top, int right, int bottom, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillRoundRect(left, top, right - left, bottom - top,
                                           xRadius * 2, yRadius * 2);
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillroundrect(left, top, right, bottom, xRadius, yRadius, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void fillrect(int left, int top, int right, int bottom, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img->m_renderTarget) {
        img->m_renderTarget->fillRect(left, top, right - left, bottom - top);
        img->m_renderTarget->drawRect(left, top, right - left, bottom - top);
    } else {
#ifdef _WIN32
        Rectangle(img->m_hDC, left, top, right, bottom);
#endif
    }
    CONVERT_IMAGE_END;
}

void solidrect(int left, int top, int right, int bottom, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillRect(left, top, right - left, bottom - top);
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillrect(left, top, right, bottom, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void bar3d(int left, int top, int right, int bottom, int depth, int topFlag, PIMAGE pimg)
{
    /* 6个外边界顶点(从左上角开始逆时针数) */
    POINT boundVertexes[6] = {
        {left, top},
        {left,  bottom},
        {right, bottom},
        {right + depth, bottom - depth},
        {right + depth, top - depth},
        {left  + depth, top - depth},
        };


    bar(left, top, right, bottom, pimg);

    line_cap_type startCap, endCap;
    getlinecap(&startCap, &endCap, pimg);
    setlinecap(LINECAP_FLAT, pimg);

    if (topFlag) {
        /* 正面右上边界的3个顶点 */
        POINT sideVertexes[3] = {{left, top}, {right, top}, {right, bottom}};
        polygon(6, (const int*)boundVertexes, pimg);
        polyline(3, (const int*)&sideVertexes, pimg);
        line(right, top, right + depth, top - depth, pimg);
    } else {
        /* 只绘制与底部相连的 5 条边 */
        polyline(5, (const int*)boundVertexes, pimg);
        line(right, top, right, bottom, pimg);
    }

    setlinecap(startCap, endCap, pimg);
}

void drawpoly(int numOfPoints, const int* points, PIMAGE pimg)
{
    /* 闭合曲线, 转为绘制带边框无填充多边形 */
    if ((numOfPoints > 3) && (points[0] == points[(numOfPoints-1)*2]) && (points[1] == points[(numOfPoints-1)*2+1])) {
        polygon(numOfPoints - 1, points, pimg);
    } else {
        polyline(numOfPoints, points, pimg);
    }
}

void fillpoly(int numOfPoints, const int* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->fillPolygon(points, numOfPoints);
            img->m_renderTarget->drawPolygon(points, numOfPoints);
        } else {
#ifdef _WIN32
            Polygon(img->m_hDC, (const POINT*)points, numOfPoints);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void solidpoly(int numOfPoints, const int *points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_renderTarget) {
        img->m_renderTarget->fillPolygon(points, numOfPoints);
    } else {
#ifdef _WIN32
    HBRUSH oldPen = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_PEN));
    fillpoly(numOfPoints, points, pimg);
    SelectObject(img->m_hDC, oldPen);
#endif
    }
    CONVERT_IMAGE_END
}

void polyline(int numOfPoints, const int *points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->drawPolyline(points, numOfPoints);
        } else {
#ifdef _WIN32
            Polyline(img->m_hDC, (const POINT*)points, numOfPoints);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void polygon(int numOfPoints, const int *points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->drawPolygon(points, numOfPoints);
        } else {
#ifdef _WIN32
            HBRUSH oldBrush = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_BRUSH));
            Polygon(img->m_hDC, (const POINT*)points, numOfPoints);
            SelectObject(img->m_hDC, oldBrush);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void fillpoly_gradient(int numOfPoints, const ege_colpoint* points, PIMAGE pimg)
{
    if (numOfPoints < 3) {
        return;
    }
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            color_t* pixels = img->getbuffer();
            if (pixels == NULL) {
                CONVERT_IMAGE_END;
                return;
            }
            const int originX = img->m_vpt.left;
            const int originY = img->m_vpt.top;
            const int clipLeft = img->m_enableclip ? img->m_vpt.left : 0;
            const int clipTop = img->m_enableclip ? img->m_vpt.top : 0;
            const int clipRight = img->m_enableclip ? img->m_vpt.right : img->m_width;
            const int clipBottom = img->m_enableclip ? img->m_vpt.bottom : img->m_height;

            const auto edge = [](float ax, float ay, float bx, float by,
                                 float px, float py) {
                return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
            };

            for (int triangle = 0; triangle < numOfPoints - 2; ++triangle) {
                const ege_colpoint& p0 = points[triangle];
                const ege_colpoint& p1 = points[triangle + 1];
                const ege_colpoint& p2 = points[triangle + 2];
                const float x0 = p0.x + originX;
                const float y0 = p0.y + originY;
                const float x1 = p1.x + originX;
                const float y1 = p1.y + originY;
                const float x2 = p2.x + originX;
                const float y2 = p2.y + originY;
                const float area = edge(x0, y0, x1, y1, x2, y2);
                if (std::abs(area) < FLOAT_EPS) continue;

                const int left = std::max(clipLeft, static_cast<int>(std::floor(
                    std::min(x0, std::min(x1, x2)))));
                const int top = std::max(clipTop, static_cast<int>(std::floor(
                    std::min(y0, std::min(y1, y2)))));
                const int right = std::min(clipRight, static_cast<int>(std::ceil(
                    std::max(x0, std::max(x1, x2)))) + 1);
                const int bottom = std::min(clipBottom, static_cast<int>(std::ceil(
                    std::max(y0, std::max(y1, y2)))) + 1);

                for (int y = top; y < bottom; ++y) {
                    for (int x = left; x < right; ++x) {
                        const float sampleX = x + 0.5f;
                        const float sampleY = y + 0.5f;
                        const float w0 = edge(x1, y1, x2, y2, sampleX, sampleY) / area;
                        const float w1 = edge(x2, y2, x0, y0, sampleX, sampleY) / area;
                        const float w2 = 1.0f - w0 - w1;
                        if (w0 < -FLOAT_EPS || w1 < -FLOAT_EPS || w2 < -FLOAT_EPS) continue;

                        const int red = static_cast<int>(std::lround(
                            w0 * EGEGET_R(p0.color) + w1 * EGEGET_R(p1.color) +
                            w2 * EGEGET_R(p2.color)));
                        const int green = static_cast<int>(std::lround(
                            w0 * EGEGET_G(p0.color) + w1 * EGEGET_G(p1.color) +
                            w2 * EGEGET_G(p2.color)));
                        const int blue = static_cast<int>(std::lround(
                            w0 * EGEGET_B(p0.color) + w1 * EGEGET_B(p1.color) +
                            w2 * EGEGET_B(p2.color)));
                        pixels[y * img->m_width + x] = EGERGB(red, green, blue);
                    }
                }
            }
            CONVERT_IMAGE_END;
            return;
        }
#ifdef _WIN32
        TRIVERTEX* vert = (TRIVERTEX*)malloc(sizeof(TRIVERTEX) * numOfPoints);
        if (vert) {
            GRADIENT_TRIANGLE* tri = (GRADIENT_TRIANGLE*)malloc(sizeof(GRADIENT_TRIANGLE) * (numOfPoints - 2));
            if (tri) {
                for (int i = 0; i < numOfPoints; ++i) {
                    vert[i].x = (long)points[i].x;
                    vert[i].y = (long)points[i].y;
                    vert[i].Red = EGEGET_R(points[i].color) << 8;
                    vert[i].Green = EGEGET_G(points[i].color) << 8;
                    vert[i].Blue = EGEGET_B(points[i].color) << 8;
                    // vert[i].Alpha   = EGEGET_A(points[i].color) << 8;
                    vert[i].Alpha = 0;
                }
                for (int j = 0; j < numOfPoints - 2; ++j) {
                    tri[j].Vertex1 = j;
                    tri[j].Vertex2 = j + 1;
                    tri[j].Vertex3 = j + 2;
                }
                dll::GradientFill(img->getdc(), vert, numOfPoints, tri, numOfPoints - 2, GRADIENT_FILL_TRIANGLE);
                free(tri);
            }
            free(vert);
        }
#endif
    }
    CONVERT_IMAGE_END;
}

void drawbezier(int numOfPoints, const int* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget && points && numOfPoints >= 4) {
            const int usablePoints = 1 + ((numOfPoints - 1) / 3) * 3;
            for (int segment = 0; segment + 3 < usablePoints; segment += 3) {
                const float x0 = (float)points[(segment + 0) * 2];
                const float y0 = (float)points[(segment + 0) * 2 + 1];
                const float x1 = (float)points[(segment + 1) * 2];
                const float y1 = (float)points[(segment + 1) * 2 + 1];
                const float x2 = (float)points[(segment + 2) * 2];
                const float y2 = (float)points[(segment + 2) * 2 + 1];
                const float x3 = (float)points[(segment + 3) * 2];
                const float y3 = (float)points[(segment + 3) * 2 + 1];
                const float controlLength = std::hypot(x1 - x0, y1 - y0) +
                                            std::hypot(x2 - x1, y2 - y1) +
                                            std::hypot(x3 - x2, y3 - y2);
                const int steps = std::max(12, std::min(128, (int)std::ceil(controlLength / 3.0f)));
                float previousX = x0;
                float previousY = y0;
                for (int i = 1; i <= steps; ++i) {
                    const float t = (float)i / steps;
                    const float u = 1.0f - t;
                    const float x = u * u * u * x0 + 3 * u * u * t * x1 +
                                    3 * u * t * t * x2 + t * t * t * x3;
                    const float y = u * u * u * y0 + 3 * u * u * t * y1 +
                                    3 * u * t * t * y2 + t * t * t * y3;
                    img->m_renderTarget->drawLineF(previousX, previousY, x, y);
                    previousX = x;
                    previousY = y;
                }
            }
        } else {
#ifdef _WIN32
        if (numOfPoints % 3 != 1) {
            numOfPoints = numOfPoints - (numOfPoints + 2) % 3;
        }
        PolyBezier(img->m_hDC, (POINT*)points, numOfPoints);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void drawlines(int numlines, const int* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img == NULL || points == NULL || numlines <= 0) {
        CONVERT_IMAGE_END;
        return;
    }
    if (img) {
        if (img->m_renderTarget) {
            for (int lineIndex = 0; lineIndex < numlines; ++lineIndex) {
                const int* linePoints = points + lineIndex * 4;
                img->m_renderTarget->drawLine(linePoints[0], linePoints[1],
                                              linePoints[2], linePoints[3]);
            }
        } else {
#ifdef _WIN32
        DWORD* pl = (DWORD*)malloc(sizeof(DWORD) * numlines);
        if (pl == NULL) {
            CONVERT_IMAGE_END;
            return;
        }
        for (int i = 0; i < numlines; ++i) {
            pl[i] = 2;
        }
        PolyPolyline(img->m_hDC, (POINT*)points, pl, numlines);
        free(pl);
#endif
        }
    }
    CONVERT_IMAGE_END;
}



void floodfill(int x, int y, int borderColor, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->floodFill(x, y, (color_t)borderColor);
        } else {
#ifdef _WIN32
            FloodFill(img->m_hDC, x, y, ARGBTOZBGR(borderColor));
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void floodfillsurface(int x, int y, color_t areacolor, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->floodFillSurface(x, y, areacolor);
        } else {
#ifdef _WIN32
        ExtFloodFill(img->m_hDC, x, y, ARGBTOZBGR(areacolor), FLOODFILLSURFACE);
#endif
        }
    }
    CONVERT_IMAGE_END;
}

void getlinestyle(int* linestyle, unsigned short* pattern, int* thickness, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (linestyle) {
        *linestyle = img->m_linestyle.linestyle;
    }
    if (pattern) {
        *pattern = img->m_linestyle.upattern;
    }
    if (thickness) {
        *thickness = img->m_linestyle.thickness;
    }
    CONVERT_IMAGE_END;
}

void setlinestyle(int linestyle, unsigned short pattern, int thickness, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        img->m_linestyle.thickness = thickness;
        img->m_linewidth = (float)thickness;
        img->m_linestyle.linestyle = linestyle;
        img->m_linestyle.upattern = pattern;

        if (img->m_renderTarget) {
            img->m_renderTarget->setLineStyle((LineStyle)linestyle, pattern, thickness);
        }

#ifdef _WIN32
        update_pen(img);
#endif
    }

    CONVERT_IMAGE_END;
}

void setlinewidth(float width, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        img->m_linestyle.thickness = (int)width;
        img->m_linewidth = width;

        if (img->m_renderTarget) {
            img->m_renderTarget->setLineWidth(width);
        }

        update_pen(img);
    }
    CONVERT_IMAGE_END;
}

#ifdef EGE_GDIPLUS
Gdiplus::LineCap convertToGdiplusLineCap(line_cap_type linecap)
{
    Gdiplus::LineCap cap = Gdiplus::LineCapFlat;
    switch(linecap) {
        case LINECAP_FLAT:   cap = Gdiplus::LineCapFlat;   break;
        case LINECAP_SQUARE: cap = Gdiplus::LineCapSquare; break;
        case LINECAP_ROUND:  cap = Gdiplus::LineCapRound;  break;
    }

    return cap;
}

Gdiplus::LineJoin convertToGdiplusLineJoin(line_join_type linejoin)
{
    Gdiplus::LineJoin joinType = Gdiplus::LineJoinMiter;
    switch(linejoin) {
        case LINEJOIN_MITER: joinType = Gdiplus::LineJoinMiter; break;
        case LINEJOIN_BEVEL: joinType = Gdiplus::LineJoinBevel; break;
        case LINEJOIN_ROUND: joinType = Gdiplus::LineJoinRound; break;
    }

    return joinType;
}
#endif

void setlinecap(line_cap_type linecap, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        img->m_linestartcap = linecap;
        img->m_lineendcap   = linecap;

        if (img->m_renderTarget) {
            img->m_renderTarget->setLineCap((RTLineCap)linecap, (RTLineCap)linecap);
        }

        update_pen(img);
    }
    CONVERT_IMAGE_END;
}

void setlinecap(line_cap_type startCap, line_cap_type endCap, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        img->m_linestartcap = startCap;
        img->m_lineendcap   = endCap;

        if (img->m_renderTarget) {
            img->m_renderTarget->setLineCap((RTLineCap)startCap, (RTLineCap)endCap);
        }

        update_pen(img);
    }
    CONVERT_IMAGE_END;
}

void getlinecap(line_cap_type* startCap, line_cap_type* endCap, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        if (startCap != NULL) {
            *startCap = img->m_linestartcap;
        }

        if (endCap != NULL) {
            *endCap = img->m_lineendcap;
        }
    }
    CONVERT_IMAGE_END
}

line_cap_type getlinecap(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        return img->m_linestartcap;
    }
    CONVERT_IMAGE_END;
    return LINECAP_FLAT;
}

void setlinejoin(line_join_type linejoin, PIMAGE pimg)
{
    float miterLimit;
    getlinejoin(NULL, &miterLimit, pimg);
    setlinejoin(linejoin, miterLimit, pimg);
}

void setlinejoin(line_join_type linejoin, float miterLimit, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        miterLimit = MAX(1.0f, miterLimit);
        img->m_linejoin = linejoin;
        img->m_linejoinmiterlimit = miterLimit;

        if (img->m_renderTarget) {
            img->m_renderTarget->setLineJoin((RTLineJoin)linejoin, miterLimit);
        }

        update_pen(img);
    }
    CONVERT_IMAGE_END;
}

void getlinejoin(line_join_type *linejoin, float *miterLimit, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        if (linejoin != NULL) {
            *linejoin = img->m_linejoin;
        }

        if (miterLimit != NULL) {
            *miterLimit = img->m_linejoinmiterlimit;
        }
    }
    CONVERT_IMAGE_END
}

line_join_type getlinejoin(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img) {
        return img->m_linejoin;
    }
    CONVERT_IMAGE_END;
    return LINEJOIN_MITER;
}

void setfillstyle(int pattern, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    img->m_fillcolor = color;
    img->m_fillstyle = pattern;
    if (img->m_renderTarget) {
        const FillStyle style = (pattern >= EMPTY_FILL && pattern <= USER_FILL)
            ? static_cast<FillStyle>(pattern) : FILL_SOLID;
        img->m_renderTarget->setFillStyle(style, color);
        img->m_renderTarget->setFillColor(color);
    }
#ifdef _WIN32
    LOGBRUSH lbr = {0};
    lbr.lbColor = ARGBTOZBGR(color);
    // SetBkColor(img->m_hDC, color);
    if (pattern == EMPTY_FILL) {
        lbr.lbStyle = BS_NULL;
    } else if (pattern == SOLID_FILL) {
        lbr.lbStyle = BS_SOLID;
    } else if (pattern < USER_FILL) { // dose not finish
        int hatchmap[] = {
            HS_HORIZONTAL,
            HS_BDIAGONAL,
            HS_BDIAGONAL,
            HS_FDIAGONAL,
            HS_FDIAGONAL,
            HS_CROSS,
            HS_DIAGCROSS,
            HS_VERTICAL,
            HS_DIAGCROSS,
            HS_DIAGCROSS
        };

        lbr.lbStyle = BS_HATCHED;
        lbr.lbHatch = hatchmap[pattern - 2];
    } else {
        lbr.lbStyle = BS_SOLID;
    }
    HBRUSH hbr = CreateBrushIndirect(&lbr);
    if (hbr) {
        DeleteObject(SelectObject(img->m_hDC, hbr));
    }
#endif
#ifdef EGE_GDIPLUS
    img->set_pattern(NULL);
#else
    clearNativeFallbackPattern(img);
#endif
    CONVERT_IMAGE_END;
}

void setrendermode(rendermode_e mode)
{
    struct _graph_setting* pg = &graph_setting;
#ifdef _WIN32
    if (mode == RENDER_MANUAL) {
        if (pg->lock_window) {
            ;
        } else {
            KillTimer(pg->hwnd, RENDER_TIMER_ID);
            pg->timer_stop_mark = true;
            PostMessageW(pg->hwnd, WM_TIMER, RENDER_TIMER_ID, 0);
            pg->lock_window = true;
            while (pg->timer_stop_mark) {
                ::Sleep(1);
            }
        }
    } else {
        delay_ms(0);
        SetTimer(pg->hwnd, RENDER_TIMER_ID, 50, NULL);
        pg->skip_timer_mark = false;
        pg->lock_window = false;
    }
#else
    if (mode == RENDER_MANUAL) {
        pg->lock_window = true;
    } else {
        delay_ms(0);
        pg->skip_timer_mark = false;
        pg->lock_window = false;
    }
#endif
}

void setactivepage(int page)
{
    struct _graph_setting* pg = &graph_setting;
    if (0 <= page && page < BITMAP_PAGE_SIZE) {
        pg->active_page = page;

        /* 为未创建的绘图页分配图像 */
        if (pg->img_page[page] == NULL) {
            color_t bkColor = (page == 0) ? pg->window_initial_color : BLACK;
            pg->img_page[page] = new IMAGE(pg->dc_w, pg->dc_h, bkColor);
        }

        pg->imgtarget = pg->img_page[page];
        pg->dc = pg->img_page[page]->m_hDC;
    }
}

void setvisualpage(int page)
{
    struct _graph_setting* pg = &graph_setting;
    if (0 <= page && page < BITMAP_PAGE_SIZE) {
        pg->visual_page = page;
        if (pg->img_page[page] == NULL) {
            pg->img_page[page] = new IMAGE(pg->dc_w, pg->dc_h, BLACK);
        }
        pg->update_mark_count = 0;
    }
}

void swappage()
{
    struct _graph_setting* pg = &graph_setting;
    setvisualpage(pg->active_page);
    setactivepage(1 - pg->active_page);
}

void window_getviewport(struct viewporttype* viewport)
{
    struct _graph_setting* pg = &graph_setting;
    viewport->left = pg->base_x;
    viewport->top = pg->base_y;
    viewport->right = pg->base_w + pg->base_x;
    viewport->bottom = pg->base_h + pg->base_y;
}

void window_getviewport(int* left, int* top, int* right, int* bottom)
{
    struct _graph_setting* pg = &graph_setting;
    if (left) {
        *left = pg->base_x;
    }
    if (top) {
        *top = pg->base_y;
    }
    if (right) {
        *right = pg->base_w + pg->base_x;
    }
    if (bottom) {
        *bottom = pg->base_h + pg->base_y;
    }
}

void window_setviewport(int left, int top, int right, int bottom)
{
    struct _graph_setting* pg = &graph_setting;
    int same_xy = 0, same_wh = 0;
    if (pg->base_x == left && pg->base_y == top) {
        same_xy = 1;
    }
    if (pg->base_w == bottom - top && pg->base_h == right - left) {
        same_wh = 1;
    }
    pg->base_x = left;
    pg->base_y = top;
    pg->base_w = right - left;
    pg->base_h = bottom - top;
    if (same_xy == 0 || same_wh == 0) {
        graph_setting.update_mark_count -= 1;
    }
    /* 修正窗口大小 */
    if (same_wh == 0) {
#ifdef _WIN32
        RECT rect, crect;
        int dw, dh;
        GetClientRect(pg->hwnd, &crect);
        GetWindowRect(pg->hwnd, &rect);
        dw = pg->base_w - crect.right;
        dh = pg->base_h - crect.bottom;
        {
            HWND hwnd = GetParent(pg->hwnd);
            if (hwnd) {
                POINT pt = {0, 0};
                ClientToScreen(hwnd, &pt);
                rect.left -= pt.x;
                rect.top -= pt.y;
                rect.right -= pt.x;
                rect.bottom -= pt.y;
            }

            MoveWindow(pg->hwnd, rect.left, rect.top, rect.right + dw - rect.left, rect.bottom + dh - rect.top, TRUE);
        }
#endif
    }
}

void getviewport(int* left, int* top, int* right, int* bottom, int* clip, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (left) {
        *left = img->m_vpt.left;
    }
    if (top) {
        *top = img->m_vpt.top;
    }
    if (right) {
        *right = img->m_vpt.right;
    }
    if (bottom) {
        *bottom = img->m_vpt.bottom;
    }
    if (clip) {
        *clip = img->m_enableclip;
    }
    CONVERT_IMAGE_END;
}

void setviewport(int left, int top, int right, int bottom, int clip, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    Bound viewport(left, top, right, bottom, false);

    if (!viewport.isNormalized()) {
        return;
    }

    Point oldOrigin(img->m_vpt.left, img->m_vpt.top);

    if (img->m_renderTarget) {
        img->m_vpt = viewport;
        img->m_enableclip = clip;
        img->m_renderTarget->setViewport(left, top, right, bottom, clip);
        img->m_renderTarget->moveTo(0, 0);
#ifdef EGE_GDIPLUS
        img->syncGraphicsViewport(oldOrigin.x, oldOrigin.y);
#endif
        CONVERT_IMAGE_END;
        return;
    }

#ifdef _WIN32
    SetViewportOrgEx(img->m_hDC, 0, 0, NULL);
#endif

    img->m_vpt = viewport;
    img->m_enableclip = clip;

#ifdef _WIN32
    if (clip) {
        HRGN rgn = CreateRectRgn(viewport.left, viewport.top, viewport.right, viewport.bottom);
        SelectClipRgn(img->m_hDC, rgn);
        DeleteObject(rgn);
    } else {
        SelectClipRgn(img->m_hDC, NULL); /* 清除裁剪区域，不做裁剪*/
    }
#endif

    /* GDI+ 设置裁剪区域时受当前坐标系影响，确保在设备坐标系下进行 */
#ifdef EGE_GDIPLUS
    img->getGraphics();
    img->syncGraphicsViewport(oldOrigin.x, oldOrigin.y);
#endif

#ifdef _WIN32
    SetViewportOrgEx(img->m_hDC, left, top, NULL);

    /* 改变视口区域后将当前位置重置为 (0, 0)*/
    MoveToEx(img->m_hDC, 0, 0, NULL);
#endif

    CONVERT_IMAGE_END;
}

void clearviewport(PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img && img->m_renderTarget) {
        img->m_renderTarget->clearViewport();
    } else if (img && img->m_hDC) {
#ifdef _WIN32
        RECT rect = {0, 0, img->m_vpt.right - img->m_vpt.left, img->m_vpt.bottom - img->m_vpt.top};
        HBRUSH hbr = CreateSolidBrush(GetBkColor(img->m_hDC));
        FillRect(img->m_hDC, &rect, hbr);
        DeleteObject(hbr);
#endif
    }
    CONVERT_IMAGE_END;
}

#ifdef EGE_GDIPLUS
Gdiplus::DashStyle linestyle_to_dashstyle(int linestyle)
{
    switch (linestyle) {
    case SOLID_LINE:
        return Gdiplus::DashStyleSolid;
    case PS_DASH:
        return Gdiplus::DashStyleDash;
    case PS_DOT:
        return Gdiplus::DashStyleDot;
    case PS_DASHDOT:
        return Gdiplus::DashStyleDashDot;
    }
    return Gdiplus::DashStyleSolid;
}

void ege_line(float x1, float y1, float x2, float y2, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawLine(pen, x1, y1, x2, y2);
    }
    CONVERT_IMAGE_END;
}

void ege_drawpoly(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    /* 当首尾顶点为同一坐标时转成多边形，否则绘制折线 */
    if (numOfPoints > 3 && points[0].x == points[numOfPoints-1].x
        && points[0].y == points[numOfPoints-1].y) {
        ege_polygon(numOfPoints - 1, points, pimg);
    } else {
        ege_polyline(numOfPoints, points, pimg);
    }
}

void ege_polyline(int numOfPoints, const ege_point *points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawLines(pen, (const Gdiplus::PointF*)points, numOfPoints);
    }
    CONVERT_IMAGE_END;
}

void ege_polygon(int numOfPoints, const ege_point *points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawPolygon(pen, (const Gdiplus::PointF*)points, numOfPoints);
    }
    CONVERT_IMAGE_END;
}

void ege_drawcurve(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawCurve(pen, (const Gdiplus::PointF*)points, numOfPoints);
    }
    CONVERT_IMAGE_END;
}

void ege_drawcurve(int numOfPoints, const ege_point *points, float tension, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->DrawCurve(img->getPen(), (const Gdiplus::PointF*)points, numOfPoints, tension);
    }
    CONVERT_IMAGE_END;
}

void ege_drawclosedcurve(int numOfPoints, const ege_point *points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->DrawClosedCurve(img->getPen(), (const Gdiplus::PointF*)points, numOfPoints);
    }
    CONVERT_IMAGE_END;
}

void ege_drawclosedcurve(int numOfPoints, const ege_point *points, float tension, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->DrawClosedCurve(img->getPen(), (const Gdiplus::PointF*)points, numOfPoints, tension);
    }
    CONVERT_IMAGE_END;
}


void ege_fillclosedcurve(int numOfPoints, const ege_point *points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->FillClosedCurve(img->getBrush(), (const Gdiplus::PointF*)points, numOfPoints);
    }
    CONVERT_IMAGE_END;
}

void ege_fillclosedcurve(int numOfPoints, const ege_point *points, float tension, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->FillClosedCurve(img->getBrush(), (const Gdiplus::PointF*)points, numOfPoints,
            Gdiplus::FillModeAlternate, tension);
    }
    CONVERT_IMAGE_END;
}

void ege_rectangle(float x, float y, float w, float h, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawRectangle(pen, x, y, w, h);
    }
    CONVERT_IMAGE_END;
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
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawEllipse(pen, x, y, w, h);
    }
    CONVERT_IMAGE_END;
}

void ege_pie(float x, float y, float w, float h, float startAngle, float sweepAngle, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawPie(pen, x, y, w, h, startAngle, sweepAngle);
    }
    CONVERT_IMAGE_END;
}

void ege_arc(float x, float y, float w, float h, float startAngle, float sweepAngle, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawArc(pen, x, y, w, h, startAngle, sweepAngle);
    }
    CONVERT_IMAGE_END;
}

void ege_bezier(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    ege_drawbezier(numOfPoints, points, pimg);
}

void ege_drawbezier(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        if (img->m_renderTarget && points && numOfPoints >= 4) {
            std::vector<int> integerPoints(static_cast<size_t>(numOfPoints) * 2);
            for (int i = 0; i < numOfPoints; ++i) {
                integerPoints[i * 2] = (int)std::lround(points[i].x);
                integerPoints[i * 2 + 1] = (int)std::lround(points[i].y);
            }
            drawbezier(numOfPoints, integerPoints.data(), img);
        } else {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawBeziers(pen, (const Gdiplus::PointF*)points, numOfPoints);
        }
    }
    CONVERT_IMAGE_END;
}

void ege_setpattern_none(PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    img->set_pattern(NULL);
    CONVERT_IMAGE_END;
}

void ege_setpattern_lineargradient(float x1, float y1, color_t c1, float x2, float y2, color_t c2, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::LinearGradientBrush* pbrush = new Gdiplus::LinearGradientBrush(
            Gdiplus::PointF(x1, y1), Gdiplus::PointF(x2, y2), Gdiplus::Color(c1), Gdiplus::Color(c2));
        img->set_pattern(pbrush);
    }
    CONVERT_IMAGE_END;
}

void ege_setpattern_pathgradient(ege_point center, color_t centerColor,
    int count, const ege_point* points,
    int colorCount, const color_t* pointColors,
    PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::PathGradientBrush* pbrush =
            new Gdiplus::PathGradientBrush((const Gdiplus::PointF*)points, count, Gdiplus::WrapModeTile);
        pbrush->SetCenterColor(Gdiplus::Color(centerColor));
        pbrush->SetCenterPoint(Gdiplus::PointF(center.x, center.y));
        pbrush->SetSurroundColors((const Gdiplus::Color*)pointColors, &colorCount);
        img->set_pattern(pbrush);
    }
    CONVERT_IMAGE_END;
}

void ege_setpattern_ellipsegradient(ege_point center,
    color_t centerColor,
    float x,
    float y,
    float w,
    float h,
    color_t color,
    PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::GraphicsPath path;
        path.AddEllipse(x, y, w, h);
        Gdiplus::PathGradientBrush* pbrush = new Gdiplus::PathGradientBrush(&path);
        int count = 1;
        pbrush->SetCenterColor(Gdiplus::Color(centerColor));
        pbrush->SetCenterPoint(Gdiplus::PointF(center.x, center.y));
        pbrush->SetSurroundColors((const Gdiplus::Color*)&color, &count);
        img->set_pattern(pbrush);
    }
    CONVERT_IMAGE_END;
}

void ege_setpattern_texture(PIMAGE srcimg, float x, float y, float w, float h, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && srcimg) {
        if (srcimg->m_texture) {
            // A generated GDI+ texture wraps the IMAGE CPU buffer. Synchronize
            // the authoritative surface before the brush captures it.
            (void)srcimg->getbuffer();
            Gdiplus::TextureBrush* pbrush =
                new Gdiplus::TextureBrush((Gdiplus::Image*)srcimg->m_texture, Gdiplus::WrapModeTile, x, y, w, h);
            img->set_pattern(pbrush);
        }
    }
    CONVERT_IMAGE_END;
}

void ege_fillpoly(int numOfPoints, const ege_point* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Brush* brush = img->getBrush();
        graphics->FillPolygon(brush, (const Gdiplus::PointF*)points, numOfPoints);
    }
    CONVERT_IMAGE_END;
}

void ege_fillrect(float x, float y, float w, float h, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Brush* brush = img->getBrush();
        graphics->FillRectangle(brush, x, y, w, h);
    }
    CONVERT_IMAGE_END;
}

static Gdiplus::GraphicsPath* createRoundRectPath(float x, float y, float w, float h,
    float radius1, float radius2, float radius3, float radius4)
{
    if ((w <= 0.0f) || (h <= 0.0f))
        return NULL;

    radius1 = clamp(radius1, 0.0f, MIN(w, h));
    radius2 = clamp(radius2, 0.0f, MIN(w - radius1, h));
    radius3 = clamp(radius3, 0.0f, MIN(h - radius2, w));
    radius4 = clamp(radius4, 0.0f, MIN(h - radius1, w - radius3));

    Gdiplus::GraphicsPath* path = new Gdiplus::GraphicsPath;

    if (radius2 < w - radius1)
        path->AddLine(x + radius1,  y,  x + w - radius2,  y);

    if (radius2 > 0.0f)
        path->AddArc (x + w - (radius2 * 2),  y,  radius2 * 2,  radius2 * 2,  270,  90);

    if (radius3 < h - radius2)
        path->AddLine(x + w,  y + radius2,  x + w,  y + h - radius3);

    if (radius3 > 0.0f)
        path->AddArc (x + w - (radius3 * 2),  y + h - (radius3 * 2),  radius3 * 2,  radius3 * 2,  0,  90);

    if (radius4 < w - radius3)
        path->AddLine(x + w - radius3,  y + h,  x + radius4,  y + h);

    if (radius4 > 0.0f)
        path->AddArc (x,  y + h - (radius4 * 2),  radius4 * 2,  radius4 * 2,  90,  90);

    if (radius4 < w - radius1)
        path->AddLine(x,  y + h - radius4 ,  x,  y + radius1);

    if (radius1 > 0.0f)
        path->AddArc (x,  y, radius1 * 2,  radius1 * 2,  180,  90);

    path->CloseFigure();

    return path;
}

void ege_roundrect(float x, float y, float w, float h,  float radius, PIMAGE pimg)
{
    ege_roundrect(x, y, w, h, radius,  radius,  radius,  radius, pimg);
}
void ege_fillroundrect(float x, float y, float w, float h,  float radius, PIMAGE pimg)
{
    ege_fillroundrect(x, y, w, h, radius,  radius,  radius,  radius, pimg);
}
void ege_roundrect(float x, float y, float w, float h,  float radius1, float radius2, float radius3, float radius4, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    Gdiplus::GraphicsPath* path = createRoundRectPath(x, y, w, h, radius1,  radius2,  radius3,  radius4);

    if (path != NULL) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->DrawPath(img->getPen(), path);
        delete path;
    }
    CONVERT_IMAGE_END
}

void ege_fillroundrect(float x, float y, float w, float h,  float radius1, float radius2, float radius3, float radius4, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    Gdiplus::GraphicsPath* path = createRoundRectPath(x, y, w, h, radius1,  radius2,  radius3,  radius4);

    if (path != NULL) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->FillPath(img->getBrush(), path);
        delete path;
    }
    CONVERT_IMAGE_END
}

void ege_fillellipse(float x, float y, float w, float h, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_renderTarget) {
            img->m_renderTarget->fillEllipse(
                static_cast<int>(x), static_cast<int>(y), 0, 360,
                static_cast<int>(w), static_cast<int>(h));
        } else {
            Gdiplus::Graphics* graphics = img->getGraphics();
            Gdiplus::Brush* brush = img->getBrush();
            graphics->FillEllipse(brush, x, y, w, h);
        }
    }
    CONVERT_IMAGE_END;
}

void ege_fillpie(float x, float y, float w, float h, float startAngle, float sweepAngle, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Brush* brush = img->getBrush();
        graphics->FillPie(brush, x, y, w, h, startAngle, sweepAngle);
    }
    CONVERT_IMAGE_END;
}

#endif
void ege_setalpha(int alpha, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        const color_t a = static_cast<color_t>(alpha) << 24;
        int len = img->m_width * img->m_height;
        color_t* buffer = img->getbuffer();
        if (buffer == NULL) {
            CONVERT_IMAGE_END;
            return;
        }
        for (int i = 0; i < len; ++i) {
            const color_t c = buffer[i];
            buffer[i] = a | (c & 0xFFFFFF);
        }
    }
    CONVERT_IMAGE_END;
}
#ifdef EGE_GDIPLUS

void ege_gentexture(bool generate, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        img->gentexture(generate);
    }
    CONVERT_IMAGE_END;
}

void ege_puttexture(PCIMAGE srcimg, float x, float y, float w, float h, PIMAGE pimg)
{
    ege_rect dest = {x, y, w, h};
    ege_puttexture(srcimg, dest, pimg);
}

void ege_puttexture(PCIMAGE srcimg, ege_rect dest, PIMAGE pimg)
{
    ege_rect src;
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && srcimg) {
        src.x = 0;
        src.y = 0;
        src.w = (float)srcimg->getwidth();
        src.h = (float)srcimg->getheight();
        ege_puttexture(srcimg, dest, src, img);
    }
    CONVERT_IMAGE_END;
}

void ege_puttexture(PCIMAGE srcimg, ege_rect dest, ege_rect src, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && srcimg) {
        if (srcimg->m_texture) {
            // Keep the legacy live-DIB texture behavior after native draws.
            (void)srcimg->getbuffer();
            Gdiplus::Graphics* graphics = img->getGraphics();
            /*
            Gdiplus::ImageAttributes ia;
            Gdiplus::ColorMatrix mx = {
                {
                    {1.0, 0.0, 0.0, 0.0, 0.0},
                    {0.0, 1.0, 0.0, 0.0, 0.0},
                    {0.0, 0.0, 1.0, 0.0, 0.0},
                    {0.0, 0.0, 0.0, 1.0, 0.0},
                    {0.0, 0.0, 0.0, 0.0, 1.0},
                }
            };
            ia.SetColorMatrix(&mx);
            // */
            // graphics.SetTransform();
            graphics->DrawImage((Gdiplus::Image*)srcimg->m_texture,
                Gdiplus::RectF(dest.x, dest.y, dest.w, dest.h),
                src.x, src.y, src.w, src.h,
                Gdiplus::UnitPixel,
                NULL);
        }
    }
    CONVERT_IMAGE_END;
}

void EGEAPI ege_drawimage(PCIMAGE srcimg, int xDest, int yDest, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && srcimg) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Bitmap bitmap(srcimg->getwidth(),
            srcimg->getheight(),
            4 * srcimg->getwidth(),
            PixelFormat32bppPARGB,
            (BYTE*)(srcimg->getbuffer()));
        Gdiplus::Point p(xDest, yDest);
        graphics->DrawImage(&bitmap, p);
    }
    CONVERT_IMAGE_END;
}

void EGEAPI ege_drawimage(PCIMAGE srcimg,
    int xDest,
    int yDest,
    int widthDest,
    int heightDest,
    int xSrc,
    int ySrc,
    int srcWidth,
    int srcHeight,
    PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && srcimg) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Bitmap bitmap(srcimg->getwidth(),
            srcimg->getheight(),
            4 * srcimg->getwidth(),
            PixelFormat32bppPARGB,
            (BYTE*)(srcimg->getbuffer()));
        Gdiplus::Point destPoints[3] = {
            Gdiplus::Point(xDest, yDest), Gdiplus::Point(xDest + widthDest, yDest), Gdiplus::Point(xDest, yDest + heightDest)};
        graphics->DrawImage(
            &bitmap, destPoints, 3, xSrc, ySrc, srcWidth, srcHeight, Gdiplus::UnitPixel, NULL, NULL, NULL);
    }
    CONVERT_IMAGE_END;
}


void EGEAPI ege_transform_rotate(float angle, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->RotateTransform(angle);
    }
    CONVERT_IMAGE_END;
}

void EGEAPI ege_transform_translate(float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->TranslateTransform(x, y);
    }
    CONVERT_IMAGE_END;
}

void EGEAPI ege_transform_scale(float scale_x, float scale_y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->ScaleTransform(scale_x, scale_y);
    }
    CONVERT_IMAGE_END;
}

void EGEAPI ege_transform_reset(PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->ResetTransform();
    }
    CONVERT_IMAGE_END;
}
void EGEAPI ege_get_transform(ege_transform_matrix* matrix, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && matrix) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Matrix mat;
        Gdiplus::REAL elements[6];
        graphics->GetTransform(&mat);
        mat.GetElements(elements);
        matrix->m11 = elements[0];
        matrix->m12 = elements[1];
        matrix->m21 = elements[2];
        matrix->m22 = elements[3];
        matrix->m31 = elements[4];
        matrix->m32 = elements[5];
    }
    CONVERT_IMAGE_END;
}

void EGEAPI ege_set_transform(const ege_transform_matrix* matrix, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && matrix) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Matrix mat;
        matrixConvert(*matrix, mat);
        graphics->SetTransform(&mat);
    }
    CONVERT_IMAGE_END;
}

ege_point EGEAPI ege_transform_calc(ege_point p, PIMAGE pimg)
{
    return ege_transform_calc(p.x, p.y, pimg);
}

ege_point EGEAPI ege_transform_calc(float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    ege_point point = {x, y};
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Matrix matrix;
        graphics->GetTransform(&matrix);
        matrix.TransformPoints((Gdiplus::PointF*)&point, 1);
    }
    CONVERT_IMAGE_END;
    return point;
}

#endif // EGEGDIPLUS

HWND getHWnd()
{
    struct _graph_setting* pg = &graph_setting;
    return pg->hwnd;
}

HINSTANCE getHInstance()
{
    struct _graph_setting* pg = &graph_setting;
    return pg->instance;
}

int message_addkeyhandler(void* param, LPMSG_KEY_PROC func)
{
    struct _graph_setting* pg = &graph_setting;
    pg->callback_key = func;
    pg->callback_key_param = param;
    return grOk;
}

int message_addmousehandler(void* param, LPMSG_MOUSE_PROC func)
{
    struct _graph_setting* pg = &graph_setting;
    pg->callback_mouse = func;
    pg->callback_mouse_param = param;
    return grOk;
}

int SetCloseHandler(LPCALLBACK_PROC func)
{
    struct _graph_setting* pg = &graph_setting;
    pg->callback_close = func;
    return grOk;
}

/* private funcion */
static void draw_frame(PIMAGE img, int l, int t, int r, int b, color_t lc, color_t dc)
{
    setcolor(lc, img);
    moveto(l, b, img);
    lineto(l, t, img);
    lineto(r, t, img);

    setcolor(dc, img);
    lineto(r, b, img);
    lineto(l, b, img);
}

#ifdef _WIN32
int inputbox_getline(const char* title, const char* text, LPSTR buf, int len)
{
    if (!buf || len <= 0) return 0;
    buf[0] = '\0';
    const std::wstring& title_w = mb2w(title ? title : "");
    const std::wstring& text_w = mb2w(text ? text : "");
    std::wstring buf_w(len, L'\0');
    int ret = inputbox_getline(title_w.c_str(), text_w.c_str(), &buf_w[0], len);
    if (ret) {
        if (!WideCharToMultiByte(getcodepage(), 0, buf_w.c_str(), -1, buf, len, 0, 0)) {
            buf[0] = '\0';
            return 0;
        }
    }
    return ret;
}

int inputbox_getline(const wchar_t* title, const wchar_t* text, LPWSTR buf, int len)
{
    if (!buf || len <= 0) return 0;
    IMAGE bg;
    IMAGE window;
    int w = 400, h = 300, x = (getwidth() - w) / 2, y = (getheight() - h) / 2;
    int ret = 0;

    bg.getimage(0, 0, getwidth(), getheight());
    window.resize(w, h);
    buf[0] = 0;

    sys_edit edit(true);
    if (edit.create(true) != grOk) return 0;
    edit.move(x + 30 + 1, y + 192 + 1);
    edit.size(w - (30 + 1) * 2, h - 40 - 192 - 2);
    edit.setmaxlen(len);
    edit.visible(true);
    edit.setfocus();

    setbkcolor(EGERGB(0x80, 0xA0, 0x80), &window);
    draw_frame(&window, 0, 0, w - 1, h - 1, EGERGB(0xA0, 0xC0, 0xA0), EGERGB(0x50, 0x70, 0x50));
    setfillcolor(EGERGB(0, 0, 0xA0), &window);

    for (int dy = 1; dy < 24; dy++) {
        setcolor(HSLtoRGB(240.0f, 1.0f, 0.5f + float(dy / 24.0 * 0.3)), &window);
        line(1, dy, w - 1, dy, &window);
    }

    setcolor(0xFFFFFF, &window);
    setbkmode(TRANSPARENT, &window);
    setfont(18, 0, L"Tahoma", &window);
    outtextxy(3, 3, title ? title : L"", &window);
    setcolor(0x0, &window);
    settextjustify(LEFT_TEXT, TOP_TEXT, &window);
    outtextrect(30, 32, w - 60, 128 - 3 - 32, text ? text : L"", &window);

    putimage(0, 0, &bg);
    putimage(x, y, &window);
    delay_ms(0);

    while (is_run()) {
        key_msg msg = getkey();
        if (msg.key == key_enter && msg.msg == key_msg_up) {
            break;
        }
    }

    edit.gettext(len, buf);
    len = lstrlenW(buf);

    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) {
        buf[--len] = 0;
    }

    ret = len;
    putimage(0, 0, &bg);
    delay_ms(0);
    getflush();
    return ret;
}
#elif defined(EGE_BACKEND_COREGRAPHICS)
static std::size_t completeUTF8PrefixLength(
    const std::string& value, std::size_t capacity)
{
    std::size_t count = std::min(value.size(), capacity);
    // If the first omitted byte is a continuation byte, capacity split a
    // multibyte scalar. Back up to (and exclude) that scalar's lead byte.
    while (count > 0 && count < value.size() &&
           (static_cast<unsigned char>(value[count]) & 0xC0U) == 0x80U) {
        --count;
    }
    return count;
}

int inputbox_getline(const char* title, const char* text, LPSTR buf, int len)
{
    if (buf == nullptr || len <= 0) {
        return 0;
    }
    buf[0] = '\0';
    std::string value;
    if (!backend::MacWindow::inputBox(title, text, &value)) {
        return 0;
    }
    const std::size_t count = completeUTF8PrefixLength(
        value, static_cast<std::size_t>(len - 1));
    std::memcpy(buf, value.data(), count);
    buf[count] = '\0';
    return static_cast<int>(count);
}

int inputbox_getline(const wchar_t* title, const wchar_t* text, LPWSTR buf, int len)
{
    if (buf == nullptr || len <= 0) {
        return 0;
    }
    buf[0] = L'\0';
    std::string value;
    const std::string titleUTF8 = w2utf8(title ? title : L"");
    const std::string textUTF8 = w2utf8(text ? text : L"");
    if (!backend::MacWindow::inputBox(
            titleUTF8.c_str(), textUTF8.c_str(), &value)) {
        return 0;
    }
    const std::wstring wideValue = utf82w(value.c_str());
    const std::size_t count = std::min<std::size_t>(
        wideValue.size(), static_cast<std::size_t>(len - 1));
    std::wmemcpy(buf, wideValue.data(), count);
    buf[count] = L'\0';
    return static_cast<int>(count);
}
#else
int inputbox_getline(const char*, const char*, LPSTR buf, int len)
{
    if (buf != nullptr && len > 0) {
        buf[0] = '\0';
    }
    return 0;
}
int inputbox_getline(const wchar_t*, const wchar_t*, LPWSTR buf, int len)
{
    if (buf != nullptr && len > 0) {
        buf[0] = L'\0';
    }
    return 0;
}
#endif

static double static_frameRate = 0.0;         /* 帧率 */
static int    static_frameCount = 0;          /* 帧数 */
static double static_totalFrameTime = 0.0;    /* 累计时间 */
static double static_lastFrameTime = 0.0;     /* 上一帧更新时间 */

/**
 * 更新帧率
 * @param addFrameCount 是否增加帧数。{true: 帧数计数加一，同时更新帧率; false: 仅更新帧率}
 * @details 帧率通过统计每个固定周期(0.5秒)内的帧数获得。在每个统计周期中，帧率仅在累计时长满一个周期后才会更新。
 */
void updateFrameRate(bool addFrameCount)
{
    double currentTime = get_highfeq_time_ls();

    if (static_lastFrameTime == 0.0) {
        static_lastFrameTime = currentTime;
        return;
    }

    double elapsedTime = static_totalFrameTime + (currentTime - static_lastFrameTime);

    if (addFrameCount) {
        static_frameCount++;
        static_totalFrameTime = elapsedTime;
        static_lastFrameTime  = currentTime;
    }

    /* 以 0.5 秒为一个统计周期，统计时间不足时不更新帧率 */
    if (elapsedTime >= 0.5) {
        static_frameRate = static_frameCount / elapsedTime;

        static_frameCount = 0;
        static_totalFrameTime = 0.0;
        static_lastFrameTime = currentTime;
    }
}

void resetFrameRate()
{
    static_frameRate = 0.0;
    static_frameCount = 0;
    static_totalFrameTime = 0.0;
    static_lastFrameTime = 0.0;
}

float getfps()
{
    return (float)static_frameRate;
}

#ifdef _WIN32
double fclock()
{
    struct _graph_setting* pg = &graph_setting;

    if (pg->fclock_start == 0) {
        pg->fclock_start = ::GetTickCount();
    }

    return (::GetTickCount() - pg->fclock_start) / 1000.0; // get_highfeq_time_ls(pg);
}

LRESULT sys_edit::onMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	// 单行模式下拦截回车以屏蔽响铃，多行模式交由 EDIT 控件处理换行
	if (message == WM_CHAR && wParam == VK_RETURN) {
		// 检查是否为多行模式（通过窗口样式判断）
		LONG style = ::GetWindowLongW(m_hwnd, GWL_STYLE);
		if (!(style & ES_MULTILINE)) {
			return 0;   // 单行模式，屏蔽响铃
		}
		// 多行模式不拦截，让 EDIT 处理换行（插入\r\n）
	}

    switch (message) {
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wParam;
        HBRUSH br = ::CreateSolidBrush(ARGBTOZBGR(m_bgcolor));

        ::SetBkColor(dc, ARGBTOZBGR(m_bgcolor));
        ::SetTextColor(dc, ARGBTOZBGR(m_color));
        ::DeleteObject(m_hBrush);
        m_hBrush = br;
        return (LRESULT)br;
    } break;
    case WM_SETFOCUS:
        m_bInputFocus = 1;
        // call textbox's own message process to show caret
        return ((LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM))m_callback)(m_hwnd, message, wParam, lParam);
    case WM_KILLFOCUS:
        m_bInputFocus = 0;
        // call textbox's own message process to hide caret
        return ((LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM))m_callback)(m_hwnd, message, wParam, lParam);
    default:
        return ((LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM))m_callback)(m_hwnd, message, wParam, lParam);
    }
}

#else
double fclock()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    static double start_time = 0;
    double current_time = ts.tv_sec + ts.tv_nsec / 1e9;
    if (start_time == 0) start_time = current_time;
    return current_time - start_time;
}

LRESULT sys_edit::onMessage(UINT message, WPARAM wParam, LPARAM lParam) { return 0; }
#endif
} // namespace ege
