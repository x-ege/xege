#pragma once
#include "../interface/Window.h"
#include "GDIGraphicsContext.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace ege {

class GDIWindow : public Window {
public:
    GDIWindow();
    ~GDIWindow();
    bool create(int width, int height, const char* title) override;
    void show() override;
    void hide() override;
    void close() override;
    bool isClosed() override;
    void processEvents() override;
    void swapBuffers() override;
    GraphicsContext* getGraphicsContext() override;
    void* getNativeHandle() override;
    int getWidth() const override;
    int getHeight() const override;

private:
#ifdef _WIN32
    HWND m_hwnd;
    HDC m_hdc;
#else
    void* m_hwnd;
    void* m_hdc;
#endif
    GDIGraphicsContext* m_context;
    int m_width;
    int m_height;
};

}
