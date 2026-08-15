#ifndef EGE_SYS_EDIT_H
#define EGE_SYS_EDIT_H

#include "egecontrolbase.h"

#define EGE_CONVERT_TO_WSTR_WITH(mbStr, block)                                               \
    {                                                                                        \
        int    bufsize = ::MultiByteToWideChar(::ege::getcodepage(), 0, mbStr, -1, NULL, 0); \
        if (bufsize > 0) {                                                                   \
            WCHAR* wStr = new WCHAR[bufsize];                                                \
            if (::MultiByteToWideChar(                                                       \
                    ::ege::getcodepage(), 0, mbStr, -1, &wStr[0], bufsize) > 0) {             \
                block                                                                        \
            }                                                                                \
            delete[] wStr;                                                                   \
        }                                                                                    \
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
        if (m_hwnd && !destroy()) {
            return grError;
        }

        msg_createwindow msg = {NULL};
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

        HWND parentWindow = getHWnd();
        if (isWindowOwnedByCurrentThread(parentWindow)) {
            // A same-thread owner can create the native child directly;
            // cross-thread legacy windows still use the private message path.
            msg.hwnd = ::CreateWindowExW(msg.exstyle, msg.classname, NULL,
                msg.style, 0, 0, 0, 0, parentWindow, (HMENU)msg.id,
                getHInstance(), NULL);
        } else {
            msg.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
            if (!msg.hEvent ||
                !::PostMessageW(parentWindow, WM_USER + 1, 1, (LPARAM)&msg)) {
                if (msg.hEvent) ::CloseHandle(msg.hEvent);
                return grError;
            }
            ::WaitForSingleObject(msg.hEvent, INFINITE);
        }

        m_hwnd    = msg.hwnd;
        m_hFont   = NULL;
        m_hBrush  = NULL;
        m_color   = 0x0;
        m_bgcolor = 0xFFFFFF;

        if (!m_hwnd) {
            if (msg.hEvent) ::CloseHandle(msg.hEvent);
            return grError;
        }

        ::SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
        m_callback = ::GetWindowLongPtrW(m_hwnd, GWLP_WNDPROC);
        ::SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, (LONG_PTR)getProcfunc());
        {
            WCHAR fontname[] = L"SimSun";
            setfont(12, 6, fontname);
        }
        visible(false);

        if (msg.hEvent) ::CloseHandle(msg.hEvent);
#else
        // sys_edit wraps a native Win32 EDIT child. Reporting success here
        // would leave Unix callers waiting on a control that does not exist.
        (void)multiline;
        (void)scrollbar;
        return grError;
#endif
        return grOk;
    }

    int destroy()
    {
#ifdef _WIN32
        if (m_hwnd) {
            visible(false);
            ::SendMessage(m_hwnd, WM_SETFONT, 0, 0);
            ::DeleteObject(m_hFont);
            m_hFont = NULL;
            bool destroyed = false;
            if (isWindowOwnedByCurrentThread(m_hwnd)) {
                destroyed = ::DestroyWindow(m_hwnd) != FALSE;
            } else {
                msg_createwindow msg = {NULL};
                msg.hwnd             = m_hwnd;
                msg.hEvent           = ::CreateEvent(NULL, TRUE, FALSE, NULL);
                if (msg.hEvent &&
                    ::PostMessageW(getHWnd(), WM_USER + 1, 0, (LPARAM)&msg)) {
                    ::WaitForSingleObject(msg.hEvent, INFINITE);
                    destroyed = !::IsWindow(m_hwnd);
                }
                if (msg.hEvent) ::CloseHandle(msg.hEvent);
            }
            if (!destroyed) return 0;
            if (m_hBrush) {
                ::DeleteObject(m_hBrush);
                m_hBrush = NULL;
            }
            m_hwnd = NULL;
            return 1;
        }
#endif
        return 0;
    }

	// implement in egegapi.cpp to use the ARGBTOZBGR macro definition from color.h
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
        lstrcpynW(lf.lfFaceName, fontface, LF_FACESIZE);
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
        if (isWindowOwnedByCurrentThread(m_hwnd)) {
            ::SetFocus(m_hwnd);
            return;
        }
        msg_createwindow msg = {NULL};
        msg.hwnd             = m_hwnd;
        msg.hEvent           = ::CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!msg.hEvent) return;
        if (::PostMessageW(getHWnd(), WM_USER + 2, 0, (LPARAM)&msg)) {
            ::WaitForSingleObject(msg.hEvent, INFINITE);
        }
        ::CloseHandle(msg.hEvent);
#endif
    }
	
	void select(int start = 0, int end = -1)
	{
#ifdef _WIN32
		setfocus();
		::SendMessageW(m_hwnd, EM_SETSEL, (WPARAM)start, (LPARAM)end);
#else
		(void)start;
		(void)end;
#endif
	}
	
	void setborder(bool border)
	{
#ifdef _WIN32
		DWORD style = ::GetWindowLongW(m_hwnd, GWL_STYLE);
		DWORD exstyle = ::GetWindowLongW(m_hwnd, GWL_EXSTYLE);
		
		if (border) {
			style |= WS_BORDER;
			exstyle |= WS_EX_CLIENTEDGE;
		} else {
			style &= ~(WS_BORDER);
			exstyle &= ~(WS_EX_CLIENTEDGE);
		}
		
		::SetWindowLongW(m_hwnd, GWL_STYLE, style);
		::SetWindowLongW(m_hwnd, GWL_EXSTYLE, exstyle);
		
		::SetWindowPos(m_hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		::RedrawWindow(m_hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
#else
		(void)border;
#endif
	}

protected:
#ifdef _WIN32
    static bool isWindowOwnedByCurrentThread(HWND window)
    {
        return window != NULL &&
            ::GetWindowThreadProcessId(window, NULL) == ::GetCurrentThreadId();
    }
#endif

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
