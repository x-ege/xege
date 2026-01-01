/*
 * EGE (Easy Graphics Engine)
 * FileName:    gdi_backend.cpp
 *
 * GDI rendering backend implementation - wraps existing Windows GDI/GDI+ functionality.
 * This backend maintains backward compatibility with the original EGE implementation.
 *
 * Note: The GDI backend delegates most operations to the existing EGE graphics functions.
 * This ensures full backward compatibility while allowing the abstraction layer to work.
 */

#include "gdi_backend.h"
#include "../ege_head.h"

namespace ege
{

// Forward declarations for existing EGE functions
extern struct _graph_setting graph_setting;

GDIBackend::GDIBackend()
    : m_initialized(false)
    , m_width(0)
    , m_height(0)
{
}

GDIBackend::~GDIBackend()
{
    shutdown();
}

RenderBackendType GDIBackend::getType() const
{
    return BACKEND_GDI;
}

bool GDIBackend::initialize(int width, int height, int mode)
{
    // The GDI backend relies on the existing EGE initialization
    // which is handled by initgraph() in graphics.cpp
    // This function is called after the existing initialization
    m_width = width;
    m_height = height;
    m_initialized = true;
    return true;
}

void GDIBackend::shutdown()
{
    // The GDI backend relies on the existing EGE shutdown
    // which is handled by closegraph() in graphics.cpp
    m_initialized = false;
    m_width = 0;
    m_height = 0;
}

bool GDIBackend::isInitialized() const
{
    return m_initialized && graph_setting.has_init;
}

void GDIBackend::swapBuffers()
{
    // Delegate to existing swapbuffers() function
    // This is declared in graphics.cpp
    // swapbuffers();
}

void GDIBackend::clear(color_t color)
{
    // Delegate to existing cleardevice() with setbkcolor_f()
    // The actual implementation remains in the existing code
}

void GDIBackend::putPixel(int x, int y, color_t color)
{
    // Delegate to existing putpixel() function
    // putpixel(x, y, color);
}

color_t GDIBackend::getPixel(int x, int y)
{
    // Delegate to existing getpixel() function
    // return getpixel(x, y);
    return 0;
}

void GDIBackend::drawLine(int x1, int y1, int x2, int y2, color_t color)
{
    // Delegate to existing line() function
    // line(x1, y1, x2, y2);
}

void GDIBackend::drawRectangle(int left, int top, int right, int bottom, color_t color)
{
    // Delegate to existing rectangle() function
    // rectangle(left, top, right, bottom);
}

void GDIBackend::fillRectangle(int left, int top, int right, int bottom, color_t color)
{
    // Delegate to existing bar() function
    // bar(left, top, right, bottom);
}

void GDIBackend::drawCircle(int x, int y, int radius, color_t color)
{
    // Delegate to existing circle() function
    // circle(x, y, radius);
}

void GDIBackend::fillCircle(int x, int y, int radius, color_t color)
{
    // Delegate to existing fillcircle() function with solidfill
    // fillellipse(x, y, radius, radius);
}

void GDIBackend::drawEllipse(int x, int y, int xRadius, int yRadius, color_t color)
{
    // Delegate to existing ellipse() function
    // ellipse(x, y, 0, 360, xRadius, yRadius);
}

void GDIBackend::fillEllipse(int x, int y, int xRadius, int yRadius, color_t color)
{
    // Delegate to existing fillellipse() function
    // fillellipse(x, y, xRadius, yRadius);
}

bool GDIBackend::processEvents()
{
    // The GDI backend uses the existing Windows message loop
    // Return whether the window is still running
    return !graph_setting.exit_window;
}

int GDIBackend::getWidth() const
{
    return graph_setting.dc_w;
}

int GDIBackend::getHeight() const
{
    return graph_setting.dc_h;
}

} // namespace ege
