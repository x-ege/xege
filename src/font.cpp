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

static UINT horizontalAlignToDrawTextFormat(int horizontalAlign)
{
    UINT format = 0;
    switch (horizontalAlign) {
    case LEFT_TEXT:    format |= DT_LEFT;   break;
    case CENTER_TEXT:  format |= DT_CENTER; break;
    case RIGHT_TEXT:   format |= DT_RIGHT;  break;
    }

    return format;
}

/* private function */
static void private_textout(PIMAGE img, const wchar_t* text, int x, int y, int horiz, int vert)
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

void outtext(const char* text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    outtext(textstring_w.c_str(), pimg);
}

void outtext(const wchar_t* text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        POINT pt;
        GetCurrentPositionEx(img->m_hDC, &pt);
        private_textout(img, text, pt.x, pt.y, -1, -1);
    }
    CONVERT_IMAGE_END;
}

void outtext(char c, PIMAGE pimg)
{
    char str[10] = {c};
    outtext(str, pimg);
}

void outtext(wchar_t c, PIMAGE pimg)
{
    wchar_t str[10] = {c};
    outtext(str, pimg);
}

void outtextxy(int x, int y, const char* text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    outtextxy(x, y, textstring_w.c_str(), pimg);
}

void outtextxy(int x, int y, const wchar_t* text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        private_textout(img, text, x, y, -1, -1);
    }
    CONVERT_IMAGE_END;
}

void outtextxy(int x, int y, char c, PIMAGE pimg)
{
    char str[10] = {c};
    outtextxy(x, y, str, pimg);
}

void outtextxy(int x, int y, wchar_t c, PIMAGE pimg)
{
    wchar_t str[10] = {c};
    outtextxy(x, y, str, pimg);
}

void outtextrect(int x, int y, int w, int h, const char* text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    outtextrect(x, y, w, h, textstring_w.c_str(), pimg);
}

void outtextrect(int x, int y, int w, int h, const wchar_t* text, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);

    if (img) {
        if ((text == NULL) || (w <= 0) || (h <= 0)) {
            return;
        }

        // DrawTextW 要求必须设置的三个对齐标志
        UINT textAlignMode = GetTextAlign(img->m_hDC);
        SetTextAlign(img->m_hDC, TA_TOP | TA_LEFT | TA_NOUPDATECP);

        UINT format = 0;
        format |= horizontalAlignToDrawTextFormat(img->m_texttype.horiz);
        format |= DT_NOPREFIX | DT_WORDBREAK | DT_EDITCONTROL | DT_EXPANDTABS;

        RECT rect = {x, y, x + w, y + h};

        // 原裁剪区域
        HRGN oldClicRgn = NULL;
        int  oldClicRegionStatus = 0;
        bool needRestoreClipRegion = false;

        int topOffset = 0;

        // 通过垂直方向上的偏移实现对齐
        if (img->m_texttype.vert != TOP_TEXT) {
            // 测量实际输出时的文本区域
            RECT measureRect = rect;
            int textHeight = DrawTextW(img->m_hDC, text, -1, &measureRect, format | DT_CALCRECT);

            int heightDiff = rect.bottom - measureRect.bottom;

            // 根据文本对齐方式偏移
            if(img->m_texttype.vert == BOTTOM_TEXT) {
                topOffset = heightDiff;
            } else if (img->m_texttype.vert == CENTER_TEXT) {
                topOffset = heightDiff / 2;
            }

            // 文本输出区域向上偏移，通过创建裁剪区域交集保持原来的文本框裁剪范围
            if (topOffset < 0) {
                // 记录原来的裁剪区域
                needRestoreClipRegion = true;
                oldClicRgn =  CreateRectRgnIndirect(&rect);
                oldClicRegionStatus = GetClipRgn(img->m_hDC, oldClicRgn);

                IntersectClipRect(img->m_hDC, rect.left, rect.top, rect.right, rect.bottom);
            }
        }

        rect.top += topOffset;

        DrawTextW(img->m_hDC, text, -1, &rect, format);

        // 恢复文本对齐方式
        SetTextAlign(img->m_hDC, textAlignMode);

        // 恢复裁剪区域
        if (needRestoreClipRegion) {
            if (oldClicRegionStatus == 0) {
                SelectClipRgn(img->m_hDC, NULL);
            } else if (oldClicRegionStatus == 1) {
                SelectClipRgn(img->m_hDC, oldClicRgn);
            } else {
                HRGN rgn = NULL;
                if (img->m_vpt.clipflag) {
                    rgn = CreateRectRgn(img->m_vpt.left, img->m_vpt.top, img->m_vpt.right, img->m_vpt.bottom);
                } else {
                    rgn = CreateRectRgn(0, 0, img->m_width, img->m_height);
                }
                SelectClipRgn(img->m_hDC, rgn);
                DeleteObject(rgn);
            }

            if (oldClicRgn != NULL) {
                DeleteObject(oldClicRgn);
            }
        }

    }

    CONVERT_IMAGE_END;
}

// NOTE: xyprintf 和 rectprintf 的 const char* 版本理论上可能出问题, 某种编码下可能出现一个字节值为 0x25, 也就是 '%',
// 导致 printf 内部处理出错. 但出这种错的机会应该极少, 故先不处理.

void xyprintf(int x, int y, const char* format, ...)
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

void xyprintf(int x, int y, const wchar_t* format, ...)
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

void rectprintf(int x, int y, int w, int h, const char* format, ...)
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

void rectprintf(int x, int y, int w, int h, const wchar_t* format, ...)
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

int textwidth(const char* text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    return textwidth(textstring_w.c_str(), pimg);
}

int textwidth(const wchar_t* text, PIMAGE pimg)
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

int textwidth(char c, PIMAGE pimg)
{
    char str[2] = {c};
    return textwidth(str, pimg);
}

int textwidth(wchar_t c, PIMAGE pimg)
{
    wchar_t str[2] = {c};
    return textwidth(str, pimg);
}

int textheight(const char* text, PIMAGE pimg)
{
    const std::wstring& textstring_w = mb2w(text);
    return textheight(textstring_w.c_str(), pimg);
}

int textheight(const wchar_t* text, PIMAGE pimg)
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

int textheight(wchar_t c, PIMAGE pimg)
{
    wchar_t str[2] = {c};
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
    const char* typeface,
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
    const wchar_t* typeface,
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
    const char* typeface,
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
    const wchar_t* typeface,
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

void setfont(int height, int width, const char* typeface, PIMAGE pimg)
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

void setfont(int height, int width, const wchar_t* typeface, PIMAGE pimg)
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

// TODO: 错误处理
static void ege_drawtext_p(const wchar_t* textstring, float x, float y,  PIMAGE img)
{
    using namespace Gdiplus;
    Gdiplus::Graphics* graphics = img->getGraphics();

    HFONT hf = (HFONT)GetCurrentObject(img->m_hDC, OBJ_FONT);
    LOGFONTW lf;
    GetObjectW(hf, sizeof(LOGFONTW), &lf);
    if (wcscmp(lf.lfFaceName, L"System") == 0) {
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

    float xScale = 1.0f, angle = 0.0f;

    if (lf.lfWidth != 0) {
        LONG fixedWidth = lf.lfWidth;

        int textCurrentWidth = textwidth(textstring);
        lf.lfWidth = 0;
        setfont(&lf);
        int textNormalWidth = textwidth(textstring);
        lf.lfWidth = fixedWidth;
        setfont(&lf);

        if (textCurrentWidth != textNormalWidth && (textCurrentWidth != 0) && (textNormalWidth != 0)) {
            xScale = (float)((double)textCurrentWidth / textNormalWidth);
        }
    }

    if (lf.lfEscapement % 3600 != 0) {
        angle = (float)(-lf.lfEscapement / 10.0);
    }

    if ((xScale != 1.0) || (angle != 0.0f)) {
        Gdiplus::Matrix oldTransformMatrix;
        graphics->GetTransform(&oldTransformMatrix);
        graphics->TranslateTransform(origin.X, origin.Y);

        if (angle != 0.0f) {
            graphics->RotateTransform(angle);
        }

        if (xScale != 1.0f) {
            graphics->ScaleTransform(xScale, 1.0f);
        }

        graphics->DrawString(textstring, -1, &font, Gdiplus::PointF(0, 0), format, &brush);
        graphics->SetTransform(&oldTransformMatrix);
    } else {
        graphics->DrawString(textstring, -1, &font, origin, format, &brush);
    }

    delete format;
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

} // namespace ege
