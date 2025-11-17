/*
 * EGE (Easy Graphics Engine)
 * OpenGL Renderer - Cross-platform OpenGL 3.3+ backend
 * 
 * This renderer uses GLFW for windowing and GLAD for OpenGL loading,
 * providing true cross-platform support (Windows, Linux, macOS).
 */

#pragma once

#include "renderer_interface.h"

#ifdef EGE_ENABLE_OPENGL

#include <vector>

// Forward declarations for GLFW
struct GLFWwindow;

namespace ege {
namespace renderer {

/**
 * @class OpenGLRenderer
 * @brief OpenGL 3.3+ rendering backend
 * 
 * This renderer uses modern OpenGL with GLFW for window management,
 * enabling true cross-platform support while maintaining EGE's API.
 */
class OpenGLRenderer : public IRenderer {
public:
    OpenGLRenderer();
    virtual ~OpenGLRenderer();

    // IRenderer interface implementation
    virtual RendererType getType() const override { return RENDERER_OPENGL; }
    virtual bool initialize(int width, int height) override;
    virtual void shutdown() override;
    virtual void beginFrame() override;
    virtual void endFrame() override;
    virtual void clear(uint32_t color) override;
    virtual void setPixel(int x, int y, uint32_t color) override;
    virtual uint32_t getPixel(int x, int y) const override;
    virtual void drawLine(int x1, int y1, int x2, int y2, uint32_t color) override;
    virtual void drawRectangle(int x, int y, int w, int h, uint32_t color) override;
    virtual void fillRectangle(int x, int y, int w, int h, uint32_t color) override;
    virtual void resize(int width, int height) override;
    virtual uint32_t* getPixelBuffer() override;
    virtual void syncPixelBuffer() override;

private:
    bool initializeGLFW();
    bool initializeOpenGL();
    void createFramebuffer();
    void deleteFramebuffer();
    void updateTexture();

    GLFWwindow* m_window;
    int m_width;
    int m_height;
    
    // OpenGL resources
    unsigned int m_fbo;           // Framebuffer object
    unsigned int m_texture;       // Texture for rendering
    unsigned int m_vao;           // Vertex array object
    unsigned int m_vbo;           // Vertex buffer object
    unsigned int m_shaderProgram; // Shader program
    
    // Pixel buffer for CPU-side operations
    std::vector<uint32_t> m_pixelBuffer;
    bool m_bufferDirty;           // Flag to track if buffer needs sync
};

} // namespace renderer
} // namespace ege

#endif // EGE_ENABLE_OPENGL
