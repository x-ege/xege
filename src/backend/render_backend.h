/*
 * EGE (Easy Graphics Engine)
 * FileName:    render_backend.h
 *
 * Abstract rendering backend interface for multi-backend support.
 * This allows EGE to support both GDI (Windows) and OpenGL (cross-platform) backends.
 */

#ifndef EGE_RENDER_BACKEND_H
#define EGE_RENDER_BACKEND_H

#include "ege.h"

namespace ege
{

/**
 * @enum RenderBackendType
 * @brief Identifies the type of rendering backend
 */
enum RenderBackendType
{
    BACKEND_GDI,      ///< Windows GDI/GDI+ backend (default)
    BACKEND_OPENGL    ///< OpenGL backend (cross-platform, experimental)
};

/**
 * @class RenderBackend
 * @brief Abstract interface for rendering backends
 *
 * This interface defines the core operations that each rendering backend must implement.
 * The GDI backend wraps existing Windows GDI/GDI+ functionality.
 * The OpenGL backend uses GLFW for window management and OpenGL for rendering.
 */
class RenderBackend
{
public:
    virtual ~RenderBackend() {}

    /**
     * @brief Get the type of this backend
     * @return The backend type identifier
     */
    virtual RenderBackendType getType() const = 0;

    /**
     * @brief Initialize the rendering backend
     * @param width Window width
     * @param height Window height
     * @param mode Initialization mode flags
     * @return true if initialization succeeded, false otherwise
     */
    virtual bool initialize(int width, int height, int mode) = 0;

    /**
     * @brief Shutdown and cleanup the backend
     */
    virtual void shutdown() = 0;

    /**
     * @brief Check if the backend is initialized
     * @return true if initialized, false otherwise
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Swap front and back buffers (present the frame)
     */
    virtual void swapBuffers() = 0;

    /**
     * @brief Clear the screen with the specified color
     * @param color The clear color
     */
    virtual void clear(color_t color) = 0;

    /**
     * @brief Set a pixel at the specified position
     * @param x X coordinate
     * @param y Y coordinate
     * @param color Pixel color
     */
    virtual void putPixel(int x, int y, color_t color) = 0;

    /**
     * @brief Get the pixel color at the specified position
     * @param x X coordinate
     * @param y Y coordinate
     * @return The pixel color
     */
    virtual color_t getPixel(int x, int y) = 0;

    /**
     * @brief Draw a line from (x1, y1) to (x2, y2)
     * @param x1 Start X coordinate
     * @param y1 Start Y coordinate
     * @param x2 End X coordinate
     * @param y2 End Y coordinate
     * @param color Line color
     */
    virtual void drawLine(int x1, int y1, int x2, int y2, color_t color) = 0;

    /**
     * @brief Draw a rectangle outline
     * @param left Left coordinate
     * @param top Top coordinate
     * @param right Right coordinate
     * @param bottom Bottom coordinate
     * @param color Line color
     */
    virtual void drawRectangle(int left, int top, int right, int bottom, color_t color) = 0;

    /**
     * @brief Fill a rectangle with the specified color
     * @param left Left coordinate
     * @param top Top coordinate
     * @param right Right coordinate
     * @param bottom Bottom coordinate
     * @param color Fill color
     */
    virtual void fillRectangle(int left, int top, int right, int bottom, color_t color) = 0;

    /**
     * @brief Draw a circle outline
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Line color
     */
    virtual void drawCircle(int x, int y, int radius, color_t color) = 0;

    /**
     * @brief Fill a circle with the specified color
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     */
    virtual void fillCircle(int x, int y, int radius, color_t color) = 0;

    /**
     * @brief Draw an ellipse outline
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param xRadius Horizontal radius
     * @param yRadius Vertical radius
     * @param color Line color
     */
    virtual void drawEllipse(int x, int y, int xRadius, int yRadius, color_t color) = 0;

    /**
     * @brief Fill an ellipse with the specified color
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param xRadius Horizontal radius
     * @param yRadius Vertical radius
     * @param color Fill color
     */
    virtual void fillEllipse(int x, int y, int xRadius, int yRadius, color_t color) = 0;

    /**
     * @brief Process pending events/messages
     * @return true if the window should continue running, false to exit
     */
    virtual bool processEvents() = 0;

    /**
     * @brief Get the current window width
     * @return Window width in pixels
     */
    virtual int getWidth() const = 0;

    /**
     * @brief Get the current window height
     * @return Window height in pixels
     */
    virtual int getHeight() const = 0;
};

/**
 * @brief Get the current rendering backend
 * @return Pointer to the active rendering backend, or nullptr if not initialized
 */
RenderBackend* getCurrentBackend();

/**
 * @brief Create and set the rendering backend based on initialization flags
 * @param mode Initialization mode flags (from initmode_flag enum)
 * @return Pointer to the created backend, or nullptr on failure
 */
RenderBackend* createBackend(int mode);

/**
 * @brief Destroy the current backend and free resources
 */
void destroyBackend();

/**
 * @brief Check if the current backend is OpenGL
 * @return true if using OpenGL backend, false otherwise
 */
bool isOpenGLBackend();

} // namespace ege

#endif // EGE_RENDER_BACKEND_H
