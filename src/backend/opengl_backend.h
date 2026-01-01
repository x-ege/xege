/*
 * EGE (Easy Graphics Engine)
 * FileName:    opengl_backend.h
 *
 * OpenGL rendering backend header - provides cross-platform rendering support.
 * This backend uses GLFW for window management and OpenGL for rendering.
 *
 * Note: This is an experimental feature for cross-platform support.
 * Enable with INIT_OPENGL flag in initgraph().
 */

#ifndef EGE_OPENGL_BACKEND_H
#define EGE_OPENGL_BACKEND_H

#ifdef EGE_ENABLE_OPENGL

#include "render_backend.h"

// Forward declarations for GLFW types
struct GLFWwindow;

namespace ege
{

/**
 * @class OpenGLBackend
 * @brief OpenGL rendering backend for cross-platform support
 *
 * This backend provides rendering using OpenGL, with GLFW handling
 * window management. This enables EGE to run on Windows, macOS, and Linux
 * without requiring Wine.
 *
 * Note: This is an experimental feature. Some EGE functions may not be
 * fully supported in OpenGL mode.
 */
class OpenGLBackend : public RenderBackend
{
public:
    OpenGLBackend();
    virtual ~OpenGLBackend();

    // RenderBackend interface implementation
    virtual RenderBackendType getType() const override;
    virtual bool initialize(int width, int height, int mode) override;
    virtual void shutdown() override;
    virtual bool isInitialized() const override;
    virtual void swapBuffers() override;
    virtual void clear(color_t color) override;
    virtual void putPixel(int x, int y, color_t color) override;
    virtual color_t getPixel(int x, int y) override;
    virtual void drawLine(int x1, int y1, int x2, int y2, color_t color) override;
    virtual void drawRectangle(int left, int top, int right, int bottom, color_t color) override;
    virtual void fillRectangle(int left, int top, int right, int bottom, color_t color) override;
    virtual void drawCircle(int x, int y, int radius, color_t color) override;
    virtual void fillCircle(int x, int y, int radius, color_t color) override;
    virtual void drawEllipse(int x, int y, int xRadius, int yRadius, color_t color) override;
    virtual void fillEllipse(int x, int y, int xRadius, int yRadius, color_t color) override;
    virtual bool processEvents() override;
    virtual int getWidth() const override;
    virtual int getHeight() const override;

private:
    /**
     * @brief Initialize GLFW library
     * @return true if GLFW initialized successfully
     */
    bool initGLFW();

    /**
     * @brief Initialize GLAD (OpenGL loader)
     * @return true if GLAD initialized successfully
     */
    bool initGLAD();

    /**
     * @brief Set up OpenGL state for 2D rendering
     */
    void setupOpenGLState();

    /**
     * @brief Draw a circle using OpenGL primitives
     * @param x Center X
     * @param y Center Y
     * @param radius Radius
     * @param fill Whether to fill the circle
     */
    void drawCircleGL(int x, int y, int radius, bool fill);

    /**
     * @brief Draw an ellipse using OpenGL primitives
     * @param x Center X
     * @param y Center Y
     * @param xRadius Horizontal radius
     * @param yRadius Vertical radius
     * @param fill Whether to fill the ellipse
     */
    void drawEllipseGL(int x, int y, int xRadius, int yRadius, bool fill);

    /**
     * @brief Set the current OpenGL color from EGE color
     * @param color EGE color value
     */
    void setGLColor(color_t color);

    GLFWwindow* m_window;
    bool m_initialized;
    int m_width;
    int m_height;
    color_t m_clearColor;
    color_t m_drawColor;

    // Frame buffer for software rendering fallback
    color_t* m_frameBuffer;
};

} // namespace ege

#endif // EGE_ENABLE_OPENGL

#endif // EGE_OPENGL_BACKEND_H
