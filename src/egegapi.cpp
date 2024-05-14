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

#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "ege_head.h"
#include "ege_common.h"
#include "ege_extension.h"


#include <stdio.h>

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
    return graph_setting.has_init;
}

int showmouse(int bShow)
{
    struct _graph_setting* pg = &graph_setting;
    int ret = pg->mouse_show;
    pg->mouse_show = bShow;
    return ret;
}

int mousepos(int* x, int* y)
{
    struct _graph_setting* pg = &graph_setting;
    *x = pg->mouse_last_x;
    *y = pg->mouse_last_y;
    return 0;
}

void setwritemode(int mode, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    SetROP2(img->m_hDC, mode);
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
    x += img->m_vpt.left;
    y += img->m_vpt.top;

    if (in_rect(x, y, img->m_width, img->m_height)) {
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
        img->m_pBuffer[y * img->m_width + x] = color;
    }
    CONVERT_IMAGE_END;
}

void putpixels(int numOfPoints, int* points, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    int x, y, c;
    PDWORD pb = &img->m_pBuffer[img->m_vpt.top * img->m_width + img->m_vpt.left];
    int w = img->m_vpt.right - img->m_vpt.left, h = img->m_vpt.bottom - img->m_vpt.top;
    int tw = img->m_width;
    for (int n = 0; n < numOfPoints; ++n, points += 3) {
        x = points[0], y = points[1], c = points[2];
        if (in_rect(x, y, w, h)) {
            pb[y * tw + x] = c;
        }
    }
    CONVERT_IMAGE_END;
}

void putpixels_f(int numOfPoints, int* points, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE(pimg);
    int x, y, c;
    int tw = img->m_width;
    int th = img->m_height;
    for (int n = 0; n < numOfPoints; ++n, points += 3) {
        x = points[0], y = points[1], c = points[2];
        if (in_rect(x, y, tw, th)) {
            img->m_pBuffer[y * tw + x] = c;
        }
    }
    CONVERT_IMAGE_END;
}

color_t getpixel_f(int x, int y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_F_CONST(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        return img->m_pBuffer[y * img->m_width + x];
    }
    return 0;
}

void putpixel_f(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_F(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        img->m_pBuffer[y * img->m_width + x] = color;
    }
}

void putpixel_withalpha(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    x += img->m_vpt.left;
    y += img->m_vpt.top;
    if (in_rect(x, y, img->m_vpt.right, img->m_vpt.bottom)) {
        color_t& dst_color = (color_t&)img->m_pBuffer[y * img->m_width + x];
        dst_color = alphablend_inline(dst_color, color, EGEGET_A(color));
    }
    CONVERT_IMAGE_END;
}

void putpixel_withalpha_f(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_F(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        color_t& dst_color = (color_t&)img->m_pBuffer[y * img->m_width + x];
        dst_color = alphablend_inline(dst_color, color, EGEGET_A(color));
    }
    CONVERT_IMAGE_END;
}

void putpixel_savealpha(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    x += img->m_vpt.left;
    y += img->m_vpt.top;
    if (in_rect(x, y, img->m_vpt.right, img->m_vpt.bottom)) {
        color_t& dst_color = (color_t&)img->m_pBuffer[y * img->m_width + x];
        dst_color = EGECOLORA(color, EGEGET_A(dst_color));
    }
    CONVERT_IMAGE_END;
}

void putpixel_savealpha_f(int x, int y, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_F(pimg);
    if (in_rect(x, y, img->m_width, img->m_height)) {
        color_t dst_color = (color_t)img->m_pBuffer[y * img->m_width + x];
        dst_color = EGECOLORA(color, EGEGET_A(dst_color));
    }
    CONVERT_IMAGE_END;
}

void moveto(int x, int y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    MoveToEx(img->m_hDC, x, y, NULL);
    CONVERT_IMAGE_END;
}

void moverel(int dx, int dy, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    POINT pt;
    GetCurrentPositionEx(img->m_hDC, &pt);
    dx += pt.x;
    dy += pt.y;
    MoveToEx(img->m_hDC, dx, dy, NULL);
    CONVERT_IMAGE_END;
}

void line(int x1, int y1, int x2, int y2, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    MoveToEx(img->m_hDC, x1, y1, NULL);
    LineTo(img->m_hDC, x2, y2);
    CONVERT_IMAGE_END;
}

void linerel(int dx, int dy, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    POINT pt;
    GetCurrentPositionEx(img->m_hDC, &pt);
    dx += pt.x;
    dy += pt.y;
    LineTo(img->m_hDC, dx, dy);
    CONVERT_IMAGE_END;
}

void lineto(int x, int y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    LineTo(img->m_hDC, x, y);
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
    POINT pt;
    GetCurrentPositionEx(img->m_hDC, &pt);
    line_base((float)pt.x, (float)pt.y, x, y, img);
    CONVERT_IMAGE_END;
}

void linerel_f(float dx, float dy, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    POINT pt;
    GetCurrentPositionEx(img->m_hDC, &pt);
    line_base((float)pt.x, (float)pt.y, (float)pt.x + dx, (float)pt.y + dy, img);
    CONVERT_IMAGE_END;
}

void line_f(float x1, float y1, float x2, float y2, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    line_base(x1, y1, x2, y2, img);
    CONVERT_IMAGE_END;
}

/*private function*/
static int saveBrush(PIMAGE img, int save) // 此函数调用前，已经有Lock
{
    struct _graph_setting* pg = &graph_setting;
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
    return 0;
}

void rectangle(int left, int top, int right, int bottom, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (saveBrush(img, 1)) {
        Rectangle(img->m_hDC, left, top, right, bottom);
        saveBrush(img, 0);
    }
    CONVERT_IMAGE_END;
}

color_t getcolor(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img && img->m_hDC) {
        CONVERT_IMAGE_END;
        return img->m_color;
        /*
        HPEN hpen_c = (HPEN)GetCurrentObject(img->m_hDC, OBJ_PEN);
        LOGPEN logPen;
        GetObject(hpen_c, sizeof(logPen), &logPen);
        CONVERT_IMAGE_END;
        return logPen.lopnColor;
        // */
    }
    CONVERT_IMAGE_END;
    return 0xFFFFFFFF;
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
    LOGBRUSH lbr;
    lbr.lbColor = ARGBTOZBGR(img->m_color);
    lbr.lbStyle = BS_SOLID;
    lbr.lbHatch = 0;

    const int linestyle = img->m_linestyle.linestyle;
    const unsigned short pattern = img->m_linestyle.upattern;
    const int thickness = img->m_linestyle.thickness;

    // 添加这些属性以获得正确的显示效果
    int ls = linestyle | PS_GEOMETRIC | PS_ENDCAP_ROUND | PS_JOIN_ROUND;

    HPEN hpen;
    if (linestyle == USERBIT_LINE) {
        DWORD style[20] = {0};
        int bn = upattern2array(pattern, style);
        hpen = ExtCreatePen(ls, thickness, &lbr, bn, style);
    } else {
        hpen = ExtCreatePen(ls, thickness, &lbr, 0, NULL);
    }
    if (hpen) {
        DeleteObject(SelectObject(img->m_hDC, hpen));
    }

    // why update pen not in IMAGE???
#ifdef EGE_GDIPLUS
    Gdiplus::Pen* pen = img->getPen();
    pen->SetColor(img->m_color);
    pen->SetWidth(img->m_linewidth);
    pen->SetDashStyle(linestyle_to_dashstyle(img->m_linestyle.linestyle));
#endif
}

void setcolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img && img->m_hDC) {
        img->m_color = color;

        update_pen(img);
        SetTextColor(img->m_hDC, ARGBTOZBGR(color));
    }
    CONVERT_IMAGE_END;
}

void setfillcolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    LOGBRUSH lbr = {0};
    img->m_fillcolor = color;
    lbr.lbColor = ARGBTOZBGR(color);
    lbr.lbHatch = BS_SOLID;
    HBRUSH hbr = CreateBrushIndirect(&lbr);
    if (hbr) {
        DeleteObject(SelectObject(img->m_hDC, hbr));
    }
#ifdef EGE_GDIPLUS
    img->set_pattern(NULL);
#endif
    CONVERT_IMAGE_END;
}

color_t getfillcolor(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img && img->m_hDC) {
        CONVERT_IMAGE_END;
        return img->m_fillcolor;
    }
    CONVERT_IMAGE_END;
    return 0xFFFFFFFF;
}

color_t getbkcolor(PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);

    CONVERT_IMAGE_END;
    if (img) {
        return img->m_bk_color;
    }
    return 0xFFFFFFFF;
}

void setbkcolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img && img->m_hDC) {
        PDWORD p = img->m_pBuffer;
        int size = img->m_width * img->m_height;
        color_t col = img->m_bk_color;
        img->m_bk_color = color;
        SetBkColor(img->m_hDC, ARGBTOZBGR(color));
        for (int n = 0; n < size; n++, p++) {
            if (*p == col) {
                *p = color;
            }
        }
    }
    CONVERT_IMAGE_END;
}

void setbkcolor_f(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img && img->m_hDC) {
        img->m_bk_color = color;
        SetBkColor(img->m_hDC, ARGBTOZBGR(color));
    }
    CONVERT_IMAGE_END;
}

void setfontbkcolor(color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img && img->m_hDC) {
        SetBkColor(img->m_hDC, ARGBTOZBGR(color));
    }
    CONVERT_IMAGE_END;
}

void setbkmode(int bkMode, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img && img->m_hDC) {
        SetBkMode(img->m_hDC, bkMode);
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

    if (img && img->m_hDC) {
        color_t c = getbkcolor(img);
        for (color_t *p = (color_t*)img->getbuffer(), *e = (color_t*)&img->getbuffer()[img->m_width * img->m_height];
             p != e;
             ++p)
        {
            *p = c;
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
        Arc(img->m_hDC,
            x - xRadius,
            y - yRadius,
            x + xRadius,
            y + yRadius,
            (int)(x + xRadius * cos(sr)),
            (int)(y - yRadius * sin(sr)),
            (int)(x + xRadius * cos(er)),
            (int)(y - yRadius * sin(er)));
    }
    CONVERT_IMAGE_END;
}

void ellipsef(float x, float y, float startAngle, float endAngle, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    double sr = startAngle / 180.0 * PI, er = endAngle / 180.0 * PI;

    if (img) {
        Arc(img->m_hDC,
            (int)(x - xRadius),
            (int)(y - yRadius),
            (int)(x + xRadius),
            (int)(y + yRadius),
            (int)(x + xRadius * cos(sr)),
            (int)(y - yRadius * sin(sr)),
            (int)(x + xRadius * cos(er)),
            (int)(y - yRadius * sin(er)));
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
    PIMAGE img = CONVERT_IMAGE(pimg);
    double sr = startAngle / 180.0 * PI, er = endAngle / 180.0 * PI;
    if (img) {
        Pie(img->m_hDC,
            x - xRadius,
            y - yRadius,
            x + xRadius,
            y + yRadius,
            (int)round(x + xRadius * cos(sr)),
            (int)round(y - yRadius * sin(sr)),
            (int)round(x + xRadius * cos(er)),
            (int)round(y - yRadius * sin(er)));
    }
    CONVERT_IMAGE_END;
}

void sectorf(float x, float y, float startAngle, float endAngle, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    double sr = startAngle / 180.0 * PI, er = endAngle / 180.0 * PI;
    if (img) {
        Pie(img->m_hDC,
            (int)(x - xRadius),
            (int)(y - yRadius),
            (int)(x + xRadius),
            (int)(y + yRadius),
            (int)round(x + xRadius * cos(sr)),
            (int)round(y - yRadius * sin(sr)),
            (int)round(x + xRadius * cos(er)),
            (int)round(y - yRadius * sin(er)));
    }
    CONVERT_IMAGE_END;
}

void fillellipse(int x, int y, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Ellipse(img->m_hDC, x - xRadius, y - yRadius, x + xRadius, y + yRadius);
    }
    CONVERT_IMAGE_END;
}

void fillellipsef(float x, float y, float xRadius, float yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Ellipse(img->m_hDC, (int)(x - xRadius), (int)(y - yRadius), (int)(x + xRadius), (int)(y + yRadius));
    }
    CONVERT_IMAGE_END;
}

void fillcircle(int x, int y, int radius, PIMAGE pimg)
{
    fillellipse(x, y, radius, radius, pimg);
}

void fillcirclef(float x, float y, float radius, PIMAGE pimg)
{
    fillellipsef(x,y,radius,radius,pimg);
}

void bar(int left, int top, int right, int bottom, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    RECT rect = {left, top, right, bottom};
    HBRUSH hbr_last = (HBRUSH)GetCurrentObject(img->m_hDC, OBJ_BRUSH); //(HBRUSH)SelectObject(pg->g_hdc, hbr);

    if (img) {
        FillRect(img->m_hDC, &rect, hbr_last);
    }
    CONVERT_IMAGE_END;
}

void fillroundrect(int left, int top, int right, int bottom, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        RoundRect(img->m_hDC, left, top, right, bottom, xRadius * 2, yRadius * 2);
    }
    CONVERT_IMAGE_END;
}

void roundrect(int left, int top, int right, int bottom, int xRadius, int yRadius, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        LOGBRUSH lbr;
        HBRUSH hbr;
        HGDIOBJ hOld;
        lbr.lbStyle = BS_NULL;
        lbr.lbHatch = BS_NULL; // ignored
        lbr.lbColor = WHITE;   // ignored
        hbr = CreateBrushIndirect(&lbr);
        hOld = SelectObject(img->m_hDC, hbr);
        RoundRect(img->m_hDC, left, top, right, bottom, xRadius * 2, yRadius * 2);
        SelectObject(img->m_hDC, hOld);
        DeleteObject(hbr);
    }
    CONVERT_IMAGE_END;
}

void fillrect(int left, int top, int right, int bottom, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Rectangle(img->m_hDC, left, top, right, bottom);
    }
    CONVERT_IMAGE_END;
}

void bar3d(int x1, int y1, int x2, int y2, int depth, int topFlag, PIMAGE pimg)
{
    --x2;
    --y2;
    {
        int pt[20] = {
            x2, y2,
            x2, y1,
            x1, y1,
            x1, y2,
            x2, y2,
            x2 + depth, y2 - depth,
            x2 + depth, y1 - depth,
            x1 + depth, y1 - depth,
            x1, y1,
        };

        bar(x1, y1, x2, y2, pimg);
        if (topFlag) {
            drawpoly(9, pt, pimg);
            line(x2, y1, x2 + depth, y1 - depth, pimg);
        } else {
            drawpoly(7, pt, pimg);
        }
    }
}

void drawpoly(int numpoints, const int* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        const POINT* points = (const POINT*)polypoints;
        /* 闭合曲线, 转为绘制带边框无填充多边形 */
        if ((numpoints > 3) && (points[0].x == points[numpoints-1].x)
                && (points[0].y == points[numpoints-1].y)) {
            HBRUSH oldBrush = (HBRUSH)SelectObject(img->m_hDC, GetStockObject(NULL_BRUSH));
            Polygon(img->m_hDC, points, numpoints - 1);
            SelectObject(img->m_hDC, oldBrush);
        } else {
            Polyline(img->m_hDC, (POINT*)polypoints, numpoints);
        }
    }
    CONVERT_IMAGE_END;
}

void drawbezier(int numpoints, const int* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (numpoints % 3 != 1) {
            numpoints = numpoints - (numpoints + 2) % 3;
        }
        PolyBezier(img->m_hDC, (POINT*)polypoints, numpoints);
    }
    CONVERT_IMAGE_END;
}

void drawlines(int numlines, const int* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        DWORD* pl = (DWORD*)malloc(sizeof(DWORD) * numlines);
        for (int i = 0; i < numlines; ++i) {
            pl[i] = 2;
        }
        PolyPolyline(img->m_hDC, (POINT*)polypoints, pl, numlines);
        free(pl);
    }
    CONVERT_IMAGE_END;
}

void fillpoly(int numpoints, const int* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        Polygon(img->m_hDC, (POINT*)polypoints, numpoints);
    }
    CONVERT_IMAGE_END;
}

void fillpoly_gradient(int numpoints, const ege_colpoint* polypoints, PIMAGE pimg)
{
    if (numpoints < 3) {
        return;
    }
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        TRIVERTEX* vert = (TRIVERTEX*)malloc(sizeof(TRIVERTEX) * numpoints);
        if (vert) {
            GRADIENT_TRIANGLE* tri = (GRADIENT_TRIANGLE*)malloc(sizeof(GRADIENT_TRIANGLE) * (numpoints - 2));
            if (tri) {
                for (int i = 0; i < numpoints; ++i) {
                    vert[i].x = (long)polypoints[i].x;
                    vert[i].y = (long)polypoints[i].y;
                    vert[i].Red = EGEGET_R(polypoints[i].color) << 8;
                    vert[i].Green = EGEGET_G(polypoints[i].color) << 8;
                    vert[i].Blue = EGEGET_B(polypoints[i].color) << 8;
                    // vert[i].Alpha   = EGEGET_A(polypoints[i].color) << 8;
                    vert[i].Alpha = 0;
                }
                for (int j = 0; j < numpoints - 2; ++j) {
                    tri[j].Vertex1 = j;
                    tri[j].Vertex2 = j + 1;
                    tri[j].Vertex3 = j + 2;
                }
                dll::GradientFill(img->getdc(), vert, numpoints, tri, numpoints - 2, GRADIENT_FILL_TRIANGLE);
                free(tri);
            }
            free(vert);
        }
    }
    CONVERT_IMAGE_END;
}

void floodfill(int x, int y, int borderColor, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        FloodFill(img->m_hDC, x, y, ARGBTOZBGR(borderColor));
    }
    CONVERT_IMAGE_END;
}

void floodfillsurface(int x, int y, color_t areacolor, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        ExtFloodFill(img->m_hDC, x, y, ARGBTOZBGR(areacolor), FLOODFILLSURFACE);
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
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (!(img && img->m_hDC)) {
        CONVERT_IMAGE_END;
        return;
    }

    img->m_linestyle.thickness = thickness;
    img->m_linewidth = (float)thickness;
    img->m_linestyle.linestyle = linestyle;
    img->m_linestyle.upattern = pattern;

    update_pen(img);

    CONVERT_IMAGE_END;
}

void setlinewidth(float width, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);

    if (img && img->m_hDC) {
        img->m_linestyle.thickness = (int)width;
        img->m_linewidth = width;

        update_pen(img);
    }
    CONVERT_IMAGE_END;
}

void setfillstyle(int pattern, color_t color, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    LOGBRUSH lbr = {0};
    img->m_fillcolor = color;
    lbr.lbColor = ARGBTOZBGR(color);
    // SetBkColor(img->m_hDC, color);
    if (pattern < SOLID_FILL) {
        lbr.lbHatch = BS_NULL;
    } else if (pattern == SOLID_FILL) {
        lbr.lbHatch = BS_SOLID;
    } else if (pattern < USER_FILL) { // dose not finish
        int hatchmap[] = {
            HS_VERTICAL,
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
        lbr.lbHatch = BS_SOLID;
    }
    HBRUSH hbr = CreateBrushIndirect(&lbr);
    if (hbr) {
        DeleteObject(SelectObject(img->m_hDC, hbr));
    }
#ifdef EGE_GDIPLUS
    img->set_pattern(NULL);
#endif
    CONVERT_IMAGE_END;
}

void setrendermode(rendermode_e mode)
{
    if (mode == RENDER_MANUAL) {
        struct _graph_setting* pg = &graph_setting;
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
        struct _graph_setting* pg = &graph_setting;
        delay_ms(0);
        SetTimer(pg->hwnd, RENDER_TIMER_ID, 50, NULL);
        pg->skip_timer_mark = false;
        pg->lock_window = false;
    }
}

void setactivepage(int page)
{
    struct _graph_setting* pg = &graph_setting;
    if (0 <= page && page < BITMAP_PAGE_SIZE) {
        pg->active_page = page;

        if (pg->img_page[page] == NULL) {
            pg->img_page[page] = new IMAGE(pg->dc_w, pg->dc_h);
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
            pg->img_page[page] = new IMAGE(pg->dc_w, pg->dc_h);
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
        *clip = img->m_vpt.clipflag;
    }
    CONVERT_IMAGE_END;
}

void setviewport(int left, int top, int right, int bottom, int clip, PIMAGE pimg)
{
    // struct _graph_setting * pg = &graph_setting;

    PIMAGE img = CONVERT_IMAGE(pimg);

    SetViewportOrgEx(img->m_hDC, 0, 0, NULL);

    img->m_vpt.left = left;
    img->m_vpt.top = top;
    img->m_vpt.right = right;
    img->m_vpt.bottom = bottom;
    img->m_vpt.clipflag = clip;

    if (img->m_vpt.left < 0) {
        img->m_vpt.left = 0;
    }
    if (img->m_vpt.top < 0) {
        img->m_vpt.top = 0;
    }
    if (img->m_vpt.right > img->m_width) {
        img->m_vpt.right = img->m_width;
    }
    if (img->m_vpt.bottom > img->m_height) {
        img->m_vpt.bottom = img->m_height;
    }

    HRGN rgn = NULL;
    if (img->m_vpt.clipflag) {
        rgn = CreateRectRgn(img->m_vpt.left, img->m_vpt.top, img->m_vpt.right, img->m_vpt.bottom);
    } else {
        rgn = CreateRectRgn(0, 0, img->m_width, img->m_height);
    }
    SelectClipRgn(img->m_hDC, rgn);
    DeleteObject(rgn);

    // OffsetViewportOrgEx(img->m_hDC, img->m_vpt.left, img->m_vpt.top, NULL);
    SetViewportOrgEx(img->m_hDC, img->m_vpt.left, img->m_vpt.top, NULL);

    CONVERT_IMAGE_END;
}

void clearviewport(PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img && img->m_hDC) {
        RECT rect = {0, 0, img->m_vpt.right - img->m_vpt.left, img->m_vpt.bottom - img->m_vpt.top};
        HBRUSH hbr = CreateSolidBrush(GetBkColor(img->m_hDC));
        FillRect(img->m_hDC, &rect, hbr);
        DeleteObject(hbr);
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

void ege_drawpoly(int numpoints, ege_point* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();

        /* 当首尾顶点为同一坐标时转成多边形，否则绘制折线 */
        if (numpoints >= 4 && polypoints[0].x == polypoints[numpoints-1].x
            && polypoints[0].y == polypoints[numpoints-1].y) {
            graphics->DrawPolygon(pen, (Gdiplus::PointF*)polypoints, numpoints - 1);
        } else {
            graphics->DrawLines(pen, (Gdiplus::PointF*)polypoints, numpoints);
        }
    }
    CONVERT_IMAGE_END;
}

void ege_drawcurve(int numpoints, ege_point* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawCurve(pen, (Gdiplus::PointF*)polypoints, numpoints);
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

void ege_bezier(int numpoints, ege_point* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (img->m_linestyle.linestyle == PS_NULL) {
            return;
        }
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Pen* pen = img->getPen();
        graphics->DrawBeziers(pen, (Gdiplus::PointF*)polypoints, numpoints);
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

void ege_setpattern_pathgradient(ege_point center,
    color_t centerColor,
    int count,
    ege_point* points,
    int colorCount,
    color_t* pointsColor,
    PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::PathGradientBrush* pbrush =
            new Gdiplus::PathGradientBrush((Gdiplus::PointF*)points, count, Gdiplus::WrapModeTile);
        pbrush->SetCenterColor(Gdiplus::Color(centerColor));
        pbrush->SetCenterPoint(Gdiplus::PointF(center.x, center.y));
        pbrush->SetSurroundColors((Gdiplus::Color*)pointsColor, &colorCount);
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
        pbrush->SetSurroundColors((Gdiplus::Color*)&color, &count);
        img->set_pattern(pbrush);
    }
    CONVERT_IMAGE_END;
}

void ege_setpattern_texture(PIMAGE srcimg, float x, float y, float w, float h, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        if (srcimg->m_texture) {
            Gdiplus::TextureBrush* pbrush =
                new Gdiplus::TextureBrush((Gdiplus::Image*)srcimg->m_texture, Gdiplus::WrapModeTile, x, y, w, h);
            img->set_pattern(pbrush);
        }
    }
    CONVERT_IMAGE_END;
}

void ege_fillpoly(int numpoints, ege_point* polypoints, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Brush* brush = img->getBrush();
        graphics->FillPolygon(brush, (Gdiplus::PointF*)polypoints, numpoints);
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

void ege_fillellipse(float x, float y, float w, float h, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Brush* brush = img->getBrush();
        graphics->FillEllipse(brush, x, y, w, h);
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

void ege_setalpha(int alpha, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_hDC) {
        int a = alpha << 24;
        int len = img->m_width * img->m_height;
        for (int i = 0; i < len; ++i) {
            DWORD c = img->m_pBuffer[i];
            img->m_pBuffer[i] = a | (c & 0xFFFFFF);
        }
    }
    CONVERT_IMAGE_END;
}

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
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
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
    if (img) {
        if (srcimg->m_texture) {
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

// TODO: 错误处理
static void ege_drawtext_p(const wchar_t* textstring, float x, float y, PIMAGE img)
{
    using namespace Gdiplus;
    Gdiplus::Graphics* graphics = img->getGraphics();

    HFONT hf = (HFONT)GetCurrentObject(img->m_hDC, OBJ_FONT);
    LOGFONT lf;
    GetObject(hf, sizeof(LOGFONT), &lf);
    if (strcmp(lf.lfFaceName, "System") == 0) {
        hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    Gdiplus::Font font(img->m_hDC, hf);
    // if (!font.IsAvailable()) {
    // 	fprintf(stderr, "!font.IsAvailable(), hf: %p\n", hf);
    // }
    Gdiplus::PointF origin(x, y);
    Gdiplus::SolidBrush brush(img->m_color);

    Gdiplus::StringFormat* format = Gdiplus::StringFormat::GenericTypographic()->Clone();

    switch(img->m_texttype.horiz) {
        case LEFT_TEXT:   format->SetAlignment(Gdiplus::StringAlignmentNear);   break;
        case CENTER_TEXT: format->SetAlignment(Gdiplus::StringAlignmentCenter); break;
        case RIGHT_TEXT:  format->SetAlignment(Gdiplus::StringAlignmentFar);    break;
    }

    if (lf.lfEscapement % 3600 != 0) {
        float angle = (float)(-lf.lfEscapement / 10.0);

        Gdiplus::Matrix matrix;
        graphics->GetTransform(&matrix);
        graphics->TranslateTransform(origin.X, origin.Y);
        graphics->RotateTransform(angle);
        graphics->DrawString(textstring, -1, &font, Gdiplus::PointF(0, 0), format,  &brush);
        graphics->SetTransform(&matrix);
    } else {
        graphics->DrawString(textstring, -1, &font, origin, format, &brush);
    }

    delete format;
    // int err;
    // if (err = graphics.DrawString(textstring, -1, &font, origin, &brush)) {
    // 	fprintf(stderr, "DrawString Err: %d\n", err);
    // }
}

void EGEAPI ege_drawtext(const char* textstring, float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_hDC) {
        int bufferSize = MultiByteToWideChar(getcodepage(), 0, textstring, -1, NULL, 0);
        if (bufferSize < 128) {
            WCHAR wStr[128];
            MultiByteToWideChar(getcodepage(), 0, textstring, -1, wStr, 128);
            ege_drawtext_p(wStr, x, y, img);
        } else {
            const std::wstring& wStr = mb2w(textstring);
            ege_drawtext_p(wStr.c_str(), x, y, img);
        }
    }
    CONVERT_IMAGE_END;
}

void EGEAPI ege_drawtext(const wchar_t* textstring, float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_hDC) {
        ege_drawtext_p(textstring, x, y, img);
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
            PixelFormat32bppARGB,
            (BYTE*)(srcimg->m_pBuffer));
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
            PixelFormat32bppARGB,
            (BYTE*)(srcimg->m_pBuffer));
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
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Matrix m;
        Gdiplus::REAL elements[6];
        graphics->GetTransform(&m);
        m.GetElements(elements);
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
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Matrix m(matrix->m11, matrix->m12, matrix->m21, matrix->m22, matrix->m31, matrix->m32);
        graphics->SetTransform(&m);
    }
    CONVERT_IMAGE_END;
}

ege_point EGEAPI ege_transform_calc(ege_point p, PIMAGE pimg) { return ege_transform_calc(p.x, p.y, pimg); }

ege_point EGEAPI ege_transform_calc(float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    ege_point p;
    if (img) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        Gdiplus::Matrix m;
        Gdiplus::REAL elements[6], m11, m12, m21, m22, m31, m32;
        graphics->GetTransform(&m);
        m.GetElements(elements);
        m11 = elements[0];
        m12 = elements[1];
        m21 = elements[2];
        m22 = elements[3];
        m31 = elements[4];
        m32 = elements[5];
        p.x = x * m11 + y * m21 + m31;
        p.y = x * m12 + y * m22 + m32;
    } else {
        p.x = 0;
        p.y = 0;
    }
    CONVERT_IMAGE_END;
    return p;
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
    line(l, b, l, t, img);
    lineto(r, t, img);
    setcolor(dc, img);
    lineto(r, b, img);
    lineto(l, b, img);
}

int inputbox_getline(const char* title, const char* text, LPSTR buf, int len)
{
    const std::wstring& title_w = mb2w(title);
    const std::wstring& text_w = mb2w(text);
    std::wstring buf_w(len, L'\0');
    int ret = inputbox_getline(title_w.c_str(), text_w.c_str(), &buf_w[0], len);
    if (ret) {
        WideCharToMultiByte(getcodepage(), 0, buf_w.c_str(), -1, buf, len, 0, 0);
    }
    return ret;
}

int inputbox_getline(const wchar_t* title, const wchar_t* text, LPWSTR buf, int len)
{
    struct _graph_setting* pg = &graph_setting;
    IMAGE bg;
    IMAGE window;
    int w = 400, h = 300, x = (getwidth() - w) / 2, y = (getheight() - h) / 2;
    int ret = 0;

    bg.getimage(0, 0, getwidth(), getheight());
    window.resize(w, h);
    buf[0] = 0;

    sys_edit edit(true);
    edit.create(true);
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
    outtextxy(3, 3, title, &window);
    setcolor(0x0, &window);

    {
        RECT rect = {30, 32, w - 30, 128 - 3};
        DrawTextW(window.m_hDC,
            text,
            -1,
            &rect,
            DT_NOPREFIX | DT_LEFT | DT_TOP | TA_NOUPDATECP | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS);
    }

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

float EGE_PRIVATE_GetFPS(int add) // 获取帧数
{
    static int fps = 0;
    static int fps_inv = 0;
    static double time = 0;
    static float flret = 0;
    static float fret = 0;
    static float fret_inv = 0;

    struct _graph_setting* pg = &graph_setting;
    double cur = get_highfeq_time_ls(pg);

    if (add == 0x100) {
        fps += 1;
    } else if (add == -0x100) {
        fps += 1;
        fps_inv += 1;
    }

    if (cur - time >= 0.5) {
        flret = fret;
        fret = (float)(fps / (cur - time));
        fret_inv = (float)((fps - fps_inv) / (cur - time));
        fps = 0;
        fps_inv = 0;
        time = cur;
    }

    if (add > 0) {
        return (fret + flret) / 2;
    } else {
        return fret_inv;
    }
}

float getfps() { return EGE_PRIVATE_GetFPS(0); }

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

} // namespace ege
