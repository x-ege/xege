#include "font.h"

#include "ege_head.h"
#include "ege_common.h"

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
static void private_textout(PIMAGE img, LPCWSTR text, int x, int y, int horiz, int vert)
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
    if (text) {
        int xOffset = 0, yOffset = 0;

        if (img->m_texttype.vert == CENTER_TEXT) {
            LOGFONTW font;
            getfont(&font, img);

            int textHeight = textheight(text, img);
            int escapement = font.lfEscapement % 3600;
            if (escapement != 0) {
                double radian = escapement / 10.0 * PI / 180.0;
                xOffset = (int)round(-textHeight * sin(radian) / 2.0);
                yOffset = (int)round(-textHeight * cos(radian) / 2.0);
            } else {
                yOffset = (int)round(-textHeight / 2.0);
            }
        }

        TextOutW(img->m_hDC, x + xOffset, y + yOffset, text, (int)lstrlenW(text));
    }
}

void outtext(LPCSTR text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    outtext(textstring_w.c_str(), pimg);
}

void outtext(LPCWSTR text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        private_textout(img, text, pt.x, pt.y, -1, -1);
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

void outtextxy(int x, int y, LPCSTR text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    outtextxy(x, y, textstring_w.c_str(), pimg);
}

void outtextxy(int x, int y, LPCWSTR text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        private_textout(img, text, x, y, -1, -1);
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

void outtextrect(int x, int y, int w, int h, LPCSTR text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    outtextrect(x, y, w, h, textstring_w.c_str(), pimg);
}

void outtextrect(int x, int y, int w, int h, LPCWSTR text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        unsigned int fmode = private_gettextmode(img);
        RECT rect = {x, y, x + w, y + h};
        DrawTextW(
            img->m_hDC, text, -1, &rect, fmode | DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS);
    }

    CONVERT_IMAGE_END;
}

// NOTE: xyprintf 和 rectprintf 的 LPCSTR 版本理论上可能出问题, 某种编码下可能出现一个字节值为 0x25, 也就是 '%',
// 导致 printf 内部处理出错. 但出这种错的机会应该极少, 故先不处理.

void xyprintf(int x, int y, LPCSTR format, ...)
{
    va_list v;
    va_start(v, format);
    {
        struct _graph_setting* pg = &graph_setting;
        char* buff = (char*)pg->g_t_buff;
        vsprintf(buff, format, v);
        outtextxy(x, y, buff);
    }
    va_end(v);
}

void xyprintf(int x, int y, LPCWSTR format, ...)
{
    va_list v;
    va_start(v, format);
    {
        struct _graph_setting* pg = &graph_setting;
        wchar_t* buff = (wchar_t*)pg->g_t_buff;
        vswprintf(buff, format, v);
        outtextxy(x, y, buff);
    }
    va_end(v);
}

void rectprintf(int x, int y, int w, int h, LPCSTR format, ...)
{
    va_list v;
    va_start(v, format);
    {
        struct _graph_setting* pg = &graph_setting;
        char* buff = (char*)pg->g_t_buff;
        vsprintf(buff, format, v);
        outtextrect(x, y, w, h, buff);
    }
    va_end(v);
}

void rectprintf(int x, int y, int w, int h, LPCWSTR format, ...)
{
    va_list v;
    va_start(v, format);
    {
        struct _graph_setting* pg = &graph_setting;
        wchar_t* buff = (wchar_t*)pg->g_t_buff;
        vswprintf(buff, format, v);
        outtextrect(x, y, w, h, buff);
    }
    va_end(v);
}

int textwidth(LPCSTR text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    return textwidth(textstring_w.c_str(), pimg);
}

int textwidth(LPCWSTR text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        SIZE sz;
        GetTextExtentPoint32W(img->m_hDC, text, (int)lstrlenW(text), &sz);
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

int textheight(LPCSTR text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    return textheight(textstring_w.c_str(), pimg);
}

int textheight(LPCWSTR text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE_CONST(pimg);
    if (img) {
        SIZE sz;
        GetTextExtentPoint32W(img->m_hDC, text, (int)lstrlenW(text), &sz);
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

void setfont(int height,
    int width,
    LPCSTR typeface,
    int  escapement,
    int  orientation,
    int  weight,
    bool italic,
    bool underline,
    bool strikeOut,
    BYTE charSet,
    BYTE outPrecision,
    BYTE clipPrecision,
    BYTE quality,
    BYTE pitchAndFamily,
    PIMAGE pimg)
{
    const std::wstring& wFace = mb2w(typeface);

    setfont(
        height,
        width,
        wFace.c_str(),
        escapement,
        orientation,
        weight,
        italic,
        underline,
        strikeOut,
        charSet,
        outPrecision,
        clipPrecision,
        quality,
        pitchAndFamily,
        pimg
    );
}

void setfont(int height,
    int width,
    LPCWSTR typeface,
    int escapement,
    int orientation,
    int weight,
    bool italic,
    bool underline,
    bool strikeOut,
    BYTE charSet,
    BYTE outPrecision,
    BYTE clipPrecision,
    BYTE quality,
    BYTE pitchAndFamily,
    PIMAGE pimg)
{
    LOGFONTW lf = {0};
    lf.lfHeight = height;
    lf.lfWidth = width;
    lf.lfEscapement = escapement;
    lf.lfOrientation = orientation;
    lf.lfWeight = weight;
    lf.lfItalic = italic;
    lf.lfUnderline = underline;
    lf.lfStrikeOut = strikeOut;
    lf.lfCharSet = charSet;
    lf.lfOutPrecision = outPrecision;
    lf.lfClipPrecision = clipPrecision;
    lf.lfQuality = quality;
    lf.lfPitchAndFamily = pitchAndFamily;
    lstrcpyW(lf.lfFaceName, typeface);

    setfont(&lf, pimg);
}

void setfont(int height,
    int width,
    LPCSTR typeface,
    int escapement,
    int orientation,
    int weight,
    bool italic,
    bool underline,
    bool strikeOut,
    PIMAGE pimg)
{
    setfont(height,
        width,
        typeface,
        escapement,
        orientation,
        weight,
        italic,
        underline,
        strikeOut,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        pimg);
}

void setfont(int height,
    int width,
    LPCWSTR typeface,
    int escapement,
    int orientation,
    int weight,
    bool italic,
    bool underline,
    bool strikeOut,
    PIMAGE pimg)
{
    setfont(height,
        width,
        typeface,
        escapement,
        orientation,
        weight,
        italic,
        underline,
        strikeOut,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH,
        pimg);
}

void setfont(int height, int width, LPCSTR typeface, PIMAGE pimg)
{
    setfont(height,
        width,
        typeface,
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

void setfont(int height, int width, LPCWSTR typeface, PIMAGE pimg)
{
    setfont(height,
        width,
        typeface,
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

// NOTE: 按照 EGE 的 codepage 来转换 LOGFONTA::lfFaceName 似乎不太合规, 所以这里保留了原行为没有修改.
// 是否完全删除两个 LOGFONTA 版本的函数?

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
