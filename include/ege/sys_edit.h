#ifndef EGE_SYS_EDIT_H
#define EGE_SYS_EDIT_H

#include "egecontrolbase.h"

#define EGE_CONVERT_TO_WSTR_WITH(mbStr, block)                                               \
    {                                                                                        \
        int    bufsize = ::MultiByteToWideChar(::ege::getcodepage(), 0, mbStr, -1, NULL, 0); \
        WCHAR* wStr    = new WCHAR[bufsize];                                                 \
        ::MultiByteToWideChar(::ege::getcodepage(), 0, mbStr, -1, &wStr[0], bufsize);        \
        block delete wStr;                                                                   \
    }

namespace ege
{

class sys_edit : public egeControlBase
{
public:
    CTL_PREINIT(sys_edit, egeControlBase)
    {
        // do sth. before sub objects' construct function call
    }

    CTL_PREINITEND;

    sys_edit(CTL_DEFPARAM) : CTL_INITBASE(egeControlBase)
    {
        CTL_INIT; // must be the first linef
        directdraw(true);
        m_hwnd = NULL;
    }

    ~sys_edit() { destroy(); }

    int create(bool multiline = false, int scrollbar = 2)
    {
#ifdef _WIN32
        if (m_hwnd) {
            destroy();
        }

        msg_createwindow msg = {NULL};
        msg.hEvent           = ::CreateEvent(NULL, TRUE, FALSE, NULL);
        msg.classname        = L"EDIT";
        msg.id               = egeControlBase::allocId();
        msg.style            = WS_CHILD | WS_BORDER | ES_LEFT | ES_WANTRETURN;

        if (multiline) {
            msg.style |= ES_MULTILINE | WS_VSCROLL;
        } else {
            msg.style |= ES_AUTOHSCROLL;
        }

        msg.exstyle = WS_EX_CLIENTEDGE; // | WS_EX_STATICEDGE;
        msg.param   = this;

        ::PostMessageW(getHWnd(), WM_USER + 1, 1, (LPARAM)&msg);
        ::WaitForSingleObject(msg.hEvent, INFINITE);

        m_hwnd    = msg.hwnd;
        m_hFont   = NULL;
        m_hBrush  = NULL;
        m_color   = 0x0;
        m_bgcolor = 0xFFFFFF;

        ::SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
        m_callback = ::GetWindowLongPtrW(m_hwnd, GWLP_WNDPROC);
        ::SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, (LONG_PTR)getProcfunc());
        {
            WCHAR fontname[] = L"SimSun";
            setfont(12, 6, fontname);
        }
        visible(false);

        ::CloseHandle(msg.hEvent);
#endif
        return 0;
    }

    int destroy()
    {
#ifdef _WIN32
        if (m_hwnd) {
            visible(false);
            msg_createwindow msg = {NULL};
            msg.hwnd             = m_hwnd;
            msg.hEvent           = ::CreateEvent(NULL, TRUE, FALSE, NULL);
            ::SendMessage(m_hwnd, WM_SETFONT, 0, 0);
            ::DeleteObject(m_hFont);
            ::PostMessageW(getHWnd(), WM_USER + 1, 0, (LPARAM)&msg);
            ::WaitForSingleObject(msg.hEvent, INFINITE);
            ::CloseHandle(msg.hEvent);
            if (m_hBrush) {
                ::DeleteObject(m_hBrush);
            }
            m_hwnd = NULL;
            return 1;
        }
#endif
        return 0;
    }

    LRESULT onMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void visible(bool bvisible)
    {
        egeControlBase::visible(bvisible);
#ifdef _WIN32
        ::ShowWindow(m_hwnd, (int)bvisible);
#endif
    }

    void setfont(int h, int w, LPCSTR fontface)
    {
#ifdef _WIN32
        EGE_CONVERT_TO_WSTR_WITH(fontface, { setfont(h, w, wStr); });
#endif
    }

    void setfont(int h, int w, LPCWSTR fontface)
    {
#ifdef _WIN32
        LOGFONTW lf         = {0};
        lf.lfHeight         = h;
        lf.lfWidth          = w;
        lf.lfEscapement     = 0;
        lf.lfOrientation    = 0;
        lf.lfWeight         = FW_DONTCARE;
        lf.lfItalic         = 0;
        lf.lfUnderline      = 0;
        lf.lfStrikeOut      = 0;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        lf.lfQuality        = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH;
        lstrcpyW(lf.lfFaceName, fontface);
        HFONT hFont = CreateFontIndirectW(&lf);
        if (hFont) {
            ::SendMessageW(m_hwnd, WM_SETFONT, (WPARAM)hFont, 0);
            ::DeleteObject(m_hFont);
            m_hFont = hFont;
        }
#endif
    }

    void move(int x, int y)
    {
        egeControlBase::move(x, y);
#ifdef _WIN32
        ::MoveWindow(m_hwnd, m_x, m_y, m_w, m_h, TRUE);
#endif
    }

    void size(int w, int h)
    {
        egeControlBase::size(w, h);
#ifdef _WIN32
        ::MoveWindow(m_hwnd, m_x, m_y, m_w, m_h, TRUE);
#endif
    }

    void settext(LPCSTR text)
    {
#ifdef _WIN32
        EGE_CONVERT_TO_WSTR_WITH(text, { settext(wStr); });
#endif
    }

    void settext(LPCWSTR text) { 
#ifdef _WIN32
        ::SendMessageW(m_hwnd, WM_SETTEXT, 0, (LPARAM)text); 
#endif
    }

    void gettext(int maxlen, LPSTR text) { 
#ifdef _WIN32
        ::SendMessageA(m_hwnd, WM_GETTEXT, (WPARAM)maxlen, (LPARAM)text); 
#endif
    }

    void gettext(int maxlen, LPWSTR text) { 
#ifdef _WIN32
        ::SendMessageW(m_hwnd, WM_GETTEXT, (WPARAM)maxlen, (LPARAM)text); 
#endif
    }

    void setmaxlen(int maxlen) { 
#ifdef _WIN32
        ::SendMessageW(m_hwnd, EM_LIMITTEXT, (WPARAM)maxlen, 0); 
#endif
    }

    void setcolor(color_t color)
    {
        m_color = color;
#ifdef _WIN32
        ::InvalidateRect(m_hwnd, NULL, TRUE);
#endif
    }

    void setbgcolor(color_t bgcolor)
    {
        m_bgcolor = bgcolor;
        //::RedrawWindow(m_hwnd, NULL, NULL, RDW_INVALIDATE);
#ifdef _WIN32
        ::InvalidateRect(m_hwnd, NULL, TRUE);
#endif
    }

    void setreadonly(bool readonly)
    {
#ifdef _WIN32
        ::SendMessageW(m_hwnd, EM_SETREADONLY, (WPARAM)readonly, 0);
        ::InvalidateRect(m_hwnd, NULL, TRUE);
#endif
    }

    void setfocus()
    {
#ifdef _WIN32
        msg_createwindow msg = {NULL};
        msg.hwnd             = m_hwnd;
        msg.hEvent           = ::CreateEvent(NULL, TRUE, FALSE, NULL);
        ::PostMessageW(getHWnd(), WM_USER + 2, 0, (LPARAM)&msg);
        ::WaitForSingleObject(msg.hEvent, INFINITE);
#endif
    }

protected:
    HWND     m_hwnd;
    HFONT    m_hFont;
    HBRUSH   m_hBrush;
    color_t  m_color;
    color_t  m_bgcolor;
    LONG_PTR m_callback;
};

#undef EGE_CONVERT_TO_WSTR_WITH

} // namespace ege
#endif /*EGE_SYS_EDIT_H*/
