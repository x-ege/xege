/*
* EGE (Easy Graphics Engine)
* FileName      graphics.cpp
* HomePage1     http://misakamm.github.com/xege
* HomePage2     http://misakamm.bitbucket.org/index.htm
* teiba1        http://tieba.baidu.com/f?kw=ege
* teiba2        http://tieba.baidu.com/f?kw=ege%C4%EF
* Blog:         http://misakamm.com
* E-Mail:       mailto:misakamm[at gmail com]

编译说明：编译为动态库时，需要定义 PNG_BULIDDLL，以导出dll函数

本图形库创建时间2010 0916

本文件定义平台密切相关的操作及接口
*/

// 整个项目和其他源文件中不需要定义 UNICODE 宏, 这里是为了解决 VC6 下 initicon 中代码的编译问题加的
#define UNICODE 1

#ifndef _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
#define _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
#endif
#ifndef _ALLOW_RUNTIME_LIBRARY_MISMATCH
#define _ALLOW_RUNTIME_LIBRARY_MISMATCH
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifndef _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_NON_CONFORMING_SWPRINTFS
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windowsx.h>
#endif

#include "ege_head.h"
#include "ege_common.h"
#include "ege_extension.h"
#include "window.h"
#if defined(EGE_BACKEND_COREGRAPHICS)
#include "backend/macos/MacWindow.h"
#endif

#ifdef _ITERATOR_DEBUG_LEVEL
#undef _ITERATOR_DEBUG_LEVEL
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4100 4127 4706)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "Imm32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Winmm.lib")

#ifdef EGE_GDIPLUS
#if _MSC_VER > 1200
#pragma comment(lib, "gdiplus.lib")
#endif
#endif

#endif

// VC6 compatible
#ifndef IS_LOW_SURROGATE
#define IS_LOW_SURROGATE(wch) (((wch) >= 0xdc00) && ((wch) <= 0xdfff))
#endif
#ifndef IS_HIGH_SURROGATE
#define IS_HIGH_SURROGATE(wch) (((wch) >= 0xd800) && ((wch) <= 0xdbff))
#endif

namespace ege
{

// 静态分配，零初始化
struct _graph_setting graph_setting;

#if defined(EGE_BACKEND_COREGRAPHICS)
static bool is_headless_mode()
{
    const char* value = getenv("EGE_HEADLESS");
    return value != NULL && value[0] == '1' && value[1] == '\0';
}
#endif

static initmode_flag   g_initoption    = INIT_DEFAULT;
#ifdef _WIN32
static DWORD g_windowstyle   = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_VISIBLE;
static DWORD g_windowexstyle = WS_EX_LEFT | WS_EX_LTRREADING;
#else
static DWORD g_windowstyle   = 0;
static DWORD g_windowexstyle = 0;
#endif
static int   g_windowpos_x   = CW_USEDEFAULT;
static int   g_windowpos_y   = CW_USEDEFAULT;
#ifdef _WIN32
static const UINT EGE_WM_PROCESS_SHUTDOWN = WM_APP + 0x3E0;
#endif

#ifdef __cplusplus
extern "C"
{
#endif
char*         getlogodata();
unsigned long getlogodatasize();
#ifdef __cplusplus
}
#endif

DWORD WINAPI messageloopthread(_graph_setting* pg);
static void on_destroy(struct _graph_setting* pg);

_graph_setting::_graph_setting() : init_sem{0}
{
    window_caption = EGE_TITLE_W;
    window_initial_color = IMAGE::initial_bk_color;
}

_graph_setting::~_graph_setting()
{
#ifdef _WIN32
    // closegraph() intentionally only hides the reusable legacy window. At
    // process teardown, however, the UI thread must destroy its own HWND so
    // GetMessage() can finish before join(). A private window message avoids
    // invoking an application close callback during static destruction.
    if (threadui.joinable() && hwnd != NULL && IsWindow(hwnd)) {
        PostMessageW(hwnd, EGE_WM_PROCESS_SHUTDOWN, 0, 0);
    }
#else
	on_destroy(this);
#endif
    if (threadui.joinable()) {
        threadui.join();
    }

    // closegraph() keeps resources alive so legacy programs can initialize
    // the same environment again. Static destruction is the final ownership
    // boundary: release EGE-owned pages before destroying queues and the
    // native window itself.
    PIMAGE ownedPages[BITMAP_PAGE_SIZE] = {};
    for (int index = 0; index < BITMAP_PAGE_SIZE; ++index) {
        ownedPages[index] = img_page[index];
        img_page[index] = NULL;
    }
    PIMAGE timerImage = img_timer_update;
    img_timer_update = NULL;
    imgtarget = NULL;
    imgtarget_set = NULL;

    for (int index = 0; index < BITMAP_PAGE_SIZE; ++index) {
        bool alreadyDeleted = false;
        for (int previous = 0; previous < index; ++previous) {
            alreadyDeleted = alreadyDeleted || ownedPages[index] == ownedPages[previous];
        }
        if (ownedPages[index] != NULL && !alreadyDeleted) {
            delete ownedPages[index];
        }
    }
    bool timerIsPage = false;
    for (int index = 0; index < BITMAP_PAGE_SIZE; ++index) {
        timerIsPage = timerIsPage || timerImage == ownedPages[index];
    }
    if (timerImage != NULL && !timerIsPage) {
        delete timerImage;
    }

    delete msgkey_queue;
    msgkey_queue = NULL;
    delete msgmouse_queue;
    msgmouse_queue = NULL;

    delete window;
    window = NULL;
    hwnd = NULL;
}

/*private function*/
static void ui_msg_process(EGEMSG& qmsg)
{
    struct _graph_setting* pg = &graph_setting;
    if ((qmsg.flag & 1)) {
        return;
    }
    qmsg.flag |= 1;
    if (qmsg.message >= WM_KEYFIRST && qmsg.message <= WM_KEYLAST) {
        if (qmsg.message == WM_KEYDOWN || qmsg.message == WM_SYSKEYDOWN) {
            pg->egectrl_root->keymsgdown((unsigned)qmsg.wParam, 0); // 以后补加flag
        } else if (qmsg.message == WM_KEYUP || qmsg.message == WM_SYSKEYUP) {
            pg->egectrl_root->keymsgup((unsigned)qmsg.wParam, 0); // 以后补加flag
        } else if (qmsg.message == WM_CHAR) {
            pg->egectrl_root->keymsgchar((unsigned)qmsg.wParam, 0); // 以后补加flag
        }
    } else if (qmsg.message >= WM_MOUSEFIRST && qmsg.message <= WM_MOUSELAST) {
        int x = (short int)((UINT)qmsg.lParam & 0xFFFF), y = (short int)((UINT)qmsg.lParam >> 16);
        if (qmsg.message == WM_LBUTTONDOWN) {
            pg->egectrl_root->mouse(x, y, mouse_msg_down | mouse_flag_left);
        } else if (qmsg.message == WM_LBUTTONUP) {
            pg->egectrl_root->mouse(x, y, mouse_msg_up | mouse_flag_left);
        } else if (qmsg.message == WM_RBUTTONDOWN) {
            pg->egectrl_root->mouse(x, y, mouse_msg_down | mouse_flag_right);
        } else if (qmsg.message == WM_RBUTTONUP) {
            pg->egectrl_root->mouse(x, y, mouse_msg_up | mouse_flag_right);
        } else if (qmsg.message == WM_MOUSEMOVE) {
            int flag = 0;
            if (pg->keystatemap[VK_LBUTTON]) {
                flag |= mouse_flag_left;
            }
            if (pg->keystatemap[VK_RBUTTON]) {
                flag |= mouse_flag_right;
            }
            pg->egectrl_root->mouse(x, y, mouse_msg_move | flag);
        }
    }
}

/*private function*/
/*
static int redraw_window(_graph_setting* pg, HDC dc)
{
    int page = pg->visual_page;
    HDC hDC  = pg->img_page[page]->m_hDC;
    int left = pg->img_page[page]->m_vpt.left, top = pg->img_page[page]->m_vpt.top;
    // HRGN rgn = pg->img_page[page]->m_rgn;

    BitBlt(dc, 0, 0, pg->base_w, pg->base_h, hDC, pg->base_x - left, pg->base_y - top, SRCCOPY);

    pg->update_mark_count = UPDATE_MAX_CALL;
    return 0;
}
*/

/**
 * @brief 将后台帧缓冲中指定区域的内容复制到前台帧缓冲指定区域
 * @param frontDC     前台帧缓冲设备句柄
 * @param frontPoint  前台帧缓冲指定区域的左上角坐标(设备坐标)
 * @param backDC      后台帧缓冲设备句柄
 * @param rect        后台帧缓冲中要复制内容的区域(设备坐标)
 * @return 错误码
 * @warning 内部使用 BitBlt 进行复制，会受 GDI 坐标变换和视口原点影响，旋转和剪切变换会发生错误。
 */
int frameBufferCopy(HDC frontDC, const Point& frontPoint, HDC backDC, const Rect& rect)
{
#ifdef _WIN32
    /* Note: BitBlt 参数指定的位置受 GDI 坐标变换和视口原点影响 */

    /* 保存影响 BitBlt 的设置 */
    POINT oldViewportOrigin, oldWindowOrigin;
    SetViewportOrgEx(backDC, 0, 0, &oldViewportOrigin);
    SetWindowOrgEx(backDC, 0, 0, &oldWindowOrigin);
    int oldMapMode = SetMapMode(backDC, MM_TEXT);

    XFORM xform;
    int oldGraphicsMode = GetGraphicsMode(backDC);
    if (oldGraphicsMode == GM_ADVANCED) {
        GetWorldTransform(backDC, &xform);
        ModifyWorldTransform(backDC, NULL, MWT_IDENTITY);
        SetGraphicsMode(backDC, GM_COMPATIBLE);
    }

    bool copyResult = BitBlt(frontDC, frontPoint.x, frontPoint.y, rect.width, rect.height, backDC, rect.x, rect.y, SRCCOPY);

    /* 恢复之前的设置 */
    SetViewportOrgEx(backDC, oldViewportOrigin.x, oldViewportOrigin.y, NULL);
    SetWindowOrgEx(backDC, oldWindowOrigin.x, oldWindowOrigin.y, NULL);
    SetMapMode(backDC, oldMapMode);

    if (oldGraphicsMode == GM_ADVANCED) {
        SetGraphicsMode(backDC, oldGraphicsMode);
        SetWorldTransform(backDC, &xform);
    }

    return copyResult ? grOk : grError;
#else
    return grOk;
#endif
}

int swapbuffers()
{
    if (!isinitialized())
        return grNoInitGraph;

    struct _graph_setting* pg = &graph_setting;

    if (pg->window) {
        PIMAGE visualPage = (pg->visual_page >= 0 && pg->visual_page < BITMAP_PAGE_SIZE)
            ? pg->img_page[pg->visual_page] : NULL;
        if (visualPage != NULL) {
            const color_t* pixels = static_cast<const IMAGE*>(visualPage)->getbuffer();
            pg->window->present(pixels, visualPage->getwidth(), visualPage->getheight(),
                                static_cast<size_t>(visualPage->getwidth()) * sizeof(color_t));
        }
        return grOk;
    }

#ifdef _WIN32
    PIMAGE backFrameBuffer = pg->img_page[pg->visual_page];
    HDC backFrameBufferDC = backFrameBuffer->getdc();

    HDC frontFrameBufferDC = GetDC(getHWnd());
    Rect backRect(0, 0, pg->base_w, pg->base_h);
    frameBufferCopy(frontFrameBufferDC, Point(0, 0), backFrameBufferDC, backRect);
    ReleaseDC(getHWnd(), frontFrameBufferDC);
#endif

    return grOk;
}

bool needToUpdate(_graph_setting* pg)
{
    return (pg != NULL) && (pg->update_mark_count < UPDATE_MAX_CALL);
}

int graphupdate(_graph_setting* pg)
{
    if (pg->exit_window) {
        return grNoInitGraph;
    }

    /* 先调整窗口大小，再交换缓冲区，否则放大窗口时会出现白边 */

#ifdef _WIN32
    RECT rect, crect;
    HWND hwnd;
    int  _dw, _dh;

    GetClientRect(pg->hwnd, &crect);
    GetWindowRect(pg->hwnd, &rect);
    int w = pg->dc_w, h = pg->dc_h;
    _dw = w - (crect.right - crect.left);
    _dh = h - (crect.bottom - crect.top);

    if (_dw != 0 || _dh != 0) {
        hwnd = ::GetParent(pg->hwnd);
        if (hwnd) {
            POINT pt = {0, 0};
            ClientToScreen(hwnd, &pt);
            rect.left   -= pt.x;
            rect.top    -= pt.y;
            rect.right  -= pt.x;
            rect.bottom -= pt.y;
        }
        SetWindowPos(pg->hwnd, NULL, 0, 0, rect.right + _dw - rect.left, rect.bottom + _dh - rect.top,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
    }

    if (IsWindowVisible(pg->hwnd)) {
        swapbuffers();
        updateFrameRate();
    } else {
        updateFrameRate(false);
    }
#else
    swapbuffers();
    updateFrameRate();
#endif

    pg->update_mark_count = UPDATE_MAX_CALL;

    return grOk;
}

static void handle_native_window_close(_graph_setting* pg)
{
    pg->exit_window = 1;
    // Match the legacy Win32 WM_DESTROY policy: default-mode applications
    // terminate even when their own loop does not poll is_run().
    if (pg->close_manually && pg->use_force_exit) {
        exit(0);
    }
}

int dealmessage(_graph_setting* pg, bool force_update)
{
    // Native backends own their event loop on the drawing thread.
    if (pg->window) {
        pg->window->processEvents();
        if (pg->window->isClosed()) {
            handle_native_window_close(pg);
        }
    }
    if (force_update || pg->update_mark_count < UPDATE_MAX_CALL) {
        graphupdate(pg);
    }
    return !pg->exit_window;
}

/*private function*/
void guiupdate(_graph_setting* pg, egeControlBase* root)
{
    // During early initialization and shutdown these pointers may be null.
    if (pg == NULL || root == NULL) {
        return;
    }
    if (pg->msgkey_queue) {
        pg->msgkey_queue->process(ui_msg_process);
    }
    if (pg->msgmouse_queue) {
        pg->msgmouse_queue->process(ui_msg_process);
    }
    root->update();
}

/*private function*/
int waitdealmessage(_graph_setting* pg)
{
    if (pg == NULL) {
        return 0;
    }

    if (pg->window) {
        pg->window->processEvents();
        if (pg->window->isClosed()) {
            handle_native_window_close(pg);
        }
    }

    // MSG msg;
    if (pg->update_mark_count < UPDATE_MAX_CALL) {
        egeControlBase* root = pg->egectrl_root;
        if (root) {
            root->draw(NULL);
        }

        graphupdate(pg);
        if (root) {
            guiupdate(pg, root);
        }
    }
    ege_sleep(1);
    return !pg->exit_window;
}

/*private function*/
void setmode(int gdriver, int gmode)
{
    struct _graph_setting* pg = &graph_setting;

    if (gdriver == TRUECOLORSIZE) {
#ifdef _WIN32
        RECT rect;
        HWND parentWindow = getParentWindow();
        if (parentWindow) {
            GetClientRect(parentWindow, &rect);
        } else {
            GetWindowRect(GetDesktopWindow(), &rect);
        }
#endif
        pg->dc_w = (short)(gmode & 0xFFFF);
        pg->dc_h = (short)((unsigned int)gmode >> 16);
        if (pg->dc_w < 0) {
#ifdef _WIN32
            pg->dc_w = rect.right - rect.left;
#else
            pg->dc_w = 640;
#endif
        }
        if (pg->dc_h < 0) {
#ifdef _WIN32
            pg->dc_h = rect.bottom - rect.top;
#else
            pg->dc_h = 480;
#endif
        }
    } else {
        pg->dc_w = 640;
        pg->dc_h = 480;
    }
}

/*private callback function*/

static BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCWSTR lpszType, LPWSTR lpszName, LONG_PTR lParam)
{
#ifdef _WIN32
    HICON hico = (HICON)LoadImageW(hModule, lpszName, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (hico) {
        *((HICON*)lParam) = hico;
        return FALSE;
    }
#endif
    return TRUE;
}

void DefCloseHandler()
{
    struct _graph_setting* pg = &graph_setting;
    pg->exit_flag             = 1;
}

/*private function*/
static void on_repaint(struct _graph_setting* pg, HWND hwnd, HDC dc)
{
    int  page    = pg->visual_page;
    bool release = false;
    pg->img_timer_update->copyimage(pg->img_page[page]);
    if (dc == NULL) {
#ifdef _WIN32
        dc      = GetDC(hwnd);
#endif
        release = true;
    }

    frameBufferCopy(dc, Point(0, 0), pg->img_timer_update->m_hDC, Rect(0, 0, pg->base_w, pg->base_h));

    if (release) {
#ifdef _WIN32
        ReleaseDC(hwnd, dc);
#endif
    }
}

/*private function*/
static void on_timer(struct _graph_setting* pg, HWND hwnd, unsigned id)
{
    if (!pg->skip_timer_mark && id == RENDER_TIMER_ID) {
        if (pg->update_mark_count < UPDATE_MAX_CALL) {
            pg->update_mark_count = UPDATE_MAX_CALL;
            on_repaint(pg, hwnd, NULL);
        }
        if (pg->timer_stop_mark) {
            pg->timer_stop_mark = false;
            pg->skip_timer_mark = true;
        }
    }
}

/*private function*/
static void on_paint(struct _graph_setting* pg, HWND hwnd)
{
#ifdef _WIN32
    if (!pg->lock_window) {
        PAINTSTRUCT ps;
        HDC         hdc;
        hdc = BeginPaint(hwnd, &ps);
        on_repaint(pg, hwnd, hdc);
        EndPaint(hwnd, &ps);
    } else {
        ValidateRect(hwnd, NULL);
        pg->update_mark_count--;
    }
#endif
}

/*private function*/
static void on_destroy(struct _graph_setting* pg)
{
    pg->exit_window = 1;
    dll::freeDlls();
#ifdef _WIN32
    PostQuitMessage(0);
#endif
    if (pg->close_manually && pg->use_force_exit) {
        exit(0);
    }
}

/*private function*/
static void on_setcursor(struct _graph_setting* pg, HWND hwnd)
{
#ifdef _WIN32
    if (pg->mouse_show) {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    } else {
        RECT  rect;
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        GetClientRect(hwnd, &rect);
        if (pt.x >= rect.left && pt.x < rect.right && pt.y >= rect.top && pt.y <= rect.bottom) {
            SetCursor(NULL);
        } else {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        }
    }
#endif
}

/*private function*/
static void on_ime_control(struct _graph_setting* pg, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
#ifdef _WIN32
    if (wparam == IMC_SETSTATUSWINDOWPOS) {
        HIMC            hImc = dll::ImmGetContext(hwnd);
        COMPOSITIONFORM cpf  = {0};
        cpf.dwStyle          = CFS_POINT;
        cpf.ptCurrentPos     = *(LPPOINT)lparam;
        dll::ImmSetCompositionWindow(hImc, &cpf);
    }
#endif
}

/*private function*/
static void windowmanager(ege::_graph_setting* pg, bool create, struct msg_createwindow* msg)
{
#ifdef _WIN32
    if (create) {
        msg->hwnd = ::CreateWindowExW(msg->exstyle, msg->classname, NULL, msg->style, 0, 0, 0, 0, getHWnd(),
            (HMENU)msg->id, getHInstance(), NULL);
        if (msg->hEvent) {
            ::SetEvent(msg->hEvent);
        }
    } else {
        if (msg->hwnd) {
            ::DestroyWindow(msg->hwnd);
        }
        if (msg->hEvent) {
            ::SetEvent(msg->hEvent);
        }
    }
#endif
}

/*private function*/
static void on_key(struct _graph_setting* pg, UINT message, unsigned long keycode, LPARAM keyflag)
{
    /* https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-keydown */
    unsigned msg = 0;
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && keycode < MAX_KEY_VCODE) {
        msg                      = 1;
        pg->keystatemap[keycode] = true;

        /* 按键按下计数，LPARAM 参数第 30 位为 0 代表消息发送前按键状态为 UP */
        if ((keyflag & 0x40000000) == 0) {
            /* 按键按下时初次发送的消息*/
            if (pg->key_press_count[keycode] < UINT16_MAX) {
                pg->key_press_count[keycode]++;
            }
        } else {
            /* 按键长按时重复发送的消息 */
            if (pg->key_repeat_count[keycode] < UINT16_MAX) {
                int repeatCount = keyflag & 0xFFFF; /* 重复次数 */
                pg->key_repeat_count[keycode] += repeatCount;
            }
        }

    }
    if ((message == WM_KEYUP || message == WM_SYSKEYUP) && keycode < MAX_KEY_VCODE) {
        pg->keystatemap[keycode] = false;

        if (pg->key_release_count[keycode] < UINT16_MAX) {
            pg->key_release_count[keycode]++;
        }
    }
    if (pg->callback_key) {
        int ret;
        if (message == WM_CHAR) {
            msg = 2;
        }
        ret = pg->callback_key(pg->callback_key_param, msg, (int)keycode);
        if (ret == 0) {
            return;
        }
    }
    {
        EGEMSG msg  = {0};
        msg.hwnd    = pg->hwnd;
        msg.message = message;
        msg.wParam  = keycode;
        msg.lParam  = keyflag;
#ifdef _WIN32
        msg.time    = ::GetTickCount();
#else
        msg.time    = static_cast<DWORD>(get_highfeq_time_ls() * 1000.0);
#endif
        pg->msgkey_queue->push(msg);
    }
}

/*private function*/
static void push_mouse_msg(struct _graph_setting* pg, UINT message, WPARAM wparam, LPARAM lparam, int time)
{
    EGEMSG msg   = {0};
    msg.hwnd     = pg->hwnd;
    msg.message  = message;
    msg.wParam   = wparam;
    msg.lParam   = lparam;

    msg.mousekey |= pg->keystatemap[VK_LBUTTON]  ? mouse_flag_left  : 0;
    msg.mousekey |= pg->keystatemap[VK_RBUTTON]  ? mouse_flag_right : 0;
    msg.mousekey |= pg->keystatemap[VK_MBUTTON]  ? mouse_flag_mid   : 0;
    msg.mousekey |= pg->keystatemap[VK_XBUTTON1] ? mouse_flag_x1    : 0;
    msg.mousekey |= pg->keystatemap[VK_XBUTTON2] ? mouse_flag_x2    : 0;

    msg.time     = time;
    pg->msgmouse_queue->push(msg);
}

#if defined(EGE_BACKEND_COREGRAPHICS)
class NativeWindowEventSink final : public WindowEventSink
{
public:
    explicit NativeWindowEventSink(_graph_setting* settings) : settings_(settings) {}

    bool onCloseRequested() override
    {
        if (settings_->callback_close) {
            settings_->callback_close();
            // Win32 treats a close callback as the application's opportunity
            // to decide when to destroy the window. Keep the AppKit window open
            // unless that callback closes it explicitly.
            return false;
        }
        settings_->exit_flag = 1;
        settings_->exit_window = 1;
        return true;
    }

    void onResize(int width, int height) override
    {
        if (width > 0 && height > 0 &&
            (settings_->base_w != width || settings_->base_h != height)) {
            resize_window_surface(width, height);
        }
    }

    void onKey(std::uint32_t key, bool pressed, bool repeat) override
    {
        const LPARAM repeatFlag = repeat ? static_cast<LPARAM>(0x40000001) : 1;
        on_key(settings_, pressed ? WM_KEYDOWN : WM_KEYUP, key, repeatFlag);
    }

    void onText(std::uint32_t codepoint) override
    {
        if (settings_->unicode_char_message) {
            on_key(settings_, WM_CHAR, codepoint, 1);
            return;
        }

        const wchar_t wideText[] = {static_cast<wchar_t>(codepoint), L'\0'};
        const std::string encoded = w2mb(wideText);
        for (std::string::const_iterator it = encoded.begin(); it != encoded.end(); ++it) {
            on_key(settings_, WM_CHAR, static_cast<unsigned char>(*it), 1);
        }
    }

    void onMouseMove(int x, int y) override
    {
        settings_->mouse_pos = Point(x, y);
        pushMouse(WM_MOUSEMOVE, 0, x, y);
    }

    void onMouseButton(int button, bool pressed, int x, int y) override
    {
        static const int keyCodes[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON};
        static const UINT downMessages[] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN};
        static const UINT upMessages[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP};
        if (button < 0 || button >= 3) {
            return;
        }
        settings_->mouse_pos = Point(x, y);
        settings_->keystatemap[keyCodes[button]] = pressed;
        if (pressed) {
            if (settings_->key_press_count[keyCodes[button]] < UINT16_MAX) {
                ++settings_->key_press_count[keyCodes[button]];
            }
        } else if (settings_->key_release_count[keyCodes[button]] < UINT16_MAX) {
            ++settings_->key_release_count[keyCodes[button]];
        }
        pushMouse(pressed ? downMessages[button] : upMessages[button], 0, x, y);
    }

    void onMouseWheel(float, float deltaY, int x, int y) override
    {
        const int wheelDelta = static_cast<int>(deltaY * WHEEL_DELTA);
        const WPARAM wheel = static_cast<WPARAM>(
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(wheelDelta)) << 16U);
        pushMouse(WM_MOUSEWHEEL, wheel, x, y);
    }

private:
    void pushMouse(UINT message, WPARAM parameter, int x, int y)
    {
        WPARAM state = parameter;
        state |= settings_->keystatemap[VK_LBUTTON] ? MK_LBUTTON : 0;
        state |= settings_->keystatemap[VK_RBUTTON] ? MK_RBUTTON : 0;
        state |= settings_->keystatemap[VK_MBUTTON] ? MK_MBUTTON : 0;
        push_mouse_msg(settings_, message, state, MAKELPARAM(x, y),
                       static_cast<int>(get_highfeq_time_ls() * 1000.0));
    }

    _graph_setting* settings_;
};
#endif

static void mouseProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN32
    /* up 消息会后紧跟一条 move 消息，标记并将其忽略 */
    static bool skipNextMoveMessage = false;
    if ((message < WM_MOUSEFIRST) || (message > WM_MOUSELAST))
        return;

    _graph_setting* pg = &graph_setting;

    bool curMsgIsNeedToPush = true;

    int key = 0;

    /* WINAPI bug: WM_MOUSEWHEEL 提供的是屏幕坐标，DPI 不等于 100% 时 ScreenToClient 的计算
     * 结果与其它鼠标消息提供的坐标不一致，故忽略 lParam 提供的值，直接使用之前记录的客户区坐标
     */
    if (message == WM_MOUSEWHEEL) {
        lParam = MAKELPARAM(pg->mouse_pos.x, pg->mouse_pos.y);
    }

    mouse_msg msg = mouseMessageConvert(message, wParam, lParam, &key);
    Point curPos(msg.x, msg.y);

    if (msg.is_up()) {
        skipNextMoveMessage = true;
    } else if (msg.is_move()) {
        /* 忽略 up 消息后伴随的同位置 move 消息 */
        if (skipNextMoveMessage && (curPos == pg->mouse_pos)) {
            curMsgIsNeedToPush = false;
            skipNextMoveMessage = false;
        }
    }

    /* 鼠标按键动作 */
    if (key != 0) {
        /* 鼠标按键状态更新*/
        pg->keystatemap[key] = msg.is_down();

        /* 鼠标按键计数 */
        uint16_t* keyCountArray = msg.is_down() ? pg->key_press_count : pg->key_release_count;
        if (keyCountArray[key] < UINT16_MAX) {
            keyCountArray[key]++;
        }

        /* 设置鼠标消息捕获 */
        if (msg.is_down()) {
            SetCapture(hWnd);
        } else {
            const int keyStateMask = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
            if ((wParam & keyStateMask) == 0) {
                ReleaseCapture();
            }
        }
    }

    if (curMsgIsNeedToPush && (hWnd == pg->hwnd)) {
        push_mouse_msg(pg, message, wParam, lParam, GetMessageTime());
    }

    pg->mouse_pos = curPos;
#endif
}

/*private function*/
static LRESULT CALLBACK wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN32
    struct _graph_setting* pg_w = NULL;
    struct _graph_setting* pg   = &graph_setting;
    // int wmId, wmEvent;

    pg_w = (struct _graph_setting*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    if (pg_w == NULL || pg->img_page[0] == NULL) {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_TIMER:
        if (pg == pg_w) {
            on_timer(pg, hWnd, (unsigned int)wParam);
        }
        break;
    case WM_PAINT:
        if (pg == pg_w) {
            on_paint(pg, hWnd);
        }
        break;
    case EGE_WM_PROCESS_SHUTDOWN:
        if (pg == pg_w) {
            pg->close_manually = false;
            pg->use_force_exit = false;
            DestroyWindow(hWnd);
        }
        break;
    case WM_CLOSE:
        if (pg == pg_w) {
            if (pg->callback_close) {
                pg->callback_close();
            }
            return DefWindowProcW(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        if (pg == pg_w) {
            on_destroy(pg);
        }
        break;
    case WM_ERASEBKGND:
        if (pg == pg_w) {
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_CHAR:
        // 这里的WM_SYSKEYDOWN和WM_SYSKEYUP是为了处理Alt键的按下和释放事件，但是如果以后需要让系统接收这两个消息，需要把他返回给默认处理
        // if (hWnd == pg->hwnd)
        {
            if (pg->unicode_char_message) {
                on_key(pg, message, (unsigned long)wParam, lParam);
            } else {
                // 将 UTF-16 编码的消息转换为相应多字节编码的消息
                // UTF-16 可能是代理对, 这里处理下
                wchar_t wc                = (wchar_t)wParam;
                wchar_t wBuf[3]           = {0};
                bool    skip_this_message = false;
                int     wCount            = 0;
                if (IS_LOW_SURROGATE(wc)) {
                    pg->wchar_message_low_surrogate_cache = wc;
                    skip_this_message                     = true;
                } else if (IS_HIGH_SURROGATE(wc)) {
                    wBuf[0]                               = pg->wchar_message_low_surrogate_cache;
                    wBuf[1]                               = wc;
                    pg->wchar_message_low_surrogate_cache = L'\0';
                    wCount                                = 2;
                } else {
                    wBuf[0]                               = wc;
                    pg->wchar_message_low_surrogate_cache = L'\0';
                    wCount                                = 1;
                }
                if (!skip_this_message) {
                    char mbBuf[8];
                    int  mbCount = WideCharToMultiByte(getcodepage(), 0, wBuf, wCount, mbBuf, 8, NULL, NULL);
                    for (int i = 0; i < mbCount; ++i) {
                        on_key(pg, message, (unsigned long)(unsigned char)mbBuf[i], lParam);
                    }
                }
            }
        }
        break;

    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEMOVE:   case WM_MOUSEWHEEL:
        mouseProc(hWnd, message, wParam, lParam);
        break;

    case WM_SETCURSOR:
        if (pg == pg_w) {
            on_setcursor(pg, hWnd);
            return TRUE;
        }
        break;
    case WM_IME_CONTROL:
        on_ime_control(pg, hWnd, message, wParam, lParam);
        break;
    case WM_USER + 1:
        windowmanager(pg, (wParam != 0), (struct msg_createwindow*)lParam);
        break;
    case WM_USER + 2: {
        struct msg_createwindow* msg = (struct msg_createwindow*)lParam;
        ::SetFocus(msg->hwnd);
        ::SetEvent(msg->hEvent);
    } break;
    case WM_CTLCOLOREDIT: {
        egeControlBase* ctl = (egeControlBase*)GetWindowLongPtrW((HWND)lParam, GWLP_USERDATA);
        return ctl->onMessage(message, wParam, lParam);
    } break;
    default:
        if (pg != pg_w) {
            return ((egeControlBase*)pg_w)->onMessage(message, wParam, lParam);
        }
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    if (pg != pg_w) {
        return ((egeControlBase*)pg_w)->onMessage(message, wParam, lParam);
    }
    return 0;
#else
    return 0;
#endif
}

PVOID getProcfunc()
{
    return (PVOID)wndproc;
}

/* private function */
int graph_init(_graph_setting* pg)
{
    pg->img_timer_update = newimage();
    pg->msgkey_queue     = new thread_queue<EGEMSG>;
    pg->msgmouse_queue   = new thread_queue<EGEMSG>;
    setactivepage(0);
    settarget(NULL);
    setvisualpage(0);
    window_setviewport(0, 0, pg->dc_w, pg->dc_h);
    return 0;
}

void logoscene()
{
    int    nsize = getlogodatasize();
    PIMAGE pimg  = newimage();
    int    alpha = 0, b_nobreak = 1, n;
    pimg->getimage((void*)getlogodata(), nsize);
    if (getwidth() <= getwidth(pimg)) {
        PIMAGE pimg1 = newimage(getwidth(pimg) / 2, getheight(pimg) / 2);
        putimage(pimg1, 0, 0, getwidth(pimg) / 2, getheight(pimg) / 2, pimg, 0, 0, getwidth(pimg), getheight(pimg));
        delimage(pimg);
        pimg = pimg1;
    }
    setrendermode(RENDER_MANUAL);
    for (n = 0; n < 5 && b_nobreak; n++, delay_fps(60)) {
        cleardevice();
    }
    for (alpha = 0; alpha <= 0xFF && b_nobreak; alpha += 32, delay_fps(60)) {
        setbkcolor_f(EGERGB(alpha, alpha, alpha));
        cleardevice();
        while (kbhit()) {
            getkey();
            b_nobreak = 0;
        }
    }
    setbkcolor_f(0xFFFFFF);
    for (alpha = 0; alpha <= 0xFF && b_nobreak; alpha += 16, delay_fps(60)) {
        cleardevice();
        putimage_alphablend(
            NULL, pimg, (getwidth() - pimg->getwidth()) / 2, (getheight() - pimg->getheight()) / 2, (unsigned char)alpha);
        while (kbhit()) {
            getkey();
            b_nobreak = 0;
        }
    }
    setbkcolor_f(0xFFFFFF);
    for (n = 0; n < 20 && b_nobreak; n++, delay_fps(60)) {
        cleardevice();
        putimage((getwidth() - pimg->getwidth()) / 2, (getheight() - pimg->getheight()) / 2, pimg);
        while (kbhit()) {
            getkey();
            b_nobreak = 0;
        }
    }
    setbkcolor_f(0xFFFFFF);
    if (alpha > 0xFF) {
        alpha -= 8;
    }
    for (; alpha >= 0 && b_nobreak; alpha -= 16, delay_fps(60)) {
        cleardevice();
        putimage_alphablend(
            NULL, pimg, (getwidth() - pimg->getwidth()) / 2, (getheight() - pimg->getheight()) / 2, (unsigned char)alpha);
        while (kbhit()) {
            getkey();
            b_nobreak = 0;
        }
    }
    cleardevice();
    for (alpha = 0xFF; alpha >= 0 && b_nobreak; alpha -= 32, delay_fps(60)) {
        setbkcolor_f(EGERGB(alpha, alpha, alpha));
        cleardevice();
        while (kbhit()) {
            getkey();
            b_nobreak = 0;
        }
    }
    delimage(pimg);
    setbkcolor_f(BLACK);
    cleardevice();
    setrendermode(RENDER_AUTO);
}

inline void init_img_page(struct _graph_setting* pg)
{
    if (!pg->init_sem.acquirable()) {
#ifdef EGE_GDIPLUS
    gdiplusinit();
#endif
    }
}

void initicon(void)
{
    struct _graph_setting* pg        = &graph_setting;
#ifdef _WIN32
    HINSTANCE              hInstance = GetModuleHandle(NULL);
    HICON                  hIcon     = NULL;

    // 提前设置了图标
    if (pg->window_hicon != 0) {
        return;
    }

    EnumResourceNames(hInstance, RT_ANIICON, EnumResNameProc, (LONG_PTR)&hIcon);
    if (hIcon) {
        pg->window_hicon = hIcon;
        return;
    }
    EnumResourceNames(hInstance, RT_GROUP_ICON, EnumResNameProc, (LONG_PTR)&hIcon);
    if (hIcon) {
        pg->window_hicon = hIcon;
        return;
    }
    EnumResourceNames(hInstance, RT_ICON, EnumResNameProc, (LONG_PTR)&hIcon);
    if (hIcon) {
        pg->window_hicon = hIcon;
        return;
    }

    // default icon
    pg->window_hicon = LoadIcon(NULL, IDI_APPLICATION);
#endif
}

void setcodepage(unsigned int codepage)
{
    graph_setting.codepage = codepage;
}

unsigned int getcodepage()
{
    return graph_setting.codepage;
}

void setunicodecharmessage(bool enable)
{
    graph_setting.unicode_char_message = enable;
}

bool getunicodecharmessage()
{
    return graph_setting.unicode_char_message;
}

void initgraph(int* gdriver, int* gmode, const char* path)
{
    struct _graph_setting* pg = &graph_setting;

    pg->exit_flag   = 0;
    pg->exit_window = 0;

    dll::loadDllsIfNot();

    // 已创建则转为改变窗口大小
    if (pg->init_sem.acquirable()) {
        int width  = (short)(*gmode & 0xFFFF);
        int height = (short)((unsigned int)(*gmode) >> 16);
        resizewindow(width, height);
#ifdef _WIN32
        if (pg->window != NULL) {
            pg->window->show();
        } else {
            HWND hwnd = getHWnd();
            if (!::IsWindowVisible(hwnd)) {
                ::ShowWindow(hwnd, SW_SHOW);
            }
        }
#else
        if (pg->window != NULL) {
            pg->window->show();
        }
#endif
        return;
    }

    // 初始化环境
    setmode(*gdriver, *gmode);
    init_img_page(pg);

#ifndef _WIN32
    pg->use_force_exit = (g_initoption & INIT_NOFORCEEXIT) == 0;
    pg->close_manually = true;
    SetCloseHandler((g_initoption & INIT_NOFORCEEXIT) ? DefCloseHandler : NULL);
#endif

    // setmode() resolves legacy negative dimensions (for example -1 means the
    // default/desktop-sized canvas). Window creation must use those resolved
    // values rather than the raw packed request.
    int width  = pg->dc_w;
    int height = pg->dc_h;

#ifdef _WIN32
    // Preserve the established HWND/message-thread/BitBlt implementation.
    pg->window = NULL;
    pg->instance = GetModuleHandle(NULL);
    initicon();
    register_classW(pg, pg->instance);
    pg->threadui = std::thread{messageloopthread, pg};
    pg->init_sem.acquire();
    pg->init_sem.add_permit();
#elif defined(EGE_BACKEND_COREGRAPHICS)
    pg->window = NULL;
    pg->hwnd = NULL;
    if (!is_headless_mode()) {
        static NativeWindowEventSink nativeEventSink(pg);
        pg->window = new backend::MacWindow();
        WindowOptions windowOptions;
        windowOptions.borderless = (g_initoption & INIT_NOBORDER) != 0;
        windowOptions.topmost = (g_initoption & INIT_TOPMOST) != 0;
        if (!pg->window->create(width, height, "EGE Window", windowOptions, &nativeEventSink)) {
            delete pg->window;
            pg->window = NULL;
            pg->exit_window = 1;
            pg->exit_flag = 1;
            return;
        }
        pg->hwnd = reinterpret_cast<HWND>(pg->window->getNativeHandle());
    }
    if (pg->dc == 0) {
        graph_init(pg);
    }
    pg->init_sem.add_permit();
#else
#error "No native EGE window backend is configured for this target"
#endif

#ifdef _WIN32
    if (pg->hwnd) {
        UpdateWindow(pg->hwnd);
    }


    if (!(g_initoption & INIT_HIDE)) {
        ShowWindow(pg->hwnd, SW_SHOWNORMAL);
        BringWindowToTop(pg->hwnd);
        SetForegroundWindow(pg->hwnd);
    }

    if (g_windowexstyle & WS_EX_TOPMOST) {
        SetWindowPos(pg->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }

    // 初始化鼠标位置数据
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(pg->hwnd, &pt);
    pg->mouse_pos = Point(pt.x, pt.y);
#endif

    if (pg->window != NULL) {
        const std::string utf8Caption = w2utf8(pg->window_caption.c_str());
        pg->window->setTitle(utf8Caption.c_str());
        if (g_windowpos_x != CW_USEDEFAULT && g_windowpos_y != CW_USEDEFAULT) {
            pg->window->setPosition(g_windowpos_x, g_windowpos_y);
        }
        if (g_initoption & INIT_HIDE) {
            pg->window->hide();
        } else {
            pg->window->show();
        }
    }

    static egeControlBase _egeControlBase;

    if ((g_initoption & INIT_WITHLOGO) && !(g_initoption & INIT_HIDE)) {
        logoscene();
    }

    if (g_initoption & INIT_RENDERMANUAL) {
        setrendermode(RENDER_MANUAL);
    }

    pg->first_show = true;
    pg->mouse_show = true;
}

void initgraph(int width, int height, initmode_flag mode)
{
    const unsigned int packedMode =
        (static_cast<unsigned int>(width) & 0xFFFFU) |
        ((static_cast<unsigned int>(height) & 0xFFFFU) << 16);
    int g = TRUECOLORSIZE, m = static_cast<int>(packedMode);
    setinitmode(mode, g_windowpos_x, g_windowpos_y);
    initgraph(&g, &m, "");
}

void detectgraph(int* gdriver, int* gmode)
{
    *gdriver = VGA;
    *gmode   = VGAHI;
}

void closegraph()
{
    struct _graph_setting* pg = &graph_setting;
#ifdef _WIN32
    if (pg->window != NULL) {
        pg->window->hide();
    } else {
        ShowWindow(pg->hwnd, SW_HIDE);
    }
#else
    if (pg->window != NULL) {
        pg->window->hide();
    }
#endif
}

/*private function*/
DWORD WINAPI messageloopthread(_graph_setting* pg)
{
#ifdef _WIN32
    /* 执行应用程序初始化: */
    if (!init_instance(pg->instance)) {
        pg->init_sem.add_permit();
        return 0xFFFFFFFF;
    }

    // 图形初始化
    if (pg->dc == 0) {
        graph_init(pg);
    }

    pg->mouse_show     = 0;
    pg->exit_flag      = 0;
    pg->use_force_exit = (g_initoption & INIT_NOFORCEEXIT ? false : true);

    if (g_initoption & INIT_NOFORCEEXIT) {
        SetCloseHandler(DefCloseHandler);
    }

    pg->close_manually = true;
    pg->skip_timer_mark = false;
    SetTimer(pg->hwnd, RENDER_TIMER_ID, 50, NULL);

    pg->init_sem.add_permit();

    MSG msg;
    while (!pg->exit_window) {
        if (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            Sleep(1);
        }
    }
#endif
    return 0;
}

/*private function*/
BOOL init_instance(HINSTANCE hInstance)
{
    struct _graph_setting* pg = &graph_setting;
#ifdef _WIN32
    int                    dw = 0, dh = 0;
    // WCHAR Title[256] = {0};
    // WCHAR Title2[256] = {0};

    // WideCharToMultiByte(CP_UTF8, 0, pg->window_caption, lstrlenW(pg->window_caption), (LPSTR)Title, 256, 0, 0);
    // MultiByteToWideChar(CP_UTF8, 0, (LPSTR)Title, -1, Title2, 256);
    dw = GetSystemMetrics(SM_CXFRAME) * 2;
    dh = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION) * 2;

    HWND parentWindow = getParentWindow();

    POINT windowPos     = {g_windowpos_x, g_windowpos_y};
    SIZE  windowSize    = {pg->dc_w + dw, pg->dc_h + dh};
    DWORD windowStyle   = g_windowstyle & ~WS_VISIBLE;
    DWORD windowExStyle = g_windowexstyle;

    if (parentWindow) {
        windowPos.x = 0;
        windowPos.y = 0;
        windowSize.cx = pg->dc_w;
        windowSize.cy = pg->dc_h;
        windowStyle &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME |
                         WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        windowStyle |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    }

    pg->hwnd =
        createWindow(getParentWindow(), pg->window_caption.c_str(), windowStyle, windowExStyle, windowPos, windowSize);

    if (pg->hwnd == NULL) {
        return FALSE;
    }

    if (parentWindow != NULL) {
        wchar_t name[64];
        swprintf(name, L"ege_%X", (DWORD)(DWORD_PTR)parentWindow);
        if (CreateEventW(NULL, FALSE, TRUE, name)) {
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                PostMessage(pg->hwnd, WM_CLOSE, 0, 0);
            }
        }
    }
    // SetWindowTextA(pg->hwnd, (const char*)Title);
    SetWindowLongPtrW(pg->hwnd, GWLP_USERDATA, (LONG_PTR)pg);

    /* {
        LOGFONTW lf = {0};
        lf.lfHeight         = 12;
        lf.lfWidth          = 6;
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
        lstrcpyW(lf.lfFaceName, L"宋体");
        HFONT hfont = CreateFontIndirectW(&lf);
        ::SendMessage(pg->hwnd, WM_SETFONT, (WPARAM)hfont, NULL);
        //DeleteObject(hfont);
    } //*/

    if (!(g_initoption & INIT_HIDE)) {
        SetActiveWindow(pg->hwnd);
    }

    pg->exit_window = 0;
#endif
    return TRUE;
}

void setinitmode(initmode_flag mode, int x, int y)
{
    g_initoption              = mode;

#ifdef _WIN32
    if (mode & INIT_NOBORDER) {
        if (mode & INIT_CHILD) {
            g_windowstyle = WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE;
        } else {
            g_windowstyle = WS_POPUP | WS_CLIPCHILDREN | WS_VISIBLE;
        }
        g_windowexstyle = WS_EX_LEFT | WS_EX_LTRREADING;
    } else {
        g_windowstyle   = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_VISIBLE;
        g_windowexstyle = WS_EX_LEFT | WS_EX_LTRREADING;
    }
    if (mode & INIT_TOPMOST) {
        g_windowexstyle |= WS_EX_TOPMOST;
    }
#endif
    if (mode & INIT_UNICODE) {
        setunicodecharmessage(true);
    }
    g_windowpos_x = x;
    g_windowpos_y = y;
}

initmode_flag getinitmode()
{
    return g_initoption;
}

// 获取当前版本
long getGraphicsVer()
{
    return EGE_VERSION_NUMBER;
}

void gdiplusinit()
{
#ifdef EGE_GDIPLUS
    if (graph_setting.g_gdiplusToken == 0) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&graph_setting.g_gdiplusToken, &gdiplusStartupInput, NULL);
    }
#endif
}

/**
 * @brief 重新创建 Graphics 对象，并保持和原来的 Graphics 同样的属性配置
 *
 * @param hdc 图像所持有的 DC 句柄
 * @param oldGraphics 旧 graphics 对象，如果为 NULL 则仅创建新的 Graphics 对象，不做额外的设置
 * @return Gdiplus::Graphics* 创建的 Graphics 对象
 */
#ifdef EGE_GDIPLUS
Gdiplus::Graphics* recreateGdiplusGraphics(HDC hdc, const Gdiplus::Graphics* oldGraphics)
{
    /* 重置视口原点(如果不重置会影响到 GDI+ Graphics 对象坐标系原点) */
    POINT origin;
    SetViewportOrgEx(hdc, 0, 0, &origin);

    /* 清除 GDI 裁剪区域，不清除则会对 GDI+ 的裁剪区域造成莫名影响 */
    HRGN oldClipRgn = CreateRectRgn(0, 0, 0, 0);
    int result = GetClipRgn(hdc, oldClipRgn);
    SelectClipRgn(hdc, NULL);

    Gdiplus::Graphics* newGraphics = Gdiplus::Graphics::FromHDC(hdc);

    /* 保持与原来相同的设置 */
    if (oldGraphics != NULL) {
        /* 裁剪区域设置 */
        Gdiplus::Region clipRegion;
        oldGraphics->GetClip(&clipRegion);
        newGraphics->SetClip(&clipRegion);

        /* 坐标变换设置 */
        Gdiplus::Matrix transform;
        oldGraphics->GetTransform(&transform);
        newGraphics->SetTransform(&transform);

        /* 绘图质量设置 */
        newGraphics->SetSmoothingMode(oldGraphics->GetSmoothingMode());
        newGraphics->SetInterpolationMode(oldGraphics->GetInterpolationMode());
        newGraphics->SetPixelOffsetMode(oldGraphics->GetPixelOffsetMode());
        newGraphics->SetTextRenderingHint(oldGraphics->GetTextRenderingHint());
        newGraphics->SetCompositingQuality(oldGraphics->GetCompositingQuality());
        newGraphics->SetTextContrast(oldGraphics->GetTextContrast());

        /* 组合模式设置 */
        newGraphics->SetCompositingMode(oldGraphics->GetCompositingMode());

        /* 页面单位和比例设置 */
        newGraphics->SetPageUnit(oldGraphics->GetPageUnit());
        newGraphics->SetPageScale(oldGraphics->GetPageScale());

        /* 渲染原点设置 */
        INT x, y;
        oldGraphics->GetRenderingOrigin(&x, &y);
        newGraphics->SetRenderingOrigin(x, y);
    }

    /* 恢复 GDI 坐标原点和裁剪区域 */
    SetViewportOrgEx(hdc, origin.x, origin.y, NULL);
    if (result == 1) {
        SelectClipRgn(hdc, oldClipRgn);
    }

    DeleteObject(oldClipRgn);

    return newGraphics;
}
#else
void* recreateGdiplusGraphics(HDC hdc, const void* oldGraphics)
{
    return NULL;
}
#endif

void replacePixels(PIMAGE pimg, color_t src, color_t dst, bool ignoreAlpha)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if (img && img->m_width > 0 && img->m_height > 0) {
        color_t* bufferBegin = img->getbuffer();
        const color_t* bufferEnd =  bufferBegin + img->m_width * img->m_height;

        if (ignoreAlpha) {
            for (color_t* itor = bufferBegin; itor != bufferEnd; ++itor) {
                if ((*itor & 0x00FFFFFF) == (src & 0x00FFFFFF)) {
                    *itor = dst;
                }
            }
        } else {
            for (color_t* itor = bufferBegin; itor != bufferEnd; ++itor) {
                if (*itor == src) {
                    *itor = dst;
                }
            }
        }
    }

    CONVERT_IMAGE_END
}

} // namespace ege
