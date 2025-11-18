/*
 * EGE (Easy Graphics Engine)
 * Renderer Abstraction Interface
 * 
 * This file defines the abstract interface for different rendering backends
 * (GDI and OpenGL) to enable cross-platform support while maintaining
 * backward compatibility.
 */

#pragma once

#include <cstdint>

namespace ege {
namespace renderer {

// Forward declarations
class IMAGE;

/**
 * @enum RendererType
 * @brief Type of rendering backend
 */
enum RendererType {
    RENDERER_GDI,     ///< Windows GDI/GDI+ backend (default, legacy)
    RENDERER_OPENGL   ///< OpenGL 3.3+ backend (cross-platform)
};

/**
 * @class IRenderer
 * @brief Abstract interface for rendering backends
 * 
 * This interface provides a unified API for different rendering backends.
 * All drawing operations are routed through this interface, allowing
 * EGE to support multiple backends transparently.
 */
class IRenderer {
public:
    virtual ~IRenderer() {}

    /**
     * @brief Get the renderer type
     */
    virtual RendererType getType() const = 0;

    /**
     * @brief Initialize the renderer
     * @param width Window width
     * @param height Window height
     * @return true on success, false on failure
     */
    virtual bool initialize(int width, int height) = 0;

    /**
     * @brief Shutdown the renderer and release resources
     */
    virtual void shutdown() = 0;

    /**
     * @brief Begin a new frame
     */
    virtual void beginFrame() = 0;

    /**
     * @brief End the current frame and present to screen
     */
    virtual void endFrame() = 0;

    /**
     * @brief Clear the rendering surface with a color
     * @param color ARGB color value
     */
    virtual void clear(uint32_t color) = 0;

    /**
     * @brief Set a pixel at the specified position
     * @param x X coordinate
     * @param y Y coordinate
     * @param color ARGB color value
     */
    virtual void setPixel(int x, int y, uint32_t color) = 0;

    /**
     * @brief Get a pixel color at the specified position
     * @param x X coordinate
     * @param y Y coordinate
     * @return ARGB color value
     */
    virtual uint32_t getPixel(int x, int y) const = 0;

    /**
     * @brief Draw a line
     * @param x1 Start X coordinate
     * @param y1 Start Y coordinate
     * @param x2 End X coordinate
     * @param y2 End Y coordinate
     * @param color Line color
     */
    virtual void drawLine(int x1, int y1, int x2, int y2, uint32_t color) = 0;

    /**
     * @brief Draw a rectangle outline
     * @param x X coordinate of top-left corner
     * @param y Y coordinate of top-left corner
     * @param w Width
     * @param h Height
     * @param color Border color
     */
    virtual void drawRectangle(int x, int y, int w, int h, uint32_t color) = 0;

    /**
     * @brief Fill a rectangle
     * @param x X coordinate of top-left corner
     * @param y Y coordinate of top-left corner
     * @param w Width
     * @param h Height
     * @param color Fill color
     */
    virtual void fillRectangle(int x, int y, int w, int h, uint32_t color) = 0;

    /**
     * @brief Draw a circle outline
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Line color
     */
    virtual void drawCircle(int x, int y, int radius, uint32_t color) = 0;

    /**
     * @brief Fill a circle
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param radius Circle radius
     * @param color Fill color
     */
    virtual void fillCircle(int x, int y, int radius, uint32_t color) = 0;

    /**
     * @brief Draw an ellipse outline
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param xRadius X-axis radius
     * @param yRadius Y-axis radius
     * @param color Line color
     */
    virtual void drawEllipse(int x, int y, int xRadius, int yRadius, uint32_t color) = 0;

    /**
     * @brief Fill an ellipse
     * @param x Center X coordinate
     * @param y Center Y coordinate
     * @param xRadius X-axis radius
     * @param yRadius Y-axis radius
     * @param color Fill color
     */
    virtual void fillEllipse(int x, int y, int xRadius, int yRadius, uint32_t color) = 0;

    /**
     * @brief Resize the rendering surface
     * @param width New width
     * @param height New height
     */
    virtual void resize(int width, int height) = 0;

    /**
     * @brief Get direct access to pixel buffer (if available)
     * @return Pointer to pixel buffer (ARGB format) or nullptr
     * @note This may not be efficient for GPU-based renderers
     */
    virtual uint32_t* getPixelBuffer() = 0;

    /**
     * @brief Synchronize pixel buffer changes to the renderer
     * @note Call this after modifying the pixel buffer directly
     */
    virtual void syncPixelBuffer() = 0;
};

/**
 * @class IRendererFactory
 * @brief Factory for creating renderer instances
 */
class IRendererFactory {
public:
    /**
     * @brief Create a renderer instance
     * @param type Type of renderer to create
     * @return Pointer to renderer instance or nullptr on failure
     */
    static IRenderer* createRenderer(RendererType type);

    /**
     * @brief Destroy a renderer instance
     * @param renderer Pointer to renderer to destroy
     */
    static void destroyRenderer(IRenderer* renderer);
};

} // namespace renderer
} // namespace ege
