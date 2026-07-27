#include "GDIWindow.h"
#include <cstddef>

#ifdef _WIN32
#include <tchar.h>

namespace ege {

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

GDIWindow::GDIWindow() : m_hwnd(NULL), m_hdc(NULL), m_context(NULL), m_width(0), m_height(0) {
}

GDIWindow::~GDIWindow() {
    close();
}

bool GDIWindow::create(int width, int height, const char* title) {
    WNDCLASSEX wc;
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandle(NULL);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "EGEWindowClass";
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "EGEWindowClass",
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if(m_hwnd == NULL) {
        return false;
    }

    m_hdc = GetDC(m_hwnd);
    m_width = width;
    m_height = height;
    m_context = new GDIGraphicsContext(m_hdc);

    return true;
}

void GDIWindow::show() {
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void GDIWindow::hide() {
    ShowWindow(m_hwnd, SW_HIDE);
}

void GDIWindow::close() {
    if (m_context) {
        delete m_context;
        m_context = NULL;
    }
    if (m_hdc) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = NULL;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
}

bool GDIWindow::isClosed() {
    return m_hwnd == NULL;
}

void GDIWindow::processEvents() {
    MSG msg;
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void GDIWindow::swapBuffers() {
    // GDI usually draws directly or uses BitBlt from backbuffer.
}

GraphicsContext* GDIWindow::getGraphicsContext() {
    return m_context;
}

void* GDIWindow::getNativeHandle() {
    return (void*)m_hwnd;
}

int GDIWindow::getWidth() const {
    return m_width;
}

int GDIWindow::getHeight() const {
    return m_height;
}

}
#else
namespace ege {
GDIWindow::GDIWindow() : m_context(NULL), m_width(0), m_height(0) {}
GDIWindow::~GDIWindow() {}
bool GDIWindow::create(int width, int height, const char* title) { return false; }
void GDIWindow::show() {}
void GDIWindow::hide() {}
void GDIWindow::close() {}
bool GDIWindow::isClosed() { return true; }
void GDIWindow::processEvents() {}
void GDIWindow::swapBuffers() {}
GraphicsContext* GDIWindow::getGraphicsContext() { return NULL; }
void* GDIWindow::getNativeHandle() { return NULL; }
int GDIWindow::getWidth() const { return 0; }
int GDIWindow::getHeight() const { return 0; }
}
#endif
