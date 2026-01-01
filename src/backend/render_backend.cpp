/*
 * EGE (Easy Graphics Engine)
 * FileName:    render_backend.cpp
 *
 * Implementation of the rendering backend factory and management functions.
 */

#include "render_backend.h"
#include "gdi_backend.h"

#ifdef EGE_ENABLE_OPENGL
#include "opengl_backend.h"
#endif

namespace ege
{

// Global pointer to the current rendering backend
static RenderBackend* g_currentBackend = nullptr;

RenderBackend* getCurrentBackend()
{
    return g_currentBackend;
}

RenderBackend* createBackend(int mode)
{
    // If a backend already exists, destroy it first
    if (g_currentBackend != nullptr) {
        destroyBackend();
    }

#ifdef EGE_ENABLE_OPENGL
    // Check if OpenGL backend is requested
    if (mode & INIT_OPENGL) {
        g_currentBackend = new OpenGLBackend();
        return g_currentBackend;
    }
#else
    // If OpenGL is requested but not available, ignore the flag and use GDI
    if (mode & INIT_OPENGL) {
        // OpenGL backend not compiled in, fall back to GDI
        // Could optionally log a warning here
    }
#endif

    // Default to GDI backend
    g_currentBackend = new GDIBackend();
    return g_currentBackend;
}

void destroyBackend()
{
    if (g_currentBackend != nullptr) {
        g_currentBackend->shutdown();
        delete g_currentBackend;
        g_currentBackend = nullptr;
    }
}

bool isOpenGLBackend()
{
    return g_currentBackend != nullptr &&
           g_currentBackend->getType() == BACKEND_OPENGL;
}

} // namespace ege
