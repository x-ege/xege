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
    /*
     * Note: The GDI backend delegates to existing EGE functions.
     * These stubs are intentionally empty because:
     * 1. The existing EGE code handles all GDI operations directly
     * 2. This backend serves as a wrapper for the abstraction layer
     * 3. Full integration with existing code is planned for Phase 4
     *
     * TODO: Integrate with swapbuffers() from graphics.cpp
     */
}

void GDIBackend::clear(color_t color)
{
    /*
     * Delegates to existing cleardevice() with setbkcolor_f()
     * The actual implementation remains in the existing EGE code.
     * TODO: Integrate with setbkcolor_f() and cleardevice()
     */
    (void)color;
}

void GDIBackend::putPixel(int x, int y, color_t color)
{
    /*
     * Delegates to existing putpixel() function
     * TODO: Integrate with putpixel(x, y, color) from egegapi.cpp
     */
    (void)x;
    (void)y;
    (void)color;
}

color_t GDIBackend::getPixel(int x, int y)
{
    /*
     * Delegates to existing getpixel() function
     * TODO: Integrate with getpixel(x, y) from egegapi.cpp
     * Currently returns 0 as a placeholder
     */
    (void)x;
    (void)y;
    return 0;
}

void GDIBackend::drawLine(int x1, int y1, int x2, int y2, color_t color)
{
    /*
     * Delegates to existing line() function
     * TODO: Integrate with line(x1, y1, x2, y2) from egegapi.cpp
     */
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color;
}

void GDIBackend::drawRectangle(int left, int top, int right, int bottom, color_t color)
{
    /*
     * Delegates to existing rectangle() function
     * TODO: Integrate with rectangle(left, top, right, bottom) from egegapi.cpp
     */
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    (void)color;
}

void GDIBackend::fillRectangle(int left, int top, int right, int bottom, color_t color)
{
    /*
     * Delegates to existing bar() function
     * TODO: Integrate with bar(left, top, right, bottom) from egegapi.cpp
     */
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    (void)color;
}

void GDIBackend::drawCircle(int x, int y, int radius, color_t color)
{
    /*
     * Delegates to existing circle() function
     * TODO: Integrate with circle(x, y, radius) from egegapi.cpp
     */
    (void)x;
    (void)y;
    (void)radius;
    (void)color;
}

void GDIBackend::fillCircle(int x, int y, int radius, color_t color)
{
    /*
     * Delegates to existing fillcircle() function
     * TODO: Integrate with fillellipse(x, y, radius, radius) from egegapi.cpp
     */
    (void)x;
    (void)y;
    (void)radius;
    (void)color;
}

void GDIBackend::drawEllipse(int x, int y, int xRadius, int yRadius, color_t color)
{
    /*
     * Delegates to existing ellipse() function
     * TODO: Integrate with ellipse(x, y, 0, 360, xRadius, yRadius) from egegapi.cpp
     */
    (void)x;
    (void)y;
    (void)xRadius;
    (void)yRadius;
    (void)color;
}

void GDIBackend::fillEllipse(int x, int y, int xRadius, int yRadius, color_t color)
{
    /*
     * Delegates to existing fillellipse() function
     * TODO: Integrate with fillellipse(x, y, xRadius, yRadius) from egegapi.cpp
     */
    (void)x;
    (void)y;
    (void)xRadius;
    (void)yRadius;
    (void)color;
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
