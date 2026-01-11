#include "GDIGraphicsContext.h"

#ifdef _WIN32
namespace ege {

GDIGraphicsContext::GDIGraphicsContext(void* hdc) : m_hdc((HDC)hdc), m_graphics(NULL), m_pen(NULL), m_brush(NULL) {
    // Initialize GDI+ if needed, or just use GDI
}

GDIGraphicsContext::~GDIGraphicsContext() {
    // Cleanup
}

void GDIGraphicsContext::setLineColor(color_t color) {
    m_lineColor = color;
    // Update pen
}

void GDIGraphicsContext::setFillColor(color_t color) {
    m_fillColor = color;
    // Update brush
}

void GDIGraphicsContext::setLineWidth(float width) {
    m_lineWidth = width;
    // Update pen
}

void GDIGraphicsContext::drawLine(int x1, int y1, int x2, int y2) {
    MoveToEx(m_hdc, x1, y1, NULL);
    LineTo(m_hdc, x2, y2);
}

void GDIGraphicsContext::drawRect(int x, int y, int w, int h) {
    // Use FrameRect or Rectangle with null brush
}

void GDIGraphicsContext::fillRect(int x, int y, int w, int h) {
    RECT r = {x, y, x+w, y+h};
    HBRUSH br = CreateSolidBrush(m_fillColor); // Convert color format!
    FillRect(m_hdc, &r, br);
    DeleteObject(br);
}

void GDIGraphicsContext::drawEllipse(int x, int y, int w, int h) {
    Arc(m_hdc, x, y, x+w, y+h, 0, 0, 0, 0); // Full ellipse
}

void GDIGraphicsContext::fillEllipse(int x, int y, int w, int h) {
    Ellipse(m_hdc, x, y, x+w, y+h);
}

void GDIGraphicsContext::drawCircle(int x, int y, int r) {
    drawEllipse(x - r, y - r, 2 * r, 2 * r);
}

void GDIGraphicsContext::fillCircle(int x, int y, int r) {
    fillEllipse(x - r, y - r, 2 * r, 2 * r);
}

void GDIGraphicsContext::putPixel(int x, int y, color_t color) {
    SetPixel(m_hdc, x, y, color); // Convert color!
}

color_t GDIGraphicsContext::getPixel(int x, int y) {
    return GetPixel(m_hdc, x, y); // Convert color!
}

void GDIGraphicsContext::clear(color_t color) {
    // Fill whole DC
}

void GDIGraphicsContext::flush() {
    GdiFlush();
}

void GDIGraphicsContext::setViewport(int x, int y, int w, int h) {
    // GDI viewport handling is done in setviewport function in egegapi.cpp currently.
    // We can move it here later.
}

}
#else
namespace ege {
GDIGraphicsContext::GDIGraphicsContext(void* hdc) {}
GDIGraphicsContext::~GDIGraphicsContext() {}
void GDIGraphicsContext::setLineColor(color_t color) {}
void GDIGraphicsContext::setFillColor(color_t color) {}
void GDIGraphicsContext::setLineWidth(float width) {}
void GDIGraphicsContext::drawLine(int x1, int y1, int x2, int y2) {}
void GDIGraphicsContext::drawRect(int x, int y, int w, int h) {}
void GDIGraphicsContext::fillRect(int x, int y, int w, int h) {}
void GDIGraphicsContext::drawEllipse(int x, int y, int w, int h) {}
void GDIGraphicsContext::fillEllipse(int x, int y, int w, int h) {}
void GDIGraphicsContext::drawCircle(int x, int y, int r) {}
void GDIGraphicsContext::fillCircle(int x, int y, int r) {}
void GDIGraphicsContext::putPixel(int x, int y, color_t color) {}
color_t GDIGraphicsContext::getPixel(int x, int y) { return 0; }
void GDIGraphicsContext::clear(color_t color) {}
void GDIGraphicsContext::flush() {}
void GDIGraphicsContext::setViewport(int x, int y, int w, int h) {}
}
#endif
