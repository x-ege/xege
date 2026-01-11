#pragma once
#include "../interface/GraphicsContext.h"
#ifdef _WIN32
#include <windows.h>
#include <gdiplus.h>
#endif

namespace ege {

class GDIGraphicsContext : public GraphicsContext {
public:
    GDIGraphicsContext(void* hdc);
    ~GDIGraphicsContext();

    void setLineColor(color_t color) override;
    void setFillColor(color_t color) override;
    void setLineWidth(float width) override;

    void drawLine(int x1, int y1, int x2, int y2) override;
    void drawRect(int x, int y, int w, int h) override;
    void fillRect(int x, int y, int w, int h) override;
    void drawEllipse(int x, int y, int w, int h) override;
    void fillEllipse(int x, int y, int w, int h) override;
    void drawCircle(int x, int y, int r) override;
    void fillCircle(int x, int y, int r) override;
    
    void putPixel(int x, int y, color_t color) override;
    color_t getPixel(int x, int y) override;

    void clear(color_t color) override;
    void flush() override;
    
    void setViewport(int x, int y, int w, int h) override;

private:
#ifdef _WIN32
    HDC m_hdc;
    Gdiplus::Graphics* m_graphics;
    Gdiplus::Pen* m_pen;
    Gdiplus::SolidBrush* m_brush;
#else
    void* m_hdc;
    void* m_graphics;
    void* m_pen;
    void* m_brush;
#endif
    color_t m_lineColor;
    color_t m_fillColor;
    float m_lineWidth;
};

}
