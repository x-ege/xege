/*
 * EGE (Easy Graphics Engine)
 * Renderer Factory Implementation
 */

#include "renderer_interface.h"
#include "gdi_renderer.h"

#ifdef EGE_ENABLE_OPENGL
#include "opengl_renderer.h"
#endif

namespace ege {
namespace renderer {

IRenderer* IRendererFactory::createRenderer(RendererType type)
{
    switch (type) {
        case RENDERER_GDI:
            return new GDIRenderer();
        
        case RENDERER_OPENGL:
#ifdef EGE_ENABLE_OPENGL
            return new OpenGLRenderer();
#else
            // Fall back to GDI if OpenGL is not available
            return new GDIRenderer();
#endif
        
        default:
            return nullptr;
    }
}

void IRendererFactory::destroyRenderer(IRenderer* renderer)
{
    delete renderer;
}

} // namespace renderer
} // namespace ege
