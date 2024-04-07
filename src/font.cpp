#include "font.h"

#include "ege_head.h"

namespace ege
{


/* private function */
static unsigned int private_gettextmode(PIMAGE img)
{
    UINT fMode = TA_NOUPDATECP; // TA_UPDATECP;
    if (img->m_texttype.horiz == RIGHT_TEXT) {
        fMode |= TA_RIGHT;
    } else if (img->m_texttype.horiz == CENTER_TEXT) {
        fMode |= TA_CENTER;
    } else {
        fMode |= TA_LEFT;
    }
    if (img->m_texttype.vert == BOTTOM_TEXT) {
        fMode |= TA_BOTTOM;
    } else {
        fMode |= TA_TOP;
    }
    return fMode;
}

/* private function */
static void private_textout(PIMAGE img, LPCSTR textstring, int x, int y, int horiz, int vert)
{
    if (horiz >= 0 && vert >= 0) {
        UINT fMode = TA_NOUPDATECP; // TA_UPDATECP;
        img->m_texttype.horiz = horiz;
        img->m_texttype.vert = vert;
        if (img->m_texttype.horiz == RIGHT_TEXT) {
            fMode |= TA_RIGHT;
        } else if (img->m_texttype.horiz == CENTER_TEXT) {
            fMode |= TA_CENTER;
        } else {
            fMode |= TA_LEFT;
        }
        if (img->m_texttype.vert == BOTTOM_TEXT) {
            fMode |= TA_BOTTOM;
        } else {
            fMode |= TA_TOP;
        }
        SetTextAlign(img->m_hDC, fMode);
    } else {
        SetTextAlign(img->m_hDC, private_gettextmode(img));
    }

    if (textstring) {
        int xOffset = 0, yOffset = 0;

        if (img->m_texttype.vert == CENTER_TEXT) {
            LOGFONT font;
            getfont(&font, img);

            int textHeight = textheight(textstring, img);
            int escapement = font.lfEscapement % 3600;
            if (escapement != 0) {
                double radian = escapement / 10.0 * PI / 180.0;
                xOffset = (int)round(-textHeight * sin(radian) / 2.0);
                yOffset = (int)round(-textHeight * cos(radian) / 2.0);
            } else {
                yOffset = (int)round(-textHeight / 2.0);
            }
        }

        TextOutA(img->m_hDC, x + xOffset, y + yOffset, textstring, (int)strlen(textstring));
    }
}

/* private function */
static void private_textout(PIMAGE img, LPCWSTR textstring, int x, int y, int horiz, int vert)
{
    if (horiz >= 0 && vert >= 0) {
        UINT fMode = TA_NOUPDATECP; // TA_UPDATECP;
        img->m_texttype.horiz = horiz;
        img->m_texttype.vert = vert;
        if (img->m_texttype.horiz == RIGHT_TEXT) {
            fMode |= TA_RIGHT;
        } else if (img->m_texttype.horiz == CENTER_TEXT) {
            fMode |= TA_CENTER;
        } else {
            fMode |= TA_LEFT;
        }
        if (img->m_texttype.vert == BOTTOM_TEXT) {
            fMode |= TA_BOTTOM;
        } else {
            fMode |= TA_TOP;
        }
        SetTextAlign(img->m_hDC, fMode);
    } else {
        SetTextAlign(img->m_hDC, private_gettextmode(img));
    }
    if (textstring) {
        int xOffset = 0, yOffset = 0;

        if (img->m_texttype.vert == CENTER_TEXT) {
            LOGFONT font;
            getfont(&font, img);

            int textHeight = textheight(textstring, img);
            int escapement = font.lfEscapement % 3600;
            if (escapement != 0) {
                double radian = escapement / 10.0 * PI / 180.0;
                xOffset = (int)round(-textHeight * sin(radian) / 2.0);
                yOffset = (int)round(-textHeight * cos(radian) / 2.0);
            } else {
                yOffset = (int)round(-textHeight / 2.0);
            }
        }

        TextOutW(img->m_hDC, x + xOffset, y + yOffset, textstring, (int)lstrlenW(textstring));
    }
}

void outtext(LPCSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        private_textout(img, textstring, pt.x, pt.y, -1, -1);
    }
    CONVERT_IMAGE_END;
}

void outtext(LPCWSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        private_textout(img, textstring, pt.x, pt.y, -1, -1);
    }
    CONVERT_IMAGE_END;
}

void outtext(CHAR c, PIMAGE pimg)
{
    CHAR str[10] = {c};
    outtext(str, pimg);
}

void outtext(WCHAR c, PIMAGE pimg)
{
    WCHAR str[10] = {c};
    outtext(str, pimg);
}

void outtextxy(int x, int y, LPCSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        private_textout(img, textstring, x, y, -1, -1);
    }
    CONVERT_IMAGE_END;
}

void outtextxy(int x, int y, LPCWSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        private_textout(img, textstring, x, y, -1, -1);
    }
    CONVERT_IMAGE_END;
}

void outtextxy(int x, int y, CHAR c, PIMAGE pimg)
{
    CHAR str[10] = {c};
    outtextxy(x, y, str, pimg);
}

void outtextxy(int x, int y, WCHAR c, PIMAGE pimg)
{
    WCHAR str[10] = {c};
    outtextxy(x, y, str, pimg);
}

void outtextrect(int x, int y, int w, int h, LPCSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        unsigned int fmode = private_gettextmode(img);
        RECT rect = {x, y, x + w, y + h};
        DrawTextA(
            img->m_hDC, textstring, -1, &rect, fmode | DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS);
    }

    CONVERT_IMAGE_END;
}

void outtextrect(int x, int y, int w, int h, LPCWSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        unsigned int fmode = private_gettextmode(img);
        RECT rect = {x, y, x + w, y + h};
        DrawTextW(
            img->m_hDC, textstring, -1, &rect, fmode | DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS);
    }

    CONVERT_IMAGE_END;
}

void xyprintf(int x, int y, LPCSTR fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    {
        struct _graph_setting* pg = &graph_setting;
        char* buff = (char*)pg->g_t_buff;
        vsprintf(buff, fmt, v);
        outtextxy(x, y, buff);
    }
    va_end(v);
}

void xyprintf(int x, int y, LPCWSTR fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    {
        struct _graph_setting* pg = &graph_setting;
        wchar_t* buff = (wchar_t*)pg->g_t_buff;
        vswprintf(buff, fmt, v);
        outtextxy(x, y, buff);
    }
    va_end(v);
}

void rectprintf(int x, int y, int w, int h, LPCSTR fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    {
        struct _graph_setting* pg = &graph_setting;
        char* buff = (char*)pg->g_t_buff;
        vsprintf(buff, fmt, v);
        outtextrect(x, y, w, h, buff);
    }
    va_end(v);
}

void rectprintf(int x, int y, int w, int h, LPCWSTR fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    {
        struct _graph_setting* pg = &graph_setting;
        wchar_t* buff = (wchar_t*)pg->g_t_buff;
        vswprintf(buff, fmt, v);
        outtextrect(x, y, w, h, buff);
    }
    va_end(v);
}

int textwidth(LPCSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        SIZE sz;
        GetTextExtentPoint32A(img->m_hDC, textstring, (int)strlen(textstring), &sz);
        CONVERT_IMAGE_END;
        return sz.cx;
    }
    CONVERT_IMAGE_END;
    return 0;
}

int textwidth(LPCWSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        SIZE sz;
        GetTextExtentPoint32W(img->m_hDC, textstring, (int)lstrlenW(textstring), &sz);
        CONVERT_IMAGE_END;
        return sz.cx;
    }
    CONVERT_IMAGE_END;
    return 0;
}

int textwidth(CHAR c, PIMAGE pimg)
{
    CHAR str[2] = {c};
    return textwidth(str, pimg);
}

int textwidth(WCHAR c, PIMAGE pimg)
{
    WCHAR str[2] = {c};
    return textwidth(str, pimg);
}

int textheight(LPCSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        SIZE sz;
        GetTextExtentPoint32A(img->m_hDC, textstring, (int)strlen(textstring), &sz);
        CONVERT_IMAGE_END;
        return sz.cy;
    }
    CONVERT_IMAGE_END;
    return 0;
}

int textheight(LPCWSTR textstring, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        SIZE sz;
        GetTextExtentPoint32W(img->m_hDC, textstring, (int)lstrlenW(textstring), &sz);
        CONVERT_IMAGE_END;
        return sz.cy;
    }
    CONVERT_IMAGE_END;
    return 0;
}

int textheight(CHAR c, PIMAGE pimg)
{
    CHAR str[2] = {c};
    return textheight(str, pimg);
}

int textheight(WCHAR c, PIMAGE pimg)
{
    WCHAR str[2] = {c};
    return textheight(str, pimg);
}

void settextjustify(int horiz, int vert, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        img->m_texttype.horiz = horiz;
        img->m_texttype.vert = vert;
    }
    CONVERT_IMAGE_END;
}

void setfont(int nHeight,
    int nWidth,
    LPCSTR lpszFace,
    int nEscapement,
    int nOrientation,
    int nWeight,
    int bItalic,
    int bUnderline,
    int bStrikeOut,
    BYTE fbCharSet,
    BYTE fbOutPrecision,
    BYTE fbClipPrecision,
    BYTE fbQuality,
    BYTE fbPitchAndFamily,
    PIMAGE pimg)
{
    LOGFONTA lf = {0};
    lf.lfHeight = nHeight;
    lf.lfWidth = nWidth;
    lf.lfEscapement = nEscapement;
    lf.lfOrientation = nOrientation;
    lf.lfWeight = nWeight;
    lf.lfItalic = (bItalic != 0);
    lf.lfUnderline = (bUnderline != 0);
    lf.lfStrikeOut = (bStrikeOut != 0);
    lf.lfCharSet = fbCharSet;
    lf.lfOutPrecision = fbOutPrecision;
    lf.lfClipPrecision = fbClipPrecision;
    lf.lfQuality = fbQuality;
    lf.lfPitchAndFamily = fbPitchAndFamily;
    lstrcpyA(lf.lfFaceName, lpszFace);

    setfont(&lf, pimg);
}

void setfont(int nHeight,
    int nWidth,
    LPCWSTR lpszFace,
    int nEscapement,
    int nOrientation,
    int nWeight,
    int bItalic,
    int bUnderline,
    int bStrikeOut,
    BYTE fbCharSet,
    BYTE fbOutPrecision,
    BYTE fbClipPrecision,
    BYTE fbQuality,
    BYTE fbPitchAndFamily,
    PIMAGE pimg)
{
    LOGFONTW lf = {0};
    lf.lfHeight = nHeight;
    lf.lfWidth = nWidth;
    lf.lfEscapement = nEscapement;
    lf.lfOrientation = nOrientation;
    lf.lfWeight = nWeight;
    lf.lfItalic = (bItalic != 0);
    lf.lfUnderline = (bUnderline != 0);
    lf.lfStrikeOut = (bStrikeOut != 0);
    lf.lfCharSet = fbCharSet;
    lf.lfOutPrecision = fbOutPrecision;
    lf.lfClipPrecision = fbClipPrecision;
    lf.lfQuality = fbQuality;
    lf.lfPitchAndFamily = fbPitchAndFamily;
    lstrcpyW(lf.lfFaceName, lpszFace);

    setfont(&lf, pimg);
}

void setfont(int nHeight,
    int nWidth,
    LPCSTR lpszFace,
    int nEscapement,
    int nOrientation,
    int nWeight,
    int bItalic,
    int bUnderline,
    int bStrikeOut,
    PIMAGE pimg)
{
    setfont(nHeight,
        nWidth,
        lpszFace,
        nEscapement,
        nOrientation,
        nWeight,
        bItalic,
        bUnderline,
        bStrikeOut,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        pimg);
}

void setfont(int nHeight,
    int nWidth,
    LPCWSTR lpszFace,
    int nEscapement,
    int nOrientation,
    int nWeight,
    int bItalic,
    int bUnderline,
    int bStrikeOut,
    PIMAGE pimg)
{
    setfont(nHeight,
        nWidth,
        lpszFace,
        nEscapement,
        nOrientation,
        nWeight,
        bItalic,
        bUnderline,
        bStrikeOut,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        pimg);
}

void setfont(int nHeight, int nWidth, LPCSTR lpszFace, PIMAGE pimg)
{
    setfont(nHeight,
        nWidth,
        lpszFace,
        0,
        0,
        FW_DONTCARE,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        pimg);
}

void setfont(int nHeight, int nWidth, LPCWSTR lpszFace, PIMAGE pimg)
{
    setfont(nHeight,
        nWidth,
        lpszFace,
        0,
        0,
        FW_DONTCARE,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        pimg);
}

void setfont(const LOGFONTA* font, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        HFONT hfont = CreateFontIndirectA(font);
        DeleteObject(SelectObject(img->m_hDC, hfont));
    }
    CONVERT_IMAGE_END;
}

void setfont(const LOGFONTW* font, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        HFONT hfont = CreateFontIndirectW(font);
        DeleteObject(SelectObject(img->m_hDC, hfont));
    }
    CONVERT_IMAGE_END;
}

void getfont(LOGFONTA* font, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        HFONT hf = (HFONT)GetCurrentObject(img->m_hDC, OBJ_FONT);
        GetObjectA(hf, sizeof(LOGFONTA), font);
    }
    CONVERT_IMAGE_END;
}

void getfont(LOGFONTW* font, PCIMAGE pimg)
{
    PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        HFONT hf = (HFONT)GetCurrentObject(img->m_hDC, OBJ_FONT);
        GetObjectW(hf, sizeof(LOGFONTW), font);
    }
    CONVERT_IMAGE_END;
}


} // namespace ege
